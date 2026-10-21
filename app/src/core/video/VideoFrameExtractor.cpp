#include "core/video/VideoFrameExtractor.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace vision3d {

VideoFrameExtractor::VideoFrameExtractor(QObject* parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &VideoFrameExtractor::onStdout);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderr.append(m_process.readAllStandardError());
    });
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &VideoFrameExtractor::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &VideoFrameExtractor::onError);
}

bool VideoFrameExtractor::start(const VideoToolPaths& tools,
                                const VideoInfo& info,
                                int targetFrames,
                                const QString& outputDirectory,
                                QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (isRunning()) {
        if (error != nullptr) {
            *error = QStringLiteral("已有视频抽帧进程正在运行。");
        }
        return false;
    }
    if (!QFileInfo(info.path).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("视频文件不存在：%1").arg(info.path);
        }
        return false;
    }
    if (tools.ffmpeg.isEmpty() || !QFileInfo(tools.ffmpeg).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("ffmpeg 不存在。请检查 FFmpeg 本地部署目录。");
        }
        return false;
    }
    if (targetFrames < 2 || info.durationSeconds <= 0.0 || info.width <= 0 || info.height <= 0) {
        if (error != nullptr) {
            *error = QStringLiteral("抽帧参数或视频信息无效。");
        }
        return false;
    }
    if (!QDir().mkpath(outputDirectory)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法创建抽帧目录：%1").arg(outputDirectory);
        }
        return false;
    }
    const QStringList existing = QDir(outputDirectory).entryList(
        {QStringLiteral("frame_*.png")}, QDir::Files, QDir::Name);
    if (!existing.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("抽帧目录已包含旧图片，拒绝覆盖：%1").arg(outputDirectory);
        }
        return false;
    }

    m_outputDirectory = QDir(outputDirectory).absolutePath();
    m_targetFrames = targetFrames;
    m_reportedFrameCount = 0;
    m_stdoutBuffer.clear();
    m_stderr.clear();
    m_cancelRequested = false;
    m_process.setProgram(QFileInfo(tools.ffmpeg).absoluteFilePath());
    m_process.setArguments(arguments(info, targetFrames, m_outputDirectory));
    m_process.setWorkingDirectory(QFileInfo(tools.ffmpeg).absolutePath());
    m_process.start();
    return true;
}

void VideoFrameExtractor::cancel()
{
    if (!isRunning()) {
        return;
    }
    m_cancelRequested = true;
    m_process.kill();
}

bool VideoFrameExtractor::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

int VideoFrameExtractor::reportedFrameCount() const
{
    return m_reportedFrameCount;
}

QStringList VideoFrameExtractor::arguments(const VideoInfo& info,
                                          int targetFrames,
                                          const QString& outputDirectory)
{
    const double sampleFps = static_cast<double>(targetFrames) / info.durationSeconds;
    const QString rate = QString::number(sampleFps, 'f', 8);
    const QString pattern = QDir(outputDirectory).filePath(QStringLiteral("frame_%06d.png"));
    return {QStringLiteral("-hide_banner"),
            QStringLiteral("-nostdin"),
            QStringLiteral("-y"),
            QStringLiteral("-i"),
            QFileInfo(info.path).absoluteFilePath(),
            QStringLiteral("-vf"),
            QStringLiteral("fps=") + rate,
            QStringLiteral("-frames:v"),
            QString::number(targetFrames),
            QStringLiteral("-start_number"),
            QStringLiteral("1"),
            QStringLiteral("-stats_period"),
            QStringLiteral("0.05"),
            QStringLiteral("-progress"),
            QStringLiteral("pipe:1"),
            QStringLiteral("-nostats"),
            pattern};
}

void VideoFrameExtractor::onStdout()
{
    m_stdoutBuffer.append(m_process.readAllStandardOutput());
    while (true) {
        const qsizetype newline = m_stdoutBuffer.indexOf('\n');
        if (newline < 0) {
            break;
        }
        const QByteArray line = m_stdoutBuffer.left(newline).trimmed();
        m_stdoutBuffer.remove(0, newline + 1);
        if (line.startsWith("frame=")) {
            bool ok = false;
            const int frame = line.mid(6).trimmed().toInt(&ok);
            if (ok && frame > m_reportedFrameCount) {
                m_reportedFrameCount = frame;
                emit progress(frame, m_targetFrames);
            }
        }
    }
}

void VideoFrameExtractor::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    onStdout();
    m_stderr.append(m_process.readAllStandardError());
    if (m_cancelRequested) {
        emit cancelled();
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        emit failed(QStringLiteral("视频解码或抽帧失败，请检查视频文件和磁盘空间。"),
                    QString::fromUtf8(m_stderr));
        return;
    }
    const QFileInfoList files = QDir(m_outputDirectory).entryInfoList(
        {QStringLiteral("frame_*.png")}, QDir::Files, QDir::Name);
    if (files.size() < 2) {
        emit failed(QStringLiteral("视频没有提取到足够的有效图像帧。"),
                    QStringLiteral("抽取文件数：%1\n%2")
                        .arg(files.size())
                        .arg(QString::fromUtf8(m_stderr)));
        return;
    }
    QStringList paths;
    paths.reserve(files.size());
    for (const QFileInfo& file : files) {
        paths.append(file.absoluteFilePath());
    }
    emit progress(paths.size(), m_targetFrames);
    emit finished(paths);
}

void VideoFrameExtractor::onError(QProcess::ProcessError error)
{
    if (m_cancelRequested && error == QProcess::Crashed) {
        return;
    }
    if (error == QProcess::FailedToStart) {
        emit failed(QStringLiteral("ffmpeg 无法启动，请检查本地 FFmpeg 文件和运行库。"),
                    m_process.errorString());
    }
}

} // namespace vision3d
