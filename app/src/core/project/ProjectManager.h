#pragma once

#include "core/project/ProjectManifest.h"
#include "core/reconstruction/ReconstructionTask.h"

#include <QList>
#include <QObject>
#include <QStringList>

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

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    bool newProject(const QString& parentDirectory, const QString& projectName, QString* error = nullptr);
    bool openProject(const QString& fileOrDirectory, QString* error = nullptr);
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

    bool hasProject() const;
    const QString& projectDirectory() const;
    const std::optional<ProjectManifest>& currentManifest() const;

signals:
    void projectChanged();

private:
    AssetImportResult importSingleImage(const QString& sourcePath);
    bool persistManifest(ProjectManifest manifest, QString* error = nullptr);
    bool markInterruptedTask(ProjectManifest* manifest, QString* error = nullptr) const;

    std::optional<ProjectManifest> m_manifest;
    QString m_projectDirectory;
};

} // namespace vision3d
