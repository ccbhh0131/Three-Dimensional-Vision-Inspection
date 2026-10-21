#include "core/video/VideoProbe.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace vision3d {

VideoProbe::VideoProbe(QObject* parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_stdout.append(m_process.readAllStandardOutput());
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderr.append(m_process.readAllStandardError());
    });
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &VideoProbe::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &VideoProbe::onError);
}

bool VideoProbe::start(const QString& videoPath, const VideoToolPaths& tools, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (isRunning()) {
        if (error != nullptr) {
            *error = QStringLiteral("正在读取另一个视频。");
        }
        return false;
    }
    if (!QFileInfo(videoPath).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("视频文件不存在：%1").arg(videoPath);
        }
        return false;
    }
    if (tools.ffprobe.isEmpty() || !QFileInfo(tools.ffprobe).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("ffprobe 不存在。请检查 FFmpeg 本地部署目录。");
        }
        return false;
    }
    m_videoPath = QFileInfo(videoPath).absoluteFilePath();
    m_toolVersion.clear();
    m_stdout.clear();
    m_stderr.clear();
    m_cancelRequested = false;
    m_versionPending = true;
    m_process.setProgram(tools.ffprobe);
    m_process.setArguments({QStringLiteral("-version")});
    m_process.setWorkingDirectory(QFileInfo(tools.ffprobe).absolutePath());
    m_process.start();
    return true;
}

void VideoProbe::cancel()
{
    if (!isRunning()) {
        return;
    }
    m_cancelRequested = true;
    m_process.terminate();
    QTimer::singleShot(1500, this, [this] {
        if (isRunning()) {
            m_process.kill();
        }
    });
}

bool VideoProbe::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QStringList VideoProbe::arguments(const QString& videoPath)
{
    return {QStringLiteral("-v"),
            QStringLiteral("error"),
            QStringLiteral("-select_streams"),
            QStringLiteral("v:0"),
            QStringLiteral("-show_entries"),
            QStringLiteral("stream=codec_name,width,height,avg_frame_rate,r_frame_rate,nb_frames,duration:format=duration"),
            QStringLiteral("-print_format"),
            QStringLiteral("json"),
            videoPath};
}

double VideoProbe::parseRate(const QString& value)
{
    const QStringList terms = value.split(QLatin1Char('/'));
    bool numeratorOk = false;
    bool denominatorOk = false;
    const double numerator = terms.value(0).toDouble(&numeratorOk);
    const double denominator = terms.size() == 2 ? terms.at(1).toDouble(&denominatorOk) : 1.0;
    if (!numeratorOk || (terms.size() == 2 && !denominatorOk) || denominator <= 0.0) {
        return 0.0;
    }
    return numerator / denominator;
}

bool VideoProbe::parseJson(const QString& videoPath,
                           const QByteArray& json,
                           VideoInfo* info,
                           QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (info == nullptr) {
        return fail(QStringLiteral("视频信息输出参数为空。"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("视频信息 JSON 无法解析：%1").arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    if (streams.isEmpty()) {
        return fail(QStringLiteral("视频中没有可读取的视频流。"));
    }
    const QJsonObject stream = streams.first().toObject();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();

    VideoInfo result;
    result.path = QFileInfo(videoPath).absoluteFilePath();
    result.width = stream.value(QStringLiteral("width")).toInt();
    result.height = stream.value(QStringLiteral("height")).toInt();
    result.fps = parseRate(stream.value(QStringLiteral("avg_frame_rate")).toString());
    if (result.fps <= 0.0) {
        result.fps = parseRate(stream.value(QStringLiteral("r_frame_rate")).toString());
    }
    result.codec = stream.value(QStringLiteral("codec_name")).toString();
    result.durationSeconds = stream.value(QStringLiteral("duration")).toString().toDouble();
    if (result.durationSeconds <= 0.0) {
        result.durationSeconds = format.value(QStringLiteral("duration")).toString().toDouble();
    }
    bool frameCountOk = false;
    result.frameCount = stream.value(QStringLiteral("nb_frames")).toString().toLongLong(&frameCountOk);
    if (!frameCountOk || result.frameCount <= 0) {
        result.frameCount = result.fps > 0.0 && result.durationSeconds > 0.0
                                ? qRound64(result.fps * result.durationSeconds)
                                : 0;
    }
    if (result.width <= 0 || result.height <= 0 || result.durationSeconds <= 0.0
        || result.fps <= 0.0 || result.codec.isEmpty()) {
        return fail(QStringLiteral("视频流缺少有效的分辨率、帧率、时长或编码信息。"));
    }
    *info = result;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void VideoProbe::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_stdout.append(m_process.readAllStandardOutput());
    m_stderr.append(m_process.readAllStandardError());
    if (m_cancelRequested) {
        emit cancelled();
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        emit failed(QStringLiteral("无法读取视频信息，请确认视频格式受支持且文件未损坏。"),
                    QString::fromUtf8(m_stderr));
        return;
    }
    if (m_versionPending) {
        const QStringList lines = QString::fromUtf8(m_stdout).split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (line.trimmed().startsWith(QStringLiteral("ffprobe version"),
                                          Qt::CaseInsensitive)) {
                m_toolVersion = line.trimmed();
                break;
            }
        }
        if (m_toolVersion.isEmpty()) {
            emit failed(QStringLiteral("无法识别本地 ffprobe 版本。"), QString::fromUtf8(m_stderr));
            return;
        }
        m_versionPending = false;
        m_stdout.clear();
        m_stderr.clear();
        m_process.setArguments(arguments(m_videoPath));
        m_process.start();
        return;
    }
    VideoInfo info;
    QString error;
    if (!parseJson(m_videoPath, m_stdout, &info, &error)) {
        emit failed(error, QString::fromUtf8(m_stderr));
        return;
    }
    info.toolVersion = m_toolVersion;
    emit finished(info);
}

void VideoProbe::onError(QProcess::ProcessError error)
{
    if (m_cancelRequested && error == QProcess::Crashed) {
        return;
    }
    if (error == QProcess::FailedToStart) {
        emit failed(QStringLiteral("ffprobe 无法启动，请检查本地 FFmpeg 文件和运行库。"),
                    m_process.errorString());
    }
}

} // namespace vision3d
