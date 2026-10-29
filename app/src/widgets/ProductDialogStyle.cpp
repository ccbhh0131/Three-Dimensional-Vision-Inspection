#include "widgets/ProductDialogStyle.h"

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QStyle>
#include <QTimer>

namespace vision3d {

namespace {

QString buttonRoleName(ProductButtonRole role)
{
    switch (role) {
    case ProductButtonRole::Secondary:
        return QStringLiteral("secondary");
    case ProductButtonRole::Primary:
        return QStringLiteral("primary");
    case ProductButtonRole::Danger:
        return QStringLiteral("danger");
    }
    return QStringLiteral("secondary");
}

constexpr auto kProductDialogStyle = R"STYLE(
QDialog {
    background-color: #F7F5F0;
    color: #202522;
}
QDialog QLabel {
    color: #202522;
}
QDialog QLabel#productDialogDescription {
    color: #59615C;
}
QDialog QLabel#productDialogError,
QDialog QLabel#gaugeValidationLabel,
QDialog QLabel#gaugeStatusRuleValidationLabel {
    color: #A84236;
}
QDialog QLineEdit,
QDialog QComboBox,
QDialog QSpinBox,
QDialog QDoubleSpinBox {
    background-color: #FFFEFB;
    border: 1px solid #DDD9D1;
    border-radius: 8px;
    padding: 7px 9px;
    color: #202522;
    selection-background-color: #DDEBE3;
    selection-color: #202522;
    min-height: 18px;
}
QDialog QLineEdit:focus,
QDialog QComboBox:focus,
QDialog QSpinBox:focus,
QDialog QDoubleSpinBox:focus {
    border: 2px solid #176B4D;
    padding: 6px 8px;
}
QDialog QLineEdit:disabled,
QDialog QComboBox:disabled,
QDialog QSpinBox:disabled,
QDialog QDoubleSpinBox:disabled {
    background-color: #F0EFEB;
    border-color: #E1DED7;
    color: #8C948F;
}
QDialog QComboBox::drop-down {
    border: 0px;
    width: 24px;
}
QDialog QPushButton {
    background-color: #FFFEFB;
    border: 1px solid #C8C4BB;
    border-radius: 8px;
    padding: 7px 16px;
    color: #202522;
    min-height: 18px;
}
QDialog QPushButton:hover {
    background-color: #F2F0EB;
    border-color: #176B4D;
}
QDialog QPushButton:pressed {
    background-color: #E9F1EC;
}
QDialog QPushButton:focus {
    border: 2px solid #176B4D;
    padding: 6px 15px;
}
QDialog QPushButton:disabled {
    background-color: #F0EFEB;
    border-color: #E1DED7;
    color: #8C948F;
}
QDialog QPushButton[productRole="primary"] {
    background-color: #176B4D;
    border-color: #176B4D;
    color: #FFFFFF;
}
QDialog QPushButton[productRole="primary"]:hover {
    background-color: #145D43;
    border-color: #145D43;
}
QDialog QPushButton[productRole="primary"]:pressed {
    background-color: #104D38;
    border-color: #104D38;
}
QDialog QPushButton[productRole="primary"]:disabled {
    background-color: #F0EFEB;
    border-color: #E1DED7;
    color: #8C948F;
}
QDialog QPushButton[productRole="danger"] {
    background-color: #FFF7F5;
    border-color: #D99A92;
    color: #A84236;
}
QDialog QPushButton[productRole="danger"]:hover {
    background-color: #FBE9E5;
    border-color: #A84236;
}
QDialog QPushButton[productRole="danger"]:pressed {
    background-color: #F4D6D0;
}
QDialog QGroupBox {
    background-color: #FFFEFB;
    border: 1px solid #DDD9D1;
    border-radius: 8px;
    margin-top: 12px;
    padding-top: 12px;
}
QDialog QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0px 4px;
    color: #59615C;
}
QDialog QCheckBox,
QDialog QRadioButton {
    color: #202522;
}
QDialog QTableWidget,
QDialog QTextBrowser {
    background-color: #FFFEFB;
    border: 1px solid #DDD9D1;
    color: #202522;
    selection-background-color: #DDEBE3;
    selection-color: #202522;
}
QDialog QHeaderView::section {
    background-color: #F2F0EB;
    border: 0px;
    border-bottom: 1px solid #DDD9D1;
    color: #59615C;
    padding: 6px;
}
)STYLE";

void refreshButtonStyle(QAbstractButton* button)
{
    if (button == nullptr) {
        return;
    }
    if (button->style() != nullptr) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
    button->update();
}

void styleDialogButtons(QDialog* dialog)
{
    if (dialog == nullptr) {
        return;
    }
    const auto buttonBoxes = dialog->findChildren<QDialogButtonBox*>();
    for (QDialogButtonBox* box : buttonBoxes) {
        for (QAbstractButton* button : box->buttons()) {
            const QDialogButtonBox::StandardButton standardButton = box->standardButton(button);
            const bool primary = standardButton == QDialogButtonBox::Ok
                || standardButton == QDialogButtonBox::Save
                || standardButton == QDialogButtonBox::Apply
                || standardButton == QDialogButtonBox::Open;
            styleProductButton(button,
                               primary ? ProductButtonRole::Primary
                                       : ProductButtonRole::Secondary);
        }
    }
}

} // namespace

void applyProductDialogStyle(QDialog* dialog)
{
    if (dialog == nullptr) {
        return;
    }

    dialog->setStyleSheet(QString::fromUtf8(kProductDialogStyle));
    styleDialogButtons(dialog);
    QTimer::singleShot(0, dialog, [dialog] { styleDialogButtons(dialog); });
}

void styleProductButton(QAbstractButton* button, ProductButtonRole role)
{
    if (button == nullptr) {
        return;
    }
    button->setProperty("productRole", buttonRoleName(role));
    refreshButtonStyle(button);
}

bool confirmProductAction(QWidget* parent,
                          const QString& title,
                          const QString& message,
                          const QString& confirmText,
                          bool danger)
{
    QMessageBox box(QMessageBox::Question,
                    title,
                    message,
                    QMessageBox::Yes | QMessageBox::No,
                    parent);
    box.setIcon(QMessageBox::NoIcon);
    box.setDefaultButton(QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(confirmText);
    box.button(QMessageBox::No)->setText(QStringLiteral("取消"));
    applyProductDialogStyle(&box);
    styleProductButton(box.button(QMessageBox::Yes),
                       danger ? ProductButtonRole::Danger : ProductButtonRole::Primary);
    styleProductButton(box.button(QMessageBox::No), ProductButtonRole::Secondary);
    return box.exec() == QMessageBox::Yes;
}

void showProductWarning(QWidget* parent, const QString& title, const QString& message)
{
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    box.setIcon(QMessageBox::NoIcon);
    applyProductDialogStyle(&box);
    styleProductButton(box.button(QMessageBox::Ok), ProductButtonRole::Secondary);
    box.exec();
}

} // namespace vision3d
