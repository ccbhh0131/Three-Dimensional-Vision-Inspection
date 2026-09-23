#pragma once

#include "backend/BackendProbeResult.h"

#include <QDialog>

class QLineEdit;

namespace vision3d {

class DeveloperSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit DeveloperSettingsDialog(const BackendProbeResult& probeResult,
                                     const QString& internalRoot,
                                     const QString& developmentFallbackRoot,
                                     QWidget* parent = nullptr);

    QString developmentFallbackRoot() const;

private:
    void chooseFallbackRoot();

    QLineEdit* m_fallbackRootEdit;
};

} // namespace vision3d
