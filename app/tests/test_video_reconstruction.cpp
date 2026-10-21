#include "core/video/FrameQualityAnalyzer.h"
#include "core/video/FrameSamplingPolicy.h"
#include "core/video/VideoFrameExtractor.h"
#include "core/video/VideoProbe.h"
#include "core/video/VideoToolBackend.h"

#include <QColor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <cmath>

using namespace vision3d;

namespace {

struct ProbeOutcome
{
    bool finished = false;
    VideoInfo info;
    QString error;
    QString detail;
};

struct ExtractionOutcome
{
    bool finished = false;
    bool cancelled = false;
    bool timedOut = false;
    QStringList paths;
    QString error;
    QString detail;
};

QString syntheticTestRoot()
{
    const QString root = QDir(QDir::tempPath()).filePath(QStringLiteral("vision3d-video-tests"));
    QDir().mkpath(root);
    return root;
}

bool runProcess(const QString& program,
                const QStringList& arguments,
                QString* error,
                int timeoutMs = 120000)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(15000)) {
        if (error != nullptr) {
            *error = process.errorString();
        }
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(5000);
        if (error != nullptr) {
            *error = QStringLiteral("进程超时：%1").arg(program);
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error != nullptr) {
            *error = QString::fromUtf8(process.readAllStandardError());
        }
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool makeSyntheticVideo(const VideoToolPaths& tools,
                        const QString& path,
                        const QString& source,
                        QString* error)
{
    return runProcess(tools.ffmpeg,
                      {QStringLiteral("-hide_banner"),
                       QStringLiteral("-loglevel"),
                       QStringLiteral("error"),
                       QStringLiteral("-y"),
                       QStringLiteral("-f"),
                       QStringLiteral("lavfi"),
                       QStringLiteral("-i"),
                       source,
                       QStringLiteral("-an"),
                       QStringLiteral("-c:v"),
                       QStringLiteral("mpeg4"),
                       QStringLiteral("-q:v"),
                       QStringLiteral("8"),
                       QStringLiteral("-pix_fmt"),
                       QStringLiteral("yuv420p"),
                       path},
                      error);
}

bool probeVideo(const VideoToolPaths& tools,
                const QString& path,
                ProbeOutcome* outcome,
                int timeoutMs = 20000)
{
    VideoProbe probe;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&probe, &VideoProbe::finished, &loop, [&](const VideoInfo& info) {
        outcome->finished = true;
        outcome->info = info;
        loop.quit();
    });
    QObject::connect(&probe, &VideoProbe::failed, &loop,
                     [&](const QString& message, const QString& detail) {
                         outcome->error = message;
                         outcome->detail = detail;
                         loop.quit();
                     });
    QObject::connect(&probe, &VideoProbe::cancelled, &loop, [&] {
        outcome->error = QStringLiteral("视频解析意外取消。");
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        outcome->error = QStringLiteral("ffprobe 等待超时。");
        loop.quit();
    });
    QString startError;
    if (!probe.start(path, tools, &startError)) {
        outcome->error = startError;
        return false;
    }
    timeout.start(timeoutMs);
    loop.exec();
    return outcome->finished;
}

bool extractVideo(const VideoToolPaths& tools,
                  const VideoInfo& info,
                  int targetFrames,
                  const QString& outputDirectory,
                  ExtractionOutcome* outcome,
                  bool cancelAfterProgress = false,
                  int timeoutMs = 45000)
{
    VideoFrameExtractor extractor;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&extractor, &VideoFrameExtractor::finished, &loop,
                     [&](const QStringList& paths) {
                         outcome->finished = true;
                         outcome->paths = paths;
                         loop.quit();
                     });
    QObject::connect(&extractor, &VideoFrameExtractor::failed, &loop,
                     [&](const QString& message, const QString& detail) {
                         outcome->error = message;
                         outcome->detail = detail;
                         loop.quit();
                     });
    QObject::connect(&extractor, &VideoFrameExtractor::cancelled, &loop, [&] {
        outcome->cancelled = true;
        loop.quit();
    });
    QObject::connect(&extractor, &VideoFrameExtractor::progress, &loop,
                     [&](int frameCount, int) {
                         if (frameCount == 1) {
                         }
                         if (cancelAfterProgress && frameCount > 0) {
                             extractor.cancel();
                         }
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        outcome->timedOut = true;
        extractor.cancel();
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    });
    QString startError;
    if (!extractor.start(tools, info, targetFrames, outputDirectory, &startError)) {
        outcome->error = startError;
        return false;
    }
    timeout.start(timeoutMs);
    loop.exec();
    return outcome->finished || outcome->cancelled;
}

bool analyzeFrames(const QStringList& paths,
                   const VideoInfo& info,
                   int targetFrames,
                   const QString& jobDirectory,
                   FrameQualitySummary* summary,
                   QString* error)
{
    return FrameQualityAnalyzer::analyze(paths,
                                         info.path,
                                         info.durationSeconds,
                                         FrameSamplingPolicy::sampleFps(targetFrames,
                                                                        info.durationSeconds),
                                         jobDirectory,
                                         summary,
                                         {},
                                         nullptr,
                                         error);
}

} // namespace

class VideoReconstructionTest final : public QObject
{
    Q_OBJECT

private slots:
    void samplingPolicyUsesBoundedTargets();
    void probeParsesJsonAndRationalFps();
    void probeRejectsInvalidJsonAndMissingVideoStream();
    void toolLookupRequiresBothLocalExecutables();
    void extractionArgumentsPreserveSourceResolution();
    void analyzerWritesQualityManifestsAndPreview();
    void liveTenSecondVideoProbeExtractAndAnalyze();
    void liveFortyFiveSecondVideoProbeAndExtract();
    void liveExtractionCanBeCancelled();
    void invalidVideoFailsWithoutCrashing();
    void invalidFfmpegPathFailsWithoutCrashing();
    void extremelyBlurryVideoProducesQualityReview();
    void staticVideoIsReducedAndCannotStartReconstruction();
};

void VideoReconstructionTest::samplingPolicyUsesBoundedTargets()
{
    QCOMPARE(FrameSamplingPolicy::recommendedTargetFrames(10.0), 120);
    QCOMPARE(FrameSamplingPolicy::recommendedTargetFrames(30.0), 180);
    QCOMPARE(FrameSamplingPolicy::recommendedTargetFrames(60.0), 240);
    QCOMPARE(FrameSamplingPolicy::recommendedTargetFrames(80.0), 330);
    QCOMPARE(FrameSamplingPolicy::recommendedTargetFrames(120.0), 450);
    QCOMPARE(FrameSamplingPolicy::clampManualTargetFrames(10), 100);
    QCOMPARE(FrameSamplingPolicy::clampManualTargetFrames(700), 500);
    QVERIFY(qAbs(FrameSamplingPolicy::sampleFps(240, 60.0) - 4.0) < 0.0001);
}

void VideoReconstructionTest::probeParsesJsonAndRationalFps()
{
    const QByteArray payload = R"({
      "streams": [{
        "codec_name": "h264",
        "width": 3840,
        "height": 2160,
        "avg_frame_rate": "30000/1001",
        "r_frame_rate": "30/1",
        "nb_frames": "1800",
        "duration": "60.060000"
      }],
      "format": {"duration": "60.060000"}
    })";
    VideoInfo info;
    QString error;
    QVERIFY2(VideoProbe::parseJson(QStringLiteral("D:/sample/equipment.mp4"),
                                   payload,
                                   &info,
                                   &error),
             qPrintable(error));
    QCOMPARE(info.width, 3840);
    QCOMPARE(info.height, 2160);
    QCOMPARE(info.codec, QStringLiteral("h264"));
    QCOMPARE(info.frameCount, qint64(1800));
    QVERIFY(qAbs(info.fps - 29.97002997) < 0.001);
    QVERIFY(qAbs(info.durationSeconds - 60.06) < 0.001);
    QVERIFY(qAbs(VideoProbe::parseRate(QStringLiteral("24000/1001")) - 23.976) < 0.001);
}

void VideoReconstructionTest::probeRejectsInvalidJsonAndMissingVideoStream()
{
    VideoInfo info;
    QString error;
    QVERIFY(!VideoProbe::parseJson(QStringLiteral("broken.mp4"),
                                   QByteArrayLiteral("not-json"),
                                   &info,
                                   &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!VideoProbe::parseJson(QStringLiteral("no-video.mp4"),
                                   QByteArrayLiteral("{\"streams\": []}"),
                                   &info,
                                   &error));
    QVERIFY(error.contains(QStringLiteral("没有可读取的视频流")));
}

void VideoReconstructionTest::toolLookupRequiresBothLocalExecutables()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
#ifdef Q_OS_WIN
    const QString suffix = QStringLiteral(".exe");
#else
    const QString suffix;
#endif
    const QString ffmpeg = directory.filePath(QStringLiteral("ffmpeg") + suffix);
    const QString ffprobe = directory.filePath(QStringLiteral("ffprobe") + suffix);
    QFile ffmpegFile(ffmpeg);
    QVERIFY(ffmpegFile.open(QIODevice::WriteOnly));
    ffmpegFile.write("stub");
    ffmpegFile.close();

    QString error;
    QVERIFY(VideoToolBackend::locateInDirectory(directory.path(), &error).ffmpeg.isEmpty());
    QVERIFY(error.contains(QStringLiteral("ffmpeg 或 ffprobe")));

    QFile ffprobeFile(ffprobe);
    QVERIFY(ffprobeFile.open(QIODevice::WriteOnly));
    ffprobeFile.write("stub");
    ffprobeFile.close();
    const VideoToolPaths tools = VideoToolBackend::locateInDirectory(directory.path(), &error);
    QVERIFY2(!tools.ffmpeg.isEmpty() && !tools.ffprobe.isEmpty(), qPrintable(error));
    QVERIFY(QFileInfo(tools.ffmpeg).isAbsolute());
    QVERIFY(QFileInfo(tools.ffprobe).isAbsolute());
}

void VideoReconstructionTest::extractionArgumentsPreserveSourceResolution()
{
    VideoInfo info;
    info.path = QStringLiteral("D:/视频/equipment.mp4");
    info.width = 3840;
    info.height = 2160;
    info.durationSeconds = 60.0;
    const QStringList arguments = VideoFrameExtractor::arguments(
        info, 240, QStringLiteral("D:/项目/job/raw_frames"));
    QVERIFY(arguments.contains(QFileInfo(info.path).absoluteFilePath()));
    QVERIFY(arguments.contains(QStringLiteral("fps=4.00000000")));
    QVERIFY(arguments.contains(QStringLiteral("-frames:v")));
    QVERIFY(arguments.contains(QStringLiteral("240")));
    QVERIFY(arguments.contains(QStringLiteral("-stats_period")));
    QVERIFY(arguments.contains(QStringLiteral("0.05")));
    QVERIFY(arguments.last().endsWith(QStringLiteral("frame_%06d.png")));
    QVERIFY(!arguments.join(QLatin1Char(' ')).contains(QStringLiteral("scale=")));
}

void VideoReconstructionTest::analyzerWritesQualityManifestsAndPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString rawDirectory = directory.filePath(QStringLiteral("raw_frames"));
    QVERIFY(QDir().mkpath(rawDirectory));
    QStringList paths;
    for (int frameIndex = 0; frameIndex < 12; ++frameIndex) {
        QImage frame(128, 128, QImage::Format_RGB32);
        frame.fill(QColor(100, 100, 100));
        QPainter painter(&frame);
        if (frameIndex < 6) {
            for (int y = 0; y < 128; y += 8) {
                for (int x = 0; x < 128; x += 8) {
                    if (((x / 8) + (y / 8)) % 2 == 0) {
                        painter.fillRect(x, y, 8, 8, Qt::white);
                    }
                }
            }
        } else if (frameIndex < 11) {
            painter.setPen(QPen(Qt::white, 3));
            for (int x = 4 + (frameIndex - 6) * 9; x < 128; x += 21) {
                painter.drawLine(x, 0, 127 - x / 2, 127);
            }
            painter.setBrush(Qt::white);
            painter.drawEllipse(QPoint(20 + frameIndex * 7, 64), 15, 15);
        }
        painter.end();
        const QString path = QDir(rawDirectory).filePath(
            QStringLiteral("frame_%1.png").arg(frameIndex + 1, 6, 10, QLatin1Char('0')));
        QVERIFY(frame.save(path, "PNG"));
        paths.append(path);
    }

    FrameQualitySummary summary;
    QString error;
    QVERIFY2(FrameQualityAnalyzer::analyze(paths,
                                           QStringLiteral("equipment.mp4"),
                                           12.0,
                                           1.0,
                                           directory.path(),
                                           &summary,
                                           {},
                                           nullptr,
                                           &error),
             qPrintable(error));
    QCOMPARE(summary.rawFrameCount, 12);
    QVERIFY(summary.blurRejectedCount >= 1);
    QVERIFY(summary.duplicateRejectedCount >= 1);
    QVERIFY(summary.coverageRecoveredCount >= 1);
    QVERIFY(summary.acceptedFrameCount >= 4);
    QVERIFY(QFileInfo(summary.frameManifestPath).isFile());
    QVERIFY(QFileInfo(summary.acceptedManifestPath).isFile());
    QVERIFY(QFileInfo(summary.contactSheetPath).isFile());
    QCOMPARE(summary.acceptedFramePaths.size(), summary.acceptedFrameCount);
}

void VideoReconstructionTest::liveTenSecondVideoProbeExtractAndAnalyze()
{
    QString toolError;
    const VideoToolPaths tools = VideoToolBackend::locate(&toolError);
    QVERIFY2(!tools.ffmpeg.isEmpty() && !tools.ffprobe.isEmpty(), qPrintable(toolError));
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("ten-second-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString videoPath = directory.filePath(QStringLiteral("十秒短视频.mp4"));
    QString error;
    const bool generated = makeSyntheticVideo(
        tools,
        videoPath,
        QStringLiteral("testsrc2=size=320x180:rate=30:duration=10"),
        &error);
    QVERIFY2(generated, qPrintable(error));

    ProbeOutcome probe;
    QVERIFY2(probeVideo(tools, videoPath, &probe), qPrintable(probe.error));
    QCOMPARE(probe.info.width, 320);
    QCOMPARE(probe.info.height, 180);
    QVERIFY(std::abs(probe.info.durationSeconds - 10.0) < 0.1);
    QCOMPARE(FrameSamplingPolicy::recommendedTargetFrames(probe.info.durationSeconds), 120);

    ExtractionOutcome extraction;
    QVERIFY2(extractVideo(tools,
                          probe.info,
                          120,
                          directory.filePath(QStringLiteral("raw_frames")),
                          &extraction),
             qPrintable(extraction.error));
    QVERIFY(!extraction.timedOut);
    QCOMPARE(extraction.paths.size(), 120);
    QCOMPARE(QFileInfo(extraction.paths.first()).fileName(), QStringLiteral("frame_000001.png"));
    QCOMPARE(QFileInfo(extraction.paths.last()).fileName(), QStringLiteral("frame_000120.png"));
    QCOMPARE(QImage(extraction.paths.first()).size(), QSize(320, 180));

    FrameQualitySummary summary;
    QVERIFY2(analyzeFrames(extraction.paths, probe.info, 120, directory.path(), &summary, &error),
             qPrintable(error));
    QCOMPARE(summary.rawFrameCount, 120);
    QVERIFY(summary.acceptedFrameCount > 0);
    QVERIFY(QFileInfo(summary.frameManifestPath).isFile());
    QVERIFY(QFileInfo(summary.acceptedManifestPath).isFile());
    QVERIFY(QFileInfo(summary.contactSheetPath).isFile());
    QFile manifest(summary.frameManifestPath);
    QVERIFY(manifest.open(QIODevice::ReadOnly));
    const QJsonObject document = QJsonDocument::fromJson(manifest.readAll()).object();
    QCOMPARE(document.value(QStringLiteral("rawFrameCount")).toInt(), 120);
    QCOMPARE(document.value(QStringLiteral("sourceVideo")).toString(), probe.info.path);
}

void VideoReconstructionTest::liveFortyFiveSecondVideoProbeAndExtract()
{
    QString toolError;
    const VideoToolPaths tools = VideoToolBackend::locate(&toolError);
    QVERIFY2(!tools.ffmpeg.isEmpty() && !tools.ffprobe.isEmpty(), qPrintable(toolError));
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("forty-five-second-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString videoPath = directory.filePath(QStringLiteral("medium-test-video.mp4"));
    QString error;
    QVERIFY2(makeSyntheticVideo(tools,
                                videoPath,
                                QStringLiteral("testsrc2=size=320x180:rate=15:duration=45"),
                                &error),
             qPrintable(error));

    ProbeOutcome probe;
    QVERIFY2(probeVideo(tools, videoPath, &probe), qPrintable(probe.error));
    QVERIFY(probe.info.durationSeconds >= 30.0 && probe.info.durationSeconds <= 60.0);
    QCOMPARE(probe.info.width, 320);
    QCOMPARE(probe.info.height, 180);
    const int targetFrames = FrameSamplingPolicy::recommendedTargetFrames(
        probe.info.durationSeconds);
    QCOMPARE(targetFrames, 240);
    QVERIFY(qAbs(FrameSamplingPolicy::sampleFps(targetFrames, probe.info.durationSeconds)
                 - (240.0 / 45.0)) < 0.001);

    ExtractionOutcome extraction;
    QVERIFY2(extractVideo(tools,
                          probe.info,
                          targetFrames,
                          directory.filePath(QStringLiteral("raw_frames")),
                          &extraction),
             qPrintable(extraction.error));
    QVERIFY(!extraction.timedOut);
    QCOMPARE(extraction.paths.size(), targetFrames);
    QCOMPARE(QImage(extraction.paths.last()).size(), QSize(320, 180));
}

void VideoReconstructionTest::liveExtractionCanBeCancelled()
{
    QString toolError;
    const VideoToolPaths tools = VideoToolBackend::locate(&toolError);
    QVERIFY2(!tools.ffmpeg.isEmpty() && !tools.ffprobe.isEmpty(), qPrintable(toolError));
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("cancel-test-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString videoPath = directory.filePath(QStringLiteral("cancel-source.mp4"));
    QString error;
    QVERIFY2(makeSyntheticVideo(tools,
                                videoPath,
                                QStringLiteral("testsrc2=size=1280x720:rate=30:duration=45"),
                                &error),
             qPrintable(error));
    ProbeOutcome probe;
    QVERIFY2(probeVideo(tools, videoPath, &probe), qPrintable(probe.error));

    ExtractionOutcome extraction;
    QVERIFY(extractVideo(tools,
                         probe.info,
                         500,
                         directory.filePath(QStringLiteral("raw_frames")),
                         &extraction,
                         true));
    const int cancelledFrameCount = QDir(directory.filePath(QStringLiteral("raw_frames")))
                                        .entryList({QStringLiteral("frame_*.png")}, QDir::Files)
                                        .size();
    QVERIFY(!extraction.timedOut);
    QVERIFY(extraction.cancelled);
    QVERIFY(!extraction.finished);
    QVERIFY(cancelledFrameCount < 500);
}

void VideoReconstructionTest::invalidVideoFailsWithoutCrashing()
{
    QString toolError;
    const VideoToolPaths tools = VideoToolBackend::locate(&toolError);
    QVERIFY2(!tools.ffprobe.isEmpty(), qPrintable(toolError));
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("invalid-video-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString invalidPath = directory.filePath(QStringLiteral("invalid.mp4"));
    QFile invalid(invalidPath);
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write("not a video stream");
    invalid.close();

    ProbeOutcome probe;
    QVERIFY(!probeVideo(tools, invalidPath, &probe));
    QVERIFY(!probe.error.isEmpty());
}

void VideoReconstructionTest::invalidFfmpegPathFailsWithoutCrashing()
{
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("invalid-ffmpeg-path-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString inputPath = directory.filePath(QStringLiteral("source.mp4"));
    QFile input(inputPath);
    QVERIFY(input.open(QIODevice::WriteOnly));
    input.write("placeholder");
    input.close();

    VideoInfo info;
    info.path = inputPath;
    info.durationSeconds = 10.0;
    info.width = 320;
    info.height = 180;
    VideoToolPaths tools;
    tools.ffmpeg = directory.filePath(QStringLiteral("missing-ffmpeg.exe"));
    QString error;
    VideoFrameExtractor extractor;
    QVERIFY(!extractor.start(tools,
                            info,
                            120,
                            directory.filePath(QStringLiteral("raw_frames")),
                            &error));
    QVERIFY(error.contains(QStringLiteral("ffmpeg 不存在")));
}

void VideoReconstructionTest::extremelyBlurryVideoProducesQualityReview()
{
    QString toolError;
    const VideoToolPaths tools = VideoToolBackend::locate(&toolError);
    QVERIFY2(!tools.ffmpeg.isEmpty() && !tools.ffprobe.isEmpty(), qPrintable(toolError));
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("blur-test-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString videoPath = directory.filePath(QStringLiteral("extremely-blurry.mp4"));
    QString error;
    const bool generated = makeSyntheticVideo(
        tools,
        videoPath,
        QStringLiteral("testsrc2=size=320x180:rate=15:duration=10,gblur=sigma=20"),
        &error);
    QVERIFY2(generated, qPrintable(error));
    ProbeOutcome probe;
    QVERIFY2(probeVideo(tools, videoPath, &probe), qPrintable(probe.error));
    const int targetFrames = FrameSamplingPolicy::recommendedTargetFrames(
        probe.info.durationSeconds);
    ExtractionOutcome extraction;
    QVERIFY2(extractVideo(tools,
                          probe.info,
                          targetFrames,
                          directory.filePath(QStringLiteral("raw_frames")),
                          &extraction),
             qPrintable(extraction.error));
    FrameQualitySummary summary;
    QVERIFY2(analyzeFrames(extraction.paths,
                           probe.info,
                           targetFrames,
                           directory.path(),
                           &summary,
                           &error),
             qPrintable(error));
    QVERIFY(std::isfinite(summary.blurMedian));
    QVERIFY(summary.rawFrameCount == targetFrames);
    QVERIFY(summary.acceptedFrameCount > 0);
    QVERIFY(QFileInfo(summary.frameManifestPath).isFile());
}

void VideoReconstructionTest::staticVideoIsReducedAndCannotStartReconstruction()
{
    QString toolError;
    const VideoToolPaths tools = VideoToolBackend::locate(&toolError);
    QVERIFY2(!tools.ffmpeg.isEmpty() && !tools.ffprobe.isEmpty(), qPrintable(toolError));
    QTemporaryDir directory(QDir(syntheticTestRoot()).filePath(
        QStringLiteral("static-test-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString videoPath = directory.filePath(QStringLiteral("static.mp4"));
    QString error;
    QVERIFY2(makeSyntheticVideo(tools,
                                videoPath,
                                QStringLiteral("color=c=gray:s=320x180:r=15:d=10"),
                                &error),
             qPrintable(error));
    ProbeOutcome probe;
    QVERIFY2(probeVideo(tools, videoPath, &probe), qPrintable(probe.error));
    const int targetFrames = FrameSamplingPolicy::recommendedTargetFrames(
        probe.info.durationSeconds);
    ExtractionOutcome extraction;
    QVERIFY2(extractVideo(tools,
                          probe.info,
                          targetFrames,
                          directory.filePath(QStringLiteral("raw_frames")),
                          &extraction),
             qPrintable(extraction.error));
    QCOMPARE(extraction.paths.size(), targetFrames);

    FrameQualitySummary summary;
    QVERIFY2(analyzeFrames(extraction.paths,
                           probe.info,
                           targetFrames,
                           directory.path(),
                           &summary,
                           &error),
             qPrintable(error));
    QVERIFY(summary.duplicateRejectedCount >= 100);
    QVERIFY(summary.coverageRecoveredCount >= 3);
    QVERIFY(summary.acceptedFrameCount < 20);
    QVERIFY(!summary.acceptedFramePaths.isEmpty());
}

QTEST_MAIN(VideoReconstructionTest)
#include "test_video_reconstruction.moc"
