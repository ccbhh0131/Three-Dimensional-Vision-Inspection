#include "MarkerHitTesting.h"

#include "core/geometry/MeshPicking.h"

#include <QVector4D>

#include <cmath>

namespace vision3d {
namespace {

bool isFinitePoint(const QPointF& point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool isFiniteVector(const QVector4D& value)
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z())
        && std::isfinite(value.w());
}

} // namespace

bool MarkerHitTesting::worldToLogicalPosition(
    const CameraController& camera,
    const QVector3D& worldPosition,
    const QSize& logicalViewport,
    qreal devicePixelRatio,
    QPointF& logicalPosition)
{
    if (!camera.hasBounds() || !camera.isFinite()
        || logicalViewport.width() <= 0 || logicalViewport.height() <= 0
        || !std::isfinite(devicePixelRatio) || devicePixelRatio <= 0.0) {
        return false;
    }

    const QVector4D cameraPosition = camera.viewMatrix()
        * QVector4D(worldPosition, 1.0f);
    if (!isFiniteVector(cameraPosition) || cameraPosition.z() >= 0.0f) {
        return false;
    }

    QVector3D ndc;
    if (!camera.worldToNdc(worldPosition, ndc)
        || !std::isfinite(ndc.x()) || !std::isfinite(ndc.y())
        || !std::isfinite(ndc.z())
        || ndc.x() < -1.0f || ndc.x() > 1.0f
        || ndc.y() < -1.0f || ndc.y() > 1.0f
        || ndc.z() < -1.0f || ndc.z() > 1.0f) {
        return false;
    }

    if (!MeshPicking::worldToLogicalPosition(camera,
                                             worldPosition,
                                             logicalViewport,
                                             devicePixelRatio,
                                             logicalPosition)) {
        return false;
    }
    return isFinitePoint(logicalPosition)
        && logicalPosition.x() >= 0.0
        && logicalPosition.y() >= 0.0
        && logicalPosition.x() <= logicalViewport.width()
        && logicalPosition.y() <= logicalViewport.height();
}

std::optional<QString> MarkerHitTesting::hitTest(
    const QList<DeviceMarkerView>& markers,
    const CameraController& camera,
    const QPointF& logicalPosition,
    const QSize& logicalViewport,
    qreal devicePixelRatio,
    qreal radiusPixels)
{
    if (!isFinitePoint(logicalPosition) || !std::isfinite(radiusPixels)
        || radiusPixels <= 0.0) {
        return std::nullopt;
    }

    const qreal radiusSquared = radiusPixels * radiusPixels;
    qreal bestDistanceSquared = radiusSquared;
    std::optional<QString> bestId;
    for (const DeviceMarkerView& marker : markers) {
        QPointF markerPosition;
        if (!worldToLogicalPosition(camera,
                                    marker.worldPosition,
                                    logicalViewport,
                                    devicePixelRatio,
                                    markerPosition)) {
            continue;
        }
        const qreal dx = logicalPosition.x() - markerPosition.x();
        const qreal dy = logicalPosition.y() - markerPosition.y();
        const qreal distanceSquared = dx * dx + dy * dy;
        if (distanceSquared <= bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestId = marker.id;
        }
    }
    return bestId;
}

} // namespace vision3d
