#pragma once

#include "core/device/GaugeAsset.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace vision3d {

struct InspectionRecord
{
    QString id;
    QString gaugeAssetId;
    double value = 0.0;
    QDateTime timestamp;
    GaugeDataSource dataSource = GaugeDataSource::None;
    QString imageAssetId;
    QString sourceImagePath;
    QString note;

    bool isValid(QString* error = nullptr) const;
    QJsonObject toJson() const;
    static std::optional<InspectionRecord> fromJson(const QJsonObject& json,
                                                     QString* error = nullptr);
    static InspectionRecord create(const QString& gaugeAssetId,
                                   double value,
                                   const QDateTime& timestamp,
                                   GaugeDataSource dataSource,
                                   const QString& imageAssetId = QString(),
                                   const QString& sourceImagePath = QString(),
                                   const QString& note = QString());
};

class InspectionRecordModel
{
public:
    bool add(const InspectionRecord& record, QString* error = nullptr);
    bool remove(const QString& id, QString* error = nullptr);
    std::optional<InspectionRecord> findById(const QString& id) const;
    QList<InspectionRecord> recordsForGauge(const QString& gaugeAssetId) const;
    QList<InspectionRecord> all() const;
    qsizetype removeForGauge(const QString& gaugeAssetId);
    bool referencesImageAsset(const QString& imageAssetId) const;

    QJsonArray toJson() const;
    static std::optional<InspectionRecordModel> fromJson(const QJsonArray& json,
                                                         QString* error = nullptr);

    bool contains(const QString& id) const;
    qsizetype size() const;
    void clear();

private:
    QList<InspectionRecord> m_records;
};

} // namespace vision3d
