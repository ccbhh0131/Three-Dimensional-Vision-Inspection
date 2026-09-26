#pragma once

#include "core/device/DeviceMarker.h"

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QVector>
#include <QString>
#include <QtGlobal>

class QOpenGLContext;
class QOpenGLFunctions_3_3_Core;

namespace vision3d {

struct MarkerRendererStatus
{
    bool contextReady = false;
    bool shaderReady = false;
    bool drawSucceeded = false;
    qsizetype markerCount = 0;
    QString shaderDiagnostic;
    QString lastError;
    quint32 lastGlError = 0;
    quint64 uploadCount = 0;
    quint64 updateCount = 0;
    quint64 drawCalls = 0;
};

class MarkerRenderer
{
public:
    MarkerRenderer() = default;
    ~MarkerRenderer();

    MarkerRenderer(const MarkerRenderer&) = delete;
    MarkerRenderer& operator=(const MarkerRenderer&) = delete;

    bool initialize(QOpenGLContext* context);
    bool release(QOpenGLContext* expectedContext);
    void invalidateContext();

    void setMarkers(const QVector<DeviceMarkerView>& markers);
    bool clearMarkers();
    bool draw(const QMatrix4x4& model,
              const QMatrix4x4& view,
              const QMatrix4x4& projection);

    const MarkerRendererStatus& status() const;

private:
    bool compileAndLinkProgram();
    bool uploadMarkers();
    void deleteMarkerResource();
    void setFailure(const QString& message, quint32 glError = 0);
    quint32 takeGlError();
    void clearGlErrors();

    QOpenGLContext* m_context = nullptr;
    QOpenGLFunctions_3_3_Core* m_functions = nullptr;
    quint32 m_program = 0;
    quint32 m_vertexArray = 0;
    quint32 m_vertexBuffer = 0;
    int m_modelLocation = -1;
    int m_viewLocation = -1;
    int m_projectionLocation = -1;
    int m_pointSizeLocation = -1;
    QVector<DeviceMarkerView> m_markers;
    bool m_markersPendingUpload = false;
    MarkerRendererStatus m_status;
};

} // namespace vision3d
