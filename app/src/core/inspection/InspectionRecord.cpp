#include "core/inspection/InspectionRecord.h"

#include <QDir>
#include <QJsonValue>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace vision3d {
namespace {

bool readTimestamp(const QJsonValue& value, QDateTime* timestamp, QString* error)
{
    if (!value.isString()) {
        if (error != nullptr) {
            *error = QStringLiteral("InspectionRecord timestamp 必须是字符串。");
        }
        return false;
    }

    const QString text = value.toString().trimmed();
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(text, Qt::ISODate);
    }
    if (!parsed.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("InspectionRecord timestamp 不是有效 ISO 时间。");
        }
        return false;
    }
    if (timestamp != nullptr) {
        *timestamp = parsed.toUTC();
    }
    return true;
}

bool isProjectRelativeImagePath(const QString& path)
{
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    return !normalized.isEmpty()
           && !QDir::isAbsolutePath(normalized)
           && normalized != QStringLiteral(".")
           && !normalized.startsWith(QStringLiteral("../"))
           && !normalized.contains(QStringLiteral("/../"))
           && normalized.startsWith(QStringLiteral("images/"));
}

} // namespace

bool InspectionRecord::isValid(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (id.trimmed().isEmpty()) {
        return fail(QStringLiteral("InspectionRecord id 不能为空。"));
    }
    if (gaugeAssetId.trimmed().isEmpty()) {
        return fail(QStringLiteral("InspectionRecord gaugeAssetId 不能为空。"));
    }
    if (!std::isfinite(value)) {
        return fail(QStringLiteral("InspectionRecord value 必须是有限数字。"));
    }
    if (!timestamp.isValid()) {
        return fail(QStringLiteral("InspectionRecord timestamp 必须是有效时间。"));
    }
    if (dataSource == GaugeDataSource::None) {
        return fail(QStringLiteral("InspectionRecord dataSource 不能为 None。"));
    }
    if (!imageAssetId.trimmed().isEmpty() && imageAssetId.contains(QChar('/'))) {
        return fail(QStringLiteral("InspectionRecord imageAssetId 不能包含路径。"));
    }
    if (!sourceImagePath.trimmed().isEmpty()
        && !isProjectRelativeImagePath(sourceImagePath)) {
        return fail(QStringLiteral("InspectionRecord sourceImagePath 必须是 images/ 下的项目相对路径。"));
    }
    return true;
}

QJsonObject InspectionRecord::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("gaugeAssetId"), gaugeAssetId},
        {QStringLiteral("value"), value},
        {QStringLiteral("timestamp"), timestamp.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("dataSource"), gaugeDataSourceToString(dataSource)},
        {QStringLiteral("imageAssetId"),
         imageAssetId.trimmed().isEmpty() ? QJsonValue(QJsonValue::Null)
                                          : QJsonValue(imageAssetId)},
        {QStringLiteral("sourceImagePath"),
         sourceImagePath.trimmed().isEmpty() ? QJsonValue(QJsonValue::Null)
                                             : QJsonValue(
                                                   QDir::fromNativeSeparators(sourceImagePath))},
        {QStringLiteral("note"),
         note.trimmed().isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(note)},
    };
}

std::optional<InspectionRecord> InspectionRecord::fromJson(const QJsonObject& json,
                                                           QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<InspectionRecord> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    const auto readRequiredText = [&json, &fail](const QString& field)
        -> std::optional<QString> {
        const QJsonValue value = json.value(field);
        if (!value.isString() || value.toString().trimmed().isEmpty()) {
            fail(QStringLiteral("InspectionRecord 缺少有效 %1。").arg(field));
            return std::nullopt;
        }
        return value.toString().trimmed();
    };

    const std::optional<QString> id = readRequiredText(QStringLiteral("id"));
    if (!id.has_value()) {
        return std::nullopt;
    }
    const std::optional<QString> gaugeAssetId =
        readRequiredText(QStringLiteral("gaugeAssetId"));
    if (!gaugeAssetId.has_value()) {
        return std::nullopt;
    }

    const QJsonValue value = json.value(QStringLiteral("value"));
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        return fail(QStringLiteral("InspectionRecord value 必须是有限数字。"));
    }

    QDateTime timestamp;
    QString timestampError;
    if (!readTimestamp(json.value(QStringLiteral("timestamp")),
                       &timestamp,
                       &timestampError)) {
        return fail(timestampError);
    }

    const QJsonValue sourceValue = json.value(QStringLiteral("dataSource"));
    if (!sourceValue.isString()) {
        return fail(QStringLiteral("InspectionRecord dataSource 必须是字符串。"));
    }
    const std::optional<GaugeDataSource> source =
        gaugeDataSourceFromString(sourceValue.toString());
    if (!source.has_value() || *source == GaugeDataSource::None) {
        return fail(QStringLiteral("InspectionRecord dataSource 无效。"));
    }

    const auto readOptionalText = [&json, &fail](const QString& field)
        -> std::optional<QString> {
        const QJsonValue value = json.value(field);
        if (value.isUndefined() || value.isNull()) {
            return QString();
        }
        if (!value.isString()) {
            fail(QStringLiteral("InspectionRecord %1 必须是字符串或 null。").arg(field));
            return std::nullopt;
        }
        return value.toString().trimmed();
    };

    const std::optional<QString> imageAssetId = readOptionalText(QStringLiteral("imageAssetId"));
    if (!imageAssetId.has_value()) {
        return std::nullopt;
    }
    const std::optional<QString> sourceImagePath =
        readOptionalText(QStringLiteral("sourceImagePath"));
    if (!sourceImagePath.has_value()) {
        return std::nullopt;
    }
    const std::optional<QString> note = readOptionalText(QStringLiteral("note"));
    if (!note.has_value()) {
        return std::nullopt;
    }

    InspectionRecord record;
    record.id = *id;
    record.gaugeAssetId = *gaugeAssetId;
    record.value = value.toDouble();
    record.timestamp = timestamp;
    record.dataSource = *source;
    record.imageAssetId = *imageAssetId;
    record.sourceImagePath = QDir::fromNativeSeparators(*sourceImagePath);
    record.note = *note;

    QString validationError;
    if (!record.isValid(&validationError)) {
        return fail(validationError);
    }
    return record;
}

InspectionRecord InspectionRecord::create(const QString& gaugeAssetId,
                                          double value,
                                          const QDateTime& timestamp,
                                          GaugeDataSource dataSource,
                                          const QString& imageAssetId,
                                          const QString& sourceImagePath,
                                          const QString& note)
{
    InspectionRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.gaugeAssetId = gaugeAssetId.trimmed();
    record.value = value;
    record.timestamp = timestamp.toUTC();
    record.dataSource = dataSource;
    record.imageAssetId = imageAssetId.trimmed();
    record.sourceImagePath = QDir::fromNativeSeparators(sourceImagePath.trimmed());
    record.note = note.trimmed();
    return record;
}

bool InspectionRecordModel::add(const InspectionRecord& record, QString* error)
{
    QString validationError;
    if (!record.isValid(&validationError)) {
        if (error != nullptr) {
            *error = validationError;
        }
        return false;
    }
    if (contains(record.id)) {
        if (error != nullptr) {
            *error = QStringLiteral("InspectionRecord id 已存在: %1").arg(record.id);
        }
        return false;
    }
    m_records.append(record);
    return true;
}

bool InspectionRecordModel::remove(const QString& id, QString* error)
{
    const QString normalizedId = id.trimmed();
    for (qsizetype index = 0; index < m_records.size(); ++index) {
        if (m_records.at(index).id == normalizedId) {
            m_records.removeAt(index);
            return true;
        }
    }
    if (error != nullptr) {
        *error = QStringLiteral("未找到 InspectionRecord: %1").arg(normalizedId);
    }
    return false;
}

std::optional<InspectionRecord> InspectionRecordModel::findById(const QString& id) const
{
    const QString normalizedId = id.trimmed();
    for (const InspectionRecord& record : m_records) {
        if (record.id == normalizedId) {
            return record;
        }
    }
    return std::nullopt;
}

QList<InspectionRecord> InspectionRecordModel::recordsForGauge(
    const QString& gaugeAssetId) const
{
    const QString normalizedId = gaugeAssetId.trimmed();
    QList<InspectionRecord> result;
    for (const InspectionRecord& record : m_records) {
        if (record.gaugeAssetId == normalizedId) {
            result.append(record);
        }
    }
    std::sort(result.begin(), result.end(), [](const InspectionRecord& left,
                                               const InspectionRecord& right) {
        if (left.timestamp != right.timestamp) {
            return left.timestamp > right.timestamp;
        }
        return left.id > right.id;
    });
    return result;
}

QList<InspectionRecord> InspectionRecordModel::all() const
{
    return m_records;
}

qsizetype InspectionRecordModel::removeForGauge(const QString& gaugeAssetId)
{
    const QString normalizedId = gaugeAssetId.trimmed();
    qsizetype removed = 0;
    for (qsizetype index = m_records.size() - 1; index >= 0; --index) {
        if (m_records.at(index).gaugeAssetId == normalizedId) {
            m_records.removeAt(index);
            ++removed;
        }
    }
    return removed;
}

bool InspectionRecordModel::referencesImageAsset(const QString& imageAssetId) const
{
    const QString normalizedId = imageAssetId.trimmed();
    if (normalizedId.isEmpty()) {
        return false;
    }
    return std::any_of(m_records.cbegin(), m_records.cend(), [&normalizedId](const auto& record) {
        return record.imageAssetId == normalizedId;
    });
}

QJsonArray InspectionRecordModel::toJson() const
{
    QJsonArray result;
    for (const InspectionRecord& record : m_records) {
        result.append(record.toJson());
    }
    return result;
}

std::optional<InspectionRecordModel> InspectionRecordModel::fromJson(const QJsonArray& json,
                                                                      QString* error)
{
    InspectionRecordModel model;
    for (qsizetype index = 0; index < json.size(); ++index) {
        if (!json.at(index).isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("inspectionRecords[%1] 必须是对象。").arg(index);
            }
            return std::nullopt;
        }
        QString recordError;
        const std::optional<InspectionRecord> record =
            InspectionRecord::fromJson(json.at(index).toObject(), &recordError);
        if (!record.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("inspectionRecords[%1] 无效: %2")
                             .arg(index)
                             .arg(recordError);
            }
            return std::nullopt;
        }
        if (!model.add(*record, &recordError)) {
            if (error != nullptr) {
                *error = QStringLiteral("inspectionRecords[%1] 无法加入模型: %2")
                             .arg(index)
                             .arg(recordError);
            }
            return std::nullopt;
        }
    }
    return model;
}

bool InspectionRecordModel::contains(const QString& id) const
{
    return findById(id).has_value();
}

qsizetype InspectionRecordModel::size() const
{
    return m_records.size();
}

void InspectionRecordModel::clear()
{
    m_records.clear();
}

} // namespace vision3d
