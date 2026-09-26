#include "core/project/ProjectManifest.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QUuid>

namespace vision3d {

ProjectManifest::ProjectManifest()
    : m_schemaVersion(CurrentSchemaVersion)
    , m_backend(QStringLiteral("colmap"))
    , m_reconstructionState(QStringLiteral("idle"))
{
}

ProjectManifest ProjectManifest::createNew(const QString& name)
{
    ProjectManifest manifest;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    manifest.m_projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    manifest.m_name = name.trimmed();
    manifest.m_createdAt = now;
    manifest.m_modifiedAt = now;
    return manifest;
}

std::optional<ProjectManifest> ProjectManifest::fromJson(const QJsonObject& json, QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<ProjectManifest> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    if (!json.contains(QStringLiteral("schemaVersion"))
        || !json.value(QStringLiteral("schemaVersion")).isDouble()) {
        return fail(QStringLiteral("project.json 缺少有效的 schemaVersion。"));
    }

    const double schemaNumber = json.value(QStringLiteral("schemaVersion")).toDouble();
    if (schemaNumber != static_cast<int>(schemaNumber)) {
        return fail(QStringLiteral("schemaVersion 必须是整数。"));
    }

    const int schemaVersion = static_cast<int>(schemaNumber);
    if (schemaVersion != CurrentSchemaVersion) {
        return fail(QStringLiteral("不支持的 schemaVersion: %1。当前支持版本为 %2。")
                        .arg(schemaVersion)
                        .arg(CurrentSchemaVersion));
    }

    if (!json.value(QStringLiteral("projectId")).isString()
        || json.value(QStringLiteral("projectId")).toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("project.json 缺少 projectId。"));
    }
    if (!json.value(QStringLiteral("name")).isString()
        || json.value(QStringLiteral("name")).toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("project.json 缺少 name。"));
    }

    if (json.contains(QStringLiteral("assets"))
        && !json.value(QStringLiteral("assets")).isArray()) {
        return fail(QStringLiteral("assets 必须是数组。"));
    }
    if (json.contains(QStringLiteral("reconstruction"))
        && !json.value(QStringLiteral("reconstruction")).isObject()) {
        return fail(QStringLiteral("reconstruction 必须是对象。"));
    }
    if (json.contains(QStringLiteral("deviceMarkers"))
        && !json.value(QStringLiteral("deviceMarkers")).isArray()) {
        return fail(QStringLiteral("deviceMarkers 必须是数组。"));
    }

    ProjectManifest manifest;
    manifest.m_schemaVersion = schemaVersion;
    manifest.m_projectId = json.value(QStringLiteral("projectId")).toString().trimmed();
    manifest.m_name = json.value(QStringLiteral("name")).toString().trimmed();
    manifest.m_createdAt = json.value(QStringLiteral("createdAt")).toString();
    manifest.m_modifiedAt = json.value(QStringLiteral("modifiedAt")).toString();
    manifest.m_assets = json.value(QStringLiteral("assets")).toArray();

    const QJsonObject reconstruction = json.value(QStringLiteral("reconstruction")).toObject();
    manifest.m_backend = reconstruction.value(QStringLiteral("backend"))
                             .toString(QStringLiteral("colmap"));
    manifest.m_backendVersion = reconstruction.value(QStringLiteral("backendVersion")).toString();
    manifest.m_reconstructionState = reconstruction.value(QStringLiteral("state"))
                                         .toString(QStringLiteral("idle"));
    if (reconstruction.contains(QStringLiteral("activeTaskId"))
        && !reconstruction.value(QStringLiteral("activeTaskId")).isString()) {
        return fail(QStringLiteral("reconstruction.activeTaskId 必须是字符串。"));
    }
    if (reconstruction.contains(QStringLiteral("latestTask"))
        && !reconstruction.value(QStringLiteral("latestTask")).isObject()) {
        return fail(QStringLiteral("reconstruction.latestTask 必须是对象。"));
    }
    manifest.m_reconstructionActiveTaskId = reconstruction.value(QStringLiteral("activeTaskId"))
                                                .toString();
    manifest.m_latestReconstructionTask = reconstruction.value(QStringLiteral("latestTask"))
                                              .toObject();
    manifest.m_deviceMarkers = json.value(QStringLiteral("deviceMarkers")).toArray();

    QString validationError;
    if (!manifest.validate(&validationError)) {
        return fail(validationError);
    }
    return manifest;
}

std::optional<ProjectManifest> ProjectManifest::load(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法读取 project.json: %1").arg(file.errorString());
        }
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("project.json JSON 解析失败: %1")
                         .arg(parseError.errorString());
        }
        return std::nullopt;
    }

    return fromJson(document.object(), error);
}

bool ProjectManifest::save(const QString& filePath, QString* error) const
{
    QString validationError;
    if (!validate(&validationError)) {
        if (error != nullptr) {
            *error = validationError;
        }
        return false;
    }

    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法写入 project.json: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray payload = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        if (error != nullptr) {
            *error = QStringLiteral("写入 project.json 内容不完整: %1").arg(file.errorString());
        }
        return false;
    }
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("提交 project.json 原子保存失败: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool ProjectManifest::validate(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (m_schemaVersion != CurrentSchemaVersion) {
        return fail(QStringLiteral("不支持的 schemaVersion: %1。当前支持版本为 %2。")
                        .arg(m_schemaVersion)
                        .arg(CurrentSchemaVersion));
    }
    if (m_projectId.trimmed().isEmpty()) {
        return fail(QStringLiteral("projectId 不能为空。"));
    }
    if (m_name.trimmed().isEmpty()) {
        return fail(QStringLiteral("项目名称不能为空。"));
    }
    for (qsizetype index = 0; index < m_assets.size(); ++index) {
        const QJsonValue value = m_assets.at(index);
        if (!value.isObject()) {
            return fail(QStringLiteral("assets[%1] 必须是对象。").arg(index));
        }
        QString assetError;
        if (!AssetRecord::fromJson(value.toObject(), &assetError).has_value()) {
            return fail(QStringLiteral("assets[%1] 无效: %2").arg(index).arg(assetError));
        }
    }
    return true;
}

QJsonObject ProjectManifest::toJson() const
{
    return QJsonObject{
        {QStringLiteral("schemaVersion"), m_schemaVersion},
        {QStringLiteral("projectId"), m_projectId},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("createdAt"), m_createdAt},
        {QStringLiteral("modifiedAt"), m_modifiedAt},
        {QStringLiteral("assets"), m_assets},
        {QStringLiteral("reconstruction"),
         QJsonObject{
             {QStringLiteral("backend"), m_backend},
             {QStringLiteral("backendVersion"), m_backendVersion},
             {QStringLiteral("state"), m_reconstructionState},
             {QStringLiteral("activeTaskId"), m_reconstructionActiveTaskId},
             {QStringLiteral("latestTask"), m_latestReconstructionTask},
         }},
        {QStringLiteral("deviceMarkers"), m_deviceMarkers},
    };
}

int ProjectManifest::schemaVersion() const { return m_schemaVersion; }
const QString& ProjectManifest::projectId() const { return m_projectId; }
const QString& ProjectManifest::name() const { return m_name; }
const QString& ProjectManifest::createdAt() const { return m_createdAt; }
const QString& ProjectManifest::modifiedAt() const { return m_modifiedAt; }
const QJsonArray& ProjectManifest::assets() const { return m_assets; }
const QString& ProjectManifest::backend() const { return m_backend; }
const QString& ProjectManifest::backendVersion() const { return m_backendVersion; }
const QString& ProjectManifest::reconstructionState() const { return m_reconstructionState; }
const QString& ProjectManifest::reconstructionActiveTaskId() const
{
    return m_reconstructionActiveTaskId;
}
const QJsonObject& ProjectManifest::latestReconstructionTask() const
{
    return m_latestReconstructionTask;
}
const QJsonArray& ProjectManifest::deviceMarkers() const { return m_deviceMarkers; }

QList<AssetRecord> ProjectManifest::imageAssetRecords(QString* error) const
{
    QList<AssetRecord> records;
    for (qsizetype index = 0; index < m_assets.size(); ++index) {
        QString assetError;
        const std::optional<AssetRecord> record =
            AssetRecord::fromJson(m_assets.at(index).toObject(), &assetError);
        if (!record.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("assets[%1] 无效: %2").arg(index).arg(assetError);
            }
            return {};
        }
        records.append(*record);
    }
    return records;
}

void ProjectManifest::setName(const QString& name) { m_name = name.trimmed(); }

void ProjectManifest::setModifiedAt(const QString& modifiedAt) { m_modifiedAt = modifiedAt; }

void ProjectManifest::setAssets(const QJsonArray& assets) { m_assets = assets; }

void ProjectManifest::setImageAssetRecords(const QList<AssetRecord>& assets)
{
    QJsonArray jsonAssets;
    for (const AssetRecord& asset : assets) {
        jsonAssets.append(asset.toJson());
    }
    m_assets = jsonAssets;
}

void ProjectManifest::setBackendVersion(const QString& backendVersion)
{
    m_backendVersion = backendVersion;
}

void ProjectManifest::setReconstructionState(const QString& state)
{
    m_reconstructionState = state;
}

void ProjectManifest::setReconstructionMetadata(const QString& activeTaskId,
                                                const QJsonObject& latestTask)
{
    m_reconstructionActiveTaskId = activeTaskId;
    m_latestReconstructionTask = latestTask;
}

void ProjectManifest::setDeviceMarkers(const QJsonArray& markers)
{
    m_deviceMarkers = markers;
}

} // namespace vision3d
