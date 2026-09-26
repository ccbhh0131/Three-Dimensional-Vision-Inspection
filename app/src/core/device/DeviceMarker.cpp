#include "core/device/DeviceMarker.h"

#include <QDateTime>
#include <QUuid>

#include <cmath>

namespace vision3d {
namespace {

bool isFiniteVector(const QVector3D& value)
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z());
}

bool readFiniteNumber(const QJsonObject& json,
                      const QString& field,
                      float* target,
                      QString* error)
{
    const QJsonValue value = json.value(field);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        if (error != nullptr) {
            *error = QStringLiteral("DeviceMarker position.%1 必须是有限数字。").arg(field);
        }
        return false;
    }
    *target = static_cast<float>(value.toDouble());
    if (!std::isfinite(*target)) {
        if (error != nullptr) {
            *error = QStringLiteral("DeviceMarker position.%1 超出 float 范围。").arg(field);
        }
        return false;
    }
    return true;
}

} // namespace

bool DeviceMarker::isValid(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (id.trimmed().isEmpty()) {
        return fail(QStringLiteral("DeviceMarker id 不能为空。"));
    }
    if (name.trimmed().isEmpty()) {
        return fail(QStringLiteral("DeviceMarker name 不能为空。"));
    }
    if (!isFiniteVector(worldPosition)) {
        return fail(QStringLiteral("DeviceMarker worldPosition 必须是有限坐标。"));
    }
    if (reconstructionTaskId.trimmed().isEmpty()) {
        return fail(QStringLiteral("DeviceMarker reconstructionTaskId 不能为空。"));
    }
    if (!createdAt.trimmed().isEmpty()) {
        const QDateTime parsed = QDateTime::fromString(createdAt, Qt::ISODateWithMs);
        if (!parsed.isValid()) {
            return fail(QStringLiteral("DeviceMarker createdAt 不是有效 ISO 时间。"));
        }
    }
    return true;
}

QJsonObject DeviceMarker::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("position"),
         QJsonObject{
             {QStringLiteral("x"), worldPosition.x()},
             {QStringLiteral("y"), worldPosition.y()},
             {QStringLiteral("z"), worldPosition.z()},
         }},
        {QStringLiteral("reconstructionTaskId"), reconstructionTaskId},
    };
    if (!createdAt.trimmed().isEmpty()) {
        json.insert(QStringLiteral("createdAt"), createdAt);
    }
    return json;
}

std::optional<DeviceMarker> DeviceMarker::fromJson(const QJsonObject& json,
                                                   QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<DeviceMarker> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    if (!json.value(QStringLiteral("id")).isString()
        || json.value(QStringLiteral("id")).toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("DeviceMarker 缺少有效 id。"));
    }
    if (!json.value(QStringLiteral("name")).isString()
        || json.value(QStringLiteral("name")).toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("DeviceMarker 缺少有效 name。"));
    }
    if (!json.value(QStringLiteral("position")).isObject()) {
        return fail(QStringLiteral("DeviceMarker 缺少 position 对象。"));
    }
    if (!json.value(QStringLiteral("reconstructionTaskId")).isString()
        || json.value(QStringLiteral("reconstructionTaskId")).toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("DeviceMarker 缺少有效 reconstructionTaskId。"));
    }
    if (json.contains(QStringLiteral("createdAt"))
        && !json.value(QStringLiteral("createdAt")).isString()) {
        return fail(QStringLiteral("DeviceMarker createdAt 必须是字符串。"));
    }

    const QJsonObject position = json.value(QStringLiteral("position")).toObject();
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    QString positionError;
    if (!readFiniteNumber(position, QStringLiteral("x"), &x, &positionError)
        || !readFiniteNumber(position, QStringLiteral("y"), &y, &positionError)
        || !readFiniteNumber(position, QStringLiteral("z"), &z, &positionError)) {
        return fail(positionError);
    }

    DeviceMarker marker;
    marker.id = json.value(QStringLiteral("id")).toString().trimmed();
    marker.name = json.value(QStringLiteral("name")).toString().trimmed();
    marker.worldPosition = QVector3D(x, y, z);
    marker.reconstructionTaskId = json.value(QStringLiteral("reconstructionTaskId"))
                                      .toString()
                                      .trimmed();
    marker.createdAt = json.value(QStringLiteral("createdAt")).toString().trimmed();
    QString validationError;
    if (!marker.isValid(&validationError)) {
        return fail(validationError);
    }
    return marker;
}

DeviceMarker DeviceMarker::create(const QString& name,
                                  const QVector3D& worldPosition,
                                  const QString& reconstructionTaskId)
{
    DeviceMarker marker;
    marker.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    marker.name = name.trimmed();
    marker.worldPosition = worldPosition;
    marker.reconstructionTaskId = reconstructionTaskId.trimmed();
    marker.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return marker;
}

bool DeviceMarkerModel::add(const DeviceMarker& marker, QString* error)
{
    QString validationError;
    if (!marker.isValid(&validationError)) {
        if (error != nullptr) {
            *error = validationError;
        }
        return false;
    }
    if (contains(marker.id)) {
        if (error != nullptr) {
            *error = QStringLiteral("DeviceMarker id 已存在: %1").arg(marker.id);
        }
        return false;
    }
    m_markers.append(marker);
    return true;
}

bool DeviceMarkerModel::remove(const QString& id, QString* error)
{
    const QString normalizedId = id.trimmed();
    for (qsizetype index = 0; index < m_markers.size(); ++index) {
        if (m_markers.at(index).id == normalizedId) {
            m_markers.removeAt(index);
            return true;
        }
    }
    if (error != nullptr) {
        *error = QStringLiteral("未找到 DeviceMarker: %1").arg(normalizedId);
    }
    return false;
}

std::optional<DeviceMarker> DeviceMarkerModel::find(const QString& id) const
{
    const QString normalizedId = id.trimmed();
    for (const DeviceMarker& marker : m_markers) {
        if (marker.id == normalizedId) {
            return marker;
        }
    }
    return std::nullopt;
}

QList<DeviceMarker> DeviceMarkerModel::list() const
{
    return m_markers;
}

QList<DeviceMarker> DeviceMarkerModel::forReconstruction(
    const QString& reconstructionTaskId) const
{
    QList<DeviceMarker> result;
    const QString normalizedTaskId = reconstructionTaskId.trimmed();
    for (const DeviceMarker& marker : m_markers) {
        if (marker.reconstructionTaskId == normalizedTaskId) {
            result.append(marker);
        }
    }
    return result;
}

QJsonArray DeviceMarkerModel::toJson() const
{
    QJsonArray result;
    for (const DeviceMarker& marker : m_markers) {
        result.append(marker.toJson());
    }
    return result;
}

std::optional<DeviceMarkerModel> DeviceMarkerModel::fromJson(const QJsonArray& json,
                                                              QString* error)
{
    DeviceMarkerModel model;
    for (qsizetype index = 0; index < json.size(); ++index) {
        if (!json.at(index).isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("deviceMarkers[%1] 必须是对象。").arg(index);
            }
            return std::nullopt;
        }
        QString markerError;
        const std::optional<DeviceMarker> marker =
            DeviceMarker::fromJson(json.at(index).toObject(), &markerError);
        if (!marker.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("deviceMarkers[%1] 无效: %2")
                             .arg(index)
                             .arg(markerError);
            }
            return std::nullopt;
        }
        if (!model.add(*marker, &markerError)) {
            if (error != nullptr) {
                *error = QStringLiteral("deviceMarkers[%1] 无法加入模型: %2")
                             .arg(index)
                             .arg(markerError);
            }
            return std::nullopt;
        }
    }
    return model;
}

bool DeviceMarkerModel::contains(const QString& id) const
{
    return find(id).has_value();
}

qsizetype DeviceMarkerModel::size() const
{
    return m_markers.size();
}

void DeviceMarkerModel::clear()
{
    m_markers.clear();
}

} // namespace vision3d
