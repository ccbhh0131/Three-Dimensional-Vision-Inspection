#pragma once

#include "core/device/GaugeStatus.h"

#include <QDialog>

#include <optional>

class QLabel;
class QLineEdit;

namespace vision3d {

class GaugeStatusRuleDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GaugeStatusRuleDialog(const QString& title,
                                   double rangeMin,
                                   double rangeMax,
                                   const std::optional<GaugeStatusRule>& initial = std::nullopt,
                                   QWidget* parent = nullptr);

    std::optional<GaugeStatusRule> statusRule() const;

private slots:
    void acceptInput();

private:
    bool readOptionalDouble(QLineEdit* field,
                            const QString& label,
                            std::optional<double>* value);

    double m_rangeMin;
    double m_rangeMax;
    QLineEdit* m_alarmLowEdit;
    QLineEdit* m_warningLowEdit;
    QLineEdit* m_warningHighEdit;
    QLineEdit* m_alarmHighEdit;
    QLabel* m_errorLabel;
    std::optional<GaugeStatusRule> m_rule;
};

} // namespace vision3d
