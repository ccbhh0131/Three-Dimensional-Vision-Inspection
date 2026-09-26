#pragma once

#include <QMetaType>
#include <QVector3D>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace vision3d {

enum class SurfaceHitFailureReason
{
    None,
    NoHit,
    InvalidRay,
    InvalidMesh,
    OutOfViewport,
    InvalidCamera,
    UnsupportedCoordinate
};

struct SurfaceHit
{
    bool hit = false;
    quint32 triangleIndex = std::numeric_limits<quint32>::max();
    QVector3D worldPosition{0.0f, 0.0f, 0.0f};
    QVector3D worldNormal{0.0f, 0.0f, 0.0f};
    float distance = 0.0f;
    // Components are ordered as (u, v, w), where w = 1 - u - v.
    QVector3D barycentric{0.0f, 0.0f, 0.0f};
    SurfaceHitFailureReason failureReason = SurfaceHitFailureReason::NoHit;

    bool isFinite() const
    {
        return std::isfinite(worldPosition.x())
            && std::isfinite(worldPosition.y())
            && std::isfinite(worldPosition.z())
            && std::isfinite(worldNormal.x())
            && std::isfinite(worldNormal.y())
            && std::isfinite(worldNormal.z())
            && std::isfinite(distance)
            && std::isfinite(barycentric.x())
            && std::isfinite(barycentric.y())
            && std::isfinite(barycentric.z());
    }

    bool isValid() const
    {
        return hit && failureReason == SurfaceHitFailureReason::None
            && distance > 0.0f && isFinite();
    }
};

} // namespace vision3d

Q_DECLARE_METATYPE(vision3d::SurfaceHit)
