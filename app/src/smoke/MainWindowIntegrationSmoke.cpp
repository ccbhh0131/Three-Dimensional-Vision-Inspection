#include "smoke/MainWindowIntegrationSmoke.h"

#include "app/MainWindow.h"
#include "core/device/DeviceMarker.h"
#include "core/geometry/MarkerHitTesting.h"
#include "core/geometry/MeshPicking.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImageAssetModel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/ModelViewerWidget.h"
#include "widgets/ReconstructionPanel.h"

#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
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
    QPushButton* viewButton = findButton(window, QStringLiteral("查看三维模型"));
    QPushButton* resetButton = findButton(window, QStringLiteral("重置三维视图"));
    QPushButton* addMarkerButton = findButton(window, QStringLiteral("添加设备标记"));
    QPushButton* deleteMarkerButton = findButton(window, QStringLiteral("删除设备标记"));
    QAction* closeProjectAction = findAction(window, QStringLiteral("关闭项目"));
    if (stack == nullptr || imagePreview == nullptr || viewer == nullptr
        || reconstructionPanel == nullptr || imageBrowser == nullptr
        || viewButton == nullptr || resetButton == nullptr
        || addMarkerButton == nullptr || deleteMarkerButton == nullptr
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
            QPointF postOrbitSurfacePosition = postOrbitPickRegions.at(1);
            for (const QPointF& candidate : postOrbitPickRegions) {
                if (!MarkerHitTesting::hitTest(viewer->markerViews(),
                                               viewer->cameraController(),
                                               candidate,
                                               viewer->size(),
                                               viewer->devicePixelRatioF(),
                                               14.0)
                         .has_value()) {
                    postOrbitSurfacePosition = candidate;
                    break;
                }
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
            const PersistedMarkerSnapshot markerAfterReopen = readPersistedMarkers(projectPath);
            if (stack->currentWidget() != viewer || !viewer->hasMesh()
                || viewer->markerViews().size() != 1
                || viewer->markerViews().first().id != markerId
                || !nearlyEqual(viewer->markerViews().first().worldPosition, markerWorldPosition)
                || !markerAfterReopen.readable || markerAfterReopen.count != 1) {
                fail(QStringLiteral("project A reopen did not restore the persisted marker"));
                return;
            }
            stream << "project_a_reopen_marker=PASS\n"
                   << "project_a_reopen_marker_count=" << viewer->markerViews().size() << '\n';

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
            stream << "stale_mesh_across_projects=NO\n"
                   << "project_b_marker_count=0 PASS\n";

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
            deleteMarkerButton->click();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
            const PersistedMarkerSnapshot markerAfterDelete = readPersistedMarkers(projectPath);
            if (!markerAfterDelete.readable || markerAfterDelete.count != 0
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
