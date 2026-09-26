#include "app/MainWindow.h"

#include "backend/ReconstructionEngineLocator.h"
#include "core/project/ProjectManifest.h"
#include "core/reconstruction/ReconstructionTask.h"
#include "widgets/BackendPanel.h"
#include "widgets/ImageAssetModel.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/LogPanel.h"
#include "widgets/ModelViewerWidget.h"
#include "widgets/ReconstructionPanel.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QImage>
#include <QListView>
#include <QJsonObject>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <QUuid>

#include <optional>

#include <cstring>

namespace {

struct MainWindowFixture
{
    QString root;
    QString projectPath;
};

void appendFloatLE(QByteArray& bytes, float value);
void appendInt32LE(QByteArray& bytes, qint32 value);

std::optional<MainWindowFixture> makeFixture(bool createMesh, bool corruptMesh)
{
    MainWindowFixture fixture;
    fixture.root = QDir(QDir::tempPath())
                       .filePath(QStringLiteral("main_window_%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    fixture.projectPath = QDir(fixture.root).filePath(QStringLiteral("project.json"));
    if (!QDir().mkpath(QDir(fixture.root).filePath(QStringLiteral("images")))
        || !QDir().mkpath(QDir(fixture.root).filePath(
            QStringLiteral("reconstruction/jobs/test-task/dense")))) {
        return std::nullopt;
    }

    const QString imagePath = QDir(fixture.root).filePath(QStringLiteral("images/test.png"));
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(QColor(45, 120, 200));
    if (!image.save(imagePath)) {
        QDir(fixture.root).removeRecursively();
        return std::nullopt;
    }

    vision3d::AssetRecord asset;
    asset.id = QStringLiteral("asset-test");
    asset.relativePath = QStringLiteral("images/test.png");
    asset.originalFileName = QStringLiteral("test.png");
    asset.fileSize = QFileInfo(imagePath).size();
    asset.width = image.width();
    asset.height = image.height();
    asset.sha256 = QString(64, QLatin1Char('0'));

    vision3d::ProjectManifest manifest = vision3d::ProjectManifest::createNew(
        QStringLiteral("MainWindow Fixture"));
    manifest.setImageAssetRecords({asset});

    vision3d::ReconstructionTask task;
    task.taskId = QStringLiteral("test-task");
    task.state = vision3d::ReconstructionState::Completed;
    task.stage = vision3d::ReconstructionStage::Completed;
    task.backendName = QStringLiteral("COLMAP");
    task.backendVersion = QStringLiteral("3.11.1");
    task.workspaceRelativePath = QStringLiteral("reconstruction/jobs/test-task");
    task.meshRelativePath = QStringLiteral("dense/meshed-poisson.ply");
    task.inputImageCount = 1;
    task.finishedAt = QDateTime::currentDateTimeUtc();
    manifest.setBackendVersion(task.backendVersion);
    manifest.setReconstructionState(QStringLiteral("completed"));
    manifest.setReconstructionMetadata(task.taskId, task.toJson());
    QString saveError;
    if (!manifest.save(fixture.projectPath, &saveError)) {
        QDir(fixture.root).removeRecursively();
        return std::nullopt;
    }

    if (createMesh) {
        const QString meshPath = QDir(fixture.root).filePath(
            QStringLiteral("reconstruction/jobs/test-task/dense/meshed-poisson.ply"));
        QFile mesh(meshPath);
        if (!mesh.open(QIODevice::WriteOnly)) {
            QDir(fixture.root).removeRecursively();
            return std::nullopt;
        }
        QByteArray content;
        if (corruptMesh) {
            content = QByteArrayLiteral("not a ply file\n");
        } else {
            content = QByteArrayLiteral(
                "ply\nformat binary_little_endian 1.0\n"
                "element vertex 3\n"
                "property float x\nproperty float y\nproperty float z\n"
                "element face 1\n"
                "property list uchar int vertex_indices\n"
                "end_header\n");
            appendFloatLE(content, 0.0F);
            appendFloatLE(content, 0.0F);
            appendFloatLE(content, 0.0F);
            appendFloatLE(content, 1.0F);
            appendFloatLE(content, 0.0F);
            appendFloatLE(content, 0.0F);
            appendFloatLE(content, 0.0F);
            appendFloatLE(content, 1.0F);
            appendFloatLE(content, 0.0F);
            content.append(static_cast<char>(3));
            appendInt32LE(content, 0);
            appendInt32LE(content, 1);
            appendInt32LE(content, 2);
        }
        if (mesh.write(content) != content.size()) {
            QDir(fixture.root).removeRecursively();
            return std::nullopt;
        }
    }
    return fixture;
}

void removeFixture(const MainWindowFixture& fixture)
{
    if (!fixture.root.isEmpty()) {
        QDir(fixture.root).removeRecursively();
    }
}

void configureTestSettings()
{
    const QString settingsRoot = QDir(QDir::tempPath()).filePath(
        QStringLiteral("vision3dinspector_test_settings"));
    QDir().mkpath(settingsRoot);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot);
    QSettings settings;
    settings.clear();
    vision3d::ReconstructionEngineLocator::saveDevelopmentBackendRoot(QString());
}

void appendFloatLE(QByteArray& bytes, float value)
{
    quint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(value));
    bytes.append(static_cast<char>(bits & 0xffu));
    bytes.append(static_cast<char>((bits >> 8) & 0xffu));
    bytes.append(static_cast<char>((bits >> 16) & 0xffu));
    bytes.append(static_cast<char>((bits >> 24) & 0xffu));
}

void appendInt32LE(QByteArray& bytes, qint32 value)
{
    quint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(value));
    bytes.append(static_cast<char>(bits & 0xffu));
    bytes.append(static_cast<char>((bits >> 8) & 0xffu));
    bytes.append(static_cast<char>((bits >> 16) & 0xffu));
    bytes.append(static_cast<char>((bits >> 24) & 0xffu));
}

class MainWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void windowStartsAndProbesEngine();
    void defaultPreviewStackAndViewerEntry();
    void openModelWithoutProjectFailsSafely();
    void openModelWithoutArtifactFailsSafely();
    void invalidArtifactPathFailsSafely();
    void invalidMeshFailsSafely();
    void imageViewerImageFlowKeepsLoadedMesh();
    void projectCloseClearsViewerAssociation();
};

void MainWindowTest::windowStartsAndProbesEngine()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    const QString developmentRoot =
        qEnvironmentVariable("VISION3DINSPECTOR_TEST_COLMAP_ROOT").trimmed();
    if (developmentRoot.isEmpty()) {
        QSKIP("Set VISION3DINSPECTOR_TEST_COLMAP_ROOT for the opt-in COLMAP probe.");
    }
    if (!QFileInfo(developmentRoot).isDir()
        || !QFileInfo(QDir(developmentRoot).filePath(QStringLiteral("bin/colmap.exe")))
                .isFile()) {
        QSKIP("VISION3DINSPECTOR_TEST_COLMAP_ROOT does not contain bin/colmap.exe.");
    }
    vision3d::ReconstructionEngineLocator::saveDevelopmentBackendRoot(developmentRoot);

    vision3d::MainWindow window;
    window.show();
    QVERIFY(window.isVisible());

    QPushButton* probeButton = nullptr;
    const QList<QPushButton*> buttons = window.findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == QStringLiteral("检测重建引擎")) {
            probeButton = button;
            break;
        }
    }
    QVERIFY(probeButton != nullptr);
    vision3d::BackendPanel* backendPanel = window.findChild<vision3d::BackendPanel*>();
    QVERIFY(backendPanel != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(backendPanel->isVerified(), 30000);
    QVERIFY(backendPanel->isGpuAvailable());
    const vision3d::BackendProbeResult& technical = backendPanel->lastResult();
    QCOMPARE(technical.backendName, QStringLiteral("COLMAP"));
    QCOMPARE(technical.version, QStringLiteral("3.11.1"));
    QCOMPARE(QDir::fromNativeSeparators(technical.rootPath),
             QDir::fromNativeSeparators(developmentRoot));
    QVERIFY(QDir::fromNativeSeparators(technical.executablePath)
                .endsWith(QStringLiteral("/bin/colmap.exe"), Qt::CaseInsensitive));

    bool hasReadyStatus = false;
    for (QLabel* label : window.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("就绪")) {
            hasReadyStatus = true;
        }
        QVERIFY(!label->text().contains(QStringLiteral("COLMAP")));
        QVERIFY(!label->text().contains(QStringLiteral("3.11.1")));
        QVERIFY(!label->text().contains(developmentRoot));
    }
    QVERIFY(hasReadyStatus);
    QVERIFY(window.findChild<QLineEdit*>() == nullptr);
    QVERIFY(probeButton->text() == QStringLiteral("检测重建引擎"));
}

void MainWindowTest::defaultPreviewStackAndViewerEntry()
{
    configureTestSettings();
    vision3d::MainWindow window;
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* imagePreview = window.findChild<vision3d::ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    QVERIFY(stack != nullptr);
    QVERIFY(imagePreview != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(panel != nullptr);
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(imagePreview));
    QVERIFY(!viewer->hasMesh());
    QVERIFY(!panel->viewModelButton()->isEnabled());
}

void MainWindowTest::openModelWithoutProjectFailsSafely()
{
    configureTestSettings();
    vision3d::MainWindow window;
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* log = window.findChild<vision3d::LogPanel*>();
    QVERIFY(viewer != nullptr);
    QVERIFY(log != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&window, "open3DModel", Qt::DirectConnection));
    QVERIFY(!viewer->hasMesh());
    QVERIFY(log->toPlainText().contains(QStringLiteral("No Active Project")));
}

void MainWindowTest::openModelWithoutArtifactFailsSafely()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeFixture(false, false);
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }
    vision3d::MainWindow window;
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* imagePreview = window.findChild<vision3d::ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    auto* log = window.findChild<vision3d::LogPanel*>();
    QVERIFY(stack != nullptr);
    QVERIFY(imagePreview != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(log != nullptr);
    QVERIFY(!panel->viewModelButton()->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window, "open3DModel", Qt::DirectConnection));
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(imagePreview));
    QVERIFY(!viewer->hasMesh());
    QVERIFY(log->toPlainText().contains(QStringLiteral("Poisson Mesh Missing")));
    removeFixture(*fixture);
}

void MainWindowTest::invalidArtifactPathFailsSafely()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeFixture(false, false);
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }

    QString manifestError;
    const std::optional<vision3d::ProjectManifest> loadedManifest =
        vision3d::ProjectManifest::load(fixture->projectPath, &manifestError);
    QVERIFY2(loadedManifest.has_value(), qPrintable(manifestError));
    if (!loadedManifest.has_value()) {
        removeFixture(*fixture);
        return;
    }
    QJsonObject task = loadedManifest->latestReconstructionTask();
    QJsonObject outputs = task.value(QStringLiteral("outputs")).toObject();
    outputs.insert(QStringLiteral("mesh"), QStringLiteral("../outside.ply"));
    task.insert(QStringLiteral("outputs"), outputs);
    vision3d::ProjectManifest invalidManifest = *loadedManifest;
    invalidManifest.setReconstructionMetadata(QStringLiteral("test-task"), task);
    QVERIFY2(invalidManifest.save(fixture->projectPath, &manifestError),
             qPrintable(manifestError));

    vision3d::MainWindow window;
    QString openError;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &openError), qPrintable(openError));
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* log = window.findChild<vision3d::LogPanel*>();
    QVERIFY(viewer != nullptr);
    QVERIFY(log != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&window, "open3DModel", Qt::DirectConnection));
    QVERIFY(!viewer->hasMesh());
    QVERIFY(log->toPlainText().contains(QStringLiteral("Artifact Invalid")));
    removeFixture(*fixture);
}

void MainWindowTest::invalidMeshFailsSafely()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeFixture(true, true);
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }
    vision3d::MainWindow window;
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* imagePreview = window.findChild<vision3d::ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    auto* log = window.findChild<vision3d::LogPanel*>();
    QVERIFY(stack != nullptr);
    QVERIFY(imagePreview != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(log != nullptr);
    QVERIFY(panel->viewModelButton()->isEnabled());
    panel->viewModelButton()->click();
    QTest::qWait(20);
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(imagePreview));
    QVERIFY(!viewer->hasMesh());
    QVERIFY(log->toPlainText().contains(QStringLiteral("三维模型加载失败")));
    removeFixture(*fixture);
}

void MainWindowTest::imageViewerImageFlowKeepsLoadedMesh()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeFixture(true, false);
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }
    vision3d::MainWindow window;
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* imagePreview = window.findChild<vision3d::ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    auto* browser = window.findChild<vision3d::ImageBrowserPanel*>();
    QVERIFY(stack != nullptr);
    QVERIFY(imagePreview != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(browser != nullptr);
    panel->viewModelButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(viewer->hasMesh());
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(viewer));
    const quint64 uploadCount = viewer->rendererStatus().uploadCount;
    browser->view()->setCurrentIndex(browser->model()->index(0, 0));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(imagePreview));
    QVERIFY(imagePreview->hasLoadedImage());
    panel->viewModelButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(viewer));
    QCOMPARE(viewer->rendererStatus().uploadCount, uploadCount);
    removeFixture(*fixture);
}

void MainWindowTest::projectCloseClearsViewerAssociation()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeFixture(true, false);
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }
    vision3d::MainWindow window;
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* imagePreview = window.findChild<vision3d::ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    QAction* closeAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("关闭项目")) {
            closeAction = action;
            break;
        }
    }
    QVERIFY(stack != nullptr);
    QVERIFY(imagePreview != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(closeAction != nullptr);
    panel->viewModelButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(viewer->hasMesh());
    closeAction->trigger();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(!viewer->hasMesh());
    QCOMPARE(stack->currentWidget(), static_cast<QWidget*>(imagePreview));
    QVERIFY(!panel->viewModelButton()->isEnabled());
    removeFixture(*fixture);
}

} // namespace

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
