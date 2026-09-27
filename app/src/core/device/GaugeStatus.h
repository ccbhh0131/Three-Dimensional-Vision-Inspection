#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <array>
#include <cmath>
#include <optional>

namespace vision3d {

enum class GaugeStatus
{
    Unknown,
    Normal,
    Warning,
    Alarm,
};

inline QString gaugeStatusToString(GaugeStatus status)
{
    switch (status) {
    case GaugeStatus::Unknown:
        return QStringLiteral("unknown");
    case GaugeStatus::Normal:
        return QStringLiteral("normal");
    case GaugeStatus::Warning:
        return QStringLiteral("warning");
    case GaugeStatus::Alarm:
        return QStringLiteral("alarm");
    }
    return QStringLiteral("unknown");
}

inline std::optional<GaugeStatus> gaugeStatusFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("unknown")) {
        return GaugeStatus::Unknown;
    }
    if (normalized == QStringLiteral("normal")) {
        return GaugeStatus::Normal;
    }
    if (normalized == QStringLiteral("warning")) {
        return GaugeStatus::Warning;
    }
    if (normalized == QStringLiteral("alarm")) {
        return GaugeStatus::Alarm;
    }
    return std::nullopt;
}

inline QString gaugeStatusDisplayName(GaugeStatus status)
{
    switch (status) {
    case GaugeStatus::Unknown:
        return QStringLiteral("未知");
    case GaugeStatus::Normal:
        return QStringLiteral("正常");
    case GaugeStatus::Warning:
        return QStringLiteral("警告");
    case GaugeStatus::Alarm:
        return QStringLiteral("报警");
    }
    return QStringLiteral("未知");
}

struct GaugeStatusRule
{
    std::optional<double> warningLow;
    std::optional<double> warningHigh;
    std::optional<double> alarmLow;
    std::optional<double> alarmHigh;

    bool isConfigured() const
    {
        return warningLow.has_value() || warningHigh.has_value()
            || alarmLow.has_value() || alarmHigh.has_value();
    }

    bool hasValidThresholdOrder(QString* error = nullptr) const
    {
        const auto fail = [error](const QString& message) {
            if (error != nullptr) {
                *error = message;
            }
            return false;
        };
        const std::array<const std::optional<double>*, 4> ordered{
            &alarmLow, &warningLow, &warningHigh, &alarmHigh};
        for (const std::optional<double>* threshold : ordered) {
            if (threshold->has_value() && !std::isfinite(**threshold)) {
                return fail(QStringLiteral("阈值必须是有限数字。"));
            }
        }
        for (qsizetype left = 0; left < static_cast<qsizetype>(ordered.size()); ++left) {
            if (!ordered.at(left)->has_value()) {
                continue;
            }
            for (qsizetype right = left + 1;
                 right < static_cast<qsizetype>(ordered.size());
                 ++right) {
                if (ordered.at(right)->has_value()
                    && ordered.at(left)->value() > ordered.at(right)->value()) {
                    return fail(QStringLiteral("阈值顺序无效。"));
                }
            }
        }
        return true;
    }

    bool isValid(double rangeMin, double rangeMax, QString* error = nullptr) const
    {
        const auto fail = [error](const QString& message) {
            if (error != nullptr) {
                *error = message;
            }
            return false;
        };
        if (!std::isfinite(rangeMin) || !std::isfinite(rangeMax)
            || rangeMax <= rangeMin) {
            return fail(QStringLiteral("GaugeAsset 量程无效。"));
        }
        const std::array<const std::optional<double>*, 4> thresholds{
            &alarmLow, &warningLow, &warningHigh, &alarmHigh};
        for (const std::optional<double>* threshold : thresholds) {
            if (threshold->has_value()
                && (!std::isfinite(**threshold)
                    || **threshold < rangeMin
                    || **threshold > rangeMax)) {
                return fail(QStringLiteral("阈值必须位于仪表量程内。"));
            }
        }
        return hasValidThresholdOrder(error);
    }

    QJsonObject toJson() const
    {
        const auto jsonValue = [](const std::optional<double>& value) {
            return value.has_value() ? QJsonValue(*value)
                                      : QJsonValue(QJsonValue::Null);
        };
        return QJsonObject{
            {QStringLiteral("warningLow"), jsonValue(warningLow)},
            {QStringLiteral("warningHigh"), jsonValue(warningHigh)},
            {QStringLiteral("alarmLow"), jsonValue(alarmLow)},
            {QStringLiteral("alarmHigh"), jsonValue(alarmHigh)},
        };
    }

    static std::optional<GaugeStatusRule> fromJson(const QJsonObject& json,
                                                   QString* error = nullptr)
    {
        const auto fail = [error](const QString& message)
            -> std::optional<GaugeStatusRule> {
            if (error != nullptr) {
                *error = message;
            }
            return std::nullopt;
        };
        GaugeStatusRule rule;
        const auto readOptional = [&json, &fail](const QString& field,
                                                 std::optional<double>* target) {
            const QJsonValue value = json.value(field);
            if (value.isUndefined() || value.isNull()) {
                target->reset();
                return true;
            }
            if (!value.isDouble() || !std::isfinite(value.toDouble())) {
                fail(QStringLiteral("GaugeStatusRule %1 必须是有限数字或 null。")
                         .arg(field));
                return false;
            }
            *target = value.toDouble();
            return true;
        };
        if (!readOptional(QStringLiteral("warningLow"), &rule.warningLow)
            || !readOptional(QStringLiteral("warningHigh"), &rule.warningHigh)
            || !readOptional(QStringLiteral("alarmLow"), &rule.alarmLow)
            || !readOptional(QStringLiteral("alarmHigh"), &rule.alarmHigh)) {
            return fail(QStringLiteral("GaugeStatusRule JSON 字段无效。"));
        }
        QString orderError;
        if (!rule.hasValidThresholdOrder(&orderError)) {
            return fail(orderError);
        }
        return rule;
    }
};

class GaugeStatusEvaluator final
{
public:
    static GaugeStatus evaluate(const std::optional<double>& value,
                                const std::optional<GaugeStatusRule>& rule)
    {
        if (!value.has_value() || !std::isfinite(*value)
            || !rule.has_value()) {
            return GaugeStatus::Unknown;
        }
        return evaluate(*value, *rule);
    }

    static GaugeStatus evaluate(double value, const GaugeStatusRule& rule)
    {
        if (!std::isfinite(value) || !rule.isConfigured()
            || !rule.hasValidThresholdOrder()) {
            return GaugeStatus::Unknown;
        }
        if ((rule.alarmLow.has_value() && value < *rule.alarmLow)
            || (rule.alarmHigh.has_value() && value > *rule.alarmHigh)) {
            return GaugeStatus::Alarm;
        }
        if ((rule.alarmLow.has_value() && value == *rule.alarmLow)
            || (rule.alarmHigh.has_value() && value == *rule.alarmHigh)
            || (rule.warningLow.has_value() && value < *rule.warningLow)
            || (rule.warningHigh.has_value() && value > *rule.warningHigh)) {
            return GaugeStatus::Warning;
        }
        return GaugeStatus::Normal;
    }
};

} // namespace vision3d
