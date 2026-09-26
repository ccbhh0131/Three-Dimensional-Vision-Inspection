#include "app/MainWindow.h"

#include "widgets/BackendPanel.h"
#include "backend/ReconstructionEngineLocator.h"
#include "core/mesh/PlyMeshLoader.h"
#include "widgets/DeveloperSettingsDialog.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/LogPanel.h"
#include "widgets/ModelViewerWidget.h"
#include "widgets/ProjectPanel.h"
#include "widgets/ReconstructionPanel.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QWidget>

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
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->addWidget(topSplitter, 2);
    centralLayout->addWidget(m_logPanel, 1);
    setCentralWidget(centralWidget);

    createActions();

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

    m_logPanel->appendInfo(QStringLiteral("应用已启动。当前为 V0.1 Stage 3。"));
    m_logPanel->appendInfo(QStringLiteral("当前已接入三维重建引擎。"));
    refreshProjectView();
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
    return m_projectManager.openProject(fileOrDirectory, error);
}

void MainWindow::closeProject()
{
    if (m_reconstructionController != nullptr && m_reconstructionController->isRunning()) {
        m_logPanel->appendError(QStringLiteral("当前正在进行三维重建，不能关闭项目。"));
        return;
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
    m_logPanel->appendInfo(QStringLiteral("已选择设备标记: %1 (%2)")
                               .arg(marker->name, marker->id));
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
