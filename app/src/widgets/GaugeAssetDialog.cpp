#include "widgets/GaugeAssetDialog.h"
#include "widgets/ProductDialogStyle.h"

#include "core/device/GaugeProfile.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace vision3d {

GaugeAssetDialog::GaugeAssetDialog(const QString& title,
                                   const std::optional<GaugeAsset>& initial,
                                   QWidget* parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit(this))
    , m_rangeMinEdit(new QLineEdit(this))
    , m_rangeMaxEdit(new QLineEdit(this))
    , m_unitEdit(new QLineEdit(this))
    , m_profileCombo(new QComboBox(this))
    , m_errorLabel(new QLabel(this))
{
    setWindowTitle(title);
    setModal(true);

    m_nameEdit->setObjectName(QStringLiteral("gaugeNameEdit"));
    m_rangeMinEdit->setObjectName(QStringLiteral("gaugeRangeMinEdit"));
    m_rangeMaxEdit->setObjectName(QStringLiteral("gaugeRangeMaxEdit"));
    m_unitEdit->setObjectName(QStringLiteral("gaugeUnitEdit"));
    m_profileCombo->setObjectName(QStringLiteral("gaugeProfileCombo"));
    m_errorLabel->setObjectName(QStringLiteral("gaugeValidationLabel"));
    m_errorLabel->setStyleSheet(QStringLiteral("color: #b00020;"));
    m_errorLabel->setWordWrap(true);

    m_nameEdit->setText(initial.has_value() ? initial->name : QString());
    m_rangeMinEdit->setText(QString::number(initial.has_value() ? initial->rangeMin : 0.0,
                                            'g',
                                            15));
    m_rangeMaxEdit->setText(QString::number(initial.has_value() ? initial->rangeMax : 1.0,
                                            'g',
                                            15));
    m_unitEdit->setText(initial.has_value() ? initial->unit : QString());
    m_profileCombo->addItem(QStringLiteral("未绑定（仅手动读数）"), QString());
    for (const GaugeProfile& profile : builtInGaugeProfiles()) {
        m_profileCombo->addItem(QStringLiteral("%1 — %2~%3 %4")
                                    .arg(profile.id)
                                    .arg(profile.rangeMin, 0, 'g', 15)
                                    .arg(profile.rangeMax, 0, 'g', 15)
                                    .arg(profile.unit),
                                profile.id);
    }
    if (initial.has_value() && !initial->gaugeProfileId.trimmed().isEmpty()) {
        const int profileIndex = m_profileCombo->findData(initial->gaugeProfileId.trimmed());
        if (profileIndex >= 0) {
            m_profileCombo->setCurrentIndex(profileIndex);
        }
    }

    auto* form = new QVBoxLayout;
    const auto addField = [this, form](const QString& label, QWidget* field) {
        auto* labelWidget = new QLabel(label, this);
        labelWidget->setObjectName(QStringLiteral("productDialogFieldLabel"));
        form->addWidget(labelWidget);
        form->addWidget(field);
    };
    addField(QStringLiteral("仪表名称"), m_nameEdit);
    addField(QStringLiteral("量程下限"), m_rangeMinEdit);
    addField(QStringLiteral("量程上限"), m_rangeMaxEdit);
    addField(QStringLiteral("单位"), m_unitEdit);
    addField(QStringLiteral("视觉 Profile"), m_profileCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("gaugeDialogOkButton"));
    buttons->button(QDialogButtonBox::Cancel)
        ->setObjectName(QStringLiteral("gaugeDialogCancelButton"));
    connect(buttons, &QDialogButtonBox::accepted, this, &GaugeAssetDialog::acceptInput);
    connect(buttons, &QDialogButtonBox::rejected, this, &GaugeAssetDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_errorLabel);
    layout->addWidget(buttons);
    applyProductDialogStyle(this);
    styleProductButton(buttons->button(QDialogButtonBox::Ok), ProductButtonRole::Primary);
    styleProductButton(buttons->button(QDialogButtonBox::Cancel),
                       ProductButtonRole::Secondary);
    m_nameEdit->setFocus();
}

QString GaugeAssetDialog::assetName() const
{
    return m_nameEdit->text().trimmed();
}

QString GaugeAssetDialog::unit() const
{
    return m_unitEdit->text().trimmed();
}

double GaugeAssetDialog::rangeMin() const
{
    return m_rangeMinEdit->text().trimmed().toDouble();
}

double GaugeAssetDialog::rangeMax() const
{
    return m_rangeMaxEdit->text().trimmed().toDouble();
}

QString GaugeAssetDialog::gaugeProfileId() const
{
    return m_profileCombo->currentData().toString().trimmed();
}

bool GaugeAssetDialog::readFiniteDouble(QLineEdit* field,
                                        double* value,
                                        const QString& label)
{
    bool ok = false;
    const double parsed = field->text().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(parsed)) {
        m_errorLabel->setText(QStringLiteral("%1 必须是有限数字。").arg(label));
        return false;
    }
    *value = parsed;
    return true;
}

void GaugeAssetDialog::acceptInput()
{
    if (assetName().isEmpty()) {
        m_errorLabel->setText(QStringLiteral("仪表名称不能为空。"));
        m_nameEdit->setFocus();
        return;
    }
    if (unit().isEmpty()) {
        m_errorLabel->setText(QStringLiteral("单位不能为空。"));
        m_unitEdit->setFocus();
        return;
    }

    double rangeMinValue = 0.0;
    double rangeMaxValue = 0.0;
    if (!readFiniteDouble(m_rangeMinEdit, &rangeMinValue, QStringLiteral("量程下限"))
        || !readFiniteDouble(m_rangeMaxEdit, &rangeMaxValue, QStringLiteral("量程上限"))) {
        return;
    }
    if (rangeMaxValue <= rangeMinValue) {
        m_errorLabel->setText(QStringLiteral("量程上限必须大于量程下限。"));
        m_rangeMaxEdit->setFocus();
        return;
    }
    accept();
}

} // namespace vision3d
