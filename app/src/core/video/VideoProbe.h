#pragma once

#include "core/video/VideoInfo.h"
#include "core/video/VideoToolBackend.h"

#include <QObject>
#include <QProcess>

namespace vision3d {

class VideoProbe final : public QObject
{
    Q_OBJECT

public:
    explicit VideoProbe(QObject* parent = nullptr);
    bool start(const QString& videoPath, const VideoToolPaths& tools, QString* error = nullptr);
    void cancel();
    bool isRunning() const;

    static QStringList arguments(const QString& videoPath);
    static bool parseJson(const QString& videoPath,
                          const QByteArray& json,
                          VideoInfo* info,
                          QString* error = nullptr);
    static double parseRate(const QString& value);

signals:
    void finished(const vision3d::VideoInfo& info);
    void failed(const QString& userMessage, const QString& detail);
    void cancelled();

private:
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onError(QProcess::ProcessError error);

    QProcess m_process;
    QString m_videoPath;
    QString m_toolVersion;
    QByteArray m_stdout;
    QByteArray m_stderr;
    bool m_cancelRequested = false;
    bool m_versionPending = false;
};

} // namespace vision3d

Q_DECLARE_METATYPE(vision3d::VideoInfo)
