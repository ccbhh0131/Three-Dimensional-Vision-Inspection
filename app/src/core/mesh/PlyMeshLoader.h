#pragma once

#include "MeshData.h"

#include <QString>

namespace vision3d {

enum class MeshLoadError
{
    None,
    FileOpenFailed,
    InvalidHeader,
    UnsupportedFormat,
    UnsupportedProperty,
    UnexpectedEOF,
    InvalidVertexIndex,
    NonTriangleFace,
    InvalidData
};

QString meshLoadErrorToString(MeshLoadError error);

struct MeshLoadMetrics
{
    double loadMilliseconds = 0.0;
    double normalGenerationMilliseconds = 0.0;
    double totalMilliseconds = 0.0;
    qsizetype approximateCpuBytes = 0;
};

struct MeshLoadResult
{
    bool success = false;
    MeshLoadError errorCode = MeshLoadError::None;
    QString message;
    MeshData mesh;
    MeshLoadMetrics metrics;
};

class PlyMeshLoader
{
public:
    static MeshLoadResult load(const QString& path);
};

} // namespace vision3d
