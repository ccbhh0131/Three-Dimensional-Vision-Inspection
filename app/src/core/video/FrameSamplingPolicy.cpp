#include "core/video/FrameSamplingPolicy.h"

#include <algorithm>

namespace vision3d {

int FrameSamplingPolicy::recommendedTargetFrames(double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        return MinimumTargetFrames;
    }
    if (durationSeconds <= 20.0) {
        return 120;
    }
    if (durationSeconds <= 40.0) {
        return 180;
    }
    if (durationSeconds <= 60.0) {
        return 240;
    }
    if (durationSeconds <= 90.0) {
        return 330;
    }
    return MaximumAutomaticFrames;
}

int FrameSamplingPolicy::clampManualTargetFrames(int targetFrames)
{
    return std::clamp(targetFrames, MinimumTargetFrames, MaximumManualFrames);
}

double FrameSamplingPolicy::sampleFps(int targetFrames, double durationSeconds)
{
    return durationSeconds > 0.0 ? static_cast<double>(targetFrames) / durationSeconds : 0.0;
}

} // namespace vision3d
