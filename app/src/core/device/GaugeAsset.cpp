#include "core/device/GaugeAsset.h"

#include <QJsonValue>
#include <QUuid>

#include <cmath>

namespace vision3d {
namespace {

bool isKnownDataSource(GaugeDataSource source)
{
    switch (source) {
    case GaugeDataSource::None:
    case GaugeDataSource::Manual:
    case GaugeDataSource::Visual:
    case GaugeDataSource::Sensor:
        return true;
    }
    return false;
}

bool readFiniteNumber(const QJsonObject& json,
                      const QString& field,
                      double* target,
                      QString* error)
{
    const QJsonValue value = json.value(field);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeAsset %1 必须是有限数字。").arg(field);
        }
        return false;
    }
    *target = value.toDouble();
    return true;
}

std::optional<QDateTime> readTimestamp(const QJsonObject& json, QString* error)
{
    const QJsonValue value = json.value(QStringLiteral("latestTimestamp"));
    if (!value.isString()) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeAsset latestTimestamp 必须是字符串或 null。");
        }
        return std::nullopt;
    }
    const QString text = value.toString().trimmed();
    QDateTime timestamp = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!timestamp.isValid()) {
        timestamp = QDateTime::fromString(text, Qt::ISODate);
    }
    if (!timestamp.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeAsset latestTimestamp 不是有效 ISO 时间。");
        }
        return std::nullopt;
    }
    return timestamp;
}

} // namespace

QString gaugeDataSourceToString(GaugeDataSource source)
{
    switch (source) {
    case GaugeDataSource::None:
        return QStringLiteral("none");
    case GaugeDataSource::Manual:
        return QStringLiteral("manual");
    case GaugeDataSource::Visual:
        return QStringLiteral("visual");
    case GaugeDataSource::Sensor:
        return QStringLiteral("sensor");
    }
    return QStringLiteral("none");
}

std::optional<GaugeDataSource> gaugeDataSourceFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("none")) {
        return GaugeDataSource::None;
    }
    if (normalized == QStringLiteral("manual")) {
        return GaugeDataSource::Manual;
    }
    if (normalized == QStringLiteral("visual")) {
        return GaugeDataSource::Visual;
    }
    if (normalized == QStringLiteral("sensor")) {
        return GaugeDataSource::Sensor;
    }
    return std::nullopt;
}

QString gaugeDataSourceDisplayName(GaugeDataSource source)
{
    switch (source) {
    case GaugeDataSource::None:
        return QStringLiteral("暂无");
    case GaugeDataSource::Manual:
        return QStringLiteral("手动");
    case GaugeDataSource::Visual:
        return QStringLiteral("视觉");
    case GaugeDataSource::Sensor:
        return QStringLiteral("传感器");
    }
    return QStringLiteral("暂无");
}

bool GaugeAsset::isValid(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (id.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeAsset id 不能为空。"));
    }
    if (deviceMarkerId.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeAsset deviceMarkerId 不能为空。"));
    }
    if (name.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeAsset name 不能为空。"));
    }
    if (!std::isfinite(rangeMin)) {
        return fail(QStringLiteral("GaugeAsset rangeMin 必须是有限数字。"));
    }
    if (!std::isfinite(rangeMax)) {
        return fail(QStringLiteral("GaugeAsset rangeMax 必须是有限数字。"));
    }
    if (rangeMax <= rangeMin) {
        return fail(QStringLiteral("GaugeAsset rangeMax 必须大于 rangeMin。"));
    }
    if (unit.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeAsset unit 不能为空。"));
    }
    if (latestValue.has_value() && !std::isfinite(*latestValue)) {
        return fail(QStringLiteral("GaugeAsset latestValue 必须是有限数字。"));
    }
    if (latestTimestamp.has_value() && !latestTimestamp->isValid()) {
        return fail(QStringLiteral("GaugeAsset latestTimestamp 必须是有效时间。"));
    }
    if (!isKnownDataSource(dataSource)) {
        return fail(QStringLiteral("GaugeAsset dataSource 无效。"));
    }
    if (statusRule.has_value()) {
        QString ruleError;
        if (!statusRule->isValid(rangeMin, rangeMax, &ruleError)) {
            return fail(QStringLiteral("GaugeAsset statusRule 无效: %1").arg(ruleError));
        }
    }
    if (modbusTcpBinding.has_value()) {
        QString bindingError;
        if (modbusTcpBinding->gaugeAssetId.trimmed() != id.trimmed()) {
            return fail(QStringLiteral("GaugeAsset modbusTcpBinding gaugeAssetId 必须匹配资产 id。"));
        }
        if (!modbusTcpBinding->isValid(&bindingError)) {
            return fail(QStringLiteral("GaugeAsset modbusTcpBinding 无效: %1").arg(bindingError));
        }
    }
    return true;
}

QJsonObject GaugeAsset::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("deviceMarkerId"), deviceMarkerId},
        {QStringLiteral("name"), name},
        {QStringLiteral("rangeMin"), rangeMin},
        {QStringLiteral("rangeMax"), rangeMax},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("gaugeProfileId"), gaugeProfileId},
        {QStringLiteral("dataSource"), gaugeDataSourceToString(dataSource)},
    };
    json.insert(QStringLiteral("latestValue"),
                latestValue.has_value() ? QJsonValue(*latestValue)
                                        : QJsonValue(QJsonValue::Null));
    json.insert(QStringLiteral("latestTimestamp"),
                latestTimestamp.has_value()
                    ? QJsonValue(latestTimestamp->toUTC().toString(Qt::ISODateWithMs))
                    : QJsonValue(QJsonValue::Null));
    json.insert(QStringLiteral("statusRule"),
                statusRule.has_value() ? QJsonValue(statusRule->toJson())
                                       : QJsonValue(QJsonValue::Null));
    json.insert(QStringLiteral("modbusTcpBinding"),
                modbusTcpBinding.has_value() ? QJsonValue(modbusTcpBinding->toJson())
                                             : QJsonValue(QJsonValue::Null));
    return json;
}

std::optional<GaugeAsset> GaugeAsset::fromJson(const QJsonObject& json, QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<GaugeAsset> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    const auto readText = [&json, &fail](const QString& field) -> std::optional<QString> {
        const QJsonValue value = json.value(field);
        if (!value.isString() || value.toString().trimmed().isEmpty()) {
            fail(QStringLiteral("GaugeAsset 缺少有效 %1。").arg(field));
            return std::nullopt;
        }
        return value.toString().trimmed();
    };

    const std::optional<QString> id = readText(QStringLiteral("id"));
    if (!id.has_value()) {
        return std::nullopt;
    }
    const std::optional<QString> markerId = readText(QStringLiteral("deviceMarkerId"));
    if (!markerId.has_value()) {
        return std::nullopt;
    }
    const std::optional<QString> name = readText(QStringLiteral("name"));
    if (!name.has_value()) {
        return std::nullopt;
    }
    const std::optional<QString> unit = readText(QStringLiteral("unit"));
    if (!unit.has_value()) {
        return std::nullopt;
    }

    GaugeAsset asset;
    asset.id = *id;
    asset.deviceMarkerId = *markerId;
    asset.name = *name;
    asset.unit = *unit;
    const QJsonValue profileValue = json.value(QStringLiteral("gaugeProfileId"));
    if (!profileValue.isUndefined() && !profileValue.isNull()) {
        if (!profileValue.isString()) {
            return fail(QStringLiteral("GaugeAsset gaugeProfileId 必须是字符串或 null。"));
        }
        asset.gaugeProfileId = profileValue.toString().trimmed();
    }
    QString numberError;
    if (!readFiniteNumber(json, QStringLiteral("rangeMin"), &asset.rangeMin, &numberError)
        || !readFiniteNumber(json, QStringLiteral("rangeMax"), &asset.rangeMax, &numberError)) {
        return fail(numberError);
    }

    const QJsonValue latestValue = json.value(QStringLiteral("latestValue"));
    if (!latestValue.isUndefined() && !latestValue.isNull()) {
        if (!latestValue.isDouble() || !std::isfinite(latestValue.toDouble())) {
            return fail(QStringLiteral("GaugeAsset latestValue 必须是有限数字或 null。"));
        }
        asset.latestValue = latestValue.toDouble();
    }

    const QJsonValue latestTimestamp = json.value(QStringLiteral("latestTimestamp"));
    if (!latestTimestamp.isUndefined() && !latestTimestamp.isNull()) {
        QString timestampError;
        const std::optional<QDateTime> parsed = readTimestamp(json, &timestampError);
        if (!parsed.has_value()) {
            return fail(timestampError);
        }
        asset.latestTimestamp = *parsed;
    }

    const QJsonValue sourceValue = json.value(QStringLiteral("dataSource"));
    if (!sourceValue.isUndefined()) {
        if (!sourceValue.isString()) {
            return fail(QStringLiteral("GaugeAsset dataSource 必须是字符串。"));
        }
        const std::optional<GaugeDataSource> source =
            gaugeDataSourceFromString(sourceValue.toString());
        if (!source.has_value()) {
            return fail(QStringLiteral("GaugeAsset dataSource 无效: %1。")
                            .arg(sourceValue.toString()));
        }
        asset.dataSource = *source;
    }

    const QJsonValue statusRuleValue = json.value(QStringLiteral("statusRule"));
    if (!statusRuleValue.isUndefined() && !statusRuleValue.isNull()) {
        if (!statusRuleValue.isObject()) {
            return fail(QStringLiteral("GaugeAsset statusRule 必须是对象或 null。"));
        }
        QString ruleError;
        const std::optional<GaugeStatusRule> rule =
            GaugeStatusRule::fromJson(statusRuleValue.toObject(), &ruleError);
        if (!rule.has_value()) {
            return fail(QStringLiteral("GaugeAsset statusRule 无效: %1").arg(ruleError));
        }
        asset.statusRule = *rule;
    }

    const QJsonValue modbusBindingValue = json.value(QStringLiteral("modbusTcpBinding"));
    if (!modbusBindingValue.isUndefined() && !modbusBindingValue.isNull()) {
        if (!modbusBindingValue.isObject()) {
            return fail(QStringLiteral("GaugeAsset modbusTcpBinding 必须是对象或 null。"));
        }
        QString bindingError;
        const std::optional<ModbusTcpBinding> binding =
            ModbusTcpBinding::fromJson(modbusBindingValue.toObject(), &bindingError);
        if (!binding.has_value()) {
            return fail(QStringLiteral("GaugeAsset modbusTcpBinding 无效: %1").arg(bindingError));
        }
        asset.modbusTcpBinding = *binding;
    }

    QString validationError;
    if (!asset.isValid(&validationError)) {
        return fail(validationError);
    }
    return asset;
}

GaugeAsset GaugeAsset::create(const QString& deviceMarkerId,
                              const QString& name,
                              double rangeMin,
                              double rangeMax,
                              const QString& unit)
{
    GaugeAsset asset;
    asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.deviceMarkerId = deviceMarkerId.trimmed();
    asset.name = name.trimmed();
    asset.rangeMin = rangeMin;
    asset.rangeMax = rangeMax;
    asset.unit = unit.trimmed();
    return asset;
}

bool GaugeAssetModel::add(const GaugeAsset& asset, QString* error)
{
    QString validationError;
    if (!asset.isValid(&validationError)) {
        if (error != nullptr) {
            *error = validationError;
        }
        return false;
    }
    if (contains(asset.id)) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeAsset id 已存在: %1").arg(asset.id);
        }
        return false;
    }
    if (findByMarkerId(asset.deviceMarkerId).has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("DeviceMarker 已绑定 GaugeAsset: %1")
                         .arg(asset.deviceMarkerId);
        }
        return false;
    }
    m_assets.append(asset);
    return true;
}

bool GaugeAssetModel::update(const GaugeAsset& asset, QString* error)
{
    QString validationError;
    if (!asset.isValid(&validationError)) {
        if (error != nullptr) {
            *error = validationError;
        }
        return false;
    }
    for (qsizetype index = 0; index < m_assets.size(); ++index) {
        if (m_assets.at(index).id != asset.id) {
            continue;
        }
        const std::optional<GaugeAsset> bound = findByMarkerId(asset.deviceMarkerId);
        if (bound.has_value() && bound->id != asset.id) {
            if (error != nullptr) {
                *error = QStringLiteral("DeviceMarker 已绑定其他 GaugeAsset: %1")
                             .arg(asset.deviceMarkerId);
            }
            return false;
        }
        m_assets[index] = asset;
        return true;
    }
    if (error != nullptr) {
        *error = QStringLiteral("未找到 GaugeAsset: %1").arg(asset.id.trimmed());
    }
    return false;
}

bool GaugeAssetModel::remove(const QString& id, QString* error)
{
    const QString normalizedId = id.trimmed();
    for (qsizetype index = 0; index < m_assets.size(); ++index) {
        if (m_assets.at(index).id == normalizedId) {
            m_assets.removeAt(index);
            return true;
        }
    }
    if (error != nullptr) {
        *error = QStringLiteral("未找到 GaugeAsset: %1").arg(normalizedId);
    }
    return false;
}

std::optional<GaugeAsset> GaugeAssetModel::findById(const QString& id) const
{
    const QString normalizedId = id.trimmed();
    for (const GaugeAsset& asset : m_assets) {
        if (asset.id == normalizedId) {
            return asset;
        }
    }
    return std::nullopt;
}

std::optional<GaugeAsset> GaugeAssetModel::findByMarkerId(const QString& deviceMarkerId) const
{
    const QString normalizedId = deviceMarkerId.trimmed();
    for (const GaugeAsset& asset : m_assets) {
        if (asset.deviceMarkerId == normalizedId) {
            return asset;
        }
    }
    return std::nullopt;
}

QList<GaugeAsset> GaugeAssetModel::list() const
{
    return m_assets;
}

QJsonArray GaugeAssetModel::toJson() const
{
    QJsonArray result;
    for (const GaugeAsset& asset : m_assets) {
        result.append(asset.toJson());
    }
    return result;
}

std::optional<GaugeAssetModel> GaugeAssetModel::fromJson(const QJsonArray& json, QString* error)
{
    GaugeAssetModel model;
    for (qsizetype index = 0; index < json.size(); ++index) {
        if (!json.at(index).isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("gaugeAssets[%1] 必须是对象。").arg(index);
            }
            return std::nullopt;
        }
        QString assetError;
        const std::optional<GaugeAsset> asset =
            GaugeAsset::fromJson(json.at(index).toObject(), &assetError);
        if (!asset.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("gaugeAssets[%1] 无效: %2")
                             .arg(index)
                             .arg(assetError);
            }
            return std::nullopt;
        }
        if (!model.add(*asset, &assetError)) {
            if (error != nullptr) {
                *error = QStringLiteral("gaugeAssets[%1] 无法加入模型: %2")
                             .arg(index)
                             .arg(assetError);
            }
            return std::nullopt;
        }
    }
    return model;
}

bool GaugeAssetModel::contains(const QString& id) const
{
    return findById(id).has_value();
}

qsizetype GaugeAssetModel::size() const
{
    return m_assets.size();
}

void GaugeAssetModel::clear()
{
    m_assets.clear();
}

} // namespace vision3d
