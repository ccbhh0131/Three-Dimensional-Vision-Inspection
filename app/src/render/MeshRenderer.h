#pragma once

#include "core/mesh/MeshData.h"

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QString>
#include <QtGlobal>

class QOpenGLContext;
class QOpenGLFunctions_3_3_Core;

namespace vision3d {

struct RendererStatus
{
    bool contextReady = false;
    bool profileAccepted = false;
    bool shaderReady = false;
    bool meshUploaded = false;
    bool depthTestEnabled = false;
    bool faceCullingEnabled = false;
    bool drawSucceeded = false;

    int actualMajorVersion = 0;
    int actualMinorVersion = 0;
    int depthBufferBits = 0;
    QString actualProfile;
    QString vendor;
    QString renderer;
    QString version;
    QString glslVersion;
    QString shaderDiagnostic;
    QString lastError;
    quint32 lastGlError = 0;

    qsizetype vertexCount = 0;
    qsizetype triangleCount = 0;
    quint32 gpuVertexStride = 0;
    quint64 vertexBufferBytes = 0;
    quint64 indexBufferBytes = 0;
    quint64 approximateGpuBytes = 0;
    double uploadMilliseconds = 0.0;
    quint64 uploadCount = 0;
    quint64 drawCalls = 0;
};

class MeshRenderer
{
public:
    MeshRenderer() = default;
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    bool initialize(QOpenGLContext* context);
    bool release(QOpenGLContext* expectedContext);
    void invalidateContext();

    bool uploadMesh(const MeshData& mesh);
    bool clearMesh();

    void clearFrame(int framebufferWidth, int framebufferHeight);
    bool draw(
        const QMatrix4x4& model,
        const QMatrix4x4& view,
        const QMatrix4x4& projection,
        const QMatrix3x3& normalMatrix,
        int framebufferWidth,
        int framebufferHeight);

    const RendererStatus& status() const;

private:
    bool compileAndLinkProgram();
    void deleteMeshResource();
    void prepareFrame(int framebufferWidth, int framebufferHeight);
    void setFailure(const QString& message, quint32 glError = 0);
    quint32 takeGlError();
    void clearGlErrors();

    QOpenGLContext* m_context = nullptr;
    QOpenGLFunctions_3_3_Core* m_functions = nullptr;

    quint32 m_program = 0;
    quint32 m_vertexArray = 0;
    quint32 m_vertexBuffer = 0;
    quint32 m_indexBuffer = 0;
    int m_modelLocation = -1;
    int m_viewLocation = -1;
    int m_projectionLocation = -1;
    int m_normalMatrixLocation = -1;
    int m_lightDirectionLocation = -1;

    RendererStatus m_status;
};

} // namespace vision3d
