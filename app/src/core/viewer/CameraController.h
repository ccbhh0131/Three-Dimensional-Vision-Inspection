#pragma once

#include "core/geometry/BoundingBox.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace vision3d {

class CameraController
{
public:
    CameraController();

    void clear();
    bool fitToBounds(const BoundingBox& bounds);
    void resetView();

    void setViewportSize(int width, int height);

    // Presets are relative to the default orientation established by fitToBounds().
    void setFrontView();
    void setBackView();
    void setLeftView();
    void setRightView();
    void setTopView();
    void setBottomView();
    void setIsometricView();

    // Mouse deltas are in Qt logical widget pixels. Positive wheelSteps zooms in.
    void orbit(float deltaX, float deltaY);
    void pan(float deltaX, float deltaY);
    void zoom(float wheelSteps);

    bool worldToNdc(const QVector3D& worldPoint, QVector3D& ndcPoint) const;
    bool ndcToWorld(const QVector3D& ndcPoint, QVector3D& worldPoint) const;

    bool hasBounds() const;
    bool isFinite() const;

    QVector3D target() const;
    QVector3D position() const;
    float yawRadians() const;
    float pitchRadians() const;
    float distance() const;
    float fieldOfViewDegrees() const;
    float aspectRatio() const;
    float nearPlane() const;
    float farPlane() const;
    float minimumDistance() const;
    float maximumDistance() const;
    float pitchLimitRadians() const;

    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;

private:
    void setPresetView(float yawRadians, float pitchRadians);
    void updateClipPlanes();
    QVector3D cameraOffset() const;
    void cameraBasis(QVector3D& right, QVector3D& up, QVector3D& forward) const;
    static bool isFiniteVector(const QVector3D& value);
    static bool isFiniteMatrix(const QMatrix4x4& value);

    bool m_hasBounds = false;
    QVector3D m_target{0.0f, 0.0f, 0.0f};
    QVector3D m_fitTarget{0.0f, 0.0f, 0.0f};
    float m_yawRadians = 0.0f;
    float m_pitchRadians = 0.0f;
    float m_distance = 1.0f;
    float m_fitDistance = 1.0f;
    float m_radius = 0.0f;
    float m_minimumDistance = 0.001f;
    float m_maximumDistance = 1.0f;
    float m_fieldOfViewDegrees = 45.0f;
    float m_aspectRatio = 1.0f;
    float m_nearPlane = 0.001f;
    float m_farPlane = 1.0f;
    int m_viewportWidth = 1;
    int m_viewportHeight = 1;
};

} // namespace vision3d
