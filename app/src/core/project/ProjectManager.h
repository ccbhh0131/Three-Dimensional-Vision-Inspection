#pragma once

#include "core/device/DeviceMarker.h"
#include "core/project/ProjectManifest.h"
#include "core/reconstruction/ReconstructionTask.h"

#include <QList>
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

    bool hasProject() const;
    const QString& projectDirectory() const;
    const std::optional<ProjectManifest>& currentManifest() const;

signals:
    void projectChanged();

private:
    AssetImportResult importSingleImage(const QString& sourcePath);
    bool persistManifest(ProjectManifest manifest, QString* error = nullptr);
    bool persistMarkerModel(const DeviceMarkerModel& model, QString* error = nullptr);
    bool markInterruptedTask(ProjectManifest* manifest, QString* error = nullptr) const;

    std::optional<ProjectManifest> m_manifest;
    QString m_projectDirectory;
    DeviceMarkerModel m_deviceMarkerModel;
};

} // namespace vision3d
