#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace vision3d {

struct AssetRecord
{
    QString id;
    QString type = QStringLiteral("image");
    QString relativePath;
    QString originalFileName;
    qint64 fileSize = 0;
    int width = 0;
    int height = 0;
    QString sha256;

    bool isValid(QString* error = nullptr) const;
    QJsonObject toJson() const;
    static std::optional<AssetRecord> fromJson(const QJsonObject& json,
                                               QString* error = nullptr);
};

} // namespace vision3d
