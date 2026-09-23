#pragma once

#include "core/reconstruction/ReconstructionTask.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace vision3d {

class ReconstructionPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ReconstructionPanel(QWidget* parent = nullptr);

    QPushButton* startButton() const;
    QPushButton* cancelButton() const;
    void setProjectContext(bool hasProject, int imageCount);
    void setEngineContext(bool engineAvailable, bool gpuAvailable);
    // Kept as a source-compatible adapter for callers that still hold the
    // technical backend probe fields; the user-facing panel never renders them.
    void setBackendContext(const QString& backend,
                           const QString& version,
                           bool verified);
    void setTask(const ReconstructionTask& task);
    void setRunning(bool running);

signals:
    void startRequested();
    void cancelRequested();

private:
    void updateControls();

    QLabel* m_stateLabel;
    QLabel* m_engineLabel;
    QLabel* m_gpuLabel;
    QLabel* m_inputLabel;
    QLabel* m_stageLabel;
    QLabel* m_progressLabel;
    QLabel* m_registeredLabel;
    QLabel* m_jobLabel;
    QLabel* m_meshLabel;
    QPushButton* m_startButton;
    QPushButton* m_cancelButton;
    bool m_hasProject = false;
    int m_imageCount = 0;
    bool m_engineAvailable = false;
    bool m_gpuAvailable = false;
    bool m_running = false;
};

} // namespace vision3d
