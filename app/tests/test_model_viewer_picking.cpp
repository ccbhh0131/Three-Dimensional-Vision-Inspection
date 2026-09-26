#include "widgets/ModelViewerWidget.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QtTest>

namespace {

vision3d::MeshData makePlaneMesh()
{
    vision3d::MeshData mesh;
    mesh.primitive = vision3d::MeshPrimitive::Triangles;
    mesh.vertices = {
        {QVector3D(-1.0f, -1.0f, 0.0f)},
        {QVector3D(1.0f, -1.0f, 0.0f)},
        {QVector3D(1.0f, 1.0f, 0.0f)},
        {QVector3D(-1.0f, 1.0f, 0.0f)},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.boundingBox.reset();
    for (const vision3d::MeshVertex& vertex : mesh.vertices) {
        mesh.boundingBox.expand(vertex.position);
    }
    return mesh;
}

void sendClick(vision3d::ModelViewerWidget& viewer, const QPointF& position)
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

void sendOrbitDrag(vision3d::ModelViewerWidget& viewer)
{
    const QPointF pressPosition(100.0, 100.0);
    const QPointF releasePosition(145.0, 80.0);
    QMouseEvent press(QEvent::MouseButtonPress,
                      pressPosition,
                      pressPosition,
                      pressPosition,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &press);
    QMouseEvent move(QEvent::MouseMove,
                     releasePosition,
                     releasePosition,
                     releasePosition,
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &move);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        releasePosition,
                        releasePosition,
                        releasePosition,
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &release);
}

void sendPanDrag(vision3d::ModelViewerWidget& viewer)
{
    const QPointF pressPosition(100.0, 100.0);
    const QPointF releasePosition(135.0, 120.0);
    QMouseEvent press(QEvent::MouseButtonPress,
                      pressPosition,
                      pressPosition,
                      pressPosition,
                      Qt::MiddleButton,
                      Qt::MiddleButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &press);
    QMouseEvent move(QEvent::MouseMove,
                     releasePosition,
                     releasePosition,
                     releasePosition,
                     Qt::NoButton,
                     Qt::MiddleButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &move);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        releasePosition,
                        releasePosition,
                        releasePosition,
                        Qt::MiddleButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &release);
}

class ModelViewerPickingTest final : public QObject
{
    Q_OBJECT

private slots:
    void clickEmitsSurfaceHit();
    void dragOrbitDoesNotEmitPicking();
    void panDoesNotEmitPicking();
    void backgroundClickEmitsSafeMiss();
    void viewerWithoutMeshIsSafe();
    void markerClickHasPriorityAndSelects();
    void markerBackgroundClickClearsSelectionAndFallsBack();
};

void ModelViewerPickingTest::clickEmitsSurfaceHit()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    viewer.setMesh(makePlaneMesh());
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);

    sendClick(viewer, QPointF(100.0, 100.0));

    QCOMPARE(hitSpy.count(), 1);
    QCOMPARE(missSpy.count(), 0);
    QVERIFY(viewer.lastSurfaceHit().isValid());
    QVERIFY(qAbs(viewer.lastSurfaceHit().worldPosition.z()) <= 1.0e-5f);
}

void ModelViewerPickingTest::dragOrbitDoesNotEmitPicking()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    viewer.setMesh(makePlaneMesh());
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);
    const float initialYaw = viewer.cameraController().yawRadians();

    sendOrbitDrag(viewer);

    QCOMPARE(hitSpy.count(), 0);
    QCOMPARE(missSpy.count(), 0);
    QVERIFY(qAbs(viewer.cameraController().yawRadians() - initialYaw) > 1.0e-4f);
}

void ModelViewerPickingTest::panDoesNotEmitPicking()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    viewer.setMesh(makePlaneMesh());
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);
    const QVector3D initialTarget = viewer.cameraController().target();

    sendPanDrag(viewer);

    QCOMPARE(hitSpy.count(), 0);
    QCOMPARE(missSpy.count(), 0);
    QVERIFY((viewer.cameraController().target() - initialTarget).length() > 1.0e-4f);
}

void ModelViewerPickingTest::backgroundClickEmitsSafeMiss()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    viewer.setMesh(makePlaneMesh());
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);

    sendClick(viewer, QPointF(1.0, 1.0));

    QCOMPARE(hitSpy.count(), 0);
    QCOMPARE(missSpy.count(), 1);
    QVERIFY(!viewer.lastSurfaceHit().hit);
    QVERIFY(viewer.lastSurfaceHit().isFinite());
}

void ModelViewerPickingTest::viewerWithoutMeshIsSafe()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);

    const vision3d::SurfaceHit result = viewer.pickAt(QPointF(100.0, 100.0));

    QVERIFY(!result.hit);
    QCOMPARE(hitSpy.count(), 0);
    QCOMPARE(missSpy.count(), 1);
    QCOMPARE(static_cast<int>(result.failureReason),
             static_cast<int>(vision3d::SurfaceHitFailureReason::InvalidMesh));
}

void ModelViewerPickingTest::markerClickHasPriorityAndSelects()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    viewer.setMesh(makePlaneMesh());
    viewer.setMarkers({vision3d::DeviceMarkerView{
        QStringLiteral("marker-1"), QStringLiteral("P01"), QVector3D(0.0f, 0.0f, 0.0f), false}});
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);
    QSignalSpy markerSpy(&viewer, &vision3d::ModelViewerWidget::markerSelected);

    sendClick(viewer, QPointF(100.0, 100.0));

    QCOMPARE(hitSpy.count(), 0);
    QCOMPARE(missSpy.count(), 0);
    QCOMPARE(markerSpy.count(), 1);
    QCOMPARE(viewer.selectedMarkerId(), QStringLiteral("marker-1"));
    QVERIFY(viewer.markerViews().first().selected);
}

void ModelViewerPickingTest::markerBackgroundClickClearsSelectionAndFallsBack()
{
    vision3d::ModelViewerWidget viewer;
    viewer.resize(200, 200);
    viewer.setMesh(makePlaneMesh());
    viewer.setMarkers({vision3d::DeviceMarkerView{
        QStringLiteral("marker-1"), QStringLiteral("P01"), QVector3D(0.0f, 0.0f, 0.0f), false}});
    viewer.setSelectedMarker(QStringLiteral("marker-1"));
    QSignalSpy hitSpy(&viewer, &vision3d::ModelViewerWidget::surfacePicked);
    QSignalSpy missSpy(&viewer, &vision3d::ModelViewerWidget::surfaceMissed);
    QSignalSpy markerSpy(&viewer, &vision3d::ModelViewerWidget::markerSelected);

    sendClick(viewer, QPointF(1.0, 1.0));

    QCOMPARE(hitSpy.count(), 0);
    QCOMPARE(missSpy.count(), 1);
    QVERIFY(markerSpy.count() >= 1);
    QVERIFY(viewer.selectedMarkerId().isEmpty());
}

} // namespace

QTEST_MAIN(ModelViewerPickingTest)

#include "test_model_viewer_picking.moc"
