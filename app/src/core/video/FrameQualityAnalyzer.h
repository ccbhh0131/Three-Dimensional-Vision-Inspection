#pragma once

#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

namespace vision3d {

struct FrameQualitySummary
{
    int rawFrameCount = 0;
    int blurRejectedCount = 0;
    int duplicateRejectedCount = 0;
    int coverageRecoveredCount = 0;
    int acceptedFrameCount = 0;
    double blurP10 = 0.0;
    double blurP25 = 0.0;
    double blurMedian = 0.0;
    double blurP75 = 0.0;
    QStringList acceptedFramePaths;
    QString frameManifestPath;
    QString acceptedManifestPath;
    QString contactSheetPath;
};

class FrameQualityAnalyzer final
{
public:
    using ProgressCallback = std::function<void(int, int)>;

    static bool analyze(const QStringList& rawFramePaths,
                        const QString& sourceVideo,
                        double durationSeconds,
                        double sampleFps,
                        const QString& jobDirectory,
                        FrameQualitySummary* summary,
                        const ProgressCallback& progress = {},
                        const std::atomic_bool* cancel = nullptr,
                        QString* error = nullptr);
};

} // namespace vision3d
