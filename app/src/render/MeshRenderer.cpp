#include "MeshRenderer.h"

#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QSurfaceFormat>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace vision3d {
namespace {

struct GPUVertex
{
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 1.0f;
    quint8 red = 255;
    quint8 green = 255;
    quint8 blue = 255;
    quint8 alpha = 255;
};

static_assert(sizeof(GPUVertex) == 28, "GPUVertex layout must remain 28 bytes");

constexpr char kVertexShader[] = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec4 vColor;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    gl_Position = uProjection * uView * worldPosition;
}
)GLSL";

constexpr char kFragmentShader[] = R"GLSL(
#version 330 core

in vec3 vNormal;
in vec4 vColor;

uniform vec3 uLightDirection;

out vec4 fragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uLightDirection)), 0.0);
    float intensity = 0.28 + 0.72 * diffuse;
    fragColor = vec4(vColor.rgb * intensity, vColor.a);
}
)GLSL";

QString glString(QOpenGLFunctions_3_3_Core* functions, unsigned int name)
{
    const auto* value = functions->glGetString(name);
    return value == nullptr
        ? QString()
        : QString::fromLatin1(reinterpret_cast<const char*>(value));
}

QString shaderInfoLog(QOpenGLFunctions_3_3_Core* functions, quint32 shader)
{
    int length = 0;
    functions->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return QString();
    }

    QByteArray log(length, '\0');
    int written = 0;
    functions->glGetShaderInfoLog(shader, length, &written, log.data());
    return QString::fromUtf8(log.constData(), written);
}

QString programInfoLog(QOpenGLFunctions_3_3_Core* functions, quint32 program)
{
    int length = 0;
    functions->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return QString();
    }

    QByteArray log(length, '\0');
    int written = 0;
    functions->glGetProgramInfoLog(program, length, &written, log.data());
    return QString::fromUtf8(log.constData(), written);
}

QString profileName(QSurfaceFormat::OpenGLContextProfile profile)
{
    switch (profile) {
    case QSurfaceFormat::CoreProfile:
        return QStringLiteral("CoreProfile");
    case QSurfaceFormat::CompatibilityProfile:
        return QStringLiteral("CompatibilityProfile");
    case QSurfaceFormat::NoProfile:
        return QStringLiteral("NoProfile");
    }
    return QStringLiteral("UnknownProfile");
}

bool isFiniteVector(const QVector3D& value)
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z());
}

} // namespace

MeshRenderer::~MeshRenderer()
{
    // GL deletion is deliberately not attempted here. QOpenGLWidget may already
    // have destroyed its context; the widget owns the explicit current-context
    // release path and this destructor is safe after context loss.
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    m_functions = nullptr;
    m_context = nullptr;
}

bool MeshRenderer::initialize(QOpenGLContext* context)
{
    m_status = RendererStatus();
    m_context = context;
    m_functions = nullptr;
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    m_modelLocation = -1;
    m_viewLocation = -1;
    m_projectionLocation = -1;
    m_normalMatrixLocation = -1;
    m_lightDirectionLocation = -1;

    if (context == nullptr || !context->isValid()
        || QOpenGLContext::currentContext() != context) {
        setFailure(QStringLiteral("OpenGL context is not valid or not current"));
        return false;
    }

    const QSurfaceFormat actualFormat = context->format();
    m_status.actualMajorVersion = actualFormat.majorVersion();
    m_status.actualMinorVersion = actualFormat.minorVersion();
    m_status.depthBufferBits = actualFormat.depthBufferSize();
    m_status.actualProfile = profileName(actualFormat.profile());

    if (actualFormat.majorVersion() < 3
        || (actualFormat.majorVersion() == 3 && actualFormat.minorVersion() < 3)) {
        setFailure(QStringLiteral("actual OpenGL context is below 3.3: %1.%2")
                       .arg(actualFormat.majorVersion())
                       .arg(actualFormat.minorVersion()));
        return false;
    }
    if (actualFormat.profile() != QSurfaceFormat::CoreProfile) {
        setFailure(QStringLiteral("actual OpenGL context profile is %1; CoreProfile is required")
                       .arg(m_status.actualProfile));
        return false;
    }

    m_functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(context);
    if (m_functions == nullptr || !m_functions->initializeOpenGLFunctions()) {
        setFailure(QStringLiteral("QOpenGLFunctions_3_3_Core could not be initialized"));
        m_functions = nullptr;
        return false;
    }

    m_status.contextReady = true;
    m_status.profileAccepted = true;
    m_status.vendor = glString(m_functions, GL_VENDOR);
    m_status.renderer = glString(m_functions, GL_RENDERER);
    m_status.version = glString(m_functions, GL_VERSION);
    m_status.glslVersion = glString(m_functions, GL_SHADING_LANGUAGE_VERSION);
    clearGlErrors();

    if (!compileAndLinkProgram()) {
        return false;
    }

    return true;
}

bool MeshRenderer::release(QOpenGLContext* expectedContext)
{
    if (m_functions != nullptr
        && (expectedContext == nullptr
            || QOpenGLContext::currentContext() != expectedContext)) {
        invalidateContext();
        return false;
    }

    if (m_functions != nullptr) {
        deleteMeshResource();
        if (m_program != 0) {
            m_functions->glDeleteProgram(m_program);
        }
    }

    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    m_functions = nullptr;
    m_context = nullptr;
    m_modelLocation = -1;
    m_viewLocation = -1;
    m_projectionLocation = -1;
    m_normalMatrixLocation = -1;
    m_lightDirectionLocation = -1;
    m_status = RendererStatus();
    return true;
}

void MeshRenderer::invalidateContext()
{
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    m_functions = nullptr;
    m_context = nullptr;
    m_modelLocation = -1;
    m_viewLocation = -1;
    m_projectionLocation = -1;
    m_normalMatrixLocation = -1;
    m_lightDirectionLocation = -1;
    m_status.contextReady = false;
    m_status.profileAccepted = false;
    m_status.shaderReady = false;
    m_status.meshUploaded = false;
    m_status.drawSucceeded = false;
    m_status.lastGlError = 0;
    m_status.lastError = QStringLiteral(
        "OpenGL context was invalidated; CPU MeshData is retained for a later rebuild");
    m_status.vertexCount = 0;
    m_status.triangleCount = 0;
    m_status.vertexBufferBytes = 0;
    m_status.indexBufferBytes = 0;
    m_status.approximateGpuBytes = 0;
}

bool MeshRenderer::uploadMesh(const MeshData& mesh)
{
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        setFailure(QStringLiteral("mesh upload requires the renderer's current OpenGL context"));
        return false;
    }
    if (!m_status.shaderReady) {
        setFailure(QStringLiteral("mesh upload requires a linked shader program"));
        return false;
    }

    QString meshError;
    if (!mesh.isValid(&meshError)) {
        setFailure(QStringLiteral("MeshData validation failed: %1").arg(meshError));
        return false;
    }
    if (mesh.primitive != MeshPrimitive::Triangles) {
        setFailure(QStringLiteral("OpenGL MVP accepts indexed triangle MeshData only"));
        return false;
    }

    const quint64 vertexCount = static_cast<quint64>(mesh.vertices.size());
    const quint64 indexCount = static_cast<quint64>(mesh.indices.size());
    const quint64 vertexBytes = vertexCount * sizeof(GPUVertex);
    const quint64 indexBytes = indexCount * sizeof(quint32);
    const quint64 maxGlBytes = static_cast<quint64>(
        std::numeric_limits<GLsizeiptr>::max());
    if (vertexBytes > maxGlBytes || indexBytes > maxGlBytes
        || indexCount > static_cast<quint64>(std::numeric_limits<GLsizei>::max())) {
        setFailure(QStringLiteral("MeshData is too large for the OpenGL MVP buffer limits"));
        return false;
    }

    QElapsedTimer uploadTimer;
    uploadTimer.start();

    std::vector<GPUVertex> gpuVertices;
    try {
        gpuVertices.resize(static_cast<std::size_t>(vertexCount));
    } catch (...) {
        setFailure(QStringLiteral("could not allocate the temporary GPU vertex staging buffer"));
        return false;
    }

    for (qsizetype index = 0; index < mesh.vertices.size(); ++index) {
        const MeshVertex& source = mesh.vertices.at(index);
        GPUVertex& destination = gpuVertices[static_cast<std::size_t>(index)];
        destination.px = source.position.x();
        destination.py = source.position.y();
        destination.pz = source.position.z();

        QVector3D normal = source.normal;
        const float normalLengthSquared = QVector3D::dotProduct(normal, normal);
        if (!isFiniteVector(normal) || !std::isfinite(normalLengthSquared)
            || normalLengthSquared <= 1.0e-12f) {
            normal = QVector3D(0.0f, 0.0f, 1.0f);
        } else {
            normal /= std::sqrt(normalLengthSquared);
        }
        destination.nx = normal.x();
        destination.ny = normal.y();
        destination.nz = normal.z();
        destination.red = source.color.red;
        destination.green = source.color.green;
        destination.blue = source.color.blue;
        destination.alpha = 255;
    }

    clearGlErrors();
    quint32 newVertexArray = 0;
    quint32 newVertexBuffer = 0;
    quint32 newIndexBuffer = 0;
    m_functions->glGenVertexArrays(1, &newVertexArray);
    m_functions->glGenBuffers(1, &newVertexBuffer);
    m_functions->glGenBuffers(1, &newIndexBuffer);
    quint32 error = takeGlError();
    if (error != 0 || newVertexArray == 0 || newVertexBuffer == 0 || newIndexBuffer == 0) {
        if (newIndexBuffer != 0) {
            m_functions->glDeleteBuffers(1, &newIndexBuffer);
        }
        if (newVertexBuffer != 0) {
            m_functions->glDeleteBuffers(1, &newVertexBuffer);
        }
        if (newVertexArray != 0) {
            m_functions->glDeleteVertexArrays(1, &newVertexArray);
        }
        setFailure(QStringLiteral("OpenGL mesh object allocation failed"), error);
        m_status.uploadMilliseconds = uploadTimer.nsecsElapsed() / 1'000'000.0;
        return false;
    }

    m_functions->glBindVertexArray(newVertexArray);
    m_functions->glBindBuffer(GL_ARRAY_BUFFER, newVertexBuffer);
    m_functions->glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertexBytes),
        gpuVertices.data(),
        GL_STATIC_DRAW);

    m_functions->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newIndexBuffer);
    m_functions->glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indexBytes),
        mesh.indices.constData(),
        GL_STATIC_DRAW);

    m_functions->glEnableVertexAttribArray(0);
    m_functions->glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(GPUVertex)),
        reinterpret_cast<const void*>(offsetof(GPUVertex, px)));
    m_functions->glEnableVertexAttribArray(1);
    m_functions->glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(GPUVertex)),
        reinterpret_cast<const void*>(offsetof(GPUVertex, nx)));
    m_functions->glEnableVertexAttribArray(2);
    m_functions->glVertexAttribPointer(
        2,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        static_cast<GLsizei>(sizeof(GPUVertex)),
        reinterpret_cast<const void*>(offsetof(GPUVertex, red)));

    m_functions->glBindVertexArray(0);
    m_functions->glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_functions->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    error = takeGlError();
    if (error != 0) {
        m_functions->glDeleteBuffers(1, &newIndexBuffer);
        m_functions->glDeleteBuffers(1, &newVertexBuffer);
        m_functions->glDeleteVertexArrays(1, &newVertexArray);
        setFailure(QStringLiteral("OpenGL mesh upload or vertex layout setup failed"), error);
        m_status.uploadMilliseconds = uploadTimer.nsecsElapsed() / 1'000'000.0;
        return false;
    }

    deleteMeshResource();
    m_vertexArray = newVertexArray;
    m_vertexBuffer = newVertexBuffer;
    m_indexBuffer = newIndexBuffer;
    m_status.meshUploaded = true;
    m_status.drawSucceeded = false;
    m_status.vertexCount = static_cast<qsizetype>(vertexCount);
    m_status.triangleCount = static_cast<qsizetype>(indexCount / 3);
    m_status.gpuVertexStride = sizeof(GPUVertex);
    m_status.vertexBufferBytes = vertexBytes;
    m_status.indexBufferBytes = indexBytes;
    m_status.approximateGpuBytes = vertexBytes + indexBytes;
    m_status.uploadMilliseconds = uploadTimer.nsecsElapsed() / 1'000'000.0;
    ++m_status.uploadCount;
    m_status.lastGlError = 0;
    m_status.lastError.clear();
    return true;
}

bool MeshRenderer::clearMesh()
{
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        setFailure(QStringLiteral("mesh resource clear requires the renderer's current OpenGL context"));
        return false;
    }

    deleteMeshResource();
    m_status.meshUploaded = false;
    m_status.drawSucceeded = false;
    m_status.vertexCount = 0;
    m_status.triangleCount = 0;
    m_status.gpuVertexStride = 0;
    m_status.vertexBufferBytes = 0;
    m_status.indexBufferBytes = 0;
    m_status.approximateGpuBytes = 0;
    m_status.lastGlError = takeGlError();
    if (m_status.lastGlError != 0) {
        setFailure(QStringLiteral("OpenGL mesh resource clear failed"), m_status.lastGlError);
        return false;
    }
    m_status.lastError.clear();
    return true;
}

void MeshRenderer::clearFrame(int framebufferWidth, int framebufferHeight)
{
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        return;
    }
    clearGlErrors();
    prepareFrame(framebufferWidth, framebufferHeight);
    const quint32 error = takeGlError();
    m_status.lastGlError = error;
    if (error != 0) {
        setFailure(QStringLiteral("OpenGL frame clear failed"), error);
    }
}

bool MeshRenderer::draw(
    const QMatrix4x4& model,
    const QMatrix4x4& view,
    const QMatrix4x4& projection,
    const QMatrix3x3& normalMatrix,
    int framebufferWidth,
    int framebufferHeight)
{
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context
        || !m_status.shaderReady) {
        return false;
    }

    clearGlErrors();
    prepareFrame(framebufferWidth, framebufferHeight);
    m_status.drawSucceeded = false;
    if (!m_status.meshUploaded || m_vertexArray == 0 || m_program == 0) {
        m_status.lastGlError = takeGlError();
        return false;
    }

    m_functions->glUseProgram(m_program);
    m_functions->glUniformMatrix4fv(m_modelLocation, 1, GL_FALSE, model.constData());
    m_functions->glUniformMatrix4fv(m_viewLocation, 1, GL_FALSE, view.constData());
    m_functions->glUniformMatrix4fv(
        m_projectionLocation,
        1,
        GL_FALSE,
        projection.constData());
    m_functions->glUniformMatrix3fv(
        m_normalMatrixLocation,
        1,
        GL_FALSE,
        normalMatrix.constData());
    m_functions->glUniform3f(m_lightDirectionLocation, 0.35f, 0.55f, 1.0f);
    m_functions->glBindVertexArray(m_vertexArray);
    m_functions->glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(m_status.triangleCount * 3),
        GL_UNSIGNED_INT,
        nullptr);
    m_functions->glBindVertexArray(0);
    m_functions->glUseProgram(0);

    const quint32 error = takeGlError();
    m_status.lastGlError = error;
    if (error != 0) {
        setFailure(QStringLiteral("indexed mesh draw failed"), error);
        return false;
    }

    m_status.drawSucceeded = true;
    ++m_status.drawCalls;
    return true;
}

const RendererStatus& MeshRenderer::status() const
{
    return m_status;
}

bool MeshRenderer::compileAndLinkProgram()
{
    const quint32 vertexShader = m_functions->glCreateShader(GL_VERTEX_SHADER);
    const quint32 fragmentShader = m_functions->glCreateShader(GL_FRAGMENT_SHADER);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            m_functions->glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            m_functions->glDeleteShader(fragmentShader);
        }
        setFailure(QStringLiteral("OpenGL shader object creation failed"), takeGlError());
        return false;
    }

    const GLchar* vertexSource = kVertexShader;
    m_functions->glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    m_functions->glCompileShader(vertexShader);
    int vertexCompiled = GL_FALSE;
    m_functions->glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexCompiled);
    const QString vertexLog = shaderInfoLog(m_functions, vertexShader);
    if (vertexCompiled != GL_TRUE) {
        m_functions->glDeleteShader(vertexShader);
        m_functions->glDeleteShader(fragmentShader);
        m_status.shaderDiagnostic = QStringLiteral("vertex shader compile log: %1").arg(vertexLog);
        setFailure(m_status.shaderDiagnostic, takeGlError());
        return false;
    }

    const GLchar* fragmentSource = kFragmentShader;
    m_functions->glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    m_functions->glCompileShader(fragmentShader);
    int fragmentCompiled = GL_FALSE;
    m_functions->glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentCompiled);
    const QString fragmentLog = shaderInfoLog(m_functions, fragmentShader);
    if (fragmentCompiled != GL_TRUE) {
        m_functions->glDeleteShader(vertexShader);
        m_functions->glDeleteShader(fragmentShader);
        m_status.shaderDiagnostic = QStringLiteral("fragment shader compile log: %1").arg(fragmentLog);
        setFailure(m_status.shaderDiagnostic, takeGlError());
        return false;
    }

    const quint32 program = m_functions->glCreateProgram();
    if (program == 0) {
        m_functions->glDeleteShader(vertexShader);
        m_functions->glDeleteShader(fragmentShader);
        setFailure(QStringLiteral("OpenGL shader program creation failed"), takeGlError());
        return false;
    }
    m_functions->glAttachShader(program, vertexShader);
    m_functions->glAttachShader(program, fragmentShader);
    m_functions->glLinkProgram(program);
    int linked = GL_FALSE;
    m_functions->glGetProgramiv(program, GL_LINK_STATUS, &linked);
    const QString linkLog = programInfoLog(m_functions, program);
    m_functions->glDeleteShader(vertexShader);
    m_functions->glDeleteShader(fragmentShader);
    if (linked != GL_TRUE) {
        m_functions->glDeleteProgram(program);
        m_status.shaderDiagnostic = QStringLiteral("shader program link log: %1").arg(linkLog);
        setFailure(m_status.shaderDiagnostic, takeGlError());
        return false;
    }

    m_program = program;
    m_modelLocation = m_functions->glGetUniformLocation(m_program, "uModel");
    m_viewLocation = m_functions->glGetUniformLocation(m_program, "uView");
    m_projectionLocation = m_functions->glGetUniformLocation(m_program, "uProjection");
    m_normalMatrixLocation = m_functions->glGetUniformLocation(m_program, "uNormalMatrix");
    m_lightDirectionLocation = m_functions->glGetUniformLocation(m_program, "uLightDirection");
    if (m_modelLocation < 0 || m_viewLocation < 0 || m_projectionLocation < 0
        || m_normalMatrixLocation < 0 || m_lightDirectionLocation < 0) {
        m_functions->glDeleteProgram(m_program);
        m_program = 0;
        setFailure(QStringLiteral("linked shader is missing one or more required uniforms"), takeGlError());
        return false;
    }

    m_status.shaderReady = true;
    m_status.shaderDiagnostic = QStringLiteral("vertex and fragment GLSL 330 core shaders compiled and linked");
    const quint32 error = takeGlError();
    if (error != 0) {
        setFailure(QStringLiteral("shader setup produced an OpenGL error"), error);
        return false;
    }
    return true;
}

void MeshRenderer::deleteMeshResource()
{
    if (m_functions == nullptr) {
        m_vertexArray = 0;
        m_vertexBuffer = 0;
        m_indexBuffer = 0;
        return;
    }
    if (m_indexBuffer != 0) {
        m_functions->glDeleteBuffers(1, &m_indexBuffer);
    }
    if (m_vertexBuffer != 0) {
        m_functions->glDeleteBuffers(1, &m_vertexBuffer);
    }
    if (m_vertexArray != 0) {
        m_functions->glDeleteVertexArrays(1, &m_vertexArray);
    }
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
}

void MeshRenderer::prepareFrame(int framebufferWidth, int framebufferHeight)
{
    const int width = std::max(1, framebufferWidth);
    const int height = std::max(1, framebufferHeight);
    m_functions->glViewport(0, 0, width, height);
    m_functions->glEnable(GL_DEPTH_TEST);
    m_functions->glDepthFunc(GL_LESS);
    m_functions->glDisable(GL_CULL_FACE);
    m_functions->glClearColor(0.055f, 0.075f, 0.105f, 1.0f);
    m_functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_status.depthTestEnabled = true;
    m_status.faceCullingEnabled = false;
}

void MeshRenderer::setFailure(const QString& message, quint32 glError)
{
    m_status.lastError = message;
    if (glError != 0) {
        m_status.lastGlError = glError;
    }
}

quint32 MeshRenderer::takeGlError()
{
    if (m_functions == nullptr) {
        return 0;
    }
    return static_cast<quint32>(m_functions->glGetError());
}

void MeshRenderer::clearGlErrors()
{
    if (m_functions == nullptr) {
        return;
    }
    while (m_functions->glGetError() != GL_NO_ERROR) {
    }
}

} // namespace vision3d
