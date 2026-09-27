#pragma once

#include "backend/ColmapBackend.h"
#include "core/device/DeviceMarker.h"
#include "core/device/GaugeAsset.h"
#include "core/geometry/SurfaceHit.h"
#include "core/process/ProcessRunner.h"
#include "core/project/ProjectManager.h"
#include "core/reconstruction/ReconstructionController.h"

#include <QMainWindow>

class QLabel;
class QAction;
class QStackedWidget;
class QQuickWidget;
class QUrl;
class QWidget;

namespace vision3d {

class AppShellViewModel;
class BackendPanel;
class ImageBrowserPanel;
class ImagePreviewWidget;
class LogPanel;
class ModelViewerWidget;
class ProjectPanel;
class ReconstructionPanel;
namespace realtime {
class RealtimeMonitoringController;
}
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    bool openProjectPath(const QString& fileOrDirectory, QString* error = nullptr);

private slots:
    void createProject();
    void openProject();
    void closeProject();
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
    void open3DModel();
    void resetViewer();
    void onSurfacePicked(const SurfaceHit& hit);
    void onSurfaceMissed();
    void onMarkerSelected(const QString& markerId);
    void addDeviceMarker();
    void deleteDeviceMarker();
    void createGaugeAsset();
    void editGaugeAsset();
    void updateGaugeReading();
    void visualGaugeReading();
    void showGaugeHistory();
    void configureGaugeStatusRule();
    void deleteGaugeAsset();
    void startMockSensor();
    void stopMockSensor();
    void recordCurrentSensorSample();
    void refreshProjectView();

private:
    void createActions();
    void startBackendProbe(const QString& root, bool allowDevelopmentFallback);
    void finishBackendProbe(const BackendProbeResult& result);
    void updateAssetActions();
    void updateReconstructionView();
    void clearViewerAssociation();
    void clearPendingSurfaceHit();
    void refreshMarkerPresentation();
    void refreshSelectedMarkerDetails();
    void updateMarkerControls();
    void showImagePreview();
    void showProjectError(const QString& message);
    void createModernInterface();
    void configureQmlWidget(QQuickWidget* widget, const QUrl& source);
    void syncModernPage();
    void ensureModernSelection(const QString& markerId);

    ProjectManager m_projectManager;
    ProcessRunner m_processRunner;
    ColmapBackend m_colmapBackend;
    ProjectPanel* m_projectPanel;
    ImageBrowserPanel* m_imageBrowserPanel;
    ImagePreviewWidget* m_imagePreviewWidget;
    QStackedWidget* m_previewStack;
    ModelViewerWidget* m_modelViewerWidget;
    BackendPanel* m_backendPanel;
    ReconstructionPanel* m_reconstructionPanel;
    LogPanel* m_logPanel;
    QLabel* m_statusLabel;
    ReconstructionController* m_reconstructionController;
    realtime::RealtimeMonitoringController* m_realtimeMonitoringController;
    AppShellViewModel* m_appShellViewModel;
    QWidget* m_legacyWidgetSurface = nullptr;
    QWidget* m_modernRoot = nullptr;
    QWidget* m_sceneContainer = nullptr;
    QQuickWidget* m_topBarWidget = nullptr;
    QQuickWidget* m_navigationWidget = nullptr;
    QQuickWidget* m_pageWidget = nullptr;
    QQuickWidget* m_sceneToolbarWidget = nullptr;
    QQuickWidget* m_inspectorWidget = nullptr;
    QQuickWidget* m_statusBarWidget = nullptr;
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
    bool m_viewerMeshLoaded = false;
    QString m_viewerProjectDirectory;
    QString m_viewerTaskId;
    QString m_viewerMeshPath;
    QString m_viewerCanonicalMeshPath;
    bool m_hasPendingSurfaceHit = false;
    SurfaceHit m_pendingSurfaceHit;
    QString m_pendingSurfaceProjectId;
    QString m_pendingSurfaceTaskId;
    QString m_selectedMarkerId;
};

} // namespace vision3d
