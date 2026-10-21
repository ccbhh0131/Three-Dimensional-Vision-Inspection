#pragma once

#include <QString>

namespace vision3d {

struct VideoToolPaths
{
    QString ffmpeg;
    QString ffprobe;
    QString directory;
};

class VideoToolBackend final
{
public:
    static VideoToolPaths locate(QString* error = nullptr);
    static VideoToolPaths locateInDirectory(const QString& directory,
                                            QString* error = nullptr);
};

} // namespace vision3d
