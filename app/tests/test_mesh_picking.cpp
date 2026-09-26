#include "core/geometry/MeshPicking.h"

#include <QPointF>
#include <QSize>
#include <QtTest>

#include <cmath>
#include <limits>

namespace {

vision3d::MeshData makePlane(float z = 0.0f)
{
    vision3d::MeshData mesh;
    mesh.primitive = vision3d::MeshPrimitive::Triangles;
    mesh.vertices = {
        {QVector3D(-1.0f, -1.0f, z)},
        {QVector3D(1.0f, -1.0f, z)},
        {QVector3D(1.0f, 1.0f, z)},
        {QVector3D(-1.0f, 1.0f, z)},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.boundingBox.reset();
    for (const vision3d::MeshVertex& vertex : mesh.vertices) {
        mesh.boundingBox.expand(vertex.position);
    }
    return mesh;
}

vision3d::MeshData makeSourceWorldPlane()
{
    vision3d::MeshData mesh;
    mesh.primitive = vision3d::MeshPrimitive::Triangles;
    mesh.vertices = {
        {QVector3D(12.0f, -7.0f, 25.0f)},
        {QVector3D(13.0f, -7.0f, 25.0f)},
        {QVector3D(13.0f, -6.0f, 25.0f)},
        {QVector3D(12.0f, -6.0f, 25.0f)},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.boundingBox.reset();
    for (const vision3d::MeshVertex& vertex : mesh.vertices) {
        mesh.boundingBox.expand(vertex.position);
    }
    return mesh;
}

vision3d::MeshData makeTwoPlaneMesh()
{
    vision3d::MeshData mesh;
    mesh.primitive = vision3d::MeshPrimitive::Triangles;
    mesh.vertices = {
        {QVector3D(-1.0f, -1.0f, 0.0f)},
        {QVector3D(1.0f, -1.0f, 0.0f)},
        {QVector3D(1.0f, 1.0f, 0.0f)},
        {QVector3D(-1.0f, 1.0f, 0.0f)},
        {QVector3D(-1.0f, -1.0f, 2.0f)},
        {QVector3D(1.0f, -1.0f, 2.0f)},
        {QVector3D(1.0f, 1.0f, 2.0f)},
        {QVector3D(-1.0f, 1.0f, 2.0f)},
    };
    // The far plane is triangle 0, the front plane is triangle 2.
    mesh.indices = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6};
    mesh.boundingBox.reset();
    for (const vision3d::MeshVertex& vertex : mesh.vertices) {
        mesh.boundingBox.expand(vertex.position);
    }
    return mesh;
}

bool fuzzyVectorEqual(const QVector3D& left,
                      const QVector3D& right,
                      float epsilon = 1.0e-4f)
{
    return (left - right).length() <= epsilon;
}

class MeshPickingTest final : public QObject
{
    Q_OBJECT

private slots:
    void rayNormalizesAndRejectsInvalid();
    void rayTriangleCenterHit();
    void rayTriangleMiss();
    void behindRayRejected();
    void parallelRayRejected();
    void edgeHit();
    void vertexHit();
    void degenerateTriangleSafe();
    void frontmostOfTwoTriangles();
    void barycentricAndWorldPosition();
    void invalidIndexSafe();
    void centerPixelRay();
    void cornerPixelRay();
    void cameraOrbitRay();
    void cameraPanRay();
    void cameraZoomRay();
    void resizeRay();
    void projectionUnprojectionConsistency();
    void highDpiLogicalToRenderConsistency();
    void sourceWorldCoordinatePreserved();
    void finiteResult();
};

void MeshPickingTest::rayNormalizesAndRejectsInvalid()
{
    const std::optional<vision3d::Ray> ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(1.0f, 2.0f, 3.0f),
        QVector3D(0.0f, 0.0f, -4.0f));
    QVERIFY(ray.has_value());
    QVERIFY(ray->isValid());
    QCOMPARE(ray->origin, QVector3D(1.0f, 2.0f, 3.0f));
    QVERIFY(qAbs(ray->direction.length() - 1.0f) <= 1.0e-6f);

    QVERIFY(!vision3d::Ray::fromOriginAndDirection(
                  QVector3D(0.0f, 0.0f, 0.0f),
                  QVector3D(0.0f, 0.0f, 0.0f))
                 .has_value());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    QVERIFY(!vision3d::Ray::fromOriginAndDirection(
                  QVector3D(nan, 0.0f, 0.0f),
                  QVector3D(0.0f, 0.0f, -1.0f))
                 .has_value());
}

void MeshPickingTest::rayTriangleCenterHit()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.25f, 0.25f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    double distance = 0.0;
    double u = 0.0;
    double v = 0.0;
    QVERIFY(vision3d::MeshPicking::intersectTriangle(
        *ray,
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        distance,
        u,
        v));
    QVERIFY(qAbs(distance - 1.0) <= 1.0e-9);
    QVERIFY(qAbs(u - 0.25) <= 1.0e-9);
    QVERIFY(qAbs(v - 0.25) <= 1.0e-9);
}

void MeshPickingTest::rayTriangleMiss()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(1.5f, 1.5f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    double distance = 0.0;
    double u = 0.0;
    double v = 0.0;
    QVERIFY(!vision3d::MeshPicking::intersectTriangle(
        *ray,
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        distance,
        u,
        v));
}

void MeshPickingTest::behindRayRejected()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.25f, 0.25f, -1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    double distance = 0.0;
    double u = 0.0;
    double v = 0.0;
    QVERIFY(!vision3d::MeshPicking::intersectTriangle(
        *ray,
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        distance,
        u,
        v));
}

void MeshPickingTest::parallelRayRejected()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.25f, 0.25f, 1.0f), QVector3D(1.0f, 0.0f, 0.0f));
    QVERIFY(ray.has_value());
    double distance = 0.0;
    double u = 0.0;
    double v = 0.0;
    QVERIFY(!vision3d::MeshPicking::intersectTriangle(
        *ray,
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        distance,
        u,
        v));
}

void MeshPickingTest::edgeHit()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.5f, 0.5f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    const vision3d::MeshData mesh = makePlane();
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, *ray);
    QVERIFY(hit.isValid());
    QVERIFY(qAbs(hit.barycentric.x() + hit.barycentric.y()
                 + hit.barycentric.z() - 1.0f) <= 1.0e-5f);
}

void MeshPickingTest::vertexHit()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(-1.0f, -1.0f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    const vision3d::MeshData mesh = makePlane();
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, *ray);
    QVERIFY(hit.isValid());
    QVERIFY(fuzzyVectorEqual(hit.worldPosition, QVector3D(-1.0f, -1.0f, 0.0f)));
}

void MeshPickingTest::degenerateTriangleSafe()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    double distance = 0.0;
    double u = 0.0;
    double v = 0.0;
    QVERIFY(!vision3d::MeshPicking::intersectTriangle(
        *ray,
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(2.0f, 0.0f, 0.0f),
        distance,
        u,
        v));
}

void MeshPickingTest::frontmostOfTwoTriangles()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(
        makeTwoPlaneMesh(), *ray);
    QVERIFY(hit.isValid());
    QCOMPARE(hit.triangleIndex, static_cast<quint32>(2));
    QVERIFY(qAbs(hit.distance - 3.0f) <= 1.0e-5f);
}

void MeshPickingTest::barycentricAndWorldPosition()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.25f, 0.25f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    vision3d::MeshData mesh;
    mesh.primitive = vision3d::MeshPrimitive::Triangles;
    mesh.vertices = {
        {QVector3D(0.0f, 0.0f, 0.0f)},
        {QVector3D(1.0f, 0.0f, 0.0f)},
        {QVector3D(0.0f, 1.0f, 0.0f)},
    };
    mesh.indices = {0, 1, 2};
    mesh.boundingBox.reset();
    for (const vision3d::MeshVertex& vertex : mesh.vertices) {
        mesh.boundingBox.expand(vertex.position);
    }
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, *ray);
    QVERIFY(hit.isValid());
    QVERIFY(qAbs(hit.barycentric.x() - 0.25f) <= 1.0e-5f);
    QVERIFY(qAbs(hit.barycentric.y() - 0.25f) <= 1.0e-5f);
    QVERIFY(qAbs(hit.barycentric.z() - 0.5f) <= 1.0e-5f);
    QVERIFY(qAbs(hit.barycentric.x() + hit.barycentric.y()
                 + hit.barycentric.z() - 1.0f) <= 1.0e-5f);
    QVERIFY(fuzzyVectorEqual(hit.worldPosition, QVector3D(0.25f, 0.25f, 0.0f)));
}

void MeshPickingTest::invalidIndexSafe()
{
    vision3d::MeshData mesh = makePlane();
    mesh.indices[0] = 999999u;
    mesh.indices[3] = 999999u;
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, *ray);
    QVERIFY(!hit.hit);
    QCOMPARE(static_cast<int>(hit.failureReason),
             static_cast<int>(vision3d::SurfaceHitFailureReason::InvalidMesh));
    QVERIFY(hit.isFinite());
}

void MeshPickingTest::centerPixelRay()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    const vision3d::MeshData mesh = makePlane();
    QVERIFY(camera.fitToBounds(mesh.boundingBox));
    vision3d::Ray ray;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, ray));
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, ray);
    QVERIFY(hit.isValid());
    QVERIFY(fuzzyVectorEqual(hit.worldPosition, camera.target(), 1.0e-3f));
}

void MeshPickingTest::cornerPixelRay()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(makePlane().boundingBox));
    vision3d::Ray ray;
    vision3d::SurfaceHitFailureReason reason = vision3d::SurfaceHitFailureReason::None;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(0.0, 0.0), QSize(800, 600), 1.0, ray, &reason));
    QVERIFY(ray.isValid());
    QCOMPARE(static_cast<int>(reason),
             static_cast<int>(vision3d::SurfaceHitFailureReason::None));
}

void MeshPickingTest::cameraOrbitRay()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(makePlane().boundingBox));
    vision3d::Ray before;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, before));
    camera.orbit(45.0f, -20.0f);
    vision3d::Ray after;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, after));
    QVERIFY(!fuzzyVectorEqual(before.direction, after.direction, 1.0e-3f));
}

void MeshPickingTest::cameraPanRay()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(makePlane().boundingBox));
    vision3d::Ray before;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, before));
    camera.pan(40.0f, -20.0f);
    vision3d::Ray after;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, after));
    QVERIFY(!fuzzyVectorEqual(before.origin, after.origin, 1.0e-3f));
}

void MeshPickingTest::cameraZoomRay()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(makePlane().boundingBox));
    vision3d::Ray before;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, before));
    camera.zoom(2.0f);
    vision3d::Ray after;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, after));
    QVERIFY(!fuzzyVectorEqual(before.origin, after.origin, 1.0e-3f));
    QVERIFY(fuzzyVectorEqual(before.direction, after.direction, 1.0e-3f));
}

void MeshPickingTest::resizeRay()
{
    vision3d::CameraController camera;
    camera.setViewportSize(1200, 600);
    const vision3d::MeshData mesh = makePlane();
    QVERIFY(camera.fitToBounds(mesh.boundingBox));
    vision3d::Ray ray;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(600.0, 300.0), QSize(1200, 600), 1.0, ray));
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, ray);
    QVERIFY(hit.isValid());
    QVERIFY(fuzzyVectorEqual(hit.worldPosition, camera.target(), 1.0e-3f));
}

void MeshPickingTest::projectionUnprojectionConsistency()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    const vision3d::MeshData mesh = makeSourceWorldPlane();
    QVERIFY(camera.fitToBounds(mesh.boundingBox));
    const QVector3D point(12.25f, -6.75f, 25.0f);
    QPointF logicalPosition;
    QVERIFY(vision3d::MeshPicking::worldToLogicalPosition(
        camera, point, QSize(800, 600), 1.0, logicalPosition));
    vision3d::Ray ray;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, logicalPosition, QSize(800, 600), 1.0, ray));
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, ray);
    QVERIFY(hit.isValid());
    QVERIFY(fuzzyVectorEqual(hit.worldPosition, point, 2.0e-3f));
}

void MeshPickingTest::highDpiLogicalToRenderConsistency()
{
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    const vision3d::MeshData mesh = makePlane();
    QVERIFY(camera.fitToBounds(mesh.boundingBox));
    const QPointF logicalPosition(287.5, 221.25);
    vision3d::Ray reference;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, logicalPosition, QSize(800, 600), 1.0, reference));
    for (const qreal dpr : {1.25, 1.5, 2.0}) {
        vision3d::Ray highDpiRay;
        QVERIFY(vision3d::MeshPicking::screenToWorldRay(
            camera, logicalPosition, QSize(800, 600), dpr, highDpiRay));
        QVERIFY(fuzzyVectorEqual(
            reference.direction, highDpiRay.direction, 1.0e-5f));
        QVERIFY(fuzzyVectorEqual(reference.origin, highDpiRay.origin, 1.0e-5f));
    }
}

void MeshPickingTest::sourceWorldCoordinatePreserved()
{
    const vision3d::MeshData mesh = makeSourceWorldPlane();
    vision3d::CameraController camera;
    camera.setViewportSize(800, 600);
    QVERIFY(camera.fitToBounds(mesh.boundingBox));
    vision3d::Ray ray;
    QVERIFY(vision3d::MeshPicking::screenToWorldRay(
        camera, QPointF(400.0, 300.0), QSize(800, 600), 1.0, ray));
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(mesh, ray);
    QVERIFY(hit.isValid());
    QVERIFY(hit.worldPosition.x() > 12.0f);
    QVERIFY(hit.worldPosition.y() < -6.0f);
    QVERIFY(qAbs(hit.worldPosition.z() - 25.0f) <= 1.0e-4f);
    QVERIFY(hit.worldPosition.x() < 13.0f);
    QVERIFY(hit.worldPosition.y() > -7.0f);
}

void MeshPickingTest::finiteResult()
{
    const auto ray = vision3d::Ray::fromOriginAndDirection(
        QVector3D(0.1f, 0.1f, 2.0f), QVector3D(0.0f, 0.0f, -1.0f));
    QVERIFY(ray.has_value());
    const vision3d::SurfaceHit hit = vision3d::MeshPicking::pick(makePlane(), *ray);
    QVERIFY(hit.isValid());
    QVERIFY(hit.isFinite());
    QVERIFY(std::isfinite(hit.distance));
}

} // namespace

QTEST_APPLESS_MAIN(MeshPickingTest)

#include "test_mesh_picking.moc"
