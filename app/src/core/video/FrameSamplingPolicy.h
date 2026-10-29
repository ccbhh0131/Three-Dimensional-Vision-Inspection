#pragma once

namespace vision3d {

class FrameSamplingPolicy final
{
public:
    static constexpr int MinimumTargetFrames = 100;
    static constexpr int MaximumAutomaticFrames = 450;
    static constexpr int MaximumManualFrames = 500;

    static int recommendedTargetFrames(double durationSeconds);
    static int clampManualTargetFrames(int targetFrames);
    static double sampleFps(int targetFrames, double durationSeconds);
};

} // namespace vision3d
