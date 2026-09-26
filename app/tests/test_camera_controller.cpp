#include "core/viewer/CameraController.h"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

vision3d::BoundingBox makeBounds()
{
    vision3d::BoundingBox bounds;
    bounds.expand(QVector3D(-2.0f, -1.0f, -3.0f));
    bounds.expand(QVector3D(4.0f, 5.0f, 3.0f));
    return bounds;
}

bool fuzzyVectorEqual(const QVector3D& left, const QVector3D& right, float epsilon = 1.0e-5f)
{
    return (left - right).length() <= epsilon;
}

bool finiteMatrix(const QMatrix4x4& matrix)
{
    const float* values = matrix.constData();
    for (int index = 0; index < 16; ++index) {
        if (!std::isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

class CameraControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultFiniteState();
    void fitToBounds();
    void fitZeroExtentSafe();
    void verySmallBoundsFinite();
    void veryLargeBoundsFinite();
    void orbitChangesPosition();
    void orbitPreservesDistance();
    void pitchClamp();
    void panMovesTarget();
    void panScaleDependsOnDistance();
    void zoomIn();
    void zoomOut();
    void zoomClamp();
    void resizeAspect();
    void resetFit();
    void worldProjectionRoundTrip();
    void repeatedInteractionFinite();
};

void CameraControllerTest::defaultFiniteState()
{
    vision3d::CameraController camera;
    QVERIFY(camera.isFinite());
    QVERIFY(!camera.hasBounds());
    QVERIFY(camera.distance() > 0.0f);
    QVERIFY(camera.nearPlane() > 0.0f);
    QVERIFY(camera.farPlane() > camera.nearPlane());
    QVERIFY(finiteMatrix(camera.viewMatrix()));
    QVERIFY(finiteMatrix(camera.projectionMatrix()));
}

void CameraControllerTest::fitToBounds()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(makeBounds()));
    QVERIFY(camera.hasBounds());
    QVERIFY(camera.isFinite());
    QVERIFY(fuzzyVectorEqual(camera.target(), QVector3D(1.0f, 2.0f, 0.0f)));
    QVERIFY(camera.distance() > 0.0f);
    QVERIFY(camera.nearPlane() > 0.0f);
    QVERIFY(camera.farPlane() > camera.nearPlane());
    QCOMPARE(camera.aspectRatio(), 800.0f / 600.0f);
}

void CameraControllerTest::fitZeroExtentSafe()
{
    vision3d::BoundingBox bounds;
    bounds.expand(QVector3D(3.0f, 4.0f, 5.0f));
    vision3d::CameraController camera;
    QVERIFY(!camera.fitToBounds(bounds));
    QVERIFY(!camera.hasBounds());
    QVERIFY(camera.isFinite());
}

void CameraControllerTest::verySmallBoundsFinite()
{
    vision3d::BoundingBox bounds;
    bounds.expand(QVector3D(-1.0e-7f, -2.0e-7f, -3.0e-7f));
    bounds.expand(QVector3D(1.0e-7f, 2.0e-7f, 3.0e-7f));
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(bounds));
    QVERIFY(camera.hasBounds());
    QVERIFY(camera.isFinite());
    QVERIFY(camera.distance() > 0.0f);
    QVERIFY(camera.farPlane() > camera.nearPlane());
}

void CameraControllerTest::veryLargeBoundsFinite()
{
    vision3d::BoundingBox bounds;
    bounds.expand(QVector3D(-1.0e8f, -2.0e8f, -3.0e8f));
    bounds.expand(QVector3D(1.0e8f, 2.0e8f, 3.0e8f));
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(bounds));
    QVERIFY(camera.hasBounds());
    QVERIFY(camera.isFinite());
    QVERIFY(camera.distance() > 0.0f);
    QVERIFY(camera.farPlane() > camera.nearPlane());
}

void CameraControllerTest::orbitChangesPosition()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    const QVector3D initialPosition = camera.position();
    const QVector3D initialTarget = camera.target();
    camera.orbit(40.0f, -25.0f);
    QVERIFY(camera.isFinite());
    QVERIFY(!fuzzyVectorEqual(camera.position(), initialPosition));
    QVERIFY(fuzzyVectorEqual(camera.target(), initialTarget));
}

void CameraControllerTest::orbitPreservesDistance()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    const float initialDistance = camera.distance();
    camera.orbit(500.0f, 125.0f);
    QVERIFY(qAbs(camera.distance() - initialDistance) <= 1.0e-6f);
}

void CameraControllerTest::pitchClamp()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    camera.orbit(0.0f, -1'000'000.0f);
    QVERIFY(camera.pitchRadians() <= camera.pitchLimitRadians());
    QVERIFY(camera.pitchRadians() >= -camera.pitchLimitRadians());
    camera.orbit(0.0f, 1'000'000.0f);
    QVERIFY(camera.pitchRadians() <= camera.pitchLimitRadians());
    QVERIFY(camera.pitchRadians() >= -camera.pitchLimitRadians());
    QVERIFY(camera.isFinite());
}

void CameraControllerTest::panMovesTarget()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    const QVector3D initialTarget = camera.target();
    const float initialDistance = camera.distance();
    camera.pan(40.0f, -20.0f);
    QVERIFY(camera.isFinite());
    QVERIFY(!fuzzyVectorEqual(camera.target(), initialTarget));
    QVERIFY(qAbs(camera.distance() - initialDistance) <= 1.0e-6f);
}

void CameraControllerTest::panScaleDependsOnDistance()
{
    vision3d::CameraController nearCamera;
    vision3d::CameraController farCamera;
    QVERIFY(nearCamera.fitToBounds(makeBounds()));
    QVERIFY(farCamera.fitToBounds(makeBounds()));
    farCamera.zoom(-4.0f);
    const QVector3D nearTarget = nearCamera.target();
    const QVector3D farTarget = farCamera.target();
    nearCamera.pan(25.0f, 0.0f);
    farCamera.pan(25.0f, 0.0f);
    const float nearDisplacement = (nearCamera.target() - nearTarget).length();
    const float farDisplacement = (farCamera.target() - farTarget).length();
    QVERIFY(farCamera.distance() > nearCamera.distance());
    QVERIFY(farDisplacement > nearDisplacement);
}

void CameraControllerTest::zoomIn()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    const float initialDistance = camera.distance();
    camera.zoom(1.0f);
    QVERIFY(camera.distance() < initialDistance);
    QVERIFY(camera.distance() >= camera.minimumDistance());
}

void CameraControllerTest::zoomOut()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    const float initialDistance = camera.distance();
    camera.zoom(-1.0f);
    QVERIFY(camera.distance() > initialDistance);
    QVERIFY(camera.distance() <= camera.maximumDistance());
}

void CameraControllerTest::zoomClamp()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    camera.zoom(1'000'000.0f);
    QVERIFY(qAbs(camera.distance() - camera.minimumDistance()) <= 1.0e-6f);
    camera.zoom(-1'000'000.0f);
    QVERIFY(qAbs(camera.distance() - camera.maximumDistance()) <= 1.0e-3f);
    QVERIFY(camera.isFinite());
}

void CameraControllerTest::resizeAspect()
{
    vision3d::CameraController camera;
    camera.setViewportSize(0, 0);
    QCOMPARE(camera.aspectRatio(), 1.0f);
    camera.setViewportSize(-20, 400);
    QCOMPARE(camera.aspectRatio(), 1.0f / 400.0f);
    camera.setViewportSize(1200, 600);
    QCOMPARE(camera.aspectRatio(), 2.0f);
    QVERIFY(camera.isFinite());
}

void CameraControllerTest::resetFit()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    const QVector3D initialTarget = camera.target();
    const float initialDistance = camera.distance();
    const float initialYaw = camera.yawRadians();
    const float initialPitch = camera.pitchRadians();
    camera.orbit(100.0f, 25.0f);
    camera.pan(20.0f, 10.0f);
    camera.zoom(3.0f);
    camera.resetView();
    QVERIFY(fuzzyVectorEqual(camera.target(), initialTarget));
    QVERIFY(qAbs(camera.distance() - initialDistance) <= 1.0e-6f);
    QVERIFY(qAbs(camera.yawRadians() - initialYaw) <= 1.0e-6f);
    QVERIFY(qAbs(camera.pitchRadians() - initialPitch) <= 1.0e-6f);
    QVERIFY(camera.isFinite());
}

void CameraControllerTest::worldProjectionRoundTrip()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(makeBounds()));
    const QVector3D sourcePoint(0.5f, 1.0f, 0.0f);
    QVector3D ndc;
    QVector3D reconstructed;
    QVERIFY(camera.worldToNdc(sourcePoint, ndc));
    QVERIFY(camera.ndcToWorld(ndc, reconstructed));
    QVERIFY(fuzzyVectorEqual(sourcePoint, reconstructed, 1.0e-3f));

    camera.orbit(80.0f, -35.0f);
    camera.pan(12.0f, -8.0f);
    camera.zoom(2.0f);
    QVERIFY(camera.worldToNdc(sourcePoint, ndc));
    QVERIFY(camera.ndcToWorld(ndc, reconstructed));
    QVERIFY(fuzzyVectorEqual(sourcePoint, reconstructed, 2.0e-3f));
}

void CameraControllerTest::repeatedInteractionFinite()
{
    vision3d::CameraController camera;
    QVERIFY(camera.fitToBounds(makeBounds()));
    for (int index = 0; index < 5000; ++index) {
        camera.orbit(3.0f, (index % 7) - 3.0f);
        camera.pan((index % 5) - 2.0f, (index % 3) - 1.0f);
        camera.zoom((index % 4 == 0) ? 0.25f : -0.1f);
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    camera.orbit(nan, inf);
    camera.pan(inf, nan);
    camera.zoom(nan);
    QVERIFY(camera.isFinite());
    QVERIFY(finiteMatrix(camera.viewMatrix()));
    QVERIFY(finiteMatrix(camera.projectionMatrix()));
}

QTEST_APPLESS_MAIN(CameraControllerTest)

#include "test_camera_controller.moc"
