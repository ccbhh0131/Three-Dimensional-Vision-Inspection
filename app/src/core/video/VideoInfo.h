#pragma once

#include <QString>
#include <QStringList>

namespace vision3d {

struct VideoInfo
{
    QString path;
    double durationSeconds = 0.0;
    double fps = 0.0;
    int width = 0;
    int height = 0;
    qint64 frameCount = 0;
    QString codec;
    QString toolVersion;
};

struct ReconstructionInput
{
    enum class Type
    {
        Images,
        Video,
    };

    Type type = Type::Images;
    QString sourcePath;
    QStringList imagePaths;
    QString workingDirectory;
    QString frameManifestPath;
    int targetFrames = 0;
    double durationSeconds = 0.0;
    int rawFrameCount = 0;
    int blurRejectedCount = 0;
    int duplicateRejectedCount = 0;
    int coverageRecoveredCount = 0;
};

} // namespace vision3d
