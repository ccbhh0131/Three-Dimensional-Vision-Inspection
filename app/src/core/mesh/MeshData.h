#pragma once

#include "core/geometry/BoundingBox.h"

#include <QVector>
#include <QVector3D>
#include <QtGlobal>

class QString;

namespace vision3d {

struct MeshColor
{
    quint8 red = 255;
    quint8 green = 255;
    quint8 blue = 255;
};

enum class MeshPrimitive
{
    Points,
    Triangles
};

struct MeshVertex
{
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D normal{0.0f, 0.0f, 0.0f};
    MeshColor color;
};

struct MeshData
{
    QVector<MeshVertex> vertices;
    QVector<quint32> indices;
    BoundingBox boundingBox;

    MeshPrimitive primitive = MeshPrimitive::Points;
    bool hasNormals = false;
    bool normalsDerived = false;
    bool hasColors = false;

    qsizetype triangleCount() const;
    qsizetype approximateCpuBytes() const;
    bool isValid(QString* errorMessage = nullptr) const;
};

} // namespace vision3d
