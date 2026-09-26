#include "MarkerRenderer.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include <cmath>
#include <cstddef>
#include <vector>

namespace vision3d {
namespace {

struct GPUMarker
{
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float red = 1.0f;
    float green = 0.2f;
    float blue = 0.05f;
    float alpha = 1.0f;
};

static_assert(sizeof(GPUMarker) == 28, "GPUMarker layout must remain 28 bytes");

constexpr char kVertexShader[] = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uPointSize;

out vec4 vColor;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    gl_PointSize = uPointSize;
    vColor = aColor;
}
)GLSL";

constexpr char kFragmentShader[] = R"GLSL(
#version 330 core

in vec4 vColor;
out vec4 fragColor;

void main()
{
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    if (dot(centered, centered) > 1.0) {
        discard;
    }
    fragColor = vColor;
}
)GLSL";

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

bool isFinitePosition(const QVector3D& position)
{
    return std::isfinite(position.x())
        && std::isfinite(position.y())
        && std::isfinite(position.z());
}

} // namespace

MarkerRenderer::~MarkerRenderer()
{
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_functions = nullptr;
    m_context = nullptr;
}

bool MarkerRenderer::initialize(QOpenGLContext* context)
{
    m_status = MarkerRendererStatus();
    m_context = context;
    m_functions = nullptr;
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_modelLocation = -1;
    m_viewLocation = -1;
    m_projectionLocation = -1;
    m_pointSizeLocation = -1;
    if (m_context == nullptr || QOpenGLContext::currentContext() != m_context) {
        setFailure(QStringLiteral("marker renderer initialization requires its current context"));
        return false;
    }

    m_functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(m_context);
    if (m_functions == nullptr || !m_functions->initializeOpenGLFunctions()) {
        setFailure(QStringLiteral("QOpenGLFunctions_3_3_Core could not be initialized"));
        m_functions = nullptr;
        return false;
    }
    m_status.contextReady = true;
    clearGlErrors();
    if (!compileAndLinkProgram()) {
        return false;
    }
    m_markersPendingUpload = true;
    return true;
}

bool MarkerRenderer::release(QOpenGLContext* expectedContext)
{
    if (m_functions != nullptr
        && (expectedContext == nullptr || QOpenGLContext::currentContext() != expectedContext)) {
        invalidateContext();
        return false;
    }
    if (m_functions != nullptr) {
        deleteMarkerResource();
        if (m_program != 0) {
            m_functions->glDeleteProgram(m_program);
        }
    }
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_functions = nullptr;
    m_context = nullptr;
    m_modelLocation = -1;
    m_viewLocation = -1;
    m_projectionLocation = -1;
    m_pointSizeLocation = -1;
    m_status = MarkerRendererStatus();
    return true;
}

void MarkerRenderer::invalidateContext()
{
    m_program = 0;
    m_vertexArray = 0;
    m_vertexBuffer = 0;
    m_functions = nullptr;
    m_context = nullptr;
    m_modelLocation = -1;
    m_viewLocation = -1;
    m_projectionLocation = -1;
    m_pointSizeLocation = -1;
    m_status.contextReady = false;
    m_status.shaderReady = false;
    m_status.drawSucceeded = false;
    m_status.lastGlError = 0;
    m_status.markerCount = 0;
    m_status.lastError = QStringLiteral(
        "OpenGL context was invalidated; CPU marker view data is retained for a later rebuild");
    m_markersPendingUpload = true;
}

void MarkerRenderer::setMarkers(const QVector<DeviceMarkerView>& markers)
{
    m_markers = markers;
    m_markersPendingUpload = true;
}

bool MarkerRenderer::clearMarkers()
{
    m_markers.clear();
    m_markersPendingUpload = false;
    m_status.markerCount = 0;
    ++m_status.updateCount;
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        m_vertexArray = 0;
        m_vertexBuffer = 0;
        return true;
    }
    deleteMarkerResource();
    return takeGlError() == 0;
}

bool MarkerRenderer::uploadMarkers()
{
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        setFailure(QStringLiteral("marker upload requires the renderer's current OpenGL context"));
        return false;
    }
    if (!m_status.shaderReady) {
        setFailure(QStringLiteral("marker upload requires a linked shader program"));
        return false;
    }
    if (m_markers.isEmpty()) {
        deleteMarkerResource();
        m_status.markerCount = 0;
        m_markersPendingUpload = false;
        ++m_status.updateCount;
        m_status.lastGlError = takeGlError();
        return m_status.lastGlError == 0;
    }

    std::vector<GPUMarker> gpuMarkers;
    gpuMarkers.reserve(static_cast<std::size_t>(m_markers.size()));
    for (const DeviceMarkerView& marker : m_markers) {
        if (marker.id.trimmed().isEmpty() || !isFinitePosition(marker.worldPosition)) {
            setFailure(QStringLiteral("marker view contains an invalid id or world position"));
            return false;
        }
        GPUMarker gpuMarker;
        gpuMarker.px = marker.worldPosition.x();
        gpuMarker.py = marker.worldPosition.y();
        gpuMarker.pz = marker.worldPosition.z();
        if (marker.selected) {
            gpuMarker.red = 1.0f;
            gpuMarker.green = 0.85f;
            gpuMarker.blue = 0.05f;
        }
        gpuMarkers.push_back(gpuMarker);
    }

    clearGlErrors();
    if (m_vertexArray == 0 || m_vertexBuffer == 0) {
        m_functions->glGenVertexArrays(1, &m_vertexArray);
        m_functions->glGenBuffers(1, &m_vertexBuffer);
    }
    if (m_vertexArray == 0 || m_vertexBuffer == 0) {
        setFailure(QStringLiteral("marker OpenGL object allocation failed"), takeGlError());
        return false;
    }
    m_functions->glBindVertexArray(m_vertexArray);
    m_functions->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    m_functions->glBufferData(GL_ARRAY_BUFFER,
                              static_cast<GLsizeiptr>(gpuMarkers.size() * sizeof(GPUMarker)),
                              gpuMarkers.data(),
                              GL_DYNAMIC_DRAW);
    m_functions->glEnableVertexAttribArray(0);
    m_functions->glVertexAttribPointer(0,
                                       3,
                                       GL_FLOAT,
                                       GL_FALSE,
                                       sizeof(GPUMarker),
                                       nullptr);
    m_functions->glEnableVertexAttribArray(1);
    m_functions->glVertexAttribPointer(1,
                                       4,
                                       GL_FLOAT,
                                       GL_FALSE,
                                       sizeof(GPUMarker),
                                       reinterpret_cast<const void*>(3 * sizeof(float)));
    m_functions->glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_functions->glBindVertexArray(0);
    const quint32 error = takeGlError();
    if (error != 0) {
        setFailure(QStringLiteral("marker buffer upload failed"), error);
        return false;
    }
    m_status.markerCount = m_markers.size();
    ++m_status.uploadCount;
    ++m_status.updateCount;
    m_status.lastGlError = 0;
    m_status.lastError.clear();
    m_markersPendingUpload = false;
    return true;
}

bool MarkerRenderer::draw(const QMatrix4x4& model,
                          const QMatrix4x4& view,
                          const QMatrix4x4& projection)
{
    if (m_functions == nullptr || m_context == nullptr
        || QOpenGLContext::currentContext() != m_context) {
        setFailure(QStringLiteral("marker draw requires the renderer's current OpenGL context"));
        return false;
    }
    if (m_markersPendingUpload && !uploadMarkers()) {
        return false;
    }
    m_status.drawSucceeded = false;
    if (m_status.markerCount == 0) {
        m_status.drawSucceeded = true;
        m_status.lastGlError = 0;
        return true;
    }

    clearGlErrors();
    m_functions->glUseProgram(m_program);
    m_functions->glUniformMatrix4fv(m_modelLocation, 1, GL_FALSE, model.constData());
    m_functions->glUniformMatrix4fv(m_viewLocation, 1, GL_FALSE, view.constData());
    m_functions->glUniformMatrix4fv(m_projectionLocation,
                                    1,
                                    GL_FALSE,
                                    projection.constData());
    m_functions->glUniform1f(m_pointSizeLocation, 14.0f);
    m_functions->glEnable(GL_PROGRAM_POINT_SIZE);
    m_functions->glEnable(GL_DEPTH_TEST);
    m_functions->glDepthFunc(GL_LEQUAL);
    m_functions->glBindVertexArray(m_vertexArray);
    m_functions->glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_status.markerCount));
    m_functions->glBindVertexArray(0);
    m_functions->glDepthFunc(GL_LESS);
    m_functions->glDisable(GL_PROGRAM_POINT_SIZE);
    m_functions->glUseProgram(0);

    m_status.lastGlError = takeGlError();
    m_status.drawSucceeded = m_status.lastGlError == 0;
    if (m_status.drawSucceeded) {
        ++m_status.drawCalls;
        m_status.lastError.clear();
    } else {
        m_status.lastError = QStringLiteral("marker draw produced an OpenGL error");
    }
    return m_status.drawSucceeded;
}

const MarkerRendererStatus& MarkerRenderer::status() const
{
    return m_status;
}

bool MarkerRenderer::compileAndLinkProgram()
{
    const quint32 vertexShader = m_functions->glCreateShader(GL_VERTEX_SHADER);
    const quint32 fragmentShader = m_functions->glCreateShader(GL_FRAGMENT_SHADER);
    if (vertexShader == 0 || fragmentShader == 0) {
        setFailure(QStringLiteral("marker shader allocation failed"));
        return false;
    }

    const GLchar* vertexSource = kVertexShader;
    const GLchar* fragmentSource = kFragmentShader;
    m_functions->glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    m_functions->glCompileShader(vertexShader);
    int vertexCompiled = GL_FALSE;
    m_functions->glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexCompiled);
    if (vertexCompiled != GL_TRUE) {
        m_status.shaderDiagnostic = QStringLiteral("marker vertex shader: %1")
                                        .arg(shaderInfoLog(m_functions, vertexShader));
        m_functions->glDeleteShader(vertexShader);
        m_functions->glDeleteShader(fragmentShader);
        setFailure(m_status.shaderDiagnostic, takeGlError());
        return false;
    }

    m_functions->glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    m_functions->glCompileShader(fragmentShader);
    int fragmentCompiled = GL_FALSE;
    m_functions->glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentCompiled);
    if (fragmentCompiled != GL_TRUE) {
        m_status.shaderDiagnostic = QStringLiteral("marker fragment shader: %1")
                                        .arg(shaderInfoLog(m_functions, fragmentShader));
        m_functions->glDeleteShader(vertexShader);
        m_functions->glDeleteShader(fragmentShader);
        setFailure(m_status.shaderDiagnostic, takeGlError());
        return false;
    }

    m_program = m_functions->glCreateProgram();
    if (m_program == 0) {
        m_functions->glDeleteShader(vertexShader);
        m_functions->glDeleteShader(fragmentShader);
        setFailure(QStringLiteral("marker shader program allocation failed"));
        return false;
    }
    m_functions->glAttachShader(m_program, vertexShader);
    m_functions->glAttachShader(m_program, fragmentShader);
    m_functions->glLinkProgram(m_program);
    int linked = GL_FALSE;
    m_functions->glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    m_functions->glDeleteShader(vertexShader);
    m_functions->glDeleteShader(fragmentShader);
    if (linked != GL_TRUE) {
        m_status.shaderDiagnostic = QStringLiteral("marker shader link: %1")
                                        .arg(programInfoLog(m_functions, m_program));
        m_functions->glDeleteProgram(m_program);
        m_program = 0;
        setFailure(m_status.shaderDiagnostic, takeGlError());
        return false;
    }

    m_modelLocation = m_functions->glGetUniformLocation(m_program, "uModel");
    m_viewLocation = m_functions->glGetUniformLocation(m_program, "uView");
    m_projectionLocation = m_functions->glGetUniformLocation(m_program, "uProjection");
    m_pointSizeLocation = m_functions->glGetUniformLocation(m_program, "uPointSize");
    if (m_modelLocation < 0 || m_viewLocation < 0 || m_projectionLocation < 0
        || m_pointSizeLocation < 0) {
        setFailure(QStringLiteral("marker shader uniform lookup failed"), takeGlError());
        return false;
    }
    m_status.shaderReady = true;
    m_status.shaderDiagnostic = QStringLiteral("marker GLSL 330 core point shader compiled and linked");
    m_status.lastGlError = takeGlError();
    return m_status.lastGlError == 0;
}

void MarkerRenderer::deleteMarkerResource()
{
    if (m_functions == nullptr) {
        m_vertexArray = 0;
        m_vertexBuffer = 0;
        return;
    }
    if (m_vertexBuffer != 0) {
        m_functions->glDeleteBuffers(1, &m_vertexBuffer);
    }
    if (m_vertexArray != 0) {
        m_functions->glDeleteVertexArrays(1, &m_vertexArray);
    }
    m_vertexArray = 0;
    m_vertexBuffer = 0;
}

void MarkerRenderer::setFailure(const QString& message, quint32 glError)
{
    m_status.lastError = message;
    if (glError != 0) {
        m_status.lastGlError = glError;
    }
}

quint32 MarkerRenderer::takeGlError()
{
    if (m_functions == nullptr) {
        return 0;
    }
    quint32 result = GL_NO_ERROR;
    quint32 error = GL_NO_ERROR;
    while ((error = m_functions->glGetError()) != GL_NO_ERROR) {
        result = error;
    }
    return result;
}

void MarkerRenderer::clearGlErrors()
{
    (void)takeGlError();
}

} // namespace vision3d
