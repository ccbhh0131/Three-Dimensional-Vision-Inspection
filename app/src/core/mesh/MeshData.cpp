#include "MeshData.h"

#include <QString>

#include <cmath>
#include <limits>

namespace vision3d {
namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isFinite(const QVector3D& value)
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z());
}

} // namespace

qsizetype MeshData::triangleCount() const
{
    return indices.size() / 3;
}

qsizetype MeshData::approximateCpuBytes() const
{
    const qsizetype vertexBytes = vertices.size() <= std::numeric_limits<qsizetype>::max() / static_cast<qsizetype>(sizeof(MeshVertex))
        ? vertices.size() * static_cast<qsizetype>(sizeof(MeshVertex))
        : std::numeric_limits<qsizetype>::max();

    const qsizetype indexBytes = indices.size() <= std::numeric_limits<qsizetype>::max() / static_cast<qsizetype>(sizeof(quint32))
        ? indices.size() * static_cast<qsizetype>(sizeof(quint32))
        : std::numeric_limits<qsizetype>::max();

    if (vertexBytes == std::numeric_limits<qsizetype>::max()
        || indexBytes == std::numeric_limits<qsizetype>::max()
        || vertexBytes > std::numeric_limits<qsizetype>::max() - indexBytes) {
        return std::numeric_limits<qsizetype>::max();
    }

    return vertexBytes + indexBytes;
}

bool MeshData::isValid(QString* errorMessage) const
{
    if (vertices.isEmpty()) {
        setError(errorMessage, QStringLiteral("mesh has no vertices"));
        return false;
    }

    if (boundingBox.isEmpty()) {
        setError(errorMessage, QStringLiteral("mesh bounding box is empty"));
        return false;
    }

    for (qsizetype vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
        const MeshVertex& vertex = vertices.at(vertexIndex);
        if (!isFinite(vertex.position)) {
            setError(errorMessage, QStringLiteral("mesh contains a non-finite vertex position"));
            return false;
        }
        if (hasNormals && (!isFinite(vertex.normal)
                           || QVector3D::dotProduct(vertex.normal, vertex.normal) <= 0.0f)) {
            setError(errorMessage, QStringLiteral("mesh contains an invalid vertex normal"));
            return false;
        }
    }

    if (indices.size() % 3 != 0) {
        setError(errorMessage, QStringLiteral("triangle index count is not divisible by three"));
        return false;
    }

    if (primitive == MeshPrimitive::Points && !indices.isEmpty()) {
        setError(errorMessage, QStringLiteral("point mesh must not contain triangle indices"));
        return false;
    }

    if (primitive == MeshPrimitive::Triangles && indices.isEmpty()) {
        setError(errorMessage, QStringLiteral("triangle mesh has no triangle indices"));
        return false;
    }

    for (qsizetype indexPosition = 0; indexPosition < indices.size(); ++indexPosition) {
        if (indices.at(indexPosition) >= static_cast<quint32>(vertices.size())) {
            setError(errorMessage, QStringLiteral("triangle index is outside the vertex array"));
            return false;
        }
    }

    return true;
}

} // namespace vision3d
