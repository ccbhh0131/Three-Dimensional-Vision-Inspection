#pragma once

#include "core/geometry/MarkerHitTesting.h"
#include "core/geometry/MeshPicking.h"
#include "core/mesh/MeshData.h"
#include "core/viewer/CameraController.h"
#include "core/viewer/SceneAlignmentTransform.h"
#include "render/MarkerRenderer.h"
#include "render/MeshRenderer.h"

#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QOpenGLWidget>
#include <QPointF>
#include <QString>

class QOpenGLContext;
class QMouseEvent;
class QWheelEvent;

namespace vision3d {

class ModelViewerWidget final : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit ModelViewerWidget(QWidget* parent = nullptr);
    ~ModelViewerWidget() override;

    void setMesh(MeshData mesh);
    void clearMesh();
    void setSceneAlignment(const SceneAlignmentTransform& alignment);
    const SceneAlignmentTransform& sceneAlignment() const;
    void setOrbitSensitivity(float sensitivity);
    void setMarkerSize(float scale);
    bool fitToView();
    void resetView();
    void setFrontView();
    void setBackView();
    void setLeftView();
    void setRightView();
    void setTopView();
    void setBottomView();
    void setIsometricView();

    const MeshData& meshData() const;
    SurfaceHit pickAt(const QPointF& logicalPosition);
    const SurfaceHit& lastSurfaceHit() const;
    void setMarkers(const QVector<DeviceMarkerView>& markers);
    void clearMarkers();
    void setSelectedMarker(const QString& markerId);
    QString selectedMarkerId() const;
    const QVector<DeviceMarkerView>& markerViews() const;

    RendererStatus rendererStatus() const;
    MarkerRendererStatus markerRendererStatus() const;
    const CameraController& cameraController() const;
    bool hasMesh() const;
    bool isRendererReady() const;
    bool hasRenderedFrame() const;
    quint64 renderedFrameCount() const;
    double firstFrameMilliseconds() const;

signals:
    void surfacePicked(const SurfaceHit& hit);
    void surfaceMissed();
    void markerSelected(const QString& markerId);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class OrbitAxis {
        None,
        Yaw,
        Pitch,
    };

    void handleContextAboutToBeDestroyed();
    void uploadPendingMesh();
    void updateModelMatrix();
    BoundingBox alignedMeshBounds() const;
    std::optional<QString> markerIdAt(const QPointF& logicalPosition) const;
    void clearMarkerPresentation();

    MeshRenderer m_renderer;
    MarkerRenderer m_markerRenderer;
    CameraController m_camera;
    MeshData m_mesh;
    SceneAlignmentTransform m_sceneAlignment;
    bool m_hasMesh = false;
    bool m_meshPendingUpload = false;
    bool m_contextAttached = false;
    QOpenGLContext* m_context = nullptr;

    QMatrix4x4 m_model;
    QMatrix3x3 m_normalMatrix;
    QString m_meshError;
    QString m_cameraError;

    bool m_orbiting = false;
    bool m_panning = false;
    bool m_leftPressActive = false;
    bool m_leftDragStarted = false;
    OrbitAxis m_orbitAxis = OrbitAxis::None;
    QPointF m_leftPressPosition;
    QPointF m_lastMousePosition;

    SurfaceHit m_lastSurfaceHit;
    QVector<DeviceMarkerView> m_markers;
    QString m_selectedMarkerId;

    QElapsedTimer m_frameTimer;
    bool m_firstFrameRendered = false;
    double m_firstFrameMilliseconds = 0.0;
    quint64 m_renderedFrameCount = 0;
};

} // namespace vision3d
