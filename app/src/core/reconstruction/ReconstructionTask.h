#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include <optional>

namespace vision3d {

enum class ReconstructionState
{
    Idle,
    Preparing,
    Running,
    Completed,
    Failed,
    Cancelled,
    Interrupted,
};

enum class ReconstructionStage
{
    None,
    PreparingInput,
    FeatureExtraction,
    FeatureMatching,
    SparseMapping,
    ImageUndistortion,
    DenseStereo,
    StereoFusion,
    Meshing,
    Completed,
};

QString reconstructionStateToString(ReconstructionState state);
std::optional<ReconstructionState> reconstructionStateFromString(const QString& value);
QString reconstructionStageToString(ReconstructionStage stage);
std::optional<ReconstructionStage> reconstructionStageFromString(const QString& value);

struct ReconstructionTask
{
    QString taskId;
    ReconstructionState state = ReconstructionState::Idle;
    ReconstructionStage stage = ReconstructionStage::None;

    QString backendName;
    QString backendVersion;
    QString workspaceRelativePath;

    QDateTime createdAt;
    QDateTime startedAt;
    QDateTime finishedAt;

    QString errorMessage;
    QString failedStage;
    int exitCode = -1;

    int inputImageCount = 0;
    int registeredImageCount = 0;
    double registrationRatio = 0.0;
    QString primaryModelRelativePath;
    QString fusedPointCloudRelativePath;
    QString meshRelativePath;
    qint64 totalElapsedMilliseconds = -1;

    QJsonObject toJson() const;
    static std::optional<ReconstructionTask> fromJson(const QJsonObject& json,
                                                      QString* error = nullptr);
    bool save(const QString& filePath, QString* error = nullptr) const;
    static std::optional<ReconstructionTask> load(const QString& filePath,
                                                  QString* error = nullptr);
};

} // namespace vision3d

Q_DECLARE_METATYPE(vision3d::ReconstructionTask)
