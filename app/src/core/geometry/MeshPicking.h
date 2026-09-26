#pragma once

#include "core/geometry/Ray.h"
#include "core/geometry/SurfaceHit.h"
#include "core/mesh/MeshData.h"
#include "core/viewer/CameraController.h"

#include <QPointF>
#include <QSize>

namespace vision3d {

class MeshPicking final
{
public:
    // logicalPosition is a Qt logical widget coordinate. The DPR is applied
    // to both the cursor position and the framebuffer extent, so no second
    // DPR multiplication is performed by the caller.
    static bool screenToWorldRay(
        const CameraController& camera,
        const QPointF& logicalPosition,
        const QSize& logicalViewport,
        qreal devicePixelRatio,
        Ray& ray,
        SurfaceHitFailureReason* failureReason = nullptr);

    static bool worldToLogicalPosition(
        const CameraController& camera,
        const QVector3D& worldPoint,
        const QSize& logicalViewport,
        qreal devicePixelRatio,
        QPointF& logicalPosition);

    static SurfaceHit pick(const MeshData& mesh, const Ray& ray);

    static bool intersectTriangle(
        const Ray& ray,
        const QVector3D& v0,
        const QVector3D& v1,
        const QVector3D& v2,
        double& distance,
        double& u,
        double& v);
};

} // namespace vision3d
