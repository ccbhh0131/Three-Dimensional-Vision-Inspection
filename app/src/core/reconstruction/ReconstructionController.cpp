#include "core/reconstruction/ReconstructionController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>

namespace vision3d {

namespace {

const QList<ReconstructionController::StageDefinition>& stageDefinitions()
{
    static const QList<ReconstructionController::StageDefinition> definitions = {
        {ReconstructionStage::FeatureExtraction, QStringLiteral("A1_feature_extraction.log")},
        {ReconstructionStage::FeatureMatching, QStringLiteral("A2_feature_matching.log")},
        {ReconstructionStage::SparseMapping, QStringLiteral("A3_sparse_mapping.log")},
        {ReconstructionStage::ImageUndistortion, QStringLiteral("A4_undistortion.log")},
        {ReconstructionStage::DenseStereo, QStringLiteral("A5_patchmatch.log")},
        {ReconstructionStage::StereoFusion, QStringLiteral("A6_fusion.log")},
        {ReconstructionStage::Meshing, QStringLiteral("A7_meshing.log")},
        {ReconstructionStage::ModelOptimization,
         QStringLiteral("A8_model_optimization.log")},
    };
    return definitions;
}

} // namespace

ReconstructionController::ReconstructionController(ProjectManager* projectManager,
                                                   ColmapBackend* backend,
                                                   ProcessRunner* processRunner,
                                                   QObject* parent)
    : QObject(parent)
    , m_projectManager(projectManager)
    , m_backend(backend)
    , m_processRunner(processRunner)
{
    Q_ASSERT(m_projectManager != nullptr);
    Q_ASSERT(m_backend != nullptr);
    if (m_processRunner == nullptr) {
        m_ownedProcessRunner = new ProcessRunner(this);
        m_processRunner = m_ownedProcessRunner;
    }
    connect(m_processRunner, &ProcessRunner::started, this, &ReconstructionController::onProcessStarted);
    connect(m_processRunner,
            &ProcessRunner::stdoutReady,
            this,
            &ReconstructionController::onProcessStdout);
    connect(m_processRunner,
            &ProcessRunner::stderrReady,
            this,
            &ReconstructionController::onProcessStderr);
    connect(m_processRunner,
            &ProcessRunner::failed,
            this,
            &ReconstructionController::onProcessFailed);
    connect(m_processRunner,
            &ProcessRunner::finished,
            this,
            &ReconstructionController::onProcessFinished);
}

bool ReconstructionController::validateStartPreconditions(const QString& backendRoot,
                                                           QList<AssetRecord>* assets,
                                                           QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (isRunning()) {
        return fail(QStringLiteral("已有 Reconstruction Task 正在运行。"));
    }
    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        return fail(QStringLiteral("开始重建前必须打开项目。"));
    }
    QString assetError;
    const QList<AssetRecord> records = m_projectManager->imageAssetRecords(&assetError);
    if (!assetError.isEmpty()) {
        return fail(assetError);
    }
    if (records.size() < 2) {
        return fail(QStringLiteral("开始重建至少需要 2 张图像。"));
    }
    for (const AssetRecord& asset : records) {
        if (!QFileInfo(m_projectManager->absoluteAssetPath(asset)).isFile()) {
            return fail(QStringLiteral("参与重建的图像文件不存在: %1")
                            .arg(asset.originalFileName));
        }
    }
    const BackendProbeResult validation = m_backend->validateRoot(backendRoot);
    if (!validation.message.startsWith(QStringLiteral("COLMAP 根目录结构有效"))) {
        return fail(validation.message);
    }
    if (assets != nullptr) {
        *assets = records;
    }
    return true;
}

bool ReconstructionController::start(const QString& backendRoot, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    QList<AssetRecord> assets;
    if (!validateStartPreconditions(backendRoot, &assets, error)) {
        return false;
    }

    m_backendRoot = QDir(backendRoot).absolutePath();
    m_assets = assets;
    m_task = ReconstructionTask();
    m_task.taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_task.state = ReconstructionState::Preparing;
    m_task.stage = ReconstructionStage::PreparingInput;
    m_task.backendName = m_backend->backendName();
    m_task.createdAt = QDateTime::currentDateTimeUtc();
    m_task.startedAt = m_task.createdAt;
    m_task.inputImageCount = m_assets.size();
    m_task.workspaceRelativePath = QStringLiteral("reconstruction/jobs/%1").arg(m_task.taskId);

    QString workspaceError;
    if (!ReconstructionWorkspace::create(m_projectManager->projectDirectory(),
                                         m_task.taskId,
                                         &m_jobPaths,
                                         &workspaceError)) {
        if (error != nullptr) {
            *error = workspaceError;
        }
        return false;
    }
    QList<ReconstructionInputMapping> mappings;
    if (!ReconstructionWorkspace::stageInputs(m_jobPaths, m_assets, &mappings, &workspaceError)) {
        if (error != nullptr) {
            *error = workspaceError;
        }
        m_task.state = ReconstructionState::Failed;
        m_task.errorMessage = workspaceError;
        m_task.finishedAt = QDateTime::currentDateTimeUtc();
        QString ignored;
        m_task.save(m_jobPaths.taskJsonPath, &ignored);
        return false;
    }

    QString persistError;
    if (!persistTask(&persistError)) {
        if (error != nullptr) {
            *error = persistError;
        }
        return false;
    }
    m_totalTimer.start();
    m_cancelRequested = false;
    m_processError.clear();
    m_probeStdout.clear();
    m_probeStderr.clear();
    beginProbe();
    return true;
}

void ReconstructionController::beginProbe()
{
    m_processPurpose = ProcessPurpose::Probe;
    m_task.state = ReconstructionState::Running;
    m_task.stage = ReconstructionStage::PreparingInput;
    setTaskChanged();
    QString logName = QStringLiteral("backend_probe.log");
    const ProcessCommand command = m_backend->probeCommand(m_backendRoot, m_jobPaths.root);
    openLog(logName, command);
    appendLog(QStringLiteral("Probe started.\n"));
    QString error;
    if (!m_processRunner->start(command.program,
                                command.arguments,
                                command.workingDirectory,
                                &error)) {
        m_processError = error;
        failTask(QStringLiteral("COLMAP Backend Probe 无法启动: %1").arg(error), -1);
    }
}

void ReconstructionController::beginStage(int index)
{
    if (index < 0 || index >= stageDefinitions().size()) {
        completeTask();
        return;
    }
    m_stageIndex = index;
    const StageDefinition definition = stageDefinitions().at(index);
    m_processPurpose = ProcessPurpose::Stage;
    m_task.state = ReconstructionState::Running;
    m_task.stage = definition.stage;
    m_task.exitCode = -1;
    m_processError.clear();
    m_stageTimer.start();
    const ProcessCommand command = commandForStage(definition.stage);
    openLog(definition.logName, command);
    appendLog(QStringLiteral("Stage started: %1\n").arg(stageName(definition.stage)));
    setTaskChanged();

    if (definition.stage == ReconstructionStage::ModelOptimization
        && !QFileInfo(command.program).isFile()) {
        completeWithRawMeshFallback(
            QStringLiteral("Vision3DGeometryWorker.exe 不存在: %1").arg(command.program),
            -1);
        return;
    }

    QString error;
    if (!m_processRunner->start(command.program,
                                command.arguments,
                                command.workingDirectory,
                                &error)) {
        m_processError = error;
        if (definition.stage == ReconstructionStage::ModelOptimization) {
            completeWithRawMeshFallback(error, -1);
        } else {
            failTask(QStringLiteral("%1 无法启动: %2").arg(stageName(definition.stage), error), -1);
        }
    }
}

ProcessCommand ReconstructionController::commandForStage(ReconstructionStage stage) const
{
    switch (stage) {
    case ReconstructionStage::FeatureExtraction:
        return m_backend->featureExtractionCommand(m_backendRoot, m_jobPaths.root, m_config);
    case ReconstructionStage::FeatureMatching:
        return m_backend->featureMatchingCommand(m_backendRoot, m_jobPaths.root, m_config);
    case ReconstructionStage::SparseMapping:
        return m_backend->sparseMappingCommand(m_backendRoot, m_jobPaths.root);
    case ReconstructionStage::ImageUndistortion:
        return m_backend->undistortionCommand(m_backendRoot,
                                               m_jobPaths.root,
                                               m_task.primaryModelRelativePath);
    case ReconstructionStage::DenseStereo:
        return m_backend->patchMatchCommand(m_backendRoot, m_jobPaths.root, m_config);
    case ReconstructionStage::StereoFusion:
        return m_backend->fusionCommand(m_backendRoot, m_jobPaths.root);
    case ReconstructionStage::Meshing:
        return m_backend->meshingCommand(m_backendRoot, m_jobPaths.root);
    case ReconstructionStage::ModelOptimization:
        return modelOptimizationCommand();
    default:
        return {};
    }
}

ProcessCommand ReconstructionController::modelOptimizationCommand() const
{
    QString program = QString::fromLocal8Bit(qgetenv("VISION3D_GEOMETRY_WORKER"));
    if (program.trimmed().isEmpty()) {
        program = QDir(QCoreApplication::applicationDirPath())
                      .filePath(QStringLiteral("Vision3DGeometryWorker.exe"));
    }
    return {program,
            {QStringLiteral("refine"),
             QStringLiteral("--cloud"),
             QDir(m_jobPaths.root).filePath(QStringLiteral("dense/fused.ply")),
             QStringLiteral("--output"),
             m_jobPaths.refinementDirectory},
            m_jobPaths.root,
            QStringLiteral("Open3D + meshoptimizer model refinement")};
}

void ReconstructionController::openLog(const QString& fileName, const ProcessCommand& command)
{
    closeLog();
    m_stageLog.setFileName(QDir(m_jobPaths.logsDirectory).filePath(fileName));
    if (!m_stageLog.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        emit logMessage(QStringLiteral("无法打开阶段日志: %1").arg(m_stageLog.fileName()), true);
        return;
    }
    appendLog(QStringLiteral("program: %1\narguments: %2\nworkingDirectory: %3\n")
                  .arg(command.program,
                       command.arguments.join(QLatin1Char(' ')),
                       command.workingDirectory));
}

void ReconstructionController::closeLog()
{
    if (m_stageLog.isOpen()) {
        m_stageLog.flush();
        m_stageLog.close();
    }
}

void ReconstructionController::appendLog(const QString& text)
{
    if (m_stageLog.isOpen()) {
        m_stageLog.write(text.toUtf8());
        m_stageLog.flush();
    }
}

void ReconstructionController::onProcessStarted()
{
    appendLog(QStringLiteral("process started\n"));
    emit logMessage(QStringLiteral("%1 进程已启动。")
                        .arg(m_processPurpose == ProcessPurpose::Probe
                                 ? QStringLiteral("Backend Probe")
                                 : stageName(m_task.stage)),
                    false);
}

void ReconstructionController::onProcessStdout(const QString& text)
{
    if (m_processPurpose == ProcessPurpose::Probe) {
        m_probeStdout.append(text);
    }
    appendLog(QStringLiteral("\n[stdout]\n") + text);
    emit logMessage(text, false);
}

void ReconstructionController::onProcessStderr(const QString& text)
{
    if (m_processPurpose == ProcessPurpose::Probe) {
        m_probeStderr.append(text);
    }
    appendLog(QStringLiteral("\n[stderr]\n") + text);
    emit logMessage(text, true);
}

void ReconstructionController::onProcessFailed(const QString& message)
{
    m_processError = message;
    appendLog(QStringLiteral("\n[process error]\n") + message + QLatin1Char('\n'));
    emit logMessage(QStringLiteral("进程错误: %1").arg(message), true);
}

void ReconstructionController::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    closeLog();
    if (m_cancelRequested) {
        cancelTask();
        return;
    }
    if (m_processPurpose == ProcessPurpose::Probe) {
        const BackendProbeResult result = m_backend->parseProbeResult(m_backendRoot,
                                                                       exitCode,
                                                                       exitStatus,
                                                                       m_probeStdout,
                                                                       m_probeStderr,
                                                                       m_processError);
        if (!result.available || result.version != QStringLiteral("3.11.1")) {
            failTask(QStringLiteral("COLMAP Backend 未通过运行时版本锁：%1")
                         .arg(result.message),
                     exitCode);
            return;
        }
        m_task.backendVersion = result.version;
        persistTask();
        QTimer::singleShot(0, this, [this] { beginStage(0); });
        return;
    }

    m_task.exitCode = exitCode;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        const QString detail = !m_processError.isEmpty()
                                   ? m_processError
                                   : QStringLiteral("exit code=%1").arg(exitCode);
        if (m_task.stage == ReconstructionStage::ModelOptimization) {
            completeWithRawMeshFallback(detail, exitCode);
        } else {
            failTask(QStringLiteral("%1 失败：%2").arg(stageName(m_task.stage), detail), exitCode);
        }
        return;
    }

    QString artifactError;
    if (!validateStageArtifact(m_task.stage, &artifactError)) {
        if (m_task.stage == ReconstructionStage::ModelOptimization) {
            completeWithRawMeshFallback(
                QStringLiteral("artifact 校验失败: %1").arg(artifactError), exitCode);
        } else {
            failTask(QStringLiteral("%1 artifact 校验失败：%2")
                         .arg(stageName(m_task.stage), artifactError),
                     exitCode);
        }
        return;
    }
    const int nextIndex = m_stageIndex + 1;
    QTimer::singleShot(0, this, [this, nextIndex] { beginStage(nextIndex); });
}

bool ReconstructionController::validateStageArtifact(ReconstructionStage stage, QString* error)
{
    switch (stage) {
    case ReconstructionStage::FeatureExtraction:
    case ReconstructionStage::FeatureMatching:
        return ReconstructionArtifactValidator::validateDatabase(m_jobPaths.databasePath, error);
    case ReconstructionStage::SparseMapping:
        return selectPrimaryModel(error);
    case ReconstructionStage::ImageUndistortion:
        return ReconstructionArtifactValidator::validateUndistortion(m_jobPaths.denseDirectory,
                                                                      error);
    case ReconstructionStage::DenseStereo:
        return ReconstructionArtifactValidator::validatePatchMatch(m_jobPaths.denseDirectory,
                                                                    error);
    case ReconstructionStage::StereoFusion:
        m_task.fusedPointCloudRelativePath = QStringLiteral("dense/fused.ply");
        return ReconstructionArtifactValidator::validateNonEmptyFile(
            QDir(m_jobPaths.root).filePath(m_task.fusedPointCloudRelativePath), error);
    case ReconstructionStage::Meshing:
        m_task.meshRelativePath = QStringLiteral("dense/meshed-poisson.ply");
        return ReconstructionArtifactValidator::validateNonEmptyFile(
            QDir(m_jobPaths.root).filePath(m_task.meshRelativePath), error);
    case ReconstructionStage::ModelOptimization: {
        const QString open3dMesh = QDir(m_jobPaths.refinementDirectory)
                                       .filePath(QStringLiteral("open3d-mesh.ply"));
        const QString finalMesh = QDir(m_jobPaths.refinementDirectory)
                                      .filePath(QStringLiteral("final-mesh.ply"));
        const QString metadata = QDir(m_jobPaths.refinementDirectory)
                                     .filePath(QStringLiteral("refinement.json"));
        if (!ReconstructionArtifactValidator::validateNonEmptyFile(open3dMesh, error)
            || !ReconstructionArtifactValidator::validateNonEmptyFile(finalMesh, error)
            || !ReconstructionArtifactValidator::validateNonEmptyFile(metadata, error)) {
            return false;
        }
        m_task.meshRelativePath = QStringLiteral("dense/refinement/final-mesh.ply");
        return true;
    }
    default:
        return true;
    }
}

bool ReconstructionController::selectPrimaryModel(QString* error)
{
    const SparseModelSelection selection =
        ReconstructionArtifactValidator::selectPrimarySparseModel(m_jobPaths.sparseDirectory);
    if (!selection.valid) {
        if (error != nullptr) {
            *error = selection.message;
        }
        return false;
    }
    m_task.primaryModelRelativePath = selection.relativePath;
    m_task.registeredImageCount = selection.registeredImages;
    m_task.registrationRatio = m_task.inputImageCount > 0
                                   ? static_cast<double>(m_task.registeredImageCount)
                                         / static_cast<double>(m_task.inputImageCount)
                                   : 0.0;
    return true;
}

void ReconstructionController::failTask(const QString& message, int exitCode)
{
    closeLog();
    m_processPurpose = ProcessPurpose::None;
    m_task.state = ReconstructionState::Failed;
    m_task.errorMessage = message;
    m_task.failedStage = stageName(m_task.stage);
    m_task.exitCode = exitCode;
    m_task.finishedAt = QDateTime::currentDateTimeUtc();
    m_task.totalElapsedMilliseconds = m_totalTimer.isValid() ? m_totalTimer.elapsed() : -1;
    QString persistError;
    if (!persistTask(&persistError) && !persistError.isEmpty()) {
        emit logMessage(QStringLiteral("失败状态持久化失败: %1").arg(persistError), true);
    }
    emit logMessage(message, true);
    emit taskChanged();
    emit finished(false);
}

void ReconstructionController::completeWithRawMeshFallback(const QString& detail, int exitCode)
{
    closeLog();
    const QString rawMeshRelativePath = QStringLiteral("dense/meshed-poisson.ply");
    QString rawMeshError;
    if (!ReconstructionArtifactValidator::validateNonEmptyFile(
            QDir(m_jobPaths.root).filePath(rawMeshRelativePath), &rawMeshError)) {
        failTask(QStringLiteral("Model Optimization Failed，且 Raw Mesh fallback 不可用: %1 (%2)")
                     .arg(detail, rawMeshError),
                 exitCode);
        return;
    }

    m_processPurpose = ProcessPurpose::None;
    m_task.state = ReconstructionState::Completed;
    m_task.stage = ReconstructionStage::Completed;
    m_task.meshRelativePath = rawMeshRelativePath;
    m_task.errorMessage = QStringLiteral("Model Optimization Failed / Using Raw Mesh: %1")
                              .arg(detail);
    m_task.failedStage = stageName(ReconstructionStage::ModelOptimization);
    m_task.exitCode = exitCode;
    m_task.finishedAt = QDateTime::currentDateTimeUtc();
    m_task.totalElapsedMilliseconds = m_totalTimer.isValid() ? m_totalTimer.elapsed() : -1;
    QString persistError;
    if (!persistTask(&persistError) && !persistError.isEmpty()) {
        emit logMessage(QStringLiteral("回退状态持久化失败: %1").arg(persistError), true);
    }
    emit logMessage(QStringLiteral("Model Optimization Failed / Using Raw Mesh"), false);
    if (!detail.isEmpty()) {
        emit logMessage(detail, true);
    }
    emit taskChanged();
    emit finished(true);
}

void ReconstructionController::cancelTask()
{
    closeLog();
    m_processPurpose = ProcessPurpose::None;
    m_task.state = ReconstructionState::Cancelled;
    m_task.errorMessage = QStringLiteral("用户取消 Reconstruction Task。 ").trimmed();
    m_task.finishedAt = QDateTime::currentDateTimeUtc();
    m_task.totalElapsedMilliseconds = m_totalTimer.isValid() ? m_totalTimer.elapsed() : -1;
    persistTask();
    emit logMessage(QStringLiteral("重建已取消。"), false);
    emit taskChanged();
    emit finished(false);
}

void ReconstructionController::completeTask()
{
    closeLog();
    m_processPurpose = ProcessPurpose::None;
    m_task.state = ReconstructionState::Completed;
    m_task.stage = ReconstructionStage::Completed;
    m_task.finishedAt = QDateTime::currentDateTimeUtc();
    m_task.totalElapsedMilliseconds = m_totalTimer.isValid() ? m_totalTimer.elapsed() : -1;
    persistTask();
    emit logMessage(QStringLiteral("A1-A8 Reconstruction Pipeline 已完成。"), false);
    emit taskChanged();
    emit finished(true);
}

void ReconstructionController::cancel()
{
    if (!isRunning()) {
        return;
    }
    m_cancelRequested = true;
    emit logMessage(QStringLiteral("正在请求取消当前阶段……"), false);
    m_processRunner->requestCancel();
}

bool ReconstructionController::isRunning() const
{
    return m_processPurpose != ProcessPurpose::None || m_processRunner->isRunning();
}

const ReconstructionTask& ReconstructionController::currentTask() const { return m_task; }

const ReconstructionJobPaths& ReconstructionController::jobPaths() const { return m_jobPaths; }

qint64 ReconstructionController::activeProcessId() const
{
    return m_processRunner == nullptr ? 0 : m_processRunner->processId();
}

int ReconstructionController::completedStageCount(ReconstructionStage stage)
{
    switch (stage) {
    case ReconstructionStage::None:
    case ReconstructionStage::PreparingInput:
        return 0;
    case ReconstructionStage::FeatureExtraction:
        return 1;
    case ReconstructionStage::FeatureMatching:
        return 2;
    case ReconstructionStage::SparseMapping:
        return 3;
    case ReconstructionStage::ImageUndistortion:
        return 4;
    case ReconstructionStage::DenseStereo:
        return 5;
    case ReconstructionStage::StereoFusion:
        return 6;
    case ReconstructionStage::Meshing:
    case ReconstructionStage::ModelOptimization:
        return 7;
    case ReconstructionStage::Completed:
        return 8;
    }
    return 0;
}

void ReconstructionController::setTaskChanged()
{
    QString ignored;
    if (!persistTask(&ignored) && !ignored.isEmpty()) {
        emit logMessage(QStringLiteral("任务状态持久化失败: %1").arg(ignored), true);
    }
    emit taskChanged();
}

bool ReconstructionController::persistTask(QString* error)
{
    if (!m_jobPaths.taskJsonPath.isEmpty() && !m_task.save(m_jobPaths.taskJsonPath, error)) {
        return false;
    }
    if (m_projectManager != nullptr && m_projectManager->hasProject()) {
        QString manifestError;
        if (!m_projectManager->updateReconstructionTask(m_task, &manifestError)) {
            if (error != nullptr) {
                *error = manifestError;
            }
            return false;
        }
    }
    return true;
}

QString ReconstructionController::stageName(ReconstructionStage stage)
{
    switch (stage) {
    case ReconstructionStage::None:
        return QStringLiteral("None");
    case ReconstructionStage::PreparingInput:
        return QStringLiteral("Preparing Input");
    case ReconstructionStage::FeatureExtraction:
        return QStringLiteral("A1 Feature Extraction");
    case ReconstructionStage::FeatureMatching:
        return QStringLiteral("A2 Feature Matching");
    case ReconstructionStage::SparseMapping:
        return QStringLiteral("A3 Sparse Mapping");
    case ReconstructionStage::ImageUndistortion:
        return QStringLiteral("A4 Image Undistortion");
    case ReconstructionStage::DenseStereo:
        return QStringLiteral("A5 PatchMatch Stereo");
    case ReconstructionStage::StereoFusion:
        return QStringLiteral("A6 Stereo Fusion");
    case ReconstructionStage::Meshing:
        return QStringLiteral("A7 Poisson Meshing");
    case ReconstructionStage::ModelOptimization:
        return QStringLiteral("A8 Model Optimization");
    case ReconstructionStage::Completed:
        return QStringLiteral("Completed");
    }
    return QStringLiteral("Unknown");
}

} // namespace vision3d
