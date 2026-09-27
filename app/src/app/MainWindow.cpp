#include "app/MainWindow.h"

#include "app/AppShellViewModel.h"
#include "widgets/BackendPanel.h"
#include "backend/ReconstructionEngineLocator.h"
#include "core/mesh/PlyMeshLoader.h"
#include "core/device/GaugeProfile.h"
#include "core/realtime/RealtimeMonitoringController.h"
#include "widgets/DeveloperSettingsDialog.h"
#include "widgets/GaugeAssetDialog.h"
#include "widgets/GaugeStatusRuleDialog.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/LogPanel.h"
#include "widgets/ModelViewerWidget.h"
#include "widgets/ProjectPanel.h"
#include "widgets/ReconstructionPanel.h"
#include "widgets/VisualGaugeReadingDialog.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenuBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickWidget>
#include <QSGRendererInterface>
#include <QUrl>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QToolBar>
#include <QElapsedTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <utility>

namespace vision3d {

namespace {

bool containsExplicitError(const QString& text)
{
    static const QRegularExpression severityPattern(
        QStringLiteral("\\b(FATAL|ERROR)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return severityPattern.match(text).hasMatch()
           || text.contains(QStringLiteral("进程错误"))
           || text.contains(QStringLiteral("失败"))
           || text.contains(QStringLiteral("未通过"))
           || text.contains(QStringLiteral("无法启动"))
           || text.contains(QStringLiteral("artifact 校验失败"));
}

QString productLogText(const QString& text)
{
    static const QRegularExpression technicalPattern(
        QStringLiteral("(?i)(colmap|database\\.db|sparse[/\\\\][0-9]+|dense[/\\\\])"));
    QStringList visibleLines;
    for (const QString& rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || technicalPattern.match(line).hasMatch()) {
            continue;
        }
        QString productLine = line;
        productLine.replace(QStringLiteral("Backend Probe"),
                            QStringLiteral("重建引擎检测"),
                            Qt::CaseInsensitive);
        visibleLines.append(productLine);
    }
    return visibleLines.join(QLatin1Char('\n'));
}

DeviceMarkerVisualState markerVisualStateForGaugeStatus(GaugeStatus status)
{
    switch (status) {
    case GaugeStatus::Unknown:
        return DeviceMarkerVisualState::Unknown;
    case GaugeStatus::Normal:
        return DeviceMarkerVisualState::Normal;
    case GaugeStatus::Warning:
        return DeviceMarkerVisualState::Warning;
    case GaugeStatus::Alarm:
        return DeviceMarkerVisualState::Alarm;
    }
    return DeviceMarkerVisualState::Unknown;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_projectManager(this)
    , m_processRunner(this)
    , m_projectPanel(new ProjectPanel(this))
    , m_imageBrowserPanel(new ImageBrowserPanel(this))
    , m_imagePreviewWidget(new ImagePreviewWidget(this))
    , m_previewStack(new QStackedWidget(this))
    , m_modelViewerWidget(new ModelViewerWidget(this))
    , m_backendPanel(new BackendPanel(this))
    , m_reconstructionPanel(new ReconstructionPanel(this))
    , m_logPanel(new LogPanel(this))
    , m_statusLabel(new QLabel(QStringLiteral("未打开项目"), this))
    , m_reconstructionController(new ReconstructionController(&m_projectManager,
                                                              &m_colmapBackend,
                                                              nullptr,
                                                              this))
    , m_realtimeMonitoringController(
          new realtime::RealtimeMonitoringController(&m_projectManager, this))
    , m_appShellViewModel(new AppShellViewModel(&m_projectManager,
                                                m_realtimeMonitoringController,
                                                this))
{
    setWindowTitle(QStringLiteral("三维视觉检测软件"));
    setStatusBar(new QStatusBar(this));
    statusBar()->addPermanentWidget(m_statusLabel, 1);

    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_projectPanel, 1);
    leftLayout->addWidget(m_imageBrowserPanel, 2);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_backendPanel, 1);
    rightLayout->addWidget(m_reconstructionPanel, 2);

    auto* topSplitter = new QSplitter(Qt::Horizontal, this);
    topSplitter->addWidget(leftPanel);
    m_previewStack->setObjectName(QStringLiteral("centralPreviewStack"));
    m_imagePreviewWidget->setObjectName(QStringLiteral("imagePreviewWidget"));
    m_modelViewerWidget->setObjectName(QStringLiteral("modelViewerWidget"));
    m_previewStack->addWidget(m_imagePreviewWidget);
    m_previewStack->addWidget(m_modelViewerWidget);
    m_previewStack->setCurrentWidget(m_imagePreviewWidget);
    topSplitter->addWidget(m_previewStack);
    topSplitter->addWidget(rightPanel);
    topSplitter->setStretchFactor(0, 1);
    topSplitter->setStretchFactor(1, 1);
    topSplitter->setStretchFactor(2, 2);
    topSplitter->setSizes({240, 560, 320});

    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("legacyWidgetSurface"));
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->addWidget(topSplitter, 2);
    centralLayout->addWidget(m_logPanel, 1);
    m_legacyWidgetSurface = centralWidget;
    setCentralWidget(centralWidget);

    createActions();
    menuBar()->hide();
    for (QToolBar* toolbar : findChildren<QToolBar*>()) {
        toolbar->hide();
    }
    statusBar()->hide();
    if (!qEnvironmentVariableIsSet("VISION3DINSPECTOR_DISABLE_QML_UI")) {
        createModernInterface();
    }

    m_internalBackendRoot = ReconstructionEngineLocator::internalEngineRoot();
    m_developmentFallbackRoot = ReconstructionEngineLocator::savedDevelopmentBackendRoot();
    m_backendPanel->setBackendRoot(ReconstructionEngineLocator::preferredRoot());

    connect(&m_projectManager,
            &ProjectManager::projectChanged,
            this,
            &MainWindow::refreshProjectView);
    connect(m_backendPanel,
            &BackendPanel::developerSettingsRequested,
            this,
            &MainWindow::openDeveloperSettings);
    connect(m_backendPanel,
            &BackendPanel::probeRequested,
            this,
            [this](const QString&) { probeBackend(); });
    connect(m_imageBrowserPanel,
            &ImageBrowserPanel::importRequested,
            this,
            &MainWindow::importImages);
    connect(m_imageBrowserPanel,
            &ImageBrowserPanel::removeRequested,
            this,
            [this](const QString&) { removeSelectedImage(); });
    connect(m_imageBrowserPanel,
            &ImageBrowserPanel::assetSelected,
            this,
            &MainWindow::onAssetSelected);
    connect(&m_processRunner, &ProcessRunner::started, this, &MainWindow::onProcessStarted);
    connect(&m_processRunner,
            &ProcessRunner::stdoutReady,
            this,
            &MainWindow::onProcessStdout);
    connect(&m_processRunner,
            &ProcessRunner::stderrReady,
            this,
            &MainWindow::onProcessStderr);
    connect(&m_processRunner,
            &ProcessRunner::finished,
            this,
            &MainWindow::onProcessFinished);
    connect(&m_processRunner,
            &ProcessRunner::failed,
            this,
            &MainWindow::onProcessFailed);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::startRequested,
            this,
            &MainWindow::startReconstruction);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::cancelRequested,
            this,
            &MainWindow::cancelReconstruction);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::viewModelRequested,
            this,
            &MainWindow::open3DModel);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::resetViewRequested,
            this,
            &MainWindow::resetViewer);
    connect(m_modelViewerWidget,
            &ModelViewerWidget::surfacePicked,
            this,
            &MainWindow::onSurfacePicked);
    connect(m_modelViewerWidget,
            &ModelViewerWidget::surfaceMissed,
            this,
            &MainWindow::onSurfaceMissed);
    connect(m_modelViewerWidget,
            &ModelViewerWidget::markerSelected,
            this,
            &MainWindow::onMarkerSelected);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::addMarkerRequested,
            this,
            &MainWindow::addDeviceMarker);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::deleteMarkerRequested,
            this,
            &MainWindow::deleteDeviceMarker);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::createGaugeRequested,
            this,
            &MainWindow::createGaugeAsset);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::editGaugeRequested,
            this,
            &MainWindow::editGaugeAsset);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::updateGaugeReadingRequested,
            this,
            &MainWindow::updateGaugeReading);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::visualGaugeReadingRequested,
            this,
            &MainWindow::visualGaugeReading);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::viewGaugeHistoryRequested,
            this,
            &MainWindow::showGaugeHistory);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::configureGaugeStatusRuleRequested,
            this,
            &MainWindow::configureGaugeStatusRule);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::deleteGaugeRequested,
            this,
            &MainWindow::deleteGaugeAsset);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::startMockSensorRequested,
            this,
            &MainWindow::startMockSensor);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::stopMockSensorRequested,
            this,
            &MainWindow::stopMockSensor);
    connect(m_reconstructionPanel,
            &ReconstructionPanel::recordCurrentSensorSampleRequested,
            this,
            &MainWindow::recordCurrentSensorSample);
    connect(m_reconstructionController,
            &ReconstructionController::taskChanged,
            this,
            &MainWindow::updateReconstructionView);
    connect(m_reconstructionController,
            &ReconstructionController::logMessage,
            this,
            &MainWindow::onReconstructionLog);
    connect(m_reconstructionController,
            &ReconstructionController::finished,
            this,
            &MainWindow::onReconstructionFinished);
    connect(m_realtimeMonitoringController,
            &realtime::RealtimeMonitoringController::liveStateChanged,
            this,
            [this](const QString&) { refreshMarkerPresentation(); });
    connect(m_realtimeMonitoringController,
            &realtime::RealtimeMonitoringController::monitoringStateChanged,
            this,
            [this](const QString&, realtime::GaugeDataSourceState) {
                refreshMarkerPresentation();
            });
    connect(m_realtimeMonitoringController,
            &realtime::RealtimeMonitoringController::errorOccurred,
            this,
            [this](const QString& error) {
                m_logPanel->appendError(QStringLiteral("实时监控错误: %1").arg(error));
            });

    m_logPanel->appendInfo(QStringLiteral("应用已启动。当前为 Stage 5A QML Hybrid。"));
    m_logPanel->appendInfo(QStringLiteral("当前已接入三维重建引擎。"));
    refreshProjectView();
    m_appShellViewModel->refresh();
    syncModernPage();
    if (!m_backendPanel->backendRoot().isEmpty()) {
        QTimer::singleShot(0, this, &MainWindow::probeBackend);
    }
}

void MainWindow::createActions()
{
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
    QAction* newProjectAction = fileMenu->addAction(QStringLiteral("新建项目"));
    QAction* openProjectAction = fileMenu->addAction(QStringLiteral("打开项目"));
    QAction* closeProjectAction = fileMenu->addAction(QStringLiteral("关闭项目"));
    m_importImagesAction = fileMenu->addAction(QStringLiteral("导入图像"));
    m_removeImageAction = fileMenu->addAction(QStringLiteral("从项目中删除图像"));
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(QStringLiteral("退出"));

    auto* toolbar = addToolBar(QStringLiteral("工具"));
    toolbar->setMovable(false);
    toolbar->addAction(newProjectAction);
    toolbar->addAction(openProjectAction);
    toolbar->addAction(m_importImagesAction);
    toolbar->addAction(m_removeImageAction);
    QAction* probeAction = toolbar->addAction(QStringLiteral("检测重建引擎"));

    connect(newProjectAction, &QAction::triggered, this, &MainWindow::createProject);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    connect(closeProjectAction, &QAction::triggered, this, &MainWindow::closeProject);
    connect(m_importImagesAction, &QAction::triggered, this, &MainWindow::importImages);
    connect(m_removeImageAction, &QAction::triggered, this, &MainWindow::removeSelectedImage);
    connect(probeAction, &QAction::triggered, this, &MainWindow::probeBackend);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::configureQmlWidget(QQuickWidget* widget, const QUrl& source)
{
    if (widget == nullptr) {
        return;
    }
    widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    widget->setClearColor(QColor(QStringLiteral("#F6F5F2")));
    widget->rootContext()->setContextProperty(QStringLiteral("appShellViewModel"),
                                              m_appShellViewModel);
    widget->setSource(source);
    // Keep the presentation adapter explicit on the loaded root object as
    // well.  This makes the boundary robust for QML files loaded from the
    // ordinary QRC resource tree and avoids relying on an imported component
    // to re-resolve the context property after creation.
    if (widget->rootObject() != nullptr) {
        widget->rootObject()->setProperty(
            "viewModel", QVariant::fromValue(static_cast<QObject*>(m_appShellViewModel)));
    }
}

void MainWindow::createModernInterface()
{
    // The legacy viewer is a QOpenGLWidget.  Qt Quick may otherwise select
    // D3D11 on Windows, which cannot be composed with that OpenGL surface.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    auto* modernRoot = new QWidget(this);
    modernRoot->setObjectName(QStringLiteral("stage5aModernShell"));
    modernRoot->setMinimumSize(980, 620);
    auto* rootLayout = new QVBoxLayout(modernRoot);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_topBarWidget = new QQuickWidget(modernRoot);
    m_topBarWidget->setObjectName(QStringLiteral("qmlTopBar"));
    m_topBarWidget->setMinimumHeight(60);
    m_topBarWidget->setMaximumHeight(60);
    configureQmlWidget(m_topBarWidget, QUrl(QStringLiteral("qrc:/stage5a/TopBar.qml")));
    rootLayout->addWidget(m_topBarWidget);

    auto* workspaceRow = new QWidget(modernRoot);
    workspaceRow->setObjectName(QStringLiteral("stage5aWorkspaceRow"));
    auto* workspaceLayout = new QHBoxLayout(workspaceRow);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    m_navigationWidget = new QQuickWidget(workspaceRow);
    m_navigationWidget->setObjectName(QStringLiteral("qmlNavigationRail"));
    m_navigationWidget->setMinimumWidth(188);
    m_navigationWidget->setMaximumWidth(204);
    configureQmlWidget(m_navigationWidget,
                       QUrl(QStringLiteral("qrc:/stage5a/components/NavigationRail.qml")));
    workspaceLayout->addWidget(m_navigationWidget);

    auto* centerWorkspace = new QWidget(workspaceRow);
    centerWorkspace->setObjectName(QStringLiteral("stage5aCenterWorkspace"));
    auto* centerLayout = new QVBoxLayout(centerWorkspace);
    centerLayout->setContentsMargins(16, 14, 16, 14);
    centerLayout->setSpacing(10);

    m_sceneContainer = new QWidget(centerWorkspace);
    m_sceneContainer->setObjectName(QStringLiteral("stage5aSceneContainer"));
    auto* sceneLayout = new QVBoxLayout(m_sceneContainer);
    sceneLayout->setContentsMargins(0, 0, 0, 0);
    sceneLayout->setSpacing(8);
    m_sceneToolbarWidget = new QQuickWidget(m_sceneContainer);
    m_sceneToolbarWidget->setObjectName(QStringLiteral("qmlSceneToolbar"));
    m_sceneToolbarWidget->setMinimumHeight(44);
    m_sceneToolbarWidget->setMaximumHeight(44);
    configureQmlWidget(m_sceneToolbarWidget,
                       QUrl(QStringLiteral("qrc:/stage5a/SceneToolbar.qml")));
    sceneLayout->addWidget(m_sceneToolbarWidget);
    m_previewStack->setParent(m_sceneContainer);
    m_previewStack->setObjectName(QStringLiteral("centralPreviewStack"));
    sceneLayout->addWidget(m_previewStack, 1);
    centerLayout->addWidget(m_sceneContainer, 1);

    m_pageWidget = new QQuickWidget(centerWorkspace);
    m_pageWidget->setObjectName(QStringLiteral("qmlPageHost"));
    configureQmlWidget(m_pageWidget, QUrl(QStringLiteral("qrc:/stage5a/PageHost.qml")));
    centerLayout->addWidget(m_pageWidget, 1);
    workspaceLayout->addWidget(centerWorkspace, 1);

    m_inspectorWidget = new QQuickWidget(workspaceRow);
    m_inspectorWidget->setObjectName(QStringLiteral("qmlInspector"));
    m_inspectorWidget->setMinimumWidth(344);
    m_inspectorWidget->setMaximumWidth(380);
    configureQmlWidget(m_inspectorWidget, QUrl(QStringLiteral("qrc:/stage5a/Inspector.qml")));
    workspaceLayout->addWidget(m_inspectorWidget);
    rootLayout->addWidget(workspaceRow, 1);

    m_statusBarWidget = new QQuickWidget(modernRoot);
    m_statusBarWidget->setObjectName(QStringLiteral("qmlStatusBar"));
    m_statusBarWidget->setMinimumHeight(30);
    m_statusBarWidget->setMaximumHeight(30);
    configureQmlWidget(m_statusBarWidget, QUrl(QStringLiteral("qrc:/stage5a/StatusBar.qml")));
    rootLayout->addWidget(m_statusBarWidget);

    // QMainWindow owns its central widget.  Detach the legacy surface before
    // replacing it so the existing C++ panels remain alive for project,
    // reconstruction, gauge, and realtime flows routed from the shell.
    QWidget* legacySurface = takeCentralWidget();
    if (legacySurface != nullptr) {
        legacySurface->setParent(this);
    }
    m_modernRoot = modernRoot;
    setCentralWidget(m_modernRoot);
    if (m_legacyWidgetSurface != nullptr) {
        m_legacyWidgetSurface->hide();
    }

    connect(m_appShellViewModel,
            &AppShellViewModel::currentPageChanged,
            this,
            &MainWindow::syncModernPage);
    connect(m_appShellViewModel,
            &AppShellViewModel::markerSelectionRequested,
            this,
            &MainWindow::ensureModernSelection);
    connect(m_appShellViewModel,
            &AppShellViewModel::createProjectRequested,
            this,
            &MainWindow::createProject);
    connect(m_appShellViewModel,
            &AppShellViewModel::openProjectRequested,
            this,
            &MainWindow::openProject);
    connect(m_appShellViewModel,
            &AppShellViewModel::importImagesRequested,
            this,
            &MainWindow::importImages);
    connect(m_appShellViewModel,
            &AppShellViewModel::openViewerRequested,
            this,
            &MainWindow::open3DModel);
    connect(m_appShellViewModel,
            &AppShellViewModel::resetViewerRequested,
            this,
            &MainWindow::resetViewer);
    connect(m_appShellViewModel,
            &AppShellViewModel::visualReadingRequested,
            this,
            &MainWindow::visualGaugeReading);
    connect(m_appShellViewModel,
            &AppShellViewModel::manualReadingRequested,
            this,
            &MainWindow::updateGaugeReading);
    connect(m_appShellViewModel,
            &AppShellViewModel::showHistoryRequested,
            this,
            &MainWindow::showGaugeHistory);
    connect(m_appShellViewModel,
            &AppShellViewModel::createGaugeRequested,
            this,
            &MainWindow::createGaugeAsset);
    connect(m_appShellViewModel,
            &AppShellViewModel::editGaugeRequested,
            this,
            &MainWindow::editGaugeAsset);
    connect(m_appShellViewModel,
            &AppShellViewModel::configureRuleRequested,
            this,
            &MainWindow::configureGaugeStatusRule);
    connect(m_appShellViewModel,
            &AppShellViewModel::startRealtimeRequested,
            this,
            &MainWindow::startMockSensor);
    connect(m_appShellViewModel,
            &AppShellViewModel::stopRealtimeRequested,
            this,
            &MainWindow::stopMockSensor);
    connect(m_appShellViewModel,
            &AppShellViewModel::recordRealtimeRequested,
            this,
            &MainWindow::recordCurrentSensorSample);
    connect(m_appShellViewModel,
            &AppShellViewModel::settingsRequested,
            this,
            &MainWindow::openDeveloperSettings);
    syncModernPage();
}

void MainWindow::syncModernPage()
{
    if (m_appShellViewModel == nullptr || m_sceneContainer == nullptr
        || m_pageWidget == nullptr) {
        return;
    }
    const bool scenePage = m_appShellViewModel->currentPage() == QStringLiteral("scene");
    m_sceneContainer->setVisible(scenePage);
    m_pageWidget->setVisible(!scenePage);
    if (m_sceneToolbarWidget != nullptr) {
        m_sceneToolbarWidget->setVisible(scenePage);
    }
}

void MainWindow::ensureModernSelection(const QString& markerId)
{
    const QString normalized = markerId.trimmed();
    if (normalized.isEmpty() || !m_projectManager.deviceMarkerById(normalized).has_value()) {
        return;
    }
    m_selectedMarkerId = normalized;
    if (m_modelViewerWidget != nullptr && m_viewerMeshLoaded) {
        m_modelViewerWidget->setSelectedMarker(normalized);
        refreshSelectedMarkerDetails();
        updateMarkerControls();
    }
}

void MainWindow::createProject()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("新建项目"),
                                               QStringLiteral("项目名称:"),
                                               QLineEdit::Normal,
                                               QString(),
                                               &accepted)
                            .trimmed();
    if (!accepted) {
        return;
    }

    const QString parentDirectory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择项目保存目录"), QDir::homePath());
    if (parentDirectory.isEmpty()) {
        return;
    }

    QString error;
    if (!m_projectManager.newProject(parentDirectory, name, &error)) {
        showProjectError(error);
        return;
    }

    m_logPanel->appendInfo(QStringLiteral("新建项目成功: %1").arg(m_projectManager.projectDirectory()));
}

void MainWindow::openProject()
{
    const QString manifestPath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开项目"),
        QDir::homePath(),
        QStringLiteral("Project Manifest (project.json)"));
    if (manifestPath.isEmpty()) {
        return;
    }

    QString error;
    if (!openProjectPath(manifestPath, &error)) {
        showProjectError(error);
        return;
    }

    m_logPanel->appendInfo(QStringLiteral("打开项目成功: %1").arg(m_projectManager.projectDirectory()));
}

bool MainWindow::openProjectPath(const QString& fileOrDirectory, QString* error)
{
    if (m_reconstructionController != nullptr && m_reconstructionController->isRunning()) {
        if (error != nullptr) {
            *error = QStringLiteral("当前正在进行三维重建，不能切换项目。");
        }
        return false;
    }
    if (m_realtimeMonitoringController != nullptr) {
        m_realtimeMonitoringController->stop();
    }
    return m_projectManager.openProject(fileOrDirectory, error);
}

void MainWindow::closeProject()
{
    if (m_reconstructionController != nullptr && m_reconstructionController->isRunning()) {
        m_logPanel->appendError(QStringLiteral("当前正在进行三维重建，不能关闭项目。"));
        return;
    }
    if (m_realtimeMonitoringController != nullptr) {
        m_realtimeMonitoringController->stop();
    }
    m_projectManager.closeProject();
    m_logPanel->appendInfo(QStringLiteral("项目已关闭。"));
}

void MainWindow::importImages()
{
    if (!m_projectManager.hasProject()) {
        return;
    }

    QStringList suffixes;
    for (const QByteArray& format : QImageReader::supportedImageFormats()) {
        suffixes.append(QStringLiteral("*.") + QString::fromLatin1(format).toLower());
    }
    suffixes.removeDuplicates();
    const QString filter = suffixes.isEmpty()
                               ? QStringLiteral("图像文件")
                               : QStringLiteral("图像文件 (%1)").arg(suffixes.join(QLatin1Char(' ')));
    const QStringList sourceFiles = QFileDialog::getOpenFileNames(
        this, QStringLiteral("导入图像"), QDir::homePath(), filter);
    if (sourceFiles.isEmpty()) {
        return;
    }

    QElapsedTimer timer;
    timer.start();
    QString error;
    const QList<AssetImportResult> results = m_projectManager.importImages(sourceFiles, &error);
    int imported = 0;
    int duplicates = 0;
    int failed = 0;
    for (const AssetImportResult& result : results) {
        switch (result.status) {
        case AssetImportStatus::Imported:
            ++imported;
            m_logPanel->appendInfo(result.message);
            break;
        case AssetImportStatus::Duplicate:
            ++duplicates;
            m_logPanel->appendInfo(result.message);
            break;
        case AssetImportStatus::Failed:
            ++failed;
            m_logPanel->appendError(QStringLiteral("图像导入失败 [%1]: %2")
                                         .arg(result.sourcePath, result.message));
            break;
        }
    }
    if (!error.isEmpty()) {
        m_logPanel->appendError(error);
    }
    m_logPanel->appendInfo(QStringLiteral("导入完成: imported=%1, duplicates=%2, failed=%3, elapsed=%4 ms")
                               .arg(imported)
                               .arg(duplicates)
                               .arg(failed)
                               .arg(timer.elapsed()));
}

void MainWindow::removeSelectedImage()
{
    const QString assetId = m_imageBrowserPanel->selectedAssetId();
    const std::optional<AssetRecord> asset = m_projectManager.assetById(assetId);
    if (!asset.has_value()) {
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("从项目中删除"),
        QStringLiteral("确认从项目中删除图像资产“%1”？\n原始外部文件不会被删除。")
            .arg(asset->originalFileName));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_projectManager.removeAsset(assetId, &error)) {
        showProjectError(error);
        return;
    }
    m_logPanel->appendInfo(QStringLiteral("已从项目中删除图像资产: %1")
                               .arg(asset->originalFileName));
}

void MainWindow::openDeveloperSettings()
{
    // Show the currently active technical result in the developer-only view.
    // On this development machine that is the saved fallback; when an internal
    // runtime is selected, the panel result is the internal probe result.
    BackendProbeResult technical = m_backendPanel->lastResult();
    if (technical.rootPath.isEmpty()) {
        technical = m_internalProbeResult;
    }
    if (technical.backendName.isEmpty()) {
        technical.backendName = m_colmapBackend.backendName();
    }
    if (technical.rootPath.isEmpty()) {
        technical.rootPath = m_internalBackendRoot;
    }
    if (technical.executablePath.isEmpty() && !technical.rootPath.isEmpty()) {
        technical.executablePath = QDir(technical.rootPath).filePath(
            QStringLiteral("bin/colmap.exe"));
    }

    DeveloperSettingsDialog dialog(technical,
                                   m_internalBackendRoot,
                                   m_developmentFallbackRoot,
                                   this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_developmentFallbackRoot = dialog.developmentFallbackRoot();
    ReconstructionEngineLocator::saveDevelopmentBackendRoot(m_developmentFallbackRoot);
    m_backendPanel->setBackendRoot(m_developmentFallbackRoot);
    m_logPanel->appendInfo(QStringLiteral("开发引擎配置已保存。"));
    updateReconstructionView();
    if (!m_developmentFallbackRoot.isEmpty()) {
        startBackendProbe(m_developmentFallbackRoot, false);
    }
}

void MainWindow::probeBackend()
{
    if (m_processRunner.isRunning()) {
        m_logPanel->appendError(QStringLiteral("三维重建引擎检测正在进行。"));
        return;
    }

    QString root = m_backendPanel->backendRoot();
    if (root.isEmpty()) {
        root = ReconstructionEngineLocator::preferredRoot();
        m_backendPanel->setBackendRoot(root);
    }
    if (root.isEmpty()) {
        BackendProbeResult result;
        m_backendPanel->setResult(result);
        m_logPanel->appendError(QStringLiteral("未发现可用的三维重建引擎。"));
        updateReconstructionView();
        return;
    }
    startBackendProbe(root, true);
}

void MainWindow::startBackendProbe(const QString& root, bool allowDevelopmentFallback)
{
    if (m_processRunner.isRunning()) {
        return;
    }

    m_pendingBackendRoot = QDir(root).absolutePath();
    m_probeUsingInternalRoot = ReconstructionEngineLocator::isInternalRoot(
        m_pendingBackendRoot);
    m_probeFallbackAllowed = allowDevelopmentFallback && m_probeUsingInternalRoot
                             && !m_developmentFallbackRoot.isEmpty()
                             && !ReconstructionEngineLocator::isInternalRoot(
                                 m_developmentFallbackRoot);
    m_probeStdout.clear();
    m_probeStderr.clear();
    m_probeProcessError.clear();
    m_probeFailed = false;

    const BackendProbeResult validation = m_colmapBackend.validateRoot(m_pendingBackendRoot);
    if (!validation.message.startsWith(QStringLiteral("COLMAP 根目录结构有效"))) {
        finishBackendProbe(validation);
        return;
    }

    m_backendPanel->setChecking();
    m_backendPanel->setProbeEnabled(false);
    m_logPanel->appendInfo(QStringLiteral("正在检测三维重建引擎。"));

    QString error;
    if (!m_processRunner.start(ColmapBackend::program(),
                               ColmapBackend::probeArguments(),
                               m_pendingBackendRoot,
                               &error)) {
        m_probeFailed = true;
        m_probeProcessError = error;
        onProcessFailed(error);
    }
}

void MainWindow::finishBackendProbe(const BackendProbeResult& result)
{
    const bool verified = result.available && result.version == QStringLiteral("3.11.1");
    if (m_probeUsingInternalRoot) {
        m_internalProbeResult = result;
    }

    m_backendPanel->setResult(result);
    m_backendPanel->setProbeEnabled(true);
    m_logPanel->appendBackend(verified ? QStringLiteral("三维重建引擎已就绪。")
                                      : QStringLiteral("三维重建引擎不可用。"));

    if (!verified && m_probeUsingInternalRoot && m_probeFallbackAllowed) {
        const QString fallback = m_developmentFallbackRoot;
        m_probeFallbackAllowed = false;
        m_probeUsingInternalRoot = false;
        m_backendPanel->setBackendRoot(fallback);
        m_logPanel->appendInfo(QStringLiteral("内置引擎未就绪，正在尝试开发回退配置。"));
        updateReconstructionView();
        QTimer::singleShot(0, this, [this, fallback] { startBackendProbe(fallback, false); });
        return;
    }

    if (verified && !ReconstructionEngineLocator::isInternalRoot(result.rootPath)) {
        m_developmentFallbackRoot = result.rootPath;
        ReconstructionEngineLocator::saveDevelopmentBackendRoot(m_developmentFallbackRoot);
    }
    updateReconstructionView();
}

void MainWindow::onAssetSelected(const QString& assetId)
{
    clearPendingSurfaceHit();
    showImagePreview();
    if (assetId.isEmpty() || !m_projectManager.hasProject()) {
        m_imagePreviewWidget->clearPreview();
        updateAssetActions();
        return;
    }

    const std::optional<AssetRecord> asset = m_projectManager.assetById(assetId);
    if (!asset.has_value()) {
        m_imagePreviewWidget->clearPreview();
        updateAssetActions();
        return;
    }
    m_imagePreviewWidget->showAsset(*asset, m_projectManager.projectDirectory());
    updateAssetActions();
}

void MainWindow::onProcessStarted()
{
    m_logPanel->appendInfo(QStringLiteral("重建引擎检测已启动。"));
}

void MainWindow::onProcessStdout(const QString& text)
{
    m_probeStdout.append(text);
}

void MainWindow::onProcessStderr(const QString& text)
{
    m_probeStderr.append(text);
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_probeFailed) {
        m_backendPanel->setProbeEnabled(true);
        return;
    }

    const BackendProbeResult result = m_colmapBackend.parseProbeResult(
        m_pendingBackendRoot,
        exitCode,
        exitStatus,
        m_probeStdout,
        m_probeStderr,
        m_probeProcessError);
    finishBackendProbe(result);
}

void MainWindow::onProcessFailed(const QString& message)
{
    m_probeFailed = true;
    m_probeProcessError = message;
    BackendProbeResult result = m_colmapBackend.validateRoot(m_pendingBackendRoot);
    result.message = QStringLiteral("COLMAP Probe 进程错误: %1").arg(message);
    finishBackendProbe(result);
}

void MainWindow::startReconstruction()
{
    QString error;
    if (!m_reconstructionController->start(m_backendPanel->backendRoot(), &error)) {
        m_logPanel->appendError(QStringLiteral("无法开始三维重建，请先检查重建引擎配置。"));
        updateReconstructionView();
        return;
    }
    m_logPanel->appendInfo(QStringLiteral("已创建三维重建任务: %1")
                               .arg(m_reconstructionController->currentTask().taskId));
    updateReconstructionView();
}

void MainWindow::cancelReconstruction()
{
    m_reconstructionController->cancel();
    updateReconstructionView();
}

void MainWindow::onReconstructionLog(const QString& text, bool isError)
{
    Q_UNUSED(isError)
    const QString visibleText = productLogText(text);
    if (visibleText.isEmpty()) {
        return;
    }
    if (containsExplicitError(text)) {
        m_logPanel->appendError(visibleText);
    } else {
        m_logPanel->appendBackend(visibleText);
    }
}

void MainWindow::onReconstructionFinished(bool success)
{
    if (success) {
        m_logPanel->appendInfo(QStringLiteral("三维重建完成，结果已保存。"));
    } else {
        m_logPanel->appendError(QStringLiteral("三维重建未完成，请查看开发设置或技术日志。"));
    }
    updateReconstructionView();
}

void MainWindow::refreshProjectView()
{
    const std::optional<ProjectManifest>& manifest = m_projectManager.currentManifest();
    if (!manifest.has_value()) {
        clearViewerAssociation();
        m_projectPanel->clearProject();
        m_imageBrowserPanel->clearProject();
        m_imagePreviewWidget->clearPreview();
        m_statusLabel->setText(QStringLiteral("未打开项目"));
        updateAssetActions();
        updateReconstructionView();
        if (m_appShellViewModel != nullptr) {
            m_appShellViewModel->refresh();
        }
        return;
    }

    const ReconstructionMeshArtifact artifact =
        m_projectManager.latestPoissonMeshArtifact();
    const QString projectDirectory = QDir(m_projectManager.projectDirectory()).absolutePath();
    const bool viewerIdentityChanged =
        m_viewerMeshLoaded
        && (m_reconstructionController->isRunning()
            || m_viewerProjectDirectory != projectDirectory
            || m_viewerTaskId != artifact.taskId
            || !artifact.isValid()
            || m_viewerCanonicalMeshPath != artifact.canonicalPath);
    if (viewerIdentityChanged) {
        clearViewerAssociation();
    }

    m_projectPanel->setProject(*manifest, m_projectManager.projectDirectory());
    m_imageBrowserPanel->setProject(*manifest, m_projectManager.projectDirectory());
    m_imagePreviewWidget->clearPreview();
    m_statusLabel->setText(manifest->name());
    updateAssetActions();
    updateReconstructionView();
    refreshSelectedMarkerDetails();
    if (m_appShellViewModel != nullptr) {
        m_appShellViewModel->refresh();
    }
}

void MainWindow::updateAssetActions()
{
    const bool hasProject = m_projectManager.hasProject();
    const bool hasSelection = hasProject && !m_imageBrowserPanel->selectedAssetId().isEmpty();
    const bool reconstructionRunning = m_reconstructionController != nullptr
                                      && m_reconstructionController->isRunning();
    if (m_importImagesAction != nullptr) {
        m_importImagesAction->setEnabled(hasProject && !reconstructionRunning);
    }
    if (m_removeImageAction != nullptr) {
        m_removeImageAction->setEnabled(hasSelection && !reconstructionRunning);
    }
    m_imageBrowserPanel->setOperationsEnabled(!reconstructionRunning);
}

void MainWindow::updateReconstructionView()
{
    if (m_reconstructionPanel == nullptr) {
        return;
    }
    if (!m_projectManager.hasProject()) {
        m_reconstructionPanel->setProjectContext(false, 0);
        m_reconstructionPanel->setTask(ReconstructionTask());
        m_reconstructionPanel->setMeshAvailable(false);
    } else {
        QString assetError;
        const int imageCount = m_projectManager.imageAssetRecords(&assetError).size();
        m_reconstructionPanel->setProjectContext(true, imageCount);
        ReconstructionTask task = m_reconstructionController->currentTask();
        if (task.taskId.isEmpty() || !m_reconstructionController->isRunning()) {
            QString taskError;
            const std::optional<ReconstructionTask> latest =
                m_projectManager.latestReconstructionTask(&taskError);
            if (latest.has_value()) {
                task = *latest;
            }
        }
        m_reconstructionPanel->setTask(task);
        m_reconstructionPanel->setMeshAvailable(
            m_projectManager.latestPoissonMeshArtifact().isValid());
    }
    m_reconstructionPanel->setEngineContext(m_backendPanel->isVerified(),
                                            m_backendPanel->isGpuAvailable());
    m_reconstructionPanel->setRunning(m_reconstructionController->isRunning());
    updateAssetActions();
}

void MainWindow::open3DModel()
{
    if (!m_projectManager.hasProject()) {
        m_logPanel->appendError(QStringLiteral(
            "无法查看三维模型：No Active Project。请先打开项目。"));
        m_reconstructionPanel->setMeshAvailable(false);
        showImagePreview();
        return;
    }

    if (m_appShellViewModel != nullptr) {
        m_appShellViewModel->selectPage(QStringLiteral("scene"));
    }

    const ReconstructionMeshArtifact artifact =
        m_projectManager.latestPoissonMeshArtifact();
    if (!artifact.isValid()) {
        m_logPanel->appendError(artifact.message);
        m_reconstructionPanel->setMeshAvailable(false);
        if (m_viewerMeshLoaded) {
            clearViewerAssociation();
        } else {
            showImagePreview();
        }
        return;
    }

    const QString projectDirectory = QDir(m_projectManager.projectDirectory()).absolutePath();
    if (m_viewerMeshLoaded
        && m_viewerProjectDirectory == projectDirectory
        && m_viewerTaskId == artifact.taskId
        && m_viewerCanonicalMeshPath == artifact.canonicalPath
        && m_modelViewerWidget->hasMesh()) {
        m_previewStack->setCurrentWidget(m_modelViewerWidget);
        m_reconstructionPanel->setViewerLoaded(true);
        refreshMarkerPresentation();
        m_logPanel->appendInfo(QStringLiteral("已切换到已加载的三维模型。"));
        return;
    }

    clearViewerAssociation();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    MeshLoadResult loadResult = PlyMeshLoader::load(artifact.path);
    QApplication::restoreOverrideCursor();
    if (!loadResult.success) {
        m_logPanel->appendError(QStringLiteral("三维模型加载失败：%1")
                                    .arg(meshLoadErrorToString(loadResult.errorCode)));
        m_logPanel->appendProcess(QStringLiteral("mesh path=%1; detail=%2")
                                      .arg(artifact.path, loadResult.message));
        m_reconstructionPanel->setMeshAvailable(true);
        showImagePreview();
        return;
    }

    m_modelViewerWidget->setMesh(std::move(loadResult.mesh));
    if (!m_modelViewerWidget->hasMesh()) {
        m_logPanel->appendError(QStringLiteral("OpenGL / Viewer Error：MeshData 未能进入 Viewer。"));
        m_logPanel->appendProcess(QStringLiteral("mesh path=%1; detail=%2")
                                      .arg(artifact.path,
                                           m_modelViewerWidget->rendererStatus().lastError));
        m_reconstructionPanel->setMeshAvailable(true);
        showImagePreview();
        return;
    }

    m_viewerMeshLoaded = true;
    m_viewerProjectDirectory = projectDirectory;
    m_viewerTaskId = artifact.taskId;
    m_viewerMeshPath = artifact.path;
    m_viewerCanonicalMeshPath = artifact.canonicalPath;
    refreshMarkerPresentation();
    m_previewStack->setCurrentWidget(m_modelViewerWidget);
    m_reconstructionPanel->setViewerLoaded(true);
    m_logPanel->appendInfo(QStringLiteral("已加载三维模型并完成 Fit To View。"));
    m_logPanel->appendProcess(QStringLiteral("mesh path=%1; cpu load=%2 ms; normal generation=%3 ms")
                                  .arg(artifact.path)
                                  .arg(loadResult.metrics.loadMilliseconds, 0, 'f', 2)
                                  .arg(loadResult.metrics.normalGenerationMilliseconds, 0, 'f', 2));
}

void MainWindow::resetViewer()
{
    if (!m_viewerMeshLoaded || !m_modelViewerWidget->hasMesh()) {
        m_logPanel->appendError(QStringLiteral("无法重置三维视图：当前没有已加载的模型。"));
        return;
    }
    m_modelViewerWidget->resetView();
    m_previewStack->setCurrentWidget(m_modelViewerWidget);
    m_logPanel->appendInfo(QStringLiteral("三维视图已重置。"));
}

void MainWindow::onSurfacePicked(const SurfaceHit& hit)
{
    if (!hit.isValid()) {
        onSurfaceMissed();
        return;
    }

    m_pendingSurfaceHit = hit;
    m_hasPendingSurfaceHit = true;
    m_pendingSurfaceProjectId = m_projectManager.currentManifest().has_value()
        ? m_projectManager.currentManifest()->projectId()
        : QString();
    m_pendingSurfaceTaskId = m_viewerTaskId;
    updateMarkerControls();

    m_logPanel->appendInfo(
        QStringLiteral("Picked triangle: %1 | World: (%2, %3, %4) | Distance: %5 | Barycentric: (%6, %7, %8)")
            .arg(static_cast<qulonglong>(hit.triangleIndex))
            .arg(hit.worldPosition.x(), 0, 'f', 6)
            .arg(hit.worldPosition.y(), 0, 'f', 6)
            .arg(hit.worldPosition.z(), 0, 'f', 6)
            .arg(hit.distance, 0, 'f', 6)
            .arg(hit.barycentric.x(), 0, 'f', 6)
            .arg(hit.barycentric.y(), 0, 'f', 6)
            .arg(hit.barycentric.z(), 0, 'f', 6));
}

void MainWindow::onSurfaceMissed()
{
    clearPendingSurfaceHit();
    m_logPanel->appendInfo(QStringLiteral("未命中表面。"));
}

void MainWindow::onMarkerSelected(const QString& markerId)
{
    clearPendingSurfaceHit();
    m_selectedMarkerId = markerId.trimmed();
    if (m_selectedMarkerId.isEmpty()) {
        m_reconstructionPanel->clearSelectedMarkerDetails();
        updateMarkerControls();
        if (m_appShellViewModel != nullptr) {
            m_appShellViewModel->setSelectedMarkerId(QString());
        }
        return;
    }

    const std::optional<DeviceMarker> marker =
        m_projectManager.deviceMarkerById(m_selectedMarkerId);
    if (!marker.has_value() || marker->reconstructionTaskId != m_viewerTaskId) {
        m_selectedMarkerId.clear();
        m_modelViewerWidget->setSelectedMarker(QString());
        m_reconstructionPanel->clearSelectedMarkerDetails();
        updateMarkerControls();
        return;
    }
    refreshSelectedMarkerDetails();
    if (m_appShellViewModel != nullptr) {
        m_appShellViewModel->setSelectedMarkerId(m_selectedMarkerId);
    }
    m_logPanel->appendInfo(QStringLiteral("已选择设备标记: %1 (%2)")
                               .arg(marker->name, marker->id));
    updateMarkerControls();
}

void MainWindow::refreshSelectedMarkerDetails()
{
    if (m_selectedMarkerId.isEmpty()) {
        m_reconstructionPanel->clearSelectedMarkerDetails();
        updateMarkerControls();
        return;
    }
    const std::optional<DeviceMarker> marker =
        m_projectManager.deviceMarkerById(m_selectedMarkerId);
    if (!marker.has_value() || marker->reconstructionTaskId != m_viewerTaskId) {
        m_selectedMarkerId.clear();
        m_modelViewerWidget->setSelectedMarker(QString());
        m_reconstructionPanel->clearSelectedMarkerDetails();
        updateMarkerControls();
        return;
    }
    m_reconstructionPanel->setSelectedMarkerDetails(marker->id,
                                                     marker->name,
                                                     marker->worldPosition);
    const std::optional<GaugeAsset> gauge =
        m_projectManager.gaugeAssetForMarker(marker->id);
    m_reconstructionPanel->setSelectedGaugeHistoryCount(
        gauge.has_value() ? m_projectManager.inspectionRecordsForGauge(gauge->id).size() : 0);
    const GaugeStatus status = gauge.has_value()
        ? GaugeStatusEvaluator::evaluate(gauge->latestValue, gauge->statusRule)
        : GaugeStatus::Unknown;
    const std::optional<realtime::GaugeLiveState> liveState = gauge.has_value()
        ? m_realtimeMonitoringController->liveStateForGauge(gauge->id)
        : std::nullopt;
    const GaugeStatus currentStatus = gauge.has_value() && liveState.has_value()
        ? GaugeStatusEvaluator::evaluate(liveState->value, gauge->statusRule)
        : status;
    m_reconstructionPanel->setSelectedGaugeDetails(gauge, currentStatus);
    m_reconstructionPanel->setRealtimeMonitoringState(
        gauge.has_value(),
        m_realtimeMonitoringController->state(),
        liveState,
        currentStatus,
        gauge.has_value() ? gauge->unit : QString());
    if (m_appShellViewModel != nullptr) {
        m_appShellViewModel->setSelectedMarkerId(m_selectedMarkerId);
    }
    updateMarkerControls();
}

void MainWindow::addDeviceMarker()
{
    if (!m_hasPendingSurfaceHit || !m_viewerMeshLoaded
        || m_pendingSurfaceTaskId != m_viewerTaskId
        || m_pendingSurfaceProjectId.isEmpty()
        || !m_projectManager.currentManifest().has_value()
        || m_pendingSurfaceProjectId != m_projectManager.currentManifest()->projectId()) {
        updateMarkerControls();
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("添加设备标记"),
                                               QStringLiteral("设备名称 / ID:"),
                                               QLineEdit::Normal,
                                               QStringLiteral("P01"),
                                               &accepted)
                            .trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    DeviceMarker createdMarker;
    QString error;
    if (!m_projectManager.addDeviceMarker(name,
                                          m_pendingSurfaceHit.worldPosition,
                                          m_viewerTaskId,
                                          &createdMarker,
                                          &error)) {
        showProjectError(error);
        return;
    }

    clearPendingSurfaceHit();
    refreshMarkerPresentation();
    m_modelViewerWidget->setSelectedMarker(createdMarker.id);
    m_logPanel->appendInfo(QStringLiteral("已添加设备标记: %1 | World: (%2, %3, %4)")
                               .arg(createdMarker.name)
                               .arg(createdMarker.worldPosition.x(), 0, 'f', 6)
                               .arg(createdMarker.worldPosition.y(), 0, 'f', 6)
                               .arg(createdMarker.worldPosition.z(), 0, 'f', 6));
}

void MainWindow::deleteDeviceMarker()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }

    const QString markerId = m_selectedMarkerId;
    const std::optional<GaugeAsset> boundGauge =
        m_projectManager.gaugeAssetForMarker(markerId);
    if (boundGauge.has_value()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            QStringLiteral("删除设备标记"),
            QStringLiteral("设备标记 %1 已绑定仪表资产“%2”。删除标记将同时删除该仪表资产，是否继续？")
                .arg(markerId, boundGauge->name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    if (boundGauge.has_value()) {
        m_realtimeMonitoringController->stopForGauge(boundGauge->id);
    }
    QString error;
    if (!m_projectManager.removeDeviceMarker(markerId, &error)) {
        showProjectError(error);
        return;
    }
    m_selectedMarkerId.clear();
    m_modelViewerWidget->setSelectedMarker(QString());
    m_reconstructionPanel->clearSelectedMarkerDetails();
    refreshMarkerPresentation();
    m_logPanel->appendInfo(QStringLiteral("已删除设备标记: %1").arg(markerId));
    updateMarkerControls();
}

void MainWindow::createGaugeAsset()
{
    if (m_selectedMarkerId.isEmpty() || !m_viewerMeshLoaded) {
        updateMarkerControls();
        return;
    }
    if (m_projectManager.gaugeAssetForMarker(m_selectedMarkerId).has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }

    GaugeAssetDialog dialog(QStringLiteral("创建仪表资产"), std::nullopt, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    GaugeAsset created = GaugeAsset::create(m_selectedMarkerId,
                                            dialog.assetName(),
                                            dialog.rangeMin(),
                                            dialog.rangeMax(),
                                            dialog.unit());
    created.gaugeProfileId = dialog.gaugeProfileId();
    QString error;
    if (!m_projectManager.addGaugeAsset(created, &error)) {
        showProjectError(error);
        return;
    }
    refreshMarkerPresentation();
    m_logPanel->appendInfo(QStringLiteral("已创建仪表资产: %1 (%2)")
                               .arg(created.name, created.id));
}

void MainWindow::editGaugeAsset()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> existing =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!existing.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }

    GaugeAssetDialog dialog(QStringLiteral("编辑仪表资产"), existing, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    GaugeAsset updated = *existing;
    updated.name = dialog.assetName();
    updated.rangeMin = dialog.rangeMin();
    updated.rangeMax = dialog.rangeMax();
    updated.unit = dialog.unit();
    updated.gaugeProfileId = dialog.gaugeProfileId();
    QString error;
    if (!m_projectManager.updateGaugeAsset(updated, &error)) {
        showProjectError(error);
        return;
    }
    refreshMarkerPresentation();
    m_logPanel->appendInfo(QStringLiteral("已更新仪表资产: %1").arg(updated.name));
}

void MainWindow::updateGaugeReading()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> existing =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!existing.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }

    bool accepted = false;
    const QString text = QInputDialog::getText(this,
                                               QStringLiteral("手动更新读数"),
                                               QStringLiteral("读数:"),
                                               QLineEdit::Normal,
                                               existing->latestValue.has_value()
                                                   ? QString::number(*existing->latestValue, 'g', 15)
                                                   : QString(),
                                               &accepted)
                             .trimmed();
    if (!accepted) {
        return;
    }
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        QMessageBox::warning(this,
                             QStringLiteral("读数无效"),
                             QStringLiteral("读数必须是有限数字。"));
        return;
    }
    QString error;
    if (!m_projectManager.updateGaugeReading(existing->id,
                                             value,
                                             QDateTime::currentDateTimeUtc(),
                                             GaugeDataSource::Manual,
                                             &error)) {
        showProjectError(error);
        return;
    }
    if (value < existing->rangeMin || value > existing->rangeMax) {
        m_logPanel->appendInfo(QStringLiteral("读数超出配置量程，已保存原始值: %1 %2")
                                   .arg(QString::number(value, 'g', 15), existing->unit));
    } else {
        m_logPanel->appendInfo(QStringLiteral("已更新手动读数: %1 %2")
                                   .arg(QString::number(value, 'g', 15), existing->unit));
    }
    refreshMarkerPresentation();
}

void MainWindow::visualGaugeReading()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> existing =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!existing.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }
    if (existing->gaugeProfileId.trimmed().isEmpty()) {
        showProjectError(QStringLiteral(
            "当前仪表没有绑定 GaugeProfile。请编辑仪表资产并选择视觉 Profile。"));
        return;
    }
    const std::optional<GaugeProfile> profile =
        builtInGaugeProfile(existing->gaugeProfileId);
    if (!profile.has_value()) {
        showProjectError(QStringLiteral("未找到 GaugeProfile: %1")
                             .arg(existing->gaugeProfileId));
        return;
    }

    QList<VisualGaugeImageChoice> projectImages;
    QString imageError;
    for (const AssetRecord& asset : m_projectManager.imageAssetRecords(&imageError)) {
        const QString path = m_projectManager.absoluteAssetPath(asset);
        if (!path.isEmpty() && QFileInfo(path).isFile()) {
            projectImages.append({asset.originalFileName, path, asset.id});
        }
    }
    if (!imageError.isEmpty()) {
        m_logPanel->appendError(imageError);
    }

    VisualGaugeReadingDialog dialog(*profile, projectImages, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    VisualGaugeReadingResult result = dialog.result();
    if (!result.success) {
        showProjectError(QStringLiteral("视觉读数失败: %1").arg(result.failureReason));
        return;
    }

    const QString captureDirectory =
        qEnvironmentVariable("VISION3DINSPECTOR_STAGE4D_CAPTURE_DIR").trimmed();
    if (!captureDirectory.isEmpty() && !result.diagnosticOverlay.isNull()) {
        if (!QDir().mkpath(captureDirectory)) {
            m_logPanel->appendError(QStringLiteral("无法创建视觉诊断 capture 目录: %1")
                                        .arg(captureDirectory));
        } else {
            const QString overlayPath = QDir(captureDirectory).filePath(
                QStringLiteral("visual_gauge_overlay_%1.png").arg(existing->id));
            if (!result.diagnosticOverlay.save(overlayPath)) {
                m_logPanel->appendError(QStringLiteral("无法保存视觉诊断 overlay: %1")
                                            .arg(overlayPath));
            } else {
                m_logPanel->appendInfo(QStringLiteral("视觉诊断 overlay 已保存: %1")
                                           .arg(overlayPath));
            }
        }
    }

    QString imageAssetId = dialog.selectedImageAssetId();
    if (imageAssetId.isEmpty()) {
        const QString selectedImagePath = dialog.selectedImagePath();
        if (selectedImagePath.isEmpty()) {
            showProjectError(QStringLiteral("视觉读数没有关联图片，无法建立巡检历史。"));
            return;
        }
        QString importError;
        const QList<AssetImportResult> importResults =
            m_projectManager.importImages({selectedImagePath}, &importError);
        if (!importError.isEmpty() || importResults.size() != 1
            || (importResults.first().status != AssetImportStatus::Imported
                && importResults.first().status != AssetImportStatus::Duplicate)) {
            showProjectError(importError.isEmpty()
                                 ? QStringLiteral("视觉巡检图片导入项目失败。 ").trimmed()
                                 : importError);
            return;
        }
        imageAssetId = importResults.first().asset.id;
        m_logPanel->appendInfo(importResults.first().message);
    }

    QString error;
    if (!m_projectManager.recordGaugeReading(existing->id,
                                             result.value,
                                             QDateTime::currentDateTimeUtc(),
                                             GaugeDataSource::Visual,
                                             imageAssetId,
                                             &error)) {
        showProjectError(error);
        return;
    }
    m_logPanel->appendInfo(
        QStringLiteral("已确认视觉读数: %1 %2 | angle=%3° | center=%4 | confidence=%5")
            .arg(QString::number(result.value, 'f', 3),
                 existing->unit,
                 QString::number(result.needleAngleDegrees, 'f', 1),
                 gaugeCenterSourceToString(result.centerSource),
                 QString::number(result.confidence, 'f', 2)));
    refreshMarkerPresentation();
}

void MainWindow::configureGaugeStatusRule()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> existing =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!existing.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }

    GaugeStatusRuleDialog dialog(QStringLiteral("配置仪表状态规则"),
                                 existing->rangeMin,
                                 existing->rangeMax,
                                 existing->statusRule,
                                 this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    GaugeAsset updated = *existing;
    updated.statusRule = dialog.statusRule();
    QString error;
    if (!m_projectManager.updateGaugeAsset(updated, &error)) {
        showProjectError(error);
        return;
    }
    refreshMarkerPresentation();
    m_logPanel->appendInfo(updated.statusRule.has_value()
                               ? QStringLiteral("已更新仪表状态规则: %1").arg(updated.name)
                               : QStringLiteral("已清除仪表状态规则: %1").arg(updated.name));
}

void MainWindow::startMockSensor()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> gauge =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!gauge.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }
    QString error;
    if (!m_realtimeMonitoringController->startMockSensor(gauge->id, &error)) {
        showProjectError(error);
        return;
    }
    m_logPanel->appendInfo(QStringLiteral("已启动仪表模拟传感器: %1").arg(gauge->name));
    refreshMarkerPresentation();
}

void MainWindow::stopMockSensor()
{
    m_realtimeMonitoringController->stop();
    m_logPanel->appendInfo(QStringLiteral("已停止实时仪表监控。"));
    refreshMarkerPresentation();
}

void MainWindow::recordCurrentSensorSample()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> gauge =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!gauge.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }
    QString error;
    if (!m_realtimeMonitoringController->recordCurrentValue(gauge->id, &error)) {
        showProjectError(error);
        return;
    }
    m_logPanel->appendInfo(QStringLiteral("已记录当前传感器读数: %1").arg(gauge->name));
    refreshMarkerPresentation();
}

void MainWindow::showGaugeHistory()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> gauge =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!gauge.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("inspectionHistoryDialog"));
    dialog.setWindowTitle(QStringLiteral("巡检历史 — %1").arg(gauge->name));
    dialog.resize(760, 420);

    auto* table = new QTableWidget(&dialog);
    table->setObjectName(QStringLiteral("inspectionHistoryTable"));
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({QStringLiteral("时间"),
                                      QStringLiteral("读数"),
                                      QStringLiteral("单位"),
                                      QStringLiteral("来源"),
                                      QStringLiteral("图片")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);

    const QList<InspectionRecord> records =
        m_projectManager.inspectionRecordsForGauge(gauge->id);
    table->setRowCount(records.size());
    for (qsizetype row = 0; row < records.size(); ++row) {
        const InspectionRecord& record = records.at(row);
        table->setItem(static_cast<int>(row),
                       0,
                       new QTableWidgetItem(
                           record.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        table->setItem(static_cast<int>(row),
                       1,
                       new QTableWidgetItem(QString::number(record.value, 'g', 15)));
        table->setItem(static_cast<int>(row), 2, new QTableWidgetItem(gauge->unit));
        table->setItem(static_cast<int>(row),
                       3,
                       new QTableWidgetItem(gaugeDataSourceDisplayName(record.dataSource)));
        table->setItem(static_cast<int>(row),
                       4,
                       new QTableWidgetItem(record.imageAssetId.isEmpty()
                                                ? QStringLiteral("—")
                                                : QStringLiteral("查看")));
    }

    auto* detailLabel = new QLabel(QStringLiteral("选择一条记录查看详情。"), &dialog);
    detailLabel->setObjectName(QStringLiteral("inspectionHistoryDetailLabel"));
    detailLabel->setWordWrap(true);
    auto* viewImageButton = new QPushButton(QStringLiteral("查看关联图片"), &dialog);
    viewImageButton->setObjectName(QStringLiteral("inspectionHistoryViewImageButton"));
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), &dialog);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(viewImageButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);

    auto updateHistorySelection = [&, detailLabel, viewImageButton] {
        const int row = table->currentRow();
        if (row < 0 || row >= records.size()) {
            detailLabel->setText(QStringLiteral("选择一条记录查看详情。"));
            viewImageButton->setEnabled(false);
            return;
        }
        const InspectionRecord& record = records.at(row);
        detailLabel->setText(
            QStringLiteral("时间: %1\n读数: %2 %3\n来源: %4\n关联图片: %5")
                .arg(record.timestamp.toLocalTime().toString(Qt::ISODateWithMs))
                .arg(QString::number(record.value, 'g', 15))
                .arg(gauge->unit)
                .arg(gaugeDataSourceDisplayName(record.dataSource))
                .arg(record.imageAssetId.isEmpty() ? QStringLiteral("无")
                                                    : record.imageAssetId));
        viewImageButton->setEnabled(!record.imageAssetId.isEmpty());
    };
    connect(table,
            &QTableWidget::itemSelectionChanged,
            &dialog,
            updateHistorySelection);
    connect(viewImageButton, &QPushButton::clicked, &dialog, [&, table] {
        const int row = table->currentRow();
        if (row < 0 || row >= records.size()) {
            return;
        }
        const InspectionRecord& record = records.at(row);
        const std::optional<AssetRecord> asset =
            m_projectManager.assetById(record.imageAssetId);
        if (!asset.has_value()) {
            showProjectError(QStringLiteral("巡检记录关联的 ImageAsset 不存在: %1")
                                 .arg(record.imageAssetId));
            return;
        }
        m_imagePreviewWidget->showAsset(*asset, m_projectManager.projectDirectory());
        showImagePreview();
        m_logPanel->appendInfo(QStringLiteral("已打开巡检图片: %1")
                                   .arg(asset->originalFileName));
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral("按时间倒序显示，历史记录不可直接编辑。"), &dialog));
    layout->addWidget(table, 1);
    layout->addWidget(detailLabel);
    layout->addLayout(buttons);
    if (!records.isEmpty()) {
        table->selectRow(0);
    } else {
        viewImageButton->setEnabled(false);
        detailLabel->setText(QStringLiteral("暂无历史记录。"));
    }
    dialog.exec();
}

void MainWindow::deleteGaugeAsset()
{
    if (m_selectedMarkerId.isEmpty()) {
        updateMarkerControls();
        return;
    }
    const std::optional<GaugeAsset> existing =
        m_projectManager.gaugeAssetForMarker(m_selectedMarkerId);
    if (!existing.has_value()) {
        refreshSelectedMarkerDetails();
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("删除仪表资产"),
        QStringLiteral("确定删除仪表资产“%1”吗？设备标记将保留。").arg(existing->name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_realtimeMonitoringController->stopForGauge(existing->id);
    QString error;
    if (!m_projectManager.removeGaugeAsset(existing->id, &error)) {
        showProjectError(error);
        return;
    }
    refreshMarkerPresentation();
    m_logPanel->appendInfo(QStringLiteral("已删除仪表资产，设备标记仍保留: %1")
                               .arg(m_selectedMarkerId));
}

void MainWindow::clearViewerAssociation()
{
    clearPendingSurfaceHit();
    if (m_modelViewerWidget != nullptr && m_modelViewerWidget->hasMesh()) {
        m_modelViewerWidget->clearMesh();
    }
    if (m_modelViewerWidget != nullptr) {
        m_modelViewerWidget->clearMarkers();
    }
    m_viewerMeshLoaded = false;
    m_viewerProjectDirectory.clear();
    m_viewerTaskId.clear();
    m_viewerMeshPath.clear();
    m_viewerCanonicalMeshPath.clear();
    m_selectedMarkerId.clear();
    if (m_previewStack != nullptr && m_imagePreviewWidget != nullptr) {
        m_previewStack->setCurrentWidget(m_imagePreviewWidget);
    }
    if (m_reconstructionPanel != nullptr) {
        m_reconstructionPanel->setViewerLoaded(false);
        m_reconstructionPanel->clearSelectedMarkerDetails();
        m_reconstructionPanel->setMarkerActionEnabled(false, false);
    }
}

void MainWindow::clearPendingSurfaceHit()
{
    m_hasPendingSurfaceHit = false;
    m_pendingSurfaceHit = SurfaceHit();
    m_pendingSurfaceProjectId.clear();
    m_pendingSurfaceTaskId.clear();
    updateMarkerControls();
}

void MainWindow::refreshMarkerPresentation()
{
    if (m_modelViewerWidget == nullptr || !m_viewerMeshLoaded || m_viewerTaskId.isEmpty()) {
        if (m_modelViewerWidget != nullptr) {
            m_modelViewerWidget->setMarkers({});
        }
        m_selectedMarkerId.clear();
        if (m_reconstructionPanel != nullptr) {
            m_reconstructionPanel->clearSelectedMarkerDetails();
        }
        updateMarkerControls();
        return;
    }

    const QList<DeviceMarker> activeMarkers =
        m_projectManager.deviceMarkersForReconstruction(m_viewerTaskId);
    QVector<DeviceMarkerView> views;
    views.reserve(activeMarkers.size());
    for (const DeviceMarker& marker : activeMarkers) {
        DeviceMarkerView view;
        view.id = marker.id;
        view.label = marker.name;
        view.worldPosition = marker.worldPosition;
        view.selected = marker.id == m_selectedMarkerId;
        const std::optional<GaugeAsset> gauge =
            m_projectManager.gaugeAssetForMarker(marker.id);
        if (gauge.has_value()) {
            const std::optional<realtime::GaugeLiveState> liveState =
                m_realtimeMonitoringController->liveStateForGauge(gauge->id);
            const GaugeStatus status = liveState.has_value()
                ? GaugeStatusEvaluator::evaluate(liveState->value, gauge->statusRule)
                : GaugeStatusEvaluator::evaluate(gauge->latestValue, gauge->statusRule);
            view.visualState = markerVisualStateForGaugeStatus(status);
        }
        views.append(view);
    }
    m_modelViewerWidget->setMarkers(views);
    if (!m_selectedMarkerId.isEmpty()
        && !m_projectManager.deviceMarkerById(m_selectedMarkerId).has_value()) {
        m_selectedMarkerId.clear();
        m_reconstructionPanel->clearSelectedMarkerDetails();
    }
    const qsizetype totalMarkers = m_projectManager.deviceMarkerModel().size();
    const qsizetype hiddenMarkers = totalMarkers - activeMarkers.size();
    if (hiddenMarkers > 0) {
        m_logPanel->appendInfo(QStringLiteral(
            "有 %1 个设备标记未显示：reconstruction identity 与当前 mesh 不一致。")
                                    .arg(hiddenMarkers));
    }
    if (!m_selectedMarkerId.isEmpty()) {
        refreshSelectedMarkerDetails();
    }
    if (m_appShellViewModel != nullptr) {
        m_appShellViewModel->refresh();
    }
    updateMarkerControls();
}

void MainWindow::updateMarkerControls()
{
    if (m_reconstructionPanel == nullptr) {
        return;
    }
    const bool canAdd = m_hasPendingSurfaceHit
        && m_viewerMeshLoaded
        && m_pendingSurfaceTaskId == m_viewerTaskId
        && m_projectManager.hasProject()
        && m_pendingSurfaceProjectId == m_projectManager.currentManifest()->projectId();
    const bool canDelete = !m_selectedMarkerId.isEmpty()
        && m_viewerMeshLoaded
        && m_projectManager.deviceMarkerById(m_selectedMarkerId).has_value();
    m_reconstructionPanel->setMarkerActionEnabled(canAdd, canDelete);
    const bool hasSelectedMarker = !m_selectedMarkerId.isEmpty()
        && m_viewerMeshLoaded
        && m_projectManager.deviceMarkerById(m_selectedMarkerId).has_value();
    const bool hasGauge = hasSelectedMarker
        && m_projectManager.gaugeAssetForMarker(m_selectedMarkerId).has_value();
    m_reconstructionPanel->setGaugeActionEnabled(hasSelectedMarker && !hasGauge,
                                                  hasGauge,
                                                  hasGauge,
                                                  hasGauge);
    m_reconstructionPanel->setVisualGaugeReadingEnabled(hasGauge);
    m_reconstructionPanel->setGaugeStatusRuleEnabled(hasGauge);
}

void MainWindow::showImagePreview()
{
    if (m_previewStack != nullptr && m_imagePreviewWidget != nullptr) {
        m_previewStack->setCurrentWidget(m_imagePreviewWidget);
    }
}

void MainWindow::showProjectError(const QString& message)
{
    m_logPanel->appendError(message);
    QMessageBox::warning(this, QStringLiteral("项目操作失败"), message);
}

} // namespace vision3d
