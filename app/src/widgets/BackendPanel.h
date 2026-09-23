#pragma once

#include "backend/BackendProbeResult.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace vision3d {

class BackendPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit BackendPanel(QWidget* parent = nullptr);

    QString backendRoot() const;
    bool isVerified() const;
    bool isGpuAvailable() const;
    const BackendProbeResult& lastResult() const;
    void setBackendRoot(const QString& root);
    void setChecking();
    void setResult(const BackendProbeResult& result);
    void setProbeEnabled(bool enabled);

signals:
    void browseRequested();
    void probeRequested(const QString& root);
    void developerSettingsRequested();

private:
    QString m_backendRoot;
    QLabel* m_engineStatusLabel;
    QLabel* m_gpuStatusLabel;
    QLabel* m_statusLabel;
    QLabel* m_messageLabel;
    QPushButton* m_probeButton;
    QPushButton* m_developerSettingsButton;
    BackendProbeResult m_lastResult;
    bool m_gpuAvailable = false;
};

} // namespace vision3d
