#pragma once

#include "core/geometry/BoundingBox.h"

#include <QJsonObject>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QStringList>
#include <QVector3D>
#include <QVector4D>

#include <cmath>
#include <optional>

namespace vision3d {

// Keeps the reconstruction coordinates immutable while describing how a scene
// is presented to the user.  The transform is deliberately rotation-only for
// Stage 5A.3 and is applied around the raw mesh bounding-box center.
class SceneAlignmentTransform
{
public:
    SceneAlignmentTransform()
        : m_rotation(1.0f, 0.0f, 0.0f, 0.0f)
    {
    }

    static SceneAlignmentTransform identity() { return SceneAlignmentTransform(); }

    const QQuaternion& rotation() const { return m_rotation; }

    void setRotation(const QQuaternion& rotation)
    {
        if (rotation.lengthSquared() <= 1.0e-12f
            || !isFiniteQuaternion(rotation)) {
            m_rotation = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
            return;
        }
        m_rotation = rotation.normalized();
    }

    void reset() { m_rotation = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f); }

    bool isValid() const
    {
        return isFiniteQuaternion(m_rotation)
            && m_rotation.lengthSquared() > 1.0e-12f;
    }

    bool rotateByAxis(const QString& axis, float degrees)
    {
        if (!std::isfinite(degrees)) {
            return false;
        }

        QVector3D axisVector;
        const QString normalizedAxis = axis.trimmed().toUpper();
        if (normalizedAxis == QStringLiteral("X")) {
            axisVector = QVector3D(1.0f, 0.0f, 0.0f);
        } else if (normalizedAxis == QStringLiteral("Y")) {
            axisVector = QVector3D(0.0f, 1.0f, 0.0f);
        } else if (normalizedAxis == QStringLiteral("Z")) {
            axisVector = QVector3D(0.0f, 0.0f, 1.0f);
        } else {
            return false;
        }

        const QQuaternion delta = QQuaternion::fromAxisAndAngle(axisVector, degrees);
        if (!isFiniteQuaternion(delta)) {
            return false;
        }

        // Pre-multiplication makes the buttons operate around the displayed
        // scene axes, while the stored value remains one quaternion.
        setRotation(delta * m_rotation);
        return isValid();
    }

    QMatrix4x4 matrix(const QVector3D& rawCenter) const
    {
        QMatrix4x4 result;
        result.setToIdentity();
        result.translate(rawCenter);
        result.rotate(m_rotation);
        result.translate(-rawCenter);
        return result;
    }

    QMatrix4x4 inverseMatrix(const QVector3D& rawCenter) const
    {
        QMatrix4x4 result;
        result.setToIdentity();
        result.translate(rawCenter);
        result.rotate(m_rotation.conjugated());
        result.translate(-rawCenter);
        return result;
    }

    QVector3D mapPoint(const QVector3D& rawPoint, const QVector3D& rawCenter) const
    {
        const QVector4D aligned = matrix(rawCenter) * QVector4D(rawPoint, 1.0f);
        if (!std::isfinite(aligned.w()) || std::abs(aligned.w()) <= 1.0e-7f) {
            return QVector3D();
        }
        return QVector3D(aligned.x() / aligned.w(),
                         aligned.y() / aligned.w(),
                         aligned.z() / aligned.w());
    }

    BoundingBox transformBounds(const BoundingBox& rawBounds) const
    {
        BoundingBox result;
        if (rawBounds.isEmpty()) {
            return result;
        }

        const QVector3D minimum = rawBounds.minimum();
        const QVector3D maximum = rawBounds.maximum();
        const QVector3D center = rawBounds.center();
        const QMatrix4x4 transform = matrix(center);
        const QVector3D corners[] = {
            QVector3D(minimum.x(), minimum.y(), minimum.z()),
            QVector3D(minimum.x(), minimum.y(), maximum.z()),
            QVector3D(minimum.x(), maximum.y(), minimum.z()),
            QVector3D(minimum.x(), maximum.y(), maximum.z()),
            QVector3D(maximum.x(), minimum.y(), minimum.z()),
            QVector3D(maximum.x(), minimum.y(), maximum.z()),
            QVector3D(maximum.x(), maximum.y(), minimum.z()),
            QVector3D(maximum.x(), maximum.y(), maximum.z()),
        };
        for (const QVector3D& corner : corners) {
            const QVector4D aligned = transform * QVector4D(corner, 1.0f);
            if (std::isfinite(aligned.x()) && std::isfinite(aligned.y())
                && std::isfinite(aligned.z()) && std::isfinite(aligned.w())
                && std::abs(aligned.w()) > 1.0e-7f) {
                result.expand(QVector3D(aligned.x() / aligned.w(),
                                        aligned.y() / aligned.w(),
                                        aligned.z() / aligned.w()));
            }
        }
        return result;
    }

    QJsonObject toJson() const
    {
        const QQuaternion normalized = isValid()
            ? m_rotation.normalized()
            : QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
        return QJsonObject{
            {QStringLiteral("rotation"),
             QJsonObject{
                 {QStringLiteral("w"), normalized.scalar()},
                 {QStringLiteral("x"), normalized.x()},
                 {QStringLiteral("y"), normalized.y()},
                 {QStringLiteral("z"), normalized.z()},
             }},
        };
    }

    static std::optional<SceneAlignmentTransform> fromJson(const QJsonObject& json,
                                                            QString* error = nullptr)
    {
        const auto fail = [error](const QString& message)
            -> std::optional<SceneAlignmentTransform> {
            if (error != nullptr) {
                *error = message;
            }
            return std::nullopt;
        };

        SceneAlignmentTransform result;
        if (!json.contains(QStringLiteral("rotation"))) {
            return result;
        }
        if (!json.value(QStringLiteral("rotation")).isObject()) {
            return fail(QStringLiteral("sceneAlignment.rotation 必须是对象。"));
        }

        const QJsonObject rotation = json.value(QStringLiteral("rotation")).toObject();
        const QStringList components{
            QStringLiteral("w"),
            QStringLiteral("x"),
            QStringLiteral("y"),
            QStringLiteral("z"),
        };
        for (const QString& component : components) {
            if (!rotation.value(component).isDouble()
                || !std::isfinite(rotation.value(component).toDouble())) {
                return fail(QStringLiteral("sceneAlignment.rotation.%1 必须是有限数字。")
                                .arg(component));
            }
        }

        const QQuaternion parsed(
            static_cast<float>(rotation.value(QStringLiteral("w")).toDouble()),
            static_cast<float>(rotation.value(QStringLiteral("x")).toDouble()),
            static_cast<float>(rotation.value(QStringLiteral("y")).toDouble()),
            static_cast<float>(rotation.value(QStringLiteral("z")).toDouble()));
        if (!isFiniteQuaternion(parsed) || parsed.lengthSquared() <= 1.0e-12f) {
            return fail(QStringLiteral("sceneAlignment.rotation 不是有效四元数。"));
        }
        result.setRotation(parsed);
        return result;
    }

private:
    static bool isFiniteQuaternion(const QQuaternion& value)
    {
        return std::isfinite(value.scalar())
            && std::isfinite(value.x())
            && std::isfinite(value.y())
            && std::isfinite(value.z());
    }

    QQuaternion m_rotation;
};

} // namespace vision3d
