#pragma once

#include "core/device/GaugeProfile.h"

#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>

namespace vision3d {

enum class GaugeCenterSource
{
    None,
    Circle,
    RoiCenterFallback,
};

QString gaugeCenterSourceToString(GaugeCenterSource source);
QString gaugeCenterSourceDisplayName(GaugeCenterSource source);

struct VisualGaugeReadingResult
{
    bool success = false;
    double value = 0.0;
    double needleAngleDegrees = 0.0;
    QPointF center;
    QPointF needleTip;
    double confidence = 0.0;
    int searchPass = 0;
    GaugeCenterSource centerSource = GaugeCenterSource::None;
    QString failureReason;
    QImage diagnosticOverlay;
};

class VisualGaugeReader final
{
public:
    static VisualGaugeReadingResult read(const QImage& image,
                                         const GaugeProfile& profile,
                                         const QRect& roi);
    static VisualGaugeReadingResult readFromFile(const QString& imagePath,
                                                 const GaugeProfile& profile,
                                                 const QRect& roi = QRect());
};

} // namespace vision3d
