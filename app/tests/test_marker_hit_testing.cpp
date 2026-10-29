#include "core/geometry/MarkerHitTesting.h"

#include <QtTest>

namespace {

vision3d::CameraController makeCamera()
{
    vision3d::BoundingBox bounds;
    bounds.expand(QVector3D(-1.0f, -1.0f, -0.25f));
    bounds.expand(QVector3D(1.0f, 1.0f, 0.25f));
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    camera.fitToBounds(bounds);
    return camera;
}

class MarkerHitTestingTest final : public QObject
{
    Q_OBJECT

private slots:
    void worldToScreenCenter();
    void worldToScreenOutsideViewport();
    void behindCameraRejected();
    void markerHitInsideRadius();
    void markerMissOutsideRadius();
    void closestMarkerSelection();
    void highDpiLogicalCoordinate();
    void transformedMarkerPositionAndHitTest();
};

void MarkerHitTestingTest::worldToScreenCenter()
{
    const vision3d::CameraController camera = makeCamera();
    QPointF position;
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera, QVector3D(0.0f, 0.0f, 0.0f), QSize(800, 600), 1.0, position));
    QVERIFY(qAbs(position.x() - 400.0) <= 1.0e-4);
    QVERIFY(qAbs(position.y() - 300.0) <= 1.0e-4);
}

void MarkerHitTestingTest::worldToScreenOutsideViewport()
{
    const vision3d::CameraController camera = makeCamera();
    QPointF position;
    QVERIFY(!vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera, QVector3D(100.0f, 0.0f, 0.0f), QSize(800, 600), 1.0, position));
}

void MarkerHitTestingTest::behindCameraRejected()
{
    const vision3d::CameraController camera = makeCamera();
    const QVector3D offset = camera.position() - camera.target();
    QPointF position;
    QVERIFY(!vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera,
        camera.position() + offset,
        QSize(800, 600),
        1.0,
        position));
}

void MarkerHitTestingTest::markerHitInsideRadius()
{
    const vision3d::CameraController camera = makeCamera();
    const vision3d::DeviceMarkerView marker{
        QStringLiteral("marker-1"), QStringLiteral("P01"), QVector3D(0.0f, 0.0f, 0.0f), false};
    const std::optional<QString> selected = vision3d::MarkerHitTesting::hitTest(
        {marker}, camera, QPointF(407.0, 304.0), QSize(800, 600), 1.0, 14.0);
    QVERIFY(selected.has_value());
    QCOMPARE(*selected, QStringLiteral("marker-1"));
}

void MarkerHitTestingTest::markerMissOutsideRadius()
{
    const vision3d::CameraController camera = makeCamera();
    const vision3d::DeviceMarkerView marker{
        QStringLiteral("marker-1"), QStringLiteral("P01"), QVector3D(0.0f, 0.0f, 0.0f), false};
    const std::optional<QString> selected = vision3d::MarkerHitTesting::hitTest(
        {marker}, camera, QPointF(430.0, 330.0), QSize(800, 600), 1.0, 14.0);
    QVERIFY(!selected.has_value());
}

void MarkerHitTestingTest::closestMarkerSelection()
{
    const vision3d::CameraController camera = makeCamera();
    QPointF leftPosition;
    QPointF rightPosition;
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera, QVector3D(-0.05f, 0.0f, 0.0f), QSize(800, 600), 1.0, leftPosition));
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera, QVector3D(0.05f, 0.0f, 0.0f), QSize(800, 600), 1.0, rightPosition));
    const QPointF click = (leftPosition + rightPosition) * 0.5;
    const QList<vision3d::DeviceMarkerView> markers{
        {QStringLiteral("left"), QStringLiteral("P01"), QVector3D(-0.05f, 0.0f, 0.0f), false},
        {QStringLiteral("right"), QStringLiteral("P02"), QVector3D(0.05f, 0.0f, 0.0f), false},
    };
    const std::optional<QString> selected = vision3d::MarkerHitTesting::hitTest(
        markers, camera, click + QPointF(2.0, 0.0), QSize(800, 600), 1.0, 20.0);
    QVERIFY(selected.has_value());
    QCOMPARE(*selected, QStringLiteral("right"));
}

void MarkerHitTestingTest::highDpiLogicalCoordinate()
{
    const vision3d::CameraController camera = makeCamera();
    QPointF oneX;
    QPointF twoX;
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera, QVector3D(0.25f, 0.25f, 0.0f), QSize(800, 600), 1.0, oneX));
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera, QVector3D(0.25f, 0.25f, 0.0f), QSize(800, 600), 2.0, twoX));
    QVERIFY(qAbs(oneX.x() - twoX.x()) <= 1.0e-4);
    QVERIFY(qAbs(oneX.y() - twoX.y()) <= 1.0e-4);
}

void MarkerHitTestingTest::transformedMarkerPositionAndHitTest()
{
    const vision3d::CameraController camera = makeCamera();
    QMatrix4x4 model;
    model.setToIdentity();
    model.rotate(90.0f, 0.0f, 0.0f, 1.0f);

    const QVector3D rawPosition(0.5f, 0.0f, 0.0f);
    QPointF transformedPosition;
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera,
        rawPosition,
        model,
        QSize(800, 600),
        1.0,
        transformedPosition));

    QPointF expectedPosition;
    QVERIFY(vision3d::MarkerHitTesting::worldToLogicalPosition(
        camera,
        QVector3D(0.0f, 0.5f, 0.0f),
        QSize(800, 600),
        1.0,
        expectedPosition));
    QVERIFY((transformedPosition - expectedPosition).manhattanLength() <= 1.0e-4);

    const vision3d::DeviceMarkerView marker{
        QStringLiteral("aligned-marker"),
        QStringLiteral("P01"),
        rawPosition,
        false};
    const std::optional<QString> selected = vision3d::MarkerHitTesting::hitTest(
        {marker},
        camera,
        model,
        transformedPosition,
        QSize(800, 600),
        1.0,
        14.0);
    QVERIFY(selected.has_value());
    QCOMPARE(*selected, QStringLiteral("aligned-marker"));
}

} // namespace

QTEST_APPLESS_MAIN(MarkerHitTestingTest)

#include "test_marker_hit_testing.moc"
