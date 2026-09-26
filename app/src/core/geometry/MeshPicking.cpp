#include "MeshPicking.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vision3d {
namespace {

constexpr double kDeterminantEpsilon = 1.0e-12;
constexpr double kBarycentricTolerance = 1.0e-9;
constexpr double kPositiveDistanceEpsilon = 1.0e-9;

struct Vec3d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vec3d toDouble(const QVector3D& value)
{
    return {value.x(), value.y(), value.z()};
}

bool isFinite(const Vec3d& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

Vec3d subtract(const Vec3d& left, const Vec3d& right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3d cross(const Vec3d& left, const Vec3d& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

double dot(const Vec3d& left, const Vec3d& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

QVector3D toFloat(const Vec3d& value)
{
    return QVector3D(
        static_cast<float>(value.x),
        static_cast<float>(value.y),
        static_cast<float>(value.z));
}

void setFailure(SurfaceHitFailureReason* failureReason,
                SurfaceHitFailureReason value)
{
    if (failureReason != nullptr) {
        *failureReason = value;
    }
}

bool isFiniteScalar(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

bool isFinitePoint(const QPointF& point)
{
    return isFiniteScalar(point.x()) && isFiniteScalar(point.y());
}

bool isFiniteVector(const QVector3D& value)
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z());
}

} // namespace

bool MeshPicking::screenToWorldRay(
    const CameraController& camera,
    const QPointF& logicalPosition,
    const QSize& logicalViewport,
    qreal devicePixelRatio,
    Ray& ray,
    SurfaceHitFailureReason* failureReason)
{
    setFailure(failureReason, SurfaceHitFailureReason::None);
    if (!camera.hasBounds() || !camera.isFinite()) {
        setFailure(failureReason, SurfaceHitFailureReason::InvalidCamera);
        return false;
    }
    if (logicalViewport.width() <= 0 || logicalViewport.height() <= 0
        || !isFiniteScalar(devicePixelRatio) || devicePixelRatio <= 0.0) {
        setFailure(failureReason, SurfaceHitFailureReason::UnsupportedCoordinate);
        return false;
    }
    if (!isFinitePoint(logicalPosition)
        || logicalPosition.x() < 0.0
        || logicalPosition.y() < 0.0
        || logicalPosition.x() > logicalViewport.width()
        || logicalPosition.y() > logicalViewport.height()) {
        setFailure(failureReason, SurfaceHitFailureReason::OutOfViewport);
        return false;
    }

    const double framebufferWidth =
        static_cast<double>(logicalViewport.width()) * devicePixelRatio;
    const double framebufferHeight =
        static_cast<double>(logicalViewport.height()) * devicePixelRatio;
    const double framebufferX =
        static_cast<double>(logicalPosition.x()) * devicePixelRatio;
    const double framebufferY =
        static_cast<double>(logicalPosition.y()) * devicePixelRatio;
    if (!std::isfinite(framebufferWidth) || !std::isfinite(framebufferHeight)
        || framebufferWidth <= 0.0 || framebufferHeight <= 0.0
        || !std::isfinite(framebufferX) || !std::isfinite(framebufferY)) {
        setFailure(failureReason, SurfaceHitFailureReason::UnsupportedCoordinate);
        return false;
    }

    const float ndcX = static_cast<float>(
        2.0 * (framebufferX / framebufferWidth) - 1.0);
    const float ndcY = static_cast<float>(
        1.0 - 2.0 * (framebufferY / framebufferHeight));
    QVector3D nearWorld;
    QVector3D farWorld;
    if (!camera.ndcToWorld(QVector3D(ndcX, ndcY, -1.0f), nearWorld)
        || !camera.ndcToWorld(QVector3D(ndcX, ndcY, 1.0f), farWorld)
        || !isFiniteVector(nearWorld) || !isFiniteVector(farWorld)) {
        setFailure(failureReason, SurfaceHitFailureReason::InvalidCamera);
        return false;
    }

    const std::optional<Ray> candidate = Ray::fromOriginAndDirection(
        camera.position(), farWorld - nearWorld);
    if (!candidate.has_value()) {
        setFailure(failureReason, SurfaceHitFailureReason::InvalidCamera);
        return false;
    }
    ray = *candidate;
    return true;
}

bool MeshPicking::worldToLogicalPosition(
    const CameraController& camera,
    const QVector3D& worldPoint,
    const QSize& logicalViewport,
    qreal devicePixelRatio,
    QPointF& logicalPosition)
{
    if (!camera.hasBounds() || !camera.isFinite()
        || logicalViewport.width() <= 0 || logicalViewport.height() <= 0
        || !isFiniteScalar(devicePixelRatio) || devicePixelRatio <= 0.0
        || !isFiniteVector(worldPoint)) {
        return false;
    }

    QVector3D ndc;
    if (!camera.worldToNdc(worldPoint, ndc)
        || !isFiniteVector(ndc)) {
        return false;
    }

    const double framebufferWidth =
        static_cast<double>(logicalViewport.width()) * devicePixelRatio;
    const double framebufferHeight =
        static_cast<double>(logicalViewport.height()) * devicePixelRatio;
    if (!std::isfinite(framebufferWidth) || !std::isfinite(framebufferHeight)
        || framebufferWidth <= 0.0 || framebufferHeight <= 0.0) {
        return false;
    }

    const double framebufferX =
        (static_cast<double>(ndc.x()) + 1.0) * 0.5 * framebufferWidth;
    const double framebufferY =
        (1.0 - static_cast<double>(ndc.y())) * 0.5 * framebufferHeight;
    logicalPosition = QPointF(
        framebufferX / devicePixelRatio,
        framebufferY / devicePixelRatio);
    return isFinitePoint(logicalPosition);
}

bool MeshPicking::intersectTriangle(
    const Ray& ray,
    const QVector3D& v0,
    const QVector3D& v1,
    const QVector3D& v2,
    double& distance,
    double& u,
    double& v)
{
    if (!ray.isValid() || !isFiniteVector(v0) || !isFiniteVector(v1)
        || !isFiniteVector(v2)) {
        return false;
    }

    const Vec3d origin = toDouble(ray.origin);
    const Vec3d direction = toDouble(ray.direction);
    const Vec3d vertex0 = toDouble(v0);
    const Vec3d edge1 = subtract(toDouble(v1), vertex0);
    const Vec3d edge2 = subtract(toDouble(v2), vertex0);
    if (!isFinite(origin) || !isFinite(direction)
        || !isFinite(vertex0) || !isFinite(edge1) || !isFinite(edge2)) {
        return false;
    }

    const Vec3d pvec = cross(direction, edge2);
    const double determinant = dot(edge1, pvec);
    if (!std::isfinite(determinant)
        || std::abs(determinant) <= kDeterminantEpsilon) {
        return false;
    }

    const double inverseDeterminant = 1.0 / determinant;
    const Vec3d originToVertex = subtract(origin, vertex0);
    const double candidateU = dot(originToVertex, pvec) * inverseDeterminant;
    if (!std::isfinite(candidateU)
        || candidateU < -kBarycentricTolerance
        || candidateU > 1.0 + kBarycentricTolerance) {
        return false;
    }

    const Vec3d qvec = cross(originToVertex, edge1);
    const double candidateV = dot(direction, qvec) * inverseDeterminant;
    if (!std::isfinite(candidateV)
        || candidateV < -kBarycentricTolerance
        || candidateU + candidateV > 1.0 + kBarycentricTolerance) {
        return false;
    }

    const double candidateDistance = dot(edge2, qvec) * inverseDeterminant;
    if (!std::isfinite(candidateDistance)
        || candidateDistance <= kPositiveDistanceEpsilon) {
        return false;
    }

    distance = candidateDistance;
    u = std::clamp(candidateU, 0.0, 1.0);
    v = std::clamp(candidateV, 0.0, 1.0 - u);
    return std::isfinite(distance) && std::isfinite(u) && std::isfinite(v);
}

SurfaceHit MeshPicking::pick(const MeshData& mesh, const Ray& ray)
{
    SurfaceHit result;
    if (!ray.isValid()) {
        result.failureReason = SurfaceHitFailureReason::InvalidRay;
        return result;
    }
    if (mesh.primitive != MeshPrimitive::Triangles || mesh.vertices.isEmpty()
        || mesh.indices.isEmpty() || mesh.indices.size() % 3 != 0) {
        result.failureReason = SurfaceHitFailureReason::InvalidMesh;
        return result;
    }

    bool invalidIndexSeen = false;
    bool usableTriangleSeen = false;
    bool bestFound = false;
    double bestDistance = std::numeric_limits<double>::infinity();
    qsizetype bestTriangle = -1;
    double bestU = 0.0;
    double bestV = 0.0;

    for (qsizetype triangleIndex = 0;
         triangleIndex < mesh.indices.size() / 3;
         ++triangleIndex) {
        const qsizetype indexOffset = triangleIndex * 3;
        const quint32 index0 = mesh.indices.at(indexOffset);
        const quint32 index1 = mesh.indices.at(indexOffset + 1);
        const quint32 index2 = mesh.indices.at(indexOffset + 2);
        if (index0 >= static_cast<quint32>(mesh.vertices.size())
            || index1 >= static_cast<quint32>(mesh.vertices.size())
            || index2 >= static_cast<quint32>(mesh.vertices.size())) {
            invalidIndexSeen = true;
            continue;
        }

        const QVector3D& vertex0 = mesh.vertices.at(index0).position;
        const QVector3D& vertex1 = mesh.vertices.at(index1).position;
        const QVector3D& vertex2 = mesh.vertices.at(index2).position;
        double distance = 0.0;
        double u = 0.0;
        double v = 0.0;
        if (!intersectTriangle(ray,
                               vertex0,
                               vertex1,
                               vertex2,
                               distance,
                               u,
                               v)) {
            continue;
        }

        usableTriangleSeen = true;
        if (!bestFound || distance < bestDistance) {
            bestFound = true;
            bestDistance = distance;
            bestTriangle = triangleIndex;
            bestU = u;
            bestV = v;
        }
    }

    if (!bestFound) {
        result.failureReason = invalidIndexSeen && !usableTriangleSeen
            ? SurfaceHitFailureReason::InvalidMesh
            : SurfaceHitFailureReason::NoHit;
        return result;
    }

    const qsizetype indexOffset = bestTriangle * 3;
    const quint32 index0 = mesh.indices.at(indexOffset);
    const quint32 index1 = mesh.indices.at(indexOffset + 1);
    const quint32 index2 = mesh.indices.at(indexOffset + 2);
    const MeshVertex& meshVertex0 = mesh.vertices.at(index0);
    const MeshVertex& meshVertex1 = mesh.vertices.at(index1);
    const MeshVertex& meshVertex2 = mesh.vertices.at(index2);
    const float u = static_cast<float>(bestU);
    const float v = static_cast<float>(bestV);
    const float w = std::max(0.0f, 1.0f - u - v);
    const QVector3D worldPosition = meshVertex0.position * w
        + meshVertex1.position * u
        + meshVertex2.position * v;
    QVector3D worldNormal = QVector3D::crossProduct(
        meshVertex1.position - meshVertex0.position,
        meshVertex2.position - meshVertex0.position);
    if (mesh.hasNormals) {
        const QVector3D interpolatedNormal = meshVertex0.normal * w
            + meshVertex1.normal * u
            + meshVertex2.normal * v;
        if (isFiniteVector(interpolatedNormal)
            && interpolatedNormal.lengthSquared() > 1.0e-12f) {
            worldNormal = interpolatedNormal;
        }
    }
    if (!isFiniteVector(worldNormal) || worldNormal.lengthSquared() <= 1.0e-12f) {
        result.failureReason = SurfaceHitFailureReason::NoHit;
        return result;
    }
    worldNormal.normalize();

    result.hit = true;
    result.triangleIndex = static_cast<quint32>(bestTriangle);
    result.worldPosition = worldPosition;
    result.worldNormal = worldNormal;
    result.distance = static_cast<float>(bestDistance);
    result.barycentric = QVector3D(u, v, w);
    result.failureReason = SurfaceHitFailureReason::None;
    if (!result.isValid()) {
        result = SurfaceHit();
        result.failureReason = SurfaceHitFailureReason::NoHit;
    }
    return result;
}

} // namespace vision3d
