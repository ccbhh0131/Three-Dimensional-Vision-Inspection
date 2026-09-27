#pragma once

#include "core/device/GaugeAsset.h"

#include <QDialog>

#include <optional>

class QLabel;
class QComboBox;
class QLineEdit;

namespace vision3d {

class GaugeAssetDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GaugeAssetDialog(const QString& title,
                              const std::optional<GaugeAsset>& initial = std::nullopt,
                              QWidget* parent = nullptr);

    QString assetName() const;
    QString unit() const;
    double rangeMin() const;
    double rangeMax() const;
    QString gaugeProfileId() const;

private slots:
    void acceptInput();

private:
    bool readFiniteDouble(QLineEdit* field, double* value, const QString& label);

    QLineEdit* m_nameEdit;
    QLineEdit* m_rangeMinEdit;
    QLineEdit* m_rangeMaxEdit;
    QLineEdit* m_unitEdit;
    QComboBox* m_profileCombo;
    QLabel* m_errorLabel;
};

} // namespace vision3d
