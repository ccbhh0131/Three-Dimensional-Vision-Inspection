#pragma once

#include "core/reconstruction/ReconstructionWorkspace.h"
#include "core/video/FrameQualityAnalyzer.h"
#include "core/video/VideoFrameExtractor.h"
#include "core/video/VideoProbe.h"

#include <QObject>
#include <QThread>

#include <atomic>

namespace vision3d {

class ProjectManager;

class VideoReconstructionController final : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Empty,
        Probing,
        Ready,
        Extracting,
        Analyzing,
        PreviewReady,
        Cancelled,
        Failed,
    };

    struct Snapshot
    {
        State state = State::Empty;
        VideoInfo info;
        bool hasInfo = false;
        int recommendedFrames = 0;
        int targetFrames = 0;
        int progress = 0;
        QString statusText;
        QString errorText;
        QString jobDirectory;
        FrameQualitySummary quality;

        bool busy() const;
        bool canPreprocess() const;
        bool canStartReconstruction() const;
    };

    explicit VideoReconstructionController(ProjectManager* projectManager,
                                           QObject* parent = nullptr);
    ~VideoReconstructionController() override;

    bool selectVideo(const QString& path, QString* error = nullptr);
    bool preprocess(int targetFrames = 0, QString* error = nullptr);
    void reset();
    void cancel();
    bool isBusy() const;
    const Snapshot& snapshot() const;
    ReconstructionInput reconstructionInput() const;

signals:
    void snapshotChanged();
    void logMessage(const QString& text, bool isError);

private:
    void setState(State state, const QString& statusText);
    void onProbeFinished(const VideoInfo& info);
    void onProbeFailed(const QString& userMessage, const QString& detail);
    void onProbeCancelled();
    void onExtractProgress(int frameCount, int targetFrames);
    void onExtractFinished(const QStringList& framePaths);
    void onExtractFailed(const QString& userMessage, const QString& detail);
    void onExtractCancelled();
    void startQualityAnalysis(const QStringList& framePaths);
    void finishQualityAnalysis(bool success,
                               const FrameQualitySummary& summary,
                               const QString& error);
    bool createJobWorkspace(QString* error);

    ProjectManager* m_projectManager = nullptr;
    VideoProbe m_probe;
    VideoFrameExtractor m_extractor;
    VideoToolPaths m_tools;
    VideoInfo m_videoInfo;
    ReconstructionJobPaths m_jobPaths;
    Snapshot m_snapshot;
    QThread* m_analysisThread = nullptr;
    std::atomic_bool m_cancelAnalysis{false};
};

} // namespace vision3d
