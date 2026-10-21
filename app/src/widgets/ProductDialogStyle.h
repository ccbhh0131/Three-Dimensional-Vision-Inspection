#pragma once

#include <QString>

class QAbstractButton;
class QDialog;
class QWidget;

namespace vision3d {

enum class ProductButtonRole {
    Secondary,
    Primary,
    Danger,
};

void applyProductDialogStyle(QDialog* dialog);
void styleProductButton(QAbstractButton* button, ProductButtonRole role);

bool confirmProductAction(QWidget* parent,
                          const QString& title,
                          const QString& message,
                          const QString& confirmText = QStringLiteral("确定"),
                          bool danger = false);

void showProductWarning(QWidget* parent, const QString& title, const QString& message);

} // namespace vision3d
