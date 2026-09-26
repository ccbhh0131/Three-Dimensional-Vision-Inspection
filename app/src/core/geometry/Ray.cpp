#include "Ray.h"

#include <cmath>

namespace vision3d {
namespace {

bool isFiniteScalar(float value)
{
    return std::isfinite(value);
}

bool isFiniteVector(const QVector3D& value)
{
    return isFiniteScalar(value.x())
        && isFiniteScalar(value.y())
        && isFiniteScalar(value.z());
}

} // namespace

bool Ray::isFinite() const
{
    return isFiniteVector(origin) && isFiniteVector(direction);
}

bool Ray::isValid(float normalizedTolerance) const
{
    if (!isFinite() || !isFiniteScalar(normalizedTolerance)
        || normalizedTolerance < 0.0f) {
        return false;
    }

    const float lengthSquared = QVector3D::dotProduct(direction, direction);
    if (!isFiniteScalar(lengthSquared) || lengthSquared <= 1.0e-12f) {
        return false;
    }

    const float length = std::sqrt(lengthSquared);
    return isFiniteScalar(length)
        && std::abs(length - 1.0f) <= normalizedTolerance;
}

std::optional<Ray> Ray::fromOriginAndDirection(
    const QVector3D& origin,
    const QVector3D& direction)
{
    if (!isFiniteVector(origin) || !isFiniteVector(direction)) {
        return std::nullopt;
    }

    const float lengthSquared = QVector3D::dotProduct(direction, direction);
    if (!isFiniteScalar(lengthSquared) || lengthSquared <= 1.0e-12f) {
        return std::nullopt;
    }

    const float length = std::sqrt(lengthSquared);
    if (!isFiniteScalar(length) || length <= 1.0e-6f) {
        return std::nullopt;
    }

    Ray ray;
    ray.origin = origin;
    ray.direction = direction / length;
    return ray.isValid() ? std::optional<Ray>(ray) : std::nullopt;
}

std::optional<Ray> Ray::fromPoints(
    const QVector3D& origin,
    const QVector3D& pointOnRay)
{
    return fromOriginAndDirection(origin, pointOnRay - origin);
}

} // namespace vision3d
