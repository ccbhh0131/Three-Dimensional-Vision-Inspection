#include "app/MainWindow.h"

#include "backend/ReconstructionEngineLocator.h"
#include "core/device/DeviceMarker.h"
#include "core/device/GaugeAsset.h"
#include "core/inspection/InspectionRecord.h"
#include "core/project/ProjectManifest.h"
#include "core/reconstruction/ReconstructionTask.h"
#include "widgets/BackendPanel.h"
#include "widgets/ImageAssetModel.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/LogPanel.h"
#include "widgets/ModelViewerWidget.h"
#include "widgets/ReconstructionPanel.h"
#include "widgets/GaugeAssetDialog.h"
#include "widgets/GaugeStatusRuleDialog.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QImage>
#include <QInputDialog>
#include <QListView>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
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
void removeFixture(const MainWindowFixture& fixture);

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

std::optional<MainWindowFixture> makeGaugeFixture()
{
    std::optional<MainWindowFixture> fixture = makeFixture(true, false);
    if (!fixture.has_value()) {
        return std::nullopt;
    }
    QString error;
    const std::optional<vision3d::ProjectManifest> loaded =
        vision3d::ProjectManifest::load(fixture->projectPath, &error);
    if (!loaded.has_value()) {
        removeFixture(*fixture);
        return std::nullopt;
    }
    vision3d::DeviceMarker marker = vision3d::DeviceMarker::create(
        QStringLiteral("P01"), QVector3D(0.0F, 0.0F, 0.0F), QStringLiteral("test-task"));
    vision3d::ProjectManifest manifest = *loaded;
    manifest.setDeviceMarkers(QJsonArray{marker.toJson()});
    if (!manifest.save(fixture->projectPath, &error)) {
        removeFixture(*fixture);
        return std::nullopt;
    }
    return fixture;
}

std::optional<MainWindowFixture> makeGaugeHistoryFixture()
{
    std::optional<MainWindowFixture> fixture = makeGaugeFixture();
    if (!fixture.has_value()) {
        return std::nullopt;
    }

    QString error;
    const std::optional<vision3d::ProjectManifest> loaded =
        vision3d::ProjectManifest::load(fixture->projectPath, &error);
    if (!loaded.has_value() || loaded->deviceMarkers().isEmpty()) {
        removeFixture(*fixture);
        return std::nullopt;
    }
    const std::optional<vision3d::DeviceMarker> marker =
        vision3d::DeviceMarker::fromJson(loaded->deviceMarkers().first().toObject(), &error);
    if (!marker.has_value()) {
        removeFixture(*fixture);
        return std::nullopt;
    }

    vision3d::GaugeAsset gauge = vision3d::GaugeAsset::create(
        marker->id, QStringLiteral("入口压力表"), 0.0, 2.5, QStringLiteral("MPa"));
    const QDateTime manualTimestamp = QDateTime::fromString(
        QStringLiteral("2026-09-26T12:10:00.000Z"), Qt::ISODateWithMs);
    const QDateTime visualTimestamp = QDateTime::fromString(
        QStringLiteral("2026-09-26T12:20:00.000Z"), Qt::ISODateWithMs);
    gauge.latestValue = 1.24;
    gauge.latestTimestamp = visualTimestamp;
    gauge.dataSource = vision3d::GaugeDataSource::Visual;

    const vision3d::InspectionRecord manual = vision3d::InspectionRecord::create(
        gauge.id, 1.20, manualTimestamp, vision3d::GaugeDataSource::Manual);
    const vision3d::InspectionRecord visual = vision3d::InspectionRecord::create(
        gauge.id,
        1.24,
        visualTimestamp,
        vision3d::GaugeDataSource::Visual,
        QStringLiteral("asset-test"),
        QStringLiteral("images/test.png"));
    vision3d::ProjectManifest manifest = *loaded;
    manifest.setGaugeAssets(QJsonArray{gauge.toJson()});
    manifest.setInspectionRecords(QJsonArray{manual.toJson(), visual.toJson()});
    if (!manifest.save(fixture->projectPath, &error)) {
        removeFixture(*fixture);
        return std::nullopt;
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
    void gaugeAssetUiFlow();
    void realtimeMonitoringUiFlow();
    void gaugeHistoryUiFlow();
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

void MainWindowTest::gaugeAssetUiFlow()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeGaugeFixture();
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }

    vision3d::MainWindow window;
    window.resize(1280, 800);
    window.show();
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* markerLabel = window.findChild<QLabel*>(QStringLiteral("deviceMarkerDetailsLabel"));
    auto* gaugeLabel = window.findChild<QLabel*>(QStringLiteral("gaugeAssetDetailsLabel"));
    QVERIFY(panel != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(markerLabel != nullptr);
    QVERIFY(gaugeLabel != nullptr);

    panel->viewModelButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(viewer->hasMesh());
    QVERIFY(viewer->markerViews().size() == 1);
    const QString markerId = viewer->markerViews().first().id;
    viewer->setSelectedMarker(markerId);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(markerLabel->text().contains(QStringLiteral("P01")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("未绑定仪表")));
    QVERIFY(panel->createGaugeButton()->isEnabled());

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<vision3d::GaugeAssetDialog*>(
            QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeNameEdit"))
            ->setText(QStringLiteral("入口压力表"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeRangeMinEdit"))
            ->setText(QStringLiteral("0.0"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeRangeMaxEdit"))
            ->setText(QStringLiteral("2.5"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeUnitEdit"))
            ->setText(QStringLiteral("MPa"));
        auto* profileCombo = dialog->findChild<QComboBox*>(QStringLiteral("gaugeProfileCombo"));
        if (profileCombo != nullptr) {
            profileCombo->setCurrentIndex(
                profileCombo->findData(QStringLiteral("pressure_0_2_5_mpa")));
        }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    const quint64 meshUploadBeforeGauge = viewer->rendererStatus().uploadCount;
    panel->createGaugeButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("入口压力表")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("0 ~ 2.5 MPa")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("pressure_0_2_5_mpa")));
    QVERIFY(panel->editGaugeButton()->isEnabled());
    QVERIFY(panel->visualGaugeReadingButton()->isEnabled());
    QVERIFY(panel->configureGaugeStatusRuleButton()->isEnabled());
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 未知")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("状态规则: 未配置")));
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeGauge);

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<vision3d::GaugeStatusRuleDialog*>(
            QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeAlarmLowEdit"))
            ->setText(QStringLiteral("0.2"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeWarningLowEdit"))
            ->setText(QStringLiteral("0.4"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeWarningHighEdit"))
            ->setText(QStringLiteral("2.0"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeAlarmHighEdit"))
            ->setText(QStringLiteral("2.3"));
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    panel->configureGaugeStatusRuleButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 未知")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("状态规则: 已配置")));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Unknown);
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeGauge);

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<vision3d::GaugeAssetDialog*>(
            QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeNameEdit"))
            ->setText(QStringLiteral("主压力表"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeRangeMinEdit"))
            ->setText(QStringLiteral("0.0"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeRangeMaxEdit"))
            ->setText(QStringLiteral("5.0"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeUnitEdit"))
            ->setText(QStringLiteral("bar"));
        auto* profileCombo = dialog->findChild<QComboBox*>(QStringLiteral("gaugeProfileCombo"));
        if (profileCombo != nullptr) {
            profileCombo->setCurrentIndex(0);
        }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    panel->editGaugeButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("主压力表")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("0 ~ 5 bar")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("视觉 Profile: 未绑定")));
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeGauge);

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        dialog->findChild<QLineEdit*>()->setText(QStringLiteral("1.25"));
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    panel->updateGaugeReadingButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("1.25 bar")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("手动")));
    QVERIFY(!gaugeLabel->text().contains(QStringLiteral("暂无")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 正常")));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Normal);
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeGauge);

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            dialog->findChild<QLineEdit*>()->setText(QStringLiteral("2.10"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        }
    });
    panel->updateGaugeReadingButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("2.1 bar")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 警告")));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Warning);

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            dialog->findChild<QLineEdit*>()->setText(QStringLiteral("2.40"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        }
    });
    panel->updateGaugeReadingButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("2.4 bar")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 报警")));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Alarm);

    QString manifestError;
    const std::optional<vision3d::ProjectManifest> persisted =
        vision3d::ProjectManifest::load(fixture->projectPath, &manifestError);
    QVERIFY2(persisted.has_value(), qPrintable(manifestError));
    const std::optional<vision3d::GaugeAssetModel> persistedGauges =
        vision3d::GaugeAssetModel::fromJson(persisted->gaugeAssets(), &manifestError);
    QVERIFY2(persistedGauges.has_value(), qPrintable(manifestError));
    QCOMPARE(persistedGauges->size(), 1);
    QVERIFY(persistedGauges->list().first().latestTimestamp.has_value());
    QCOMPARE(persistedGauges->list().first().dataSource, vision3d::GaugeDataSource::Manual);
    QVERIFY(persistedGauges->list().first().statusRule.has_value());

    QTimer::singleShot(0, &window, []() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box != nullptr) {
            box->button(QMessageBox::Yes)->click();
        }
    });
    panel->deleteGaugeButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("未绑定仪表")));
    QVERIFY(viewer->markerViews().size() == 1);
    QVERIFY(panel->createGaugeButton()->isEnabled());

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<vision3d::GaugeAssetDialog*>(
            QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeNameEdit"))
            ->setText(QStringLiteral("级联测试表"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeRangeMinEdit"))
            ->setText(QStringLiteral("0"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeRangeMaxEdit"))
            ->setText(QStringLiteral("1"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeUnitEdit"))
            ->setText(QStringLiteral("bar"));
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    panel->createGaugeButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(panel->deleteMarkerButton()->isEnabled());

    QTimer::singleShot(0, &window, []() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box != nullptr) {
            box->button(QMessageBox::Yes)->click();
        }
    });
    panel->deleteMarkerButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(viewer->markerViews().isEmpty());
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("未绑定仪表")));
    const std::optional<vision3d::ProjectManifest> afterCascade =
        vision3d::ProjectManifest::load(fixture->projectPath, &manifestError);
    QVERIFY2(afterCascade.has_value(), qPrintable(manifestError));
    QVERIFY(afterCascade->deviceMarkers().isEmpty());
    QVERIFY(afterCascade->gaugeAssets().isEmpty());
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeGauge);
    removeFixture(*fixture);
}

void MainWindowTest::realtimeMonitoringUiFlow()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeGaugeHistoryFixture();
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }

    vision3d::MainWindow window;
    window.resize(1280, 800);
    window.show();
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* gaugeLabel = window.findChild<QLabel*>(QStringLiteral("gaugeAssetDetailsLabel"));
    auto* stateLabel = window.findChild<QLabel*>(QStringLiteral("realtimeMonitoringStateLabel"));
    auto* valueLabel = window.findChild<QLabel*>(QStringLiteral("realtimeGaugeValueLabel"));
    auto* sourceLabel = window.findChild<QLabel*>(QStringLiteral("realtimeGaugeSourceLabel"));
    auto* realtimeStatusLabel = window.findChild<QLabel*>(
        QStringLiteral("realtimeGaugeStatusLabel"));
    QVERIFY(panel != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(gaugeLabel != nullptr);
    QVERIFY(stateLabel != nullptr);
    QVERIFY(valueLabel != nullptr);
    QVERIFY(sourceLabel != nullptr);
    QVERIFY(realtimeStatusLabel != nullptr);

    panel->viewModelButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(viewer->hasMesh());
    QVERIFY(viewer->markerViews().size() == 1);
    viewer->setSelectedMarker(viewer->markerViews().first().id);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(panel->configureGaugeStatusRuleButton()->isEnabled());

    QTimer::singleShot(0, &window, []() {
        auto* dialog = qobject_cast<vision3d::GaugeStatusRuleDialog*>(
            QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeAlarmLowEdit"))
            ->setText(QStringLiteral("0.2"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeWarningLowEdit"))
            ->setText(QStringLiteral("0.4"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeWarningHighEdit"))
            ->setText(QStringLiteral("2.0"));
        dialog->findChild<QLineEdit*>(QStringLiteral("gaugeAlarmHighEdit"))
            ->setText(QStringLiteral("2.3"));
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    panel->configureGaugeStatusRuleButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("状态规则: 已配置")));

    const quint64 meshUploadBeforeRealtime = viewer->rendererStatus().uploadCount;
    QCOMPARE(panel->startMockSensorButton()->isEnabled(), true);
    QCOMPARE(panel->recordCurrentSensorSampleButton()->isEnabled(), false);
    panel->startMockSensorButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(valueLabel->text().contains(QStringLiteral("1.2 MPa")), 1000);
    QCOMPARE(stateLabel->text(), QStringLiteral("Running"));
    QCOMPARE(realtimeStatusLabel->text(), QStringLiteral("正常"));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Normal);

    QString manifestError;
    std::optional<vision3d::ProjectManifest> persisted =
        vision3d::ProjectManifest::load(fixture->projectPath, &manifestError);
    QVERIFY2(persisted.has_value(), qPrintable(manifestError));
    QCOMPARE(persisted->inspectionRecords().size(), 2);
    QVERIFY(persisted->gaugeAssets().first().toObject()
                .value(QStringLiteral("dataSource"))
                .toString()
            == QStringLiteral("visual"));

    QTRY_VERIFY_WITH_TIMEOUT(valueLabel->text().contains(QStringLiteral("2.1 MPa")), 2500);
    QCOMPARE(realtimeStatusLabel->text(), QStringLiteral("警告"));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Warning);

    QTRY_VERIFY_WITH_TIMEOUT(valueLabel->text().contains(QStringLiteral("2.4 MPa")), 2500);
    QCOMPARE(realtimeStatusLabel->text(), QStringLiteral("报警"));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Alarm);
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeRealtime);
    QVERIFY(panel->recordCurrentSensorSampleButton()->isEnabled());

    panel->recordCurrentSensorSampleButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(sourceLabel->text() == QStringLiteral("传感器"));
    persisted = vision3d::ProjectManifest::load(fixture->projectPath, &manifestError);
    QVERIFY2(persisted.has_value(), qPrintable(manifestError));
    QCOMPARE(persisted->inspectionRecords().size(), 3);
    const QJsonObject persistedGauge = persisted->gaugeAssets().first().toObject();
    QCOMPARE(persistedGauge.value(QStringLiteral("dataSource")).toString(),
             QStringLiteral("sensor"));
    QCOMPARE(persistedGauge.value(QStringLiteral("latestValue")).toDouble(), 2.4);
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeRealtime);

    panel->stopMockSensorButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QCOMPARE(stateLabel->text(), QStringLiteral("Stopped"));
    QCOMPARE(valueLabel->text(), QStringLiteral("暂无"));
    QCOMPARE(realtimeStatusLabel->text(), QStringLiteral("报警"));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("2.4 MPa")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("传感器")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 报警")));
    QCOMPARE(viewer->markerViews().first().visualState,
             vision3d::DeviceMarkerVisualState::Alarm);
    QCOMPARE(viewer->rendererStatus().uploadCount, meshUploadBeforeRealtime);
    removeFixture(*fixture);
}

void MainWindowTest::gaugeHistoryUiFlow()
{
    configureTestSettings();
    const std::optional<MainWindowFixture> fixture = makeGaugeHistoryFixture();
    QVERIFY(fixture.has_value());
    if (!fixture.has_value()) {
        return;
    }

    vision3d::MainWindow window;
    window.resize(1280, 800);
    window.show();
    QString error;
    QVERIFY2(window.openProjectPath(fixture->projectPath, &error), qPrintable(error));
    auto* panel = window.findChild<vision3d::ReconstructionPanel*>();
    auto* viewer = window.findChild<vision3d::ModelViewerWidget*>(
        QStringLiteral("modelViewerWidget"));
    auto* preview = window.findChild<vision3d::ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* gaugeLabel = window.findChild<QLabel*>(QStringLiteral("gaugeAssetDetailsLabel"));
    QVERIFY(panel != nullptr);
    QVERIFY(viewer != nullptr);
    QVERIFY(preview != nullptr);
    QVERIFY(stack != nullptr);
    QVERIFY(gaugeLabel != nullptr);

    panel->viewModelButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(viewer->hasMesh());
    QCOMPARE(viewer->markerViews().size(), 1);
    viewer->setSelectedMarker(viewer->markerViews().first().id);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("历史记录: 2")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("当前状态: 未知")));
    QVERIFY(gaugeLabel->text().contains(QStringLiteral("状态规则: 未配置")));
    QVERIFY(panel->viewGaugeHistoryButton()->isEnabled());

    bool historyDialogVerified = false;
    QTimer::singleShot(0, &window, [&window, &historyDialogVerified]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr || dialog->objectName() != QStringLiteral("inspectionHistoryDialog")) {
            return;
        }
        auto* table = dialog->findChild<QTableWidget*>(QStringLiteral("inspectionHistoryTable"));
        auto* viewImageButton = dialog->findChild<QPushButton*>(
            QStringLiteral("inspectionHistoryViewImageButton"));
        if (table == nullptr || viewImageButton == nullptr || table->rowCount() != 2
            || table->item(0, 1) == nullptr || table->item(0, 1)->text() != QStringLiteral("1.24")
            || table->item(0, 3) == nullptr
            || table->item(0, 3)->text() != QStringLiteral("视觉")
            || table->item(1, 1) == nullptr || table->item(1, 1)->text() != QStringLiteral("1.2")
            || table->item(1, 3) == nullptr
            || table->item(1, 3)->text() != QStringLiteral("手动")) {
            return;
        }
        table->selectRow(0);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!viewImageButton->isEnabled()) {
            return;
        }
        viewImageButton->click();
        historyDialogVerified = true;
        dialog->accept();
    });
    panel->viewGaugeHistoryButton()->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    QVERIFY(historyDialogVerified);
    QCOMPARE(stack->currentWidget(), preview);
    QCOMPARE(preview->currentAssetId(), QStringLiteral("asset-test"));
    QVERIFY(preview->hasLoadedImage());
    removeFixture(*fixture);
}

} // namespace

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
