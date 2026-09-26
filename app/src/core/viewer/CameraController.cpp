#include "CameraController.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <limits>

namespace vision3d {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfPi = 0.5f * kPi;
constexpr float kDefaultFovYDegrees = 45.0f;
constexpr float kMinimumRadius = 1.0e-6f;
constexpr float kMinimumDistanceFloor = 1.0e-7f;
constexpr float kOrbitRadiansPerLogicalPixel = 0.008f;
constexpr float kZoomLogScalePerWheelStep = 0.2f;
constexpr float kPitchLimitDegrees = 89.5f;
constexpr float kClipNearRadiusScale = 0.001f;
constexpr float kClipNearDistanceScale = 0.001f;
constexpr float kClipFarRadiusScale = 3.0f;

bool isFiniteScalar(float value)
{
    return std::isfinite(value);
}

float pitchLimitRadians()
{
    return kPitchLimitDegrees * kPi / 180.0f;
}

} // namespace

CameraController::CameraController()
{
    clear();
}

void CameraController::clear()
{
    m_hasBounds = false;
    m_target = QVector3D(0.0f, 0.0f, 0.0f);
    m_fitTarget = m_target;
    m_yawRadians = 0.0f;
    m_pitchRadians = 0.0f;
    m_distance = 1.0f;
    m_fitDistance = 1.0f;
    m_radius = 0.0f;
    m_minimumDistance = 0.001f;
    m_maximumDistance = 1.0f;
    m_fieldOfViewDegrees = kDefaultFovYDegrees;
    m_aspectRatio = 1.0f;
    m_nearPlane = 0.001f;
    m_farPlane = 1.0f;
    m_viewportWidth = 1;
    m_viewportHeight = 1;
}

bool CameraController::fitToBounds(const BoundingBox& bounds)
{
    if (bounds.isEmpty()) {
        return false;
    }

    const QVector3D center = bounds.center();
    const QVector3D extent = bounds.extent();
    const float extentLength = extent.length();
    if (!isFiniteVector(center) || !isFiniteVector(extent)
        || !isFiniteScalar(extentLength) || extentLength <= 0.0f) {
        return false;
    }

    const float radius = std::max(0.5f * extentLength, kMinimumRadius);
    const float halfFovRadians = 0.5f * m_fieldOfViewDegrees * kPi / 180.0f;
    const float tangent = std::tan(halfFovRadians);
    const float distance = 1.5f * radius / tangent;
    if (!isFiniteScalar(radius) || !isFiniteScalar(distance)
        || distance <= kMinimumDistanceFloor) {
        return false;
    }

    const float minimumDistance = std::max(radius * 0.05f, kMinimumDistanceFloor);
    const float maximumDistance = std::max(
        {radius * 100.0f, distance * 100.0f, minimumDistance * 10.0f});
    if (!isFiniteScalar(minimumDistance) || minimumDistance <= 0.0f
        || !isFiniteScalar(maximumDistance)
        || maximumDistance < minimumDistance) {
        return false;
    }

    m_hasBounds = true;
    m_fitTarget = center;
    m_radius = radius;
    m_fitDistance = distance;
    m_minimumDistance = minimumDistance;
    m_maximumDistance = maximumDistance;
    m_target = m_fitTarget;
    m_yawRadians = 0.0f;
    m_pitchRadians = 0.0f;
    m_distance = std::clamp(m_fitDistance, m_minimumDistance, m_maximumDistance);
    updateClipPlanes();
    return isFinite();
}

void CameraController::resetView()
{
    if (!m_hasBounds) {
        return;
    }

    m_target = m_fitTarget;
    m_yawRadians = 0.0f;
    m_pitchRadians = 0.0f;
    m_distance = m_fitDistance;
    updateClipPlanes();
}

void CameraController::setViewportSize(int width, int height)
{
    m_viewportWidth = std::max(1, width);
    m_viewportHeight = std::max(1, height);
    m_aspectRatio = static_cast<float>(m_viewportWidth)
        / static_cast<float>(m_viewportHeight);
    if (!isFiniteScalar(m_aspectRatio) || m_aspectRatio <= 0.0f) {
        m_aspectRatio = 1.0f;
    }
}

void CameraController::orbit(float deltaX, float deltaY)
{
    if (!m_hasBounds || !isFiniteScalar(deltaX) || !isFiniteScalar(deltaY)) {
        return;
    }

    m_yawRadians += deltaX * kOrbitRadiansPerLogicalPixel;
    m_pitchRadians -= deltaY * kOrbitRadiansPerLogicalPixel;
    if (!isFiniteScalar(m_yawRadians) || !isFiniteScalar(m_pitchRadians)) {
        m_yawRadians = 0.0f;
        m_pitchRadians = 0.0f;
    }

    m_yawRadians = std::fmod(m_yawRadians + kPi, 2.0f * kPi);
    if (m_yawRadians < 0.0f) {
        m_yawRadians += 2.0f * kPi;
    }
    m_yawRadians -= kPi;
    m_pitchRadians = std::clamp(
        m_pitchRadians,
        -pitchLimitRadians(),
        pitchLimitRadians());
}

void CameraController::pan(float deltaX, float deltaY)
{
    if (!m_hasBounds || !isFiniteScalar(deltaX) || !isFiniteScalar(deltaY)) {
        return;
    }

    QVector3D right;
    QVector3D up;
    QVector3D forward;
    cameraBasis(right, up, forward);
    Q_UNUSED(forward);

    const float halfFovRadians = 0.5f * m_fieldOfViewDegrees * kPi / 180.0f;
    const float verticalSpan = 2.0f * m_distance * std::tan(halfFovRadians);
    const float unitsPerLogicalPixel = verticalSpan
        / static_cast<float>(std::max(1, m_viewportHeight));
    const float horizontalUnitsPerPixel = unitsPerLogicalPixel * m_aspectRatio;
    if (!isFiniteScalar(unitsPerLogicalPixel)
        || !isFiniteScalar(horizontalUnitsPerPixel)) {
        return;
    }

    // Content follows the drag: moving right/down moves the target left/up in
    // the camera plane, which moves the rendered surface with the cursor.
    const QVector3D candidate = m_target
        - right * (deltaX * horizontalUnitsPerPixel)
        + up * (deltaY * unitsPerLogicalPixel);
    if (isFiniteVector(candidate)) {
        m_target = candidate;
    }
}

void CameraController::zoom(float wheelSteps)
{
    if (!m_hasBounds || !isFiniteScalar(wheelSteps)) {
        return;
    }

    const float clampedSteps = std::clamp(wheelSteps, -100.0f, 100.0f);
    const float exponent = std::clamp(
        -clampedSteps * kZoomLogScalePerWheelStep,
        -20.0f,
        20.0f);
    const float factor = std::exp(exponent);
    const float candidate = m_distance * factor;
    if (!isFiniteScalar(candidate) || candidate <= 0.0f) {
        return;
    }

    m_distance = std::clamp(candidate, m_minimumDistance, m_maximumDistance);
    updateClipPlanes();
}

bool CameraController::worldToNdc(
    const QVector3D& worldPoint,
    QVector3D& ndcPoint) const
{
    if (!isFiniteVector(worldPoint) || !isFinite()) {
        return false;
    }

    const QVector4D clip = projectionMatrix() * viewMatrix()
        * QVector4D(worldPoint, 1.0f);
    if (!isFiniteScalar(clip.x()) || !isFiniteScalar(clip.y())
        || !isFiniteScalar(clip.z()) || !isFiniteScalar(clip.w())
        || std::abs(clip.w()) <= 1.0e-7f) {
        return false;
    }

    ndcPoint = QVector3D(
        clip.x() / clip.w(),
        clip.y() / clip.w(),
        clip.z() / clip.w());
    return isFiniteVector(ndcPoint);
}

bool CameraController::ndcToWorld(
    const QVector3D& ndcPoint,
    QVector3D& worldPoint) const
{
    if (!isFiniteVector(ndcPoint) || !isFinite()) {
        return false;
    }

    bool invertible = false;
    const QMatrix4x4 inverse = (projectionMatrix() * viewMatrix()).inverted(&invertible);
    if (!invertible || !isFiniteMatrix(inverse)) {
        return false;
    }

    const QVector4D worldHomogeneous = inverse
        * QVector4D(ndcPoint, 1.0f);
    if (!isFiniteScalar(worldHomogeneous.x())
        || !isFiniteScalar(worldHomogeneous.y())
        || !isFiniteScalar(worldHomogeneous.z())
        || !isFiniteScalar(worldHomogeneous.w())
        || std::abs(worldHomogeneous.w()) <= 1.0e-7f) {
        return false;
    }

    worldPoint = QVector3D(
        worldHomogeneous.x() / worldHomogeneous.w(),
        worldHomogeneous.y() / worldHomogeneous.w(),
        worldHomogeneous.z() / worldHomogeneous.w());
    return isFiniteVector(worldPoint);
}

bool CameraController::hasBounds() const
{
    return m_hasBounds;
}

bool CameraController::isFinite() const
{
    if (!isFiniteVector(m_target) || !isFiniteVector(m_fitTarget)
        || !isFiniteScalar(m_yawRadians) || !isFiniteScalar(m_pitchRadians)
        || !isFiniteScalar(m_distance) || m_distance <= 0.0f
        || !isFiniteScalar(m_fitDistance) || m_fitDistance <= 0.0f
        || !isFiniteScalar(m_radius) || m_radius < 0.0f
        || !isFiniteScalar(m_minimumDistance) || m_minimumDistance <= 0.0f
        || !isFiniteScalar(m_maximumDistance)
        || m_maximumDistance < m_minimumDistance
        || !isFiniteScalar(m_fieldOfViewDegrees)
        || m_fieldOfViewDegrees <= 0.0f || m_fieldOfViewDegrees >= 180.0f
        || !isFiniteScalar(m_aspectRatio) || m_aspectRatio <= 0.0f
        || !isFiniteScalar(m_nearPlane) || m_nearPlane <= 0.0f
        || !isFiniteScalar(m_farPlane) || m_farPlane <= m_nearPlane) {
        return false;
    }
    if (m_hasBounds && m_radius <= 0.0f) {
        return false;
    }
    return isFiniteVector(position())
        && isFiniteMatrix(viewMatrix())
        && isFiniteMatrix(projectionMatrix());
}

QVector3D CameraController::target() const
{
    return m_target;
}

QVector3D CameraController::position() const
{
    return m_target + cameraOffset();
}

float CameraController::yawRadians() const
{
    return m_yawRadians;
}

float CameraController::pitchRadians() const
{
    return m_pitchRadians;
}

float CameraController::distance() const
{
    return m_distance;
}

float CameraController::fieldOfViewDegrees() const
{
    return m_fieldOfViewDegrees;
}

float CameraController::aspectRatio() const
{
    return m_aspectRatio;
}

float CameraController::nearPlane() const
{
    return m_nearPlane;
}

float CameraController::farPlane() const
{
    return m_farPlane;
}

float CameraController::minimumDistance() const
{
    return m_minimumDistance;
}

float CameraController::maximumDistance() const
{
    return m_maximumDistance;
}

float CameraController::pitchLimitRadians() const
{
    return kPitchLimitDegrees * kPi / 180.0f;
}

QMatrix4x4 CameraController::viewMatrix() const
{
    QMatrix4x4 view;
    view.lookAt(
        position(),
        m_target,
        QVector3D(0.0f, 1.0f, 0.0f));
    return view;
}

QMatrix4x4 CameraController::projectionMatrix() const
{
    QMatrix4x4 projection;
    projection.perspective(
        m_fieldOfViewDegrees,
        m_aspectRatio,
        m_nearPlane,
        m_farPlane);
    return projection;
}

void CameraController::updateClipPlanes()
{
    if (!m_hasBounds) {
        return;
    }

    const float safeRadius = std::max(m_radius, kMinimumRadius);
    const float nearPlane = std::max(
        safeRadius * kClipNearRadiusScale,
        m_distance * kClipNearDistanceScale);
    const float farPlane = m_distance + safeRadius * kClipFarRadiusScale;
    if (isFiniteScalar(nearPlane) && isFiniteScalar(farPlane)
        && nearPlane > 0.0f && farPlane > nearPlane) {
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
        return;
    }

    const float fallbackNear = std::max(kMinimumDistanceFloor, safeRadius * 0.0001f);
    const float fallbackSpan = std::max(safeRadius, kMinimumRadius) * 4.0f;
    const float fallbackFar = fallbackNear + fallbackSpan;
    if (isFiniteScalar(fallbackNear) && isFiniteScalar(fallbackFar)
        && fallbackNear > 0.0f && fallbackFar > fallbackNear) {
        m_nearPlane = fallbackNear;
        m_farPlane = fallbackFar;
        return;
    }

    // Keep the controller finite even when an externally supplied mesh uses
    // coordinates too large for a float perspective range.
    m_nearPlane = 1.0f;
    m_farPlane = std::numeric_limits<float>::max() * 0.25f;
}

QVector3D CameraController::cameraOffset() const
{
    const float cosPitch = std::cos(m_pitchRadians);
    const float sinPitch = std::sin(m_pitchRadians);
    const float sinYaw = std::sin(m_yawRadians);
    const float cosYaw = std::cos(m_yawRadians);
    return QVector3D(
        m_distance * cosPitch * sinYaw,
        m_distance * sinPitch,
        m_distance * cosPitch * cosYaw);
}

void CameraController::cameraBasis(
    QVector3D& right,
    QVector3D& up,
    QVector3D& forward) const
{
    const QVector3D cameraPosition = position();
    forward = m_target - cameraPosition;
    if (forward.lengthSquared() <= 1.0e-20f || !isFiniteVector(forward)) {
        forward = QVector3D(0.0f, 0.0f, -1.0f);
    } else {
        forward.normalize();
    }

    right = QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f));
    if (right.lengthSquared() <= 1.0e-20f || !isFiniteVector(right)) {
        right = QVector3D::crossProduct(forward, QVector3D(0.0f, 0.0f, 1.0f));
    }
    right.normalize();
    up = QVector3D::crossProduct(right, forward);
    up.normalize();
}

bool CameraController::isFiniteVector(const QVector3D& value)
{
    return isFiniteScalar(value.x())
        && isFiniteScalar(value.y())
        && isFiniteScalar(value.z());
}

bool CameraController::isFiniteMatrix(const QMatrix4x4& value)
{
    const float* data = value.constData();
    for (int index = 0; index < 16; ++index) {
        if (!isFiniteScalar(data[index])) {
            return false;
        }
    }
    return true;
}

} // namespace vision3d
