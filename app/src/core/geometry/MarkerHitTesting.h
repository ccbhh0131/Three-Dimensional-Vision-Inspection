#pragma once

#include "core/device/DeviceMarker.h"
#include "core/viewer/CameraController.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QSize>

#include <optional>

namespace vision3d {

class MarkerHitTesting
{
public:
    static bool worldToLogicalPosition(const CameraController& camera,
                                       const QVector3D& worldPosition,
                                       const QSize& logicalViewport,
                                       qreal devicePixelRatio,
                                       QPointF& logicalPosition);

    static bool worldToLogicalPosition(const CameraController& camera,
                                       const QVector3D& rawWorldPosition,
                                       const QMatrix4x4& modelMatrix,
                                       const QSize& logicalViewport,
                                       qreal devicePixelRatio,
                                       QPointF& logicalPosition);

    static std::optional<QString> hitTest(const QList<DeviceMarkerView>& markers,
                                           const CameraController& camera,
                                           const QPointF& logicalPosition,
                                           const QSize& logicalViewport,
                                           qreal devicePixelRatio,
                                           qreal radiusPixels);

    static std::optional<QString> hitTest(
        const QList<DeviceMarkerView>& markers,
        const CameraController& camera,
        const QMatrix4x4& modelMatrix,
        const QPointF& logicalPosition,
        const QSize& logicalViewport,
        qreal devicePixelRatio,
        qreal radiusPixels);
};

} // namespace vision3d
