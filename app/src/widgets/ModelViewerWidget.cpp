#include "ModelViewerWidget.h"

#include <QApplication>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QStyleHints>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace vision3d {

ModelViewerWidget::ModelViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    m_model.setToIdentity();
    m_normalMatrix.setToIdentity();
    m_camera.setViewportSize(1, 1);
    m_lastSurfaceHit.failureReason = SurfaceHitFailureReason::NoHit;
}

ModelViewerWidget::~ModelViewerWidget()
{
    if (m_context != nullptr && m_context->isValid()) {
        makeCurrent();
        if (QOpenGLContext::currentContext() == m_context) {
            m_renderer.release(m_context);
            m_markerRenderer.release(m_context);
        } else {
            m_renderer.invalidateContext();
            m_markerRenderer.invalidateContext();
        }
        doneCurrent();
    } else {
        m_renderer.invalidateContext();
        m_markerRenderer.invalidateContext();
    }
}

void ModelViewerWidget::setMesh(MeshData mesh)
{
    QString validationError;
    if (!mesh.isValid(&validationError)) {
        clearMarkerPresentation();
        m_mesh = MeshData();
        m_hasMesh = false;
        m_meshPendingUpload = false;
        m_meshError = QStringLiteral("MeshData validation failed: %1").arg(validationError);
        m_camera.clear();
        m_cameraError.clear();
        m_lastSurfaceHit = SurfaceHit();
        update();
        return;
    }

    clearMarkerPresentation();
    m_mesh = std::move(mesh);
    m_hasMesh = true;
    m_meshPendingUpload = true;
    m_meshError.clear();
    m_firstFrameRendered = false;
    m_firstFrameMilliseconds = 0.0;
    m_renderedFrameCount = 0;
    m_lastSurfaceHit = SurfaceHit();
    m_frameTimer.start();
    m_camera.clear();
    if (!m_camera.fitToBounds(m_mesh.boundingBox)) {
        m_cameraError = QStringLiteral(
            "camera fit requires a finite, non-degenerate MeshData bounding box");
    } else {
        m_cameraError.clear();
    }

    if (m_contextAttached && m_context != nullptr && m_context->isValid()) {
        makeCurrent();
        if (QOpenGLContext::currentContext() == m_context) {
            uploadPendingMesh();
        }
        doneCurrent();
    }
    update();
}

void ModelViewerWidget::clearMesh()
{
    if (m_contextAttached && m_context != nullptr && m_context->isValid()) {
        makeCurrent();
        if (QOpenGLContext::currentContext() == m_context) {
            m_renderer.clearMesh();
            m_markerRenderer.clearMarkers();
        }
        doneCurrent();
    }

    m_mesh = MeshData();
    m_hasMesh = false;
    m_meshPendingUpload = false;
    m_meshError.clear();
    m_cameraError.clear();
    m_camera.clear();
    m_firstFrameRendered = false;
    m_firstFrameMilliseconds = 0.0;
    m_renderedFrameCount = 0;
    m_lastSurfaceHit = SurfaceHit();
    m_markers.clear();
    m_selectedMarkerId.clear();
    update();
}

bool ModelViewerWidget::fitToView()
{
    if (!m_hasMesh) {
        return false;
    }
    m_camera.clear();
    const bool fitted = m_camera.fitToBounds(m_mesh.boundingBox);
    m_cameraError = fitted
        ? QString()
        : QStringLiteral("camera fit requires a finite, non-degenerate MeshData bounding box");
    update();
    return fitted;
}

void ModelViewerWidget::resetView()
{
    m_camera.resetView();
    update();
}

const MeshData& ModelViewerWidget::meshData() const
{
    return m_mesh;
}

SurfaceHit ModelViewerWidget::pickAt(const QPointF& logicalPosition)
{
    SurfaceHit result;
    if (!m_hasMesh) {
        result.failureReason = SurfaceHitFailureReason::InvalidMesh;
    } else {
        Ray ray;
        SurfaceHitFailureReason failureReason = SurfaceHitFailureReason::None;
        if (!MeshPicking::screenToWorldRay(
                m_camera,
                logicalPosition,
                size(),
                devicePixelRatioF(),
                ray,
                &failureReason)) {
            result.failureReason = failureReason;
        } else {
            result = MeshPicking::pick(m_mesh, ray);
        }
    }

    m_lastSurfaceHit = result;
    if (result.hit) {
        emit surfacePicked(result);
    } else {
        emit surfaceMissed();
    }
    return result;
}

const SurfaceHit& ModelViewerWidget::lastSurfaceHit() const
{
    return m_lastSurfaceHit;
}

void ModelViewerWidget::setMarkers(const QVector<DeviceMarkerView>& markers)
{
    const QString previousSelection = m_selectedMarkerId;
    m_markers = markers;
    bool selectedMarkerStillExists = m_selectedMarkerId.isEmpty();
    for (DeviceMarkerView& marker : m_markers) {
        marker.selected = !m_selectedMarkerId.isEmpty()
            && marker.id == m_selectedMarkerId;
        if (marker.selected) {
            selectedMarkerStillExists = true;
        }
    }
    if (!selectedMarkerStillExists) {
        m_selectedMarkerId.clear();
        for (DeviceMarkerView& marker : m_markers) {
            marker.selected = false;
        }
    }
    m_markerRenderer.setMarkers(m_markers);
    if (previousSelection != m_selectedMarkerId) {
        emit markerSelected(m_selectedMarkerId);
    }
    update();
}

void ModelViewerWidget::clearMarkers()
{
    const bool hadSelection = !m_selectedMarkerId.isEmpty();
    m_markers.clear();
    m_selectedMarkerId.clear();
    if (m_contextAttached && m_context != nullptr && m_context->isValid()) {
        makeCurrent();
        if (QOpenGLContext::currentContext() == m_context) {
            m_markerRenderer.clearMarkers();
        } else {
            m_markerRenderer.setMarkers({});
        }
        doneCurrent();
    } else {
        m_markerRenderer.setMarkers({});
    }
    if (hadSelection) {
        emit markerSelected(QString());
    }
    update();
}

void ModelViewerWidget::setSelectedMarker(const QString& markerId)
{
    QString normalizedId = markerId.trimmed();
    if (!normalizedId.isEmpty()) {
        bool found = false;
        for (const DeviceMarkerView& marker : m_markers) {
            if (marker.id == normalizedId) {
                found = true;
                break;
            }
        }
        if (!found) {
            normalizedId.clear();
        }
    }
    if (normalizedId == m_selectedMarkerId) {
        return;
    }
    m_selectedMarkerId = normalizedId;
    for (DeviceMarkerView& marker : m_markers) {
        marker.selected = !m_selectedMarkerId.isEmpty()
            && marker.id == m_selectedMarkerId;
    }
    m_markerRenderer.setMarkers(m_markers);
    emit markerSelected(m_selectedMarkerId);
    update();
}

QString ModelViewerWidget::selectedMarkerId() const
{
    return m_selectedMarkerId;
}

const QVector<DeviceMarkerView>& ModelViewerWidget::markerViews() const
{
    return m_markers;
}

RendererStatus ModelViewerWidget::rendererStatus() const
{
    RendererStatus result = m_renderer.status();
    if (!m_meshError.isEmpty()) {
        result.lastError = m_meshError;
    }
    if (!m_cameraError.isEmpty()) {
        if (!result.lastError.isEmpty()) {
            result.lastError += QStringLiteral("; ");
        }
        result.lastError += m_cameraError;
    }
    return result;
}

MarkerRendererStatus ModelViewerWidget::markerRendererStatus() const
{
    return m_markerRenderer.status();
}

const CameraController& ModelViewerWidget::cameraController() const
{
    return m_camera;
}

bool ModelViewerWidget::hasMesh() const
{
    return m_hasMesh;
}

bool ModelViewerWidget::isRendererReady() const
{
    const RendererStatus status = rendererStatus();
    return m_camera.hasBounds()
        && m_camera.isFinite()
        && status.contextReady
        && status.profileAccepted
        && status.shaderReady
        && status.meshUploaded;
}

bool ModelViewerWidget::hasRenderedFrame() const
{
    return m_firstFrameRendered;
}

quint64 ModelViewerWidget::renderedFrameCount() const
{
    return m_renderedFrameCount;
}

double ModelViewerWidget::firstFrameMilliseconds() const
{
    return m_firstFrameMilliseconds;
}

void ModelViewerWidget::initializeGL()
{
    m_context = context();
    m_contextAttached = m_context != nullptr;
    if (!m_contextAttached) {
        m_meshError = QStringLiteral("QOpenGLWidget did not provide a context");
        return;
    }

    connect(
        m_context,
        &QOpenGLContext::aboutToBeDestroyed,
        this,
        [this]() { handleContextAboutToBeDestroyed(); },
        Qt::DirectConnection);

    if (!m_renderer.initialize(m_context)) {
        return;
    }
    if (!m_markerRenderer.initialize(m_context)) {
        m_meshError = QStringLiteral("MarkerRenderer initialization failed: %1")
                          .arg(m_markerRenderer.status().lastError);
    }

    if (m_hasMesh) {
        uploadPendingMesh();
    }
}

void ModelViewerWidget::resizeGL(int width, int height)
{
    m_camera.setViewportSize(width, height);
}

void ModelViewerWidget::paintGL()
{
    ++m_renderedFrameCount;
    if (!m_contextAttached || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        return;
    }

    if (m_meshPendingUpload && m_hasMesh) {
        uploadPendingMesh();
    }

    const int framebufferWidth = std::max(
        1,
        qRound(static_cast<qreal>(width()) * devicePixelRatioF()));
    const int framebufferHeight = std::max(
        1,
        qRound(static_cast<qreal>(height()) * devicePixelRatioF()));

    if (m_hasMesh && m_camera.hasBounds() && m_camera.isFinite()
        && m_renderer.status().meshUploaded) {
        const QMatrix4x4 view = m_camera.viewMatrix();
        const QMatrix4x4 projection = m_camera.projectionMatrix();
        m_renderer.draw(
            m_model,
            view,
            projection,
            m_normalMatrix,
            framebufferWidth,
            framebufferHeight);
        m_markerRenderer.draw(m_model, view, projection);
        if (m_renderer.status().drawSucceeded && !m_firstFrameRendered) {
            m_firstFrameRendered = true;
            m_firstFrameMilliseconds = m_frameTimer.isValid()
                ? m_frameTimer.nsecsElapsed() / 1'000'000.0
                : 0.0;
        }
    } else {
        m_renderer.clearFrame(framebufferWidth, framebufferHeight);
    }
}

void ModelViewerWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_camera.hasBounds()) {
        event->ignore();
        return;
    }

    m_lastMousePosition = event->position();
    if (event->button() == Qt::LeftButton) {
        m_orbiting = true;
        m_panning = false;
        m_leftPressActive = true;
        m_leftDragStarted = false;
        m_leftPressPosition = event->position();
        setFocus(Qt::MouseFocusReason);
        event->accept();
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_orbiting = false;
        m_leftPressActive = false;
        m_leftDragStarted = false;
        setFocus(Qt::MouseFocusReason);
        event->accept();
    } else {
        QOpenGLWidget::mousePressEvent(event);
    }
}

void ModelViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_orbiting && !m_panning) {
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    const QPointF currentPosition = event->position();
    if (m_orbiting && !m_leftDragStarted) {
        const QPointF pressDelta = currentPosition - m_leftPressPosition;
        const int threshold = QApplication::styleHints()->startDragDistance();
        const qreal distance = std::hypot(pressDelta.x(), pressDelta.y());
        if (distance < static_cast<qreal>(std::max(1, threshold))) {
            m_lastMousePosition = currentPosition;
            event->accept();
            return;
        }
        m_leftDragStarted = true;
        const QPointF dragDelta = currentPosition - m_leftPressPosition;
        m_lastMousePosition = currentPosition;
        m_camera.orbit(
            static_cast<float>(dragDelta.x()),
            static_cast<float>(dragDelta.y()));
        update();
        event->accept();
        return;
    }

    const QPointF delta = currentPosition - m_lastMousePosition;
    m_lastMousePosition = currentPosition;
    if (m_orbiting) {
        m_camera.orbit(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
    } else if (m_panning) {
        m_camera.pan(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
    }
    update();
    event->accept();
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const bool shouldPick = m_leftPressActive && !m_leftDragStarted;
        m_orbiting = false;
        m_leftPressActive = false;
        m_leftDragStarted = false;
        if (shouldPick) {
            const std::optional<QString> markerId = markerIdAt(event->position());
            if (markerId.has_value()) {
                setSelectedMarker(*markerId);
            } else {
                setSelectedMarker(QString());
                pickAt(event->position());
            }
        }
        event->accept();
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        event->accept();
    } else {
        QOpenGLWidget::mouseReleaseEvent(event);
    }
}

void ModelViewerWidget::wheelEvent(QWheelEvent* event)
{
    float wheelSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    if (std::abs(wheelSteps) <= 1.0e-6f) {
        wheelSteps = static_cast<float>(event->pixelDelta().y()) / 120.0f;
    }
    if (m_camera.hasBounds() && std::isfinite(wheelSteps)
        && std::abs(wheelSteps) > 1.0e-6f) {
        m_camera.zoom(wheelSteps);
        update();
        event->accept();
    } else {
        event->ignore();
    }
}

void ModelViewerWidget::handleContextAboutToBeDestroyed()
{
    if (m_context != nullptr
        && QOpenGLContext::currentContext() == m_context) {
        m_renderer.release(m_context);
        m_markerRenderer.release(m_context);
    } else {
        m_renderer.invalidateContext();
        m_markerRenderer.invalidateContext();
    }
    m_context = nullptr;
    m_contextAttached = false;
    m_meshPendingUpload = m_hasMesh;
}

void ModelViewerWidget::uploadPendingMesh()
{
    if (!m_hasMesh || !m_contextAttached || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        return;
    }
    m_renderer.uploadMesh(m_mesh);
    m_meshPendingUpload = false;
}

std::optional<QString> ModelViewerWidget::markerIdAt(const QPointF& logicalPosition) const
{
    return MarkerHitTesting::hitTest(m_markers,
                                     m_camera,
                                     logicalPosition,
                                     size(),
                                     devicePixelRatioF(),
                                     14.0);
}

void ModelViewerWidget::clearMarkerPresentation()
{
    m_markers.clear();
    m_selectedMarkerId.clear();
    if (m_contextAttached && m_context != nullptr && m_context->isValid()) {
        makeCurrent();
        if (QOpenGLContext::currentContext() == m_context) {
            m_markerRenderer.clearMarkers();
        } else {
            m_markerRenderer.setMarkers({});
        }
        doneCurrent();
    } else {
        m_markerRenderer.setMarkers({});
    }
}

} // namespace vision3d
