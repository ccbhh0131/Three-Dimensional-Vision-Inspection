#include "core/project/ProjectManager.h"

#include "core/device/GaugeProfile.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {

bool sha256ForFile(const QString& path, QString* digest, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法读取文件计算 SHA-256: %1").arg(file.errorString());
        }
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && !file.atEnd()) {
            if (error != nullptr) {
                *error = QStringLiteral("读取文件计算 SHA-256 时失败: %1")
                             .arg(file.errorString());
            }
            return false;
        }
        hash.addData(chunk);
    }
    if (digest != nullptr) {
        *digest = QString::fromLatin1(hash.result().toHex());
    }
    return true;
}

bool copyFile(const QString& sourcePath, const QString& targetPath, QString* error)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法打开源图像: %1").arg(source.errorString());
        }
        return false;
    }

    QFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法创建项目内部图像: %1").arg(target.errorString());
        }
        return false;
    }

    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && !source.atEnd()) {
            if (error != nullptr) {
                *error = QStringLiteral("读取源图像时失败: %1").arg(source.errorString());
            }
            target.close();
            QFile::remove(targetPath);
            return false;
        }
        if (target.write(chunk) != chunk.size()) {
            if (error != nullptr) {
                *error = QStringLiteral("写入项目内部图像时失败: %1")
                             .arg(target.errorString());
            }
            target.close();
            QFile::remove(targetPath);
            return false;
        }
    }
    if (!target.flush()) {
        if (error != nullptr) {
            *error = QStringLiteral("刷新项目内部图像时失败: %1").arg(target.errorString());
        }
        target.close();
        QFile::remove(targetPath);
        return false;
    }
    target.close();
    return true;
}

QStringList thumbnailPaths(const QString& projectDirectory, const QString& assetId)
{
    const QDir cacheDirectory(QDir(projectDirectory).filePath(QStringLiteral("cache/thumbnails")));
    return {
        cacheDirectory.filePath(assetId + QStringLiteral(".jpg")),
        cacheDirectory.filePath(assetId + QStringLiteral(".png")),
    };
}

QString normalizedAbsolutePath(const QString& path)
{
    return QDir::fromNativeSeparators(
        QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
}

bool isWithinDirectory(const QString& directory, const QString& candidate)
{
    const QString normalizedDirectory = normalizedAbsolutePath(directory).toLower();
    const QString normalizedCandidate = normalizedAbsolutePath(candidate).toLower();
    return normalizedCandidate == normalizedDirectory
           || normalizedCandidate.startsWith(normalizedDirectory + QLatin1Char('/'));
}

} // namespace

namespace vision3d {

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
}

bool ProjectManager::newProject(const QString& parentDirectory,
                                const QString& projectName,
                                QString* error)
{
    const QString trimmedName = projectName.trimmed();
    if (trimmedName.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("项目名称不能为空。请填写项目名称后重试。");
        }
        return false;
    }
    if (trimmedName == QStringLiteral(".") || trimmedName == QStringLiteral("..")
        || trimmedName.contains(QLatin1Char('/'))
        || trimmedName.contains(QLatin1Char('\\'))) {
        if (error != nullptr) {
            *error = QStringLiteral("项目名称不能包含路径分隔符。请使用单一项目名称。");
        }
        return false;
    }

    QDir parent(parentDirectory);
    if (!parent.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("保存目录不存在: %1").arg(parentDirectory);
        }
        return false;
    }

    const QString workspacePath = parent.absoluteFilePath(trimmedName);
    const QString manifestPath = QDir(workspacePath).filePath(QStringLiteral("project.json"));
    if (QFileInfo::exists(manifestPath)) {
        if (error != nullptr) {
            *error = QStringLiteral("目标目录已有 project.json，未执行覆盖: %1").arg(workspacePath);
        }
        return false;
    }

    QDir workspace(workspacePath);
    if (!workspace.exists() && !QDir().mkpath(workspacePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法创建项目目录: %1").arg(workspacePath);
        }
        return false;
    }

    const QStringList requiredDirectories = {
        QStringLiteral("images"),
        QStringLiteral("cache"),
        QStringLiteral("cache/thumbnails"),
        QStringLiteral("reconstruction"),
        QStringLiteral("logs"),
    };
    for (const QString& directory : requiredDirectories) {
        if (!workspace.mkpath(directory)) {
            if (error != nullptr) {
                *error = QStringLiteral("无法创建项目子目录: %1").arg(directory);
            }
            return false;
        }
    }

    ProjectManifest manifest = ProjectManifest::createNew(trimmedName);
    if (!manifest.save(manifestPath, error)) {
        return false;
    }

    m_manifest = manifest;
    m_projectDirectory = workspace.absolutePath();
    m_deviceMarkerModel.clear();
    m_gaugeAssetModel.clear();
    m_inspectionRecordModel.clear();
    emit projectChanged();
    return true;
}

bool ProjectManager::openProject(const QString& fileOrDirectory, QString* error)
{
    QFileInfo input(fileOrDirectory);
    QString manifestPath;
    if (input.isDir()) {
        manifestPath = QDir(input.absoluteFilePath()).filePath(QStringLiteral("project.json"));
    } else if (input.isFile()) {
        manifestPath = input.absoluteFilePath();
    } else {
        if (error != nullptr) {
            *error = QStringLiteral("项目路径不存在: %1").arg(fileOrDirectory);
        }
        return false;
    }

    QString loadError;
    const std::optional<ProjectManifest> manifest = ProjectManifest::load(manifestPath, &loadError);
    if (!manifest.has_value()) {
        if (error != nullptr) {
            *error = loadError;
        }
        return false;
    }

    ProjectManifest openedManifest = *manifest;
    QString interruptedError;
    if (!markInterruptedTask(&openedManifest, &interruptedError) && !interruptedError.isEmpty()) {
        if (error != nullptr) {
            *error = interruptedError;
        }
        return false;
    }

    QString markerError;
    const std::optional<DeviceMarkerModel> markerModel =
        DeviceMarkerModel::fromJson(openedManifest.deviceMarkers(), &markerError);
    if (!markerModel.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("project.json deviceMarkers 无效: %1").arg(markerError);
        }
        return false;
    }

    QString gaugeError;
    const std::optional<GaugeAssetModel> gaugeModel =
        GaugeAssetModel::fromJson(openedManifest.gaugeAssets(), &gaugeError);
    if (!gaugeModel.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("project.json gaugeAssets 无效: %1").arg(gaugeError);
        }
        return false;
    }
    if (!validateGaugeBindings(*markerModel, *gaugeModel, &gaugeError)) {
        if (error != nullptr) {
            *error = gaugeError;
        }
        return false;
    }

    QString recordError;
    const std::optional<InspectionRecordModel> recordModel =
        InspectionRecordModel::fromJson(openedManifest.inspectionRecords(), &recordError);
    if (!recordModel.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("project.json inspectionRecords 无效: %1").arg(recordError);
        }
        return false;
    }
    QString imageError;
    const QList<AssetRecord> imageAssets = openedManifest.imageAssetRecords(&imageError);
    if (!imageError.isEmpty()) {
        if (error != nullptr) {
            *error = imageError;
        }
        return false;
    }
    if (!validateInspectionBindings(*markerModel,
                                    *gaugeModel,
                                    *recordModel,
                                    imageAssets,
                                    &recordError)) {
        if (error != nullptr) {
            *error = recordError;
        }
        return false;
    }

    m_manifest = openedManifest;
    m_projectDirectory = QFileInfo(manifestPath).absolutePath();
    m_deviceMarkerModel = *markerModel;
    m_gaugeAssetModel = *gaugeModel;
    m_inspectionRecordModel = *recordModel;
    if (openedManifest.latestReconstructionTask().isEmpty()) {
        // No task metadata is a valid Stage 3 state for an otherwise openable project.
    } else if (openedManifest.reconstructionState() == QStringLiteral("interrupted")) {
        QString saveError;
        if (!openedManifest.save(manifestPath, &saveError)) {
            if (error != nullptr) {
                *error = QStringLiteral("任务已标记 Interrupted，但 project.json 保存失败: %1")
                             .arg(saveError);
            }
            return false;
        }
    }
    emit projectChanged();
    return true;
}

void ProjectManager::closeProject()
{
    if (!m_manifest.has_value() && m_projectDirectory.isEmpty()) {
        return;
    }
    m_manifest.reset();
    m_projectDirectory.clear();
    m_deviceMarkerModel.clear();
    m_gaugeAssetModel.clear();
    m_inspectionRecordModel.clear();
    emit projectChanged();
}

bool ProjectManager::saveProject(QString* error)
{
    if (!m_manifest.has_value() || m_projectDirectory.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法保存。");
        }
        return false;
    }

    m_manifest->setDeviceMarkers(m_deviceMarkerModel.toJson());
    m_manifest->setGaugeAssets(m_gaugeAssetModel.toJson());
    m_manifest->setInspectionRecords(m_inspectionRecordModel.toJson());
    m_manifest->setModifiedAt(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return m_manifest->save(QDir(m_projectDirectory).filePath(QStringLiteral("project.json")), error);
}

QList<AssetImportResult> ProjectManager::importImages(const QStringList& sourceFiles,
                                                      QString* error)
{
    QList<AssetImportResult> results;
    if (error != nullptr) {
        error->clear();
    }
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法导入图像。");
        }
        return results;
    }

    for (const QString& sourcePath : sourceFiles) {
        results.append(importSingleImage(sourcePath));
    }
    return results;
}

AssetImportResult ProjectManager::importSingleImage(const QString& sourcePath)
{
    AssetImportResult result;
    result.sourcePath = sourcePath;

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.isFile()) {
        result.message = QStringLiteral("源文件不存在或不是普通文件: %1").arg(sourcePath);
        return result;
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        result.message = QStringLiteral("无法读取图像格式或文件已损坏: %1")
                             .arg(reader.errorString());
        return result;
    }

    QSize imageSize = reader.size();
    if (!imageSize.isValid() || imageSize.width() <= 0 || imageSize.height() <= 0) {
        const QImage decoded = reader.read();
        if (decoded.isNull()) {
            result.message = QStringLiteral("图像尺寸读取失败: %1").arg(reader.errorString());
            return result;
        }
        imageSize = decoded.size();
    }

    QString digest;
    QString fileError;
    if (!sha256ForFile(sourcePath, &digest, &fileError)) {
        result.message = fileError;
        return result;
    }

    QString recordsError;
    const QList<AssetRecord> existing = imageAssetRecords(&recordsError);
    if (!recordsError.isEmpty()) {
        result.message = recordsError;
        return result;
    }
    for (const AssetRecord& existingAsset : existing) {
        if (existingAsset.sha256.compare(digest, Qt::CaseInsensitive) == 0) {
            result.status = AssetImportStatus::Duplicate;
            result.asset = existingAsset;
            result.message = QStringLiteral("内容重复，未创建新资产: %1")
                                 .arg(existingAsset.originalFileName);
            return result;
        }
    }

    const QString assetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString extension = sourceInfo.suffix().toLower();
    if (extension.isEmpty()) {
        extension = QString::fromLatin1(reader.format()).toLower();
    }
    if (extension.isEmpty()) {
        extension = QStringLiteral("img");
    }

    const QDir projectDirectory(m_projectDirectory);
    const QDir imagesDirectory(projectDirectory.filePath(QStringLiteral("images")));
    if (!imagesDirectory.exists() && !QDir().mkpath(imagesDirectory.absolutePath())) {
        result.message = QStringLiteral("无法创建项目 images 目录: %1")
                             .arg(imagesDirectory.absolutePath());
        return result;
    }

    const QString relativePath = QStringLiteral("images/%1.%2").arg(assetId, extension);
    const QString targetPath = projectDirectory.filePath(relativePath);
    const QString temporaryPath = imagesDirectory.filePath(
        QStringLiteral(".%1.importing").arg(assetId));

    QFile::remove(temporaryPath);
    if (!copyFile(sourcePath, temporaryPath, &fileError)) {
        result.message = fileError;
        return result;
    }

    QString copiedDigest;
    if (!sha256ForFile(temporaryPath, &copiedDigest, &fileError)
        || copiedDigest.compare(digest, Qt::CaseInsensitive) != 0
        || QFileInfo(temporaryPath).size() != sourceInfo.size()) {
        QFile::remove(temporaryPath);
        result.message = QStringLiteral("内部复制校验失败: %1")
                             .arg(fileError.isEmpty() ? QStringLiteral("SHA-256 或文件大小不一致")
                                                      : fileError);
        return result;
    }

    if (!QFile::rename(temporaryPath, targetPath)) {
        QFile::remove(temporaryPath);
        result.message = QStringLiteral("无法提交项目内部图像: %1").arg(targetPath);
        return result;
    }

    AssetRecord asset;
    asset.id = assetId;
    asset.type = QStringLiteral("image");
    asset.relativePath = relativePath;
    asset.originalFileName = sourceInfo.fileName();
    asset.fileSize = sourceInfo.size();
    asset.width = imageSize.width();
    asset.height = imageSize.height();
    asset.sha256 = digest;

    ProjectManifest candidate = *m_manifest;
    QList<AssetRecord> updated = existing;
    updated.append(asset);
    candidate.setImageAssetRecords(updated);
    if (!persistManifest(candidate, &fileError)) {
        QFile::remove(targetPath);
        result.message = fileError;
        return result;
    }

    result.status = AssetImportStatus::Imported;
    result.asset = asset;
    result.message = QStringLiteral("已导入图像: %1").arg(asset.originalFileName);
    return result;
}

bool ProjectManager::removeAsset(const QString& assetId, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法删除资产。");
        }
        return false;
    }

    QString recordsError;
    const QList<AssetRecord> existing = imageAssetRecords(&recordsError);
    if (!recordsError.isEmpty()) {
        if (error != nullptr) {
            *error = recordsError;
        }
        return false;
    }

    std::optional<AssetRecord> selected;
    QList<AssetRecord> updated;
    for (const AssetRecord& asset : existing) {
        if (asset.id == assetId) {
            selected = asset;
        } else {
            updated.append(asset);
        }
    }
    if (!selected.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("未找到要删除的图像资产: %1").arg(assetId);
        }
        return false;
    }

    if (m_inspectionRecordModel.referencesImageAsset(assetId)) {
        if (error != nullptr) {
            *error = QStringLiteral("图像资产仍被巡检历史引用，不能删除: %1")
                         .arg(assetId.trimmed());
        }
        return false;
    }

    ProjectManifest candidate = *m_manifest;
    candidate.setImageAssetRecords(updated);
    QString persistError;
    if (!persistManifest(candidate, &persistError)) {
        if (error != nullptr) {
            *error = persistError;
        }
        return false;
    }

    const QString imagePath = absoluteAssetPath(*selected);
    if (!imagePath.isEmpty() && QFileInfo::exists(imagePath) && !QFile::remove(imagePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("manifest 已更新，但无法删除项目内部图像: %1")
                         .arg(imagePath);
        }
        return false;
    }

    QStringList cacheErrors;
    for (const QString& cachePath : thumbnailPaths(m_projectDirectory, selected->id)) {
        if (QFileInfo::exists(cachePath) && !QFile::remove(cachePath)) {
            cacheErrors.append(cachePath);
        }
    }
    if (!cacheErrors.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("资产已从 manifest 删除，但无法删除缩略图缓存: %1")
                         .arg(cacheErrors.join(QStringLiteral(", ")));
        }
        return false;
    }
    return true;
}

QList<AssetRecord> ProjectManager::imageAssetRecords(QString* error) const
{
    if (error != nullptr) {
        error->clear();
    }
    if (!m_manifest.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目。");
        }
        return {};
    }
    return m_manifest->imageAssetRecords(error);
}

std::optional<AssetRecord> ProjectManager::assetById(const QString& assetId) const
{
    const QList<AssetRecord> records = imageAssetRecords();
    for (const AssetRecord& record : records) {
        if (record.id == assetId) {
            return record;
        }
    }
    return std::nullopt;
}

QString ProjectManager::absoluteAssetPath(const AssetRecord& asset) const
{
    if (m_projectDirectory.isEmpty()) {
        return {};
    }
    QString validationError;
    if (!asset.isValid(&validationError)) {
        return {};
    }
    return QDir(m_projectDirectory).filePath(
        QDir::fromNativeSeparators(QDir::cleanPath(asset.relativePath)));
}

bool ProjectManager::updateReconstructionTask(const ReconstructionTask& task, QString* error)
{
    if (!hasProject() || m_projectDirectory.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法保存重建任务。 ").trimmed();
        }
        return false;
    }
    ProjectManifest candidate = *m_manifest;
    candidate.setBackendVersion(task.backendVersion);
    candidate.setReconstructionState(reconstructionStateToString(task.state));
    candidate.setReconstructionMetadata(task.taskId, task.toJson());
    return persistManifest(candidate, error);
}

std::optional<ReconstructionTask> ProjectManager::latestReconstructionTask(QString* error) const
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目。 ").trimmed();
        }
        return std::nullopt;
    }
    const QJsonObject latest = m_manifest->latestReconstructionTask();
    if (latest.isEmpty()) {
        return std::nullopt;
    }
    return ReconstructionTask::fromJson(latest, error);
}

bool ReconstructionMeshArtifact::isValid() const
{
    return status == ReconstructionMeshArtifactStatus::Valid
           && !path.isEmpty()
           && !canonicalPath.isEmpty();
}

ReconstructionMeshArtifact ProjectManager::latestPoissonMeshArtifact() const
{
    ReconstructionMeshArtifact artifact;
    if (!hasProject() || m_projectDirectory.isEmpty()) {
        artifact.message = QStringLiteral("No Active Project: 当前没有打开的项目。");
        return artifact;
    }

    QString taskError;
    const std::optional<ReconstructionTask> task = latestReconstructionTask(&taskError);
    if (!task.has_value()) {
        artifact.status = ReconstructionMeshArtifactStatus::NoReconstructionWorkspace;
        artifact.message = taskError.isEmpty()
            ? QStringLiteral("No Reconstruction Workspace: 当前项目没有已完成的 Reconstruction Workspace。")
            : QStringLiteral("No Reconstruction Workspace: %1").arg(taskError);
        return artifact;
    }
    artifact.taskId = task->taskId;
    if (task->state != ReconstructionState::Completed
        || task->workspaceRelativePath.trimmed().isEmpty()
        || task->meshRelativePath.trimmed().isEmpty()) {
        artifact.status = ReconstructionMeshArtifactStatus::NoReconstructionWorkspace;
        artifact.message = QStringLiteral(
            "No Reconstruction Workspace: 当前项目没有已完成且带 Poisson mesh 的重建任务。");
        return artifact;
    }

    const QString workspaceRelative =
        QDir::fromNativeSeparators(QDir::cleanPath(task->workspaceRelativePath));
    const QString meshRelative =
        QDir::fromNativeSeparators(QDir::cleanPath(task->meshRelativePath));
    if (QDir::isAbsolutePath(workspaceRelative)
        || QDir::isAbsolutePath(meshRelative)
        || workspaceRelative == QStringLiteral(".")
        || meshRelative == QStringLiteral(".")
        || workspaceRelative.startsWith(QStringLiteral("../"))
        || workspaceRelative.contains(QStringLiteral("/../"))
        || meshRelative.startsWith(QStringLiteral("../"))
        || meshRelative.contains(QStringLiteral("/../"))) {
        artifact.status = ReconstructionMeshArtifactStatus::Invalid;
        artifact.message = QStringLiteral(
            "Artifact Invalid: Poisson mesh 路径不属于当前 reconstruction workspace。");
        return artifact;
    }

    const QString projectDirectory = QFileInfo(m_projectDirectory).absoluteFilePath();
    const QString workspacePath = QDir(projectDirectory).filePath(workspaceRelative);
    const QString meshPath = QDir(workspacePath).filePath(meshRelative);
    artifact.relativePath = workspaceRelative + QLatin1Char('/') + meshRelative;
    artifact.path = QDir::cleanPath(meshPath);
    if (!isWithinDirectory(workspacePath, artifact.path)) {
        artifact.status = ReconstructionMeshArtifactStatus::Invalid;
        artifact.message = QStringLiteral(
            "Artifact Invalid: Poisson mesh 路径越出当前 reconstruction workspace。");
        return artifact;
    }

    const QFileInfo meshInfo(artifact.path);
    if (!meshInfo.exists()) {
        artifact.status = ReconstructionMeshArtifactStatus::Missing;
        artifact.message = QStringLiteral("Poisson Mesh Missing: %1").arg(artifact.path);
        return artifact;
    }
    if (!meshInfo.isFile() || meshInfo.size() <= 0) {
        artifact.status = ReconstructionMeshArtifactStatus::Invalid;
        artifact.message = QStringLiteral("Artifact Invalid: Poisson mesh 不是非空普通文件: %1")
                               .arg(artifact.path);
        return artifact;
    }

    const QString canonicalWorkspace = QFileInfo(workspacePath).canonicalFilePath();
    artifact.canonicalPath = meshInfo.canonicalFilePath();
    if (canonicalWorkspace.isEmpty() || artifact.canonicalPath.isEmpty()
        || !isWithinDirectory(canonicalWorkspace, artifact.canonicalPath)) {
        artifact.status = ReconstructionMeshArtifactStatus::Invalid;
        artifact.message = QStringLiteral(
            "Artifact Invalid: Poisson mesh canonical path 不属于当前 reconstruction workspace。");
        return artifact;
    }

    artifact.status = ReconstructionMeshArtifactStatus::Valid;
    artifact.message = QStringLiteral("Poisson mesh artifact valid: %1").arg(artifact.path);
    return artifact;
}

const DeviceMarkerModel& ProjectManager::deviceMarkerModel() const
{
    return m_deviceMarkerModel;
}

QList<DeviceMarker> ProjectManager::deviceMarkersForReconstruction(
    const QString& reconstructionTaskId) const
{
    return m_deviceMarkerModel.forReconstruction(reconstructionTaskId);
}

std::optional<DeviceMarker> ProjectManager::deviceMarkerById(const QString& id) const
{
    return m_deviceMarkerModel.find(id);
}

bool ProjectManager::addDeviceMarker(const QString& name,
                                     const QVector3D& worldPosition,
                                     const QString& reconstructionTaskId,
                                     DeviceMarker* createdMarker,
                                     QString* error)
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法添加设备标记。 ").trimmed();
        }
        return false;
    }

    DeviceMarker marker = DeviceMarker::create(name, worldPosition, reconstructionTaskId);
    DeviceMarkerModel candidate = m_deviceMarkerModel;
    if (!candidate.add(marker, error)) {
        return false;
    }
    if (!persistMarkerModel(candidate, error)) {
        return false;
    }
    if (createdMarker != nullptr) {
        *createdMarker = marker;
    }
    return true;
}

bool ProjectManager::removeDeviceMarker(const QString& id, QString* error)
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法删除设备标记。 ").trimmed();
        }
        return false;
    }

    DeviceMarkerModel candidate = m_deviceMarkerModel;
    if (!candidate.remove(id, error)) {
        return false;
    }
    GaugeAssetModel candidateGauges = m_gaugeAssetModel;
    InspectionRecordModel candidateRecords = m_inspectionRecordModel;
    const std::optional<GaugeAsset> boundGauge = candidateGauges.findByMarkerId(id);
    if (boundGauge.has_value()) {
        QString gaugeError;
        if (!candidateGauges.remove(boundGauge->id, &gaugeError)) {
            if (error != nullptr) {
                *error = gaugeError;
            }
            return false;
        }
        candidateRecords.removeForGauge(boundGauge->id);
    }
    return persistModels(candidate, candidateGauges, candidateRecords, error);
}

const GaugeAssetModel& ProjectManager::gaugeAssetModel() const
{
    return m_gaugeAssetModel;
}

QList<GaugeAsset> ProjectManager::gaugeAssets() const
{
    return m_gaugeAssetModel.list();
}

std::optional<GaugeAsset> ProjectManager::gaugeAssetById(const QString& id) const
{
    return m_gaugeAssetModel.findById(id);
}

std::optional<GaugeAsset> ProjectManager::gaugeAssetForMarker(const QString& deviceMarkerId) const
{
    return m_gaugeAssetModel.findByMarkerId(deviceMarkerId);
}

std::optional<GaugeAsset> ProjectManager::gaugeAssetByMarkerId(
    const QString& deviceMarkerId) const
{
    return gaugeAssetForMarker(deviceMarkerId);
}

bool ProjectManager::addGaugeAsset(const GaugeAsset& asset, QString* error)
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法添加仪表资产。");
        }
        return false;
    }
    GaugeAsset normalized = asset;
    normalized.id = normalized.id.trimmed();
    normalized.deviceMarkerId = normalized.deviceMarkerId.trimmed();
    normalized.name = normalized.name.trimmed();
    normalized.unit = normalized.unit.trimmed();
    normalized.gaugeProfileId = normalized.gaugeProfileId.trimmed();
    const QString markerId = normalized.deviceMarkerId;
    if (!m_deviceMarkerModel.contains(markerId)) {
        if (error != nullptr) {
            *error = QStringLiteral("未找到要绑定的 DeviceMarker: %1").arg(markerId);
        }
        return false;
    }

    GaugeAssetModel candidate = m_gaugeAssetModel;
    if (!candidate.add(normalized, error)) {
        return false;
    }
    return persistModels(m_deviceMarkerModel, candidate, error);
}

bool ProjectManager::addGaugeAsset(const QString& deviceMarkerId,
                                   const QString& name,
                                   double rangeMin,
                                   double rangeMax,
                                   const QString& unit,
                                   GaugeAsset* createdAsset,
                                   QString* error)
{
    const GaugeAsset asset =
        GaugeAsset::create(deviceMarkerId, name, rangeMin, rangeMax, unit);
    if (!addGaugeAsset(asset, error)) {
        return false;
    }
    if (createdAsset != nullptr) {
        *createdAsset = asset;
    }
    return true;
}

bool ProjectManager::addGaugeAsset(const QString& deviceMarkerId,
                                   const QString& name,
                                   double rangeMin,
                                   double rangeMax,
                                   const QString& unit,
                                   QString* error)
{
    return addGaugeAsset(deviceMarkerId,
                         name,
                         rangeMin,
                         rangeMax,
                         unit,
                         nullptr,
                         error);
}

bool ProjectManager::updateGaugeAsset(const GaugeAsset& asset, QString* error)
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法更新仪表资产。");
        }
        return false;
    }
    const std::optional<GaugeAsset> existing = m_gaugeAssetModel.findById(asset.id);
    if (!existing.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("未找到 GaugeAsset: %1").arg(asset.id.trimmed());
        }
        return false;
    }
    if (existing->deviceMarkerId != asset.deviceMarkerId.trimmed()) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeAsset 的 deviceMarkerId 不能通过普通编辑修改。");
        }
        return false;
    }

    GaugeAsset normalized = asset;
    normalized.id = existing->id;
    normalized.deviceMarkerId = existing->deviceMarkerId;
    normalized.name = normalized.name.trimmed();
    normalized.unit = normalized.unit.trimmed();
    normalized.gaugeProfileId = normalized.gaugeProfileId.trimmed();
    if (!m_deviceMarkerModel.contains(normalized.deviceMarkerId)) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeAsset 绑定的 DeviceMarker 不存在: %1")
                         .arg(normalized.deviceMarkerId);
        }
        return false;
    }
    GaugeAssetModel candidate = m_gaugeAssetModel;
    if (!candidate.update(normalized, error)) {
        return false;
    }
    return persistModels(m_deviceMarkerModel, candidate, error);
}

bool ProjectManager::removeGaugeAsset(const QString& id, QString* error)
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法删除仪表资产。");
        }
        return false;
    }
    GaugeAssetModel candidate = m_gaugeAssetModel;
    if (!candidate.remove(id, error)) {
        return false;
    }
    InspectionRecordModel candidateRecords = m_inspectionRecordModel;
    candidateRecords.removeForGauge(id);
    return persistModels(m_deviceMarkerModel, candidate, candidateRecords, error);
}

const InspectionRecordModel& ProjectManager::inspectionRecordModel() const
{
    return m_inspectionRecordModel;
}

QList<InspectionRecord> ProjectManager::inspectionRecords() const
{
    return m_inspectionRecordModel.all();
}

QList<InspectionRecord> ProjectManager::inspectionRecordsForGauge(
    const QString& gaugeAssetId) const
{
    return m_inspectionRecordModel.recordsForGauge(gaugeAssetId);
}

std::optional<InspectionRecord> ProjectManager::inspectionRecordById(const QString& id) const
{
    return m_inspectionRecordModel.findById(id);
}

bool ProjectManager::recordGaugeReading(const QString& assetId,
                                        double value,
                                        const QDateTime& timestamp,
                                        GaugeDataSource source,
                                        QString* error)
{
    return recordGaugeReading(assetId, value, timestamp, source, QString(), error);
}

bool ProjectManager::recordGaugeReading(const QString& assetId,
                                        double value,
                                        const QDateTime& timestamp,
                                        GaugeDataSource source,
                                        const QString& imageAssetId,
                                        QString* error)
{
    if (!hasProject()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法更新仪表读数。");
        }
        return false;
    }
    if (!std::isfinite(value)) {
        if (error != nullptr) {
            *error = QStringLiteral("仪表读数必须是有限数字。");
        }
        return false;
    }
    if (!timestamp.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("仪表读数时间戳无效。");
        }
        return false;
    }
    if (source == GaugeDataSource::None) {
        if (error != nullptr) {
            *error = QStringLiteral("更新仪表读数时 dataSource 不能为 None。");
        }
        return false;
    }

    const std::optional<GaugeAsset> existing = m_gaugeAssetModel.findById(assetId);
    if (!existing.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("未找到 GaugeAsset: %1").arg(assetId.trimmed());
        }
        return false;
    }

    QString normalizedImageAssetId = imageAssetId.trimmed();
    QString sourceImagePath;
    if (!normalizedImageAssetId.isEmpty()) {
        const std::optional<AssetRecord> imageAsset = assetById(normalizedImageAssetId);
        if (!imageAsset.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("巡检图片 ImageAsset 不存在: %1")
                             .arg(normalizedImageAssetId);
            }
            return false;
        }
        sourceImagePath = imageAsset->relativePath;
    }

    const InspectionRecord record = InspectionRecord::create(existing->id,
                                                             value,
                                                             timestamp.toUTC(),
                                                             source,
                                                             normalizedImageAssetId,
                                                             sourceImagePath);
    InspectionRecordModel candidateRecords = m_inspectionRecordModel;
    if (!candidateRecords.add(record, error)) {
        return false;
    }

    GaugeAssetModel candidateGauges = m_gaugeAssetModel;
    GaugeAsset updated = *existing;
    const QList<InspectionRecord> history = candidateRecords.recordsForGauge(existing->id);
    if (history.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("巡检记录写入后未找到对应 GaugeAsset 历史。 ").trimmed();
        }
        return false;
    }
    const InspectionRecord& latest = history.first();
    if (!updated.latestTimestamp.has_value()
        || latest.timestamp >= updated.latestTimestamp.value()) {
        updated.latestValue = latest.value;
        updated.latestTimestamp = latest.timestamp.toUTC();
        updated.dataSource = latest.dataSource;
    }
    if (!candidateGauges.update(updated, error)) {
        return false;
    }
    return persistModels(m_deviceMarkerModel, candidateGauges, candidateRecords, error);
}

bool ProjectManager::updateGaugeReading(const QString& assetId,
                                        double value,
                                        const QDateTime& timestamp,
                                        GaugeDataSource source,
                                        QString* error)
{
    return recordGaugeReading(assetId, value, timestamp, source, QString(), error);
}

bool ProjectManager::markInterruptedTask(ProjectManifest* manifest, QString* error) const
{
    if (manifest == nullptr || manifest->latestReconstructionTask().isEmpty()) {
        return true;
    }
    QString parseError;
    const std::optional<ReconstructionTask> latest =
        ReconstructionTask::fromJson(manifest->latestReconstructionTask(), &parseError);
    if (!latest.has_value()) {
        // A damaged task record must not make the project itself unopenable.
        return true;
    }
    if (latest->state != ReconstructionState::Running
        && latest->state != ReconstructionState::Preparing) {
        return true;
    }
    ReconstructionTask interrupted = *latest;
    interrupted.state = ReconstructionState::Interrupted;
    interrupted.finishedAt = QDateTime::currentDateTimeUtc();
    interrupted.errorMessage = QStringLiteral("应用在重建运行期间退出，任务标记为 Interrupted。 ").trimmed();
    manifest->setReconstructionState(reconstructionStateToString(interrupted.state));
    manifest->setBackendVersion(interrupted.backendVersion);
    manifest->setReconstructionMetadata(interrupted.taskId, interrupted.toJson());
    Q_UNUSED(error)
    return true;
}

bool ProjectManager::persistManifest(ProjectManifest manifest, QString* error)
{
    if (m_projectDirectory.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前项目目录为空，无法保存 manifest。");
        }
        return false;
    }
    manifest.setDeviceMarkers(m_deviceMarkerModel.toJson());
    manifest.setGaugeAssets(m_gaugeAssetModel.toJson());
    manifest.setInspectionRecords(m_inspectionRecordModel.toJson());
    manifest.setModifiedAt(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const QString manifestPath = QDir(m_projectDirectory).filePath(QStringLiteral("project.json"));
    if (!manifest.save(manifestPath, error)) {
        return false;
    }
    m_manifest = manifest;
    emit projectChanged();
    return true;
}

bool ProjectManager::persistMarkerModel(const DeviceMarkerModel& model, QString* error)
{
    return persistModels(model, m_gaugeAssetModel, error);
}

bool ProjectManager::persistModels(const DeviceMarkerModel& markerModel,
                                   const GaugeAssetModel& gaugeModel,
                                   QString* error)
{
    return persistModels(markerModel, gaugeModel, m_inspectionRecordModel, error);
}

bool ProjectManager::persistModels(const DeviceMarkerModel& markerModel,
                                   const GaugeAssetModel& gaugeModel,
                                   const InspectionRecordModel& recordModel,
                                   QString* error)
{
    if (!m_manifest.has_value() || m_projectDirectory.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前没有打开的项目，无法保存项目设备数据。");
        }
        return false;
    }
    if (!validateGaugeBindings(markerModel, gaugeModel, error)) {
        return false;
    }
    QString imageError;
    const QList<AssetRecord> imageAssets = m_manifest->imageAssetRecords(&imageError);
    if (!imageError.isEmpty()) {
        if (error != nullptr) {
            *error = imageError;
        }
        return false;
    }
    if (!validateInspectionBindings(markerModel,
                                    gaugeModel,
                                    recordModel,
                                    imageAssets,
                                    error)) {
        return false;
    }

    ProjectManifest candidate = *m_manifest;
    candidate.setDeviceMarkers(markerModel.toJson());
    candidate.setGaugeAssets(gaugeModel.toJson());
    candidate.setInspectionRecords(recordModel.toJson());
    candidate.setModifiedAt(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const QString manifestPath = QDir(m_projectDirectory).filePath(QStringLiteral("project.json"));
    if (!candidate.save(manifestPath, error)) {
        return false;
    }
    m_manifest = candidate;
    m_deviceMarkerModel = markerModel;
    m_gaugeAssetModel = gaugeModel;
    m_inspectionRecordModel = recordModel;
    emit projectChanged();
    return true;
}

bool ProjectManager::validateGaugeBindings(const DeviceMarkerModel& markerModel,
                                           const GaugeAssetModel& gaugeModel,
                                           QString* error) const
{
    for (const GaugeAsset& asset : gaugeModel.list()) {
        if (!markerModel.contains(asset.deviceMarkerId)) {
            if (error != nullptr) {
                *error = QStringLiteral("GaugeAsset %1 绑定了不存在的 DeviceMarker: %2")
                             .arg(asset.id, asset.deviceMarkerId);
            }
            return false;
        }
        if (asset.gaugeProfileId.trimmed().isEmpty()) {
            continue;
        }
        const std::optional<GaugeProfile> profile =
            builtInGaugeProfile(asset.gaugeProfileId);
        if (!profile.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("GaugeAsset %1 引用了未知 GaugeProfile: %2")
                             .arg(asset.id, asset.gaugeProfileId);
            }
            return false;
        }
        if (asset.unit.trimmed().compare(profile->unit.trimmed(), Qt::CaseInsensitive) != 0
            || std::abs(asset.rangeMin - profile->rangeMin) > 1.0e-9
            || std::abs(asset.rangeMax - profile->rangeMax) > 1.0e-9) {
            if (error != nullptr) {
                *error = QStringLiteral(
                             "GaugeAsset %1 与 GaugeProfile %2 的 unit/range 不一致: asset=%3 %4~%5, profile=%6 %7~%8")
                             .arg(asset.id,
                                  profile->id,
                                  asset.unit,
                                  QString::number(asset.rangeMin, 'g', 15),
                                  QString::number(asset.rangeMax, 'g', 15),
                                  profile->unit,
                                  QString::number(profile->rangeMin, 'g', 15),
                                  QString::number(profile->rangeMax, 'g', 15));
            }
            return false;
        }
    }
    return true;
}

bool ProjectManager::validateInspectionBindings(
    const DeviceMarkerModel& markerModel,
    const GaugeAssetModel& gaugeModel,
    const InspectionRecordModel& recordModel,
    const QList<AssetRecord>& imageAssets,
    QString* error) const
{
    Q_UNUSED(markerModel)
    for (const InspectionRecord& record : recordModel.all()) {
        if (!gaugeModel.contains(record.gaugeAssetId)) {
            if (error != nullptr) {
                *error = QStringLiteral("InspectionRecord %1 引用了不存在的 GaugeAsset: %2")
                             .arg(record.id, record.gaugeAssetId);
            }
            return false;
        }
        if (record.imageAssetId.trimmed().isEmpty()) {
            continue;
        }
        const auto imageIt = std::find_if(
            imageAssets.cbegin(), imageAssets.cend(), [&record](const AssetRecord& asset) {
                return asset.id == record.imageAssetId;
            });
        if (imageIt == imageAssets.cend()) {
            if (error != nullptr) {
                *error = QStringLiteral("InspectionRecord %1 引用了不存在的 ImageAsset: %2")
                             .arg(record.id, record.imageAssetId);
            }
            return false;
        }
        if (!record.sourceImagePath.trimmed().isEmpty()
            && QDir::fromNativeSeparators(record.sourceImagePath)
                   != QDir::fromNativeSeparators(imageIt->relativePath)) {
            if (error != nullptr) {
                *error = QStringLiteral("InspectionRecord %1 的 sourceImagePath 与 ImageAsset 不一致。")
                             .arg(record.id);
            }
            return false;
        }
    }
    return true;
}

bool ProjectManager::hasProject() const { return m_manifest.has_value(); }

const QString& ProjectManager::projectDirectory() const { return m_projectDirectory; }

const std::optional<ProjectManifest>& ProjectManager::currentManifest() const { return m_manifest; }

} // namespace vision3d
