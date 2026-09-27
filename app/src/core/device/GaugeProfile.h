#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

#include <optional>

namespace vision3d {

enum class GaugeDirection
{
    Clockwise,
    CounterClockwise,
};

struct CalibrationPoint
{
    double angleDegrees = 0.0;
    double value = 0.0;
};

struct GaugeProfile
{
    QString id;
    QString name;
    QString unit;
    double rangeMin = 0.0;
    double rangeMax = 1.0;
    GaugeDirection direction = GaugeDirection::Clockwise;
    QVector<CalibrationPoint> calibrationPoints;

    bool isValid(QString* error = nullptr) const;
    QVector<double> unwrappedCalibrationAngles(QString* error = nullptr) const;
    std::optional<double> valueForAngle(double angleDegrees,
                                        QString* error = nullptr) const;
    std::optional<double> angleForValue(double value, QString* error = nullptr) const;

    QJsonObject toJson() const;
    static std::optional<GaugeProfile> fromJson(const QJsonObject& json,
                                                QString* error = nullptr);

    static GaugeProfile pressure025Mpa();
};

QString gaugeDirectionToString(GaugeDirection direction);
std::optional<GaugeDirection> gaugeDirectionFromString(const QString& value);
QString gaugeDirectionDisplayName(GaugeDirection direction);

QList<GaugeProfile> builtInGaugeProfiles();
std::optional<GaugeProfile> builtInGaugeProfile(const QString& id);

} // namespace vision3d
