#pragma once

#include "core/assets/AssetRecord.h"
#include "core/device/GaugeAsset.h"
#include "core/inspection/InspectionRecord.h"
#include "core/viewer/SceneAlignmentTransform.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace vision3d {

class ProjectManifest
{
public:
    static constexpr int CurrentSchemaVersion = 1;

    ProjectManifest();

    static ProjectManifest createNew(const QString& name);
    static std::optional<ProjectManifest> fromJson(const QJsonObject& json, QString* error = nullptr);
    static std::optional<ProjectManifest> load(const QString& filePath, QString* error = nullptr);

    bool save(const QString& filePath, QString* error = nullptr) const;
    bool validate(QString* error = nullptr) const;
    QJsonObject toJson() const;

    int schemaVersion() const;
    const QString& projectId() const;
    const QString& name() const;
    const QString& createdAt() const;
    const QString& modifiedAt() const;
    const QJsonArray& assets() const;
    const QString& backend() const;
    const QString& backendVersion() const;
    const QString& reconstructionState() const;
    const QString& reconstructionActiveTaskId() const;
    const QJsonObject& latestReconstructionTask() const;
    const SceneAlignmentTransform& sceneAlignment() const;
    const QJsonArray& deviceMarkers() const;
    const QJsonArray& gaugeAssets() const;
    const QJsonArray& inspectionRecords() const;
    QList<AssetRecord> imageAssetRecords(QString* error = nullptr) const;

    void setName(const QString& name);
    void setModifiedAt(const QString& modifiedAt);
    void setAssets(const QJsonArray& assets);
    void setImageAssetRecords(const QList<AssetRecord>& assets);
    void setBackendVersion(const QString& backendVersion);
    void setReconstructionState(const QString& state);
    void setReconstructionMetadata(const QString& activeTaskId,
                                   const QJsonObject& latestTask);
    void setSceneAlignment(const SceneAlignmentTransform& alignment);
    void setDeviceMarkers(const QJsonArray& markers);
    void setGaugeAssets(const QJsonArray& assets);
    void setInspectionRecords(const QJsonArray& records);

private:
    int m_schemaVersion;
    QString m_projectId;
    QString m_name;
    QString m_createdAt;
    QString m_modifiedAt;
    QJsonArray m_assets;
    QString m_backend;
    QString m_backendVersion;
    QString m_reconstructionState;
    QString m_reconstructionActiveTaskId;
    QJsonObject m_latestReconstructionTask;
    SceneAlignmentTransform m_sceneAlignment;
    QJsonArray m_deviceMarkers;
    QJsonArray m_gaugeAssets;
    QJsonArray m_inspectionRecords;
};

} // namespace vision3d
