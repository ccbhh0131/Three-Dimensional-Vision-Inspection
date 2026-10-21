#pragma once

#include "core/video/VideoInfo.h"
#include "core/video/VideoToolBackend.h"

#include <QObject>
#include <QProcess>

namespace vision3d {

class VideoFrameExtractor final : public QObject
{
    Q_OBJECT

public:
    explicit VideoFrameExtractor(QObject* parent = nullptr);
    bool start(const VideoToolPaths& tools,
               const VideoInfo& info,
               int targetFrames,
               const QString& outputDirectory,
               QString* error = nullptr);
    void cancel();
    bool isRunning() const;
    int reportedFrameCount() const;

    static QStringList arguments(const VideoInfo& info,
                                 int targetFrames,
                                 const QString& outputDirectory);

signals:
    void progress(int frameCount, int targetFrames);
    void finished(const QStringList& framePaths);
    void failed(const QString& userMessage, const QString& detail);
    void cancelled();

private:
    void onStdout();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onError(QProcess::ProcessError error);

    QProcess m_process;
    QString m_outputDirectory;
    int m_targetFrames = 0;
    int m_reportedFrameCount = 0;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderr;
    bool m_cancelRequested = false;
};

} // namespace vision3d
