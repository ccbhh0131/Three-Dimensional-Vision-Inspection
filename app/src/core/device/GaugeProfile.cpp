#include "core/device/GaugeProfile.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace vision3d {
namespace {

constexpr double kAngleEpsilon = 1.0e-9;
constexpr double kValueEpsilon = 1.0e-9;

bool finite(double value)
{
    return std::isfinite(value);
}

double normalizeAngle(double angleDegrees)
{
    double normalized = std::fmod(angleDegrees, 360.0);
    if (normalized >= 180.0) {
        normalized -= 360.0;
    }
    if (normalized < -180.0) {
        normalized += 360.0;
    }
    return normalized;
}

bool readFiniteNumber(const QJsonObject& json,
                      const QString& field,
                      double* target,
                      QString* error)
{
    const QJsonValue value = json.value(field);
    if (!value.isDouble() || !finite(value.toDouble())) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeProfile %1 必须是有限数字。").arg(field);
        }
        return false;
    }
    *target = value.toDouble();
    return true;
}

bool validateBasic(const GaugeProfile& profile, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (profile.id.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeProfile id 不能为空。"));
    }
    if (profile.name.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeProfile name 不能为空。"));
    }
    if (profile.unit.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeProfile unit 不能为空。"));
    }
    if (!finite(profile.rangeMin) || !finite(profile.rangeMax)) {
        return fail(QStringLiteral("GaugeProfile range 必须是有限数字。"));
    }
    if (profile.rangeMax <= profile.rangeMin) {
        return fail(QStringLiteral("GaugeProfile rangeMax 必须大于 rangeMin。"));
    }
    if (profile.calibrationPoints.size() < 2) {
        return fail(QStringLiteral("GaugeProfile 至少需要两个 calibrationPoints。"));
    }

    double previousValue = 0.0;
    bool hasPreviousValue = false;
    for (const CalibrationPoint& point : profile.calibrationPoints) {
        if (!finite(point.angleDegrees) || !finite(point.value)) {
            return fail(QStringLiteral("GaugeProfile calibration point 必须是有限数字。"));
        }
        if (hasPreviousValue && point.value <= previousValue + kValueEpsilon) {
            return fail(QStringLiteral("GaugeProfile calibration point value 必须严格递增。"));
        }
        previousValue = point.value;
        hasPreviousValue = true;
    }
    if (std::abs(profile.calibrationPoints.first().value - profile.rangeMin)
            > kValueEpsilon
        || std::abs(profile.calibrationPoints.last().value - profile.rangeMax)
               > kValueEpsilon) {
        return fail(QStringLiteral(
            "GaugeProfile calibration points 必须覆盖 rangeMin 到 rangeMax。"));
    }
    return true;
}

bool buildUnwrappedAngles(const GaugeProfile& profile,
                          QVector<double>* angles,
                          QString* error)
{
    if (!validateBasic(profile, error)) {
        return false;
    }
    angles->clear();
    angles->reserve(profile.calibrationPoints.size());
    double previous = normalizeAngle(profile.calibrationPoints.first().angleDegrees);
    angles->append(previous);

    for (qsizetype index = 1; index < profile.calibrationPoints.size(); ++index) {
        double current = normalizeAngle(profile.calibrationPoints.at(index).angleDegrees);
        if (profile.direction == GaugeDirection::CounterClockwise) {
            while (current >= previous - kAngleEpsilon) {
                current -= 360.0;
            }
            if (current >= previous - kAngleEpsilon) {
                if (error != nullptr) {
                    *error = QStringLiteral("GaugeProfile CounterClockwise sweep 无法展开。 ");
                }
                return false;
            }
        } else {
            while (current <= previous + kAngleEpsilon) {
                current += 360.0;
            }
            if (current <= previous + kAngleEpsilon) {
                if (error != nullptr) {
                    *error = QStringLiteral("GaugeProfile Clockwise sweep 无法展开。 ");
                }
                return false;
            }
        }
        angles->append(current);
        previous = current;
    }
    return true;
}

std::optional<double> readAngle(const QJsonObject& json,
                                const QString& field,
                                QString* error)
{
    double value = 0.0;
    if (!readFiniteNumber(json, field, &value, error)) {
        return std::nullopt;
    }
    return value;
}

} // namespace

QString gaugeDirectionToString(GaugeDirection direction)
{
    switch (direction) {
    case GaugeDirection::Clockwise:
        return QStringLiteral("clockwise");
    case GaugeDirection::CounterClockwise:
        return QStringLiteral("counter_clockwise");
    }
    return QStringLiteral("clockwise");
}

std::optional<GaugeDirection> gaugeDirectionFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("clockwise") || normalized == QStringLiteral("cw")) {
        return GaugeDirection::Clockwise;
    }
    if (normalized == QStringLiteral("counter_clockwise")
        || normalized == QStringLiteral("counterclockwise")
        || normalized == QStringLiteral("ccw")) {
        return GaugeDirection::CounterClockwise;
    }
    return std::nullopt;
}

QString gaugeDirectionDisplayName(GaugeDirection direction)
{
    return direction == GaugeDirection::CounterClockwise
        ? QStringLiteral("逆时针")
        : QStringLiteral("顺时针");
}

bool GaugeProfile::isValid(QString* error) const
{
    QVector<double> ignored;
    return buildUnwrappedAngles(*this, &ignored, error);
}

QVector<double> GaugeProfile::unwrappedCalibrationAngles(QString* error) const
{
    QVector<double> angles;
    if (!buildUnwrappedAngles(*this, &angles, error)) {
        angles.clear();
    }
    return angles;
}

std::optional<double> GaugeProfile::valueForAngle(double angleDegrees, QString* error) const
{
    QVector<double> angles;
    if (!buildUnwrappedAngles(*this, &angles, error)) {
        return std::nullopt;
    }
    if (!finite(angleDegrees)) {
        if (error != nullptr) {
            *error = QStringLiteral("GaugeProfile angle 必须是有限数字。 ");
        }
        return std::nullopt;
    }

    const double lower = std::min(angles.first(), angles.last());
    const double upper = std::max(angles.first(), angles.last());
    double candidate = normalizeAngle(angleDegrees);
    while (candidate < lower - kAngleEpsilon) {
        candidate += 360.0;
    }
    while (candidate > upper + kAngleEpsilon) {
        candidate -= 360.0;
    }
    if (candidate < lower - kAngleEpsilon || candidate > upper + kAngleEpsilon) {
        if (error != nullptr) {
            *error = QStringLiteral("angle %1 超出 GaugeProfile 有效 sweep。 ")
                         .arg(angleDegrees, 0, 'f', 6)
                         .trimmed();
        }
        return std::nullopt;
    }

    for (qsizetype index = 0; index + 1 < angles.size(); ++index) {
        const double firstAngle = angles.at(index);
        const double secondAngle = angles.at(index + 1);
        const double segmentMin = std::min(firstAngle, secondAngle) - kAngleEpsilon;
        const double segmentMax = std::max(firstAngle, secondAngle) + kAngleEpsilon;
        if (candidate < segmentMin || candidate > segmentMax) {
            continue;
        }
        const double denominator = secondAngle - firstAngle;
        const double t = std::clamp((candidate - firstAngle) / denominator, 0.0, 1.0);
        const double firstValue = calibrationPoints.at(index).value;
        const double secondValue = calibrationPoints.at(index + 1).value;
        return firstValue + t * (secondValue - firstValue);
    }

    if (error != nullptr) {
        *error = QStringLiteral("angle %1 未落入 GaugeProfile calibration segment。 ")
                     .arg(angleDegrees, 0, 'f', 6)
                     .trimmed();
    }
    return std::nullopt;
}

std::optional<double> GaugeProfile::angleForValue(double value, QString* error) const
{
    QVector<double> angles;
    if (!buildUnwrappedAngles(*this, &angles, error)) {
        return std::nullopt;
    }
    if (!finite(value) || value < rangeMin - kValueEpsilon || value > rangeMax + kValueEpsilon) {
        if (error != nullptr) {
            *error = QStringLiteral("value %1 超出 GaugeProfile range。 ")
                         .arg(value, 0, 'f', 6)
                         .trimmed();
        }
        return std::nullopt;
    }
    for (qsizetype index = 0; index + 1 < calibrationPoints.size(); ++index) {
        const double firstValue = calibrationPoints.at(index).value;
        const double secondValue = calibrationPoints.at(index + 1).value;
        if (value < firstValue - kValueEpsilon || value > secondValue + kValueEpsilon) {
            continue;
        }
        const double t = std::clamp((value - firstValue) / (secondValue - firstValue), 0.0, 1.0);
        return normalizeAngle(angles.at(index) + t * (angles.at(index + 1) - angles.at(index)));
    }
    if (error != nullptr) {
        *error = QStringLiteral("value %1 未落入 GaugeProfile calibration segment。 ")
                     .arg(value, 0, 'f', 6)
                     .trimmed();
    }
    return std::nullopt;
}

QJsonObject GaugeProfile::toJson() const
{
    QJsonArray points;
    for (const CalibrationPoint& point : calibrationPoints) {
        points.append(QJsonObject{
            {QStringLiteral("angleDegrees"), point.angleDegrees},
            {QStringLiteral("value"), point.value},
        });
    }
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("rangeMin"), rangeMin},
        {QStringLiteral("rangeMax"), rangeMax},
        {QStringLiteral("direction"), gaugeDirectionToString(direction)},
        {QStringLiteral("calibrationPoints"), points},
    };
}

std::optional<GaugeProfile> GaugeProfile::fromJson(const QJsonObject& json, QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<GaugeProfile> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };
    const auto readText = [&json, &fail](const QString& field) -> std::optional<QString> {
        const QJsonValue value = json.value(field);
        if (!value.isString() || value.toString().trimmed().isEmpty()) {
            fail(QStringLiteral("GaugeProfile 缺少有效 %1。 ").arg(field).trimmed());
            return std::nullopt;
        }
        return value.toString().trimmed();
    };

    const std::optional<QString> id = readText(QStringLiteral("id"));
    const std::optional<QString> name = readText(QStringLiteral("name"));
    const std::optional<QString> unit = readText(QStringLiteral("unit"));
    if (!id.has_value() || !name.has_value() || !unit.has_value()) {
        return std::nullopt;
    }

    GaugeProfile profile;
    profile.id = *id;
    profile.name = *name;
    profile.unit = *unit;
    if (!readFiniteNumber(json, QStringLiteral("rangeMin"), &profile.rangeMin, error)
        || !readFiniteNumber(json, QStringLiteral("rangeMax"), &profile.rangeMax, error)) {
        return std::nullopt;
    }
    const QJsonValue directionValue = json.value(QStringLiteral("direction"));
    if (!directionValue.isString()) {
        return fail(QStringLiteral("GaugeProfile direction 必须是字符串。"));
    }
    const std::optional<GaugeDirection> direction =
        gaugeDirectionFromString(directionValue.toString());
    if (!direction.has_value()) {
        return fail(QStringLiteral("GaugeProfile direction 无效: %1。")
                        .arg(directionValue.toString()));
    }
    profile.direction = *direction;

    const QJsonValue pointsValue = json.value(QStringLiteral("calibrationPoints"));
    if (!pointsValue.isArray()) {
        return fail(QStringLiteral("GaugeProfile calibrationPoints 必须是数组。"));
    }
    const QJsonArray points = pointsValue.toArray();
    for (qsizetype index = 0; index < points.size(); ++index) {
        if (!points.at(index).isObject()) {
            return fail(QStringLiteral("GaugeProfile calibrationPoints[%1] 必须是对象。")
                            .arg(index));
        }
        const QJsonObject pointJson = points.at(index).toObject();
        const std::optional<double> angle =
            readAngle(pointJson, QStringLiteral("angleDegrees"), error);
        const std::optional<double> pointValue =
            readAngle(pointJson, QStringLiteral("value"), error);
        if (!angle.has_value() || !pointValue.has_value()) {
            return std::nullopt;
        }
        profile.calibrationPoints.append({*angle, *pointValue});
    }

    QString validationError;
    if (!profile.isValid(&validationError)) {
        return fail(validationError);
    }
    return profile;
}

GaugeProfile GaugeProfile::pressure025Mpa()
{
    GaugeProfile profile;
    profile.id = QStringLiteral("pressure_0_2_5_mpa");
    profile.name = QStringLiteral("0~2.5 MPa 入口压力表");
    profile.unit = QStringLiteral("MPa");
    profile.rangeMin = 0.0;
    profile.rangeMax = 2.5;
    profile.direction = GaugeDirection::CounterClockwise;
    profile.calibrationPoints = {
        {-132.5, 0.0},
        {173.5, 0.5},
        {117.0, 1.0},
        {64.5, 1.5},
        {11.2, 2.0},
        {-43.0, 2.5},
    };
    return profile;
}

QList<GaugeProfile> builtInGaugeProfiles()
{
    return {GaugeProfile::pressure025Mpa()};
}

std::optional<GaugeProfile> builtInGaugeProfile(const QString& id)
{
    const QString normalized = id.trimmed();
    for (const GaugeProfile& profile : builtInGaugeProfiles()) {
        if (profile.id == normalized) {
            return profile;
        }
    }
    return std::nullopt;
}

} // namespace vision3d
