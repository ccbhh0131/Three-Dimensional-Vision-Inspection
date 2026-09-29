#pragma once

#include "core/device/DeviceMarker.h"
#include "core/device/GaugeAsset.h"
#include "core/inspection/InspectionRecord.h"
#include "core/project/ProjectManifest.h"
#include "core/reconstruction/ReconstructionTask.h"

#include <QList>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVector3D>

#include <optional>

namespace vision3d {

enum class AssetImportStatus
{
    Imported,
    Duplicate,
    Failed,
};

struct AssetImportResult
{
    AssetImportStatus status = AssetImportStatus::Failed;
    QString sourcePath;
    AssetRecord asset;
    QString message;
};

enum class ReconstructionMeshArtifactStatus
{
    NoActiveProject,
    NoReconstructionWorkspace,
    Missing,
    Invalid,
    Valid,
};

struct ReconstructionMeshArtifact
{
    ReconstructionMeshArtifactStatus status =
        ReconstructionMeshArtifactStatus::NoActiveProject;
    QString taskId;
    QString relativePath;
    QString path;
    QString canonicalPath;
    QString message;

    bool isValid() const;
};

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    bool newProject(const QString& parentDirectory, const QString& projectName, QString* error = nullptr);
    bool openProject(const QString& fileOrDirectory, QString* error = nullptr);
    void closeProject();
    bool saveProject(QString* error = nullptr);

    QList<AssetImportResult> importImages(const QStringList& sourceFiles,
                                          QString* error = nullptr);
    bool removeAsset(const QString& assetId, QString* error = nullptr);
    QList<AssetRecord> imageAssetRecords(QString* error = nullptr) const;
    std::optional<AssetRecord> assetById(const QString& assetId) const;
    QString absoluteAssetPath(const AssetRecord& asset) const;

    bool updateReconstructionTask(const ReconstructionTask& task,
                                  QString* error = nullptr);
    bool setSceneAlignment(const SceneAlignmentTransform& alignment,
                           QString* error = nullptr);
    std::optional<ReconstructionTask> latestReconstructionTask(QString* error = nullptr) const;
    ReconstructionMeshArtifact latestPoissonMeshArtifact() const;

    const DeviceMarkerModel& deviceMarkerModel() const;
    QList<DeviceMarker> deviceMarkersForReconstruction(
        const QString& reconstructionTaskId) const;
    std::optional<DeviceMarker> deviceMarkerById(const QString& id) const;
    bool addDeviceMarker(const QString& name,
                         const QVector3D& worldPosition,
                         const QString& reconstructionTaskId,
                         DeviceMarker* createdMarker = nullptr,
                         QString* error = nullptr);
    bool removeDeviceMarker(const QString& id, QString* error = nullptr);

    const GaugeAssetModel& gaugeAssetModel() const;
    QList<GaugeAsset> gaugeAssets() const;
    std::optional<GaugeAsset> gaugeAssetById(const QString& id) const;
    std::optional<GaugeAsset> gaugeAssetForMarker(const QString& deviceMarkerId) const;
    std::optional<GaugeAsset> gaugeAssetByMarkerId(const QString& deviceMarkerId) const;
    bool addGaugeAsset(const GaugeAsset& asset, QString* error = nullptr);
    bool addGaugeAsset(const QString& deviceMarkerId,
                       const QString& name,
                       double rangeMin,
                       double rangeMax,
                       const QString& unit,
                       GaugeAsset* createdAsset = nullptr,
                       QString* error = nullptr);
    bool addGaugeAsset(const QString& deviceMarkerId,
                       const QString& name,
                       double rangeMin,
                       double rangeMax,
                       const QString& unit,
                       QString* error);
    bool updateGaugeAsset(const GaugeAsset& asset, QString* error = nullptr);
    bool removeGaugeAsset(const QString& id, QString* error = nullptr);
    const InspectionRecordModel& inspectionRecordModel() const;
    QList<InspectionRecord> inspectionRecords() const;
    QList<InspectionRecord> inspectionRecordsForGauge(const QString& gaugeAssetId) const;
    std::optional<InspectionRecord> inspectionRecordById(const QString& id) const;
    bool recordGaugeReading(const QString& assetId,
                            double value,
                            const QDateTime& timestamp,
                            GaugeDataSource source,
                            QString* error = nullptr);
    bool recordGaugeReading(const QString& assetId,
                            double value,
                            const QDateTime& timestamp,
                            GaugeDataSource source,
                            const QString& imageAssetId,
                            QString* error = nullptr);
    bool updateGaugeReading(const QString& assetId,
                            double value,
                            const QDateTime& timestamp,
                            GaugeDataSource source,
                            QString* error = nullptr);

    bool hasProject() const;
    const QString& projectDirectory() const;
    const std::optional<ProjectManifest>& currentManifest() const;

signals:
    void projectChanged();

private:
    AssetImportResult importSingleImage(const QString& sourcePath);
    bool persistManifest(ProjectManifest manifest, QString* error = nullptr);
    bool persistMarkerModel(const DeviceMarkerModel& model, QString* error = nullptr);
    bool persistModels(const DeviceMarkerModel& markerModel,
                       const GaugeAssetModel& gaugeModel,
                       QString* error = nullptr);
    bool persistModels(const DeviceMarkerModel& markerModel,
                       const GaugeAssetModel& gaugeModel,
                       const InspectionRecordModel& recordModel,
                       QString* error = nullptr);
    bool validateGaugeBindings(const DeviceMarkerModel& markerModel,
                               const GaugeAssetModel& gaugeModel,
                               QString* error = nullptr) const;
    bool validateInspectionBindings(const DeviceMarkerModel& markerModel,
                                    const GaugeAssetModel& gaugeModel,
                                    const InspectionRecordModel& recordModel,
                                    const QList<AssetRecord>& imageAssets,
                                    QString* error = nullptr) const;
    bool markInterruptedTask(ProjectManifest* manifest, QString* error = nullptr) const;

    std::optional<ProjectManifest> m_manifest;
    QString m_projectDirectory;
    DeviceMarkerModel m_deviceMarkerModel;
    GaugeAssetModel m_gaugeAssetModel;
    InspectionRecordModel m_inspectionRecordModel;
};

} // namespace vision3d
