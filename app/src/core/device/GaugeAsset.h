#pragma once

#include "core/device/ModbusTcpBinding.h"
#include "core/device/GaugeStatus.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace vision3d {

enum class GaugeDataSource
{
    None,
    Manual,
    Visual,
    Sensor,
};

QString gaugeDataSourceToString(GaugeDataSource source);
std::optional<GaugeDataSource> gaugeDataSourceFromString(const QString& value);
QString gaugeDataSourceDisplayName(GaugeDataSource source);

struct GaugeAsset
{
    QString id;
    QString deviceMarkerId;
    QString name;
    double rangeMin = 0.0;
    double rangeMax = 1.0;
    QString unit;
    QString gaugeProfileId;
    std::optional<double> latestValue;
    std::optional<QDateTime> latestTimestamp;
    GaugeDataSource dataSource = GaugeDataSource::None;
    std::optional<GaugeStatusRule> statusRule;
    std::optional<ModbusTcpBinding> modbusTcpBinding;

    bool isValid(QString* error = nullptr) const;
    QJsonObject toJson() const;
    static std::optional<GaugeAsset> fromJson(const QJsonObject& json,
                                              QString* error = nullptr);
    static GaugeAsset create(const QString& deviceMarkerId,
                             const QString& name,
                             double rangeMin,
                             double rangeMax,
                             const QString& unit);
};

class GaugeAssetModel
{
public:
    bool add(const GaugeAsset& asset, QString* error = nullptr);
    bool update(const GaugeAsset& asset, QString* error = nullptr);
    bool remove(const QString& id, QString* error = nullptr);
    std::optional<GaugeAsset> findById(const QString& id) const;
    std::optional<GaugeAsset> findByMarkerId(const QString& deviceMarkerId) const;

    QList<GaugeAsset> list() const;
    QJsonArray toJson() const;
    static std::optional<GaugeAssetModel> fromJson(const QJsonArray& json,
                                                   QString* error = nullptr);

    bool contains(const QString& id) const;
    qsizetype size() const;
    void clear();

private:
    QList<GaugeAsset> m_assets;
};

} // namespace vision3d
