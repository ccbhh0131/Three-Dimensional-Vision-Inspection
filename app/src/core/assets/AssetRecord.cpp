#include "core/assets/AssetRecord.h"

#include <QDir>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

namespace vision3d {

bool AssetRecord::isValid(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (id.trimmed().isEmpty() || id.contains(QLatin1Char('/'))
        || id.contains(QLatin1Char('\\'))) {
        return fail(QStringLiteral("资产 id 不能为空且不能包含路径分隔符。"));
    }
    if (type != QStringLiteral("image")) {
        return fail(QStringLiteral("不支持的资产类型: %1。当前仅支持 image。")
                        .arg(type));
    }
    const QString normalizedPath = QDir::cleanPath(QDir::fromNativeSeparators(relativePath));
    if (relativePath.trimmed().isEmpty() || QDir::isAbsolutePath(normalizedPath)
        || normalizedPath == QStringLiteral(".")
        || normalizedPath.startsWith(QStringLiteral("../"))
        || normalizedPath.contains(QStringLiteral("/../"))
        || !normalizedPath.startsWith(QStringLiteral("images/"))) {
        return fail(QStringLiteral("image 资产 relativePath 必须位于 images/ 下: %1")
                        .arg(relativePath));
    }
    if (originalFileName.trimmed().isEmpty()) {
        return fail(QStringLiteral("资产 originalFileName 不能为空。"));
    }
    if (fileSize < 0) {
        return fail(QStringLiteral("资产 fileSize 不能为负数。"));
    }
    if (width <= 0 || height <= 0) {
        return fail(QStringLiteral("资产 width 和 height 必须为正数。"));
    }
    const QRegularExpression shaExpression(QStringLiteral("^[0-9a-fA-F]{64}$"));
    if (!shaExpression.match(sha256).hasMatch()) {
        return fail(QStringLiteral("资产 sha256 必须是 64 位十六进制字符串。"));
    }
    return true;
}

QJsonObject AssetRecord::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("relativePath"), relativePath},
        {QStringLiteral("originalFileName"), originalFileName},
        {QStringLiteral("fileSize"), static_cast<qint64>(fileSize)},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height},
        {QStringLiteral("sha256"), sha256},
    };
}

std::optional<AssetRecord> AssetRecord::fromJson(const QJsonObject& json, QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<AssetRecord> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    const QStringList requiredStringFields = {
        QStringLiteral("id"),
        QStringLiteral("type"),
        QStringLiteral("relativePath"),
        QStringLiteral("originalFileName"),
        QStringLiteral("sha256"),
    };
    for (const QString& field : requiredStringFields) {
        if (!json.value(field).isString()) {
            return fail(QStringLiteral("资产缺少有效字段: %1。").arg(field));
        }
    }

    const auto readInteger = [&json, &fail](const QString& field,
                                            qint64* value) -> bool {
        const QJsonValue jsonValue = json.value(field);
        if (!jsonValue.isDouble()) {
            fail(QStringLiteral("资产缺少有效数值字段: %1。").arg(field));
            return false;
        }
        const double number = jsonValue.toDouble();
        if (number != static_cast<qint64>(number)) {
            fail(QStringLiteral("资产字段必须是整数: %1。").arg(field));
            return false;
        }
        *value = static_cast<qint64>(number);
        return true;
    };

    qint64 fileSize = 0;
    qint64 width = 0;
    qint64 height = 0;
    if (!readInteger(QStringLiteral("fileSize"), &fileSize)
        || !readInteger(QStringLiteral("width"), &width)
        || !readInteger(QStringLiteral("height"), &height)) {
        return std::nullopt;
    }

    AssetRecord record;
    record.id = json.value(QStringLiteral("id")).toString();
    record.type = json.value(QStringLiteral("type")).toString();
    record.relativePath = QDir::fromNativeSeparators(
        json.value(QStringLiteral("relativePath")).toString());
    record.originalFileName = json.value(QStringLiteral("originalFileName")).toString();
    record.fileSize = fileSize;
    record.width = static_cast<int>(width);
    record.height = static_cast<int>(height);
    record.sha256 = json.value(QStringLiteral("sha256")).toString().toLower();

    QString validationError;
    if (!record.isValid(&validationError)) {
        return fail(validationError);
    }
    return record;
}

} // namespace vision3d
