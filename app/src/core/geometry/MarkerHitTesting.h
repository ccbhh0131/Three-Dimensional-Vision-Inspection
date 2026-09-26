#pragma once

#include "core/device/DeviceMarker.h"
#include "core/viewer/CameraController.h"

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

    static std::optional<QString> hitTest(const QList<DeviceMarkerView>& markers,
                                           const CameraController& camera,
                                           const QPointF& logicalPosition,
                                           const QSize& logicalViewport,
                                           qreal devicePixelRatio,
                                           qreal radiusPixels);
};

} // namespace vision3d
