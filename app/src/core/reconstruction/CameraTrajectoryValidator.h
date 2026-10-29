#pragma once

#include <QString>
#include <QStringList>

namespace vision3d {

struct CameraTrajectoryResult
{
    bool readable = false;
    int registeredImages = 0;
    int translationJumpCount = 0;
    int orientationJumpCount = 0;
    int frameGapCount = 0;
    QStringList warnings;
};

class CameraTrajectoryValidator final
{
public:
    static CameraTrajectoryResult inspect(const QString& modelDirectory,
                                          double sampleFps,
                                          double maximumGapSeconds = 3.0);
};

} // namespace vision3d
