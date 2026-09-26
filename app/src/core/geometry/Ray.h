#pragma once

#include <QVector3D>

#include <optional>

namespace vision3d {

struct Ray
{
    QVector3D origin{0.0f, 0.0f, 0.0f};
    QVector3D direction{0.0f, 0.0f, -1.0f};

    bool isFinite() const;
    bool isValid(float normalizedTolerance = 1.0e-3f) const;

    static std::optional<Ray> fromOriginAndDirection(
        const QVector3D& origin,
        const QVector3D& direction);
    static std::optional<Ray> fromPoints(
        const QVector3D& origin,
        const QVector3D& pointOnRay);
};

} // namespace vision3d
