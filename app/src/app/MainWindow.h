#pragma once

#include "backend/ColmapBackend.h"
#include "core/process/ProcessRunner.h"
#include "core/project/ProjectManager.h"
#include "core/reconstruction/ReconstructionController.h"

#include <QMainWindow>

class QLabel;
class QAction;

namespace vision3d {

class BackendPanel;
class ImageBrowserPanel;
class ImagePreviewWidget;
class LogPanel;
class ProjectPanel;
class ReconstructionPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void createProject();
    void openProject();
    void importImages();
    void removeSelectedImage();
    void openDeveloperSettings();
    void probeBackend();
    void onAssetSelected(const QString& assetId);
    void onProcessStarted();
    void onProcessStdout(const QString& text);
    void onProcessStderr(const QString& text);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessFailed(const QString& message);
    void startReconstruction();
    void cancelReconstruction();
    void onReconstructionLog(const QString& text, bool isError);
    void onReconstructionFinished(bool success);
    void refreshProjectView();

private:
    void createActions();
    void startBackendProbe(const QString& root, bool allowDevelopmentFallback);
    void finishBackendProbe(const BackendProbeResult& result);
    void updateAssetActions();
    void updateReconstructionView();
    void showProjectError(const QString& message);

    ProjectManager m_projectManager;
    ProcessRunner m_processRunner;
    ColmapBackend m_colmapBackend;
    ProjectPanel* m_projectPanel;
    ImageBrowserPanel* m_imageBrowserPanel;
    ImagePreviewWidget* m_imagePreviewWidget;
    BackendPanel* m_backendPanel;
    ReconstructionPanel* m_reconstructionPanel;
    LogPanel* m_logPanel;
    QLabel* m_statusLabel;
    ReconstructionController* m_reconstructionController;
    QAction* m_importImagesAction = nullptr;
    QAction* m_removeImageAction = nullptr;
    QString m_pendingBackendRoot;
    QString m_probeStdout;
    QString m_probeStderr;
    QString m_probeProcessError;
    QString m_internalBackendRoot;
    QString m_developmentFallbackRoot;
    BackendProbeResult m_internalProbeResult;
    bool m_probeFailed = false;
    bool m_probeUsingInternalRoot = false;
    bool m_probeFallbackAllowed = false;
};

} // namespace vision3d
