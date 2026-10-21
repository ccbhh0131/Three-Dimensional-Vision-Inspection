#include "core/video/VideoReconstructionController.h"

#include "core/project/ProjectManager.h"
#include "core/video/FrameSamplingPolicy.h"
#include "core/video/VideoToolBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QUuid>

#include <memory>

namespace vision3d {

namespace {

constexpr int MinimumVideoReconstructionFrames = 20;

struct AnalysisRun
{
    bool success = false;
    FrameQualitySummary summary;
    QString error;
};

} // namespace

bool VideoReconstructionController::Snapshot::busy() const
{
    return state == State::Probing || state == State::Extracting || state == State::Analyzing;
}

bool VideoReconstructionController::Snapshot::canPreprocess() const
{
    return state == State::Ready && hasInfo;
}

bool VideoReconstructionController::Snapshot::canStartReconstruction() const
{
    return state == State::PreviewReady && quality.acceptedFrameCount >= MinimumVideoReconstructionFrames;
}

VideoReconstructionController::VideoReconstructionController(ProjectManager* projectManager,
                                                             QObject* parent)
    : QObject(parent)
    , m_projectManager(projectManager)
    , m_probe(nullptr)
    , m_extractor(nullptr)
{
    Q_ASSERT(m_projectManager != nullptr);
    connect(&m_probe, &VideoProbe::finished, this,
            &VideoReconstructionController::onProbeFinished);
    connect(&m_probe, &VideoProbe::failed, this,
            &VideoReconstructionController::onProbeFailed);
    connect(&m_probe, &VideoProbe::cancelled, this,
            &VideoReconstructionController::onProbeCancelled);
    connect(&m_extractor, &VideoFrameExtractor::progress, this,
            &VideoReconstructionController::onExtractProgress);
    connect(&m_extractor, &VideoFrameExtractor::finished, this,
            &VideoReconstructionController::onExtractFinished);
    connect(&m_extractor, &VideoFrameExtractor::failed, this,
            &VideoReconstructionController::onExtractFailed);
    connect(&m_extractor, &VideoFrameExtractor::cancelled, this,
            &VideoReconstructionController::onExtractCancelled);
}

VideoReconstructionController::~VideoReconstructionController()
{
    m_cancelAnalysis.store(true);
    if (m_probe.isRunning()) {
        m_probe.cancel();
    }
    if (m_extractor.isRunning()) {
        m_extractor.cancel();
    }
    if (m_analysisThread != nullptr && m_analysisThread->isRunning()) {
        m_analysisThread->wait();
    }
}

bool VideoReconstructionController::selectVideo(const QString& path, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (isBusy()) {
        if (error != nullptr) {
            *error = QStringLiteral("视频处理正在运行，请先取消当前任务。");
        }
        return false;
    }
    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("请先打开或创建项目，再导入视频。");
        }
        return false;
    }
    QString toolError;
    m_tools = VideoToolBackend::locate(&toolError);
    if (m_tools.ffprobe.isEmpty()) {
        m_snapshot = Snapshot();
        m_snapshot.state = State::Failed;
        m_snapshot.errorText = toolError;
        m_snapshot.statusText = toolError;
        emit snapshotChanged();
        emit logMessage(toolError, true);
        if (error != nullptr) {
            *error = toolError;
        }
        return false;
    }
    m_snapshot = Snapshot();
    m_snapshot.info.path = QFileInfo(path).absoluteFilePath();
    m_snapshot.statusText = QStringLiteral("正在解析视频信息…");
    m_snapshot.state = State::Probing;
    emit snapshotChanged();
    emit logMessage(QStringLiteral("正在解析视频：%1").arg(path), false);
    if (!m_probe.start(path, m_tools, error)) {
        const QString message = error == nullptr ? QStringLiteral("无法启动视频信息解析。") : *error;
        m_snapshot.state = State::Failed;
        m_snapshot.errorText = message;
        m_snapshot.statusText = message;
        emit snapshotChanged();
        emit logMessage(message, true);
        return false;
    }
    return true;
}

bool VideoReconstructionController::preprocess(int targetFrames, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (!m_snapshot.canPreprocess()) {
        if (error != nullptr) {
            *error = QStringLiteral("请先选择视频并等待视频信息读取完成。");
        }
        return false;
    }
    m_snapshot.targetFrames = targetFrames <= 0
                                  ? m_snapshot.recommendedFrames
                                  : FrameSamplingPolicy::clampManualTargetFrames(targetFrames);
    if (!createJobWorkspace(error)) {
        const QString message = error == nullptr ? QStringLiteral("无法创建视频处理任务目录。") : *error;
        m_snapshot.state = State::Failed;
        m_snapshot.errorText = message;
        m_snapshot.statusText = message;
        emit snapshotChanged();
        emit logMessage(message, true);
        return false;
    }

    const long double requiredBytes = static_cast<long double>(m_videoInfo.width)
                                     * static_cast<long double>(m_videoInfo.height)
                                     * 3.0L * static_cast<long double>(m_snapshot.targetFrames);
    QStorageInfo storage(m_jobPaths.root);
    storage.refresh();
    if (storage.isValid() && storage.bytesAvailable() > 0
        && requiredBytes > static_cast<long double>(storage.bytesAvailable())) {
        const QString message = QStringLiteral("磁盘可用空间不足，按未压缩图像估算至少需要 %1 GB。")
                                    .arg(static_cast<double>(requiredBytes / 1000000000.0L),
                                         0,
                                         'f',
                                         1);
        m_snapshot.state = State::Failed;
        m_snapshot.errorText = message;
        m_snapshot.statusText = message;
        emit snapshotChanged();
        emit logMessage(message, true);
        if (error != nullptr) {
            *error = message;
        }
        return false;
    }

    m_snapshot.jobDirectory = m_jobPaths.root;
    m_snapshot.progress = 0;
    m_snapshot.errorText.clear();
    m_snapshot.statusText = QStringLiteral("正在提取关键帧…");
    m_snapshot.state = State::Extracting;
    emit snapshotChanged();
    emit logMessage(QStringLiteral("开始抽取视频帧：target=%1，目录=%2")
                        .arg(m_snapshot.targetFrames)
                        .arg(QDir(m_jobPaths.root).filePath(QStringLiteral("raw_frames"))),
                    false);
    const QString rawFrames = QDir(m_jobPaths.root).filePath(QStringLiteral("raw_frames"));
    if (!m_extractor.start(m_tools, m_videoInfo, m_snapshot.targetFrames, rawFrames, error)) {
        const QString message = error == nullptr ? QStringLiteral("无法启动视频抽帧。") : *error;
        m_snapshot.state = State::Failed;
        m_snapshot.errorText = message;
        m_snapshot.statusText = message;
        emit snapshotChanged();
        emit logMessage(message, true);
        return false;
    }
    return true;
}

void VideoReconstructionController::reset()
{
    if (isBusy()) {
        return;
    }
    m_snapshot = Snapshot();
    m_videoInfo = VideoInfo();
    m_jobPaths = ReconstructionJobPaths();
    m_cancelAnalysis.store(false);
    emit snapshotChanged();
}

void VideoReconstructionController::cancel()
{
    if (m_probe.isRunning()) {
        m_probe.cancel();
        m_snapshot.statusText = QStringLiteral("正在取消视频读取…");
    } else if (m_extractor.isRunning()) {
        m_extractor.cancel();
        m_snapshot.statusText = QStringLiteral("正在取消视频抽帧…");
    } else if (m_analysisThread != nullptr && m_analysisThread->isRunning()) {
        m_cancelAnalysis.store(true);
        m_snapshot.statusText = QStringLiteral("正在取消图像质量检查…");
    } else {
        return;
    }
    emit snapshotChanged();
    emit logMessage(QStringLiteral("正在取消视频预处理；已生成文件保留在任务目录。"), false);
}

bool VideoReconstructionController::isBusy() const
{
    return m_snapshot.busy() || (m_analysisThread != nullptr && m_analysisThread->isRunning());
}

const VideoReconstructionController::Snapshot& VideoReconstructionController::snapshot() const
{
    return m_snapshot;
}

ReconstructionInput VideoReconstructionController::reconstructionInput() const
{
    ReconstructionInput input;
    input.type = ReconstructionInput::Type::Video;
    input.sourcePath = m_videoInfo.path;
    input.imagePaths = m_snapshot.quality.acceptedFramePaths;
    input.workingDirectory = m_snapshot.jobDirectory;
    input.frameManifestPath = m_snapshot.quality.acceptedManifestPath;
    input.targetFrames = m_snapshot.targetFrames;
    input.durationSeconds = m_videoInfo.durationSeconds;
    input.rawFrameCount = m_snapshot.quality.rawFrameCount;
    input.blurRejectedCount = m_snapshot.quality.blurRejectedCount;
    input.duplicateRejectedCount = m_snapshot.quality.duplicateRejectedCount;
    input.coverageRecoveredCount = m_snapshot.quality.coverageRecoveredCount;
    return input;
}

void VideoReconstructionController::setState(State state, const QString& statusText)
{
    m_snapshot.state = state;
    m_snapshot.statusText = statusText;
    emit snapshotChanged();
}

void VideoReconstructionController::onProbeFinished(const VideoInfo& info)
{
    m_videoInfo = info;
    m_snapshot.info = info;
    m_snapshot.hasInfo = true;
    m_snapshot.recommendedFrames = FrameSamplingPolicy::recommendedTargetFrames(info.durationSeconds);
    m_snapshot.targetFrames = m_snapshot.recommendedFrames;
    const QString detail = QStringLiteral("视频信息已读取：%1×%2，%3 FPS，%4 秒，%5")
                               .arg(info.width)
                               .arg(info.height)
                               .arg(info.fps, 0, 'f', 2)
                               .arg(info.durationSeconds, 0, 'f', 1)
                               .arg(info.codec);
    setState(State::Ready, detail);
    emit logMessage(detail, false);
}

void VideoReconstructionController::onProbeFailed(const QString& userMessage,
                                                  const QString& detail)
{
    m_snapshot.errorText = userMessage;
    setState(State::Failed, userMessage);
    emit logMessage(userMessage, true);
    if (!detail.trimmed().isEmpty()) {
        emit logMessage(QStringLiteral("ffprobe 详细信息：%1").arg(detail.trimmed()), true);
    }
}

void VideoReconstructionController::onProbeCancelled()
{
    setState(State::Cancelled, QStringLiteral("视频读取已取消。"));
}

void VideoReconstructionController::onExtractProgress(int frameCount, int targetFrames)
{
    m_snapshot.progress = targetFrames > 0
                              ? qBound(0, frameCount * 100 / targetFrames, 100)
                              : 0;
    m_snapshot.statusText = QStringLiteral("正在提取关键帧：%1 / %2")
                                .arg(frameCount)
                                .arg(targetFrames);
    emit snapshotChanged();
}

void VideoReconstructionController::onExtractFinished(const QStringList& framePaths)
{
    m_snapshot.quality.rawFrameCount = framePaths.size();
    m_snapshot.progress = 0;
    m_snapshot.state = State::Analyzing;
    m_snapshot.statusText = QStringLiteral("正在检查图像清晰度、重复帧和时间覆盖…");
    emit snapshotChanged();
    emit logMessage(QStringLiteral("抽帧完成：raw=%1，开始图像质量检查。")
                        .arg(framePaths.size()),
                    false);
    startQualityAnalysis(framePaths);
}

void VideoReconstructionController::onExtractFailed(const QString& userMessage,
                                                    const QString& detail)
{
    m_snapshot.errorText = userMessage;
    setState(State::Failed, userMessage);
    emit logMessage(userMessage, true);
    if (!detail.trimmed().isEmpty()) {
        const QString logDirectory = QDir(m_jobPaths.root).filePath(QStringLiteral("logs"));
        QDir().mkpath(logDirectory);
        QFile log(QDir(logDirectory).filePath(QStringLiteral("video_ffmpeg_stderr.log")));
        if (log.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            log.write(detail.toUtf8());
        }
        emit logMessage(QStringLiteral("ffmpeg 详细错误已写入任务日志。"), false);
    }
}

void VideoReconstructionController::onExtractCancelled()
{
    setState(State::Cancelled, QStringLiteral("视频抽帧已取消，已生成文件保留在任务目录。"));
}

void VideoReconstructionController::startQualityAnalysis(const QStringList& framePaths)
{
    m_cancelAnalysis.store(false);
    const QString sourceVideo = m_videoInfo.path;
    const double duration = m_videoInfo.durationSeconds;
    const double sampleFps = FrameSamplingPolicy::sampleFps(m_snapshot.targetFrames, duration);
    const QString jobDirectory = m_jobPaths.root;
    const auto run = std::make_shared<AnalysisRun>();
    QThread* thread = QThread::create([this, run, framePaths, sourceVideo, duration,
                                       sampleFps, jobDirectory] {
        run->success = FrameQualityAnalyzer::analyze(
            framePaths,
            sourceVideo,
            duration,
            sampleFps,
            jobDirectory,
            &run->summary,
            [this](int current, int total) {
                QMetaObject::invokeMethod(this, [this, current, total] {
                    if (m_snapshot.state != State::Analyzing) {
                        return;
                    }
                    m_snapshot.progress = total > 0 ? current * 100 / total : 0;
                    m_snapshot.statusText = QStringLiteral("正在检查图像质量：%1 / %2")
                                                .arg(current)
                                                .arg(total);
                    emit snapshotChanged();
                }, Qt::QueuedConnection);
            },
            &m_cancelAnalysis,
            &run->error);
    });
    m_analysisThread = thread;
    connect(thread, &QThread::finished, this, [this, thread, run] {
        if (m_analysisThread == thread) {
            m_analysisThread = nullptr;
        }
        finishQualityAnalysis(run->success, run->summary, run->error);
        thread->deleteLater();
    });
    thread->start();
}

void VideoReconstructionController::finishQualityAnalysis(bool success,
                                                          const FrameQualitySummary& summary,
                                                          const QString& error)
{
    if (m_cancelAnalysis.load()) {
        setState(State::Cancelled, QStringLiteral("图像质量检查已取消，已生成文件保留在任务目录。"));
        return;
    }
    if (!success) {
        m_snapshot.errorText = error;
        setState(State::Failed, error);
        emit logMessage(error, true);
        return;
    }
    m_snapshot.quality = summary;
    m_snapshot.progress = 100;
    const QString detail = QStringLiteral("视频抽帧完成：候选 %1，严重模糊 %2，重复 %3，时间覆盖恢复 %4，最终有效 %5。")
                               .arg(summary.rawFrameCount)
                               .arg(summary.blurRejectedCount)
                               .arg(summary.duplicateRejectedCount)
                               .arg(summary.coverageRecoveredCount)
                               .arg(summary.acceptedFrameCount);
    setState(State::PreviewReady, detail);
    emit logMessage(detail, false);
    if (summary.acceptedFrameCount < MinimumVideoReconstructionFrames) {
        const QString warning = QStringLiteral("有效帧少于 %1 张，当前数据不适合自动启动 COLMAP。请换一个有明显视角变化的视频。")
                                    .arg(MinimumVideoReconstructionFrames);
        m_snapshot.errorText = warning;
        emit snapshotChanged();
        emit logMessage(warning, true);
    }
}

bool VideoReconstructionController::createJobWorkspace(QString* error)
{
    if (!m_projectManager->hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("请先打开或创建项目。");
        }
        return false;
    }
    const QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!ReconstructionWorkspace::create(m_projectManager->projectDirectory(),
                                         taskId,
                                         &m_jobPaths,
                                         error)) {
        return false;
    }
    for (const QString& directory : {QStringLiteral("source"),
                                     QStringLiteral("raw_frames"),
                                     QStringLiteral("rejected_frames/blur"),
                                     QStringLiteral("rejected_frames/duplicate"),
                                     QStringLiteral("accepted_frames"),
                                     QStringLiteral("manifests")}) {
        if (!QDir(m_jobPaths.root).mkpath(directory)) {
            if (error != nullptr) {
                *error = QStringLiteral("无法创建视频任务目录：%1")
                             .arg(QDir(m_jobPaths.root).filePath(directory));
            }
            return false;
        }
    }
    return true;
}

} // namespace vision3d
