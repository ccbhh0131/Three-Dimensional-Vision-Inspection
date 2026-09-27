#include "smoke/MainWindowIntegrationSmoke.h"

#include "app/MainWindow.h"
#include "core/device/DeviceMarker.h"
#include "core/device/GaugeAsset.h"
#include "core/inspection/InspectionRecord.h"
#include "core/geometry/MarkerHitTesting.h"
#include "core/geometry/MeshPicking.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImageAssetModel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/ModelViewerWidget.h"
#include "widgets/GaugeAssetDialog.h"
#include "widgets/GaugeStatusRuleDialog.h"
#include "widgets/ReconstructionPanel.h"
#include "widgets/VisualGaugeReadingDialog.h"

#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QStackedWidget>
#include <QTextStream>
#include <QTableWidget>
#include <QTimer>
#include <QLineEdit>
#include <QWheelEvent>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <QVector>

namespace vision3d {
namespace {

struct CaptureResult
{
    bool saved = false;
    QString path;
    QString hash;
    QSize size;
    QString error;
};

QPushButton* findButton(MainWindow& window, const QString& text)
{
    const QList<QPushButton*> buttons = window.findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

QAction* findAction(MainWindow& window, const QString& text)
{
    const QList<QAction*> actions = window.findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

QString imageHash(const QImage& image)
{
    if (image.isNull() || image.sizeInBytes() == 0) {
        return {};
    }
    const QByteArray bytes(reinterpret_cast<const char*>(image.constBits()),
                           static_cast<qsizetype>(image.sizeInBytes()));
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

struct PersistedMarkerSnapshot
{
    bool readable = false;
    int count = 0;
    QString id;
    QString reconstructionTaskId;
    QVector3D worldPosition;
};

PersistedMarkerSnapshot readPersistedMarkers(const QString& projectPath)
{
    PersistedMarkerSnapshot snapshot;
    const QString manifestPath = QFileInfo(projectPath).isDir()
        ? QDir(projectPath).filePath(QStringLiteral("project.json"))
        : projectPath;
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return snapshot;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return snapshot;
    }
    const QJsonArray markers = document.object().value(QStringLiteral("deviceMarkers"))
                                   .toArray();
    snapshot.readable = true;
    snapshot.count = markers.size();
    if (!markers.isEmpty() && markers.first().isObject()) {
        const QJsonObject marker = markers.first().toObject();
        snapshot.id = marker.value(QStringLiteral("id")).toString();
        snapshot.reconstructionTaskId = marker.value(QStringLiteral("reconstructionTaskId"))
                                           .toString();
        const QJsonObject position = marker.value(QStringLiteral("position")).toObject();
        snapshot.worldPosition = QVector3D(
            static_cast<float>(position.value(QStringLiteral("x")).toDouble()),
            static_cast<float>(position.value(QStringLiteral("y")).toDouble()),
            static_cast<float>(position.value(QStringLiteral("z")).toDouble()));
    }
    return snapshot;
}

struct PersistedGaugeSnapshot
{
    bool readable = false;
    int count = 0;
    GaugeAsset asset;
};

struct PersistedInspectionSnapshot
{
    bool readable = false;
    QList<InspectionRecord> records;
};

PersistedInspectionSnapshot readPersistedInspectionRecords(const QString& projectPath)
{
    PersistedInspectionSnapshot snapshot;
    const QString manifestPath = QFileInfo(projectPath).isDir()
        ? QDir(projectPath).filePath(QStringLiteral("project.json"))
        : projectPath;
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return snapshot;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return snapshot;
    }
    QString modelError;
    const std::optional<InspectionRecordModel> model = InspectionRecordModel::fromJson(
        document.object().value(QStringLiteral("inspectionRecords")).toArray(), &modelError);
    if (!model.has_value()) {
        return snapshot;
    }
    snapshot.readable = true;
    snapshot.records = model->all();
    std::sort(snapshot.records.begin(),
              snapshot.records.end(),
              [](const InspectionRecord& left, const InspectionRecord& right) {
                  return left.timestamp > right.timestamp;
              });
    return snapshot;
}

PersistedGaugeSnapshot readPersistedGauges(const QString& projectPath)
{
    PersistedGaugeSnapshot snapshot;
    const QString manifestPath = QFileInfo(projectPath).isDir()
        ? QDir(projectPath).filePath(QStringLiteral("project.json"))
        : projectPath;
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return snapshot;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return snapshot;
    }
    QString modelError;
    const std::optional<GaugeAssetModel> model = GaugeAssetModel::fromJson(
        document.object().value(QStringLiteral("gaugeAssets")).toArray(), &modelError);
    if (!model.has_value()) {
        return snapshot;
    }
    snapshot.readable = true;
    snapshot.count = static_cast<int>(model->size());
    if (!model->list().isEmpty()) {
        snapshot.asset = model->list().first();
    }
    return snapshot;
}

bool nearlyEqual(const QVector3D& left, const QVector3D& right, float epsilon = 1.0e-5f)
{
    return (left - right).length() <= epsilon;
}

CaptureResult captureViewer(ModelViewerWidget& viewer,
                            const QString& captureDirectory,
                            const QString& fileName)
{
    CaptureResult result;
    result.path = QDir(captureDirectory).filePath(fileName);
    const QImage image = viewer.grabFramebuffer();
    result.size = image.size();
    result.hash = imageHash(image);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        result.error = QStringLiteral("grabFramebuffer returned an empty image");
        return result;
    }
    if (!image.save(result.path)) {
        result.error = QStringLiteral("could not save %1").arg(result.path);
        return result;
    }
    result.saved = true;
    return result;
}

CaptureResult captureWindow(MainWindow& window,
                            const QString& captureDirectory,
                            const QString& fileName)
{
    CaptureResult result;
    result.path = QDir(captureDirectory).filePath(fileName);
    const QImage image = window.grab().toImage();
    result.size = image.size();
    result.hash = imageHash(image);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        result.error = QStringLiteral("MainWindow grab returned an empty image");
        return result;
    }
    if (!image.save(result.path)) {
        result.error = QStringLiteral("could not save %1").arg(result.path);
        return result;
    }
    result.saved = true;
    return result;
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return predicate();
}

void sendOrbit(ModelViewerWidget& viewer)
{
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(400.0, 300.0),
                      QPointF(400.0, 300.0),
                      QPointF(400.0, 300.0),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &press);
    QMouseEvent move(QEvent::MouseMove,
                     QPointF(465.0, 265.0),
                     QPointF(465.0, 265.0),
                     QPointF(465.0, 265.0),
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &move);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(465.0, 265.0),
                        QPointF(465.0, 265.0),
                        QPointF(465.0, 265.0),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &release);
}

void sendPan(ModelViewerWidget& viewer)
{
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(400.0, 300.0),
                      QPointF(400.0, 300.0),
                      QPointF(400.0, 300.0),
                      Qt::MiddleButton,
                      Qt::MiddleButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &press);
    QMouseEvent move(QEvent::MouseMove,
                     QPointF(435.0, 280.0),
                     QPointF(435.0, 280.0),
                     QPointF(435.0, 280.0),
                     Qt::NoButton,
                     Qt::MiddleButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &move);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(435.0, 280.0),
                        QPointF(435.0, 280.0),
                        QPointF(435.0, 280.0),
                        Qt::MiddleButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &release);
}

void sendZoom(ModelViewerWidget& viewer)
{
    QWheelEvent wheel(QPointF(400.0, 300.0),
                      QPointF(400.0, 300.0),
                      QPoint(0, 0),
                      QPoint(0, 240),
                      Qt::NoButton,
                      Qt::NoModifier,
                      Qt::NoScrollPhase,
                      false);
    QCoreApplication::sendEvent(&viewer, &wheel);
}

void sendClick(ModelViewerWidget& viewer, const QPointF& position)
{
    QMouseEvent press(QEvent::MouseButtonPress,
                      position,
                      position,
                      position,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &press);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        position,
                        position,
                        position,
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &release);
}

bool sendGaugeRoi(VisualGaugeReadingDialog& dialog,
                  const QSize& imageSize,
                  const QRect& imageRoi)
{
    QWidget* canvas = dialog.findChild<QWidget*>(QStringLiteral("visualImageCanvas"));
    if (canvas == nullptr || imageSize.isEmpty()) {
        return false;
    }
    const QSize fitted = imageSize.scaled(canvas->size(), Qt::KeepAspectRatio);
    if (fitted.isEmpty()) {
        return false;
    }
    const QRect target((canvas->width() - fitted.width()) / 2,
                      (canvas->height() - fitted.height()) / 2,
                      fitted.width(),
                      fitted.height());
    const QRect boundedRoi = imageRoi.intersected(QRect(QPoint(0, 0), imageSize));
    if (!boundedRoi.isValid() || boundedRoi.width() < 32 || boundedRoi.height() < 32) {
        return false;
    }
    const auto toWidget = [&target, &imageSize](const QPoint& imagePoint) {
        return QPoint(target.x() + imagePoint.x() * target.width() / imageSize.width(),
                      target.y() + imagePoint.y() * target.height() / imageSize.height());
    };
    const QPoint start = toWidget(boundedRoi.topLeft());
    const QPoint end = toWidget(boundedRoi.bottomRight());
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(start),
                      QPointF(start),
                      QPointF(start),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &press);
    QMouseEvent move(QEvent::MouseMove,
                     QPointF(end),
                     QPointF(end),
                     QPointF(end),
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &move);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(end),
                        QPointF(end),
                        QPointF(end),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &release);
    return true;
}

QVector<QPointF> projectedTriangleCenters(ModelViewerWidget& viewer)
{
    QVector<QPointF> candidates;
    const MeshData& mesh = viewer.meshData();
    const qsizetype triangleCount = mesh.triangleCount();
    const qsizetype stride = std::max<qsizetype>(1, triangleCount / 12000);
    const QSize viewport = viewer.size();
    const qreal devicePixelRatio = viewer.devicePixelRatioF();
    for (qsizetype triangleIndex = 0;
         triangleIndex < triangleCount;
         triangleIndex += stride) {
        const qsizetype indexOffset = triangleIndex * 3;
        const quint32 index0 = mesh.indices.at(indexOffset);
        const quint32 index1 = mesh.indices.at(indexOffset + 1);
        const quint32 index2 = mesh.indices.at(indexOffset + 2);
        if (index0 >= static_cast<quint32>(mesh.vertices.size())
            || index1 >= static_cast<quint32>(mesh.vertices.size())
            || index2 >= static_cast<quint32>(mesh.vertices.size())) {
            continue;
        }
        const QVector3D center = (
            mesh.vertices.at(index0).position
            + mesh.vertices.at(index1).position
            + mesh.vertices.at(index2).position) / 3.0f;
        QPointF logicalPosition;
        if (!MeshPicking::worldToLogicalPosition(
                viewer.cameraController(),
                center,
                viewport,
                devicePixelRatio,
                logicalPosition)) {
            continue;
        }
        if (logicalPosition.x() >= 0.0 && logicalPosition.x() <= viewport.width()
            && logicalPosition.y() >= 0.0 && logicalPosition.y() <= viewport.height()) {
            candidates.append(logicalPosition);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const QPointF& left,
                                                       const QPointF& right) {
        return left.x() < right.x();
    });
    return candidates;
}

QVector<QPointF> selectPickRegions(ModelViewerWidget& viewer)
{
    const QVector<QPointF> candidates = projectedTriangleCenters(viewer);
    if (candidates.size() < 3) {
        return {};
    }
    return {
        candidates.at(candidates.size() / 10),
        candidates.at(candidates.size() / 2),
        candidates.at((candidates.size() * 9) / 10),
    };
}

void writeHit(QTextStream& stream,
              const QString& label,
              const SurfaceHit& hit,
              double elapsedMilliseconds)
{
    stream << "pick_" << label << "=PASS"
           << " triangle=" << hit.triangleIndex
           << " world=(" << hit.worldPosition.x() << ','
           << hit.worldPosition.y() << ',' << hit.worldPosition.z() << ')'
           << " distance=" << hit.distance
           << " barycentric=(" << hit.barycentric.x() << ','
           << hit.barycentric.y() << ',' << hit.barycentric.z() << ')'
           << " elapsed_ms=" << elapsedMilliseconds << '\n';
}

} // namespace

int runMainWindowIntegrationSmoke(QApplication& application,
                                  const QString& projectPath,
                                  const QString& secondProjectPath,
                                  const QString& captureDirectory,
                                  const QString& logPath)
{
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return 2;
    }
    QTextStream stream(&logFile);
    stream << "formal_app=Vision3DInspector\n"
           << "project_path=" << projectPath << '\n'
           << "second_project_path=" << secondProjectPath << '\n'
           << "capture_directory=" << captureDirectory << '\n';

    if (!QDir().mkpath(captureDirectory)) {
        stream << "ERROR=capture directory could not be created\n";
        return 2;
    }

    MainWindow window;
    window.resize(1120, 720);
    window.show();

    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("centralPreviewStack"));
    auto* imagePreview = window.findChild<ImagePreviewWidget*>(
        QStringLiteral("imagePreviewWidget"));
    auto* viewer = window.findChild<ModelViewerWidget*>(QStringLiteral("modelViewerWidget"));
    auto* reconstructionPanel = window.findChild<ReconstructionPanel*>();
    auto* imageBrowser = window.findChild<ImageBrowserPanel*>();
    auto* gaugeLabel = window.findChild<QLabel*>(QStringLiteral("gaugeAssetDetailsLabel"));
    auto* realtimeStateLabel = window.findChild<QLabel*>(
        QStringLiteral("realtimeMonitoringStateLabel"));
    auto* realtimeValueLabel = window.findChild<QLabel*>(
        QStringLiteral("realtimeGaugeValueLabel"));
    auto* realtimeSourceLabel = window.findChild<QLabel*>(
        QStringLiteral("realtimeGaugeSourceLabel"));
    auto* realtimeStatusLabel = window.findChild<QLabel*>(
        QStringLiteral("realtimeGaugeStatusLabel"));
    QPushButton* viewButton = findButton(window, QStringLiteral("查看三维模型"));
    QPushButton* resetButton = findButton(window, QStringLiteral("重置三维视图"));
    QPushButton* addMarkerButton = findButton(window, QStringLiteral("添加设备标记"));
    QPushButton* deleteMarkerButton = findButton(window, QStringLiteral("删除设备标记"));
    QPushButton* createGaugeButton = findButton(window, QStringLiteral("创建仪表资产"));
    QPushButton* editGaugeButton = findButton(window, QStringLiteral("编辑仪表资产"));
    QPushButton* updateGaugeReadingButton = findButton(window, QStringLiteral("手动更新读数"));
    QPushButton* visualGaugeReadingButton = findButton(window, QStringLiteral("视觉读数"));
    QPushButton* viewGaugeHistoryButton = findButton(window, QStringLiteral("查看巡检历史"));
    QPushButton* configureGaugeStatusRuleButton = findButton(
        window, QStringLiteral("配置状态规则"));
    QPushButton* deleteGaugeButton = findButton(window, QStringLiteral("删除仪表资产"));
    QPushButton* startMockSensorButton = findButton(window, QStringLiteral("启动模拟数据"));
    QPushButton* stopMockSensorButton = findButton(window, QStringLiteral("停止"));
    QPushButton* recordCurrentSensorSampleButton = findButton(
        window, QStringLiteral("记录当前值"));
    QAction* closeProjectAction = findAction(window, QStringLiteral("关闭项目"));
    if (stack == nullptr || imagePreview == nullptr || viewer == nullptr
        || reconstructionPanel == nullptr || imageBrowser == nullptr
        || gaugeLabel == nullptr || realtimeStateLabel == nullptr
        || realtimeValueLabel == nullptr || realtimeSourceLabel == nullptr
        || realtimeStatusLabel == nullptr
        || viewButton == nullptr || resetButton == nullptr
        || addMarkerButton == nullptr || deleteMarkerButton == nullptr
        || createGaugeButton == nullptr || editGaugeButton == nullptr
        || updateGaugeReadingButton == nullptr || visualGaugeReadingButton == nullptr
        || viewGaugeHistoryButton == nullptr
        || configureGaugeStatusRuleButton == nullptr
        || deleteGaugeButton == nullptr
        || startMockSensorButton == nullptr || stopMockSensorButton == nullptr
        || recordCurrentSensorSampleButton == nullptr
        || closeProjectAction == nullptr) {
        stream << "ERROR=required MainWindow controls were not found\n";
        return 3;
    }

    auto success = std::make_shared<bool>(true);
    auto finish = std::make_shared<std::function<void()>>();
    auto fail = [success, &stream, &application](const QString& message) {
        *success = false;
        stream << "ERROR=" << message << '\n';
        stream.flush();
        application.exit(1);
    };
    auto pickedCount = std::make_shared<int>(0);
    auto missedCount = std::make_shared<int>(0);
    auto markerSelectedCount = std::make_shared<int>(0);
    auto lastSelectedMarkerId = std::make_shared<QString>();
    auto lastHit = std::make_shared<SurfaceHit>();
    QObject::connect(viewer,
                     &ModelViewerWidget::surfacePicked,
                     &window,
                     [pickedCount, lastHit](const SurfaceHit& hit) {
                         ++(*pickedCount);
                         *lastHit = hit;
                     });
    QObject::connect(viewer,
                     &ModelViewerWidget::surfaceMissed,
                     &window,
                     [missedCount]() { ++(*missedCount); });
    QObject::connect(viewer,
                     &ModelViewerWidget::markerSelected,
                     &window,
                     [markerSelectedCount, lastSelectedMarkerId](const QString& markerId) {
                         ++(*markerSelectedCount);
                         *lastSelectedMarkerId = markerId;
                     });

    QTimer::singleShot(500, &application, [&, success, finish, fail]() {
        QString openError;
        if (!window.openProjectPath(projectPath, &openError)) {
            fail(QStringLiteral("open test project failed: %1").arg(openError));
            return;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
        if (stack->currentWidget() != imagePreview || !viewButton->isEnabled()) {
            fail(QStringLiteral("default image preview or valid mesh entry state failed"));
            return;
        }
        stream << "default_preview=PASS\n"
               << "open_3d_model_button=ENABLED\n";
        viewButton->click();

        *finish = [&, success, finish, fail]() {
            if (!viewer->hasMesh() || stack->currentWidget() != viewer
                || !viewer->hasRenderedFrame()) {
                fail(QStringLiteral("formal viewer did not render the loaded mesh"));
                return;
            }
            const RendererStatus initialStatus = viewer->rendererStatus();
            if (!initialStatus.contextReady || !initialStatus.profileAccepted
                || !initialStatus.shaderReady || !initialStatus.meshUploaded
                || !initialStatus.drawSucceeded || initialStatus.lastGlError != 0) {
                fail(QStringLiteral("formal viewer OpenGL status is not ready"));
                return;
            }
            const quint64 initialUploadCount = initialStatus.uploadCount;
            const CaptureResult initial = captureViewer(
                *viewer, captureDirectory, QStringLiteral("main_app_3d_viewer.png"));
            if (!initial.saved) {
                fail(QStringLiteral("initial formal viewer capture failed: %1").arg(initial.error));
                return;
            }
            stream << "viewer=PASS\n"
                   << "actual_version=" << initialStatus.actualMajorVersion << '.'
                   << initialStatus.actualMinorVersion << '\n'
                   << "profile=" << initialStatus.actualProfile << '\n'
                   << "vendor=" << initialStatus.vendor << '\n'
                   << "renderer=" << initialStatus.renderer << '\n'
                   << "initial_upload_count=" << initialUploadCount << '\n'
                   << "initial_capture=" << initial.path << '\n';

            const QVector<QPointF> pickRegions = selectPickRegions(*viewer);
            if (pickRegions.size() != 3) {
                fail(QStringLiteral("could not derive three in-viewport projected triangle centers"));
                return;
            }
            const QStringList pickLabels{
                QStringLiteral("left"), QStringLiteral("center"), QStringLiteral("right")};
            QVector3D postOrbitSurfaceWorldPosition;
            for (int index = 0; index < pickRegions.size(); ++index) {
                const int previousPickedCount = *pickedCount;
                QElapsedTimer timer;
                timer.start();
                sendClick(*viewer, pickRegions.at(index));
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                const double elapsedMilliseconds = timer.nsecsElapsed() / 1000000.0;
                if (*pickedCount != previousPickedCount + 1 || !lastHit->isValid()) {
                    fail(QStringLiteral("formal %1 click did not produce a valid SurfaceHit")
                             .arg(pickLabels.at(index)));
                    return;
                }
                writeHit(stream,
                         pickLabels.at(index),
                         *lastHit,
                         elapsedMilliseconds);
                if (index == 0) {
                    postOrbitSurfaceWorldPosition = lastHit->worldPosition;
                }
            }

            QVector<double> pickingSamples;
            pickingSamples.reserve(10);
            for (int sampleIndex = 0; sampleIndex < 10; ++sampleIndex) {
                const int previousPickedCount = *pickedCount;
                QElapsedTimer timer;
                timer.start();
                sendClick(*viewer, pickRegions.at(sampleIndex % pickRegions.size()));
                QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
                const double elapsedMilliseconds = timer.nsecsElapsed() / 1000000.0;
                if (*pickedCount != previousPickedCount + 1 || !lastHit->isValid()) {
                    fail(QStringLiteral("formal timed picking sample %1 did not produce a valid SurfaceHit")
                             .arg(sampleIndex + 1));
                    return;
                }
                pickingSamples.append(elapsedMilliseconds);
            }
            QVector<double> sortedPickingSamples = pickingSamples;
            std::sort(sortedPickingSamples.begin(), sortedPickingSamples.end());
            double sumMilliseconds = 0.0;
            for (const double sample : pickingSamples) {
                sumMilliseconds += sample;
            }
            const double minimumMilliseconds = sortedPickingSamples.first();
            const double medianMilliseconds =
                (sortedPickingSamples.at(4) + sortedPickingSamples.at(5)) / 2.0;
            const double meanMilliseconds = sumMilliseconds / pickingSamples.size();
            const double p90Milliseconds = sortedPickingSamples.at(8);
            const double maximumMilliseconds = sortedPickingSamples.last();
            const bool clickMvpAcceptable = medianMilliseconds <= 250.0
                && maximumMilliseconds <= 500.0;
            const QString bruteForceStatus = clickMvpAcceptable
                ? QStringLiteral("ACCEPTABLE_FOR_CLICK_MVP")
                : QStringLiteral("BVH_REQUIRED_NEXT");
            const quint64 pickingUploadCount = viewer->rendererStatus().uploadCount;
            if (pickingUploadCount != initialUploadCount) {
                fail(QStringLiteral("CPU picking changed the GPU mesh upload count"));
                return;
            }
            stream << "picking_triangles=" << viewer->meshData().triangleCount() << '\n'
                   << "picking_samples=" << pickingSamples.size() << '\n'
                   << "picking_min_ms=" << minimumMilliseconds << '\n'
                   << "picking_median_ms=" << medianMilliseconds << '\n'
                   << "picking_mean_ms=" << meanMilliseconds << '\n'
                   << "picking_p90_ms=" << p90Milliseconds << '\n'
                   << "picking_max_ms=" << maximumMilliseconds << '\n'
                   << "BRUTE_FORCE_INTERACTIVE_STATUS=" << bruteForceStatus << '\n'
                   << "picking_upload_count=" << pickingUploadCount << '\n'
                   << "picking_gpu_reupload=NO\n";

            const int pickedBeforeMarker = *pickedCount;
            const int missedBeforeMarker = *missedCount;
            sendClick(*viewer, pickRegions.at(1));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            if (*pickedCount != pickedBeforeMarker + 1 || *missedCount != missedBeforeMarker
                || !lastHit->isValid() || !addMarkerButton->isEnabled()) {
                fail(QStringLiteral("surface hit did not enable Add Device Marker"));
                return;
            }
            const SurfaceHit markerSourceHit = *lastHit;
            stream << "marker_source_surface_hit=PASS\n"
                   << "marker_add_button=ENABLED\n";

            QTimer::singleShot(0, &application, []() {
                auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                if (dialog != nullptr) {
                    dialog->setTextValue(QStringLiteral("P01"));
                    dialog->accept();
                }
            });
            addMarkerButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);

            const PersistedMarkerSnapshot markerAfterAdd = readPersistedMarkers(projectPath);
            if (viewer->markerViews().size() != 1
                || viewer->selectedMarkerId().isEmpty()
                || !markerAfterAdd.readable
                || markerAfterAdd.count != 1
                || markerAfterAdd.id != viewer->selectedMarkerId()
                || markerAfterAdd.reconstructionTaskId.isEmpty()
                || !nearlyEqual(markerAfterAdd.worldPosition, markerSourceHit.worldPosition)
                || !deleteMarkerButton->isEnabled()) {
                fail(QStringLiteral("Add Device Marker did not create a selected persisted marker"));
                return;
            }
            const QString markerId = viewer->selectedMarkerId();
            const QVector3D markerWorldPosition = viewer->markerViews().first().worldPosition;
            const quint64 meshUploadAfterMarkerAdd = viewer->rendererStatus().uploadCount;
            const MarkerRendererStatus markerStatusAfterAdd = viewer->markerRendererStatus();
            if (meshUploadAfterMarkerAdd != initialUploadCount
                || !markerStatusAfterAdd.contextReady
                || !markerStatusAfterAdd.shaderReady
                || !markerStatusAfterAdd.drawSucceeded
                || markerStatusAfterAdd.markerCount != 1
                || markerStatusAfterAdd.uploadCount < 1
                || markerStatusAfterAdd.lastGlError != 0) {
                fail(QStringLiteral("marker renderer status or mesh upload boundary failed after add"));
                return;
            }
            const CaptureResult markerAdded = captureViewer(
                *viewer, captureDirectory, QStringLiteral("marker_added.png"));
            if (!markerAdded.saved) {
                fail(QStringLiteral("marker_added capture failed: %1").arg(markerAdded.error));
                return;
            }
            stream << "marker_added=PASS\n"
                   << "marker_id=" << markerId << '\n'
                   << "marker_name=P01\n"
                   << "marker_source_world=(" << markerSourceHit.worldPosition.x() << ','
                   << markerSourceHit.worldPosition.y() << ','
                   << markerSourceHit.worldPosition.z() << ")\n"
                   << "marker_persisted=PASS count=" << markerAfterAdd.count
                   << " task_id=" << markerAfterAdd.reconstructionTaskId << '\n'
                   << "marker_added_capture=" << markerAdded.path << '\n'
                   << "marker_upload_count_after_add=" << markerStatusAfterAdd.uploadCount << '\n'
                   << "mesh_upload_count_after_add=" << meshUploadAfterMarkerAdd << '\n';

            if (!createGaugeButton->isEnabled()
                || !gaugeLabel->text().contains(QStringLiteral("未绑定仪表"))) {
                fail(QStringLiteral("selected P01 did not expose the unbound GaugeAsset state"));
                return;
            }
            const quint64 meshUploadBeforeGauge = viewer->rendererStatus().uploadCount;
            QTimer::singleShot(0, &application, []() {
                auto* dialog = qobject_cast<GaugeAssetDialog*>(QApplication::activeModalWidget());
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
                auto* profileCombo = dialog->findChild<QComboBox*>(
                    QStringLiteral("gaugeProfileCombo"));
                if (profileCombo != nullptr) {
                    profileCombo->setCurrentIndex(
                        profileCombo->findData(QStringLiteral("pressure_0_2_5_mpa")));
                }
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
            createGaugeButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedGaugeSnapshot gaugeAfterCreate = readPersistedGauges(projectPath);
            if (!gaugeAfterCreate.readable || gaugeAfterCreate.count != 1
                || gaugeAfterCreate.asset.deviceMarkerId != markerId
                || gaugeAfterCreate.asset.name != QStringLiteral("入口压力表")
                || gaugeAfterCreate.asset.rangeMin != 0.0
                || gaugeAfterCreate.asset.rangeMax != 2.5
                || gaugeAfterCreate.asset.unit != QStringLiteral("MPa")
                || gaugeAfterCreate.asset.gaugeProfileId
                       != QStringLiteral("pressure_0_2_5_mpa")
                || gaugeAfterCreate.asset.dataSource != GaugeDataSource::None
                || !gaugeLabel->text().contains(QStringLiteral("入口压力表"))
                || !gaugeLabel->text().contains(QStringLiteral("0 ~ 2.5 MPa"))
                || !gaugeLabel->text().contains(QStringLiteral("pressure_0_2_5_mpa"))
                || !editGaugeButton->isEnabled()
                || !updateGaugeReadingButton->isEnabled()
                || !visualGaugeReadingButton->isEnabled()
                || !deleteGaugeButton->isEnabled()
                || !configureGaugeStatusRuleButton->isEnabled()
                || gaugeAfterCreate.asset.statusRule.has_value()
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 未知"))
                || !gaugeLabel->text().contains(QStringLiteral("状态规则: 未配置"))
                || viewer->rendererStatus().uploadCount != meshUploadBeforeGauge) {
                fail(QStringLiteral("Create GaugeAsset did not persist or present the bound asset"));
                return;
            }
            const CaptureResult gaugeCreated = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_asset_created.png"));
            if (!gaugeCreated.saved) {
                fail(QStringLiteral("gauge_asset_created capture failed: %1")
                         .arg(gaugeCreated.error));
                return;
            }

            QTimer::singleShot(0, &application, [&, fail]() {
                auto* dialog = qobject_cast<GaugeStatusRuleDialog*>(
                    QApplication::activeModalWidget());
                if (dialog == nullptr) {
                    fail(QStringLiteral("gauge status rule dialog did not open"));
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
                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                if (buttons == nullptr || buttons->button(QDialogButtonBox::Ok) == nullptr) {
                    fail(QStringLiteral("gauge status rule dialog buttons were not found"));
                    return;
                }
                buttons->button(QDialogButtonBox::Ok)->click();
            });
            configureGaugeStatusRuleButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedGaugeSnapshot gaugeAfterRule = readPersistedGauges(projectPath);
            if (!gaugeAfterRule.readable || gaugeAfterRule.count != 1
                || !gaugeAfterRule.asset.statusRule.has_value()
                || !gaugeAfterRule.asset.statusRule->alarmLow.has_value()
                || !gaugeAfterRule.asset.statusRule->warningLow.has_value()
                || !gaugeAfterRule.asset.statusRule->warningHigh.has_value()
                || !gaugeAfterRule.asset.statusRule->alarmHigh.has_value()
                || !qFuzzyCompare(*gaugeAfterRule.asset.statusRule->alarmLow, 0.2)
                || !qFuzzyCompare(*gaugeAfterRule.asset.statusRule->warningLow, 0.4)
                || !qFuzzyCompare(*gaugeAfterRule.asset.statusRule->warningHigh, 2.0)
                || !qFuzzyCompare(*gaugeAfterRule.asset.statusRule->alarmHigh, 2.3)
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 未知"))
                || !gaugeLabel->text().contains(QStringLiteral("状态规则: 已配置"))
                || viewer->markerViews().first().visualState != DeviceMarkerVisualState::Unknown
                || viewer->rendererStatus().uploadCount != meshUploadBeforeGauge) {
                fail(QStringLiteral("GaugeStatusRule did not persist or present the Unknown state"));
                return;
            }
            stream << "gauge_status_rule=PASS alarmLow=0.2 warningLow=0.4 warningHigh=2.0 alarmHigh=2.3\n"
                   << "gauge_status_unknown=PASS\n"
                   << "mesh_gpu_reupload_on_status_rule=NO\n";

            QTimer::singleShot(0, &application, []() {
                auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                if (dialog != nullptr) {
                    dialog->setTextValue(QStringLiteral("1.20"));
                    dialog->accept();
                }
            });
            updateGaugeReadingButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedGaugeSnapshot gaugeAfterReading = readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyAfterManual =
                readPersistedInspectionRecords(projectPath);
            if (!gaugeAfterReading.readable || gaugeAfterReading.count != 1
                || !gaugeAfterReading.asset.latestValue.has_value()
                || !qFuzzyCompare(*gaugeAfterReading.asset.latestValue, 1.20)
                || !gaugeAfterReading.asset.latestTimestamp.has_value()
                || gaugeAfterReading.asset.dataSource != GaugeDataSource::Manual
                || !gaugeLabel->text().contains(QStringLiteral("1.2 MPa"))
                || !gaugeLabel->text().contains(QStringLiteral("手动"))
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 正常"))
                || !gaugeLabel->text().contains(QStringLiteral("历史记录: 1"))
                || !historyAfterManual.readable || historyAfterManual.records.size() != 1
                || historyAfterManual.records.first().dataSource != GaugeDataSource::Manual
                || GaugeStatusEvaluator::evaluate(gaugeAfterReading.asset.latestValue,
                                                  gaugeAfterReading.asset.statusRule)
                       != GaugeStatus::Normal
                || viewer->markerViews().first().visualState != DeviceMarkerVisualState::Normal
                || !viewGaugeHistoryButton->isEnabled()
                || viewer->rendererStatus().uploadCount != meshUploadBeforeGauge) {
                fail(QStringLiteral("Manual GaugeAsset reading did not persist or present"));
                return;
            }
            const CaptureResult gaugeManualReading = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_manual_reading.png"));
            if (!gaugeManualReading.saved) {
                fail(QStringLiteral("gauge_manual_reading capture failed: %1")
                         .arg(gaugeManualReading.error));
                return;
            }
            const CaptureResult gaugeStatusNormal = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_status_normal.png"));
            if (!gaugeStatusNormal.saved) {
                fail(QStringLiteral("gauge_status_normal capture failed: %1")
                         .arg(gaugeStatusNormal.error));
                return;
            }
            stream << "gauge_asset_created=PASS\n"
                   << "gauge_id=" << gaugeAfterCreate.asset.id << '\n'
                   << "gauge_marker_id=" << gaugeAfterCreate.asset.deviceMarkerId << '\n'
                   << "gauge_name=入口压力表\n"
                   << "gauge_range=0.0~2.5 MPa\n"
                   << "gauge_data_source_after_create=None\n"
                   << "gauge_asset_created_capture=" << gaugeCreated.path << '\n'
                   << "gauge_manual_reading=PASS\n"
                   << "gauge_latest_value=" << *gaugeAfterReading.asset.latestValue << '\n'
                   << "gauge_data_source=Manual\n"
                   << "gauge_latest_timestamp="
                   << gaugeAfterReading.asset.latestTimestamp->toUTC().toString(Qt::ISODateWithMs)
                   << '\n'
                   << "gauge_manual_reading_capture=" << gaugeManualReading.path << '\n'
                   << "gauge_history_after_manual=PASS count="
                   << historyAfterManual.records.size() << '\n'
                   << "gauge_status_normal=PASS value=1.20\n"
                   << "gauge_status_normal_capture=" << gaugeStatusNormal.path << '\n'
                   << "mesh_gpu_reupload_on_gauge_change=NO\n";

            QTimer::singleShot(0, &application, []() {
                auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                if (dialog != nullptr) {
                    dialog->setTextValue(QStringLiteral("2.10"));
                    dialog->accept();
                }
            });
            updateGaugeReadingButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedGaugeSnapshot gaugeAfterWarning = readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyAfterWarning =
                readPersistedInspectionRecords(projectPath);
            if (!gaugeAfterWarning.readable || gaugeAfterWarning.count != 1
                || !gaugeAfterWarning.asset.latestValue.has_value()
                || !qFuzzyCompare(*gaugeAfterWarning.asset.latestValue, 2.10)
                || gaugeAfterWarning.asset.dataSource != GaugeDataSource::Manual
                || GaugeStatusEvaluator::evaluate(gaugeAfterWarning.asset.latestValue,
                                                  gaugeAfterWarning.asset.statusRule)
                       != GaugeStatus::Warning
                || !gaugeLabel->text().contains(QStringLiteral("2.1 MPa"))
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 警告"))
                || !historyAfterWarning.readable || historyAfterWarning.records.size() != 2
                || viewer->markerViews().first().visualState != DeviceMarkerVisualState::Warning
                || viewer->rendererStatus().uploadCount != meshUploadBeforeGauge) {
                fail(QStringLiteral("Manual 2.10 GaugeAsset reading did not present Warning"));
                return;
            }
            const CaptureResult gaugeStatusWarning = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_status_warning.png"));
            if (!gaugeStatusWarning.saved) {
                fail(QStringLiteral("gauge_status_warning capture failed: %1")
                         .arg(gaugeStatusWarning.error));
                return;
            }
            stream << "gauge_status_warning=PASS value=2.10\n"
                   << "gauge_status_warning_capture=" << gaugeStatusWarning.path << '\n';

            const QString realGaugeImagePath = qEnvironmentVariable(
                "VISION3DINSPECTOR_STAGE4D_REAL_IMAGE").trimmed();
            if (realGaugeImagePath.isEmpty() || !QFileInfo(realGaugeImagePath).isFile()) {
                fail(QStringLiteral(
                    "VISION3DINSPECTOR_STAGE4D_REAL_IMAGE is missing for visual smoke"));
                return;
            }
            const QRect realGaugeRoi(400, 1100, 2200, 2200);
            const QString visualDialogCapturePath = QDir(captureDirectory).filePath(
                QStringLiteral("visual_gauge_dialog_result.png"));
            QTimer::singleShot(0, &application, [&, fail, realGaugeImagePath, realGaugeRoi,
                                                  visualDialogCapturePath]() {
                auto* dialog = qobject_cast<VisualGaugeReadingDialog*>(
                    QApplication::activeModalWidget());
                if (dialog == nullptr) {
                    fail(QStringLiteral("visual gauge dialog did not open"));
                    return;
                }
                auto* imageCombo = dialog->findChild<QComboBox*>(
                    QStringLiteral("visualProjectImageCombo"));
                if (imageCombo == nullptr) {
                    fail(QStringLiteral("visual gauge project image combo was not found"));
                    return;
                }
                int imageIndex = imageCombo->findData(realGaugeImagePath);
                if (imageIndex < 0) {
                    const QString fileName = QFileInfo(realGaugeImagePath).fileName();
                    for (int index = 0; index < imageCombo->count(); ++index) {
                        if (imageCombo->itemText(index).contains(fileName)) {
                            imageIndex = index;
                            break;
                        }
                    }
                }
                if (imageIndex < 0) {
                    fail(QStringLiteral("Stage4A real image was not exposed in the project image list"));
                    return;
                }
                imageCombo->setCurrentIndex(imageIndex);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 750);
                const QImage realImage(realGaugeImagePath);
                if (realImage.isNull()
                    || !sendGaugeRoi(*dialog, realImage.size(), realGaugeRoi)) {
                    fail(QStringLiteral("could not select the Stage4A real-image ROI"));
                    return;
                }
                auto* analyzeButton = dialog->findChild<QPushButton*>(
                    QStringLiteral("visualAnalyzeButton"));
                auto* resultLabel = dialog->findChild<QLabel*>(
                    QStringLiteral("visualReadingResultLabel"));
                auto* confirmButton = dialog->findChild<QPushButton*>(
                    QStringLiteral("visualConfirmButton"));
                if (analyzeButton == nullptr || resultLabel == nullptr || confirmButton == nullptr) {
                    fail(QStringLiteral("visual gauge result controls were not found"));
                    return;
                }
                analyzeButton->click();
                QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);
                if (!resultLabel->text().contains(QStringLiteral("成功"))) {
                    fail(QStringLiteral("Stage4A real-image visual analysis failed: %1")
                             .arg(resultLabel->text()));
                    return;
                }
                const QImage dialogCapture = dialog->grab().toImage();
                if (dialogCapture.isNull() || !dialogCapture.save(visualDialogCapturePath)) {
                    fail(QStringLiteral("visual gauge dialog result capture failed"));
                    return;
                }
                confirmButton->click();
            });
            visualGaugeReadingButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1500);
            if (!*success) {
                return;
            }
            const PersistedGaugeSnapshot gaugeAfterVisual = readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyAfterVisual =
                readPersistedInspectionRecords(projectPath);
            const QString visualOverlayPath = QDir(captureDirectory).filePath(
                QStringLiteral("visual_gauge_overlay_%1.png").arg(gaugeAfterCreate.asset.id));
            if (!gaugeAfterVisual.readable || gaugeAfterVisual.count != 1
                || gaugeAfterVisual.asset.gaugeProfileId
                       != QStringLiteral("pressure_0_2_5_mpa")
                || !gaugeAfterVisual.asset.latestValue.has_value()
                || *gaugeAfterVisual.asset.latestValue < gaugeAfterVisual.asset.rangeMin
                || *gaugeAfterVisual.asset.latestValue > gaugeAfterVisual.asset.rangeMax
                || std::abs(*gaugeAfterVisual.asset.latestValue - 2.44548) > 0.02
                || gaugeAfterVisual.asset.dataSource != GaugeDataSource::Visual
                || !gaugeAfterVisual.asset.latestTimestamp.has_value()
                || !gaugeLabel->text().contains(QStringLiteral("视觉"))
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 报警"))
                || !gaugeLabel->text().contains(QStringLiteral("历史记录: 3"))
                || !historyAfterVisual.readable || historyAfterVisual.records.size() != 3
                || historyAfterVisual.records.first().dataSource != GaugeDataSource::Visual
                || historyAfterVisual.records.first().imageAssetId.isEmpty()
                || GaugeStatusEvaluator::evaluate(gaugeAfterVisual.asset.latestValue,
                                                  gaugeAfterVisual.asset.statusRule)
                       != GaugeStatus::Alarm
                || viewer->markerViews().first().visualState != DeviceMarkerVisualState::Alarm
                || !QFileInfo::exists(visualOverlayPath)
                || !QFileInfo::exists(visualDialogCapturePath)
                || viewer->rendererStatus().uploadCount != meshUploadBeforeGauge) {
                fail(QStringLiteral("Visual GaugeAsset reading did not persist or present"));
                return;
            }
            const CaptureResult gaugeVisualReading = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_visual_reading.png"));
            if (!gaugeVisualReading.saved) {
                fail(QStringLiteral("gauge_visual_reading capture failed: %1")
                         .arg(gaugeVisualReading.error));
                return;
            }
            const CaptureResult gaugeStatusAlarm = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_status_alarm.png"));
            if (!gaugeStatusAlarm.saved) {
                fail(QStringLiteral("gauge_status_alarm capture failed: %1")
                         .arg(gaugeStatusAlarm.error));
                return;
            }
            stream << "gauge_visual_reading=PASS\n"
                   << "gauge_visual_value=" << *gaugeAfterVisual.asset.latestValue << '\n'
                   << "gauge_status_alarm=PASS value=" << *gaugeAfterVisual.asset.latestValue << '\n'
                   << "gauge_visual_angle_not_persisted=YES\n"
                   << "gauge_data_source_after_visual=Visual\n"
                   << "gauge_history_after_visual=PASS count="
                   << historyAfterVisual.records.size() << '\n'
                   << "gauge_visual_image_asset_id="
                   << historyAfterVisual.records.first().imageAssetId << '\n'
                   << "gauge_visual_overlay=" << visualOverlayPath << '\n'
                   << "gauge_visual_dialog_capture=" << visualDialogCapturePath << '\n'
                   << "gauge_visual_reading_capture=" << gaugeVisualReading.path << '\n'
                   << "gauge_status_alarm_capture=" << gaugeStatusAlarm.path << '\n';

            bool historyUiPass = false;
            QTimer::singleShot(0, &application, [&historyUiPass]() {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (dialog == nullptr
                    || dialog->objectName() != QStringLiteral("inspectionHistoryDialog")) {
                    return;
                }
                auto* table = dialog->findChild<QTableWidget*>(
                    QStringLiteral("inspectionHistoryTable"));
                auto* viewImageButton = dialog->findChild<QPushButton*>(
                    QStringLiteral("inspectionHistoryViewImageButton"));
                if (table == nullptr || viewImageButton == nullptr || table->rowCount() != 3) {
                    return;
                }
                table->selectRow(0);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                if (!viewImageButton->isEnabled()) {
                    return;
                }
                viewImageButton->click();
                historyUiPass = true;
                dialog->accept();
            });
            viewGaugeHistoryButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (!historyUiPass || stack->currentWidget() != imagePreview
                || imagePreview->currentAssetId().isEmpty() || !imagePreview->hasLoadedImage()) {
                fail(QStringLiteral("Gauge history UI did not open the associated visual image"));
                return;
            }
            stream << "gauge_history_ui=PASS rows=3\n"
                   << "gauge_visual_record_image_preview=PASS asset_id="
                   << imagePreview->currentAssetId() << '\n';
            viewButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 150);

            viewer->setSelectedMarker(markerId);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            const quint64 meshUploadBeforeRealtime = viewer->rendererStatus().uploadCount;
            const PersistedGaugeSnapshot persistedBeforeRealtime =
                readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyBeforeRealtime =
                readPersistedInspectionRecords(projectPath);
            if (stack->currentWidget() != viewer || !viewer->hasMesh()
                || viewer->selectedMarkerId() != markerId
                || !persistedBeforeRealtime.readable
                || persistedBeforeRealtime.asset.dataSource != GaugeDataSource::Visual
                || !persistedBeforeRealtime.asset.latestValue.has_value()
                || std::abs(*persistedBeforeRealtime.asset.latestValue - 2.44548) > 0.02
                || !historyBeforeRealtime.readable
                || historyBeforeRealtime.records.size() != 3
                || realtimeStateLabel->text() != QStringLiteral("Stopped")
                || realtimeValueLabel->text() != QStringLiteral("暂无")) {
                fail(QStringLiteral("realtime monitoring did not start from the persisted visual snapshot"));
                return;
            }

            startMockSensorButton->click();
            if (!waitUntil([&]() {
                    return realtimeStateLabel->text() == QStringLiteral("Running")
                        && realtimeValueLabel->text().contains(QStringLiteral("1.2 MPa"))
                        && realtimeStatusLabel->text() == QStringLiteral("正常")
                        && viewer->markerViews().first().visualState
                               == DeviceMarkerVisualState::Normal;
                },
                1500)) {
                fail(QStringLiteral("Mock sensor did not produce the 1.20 Normal state"));
                return;
            }
            const PersistedGaugeSnapshot persistedAfterRealtimeNormal =
                readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyAfterRealtimeNormal =
                readPersistedInspectionRecords(projectPath);
            if (!persistedAfterRealtimeNormal.readable
                || persistedAfterRealtimeNormal.asset.dataSource != GaugeDataSource::Visual
                || !persistedAfterRealtimeNormal.asset.latestValue.has_value()
                || std::abs(*persistedAfterRealtimeNormal.asset.latestValue - 2.44548) > 0.02
                || !historyAfterRealtimeNormal.readable
                || historyAfterRealtimeNormal.records.size() != 3
                || viewer->rendererStatus().uploadCount != meshUploadBeforeRealtime) {
                fail(QStringLiteral("1.20 realtime sample changed persisted history or mesh upload"));
                return;
            }
            const CaptureResult realtimeNormal = captureWindow(
                window, captureDirectory, QStringLiteral("realtime_normal.png"));
            if (!realtimeNormal.saved) {
                fail(QStringLiteral("realtime_normal capture failed: %1")
                         .arg(realtimeNormal.error));
                return;
            }

            if (!waitUntil([&]() {
                    return realtimeValueLabel->text().contains(QStringLiteral("2.1 MPa"))
                        && realtimeStatusLabel->text() == QStringLiteral("警告")
                        && viewer->markerViews().first().visualState
                               == DeviceMarkerVisualState::Warning;
                },
                3000)) {
                fail(QStringLiteral("Mock sensor did not produce the 2.10 Warning state"));
                return;
            }
            const CaptureResult realtimeWarning = captureWindow(
                window, captureDirectory, QStringLiteral("realtime_warning.png"));
            if (!realtimeWarning.saved) {
                fail(QStringLiteral("realtime_warning capture failed: %1")
                         .arg(realtimeWarning.error));
                return;
            }

            if (!waitUntil([&]() {
                    return realtimeValueLabel->text().contains(QStringLiteral("2.4 MPa"))
                        && realtimeStatusLabel->text() == QStringLiteral("报警")
                        && viewer->markerViews().first().visualState
                               == DeviceMarkerVisualState::Alarm;
                },
                3000)) {
                fail(QStringLiteral("Mock sensor did not produce the 2.40 Alarm state"));
                return;
            }
            const CaptureResult realtimeAlarm = captureWindow(
                window, captureDirectory, QStringLiteral("realtime_alarm.png"));
            if (!realtimeAlarm.saved) {
                fail(QStringLiteral("realtime_alarm capture failed: %1")
                         .arg(realtimeAlarm.error));
                return;
            }
            if (!recordCurrentSensorSampleButton->isEnabled()
                || viewer->rendererStatus().uploadCount != meshUploadBeforeRealtime) {
                fail(QStringLiteral("realtime alarm state did not enable snapshot recording"));
                return;
            }

            recordCurrentSensorSampleButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedGaugeSnapshot gaugeAfterSensor = readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyAfterSensor =
                readPersistedInspectionRecords(projectPath);
            if (!gaugeAfterSensor.readable || gaugeAfterSensor.count != 1
                || !gaugeAfterSensor.asset.latestValue.has_value()
                || std::abs(*gaugeAfterSensor.asset.latestValue - 2.4) > 1.0e-9
                || gaugeAfterSensor.asset.dataSource != GaugeDataSource::Sensor
                || GaugeStatusEvaluator::evaluate(gaugeAfterSensor.asset.latestValue,
                                                  gaugeAfterSensor.asset.statusRule)
                       != GaugeStatus::Alarm
                || !historyAfterSensor.readable || historyAfterSensor.records.size() != 4
                || historyAfterSensor.records.first().dataSource != GaugeDataSource::Sensor
                || std::abs(historyAfterSensor.records.first().value - 2.4) > 1.0e-9
                || !historyAfterSensor.records.first().imageAssetId.isEmpty()
                || !historyAfterSensor.records.first().sourceImagePath.isEmpty()
                || realtimeSourceLabel->text() != QStringLiteral("传感器")
                || viewer->rendererStatus().uploadCount != meshUploadBeforeRealtime) {
                fail(QStringLiteral("Record current sensor sample did not persist a Sensor snapshot"));
                return;
            }
            const CaptureResult sensorHistoryRecord = captureWindow(
                window, captureDirectory, QStringLiteral("sensor_history_record.png"));
            if (!sensorHistoryRecord.saved) {
                fail(QStringLiteral("sensor_history_record capture failed: %1")
                         .arg(sensorHistoryRecord.error));
                return;
            }

            bool sensorHistoryUiPass = false;
            QTimer::singleShot(0, &application, [&sensorHistoryUiPass]() {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (dialog == nullptr
                    || dialog->objectName() != QStringLiteral("inspectionHistoryDialog")) {
                    return;
                }
                auto* table = dialog->findChild<QTableWidget*>(
                    QStringLiteral("inspectionHistoryTable"));
                auto* detailLabel = dialog->findChild<QLabel*>(
                    QStringLiteral("inspectionHistoryDetailLabel"));
                auto* viewImageButton = dialog->findChild<QPushButton*>(
                    QStringLiteral("inspectionHistoryViewImageButton"));
                if (table == nullptr || detailLabel == nullptr || viewImageButton == nullptr
                    || table->rowCount() != 4 || table->item(0, 1) == nullptr
                    || table->item(0, 3) == nullptr) {
                    return;
                }
                table->selectRow(0);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                sensorHistoryUiPass = table->item(0, 1)->text().startsWith(QStringLiteral("2.4"))
                    && table->item(0, 3)->text() == QStringLiteral("传感器")
                    && detailLabel->text().contains(QStringLiteral("传感器"))
                    && !viewImageButton->isEnabled();
                dialog->accept();
            });
            viewGaugeHistoryButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (!sensorHistoryUiPass || stack->currentWidget() != viewer) {
                fail(QStringLiteral("Sensor history UI did not show the newest Sensor row"));
                return;
            }

            stopMockSensorButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (realtimeStateLabel->text() != QStringLiteral("Stopped")
                || realtimeValueLabel->text() != QStringLiteral("暂无")
                || realtimeSourceLabel->text() != QStringLiteral("-")
                || realtimeStatusLabel->text() != QStringLiteral("报警")
                || !gaugeLabel->text().contains(QStringLiteral("2.4 MPa"))
                || !gaugeLabel->text().contains(QStringLiteral("传感器"))
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 报警"))
                || viewer->markerViews().first().visualState != DeviceMarkerVisualState::Alarm
                || viewer->rendererStatus().uploadCount != meshUploadBeforeRealtime) {
                fail(QStringLiteral("Stop did not clear live state and fall back to Sensor persistence"));
                return;
            }
            stream << "realtime_sequence=PASS 1.20->Normal 2.10->Warning 2.40->Alarm\n"
                   << "automatic_inspection_record_per_sample=NO\n"
                   << "realtime_normal_capture=" << realtimeNormal.path << '\n'
                   << "realtime_warning_capture=" << realtimeWarning.path << '\n'
                   << "realtime_alarm_capture=" << realtimeAlarm.path << '\n'
                   << "sensor_snapshot=PASS value=2.40 history_count="
                   << historyAfterSensor.records.size() << '\n'
                   << "sensor_history_record_capture=" << sensorHistoryRecord.path << '\n'
                   << "sensor_history_ui=PASS rows=4 source=Sensor\n"
                   << "stop_clears_live_state=PASS\n"
                   << "persisted_fallback=PASS source=Sensor value=2.40\n"
                   << "mesh_gpu_reupload_on_realtime_status=NO\n";

            viewer->setSelectedMarker(QString());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            if (!viewer->selectedMarkerId().isEmpty() || deleteMarkerButton->isEnabled()) {
                fail(QStringLiteral("marker deselection did not clear the delete action"));
                return;
            }

            const int pickedBeforeOrbit = *pickedCount;
            const int missedBeforeOrbit = *missedCount;
            sendOrbit(*viewer);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (*pickedCount != pickedBeforeOrbit || *missedCount != missedBeforeOrbit) {
                fail(QStringLiteral("left drag unexpectedly emitted a picking result"));
                return;
            }
            stream << "orbit_no_pick=PASS\n";
            const CaptureResult orbit = captureViewer(
                *viewer, captureDirectory, QStringLiteral("main_app_3d_viewer_orbit.png"));
            if (!orbit.saved || orbit.hash == initial.hash) {
                fail(QStringLiteral("formal Orbit capture did not change"));
                return;
            }
            if (viewer->markerViews().size() != 1
                || !nearlyEqual(viewer->markerViews().first().worldPosition,
                                markerWorldPosition)) {
                fail(QStringLiteral("marker did not remain anchored to source world coordinates after orbit"));
                return;
            }
            const CaptureResult markerOrbit = captureViewer(
                *viewer, captureDirectory, QStringLiteral("marker_orbit.png"));
            if (!markerOrbit.saved || markerOrbit.hash == markerAdded.hash) {
                fail(QStringLiteral("marker_orbit capture did not change from marker_added"));
                return;
            }
            const MarkerRendererStatus markerStatusAfterOrbit = viewer->markerRendererStatus();
            if (!markerStatusAfterOrbit.drawSucceeded || markerStatusAfterOrbit.lastGlError != 0) {
                fail(QStringLiteral("marker renderer failed after orbit"));
                return;
            }
            stream << "orbit=PASS\n"
                   << "marker_orbit=PASS\n"
                   << "marker_world_anchoring=PASS\n"
                   << "marker_orbit_capture=" << markerOrbit.path << '\n';

            const QVector<QPointF> postOrbitPickRegions = selectPickRegions(*viewer);
            if (postOrbitPickRegions.size() != 3) {
                fail(QStringLiteral("could not derive post-orbit pick centers"));
                return;
            }

            QPointF markerScreenPosition;
            if (!MarkerHitTesting::worldToLogicalPosition(viewer->cameraController(),
                                                          markerWorldPosition,
                                                          viewer->size(),
                                                          viewer->devicePixelRatioF(),
                                                          markerScreenPosition)) {
                fail(QStringLiteral("marker source world did not project into the post-orbit viewport"));
                return;
            }
            const int pickedBeforeMarkerSelect = *pickedCount;
            const int missedBeforeMarkerSelect = *missedCount;
            const int markerSignalsBeforeSelect = *markerSelectedCount;
            sendClick(*viewer, markerScreenPosition);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            if (*pickedCount != pickedBeforeMarkerSelect
                || *missedCount != missedBeforeMarkerSelect
                || *markerSelectedCount != markerSignalsBeforeSelect + 1
                || *lastSelectedMarkerId != markerId
                || viewer->selectedMarkerId() != markerId) {
                fail(QStringLiteral("marker click did not have priority over mesh picking"));
                return;
            }
            const CaptureResult markerSelected = captureViewer(
                *viewer, captureDirectory, QStringLiteral("marker_selected.png"));
            if (!markerSelected.saved) {
                fail(QStringLiteral("marker_selected capture failed: %1").arg(markerSelected.error));
                return;
            }
            stream << "marker_screen_hit_test=PASS\n"
                   << "marker_click_priority=PASS\n"
                   << "marker_selected=PASS\n"
                   << "marker_selected_capture=" << markerSelected.path << '\n';

            const quint64 meshUploadBeforeMarkerCounts = viewer->rendererStatus().uploadCount;
            const quint64 markerUploadBeforeCounts = viewer->markerRendererStatus().uploadCount;
            for (const int markerCount : {1, 10, 100}) {
                QVector<DeviceMarkerView> performanceMarkers;
                performanceMarkers.reserve(markerCount);
                for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
                    DeviceMarkerView performanceMarker;
                    performanceMarker.id = QStringLiteral("performance-%1").arg(markerIndex);
                    performanceMarker.label = performanceMarker.id;
                    performanceMarker.worldPosition = markerWorldPosition
                        + QVector3D(static_cast<float>(markerIndex % 10) * 0.001F,
                                    static_cast<float>(markerIndex / 10) * 0.001F,
                                    0.0F);
                    performanceMarkers.append(performanceMarker);
                }
                viewer->setMarkers(performanceMarkers);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                (void)viewer->grabFramebuffer();
                const MarkerRendererStatus performanceStatus = viewer->markerRendererStatus();
                if (performanceStatus.markerCount != markerCount
                    || !performanceStatus.drawSucceeded
                    || performanceStatus.lastGlError != 0) {
                    fail(QStringLiteral("marker performance count %1 did not render").arg(markerCount));
                    return;
                }
                stream << "marker_performance_" << markerCount << "=PASS upload_count="
                       << performanceStatus.uploadCount << '\n';
            }
            viewer->setMarkers(viewer->markerViews());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            viewer->setMarkers({DeviceMarkerView{markerId,
                                                 QStringLiteral("P01"),
                                                 markerWorldPosition,
                                                 false}});
            viewer->setSelectedMarker(markerId);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            const MarkerRendererStatus markerStatusAfterCounts = viewer->markerRendererStatus();
            if (viewer->rendererStatus().uploadCount != meshUploadBeforeMarkerCounts
                || markerStatusAfterCounts.uploadCount <= markerUploadBeforeCounts
                || markerStatusAfterCounts.markerCount != 1
                || !markerStatusAfterCounts.drawSucceeded
                || markerStatusAfterCounts.lastGlError != 0) {
                fail(QStringLiteral("marker count changes crossed the mesh upload boundary"));
                return;
            }
            stream << "marker_performance_counts=1,10,100 PASS\n"
                   << "marker_mesh_upload_before=" << meshUploadBeforeMarkerCounts << '\n'
                   << "marker_mesh_upload_after=" << viewer->rendererStatus().uploadCount << '\n'
                   << "marker_upload_count_before=" << markerUploadBeforeCounts << '\n'
                   << "marker_upload_count_after=" << markerStatusAfterCounts.uploadCount << '\n'
                   << "mesh_gpu_reupload_on_marker_change=NO\n";

            const int previousPickedAfterOrbit = *pickedCount;
            QPointF postOrbitSurfacePosition;
            bool postOrbitSurfacePositionFound = false;
            const auto tryPostOrbitSurfacePosition = [&](const QPointF& candidate) {
                if (MarkerHitTesting::hitTest(viewer->markerViews(),
                                               viewer->cameraController(),
                                               candidate,
                                               viewer->size(),
                                               viewer->devicePixelRatioF(),
                                               14.0)
                    .has_value()) {
                    return false;
                }
                Ray ray;
                if (!MeshPicking::screenToWorldRay(viewer->cameraController(),
                                                   candidate,
                                                   viewer->size(),
                                                   viewer->devicePixelRatioF(),
                                                   ray)) {
                    return false;
                }
                return MeshPicking::pick(viewer->meshData(), ray).isValid();
            };

            QPointF projectedAnchorPosition;
            if (MeshPicking::worldToLogicalPosition(viewer->cameraController(),
                                                    postOrbitSurfaceWorldPosition,
                                                    viewer->size(),
                                                    viewer->devicePixelRatioF(),
                                                    projectedAnchorPosition)
                && tryPostOrbitSurfacePosition(projectedAnchorPosition)) {
                postOrbitSurfacePosition = projectedAnchorPosition;
                postOrbitSurfacePositionFound = true;
            }
            if (!postOrbitSurfacePositionFound) {
                const QVector<QPointF> postOrbitCandidates = projectedTriangleCenters(*viewer);
                const qsizetype sampleCount = std::min<qsizetype>(128, postOrbitCandidates.size());
                for (qsizetype sampleIndex = 0;
                     sampleIndex < sampleCount;
                     ++sampleIndex) {
                    const qsizetype candidateIndex = sampleCount <= 1
                        ? 0
                        : (sampleIndex * (postOrbitCandidates.size() - 1))
                            / (sampleCount - 1);
                    const QPointF candidate = postOrbitCandidates.at(candidateIndex);
                    if (tryPostOrbitSurfacePosition(candidate)) {
                        postOrbitSurfacePosition = candidate;
                        postOrbitSurfacePositionFound = true;
                        break;
                    }
                }
            }
            if (!postOrbitSurfacePositionFound) {
                fail(QStringLiteral("could not derive a post-orbit surface pick position"));
                return;
            }
            sendClick(*viewer, postOrbitSurfacePosition);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            if (*pickedCount != previousPickedAfterOrbit + 1 || !lastHit->isValid()) {
                fail(QStringLiteral("post-orbit click did not produce a valid SurfaceHit"));
                return;
            }
            writeHit(stream, QStringLiteral("post_orbit"), *lastHit, 0.0);
            stream << "post_orbit_surface_hit=PASS\n";

            const int pickedBeforePan = *pickedCount;
            const int missedBeforePan = *missedCount;
            sendPan(*viewer);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (*pickedCount != pickedBeforePan || *missedCount != missedBeforePan) {
                fail(QStringLiteral("middle-button pan unexpectedly emitted a picking result"));
                return;
            }
            const CaptureResult pan = captureViewer(
                *viewer, captureDirectory, QStringLiteral("main_app_3d_viewer_pan.png"));
            if (!pan.saved || pan.hash == initial.hash) {
                fail(QStringLiteral("formal Pan capture did not change"));
                return;
            }
            stream << "pan=PASS\n";

            const int pickedBeforeZoom = *pickedCount;
            const int missedBeforeZoom = *missedCount;
            sendZoom(*viewer);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (*pickedCount != pickedBeforeZoom || *missedCount != missedBeforeZoom) {
                fail(QStringLiteral("wheel zoom unexpectedly emitted a picking result"));
                return;
            }
            const CaptureResult zoom = captureViewer(
                *viewer, captureDirectory, QStringLiteral("main_app_3d_viewer_zoom.png"));
            if (!zoom.saved || zoom.hash == initial.hash) {
                fail(QStringLiteral("formal Zoom capture did not change"));
                return;
            }
            stream << "zoom=PASS\n";

            resetButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (stack->currentWidget() != viewer) {
                fail(QStringLiteral("formal Reset View did not keep the viewer active"));
                return;
            }
            stream << "reset=PASS\n";

            imageBrowser->view()->setCurrentIndex(imageBrowser->model()->index(0, 0));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (stack->currentWidget() != imagePreview || !imagePreview->hasLoadedImage()) {
                fail(QStringLiteral("3D to image preview switch failed"));
                return;
            }
            viewButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
            const quint64 afterReturnUploadCount = viewer->rendererStatus().uploadCount;
            if (stack->currentWidget() != viewer || !viewer->hasMesh()
                || afterReturnUploadCount != initialUploadCount) {
                fail(QStringLiteral("image to 3D return reloaded the same mesh"));
                return;
            }
            stream << "image_to_3d_to_image_to_3d=PASS\n";

            for (int index = 0; index < 10; ++index) {
                imageBrowser->view()->setCurrentIndex(imageBrowser->model()->index(0, 0));
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                viewButton->click();
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
                if (stack->currentWidget() != viewer || !viewer->hasMesh()
                    || viewer->rendererStatus().uploadCount != initialUploadCount) {
                    fail(QStringLiteral("repeated image/viewer switch changed GPU upload count at iteration %1")
                             .arg(index));
                    return;
                }
            }
            stream << "repeated_switches=10 PASS\n"
                   << "same_mesh_reupload=NO\n";

            closeProjectAction->trigger();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (viewer->hasMesh() || stack->currentWidget() != imagePreview
                || viewButton->isEnabled() || !viewer->markerViews().isEmpty()
                || !viewer->selectedMarkerId().isEmpty() || deleteMarkerButton->isEnabled()) {
                fail(QStringLiteral("project close did not clear viewer association"));
                return;
            }
            stream << "project_close_clears_viewer=YES\n"
                   << "project_close_clears_markers=YES\n";

            QString reopenAError;
            if (!window.openProjectPath(projectPath, &reopenAError)) {
                fail(QStringLiteral("reopen project A failed: %1").arg(reopenAError));
                return;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (!viewButton->isEnabled()) {
                fail(QStringLiteral("reopened project A did not restore its mesh entry"));
                return;
            }
            viewButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
            viewer->setSelectedMarker(markerId);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            const PersistedMarkerSnapshot markerAfterReopen = readPersistedMarkers(projectPath);
            const PersistedGaugeSnapshot gaugeAfterReopen = readPersistedGauges(projectPath);
            const PersistedInspectionSnapshot historyAfterReopen =
                readPersistedInspectionRecords(projectPath);
            if (stack->currentWidget() != viewer || !viewer->hasMesh()
                || viewer->markerViews().size() != 1
                || viewer->markerViews().first().id != markerId
                || !nearlyEqual(viewer->markerViews().first().worldPosition, markerWorldPosition)
                || !markerAfterReopen.readable || markerAfterReopen.count != 1
                || !gaugeAfterReopen.readable || gaugeAfterReopen.count != 1
                || gaugeAfterReopen.asset.id != gaugeAfterCreate.asset.id
                || gaugeAfterReopen.asset.deviceMarkerId != markerId
                || gaugeAfterReopen.asset.name != QStringLiteral("入口压力表")
                || gaugeAfterReopen.asset.gaugeProfileId
                       != QStringLiteral("pressure_0_2_5_mpa")
                || !gaugeAfterReopen.asset.latestValue.has_value()
                || std::abs(*gaugeAfterReopen.asset.latestValue - 2.4) > 1.0e-9
                || gaugeAfterReopen.asset.dataSource != GaugeDataSource::Sensor
                || !gaugeAfterReopen.asset.statusRule.has_value()
                || GaugeStatusEvaluator::evaluate(gaugeAfterReopen.asset.latestValue,
                                                  gaugeAfterReopen.asset.statusRule)
                       != GaugeStatus::Alarm
                || !historyAfterReopen.readable || historyAfterReopen.records.size() != 4
                || historyAfterReopen.records.first().dataSource != GaugeDataSource::Sensor
                || std::abs(historyAfterReopen.records.first().value - 2.4) > 1.0e-9
                || !historyAfterReopen.records.first().imageAssetId.isEmpty()
                || !gaugeLabel->text().contains(QStringLiteral("入口压力表"))
                || !gaugeLabel->text().contains(QStringLiteral("传感器"))
                || !gaugeLabel->text().contains(QStringLiteral("当前状态: 报警"))
                || !gaugeLabel->text().contains(QStringLiteral("状态规则: 已配置"))
                || realtimeStateLabel->text() != QStringLiteral("Stopped")
                || realtimeValueLabel->text() != QStringLiteral("暂无")
                || realtimeSourceLabel->text() != QStringLiteral("-")
                || viewer->markerViews().first().visualState != DeviceMarkerVisualState::Alarm) {
                fail(QStringLiteral("project A reopen did not restore the persisted marker and GaugeAsset"));
                return;
            }
            const CaptureResult gaugeAfterReopenCapture = captureWindow(
                window, captureDirectory, QStringLiteral("gauge_after_reopen.png"));
            if (!gaugeAfterReopenCapture.saved) {
                fail(QStringLiteral("gauge_after_reopen capture failed: %1")
                         .arg(gaugeAfterReopenCapture.error));
                return;
            }
            const CaptureResult statusAfterReopen = captureWindow(
                window, captureDirectory, QStringLiteral("status_after_reopen.png"));
            if (!statusAfterReopen.saved) {
                fail(QStringLiteral("status_after_reopen capture failed: %1")
                         .arg(statusAfterReopen.error));
                return;
            }
            const CaptureResult realtimeAfterReopen = captureWindow(
                window, captureDirectory, QStringLiteral("realtime_after_reopen.png"));
            if (!realtimeAfterReopen.saved) {
                fail(QStringLiteral("realtime_after_reopen capture failed: %1")
                         .arg(realtimeAfterReopen.error));
                return;
            }
            stream << "project_a_reopen_marker=PASS\n"
                   << "project_a_reopen_marker_count=" << viewer->markerViews().size() << '\n'
                   << "project_a_reopen_gauge=PASS\n"
                   << "project_a_reopen_gauge_count=" << gaugeAfterReopen.count << '\n'
                   << "project_a_reopen_history_count=" << historyAfterReopen.records.size()
                   << '\n'
                   << "project_a_reopen_sensor_fallback=PASS value=2.40\n"
                   << "gauge_after_reopen_capture=" << gaugeAfterReopenCapture.path << '\n'
                   << "status_after_reopen=PASS\n"
                   << "status_after_reopen_capture=" << statusAfterReopen.path << '\n'
                   << "realtime_after_reopen_capture=" << realtimeAfterReopen.path << '\n';

            closeProjectAction->trigger();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);

            QString secondError;
            if (!window.openProjectPath(secondProjectPath, &secondError)) {
                fail(QStringLiteral("open second project failed: %1").arg(secondError));
                return;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            if (viewer->hasMesh() || stack->currentWidget() != imagePreview
                || viewButton->isEnabled() || !viewer->markerViews().isEmpty()
                || !viewer->selectedMarkerId().isEmpty()) {
                fail(QStringLiteral("old project mesh remained visible after project switch"));
                return;
            }
            if (!gaugeLabel->text().contains(QStringLiteral("未绑定仪表"))) {
                fail(QStringLiteral("project B retained the old project GaugeAsset presentation"));
                return;
            }
            stream << "stale_mesh_across_projects=NO\n"
                   << "project_b_marker_count=0 PASS\n"
                   << "project_b_gauge_presentation=NONE\n";

            closeProjectAction->trigger();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            QString finalReopenError;
            if (!window.openProjectPath(projectPath, &finalReopenError)) {
                fail(QStringLiteral("final reopen project A failed: %1").arg(finalReopenError));
                return;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            viewButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
            if (!viewer->hasMesh() || viewer->markerViews().size() != 1) {
                fail(QStringLiteral("final project A reopen did not restore the marker before delete"));
                return;
            }
            QPointF finalMarkerScreenPosition;
            if (!MarkerHitTesting::worldToLogicalPosition(viewer->cameraController(),
                                                          markerWorldPosition,
                                                          viewer->size(),
                                                          viewer->devicePixelRatioF(),
                                                          finalMarkerScreenPosition)) {
                fail(QStringLiteral("final marker did not project for delete selection"));
                return;
            }
            sendClick(*viewer, finalMarkerScreenPosition);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            if (viewer->selectedMarkerId() != markerId || !deleteMarkerButton->isEnabled()) {
                fail(QStringLiteral("final marker selection for delete failed"));
                return;
            }
            const quint64 meshUploadBeforeDelete = viewer->rendererStatus().uploadCount;
            QTimer::singleShot(0, &application, []() {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (box != nullptr) {
                    box->button(QMessageBox::Yes)->click();
                }
            });
            deleteMarkerButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedMarkerSnapshot markerAfterDelete = readPersistedMarkers(projectPath);
            const PersistedGaugeSnapshot gaugeAfterDelete = readPersistedGauges(projectPath);
            if (!markerAfterDelete.readable || markerAfterDelete.count != 0
                || !gaugeAfterDelete.readable || gaugeAfterDelete.count != 0
                || !viewer->markerViews().isEmpty() || !viewer->selectedMarkerId().isEmpty()
                || deleteMarkerButton->isEnabled()
                || viewer->rendererStatus().uploadCount != meshUploadBeforeDelete) {
                fail(QStringLiteral("Delete Device Marker did not clear persistence and presentation"));
                return;
            }
            const MarkerRendererStatus finalMarkerStatus = viewer->markerRendererStatus();
            if (!finalMarkerStatus.contextReady || !finalMarkerStatus.shaderReady
                || !finalMarkerStatus.drawSucceeded || finalMarkerStatus.lastGlError != 0) {
                fail(QStringLiteral("final marker renderer OpenGL status is not clean"));
                return;
            }
            stream << "project_a_reopen_before_delete=PASS\n"
                   << "marker_deleted=PASS\n"
                   << "marker_persisted_after_delete=PASS count=" << markerAfterDelete.count << '\n'
                   << "gauge_cascade_after_marker_delete=PASS count=" << gaugeAfterDelete.count
                   << '\n'
                   << "mesh_upload_count_after_delete=" << viewer->rendererStatus().uploadCount << '\n'
                   << "marker_gl_error=0x" << Qt::hex << finalMarkerStatus.lastGlError << Qt::dec << '\n'
                   << "final_upload_count=" << viewer->rendererStatus().uploadCount << '\n'
                   << "gl_error=0x" << Qt::hex << initialStatus.lastGlError << Qt::dec << '\n'
                   << "FORMAL_APP_SMOKE=PASS\n";
            stream.flush();
            application.exit(0);
        };

        QTimer::singleShot(1800, &application, [finish]() { (*finish)(); });
    });

    const int result = application.exec();
    logFile.flush();
    return result == 0 && *success ? 0 : 1;
}

} // namespace vision3d
