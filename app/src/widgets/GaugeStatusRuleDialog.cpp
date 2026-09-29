#include "widgets/GaugeStatusRuleDialog.h"
#include "widgets/ProductDialogStyle.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace vision3d {

GaugeStatusRuleDialog::GaugeStatusRuleDialog(
    const QString& title,
    double rangeMin,
    double rangeMax,
    const std::optional<GaugeStatusRule>& initial,
    QWidget* parent)
    : QDialog(parent)
    , m_rangeMin(rangeMin)
    , m_rangeMax(rangeMax)
    , m_alarmLowEdit(new QLineEdit(this))
    , m_warningLowEdit(new QLineEdit(this))
    , m_warningHighEdit(new QLineEdit(this))
    , m_alarmHighEdit(new QLineEdit(this))
    , m_errorLabel(new QLabel(this))
    , m_rule(initial)
{
    setWindowTitle(title);
    setModal(true);
    setObjectName(QStringLiteral("gaugeStatusRuleDialog"));

    m_alarmLowEdit->setObjectName(QStringLiteral("gaugeAlarmLowEdit"));
    m_warningLowEdit->setObjectName(QStringLiteral("gaugeWarningLowEdit"));
    m_warningHighEdit->setObjectName(QStringLiteral("gaugeWarningHighEdit"));
    m_alarmHighEdit->setObjectName(QStringLiteral("gaugeAlarmHighEdit"));
    m_errorLabel->setObjectName(QStringLiteral("gaugeStatusRuleValidationLabel"));
    m_errorLabel->setStyleSheet(QStringLiteral("color: #b00020;"));
    m_errorLabel->setWordWrap(true);

    const auto setText = [](QLineEdit* field, const std::optional<double>& value) {
        if (value.has_value()) {
            field->setText(QString::number(*value, 'g', 15));
        }
        field->setPlaceholderText(QStringLiteral("留空表示不配置"));
    };
    setText(m_alarmLowEdit, initial.has_value() ? initial->alarmLow : std::nullopt);
    setText(m_warningLowEdit, initial.has_value() ? initial->warningLow : std::nullopt);
    setText(m_warningHighEdit, initial.has_value() ? initial->warningHigh : std::nullopt);
    setText(m_alarmHighEdit, initial.has_value() ? initial->alarmHigh : std::nullopt);

    auto* form = new QVBoxLayout;
    const auto addField = [this, form](const QString& label, QWidget* field) {
        auto* labelWidget = new QLabel(label, this);
        labelWidget->setObjectName(QStringLiteral("productDialogFieldLabel"));
        form->addWidget(labelWidget);
        form->addWidget(field);
    };
    addField(QStringLiteral("报警下限"), m_alarmLowEdit);
    addField(QStringLiteral("警告下限"), m_warningLowEdit);
    addField(QStringLiteral("警告上限"), m_warningHighEdit);
    addField(QStringLiteral("报警上限"), m_alarmHighEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)
        ->setObjectName(QStringLiteral("gaugeStatusRuleDialogOkButton"));
    buttons->button(QDialogButtonBox::Cancel)
        ->setObjectName(QStringLiteral("gaugeStatusRuleDialogCancelButton"));
    connect(buttons, &QDialogButtonBox::accepted, this, &GaugeStatusRuleDialog::acceptInput);
    connect(buttons, &QDialogButtonBox::rejected, this, &GaugeStatusRuleDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        QStringLiteral("阈值必须按量程和上下限顺序配置；所有字段均可留空。"), this));
    layout->addLayout(form);
    layout->addWidget(m_errorLabel);
    layout->addWidget(buttons);
    applyProductDialogStyle(this);
    styleProductButton(buttons->button(QDialogButtonBox::Ok), ProductButtonRole::Primary);
    styleProductButton(buttons->button(QDialogButtonBox::Cancel),
                       ProductButtonRole::Secondary);
    m_alarmLowEdit->setFocus();
}

std::optional<GaugeStatusRule> GaugeStatusRuleDialog::statusRule() const
{
    return m_rule;
}

bool GaugeStatusRuleDialog::readOptionalDouble(QLineEdit* field,
                                               const QString& label,
                                               std::optional<double>* value)
{
    const QString text = field->text().trimmed();
    if (text.isEmpty()) {
        value->reset();
        return true;
    }
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (!ok || !std::isfinite(parsed)) {
        m_errorLabel->setText(QStringLiteral("%1 必须是有限数字或留空。").arg(label));
        field->setFocus();
        return false;
    }
    *value = parsed;
    return true;
}

void GaugeStatusRuleDialog::acceptInput()
{
    GaugeStatusRule candidate;
    if (!readOptionalDouble(m_alarmLowEdit, QStringLiteral("报警下限"), &candidate.alarmLow)
        || !readOptionalDouble(m_warningLowEdit,
                               QStringLiteral("警告下限"),
                               &candidate.warningLow)
        || !readOptionalDouble(m_warningHighEdit,
                               QStringLiteral("警告上限"),
                               &candidate.warningHigh)
        || !readOptionalDouble(m_alarmHighEdit,
                               QStringLiteral("报警上限"),
                               &candidate.alarmHigh)) {
        return;
    }

    QString validationError;
    if (!candidate.isValid(m_rangeMin, m_rangeMax, &validationError)) {
        m_errorLabel->setText(validationError.isEmpty()
                                   ? QStringLiteral("阈值顺序无效。")
                                   : validationError);
        return;
    }
    m_rule = candidate.isConfigured()
        ? std::optional<GaugeStatusRule>(candidate)
        : std::nullopt;
    accept();
}

} // namespace vision3d
