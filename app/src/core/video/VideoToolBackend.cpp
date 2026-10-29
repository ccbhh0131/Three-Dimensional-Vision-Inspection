#include "core/video/VideoToolBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QOperatingSystemVersion>
#include <QStringList>

namespace vision3d {

VideoToolPaths VideoToolBackend::locate(QString* error)
{
    QStringList candidates;
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    candidates.append(QDir(applicationDirectory).filePath(QStringLiteral("tools/ffmpeg/bin")));

    QString ancestor = QDir(applicationDirectory).absolutePath();
    for (int depth = 0; depth < 8; ++depth) {
        const QDir current(ancestor);
        if (QFileInfo(current.filePath(QStringLiteral("app/CMakeLists.txt"))).isFile()) {
            candidates.append(current.filePath(QStringLiteral("tools/ffmpeg/bin")));
            break;
        }
        const QString parent = current.filePath(QStringLiteral(".."));
        if (QDir::cleanPath(parent) == QDir::cleanPath(ancestor)) {
            break;
        }
        ancestor = QDir(parent).absolutePath();
    }

    QStringList checked;
    for (const QString& candidate : candidates) {
        const QString clean = QDir::cleanPath(candidate);
        if (checked.contains(clean, Qt::CaseInsensitive)) {
            continue;
        }
        checked.append(clean);
        QString localError;
        const VideoToolPaths paths = locateInDirectory(clean, &localError);
        if (!paths.ffmpeg.isEmpty() && !paths.ffprobe.isEmpty()) {
            if (error != nullptr) {
                error->clear();
            }
            return paths;
        }
    }

    if (error != nullptr) {
        *error = QStringLiteral("未找到 FFmpeg。请将本地 Windows x64 版本放在应用目录 tools/ffmpeg/bin，或开发工程根目录 tools/ffmpeg/bin。未修改系统 PATH。检查位置：%1")
                     .arg(checked.join(QStringLiteral("；")));
    }
    return {};
}

VideoToolPaths VideoToolBackend::locateInDirectory(const QString& directory,
                                                   QString* error)
{
    VideoToolPaths result;
    result.directory = QDir(directory).absolutePath();
    const QString suffix =
#ifdef Q_OS_WIN
        QStringLiteral(".exe");
#else
        QString();
#endif
    const QString ffmpeg = QDir(result.directory).filePath(QStringLiteral("ffmpeg") + suffix);
    const QString ffprobe = QDir(result.directory).filePath(QStringLiteral("ffprobe") + suffix);
    if (!QFileInfo(ffmpeg).isFile() || !QFileInfo(ffprobe).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("FFmpeg 目录缺少 ffmpeg 或 ffprobe：%1").arg(result.directory);
        }
        return result;
    }
    result.ffmpeg = QFileInfo(ffmpeg).absoluteFilePath();
    result.ffprobe = QFileInfo(ffprobe).absoluteFilePath();
    if (error != nullptr) {
        error->clear();
    }
    return result;
}

} // namespace vision3d
