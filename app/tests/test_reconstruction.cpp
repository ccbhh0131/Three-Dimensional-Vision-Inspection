#include "backend/ColmapBackend.h"
#include "core/process/ProcessRunner.h"
#include "core/project/ProjectManager.h"
#include "core/reconstruction/ReconstructionArtifactValidator.h"
#include "core/reconstruction/ReconstructionController.h"
#include "core/reconstruction/ReconstructionTask.h"
#include "core/reconstruction/ReconstructionWorkspace.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace {

QString writeImage(const QString& directory, const QString& fileName, const QColor& color)
{
    QDir().mkpath(directory);
    const QString path = QDir(directory).filePath(fileName);
    QImage image(QSize(32, 24), QImage::Format_RGB32);
    image.fill(color);
    return image.save(path, "PNG") ? path : QString();
}

bool createFakeBackend(const QString& root, QString* error = nullptr)
{
    if (!QDir().mkpath(QDir(root).filePath(QStringLiteral("bin")))) {
        if (error != nullptr) {
            *error = QStringLiteral("无法创建 fake backend 目录");
        }
        return false;
    }
    QFile batch(QDir(root).filePath(QStringLiteral("COLMAP.bat")));
    if (!batch.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = batch.errorString();
        }
        return false;
    }
    batch.write("@echo off\r\n");
    batch.close();
    QFile executable(QDir(root).filePath(QStringLiteral("bin/colmap.exe")));
    if (!executable.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = executable.errorString();
        }
        return false;
    }
    executable.write("fake");
    executable.close();
    return true;
}

bool createProjectWithImages(vision3d::ProjectManager& manager,
                             const QString& parentDirectory,
                             int imageCount,
                             QString* error = nullptr)
{
    if (!manager.newProject(parentDirectory, QStringLiteral("ReconstructionTest"), error)) {
        return false;
    }
    QStringList sources;
    for (int index = 0; index < imageCount; ++index) {
        const QColor color = index == 0 ? QColor(QStringLiteral("#2f80ed"))
                                       : QColor(QStringLiteral("#eb5757"));
        const QString source = writeImage(
            QDir(parentDirectory).filePath(QStringLiteral("sources")),
            QStringLiteral("source_%1.png").arg(index + 1),
            color);
        if (source.isEmpty()) {
            if (error != nullptr) {
                *error = QStringLiteral("无法生成测试图像");
            }
            return false;
        }
        sources.append(source);
    }
    const QList<vision3d::AssetImportResult> results = manager.importImages(sources, error);
    if (results.size() != imageCount) {
        if (error != nullptr) {
            *error = QStringLiteral("测试图像导入数量不符");
        }
        return false;
    }
    for (const vision3d::AssetImportResult& result : results) {
        if (result.status != vision3d::AssetImportStatus::Imported) {
            if (error != nullptr) {
                *error = result.message;
            }
            return false;
        }
    }
    return true;
}

class FakeProcessRunner final : public vision3d::ProcessRunner
{
public:
    explicit FakeProcessRunner(QObject* parent = nullptr)
        : ProcessRunner(parent)
    {
    }

    QString version = QStringLiteral("3.11.1");
    QString failCommand;
    bool holdStage = false;
    QStringList startedCommands;

    bool start(const QString& program,
               const QStringList& arguments,
               const QString& workingDirectory,
               QString* error = nullptr) override
    {
        Q_UNUSED(program)
        Q_UNUSED(error)
        if (m_running) {
            return false;
        }
        m_running = true;
        m_cancelled = false;
        ++m_serial;
        const int serial = m_serial;
        const QString command = arguments.join(QLatin1Char(' '));
        m_workingDirectory = workingDirectory;
        startedCommands.append(command);
        const bool probe = command == QStringLiteral("-h")
                           || command.contains(QStringLiteral(" -h"));
        const bool shouldFail = !probe && !failCommand.isEmpty()
                                && command.contains(failCommand);
        if (!probe && !shouldFail) {
            createArtifacts(command, workingDirectory);
        }
        const int delay = (!probe && holdStage) ? 250 : 0;
        QTimer::singleShot(delay, this, [this, serial, command, probe, shouldFail] {
            if (serial != m_serial || !m_running || m_cancelled) {
                return;
            }
            emit started();
            if (probe) {
                emit stdoutReady(QStringLiteral("COLMAP %1 (fake)\n").arg(version));
            } else if (shouldFail) {
                emit stderrReady(QStringLiteral("fake stage failure: %1\n").arg(command));
            } else {
                emit stdoutReady(QStringLiteral("fake stage passed\n"));
            }
            m_running = false;
            emit finished(shouldFail ? 2 : 0,
                          shouldFail ? QProcess::NormalExit : QProcess::NormalExit);
        });
        return true;
    }

    bool isRunning() const override { return m_running; }

    void requestCancel(int terminateWaitMilliseconds = 1500) override
    {
        Q_UNUSED(terminateWaitMilliseconds)
        if (!m_running) {
            return;
        }
        m_cancelled = true;
        const int serial = m_serial;
        QTimer::singleShot(0, this, [this, serial] {
            if (serial != m_serial || !m_running) {
                return;
            }
            m_running = false;
            emit finished(-1, QProcess::CrashExit);
        });
    }

    qint64 processId() const override { return m_running ? 4242 : 0; }

private:
    void createArtifacts(const QString& command, const QString& workingDirectory)
    {
        const QDir root(workingDirectory);
        if (command.contains(QStringLiteral("feature_extractor"))
            || command.contains(QStringLiteral("exhaustive_matcher"))) {
            QFile database(root.filePath(QStringLiteral("database.db")));
            database.open(QIODevice::WriteOnly | QIODevice::Append);
            database.write("fake database\n");
            database.close();
        } else if (command == QStringLiteral("mapper")
                   || command.startsWith(QStringLiteral("mapper "))) {
            const QDir model(root.filePath(QStringLiteral("sparse/0")));
            QDir().mkpath(model.absolutePath());
            for (const QString& name : {QStringLiteral("cameras.bin"),
                                        QStringLiteral("points3D.bin")}) {
                QFile file(model.filePath(name));
                file.open(QIODevice::WriteOnly);
                file.write("fake");
                file.close();
            }
            QFile images(model.filePath(QStringLiteral("images.bin")));
            images.open(QIODevice::WriteOnly);
            QByteArray count(8, '\0');
            count[0] = 2;
            images.write(count);
            images.write("fake image records");
            images.close();
        } else if (command.contains(QStringLiteral("image_undistorter"))) {
            QDir().mkpath(root.filePath(QStringLiteral("dense/images")));
            QDir().mkpath(root.filePath(QStringLiteral("dense/sparse")));
        } else if (command.contains(QStringLiteral("patch_match_stereo"))) {
            QDir().mkpath(root.filePath(QStringLiteral("dense/stereo/depth_maps")));
            QDir().mkpath(root.filePath(QStringLiteral("dense/stereo/normal_maps")));
            QFile depth(root.filePath(QStringLiteral("dense/stereo/depth_maps/a.geometric.bin")));
            depth.open(QIODevice::WriteOnly);
            depth.write("depth");
            depth.close();
        } else if (command.contains(QStringLiteral("stereo_fusion"))) {
            QFile fused(root.filePath(QStringLiteral("dense/fused.ply")));
            fused.open(QIODevice::WriteOnly);
            fused.write("ply\n");
            fused.close();
        } else if (command.contains(QStringLiteral("poisson_mesher"))) {
            QFile mesh(root.filePath(QStringLiteral("dense/meshed-poisson.ply")));
            mesh.open(QIODevice::WriteOnly);
            mesh.write("ply\nmesh\n");
            mesh.close();
        }
    }

    bool m_running = false;
    bool m_cancelled = false;
    int m_serial = 0;
    QString m_workingDirectory;
};

class ReconstructionTest final : public QObject
{
    Q_OBJECT

private slots:
    void taskSerializationRoundtrip();
    void stateEnumConversion();
    void stageEnumConversion();
    void taskWorkspaceCreation();
    void inputStaging();
    void inputManifestMapping();
    void relativePathPolicy();
    void missingAssetPreventsStart();
    void backendUnavailablePreventsStart();
    void unverifiedBackendVersionPreventsStart();
    void failedStageStopsNextStages();
    void completedTaskMetadataLoad();
    void runningTaskReopenInterrupted();
    void requiredArtifactValidator();
    void stageProgressMapping();
    void fakeControllerCompletesPipeline();
    void cancellationStopsPipeline();
};

void ReconstructionTest::taskSerializationRoundtrip()
{
    vision3d::ReconstructionTask original;
    original.taskId = QStringLiteral("task-1");
    original.state = vision3d::ReconstructionState::Completed;
    original.stage = vision3d::ReconstructionStage::Completed;
    original.backendName = QStringLiteral("COLMAP");
    original.backendVersion = QStringLiteral("3.11.1");
    original.workspaceRelativePath = QStringLiteral("reconstruction/jobs/task-1");
    original.createdAt = QDateTime::currentDateTimeUtc();
    original.startedAt = original.createdAt;
    original.finishedAt = original.createdAt;
    original.inputImageCount = 47;
    original.registeredImageCount = 47;
    original.registrationRatio = 1.0;
    original.primaryModelRelativePath = QStringLiteral("sparse/0");
    original.fusedPointCloudRelativePath = QStringLiteral("dense/fused.ply");
    original.meshRelativePath = QStringLiteral("dense/meshed-poisson.ply");

    QString error;
    const std::optional<vision3d::ReconstructionTask> restored =
        vision3d::ReconstructionTask::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->taskId, original.taskId);
    QCOMPARE(restored->state, original.state);
    QCOMPARE(restored->stage, original.stage);
    QCOMPARE(restored->backendVersion, original.backendVersion);
    QCOMPARE(restored->workspaceRelativePath, original.workspaceRelativePath);
    QCOMPARE(restored->registeredImageCount, 47);
    QCOMPARE(restored->meshRelativePath, original.meshRelativePath);
}

void ReconstructionTest::stateEnumConversion()
{
    const QList<vision3d::ReconstructionState> states = {
        vision3d::ReconstructionState::Idle,
        vision3d::ReconstructionState::Preparing,
        vision3d::ReconstructionState::Running,
        vision3d::ReconstructionState::Completed,
        vision3d::ReconstructionState::Failed,
        vision3d::ReconstructionState::Cancelled,
        vision3d::ReconstructionState::Interrupted,
    };
    for (const auto state : states) {
        const auto restored = vision3d::reconstructionStateFromString(
            vision3d::reconstructionStateToString(state));
        QVERIFY(restored.has_value());
        QCOMPARE(*restored, state);
    }
}

void ReconstructionTest::stageEnumConversion()
{
    const QList<vision3d::ReconstructionStage> stages = {
        vision3d::ReconstructionStage::None,
        vision3d::ReconstructionStage::PreparingInput,
        vision3d::ReconstructionStage::FeatureExtraction,
        vision3d::ReconstructionStage::FeatureMatching,
        vision3d::ReconstructionStage::SparseMapping,
        vision3d::ReconstructionStage::ImageUndistortion,
        vision3d::ReconstructionStage::DenseStereo,
        vision3d::ReconstructionStage::StereoFusion,
        vision3d::ReconstructionStage::Meshing,
        vision3d::ReconstructionStage::ModelOptimization,
        vision3d::ReconstructionStage::Completed,
    };
    for (const auto stage : stages) {
        const auto restored = vision3d::reconstructionStageFromString(
            vision3d::reconstructionStageToString(stage));
        QVERIFY(restored.has_value());
        QCOMPARE(*restored, stage);
    }
}

void ReconstructionTest::taskWorkspaceCreation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ReconstructionJobPaths paths;
    QString error;
    QVERIFY2(vision3d::ReconstructionWorkspace::create(directory.path(),
                                                       QStringLiteral("task-1"),
                                                       &paths,
                                                       &error),
             qPrintable(error));
    QVERIFY(QDir(paths.inputDirectory).exists());
    QVERIFY(QDir(paths.sparseDirectory).exists());
    QVERIFY(QDir(paths.denseDirectory).exists());
    QVERIFY(QDir(paths.refinementDirectory).exists());
    QVERIFY(QDir(paths.logsDirectory).exists());
    QCOMPARE(paths.relativeRoot, QStringLiteral("reconstruction/jobs/task-1"));
    QVERIFY(!QFileInfo::exists(paths.root + QStringLiteral("/task-2")));
}

void ReconstructionTest::inputStaging()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(createProjectWithImages(manager, directory.path(), 2, &error), qPrintable(error));
    vision3d::ReconstructionJobPaths paths;
    QVERIFY(vision3d::ReconstructionWorkspace::create(manager.projectDirectory(),
                                                      QStringLiteral("stage"),
                                                      &paths,
                                                      &error));
    QList<vision3d::ReconstructionInputMapping> mappings;
    QVERIFY2(vision3d::ReconstructionWorkspace::stageInputs(paths,
                                                             manager.imageAssetRecords(),
                                                             &mappings,
                                                             &error),
             qPrintable(error));
    QCOMPARE(mappings.size(), 2);
    for (const auto& mapping : mappings) {
        QVERIFY(mapping.jobFile.startsWith(QStringLiteral("input/")));
        QVERIFY(QFileInfo(QDir(paths.root).filePath(mapping.jobFile)).isFile());
        QVERIFY(!mapping.originalFileName.isEmpty());
    }
}

void ReconstructionTest::inputManifestMapping()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    vision3d::ReconstructionJobPaths paths;
    QVERIFY(vision3d::ReconstructionWorkspace::create(manager.projectDirectory(),
                                                      QStringLiteral("manifest"),
                                                      &paths,
                                                      &error));
    QList<vision3d::ReconstructionInputMapping> mappings;
    QVERIFY(vision3d::ReconstructionWorkspace::stageInputs(paths,
                                                           manager.imageAssetRecords(),
                                                           &mappings,
                                                           &error));
    QFile file(paths.inputManifestPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(document.isArray());
    QCOMPARE(document.array().size(), 2);
    for (const QJsonValue& value : document.array()) {
        const QJsonObject object = value.toObject();
        QVERIFY(object.value(QStringLiteral("assetId")).isString());
        QVERIFY(object.value(QStringLiteral("jobFile")).toString().startsWith(
            QStringLiteral("input/")));
        QVERIFY(object.value(QStringLiteral("originalFileName")).isString());
    }
}

void ReconstructionTest::relativePathPolicy()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("backend"));
    const QString job = directory.filePath(QStringLiteral("项目/reconstruction/jobs/task"));
    QString error;
    QVERIFY(createFakeBackend(root, &error));
    QVERIFY(QDir().mkpath(job));
    const vision3d::ColmapBackend backend;
    const vision3d::ProcessCommand command = backend.featureExtractionCommand(
        root, job, vision3d::ReconstructionConfig());
    const QString arguments = command.arguments.join(QLatin1Char(' '));
    QVERIFY(arguments.contains(QStringLiteral("--database_path database.db")));
    QVERIFY(arguments.contains(QStringLiteral("--image_path input")));
    QVERIFY(!arguments.contains(QStringLiteral("database.db"), Qt::CaseSensitive)
            || !arguments.contains(QDir(job).filePath(QStringLiteral("database.db"))));
    QCOMPARE(command.workingDirectory, QDir(job).absolutePath());
}

void ReconstructionTest::missingAssetPreventsStart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    const auto records = manager.imageAssetRecords();
    QVERIFY(QFile::remove(manager.absoluteAssetPath(records.first())));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QVERIFY(!controller.start(QStringLiteral("Z:/missing"), &error));
    QVERIFY(error.contains(QStringLiteral("不存在")));
    QVERIFY(runner.startedCommands.isEmpty());
}

void ReconstructionTest::backendUnavailablePreventsStart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QVERIFY(!controller.start(QStringLiteral("Z:/missing"), &error));
    QVERIFY(error.contains(QStringLiteral("不存在")));
    QVERIFY(runner.startedCommands.isEmpty());
}

void ReconstructionTest::unverifiedBackendVersionPreventsStart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    const QString backendRoot = directory.filePath(QStringLiteral("backend"));
    QVERIFY(createFakeBackend(backendRoot, &error));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    runner.version = QStringLiteral("4.1.1");
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QVERIFY(controller.start(backendRoot, &error));
    QTRY_COMPARE_WITH_TIMEOUT(controller.currentTask().state,
                              vision3d::ReconstructionState::Failed,
                              5000);
    QVERIFY(controller.currentTask().errorMessage.contains(QStringLiteral("版本锁")));
    QCOMPARE(runner.startedCommands.size(), 1);
}

void ReconstructionTest::failedStageStopsNextStages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    const QString backendRoot = directory.filePath(QStringLiteral("backend"));
    QVERIFY(createFakeBackend(backendRoot, &error));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    runner.failCommand = QStringLiteral("mapper ");
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QVERIFY(controller.start(backendRoot, &error));
    QTRY_COMPARE_WITH_TIMEOUT(controller.currentTask().state,
                              vision3d::ReconstructionState::Failed,
                              5000);
    QVERIFY(controller.currentTask().failedStage.contains(QStringLiteral("A3")));
    QVERIFY(runner.startedCommands.last().contains(QStringLiteral("mapper")));
    QVERIFY(!runner.startedCommands.join(QLatin1Char('\n')).contains(
        QStringLiteral("image_undistorter")));
}

void ReconstructionTest::completedTaskMetadataLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    const QString backendRoot = directory.filePath(QStringLiteral("backend"));
    QVERIFY(createFakeBackend(backendRoot, &error));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QVERIFY(controller.start(backendRoot, &error));
    QTRY_COMPARE_WITH_TIMEOUT(controller.currentTask().state,
                              vision3d::ReconstructionState::Completed,
                              5000);
    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    const auto task = reopened.latestReconstructionTask(&error);
    QVERIFY2(task.has_value(), qPrintable(error));
    QCOMPARE(task->state, vision3d::ReconstructionState::Completed);
    QCOMPARE(task->meshRelativePath, QStringLiteral("dense/meshed-poisson.ply"));
}

void ReconstructionTest::runningTaskReopenInterrupted()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("running"), &error));
    vision3d::ReconstructionTask task;
    task.taskId = QStringLiteral("running-task");
    task.state = vision3d::ReconstructionState::Running;
    task.stage = vision3d::ReconstructionStage::FeatureExtraction;
    task.backendName = QStringLiteral("COLMAP");
    task.backendVersion = QStringLiteral("3.11.1");
    task.workspaceRelativePath = QStringLiteral("reconstruction/jobs/running-task");
    task.createdAt = QDateTime::currentDateTimeUtc();
    QVERIFY(manager.updateReconstructionTask(task, &error));

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    const auto restored = reopened.latestReconstructionTask(&error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->state, vision3d::ReconstructionState::Interrupted);
    QCOMPARE(reopened.currentManifest()->reconstructionState(), QStringLiteral("interrupted"));
}

void ReconstructionTest::requiredArtifactValidator()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QDir root(directory.path());
    QFile database(root.filePath(QStringLiteral("database.db")));
    QVERIFY(database.open(QIODevice::WriteOnly));
    database.write("db");
    database.close();
    QVERIFY(vision3d::ReconstructionArtifactValidator::validateDatabase(database.fileName()));

    const QDir model(root.filePath(QStringLiteral("sparse/0")));
    QVERIFY(QDir().mkpath(model.absolutePath()));
    for (const QString& name : {QStringLiteral("cameras.txt"),
                                QStringLiteral("points3D.txt")}) {
        QFile file(model.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
    }
    QFile images(model.filePath(QStringLiteral("images.txt")));
    QVERIFY(images.open(QIODevice::WriteOnly));
    images.write("1 pose\npoints\n2 pose\npoints\n");
    images.close();
    const auto selection =
        vision3d::ReconstructionArtifactValidator::selectPrimarySparseModel(
            root.filePath(QStringLiteral("sparse")));
    QVERIFY(selection.valid);
    QCOMPARE(selection.registeredImages, 2);
    QVERIFY(vision3d::ReconstructionArtifactValidator::validateUndistortion(
        root.filePath(QStringLiteral("dense")), nullptr) == false);
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("dense/images"))));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("dense/sparse"))));
    QVERIFY(vision3d::ReconstructionArtifactValidator::validateUndistortion(
        root.filePath(QStringLiteral("dense"))));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("dense/stereo/depth_maps"))));
    QFile depth(root.filePath(QStringLiteral("dense/stereo/depth_maps/x.geometric.bin")));
    QVERIFY(depth.open(QIODevice::WriteOnly));
    depth.write("depth");
    depth.close();
    QVERIFY(vision3d::ReconstructionArtifactValidator::validatePatchMatch(
        root.filePath(QStringLiteral("dense"))));
}

void ReconstructionTest::stageProgressMapping()
{
    QCOMPARE(vision3d::ReconstructionController::completedStageCount(
                  vision3d::ReconstructionStage::PreparingInput),
              0);
    QCOMPARE(vision3d::ReconstructionController::completedStageCount(
                  vision3d::ReconstructionStage::FeatureExtraction),
              1);
    QCOMPARE(vision3d::ReconstructionController::completedStageCount(
                  vision3d::ReconstructionStage::Meshing),
              7);
    QCOMPARE(vision3d::ReconstructionController::completedStageCount(
                  vision3d::ReconstructionStage::ModelOptimization),
              7);
    QCOMPARE(vision3d::ReconstructionController::completedStageCount(
                  vision3d::ReconstructionStage::Completed),
              8);
}

void ReconstructionTest::fakeControllerCompletesPipeline()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    const QString backendRoot = directory.filePath(QStringLiteral("backend"));
    QVERIFY(createFakeBackend(backendRoot, &error));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QSignalSpy finishedSpy(&controller, &vision3d::ReconstructionController::finished);
    QVERIFY(controller.start(backendRoot, &error));
    QTRY_COMPARE_WITH_TIMEOUT(controller.currentTask().state,
                              vision3d::ReconstructionState::Completed,
                              5000);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(controller.currentTask().registeredImageCount, 2);
    QVERIFY(QFileInfo(QDir(controller.jobPaths().root).filePath(
                QStringLiteral("dense/fused.ply")))
                .isFile());
    QVERIFY(QFileInfo(QDir(controller.jobPaths().root).filePath(
                QStringLiteral("dense/meshed-poisson.ply")))
                .isFile());
    QCOMPARE(runner.startedCommands.size(), 8); // probe plus A1-A7
}

void ReconstructionTest::cancellationStopsPipeline()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(createProjectWithImages(manager, directory.path(), 2, &error));
    const QString backendRoot = directory.filePath(QStringLiteral("backend"));
    QVERIFY(createFakeBackend(backendRoot, &error));
    vision3d::ColmapBackend backend;
    FakeProcessRunner runner;
    runner.holdStage = true;
    vision3d::ReconstructionController controller(&manager, &backend, &runner);
    QVERIFY(controller.start(backendRoot, &error));
    QTRY_VERIFY_WITH_TIMEOUT(controller.currentTask().stage
                                 == vision3d::ReconstructionStage::FeatureExtraction,
                             5000);
    controller.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(controller.currentTask().state,
                              vision3d::ReconstructionState::Cancelled,
                              5000);
    QCOMPARE(runner.startedCommands.size(), 2); // probe plus A1
    QVERIFY(!runner.startedCommands.join(QLatin1Char('\n')).contains(
        QStringLiteral("exhaustive_matcher")));
}

} // namespace

QTEST_MAIN(ReconstructionTest)
#include "test_reconstruction.moc"
