#include "PlyMeshLoader.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QList>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace vision3d {
namespace {

constexpr qint64 kMaxHeaderBytes = 1024 * 1024;
constexpr qint64 kMaxHeaderLineBytes = 64 * 1024;
constexpr quint64 kMaxElementCount = 10'000'000;
constexpr float kNormalLengthSquaredEpsilon = 1.0e-20f;

enum class PlyScalarType
{
    Float32,
    UChar,
    Int32
};

struct PlyProperty
{
    QString name;
    PlyScalarType type = PlyScalarType::Float32;
    bool isList = false;
    PlyScalarType listCountType = PlyScalarType::UChar;
    PlyScalarType listItemType = PlyScalarType::Int32;
};

struct PlyElement
{
    QString name;
    quint64 count = 0;
    QVector<PlyProperty> properties;
};

struct HeaderParseResult
{
    bool success = false;
    MeshLoadError errorCode = MeshLoadError::InvalidHeader;
    QString message;
    QVector<PlyElement> elements;
};

HeaderParseResult headerFailure(MeshLoadError errorCode, const QString& message)
{
    HeaderParseResult result;
    result.errorCode = errorCode;
    result.message = message;
    return result;
}

bool parseScalarType(const QByteArray& token, PlyScalarType& type)
{
    if (token == "float") {
        type = PlyScalarType::Float32;
        return true;
    }
    if (token == "uchar") {
        type = PlyScalarType::UChar;
        return true;
    }
    if (token == "int") {
        type = PlyScalarType::Int32;
        return true;
    }
    return false;
}

HeaderParseResult parseHeader(QFile& file)
{
    HeaderParseResult result;
    bool firstLine = true;
    bool formatSeen = false;
    bool endHeaderSeen = false;
    int currentElementIndex = -1;
    qint64 headerBytes = 0;

    while (!file.atEnd()) {
        const QByteArray line = file.readLine(kMaxHeaderLineBytes + 1);
        if (line.isEmpty()) {
            break;
        }

        headerBytes += line.size();
        if (line.size() > kMaxHeaderLineBytes || headerBytes > kMaxHeaderBytes) {
            return headerFailure(
                MeshLoadError::InvalidHeader,
                QStringLiteral("PLY header exceeds the controlled size limit"));
        }

        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const QList<QByteArray> tokens = trimmed.simplified().split(' ');
        const QByteArray keyword = tokens.value(0);

        if (firstLine) {
            firstLine = false;
            if (tokens.size() != 1 || keyword != "ply") {
                return headerFailure(
                    MeshLoadError::InvalidHeader,
                    QStringLiteral("PLY header must start with 'ply'"));
            }
            continue;
        }

        if (keyword == "format") {
            if (tokens.size() != 3) {
                return headerFailure(
                    MeshLoadError::InvalidHeader,
                    QStringLiteral("PLY format declaration is malformed"));
            }
            formatSeen = true;
            if (tokens.at(1) != "binary_little_endian" || tokens.at(2) != "1.0") {
                return headerFailure(
                    MeshLoadError::UnsupportedFormat,
                    QStringLiteral("only PLY binary_little_endian 1.0 is supported"));
            }
            continue;
        }

        if (keyword == "comment" || keyword == "obj_info") {
            continue;
        }

        if (keyword == "element") {
            if (tokens.size() != 3) {
                return headerFailure(
                    MeshLoadError::InvalidHeader,
                    QStringLiteral("PLY element declaration is malformed"));
            }

            const QByteArray elementName = tokens.at(1);
            if (elementName != "vertex" && elementName != "face") {
                return headerFailure(
                    MeshLoadError::UnsupportedProperty,
                    QStringLiteral("unsupported PLY element '%1'")
                        .arg(QString::fromLatin1(elementName)));
            }

            bool countOk = false;
            const qulonglong count = tokens.at(2).toULongLong(&countOk);
            if (!countOk || count > kMaxElementCount) {
                return headerFailure(
                    MeshLoadError::InvalidData,
                    QStringLiteral("PLY element count is invalid or exceeds the controlled limit"));
            }

            for (const PlyElement& existing : result.elements) {
                if (existing.name == QString::fromLatin1(elementName)) {
                    return headerFailure(
                        MeshLoadError::InvalidHeader,
                        QStringLiteral("duplicate PLY element '%1'")
                            .arg(QString::fromLatin1(elementName)));
                }
            }

            PlyElement element;
            element.name = QString::fromLatin1(elementName);
            element.count = static_cast<quint64>(count);
            result.elements.append(element);
            currentElementIndex = result.elements.size() - 1;
            continue;
        }

        if (keyword == "property") {
            if (currentElementIndex < 0 || currentElementIndex >= result.elements.size()) {
                return headerFailure(
                    MeshLoadError::InvalidHeader,
                    QStringLiteral("PLY property appears before an element"));
            }

            PlyProperty property;
            if (tokens.value(1) == "list") {
                if (tokens.size() != 5) {
                    return headerFailure(
                        MeshLoadError::InvalidHeader,
                        QStringLiteral("PLY list property declaration is malformed"));
                }
                if (!parseScalarType(tokens.at(2), property.listCountType)
                    || !parseScalarType(tokens.at(3), property.listItemType)) {
                    return headerFailure(
                        MeshLoadError::UnsupportedProperty,
                        QStringLiteral("PLY list property uses an unsupported scalar type"));
                }
                property.isList = true;
                property.name = QString::fromLatin1(tokens.at(4));
                if (result.elements.at(currentElementIndex).name != QStringLiteral("face")
                    || property.name != QStringLiteral("vertex_indices")
                    || property.listCountType != PlyScalarType::UChar
                    || property.listItemType != PlyScalarType::Int32) {
                    return headerFailure(
                        MeshLoadError::UnsupportedProperty,
                        QStringLiteral("only face property list uchar int vertex_indices is supported"));
                }
            } else {
                if (tokens.size() != 3 || !parseScalarType(tokens.at(1), property.type)) {
                    return headerFailure(
                        MeshLoadError::UnsupportedProperty,
                        QStringLiteral("PLY property uses an unsupported scalar type"));
                }
                property.name = QString::fromLatin1(tokens.at(2));
            }

            result.elements[currentElementIndex].properties.append(property);
            continue;
        }

        if (keyword == "end_header") {
            if (tokens.size() != 1) {
                return headerFailure(
                    MeshLoadError::InvalidHeader,
                    QStringLiteral("PLY end_header declaration is malformed"));
            }
            endHeaderSeen = true;
            break;
        }

        return headerFailure(
            MeshLoadError::InvalidHeader,
            QStringLiteral("unsupported or malformed PLY header line '%1'")
                .arg(QString::fromUtf8(trimmed)));
    }

    if (firstLine || !endHeaderSeen) {
        return headerFailure(
            MeshLoadError::InvalidHeader,
            QStringLiteral("PLY header does not contain end_header"));
    }
    if (!formatSeen) {
        return headerFailure(
            MeshLoadError::InvalidHeader,
            QStringLiteral("PLY header does not contain a format declaration"));
    }

    int vertexIndex = -1;
    int faceIndex = -1;
    for (int index = 0; index < result.elements.size(); ++index) {
        if (result.elements.at(index).name == QStringLiteral("vertex")) {
            vertexIndex = index;
        } else if (result.elements.at(index).name == QStringLiteral("face")) {
            faceIndex = index;
        }
    }

    if (vertexIndex < 0) {
        return headerFailure(
            MeshLoadError::InvalidHeader,
            QStringLiteral("PLY header does not contain a vertex element"));
    }
    if (faceIndex >= 0 && faceIndex < vertexIndex) {
        return headerFailure(
            MeshLoadError::InvalidHeader,
            QStringLiteral("face element must follow vertex element"));
    }

    result.success = true;
    result.errorCode = MeshLoadError::None;
    return result;
}

bool validateVertexProperties(
    const PlyElement& element,
    bool& hasStoredNormals,
    bool& hasStoredColors,
    MeshLoadError& errorCode,
    QString& errorMessage)
{
    bool seenX = false;
    bool seenY = false;
    bool seenZ = false;
    bool seenNX = false;
    bool seenNY = false;
    bool seenNZ = false;
    bool seenRed = false;
    bool seenGreen = false;
    bool seenBlue = false;

    for (const PlyProperty& property : element.properties) {
        if (property.isList) {
            errorCode = MeshLoadError::UnsupportedProperty;
            errorMessage = QStringLiteral("vertex list properties are not supported");
            return false;
        }

        const bool isPosition = property.name == QStringLiteral("x")
            || property.name == QStringLiteral("y")
            || property.name == QStringLiteral("z");
        const bool isNormal = property.name == QStringLiteral("nx")
            || property.name == QStringLiteral("ny")
            || property.name == QStringLiteral("nz");
        const bool isColor = property.name == QStringLiteral("red")
            || property.name == QStringLiteral("green")
            || property.name == QStringLiteral("blue");

        if ((isPosition || isNormal || property.name == QStringLiteral("value"))
            && property.type != PlyScalarType::Float32) {
            errorCode = MeshLoadError::UnsupportedProperty;
            errorMessage = QStringLiteral("property '%1' must use float")
                .arg(property.name);
            return false;
        }

        if (isColor && property.type != PlyScalarType::UChar) {
            errorCode = MeshLoadError::UnsupportedProperty;
            errorMessage = QStringLiteral("color property '%1' must use uchar")
                .arg(property.name);
            return false;
        }

        bool* seen = nullptr;
        if (property.name == QStringLiteral("x")) {
            seen = &seenX;
        } else if (property.name == QStringLiteral("y")) {
            seen = &seenY;
        } else if (property.name == QStringLiteral("z")) {
            seen = &seenZ;
        } else if (property.name == QStringLiteral("nx")) {
            seen = &seenNX;
        } else if (property.name == QStringLiteral("ny")) {
            seen = &seenNY;
        } else if (property.name == QStringLiteral("nz")) {
            seen = &seenNZ;
        } else if (property.name == QStringLiteral("red")) {
            seen = &seenRed;
        } else if (property.name == QStringLiteral("green")) {
            seen = &seenGreen;
        } else if (property.name == QStringLiteral("blue")) {
            seen = &seenBlue;
        }

        if (seen != nullptr) {
            if (*seen) {
                errorCode = MeshLoadError::InvalidHeader;
                errorMessage = QStringLiteral("duplicate vertex property '%1'")
                    .arg(property.name);
                return false;
            }
            *seen = true;
        }
    }

    if (!seenX || !seenY || !seenZ) {
        errorCode = MeshLoadError::InvalidHeader;
        errorMessage = QStringLiteral("vertex element must contain float x, y and z properties");
        return false;
    }

    const int colorCount = static_cast<int>(seenRed) + static_cast<int>(seenGreen) + static_cast<int>(seenBlue);
    if (colorCount != 0 && colorCount != 3) {
        errorCode = MeshLoadError::InvalidData;
        errorMessage = QStringLiteral("red, green and blue properties must appear together");
        return false;
    }

    const int normalCount = static_cast<int>(seenNX) + static_cast<int>(seenNY) + static_cast<int>(seenNZ);
    if (normalCount != 0 && normalCount != 3) {
        errorCode = MeshLoadError::InvalidData;
        errorMessage = QStringLiteral("nx, ny and nz properties must appear together");
        return false;
    }

    hasStoredColors = colorCount == 3;
    hasStoredNormals = normalCount == 3;
    return true;
}

bool validateFaceProperties(
    const PlyElement& element,
    MeshLoadError& errorCode,
    QString& errorMessage)
{
    int indexPropertyCount = 0;
    for (const PlyProperty& property : element.properties) {
        if (property.isList) {
            if (property.name != QStringLiteral("vertex_indices")
                || property.listCountType != PlyScalarType::UChar
                || property.listItemType != PlyScalarType::Int32) {
                errorCode = MeshLoadError::UnsupportedProperty;
                errorMessage = QStringLiteral("only face property list uchar int vertex_indices is supported");
                return false;
            }
            ++indexPropertyCount;
        }
    }

    if (indexPropertyCount != 1) {
        errorCode = MeshLoadError::InvalidHeader;
        errorMessage = QStringLiteral("face element must contain exactly one vertex_indices list");
        return false;
    }
    return true;
}

qint64 scalarByteSize(PlyScalarType type)
{
    switch (type) {
    case PlyScalarType::Float32:
        return 4;
    case PlyScalarType::UChar:
        return 1;
    case PlyScalarType::Int32:
        return 4;
    }
    return 0;
}

qint64 recordByteSize(const PlyElement& element)
{
    qint64 size = 0;
    for (const PlyProperty& property : element.properties) {
        const qint64 propertySize = property.isList
            ? 1 + 3 * 4
            : scalarByteSize(property.type);
        if (propertySize <= 0 || size > std::numeric_limits<qint64>::max() - propertySize) {
            return 0;
        }
        size += propertySize;
    }
    return size;
}

bool payloadHasAtLeast(QFile& file, quint64 recordCount, qint64 recordSize)
{
    if (recordSize <= 0 || file.pos() < 0 || file.size() < file.pos()) {
        return false;
    }
    const quint64 remaining = static_cast<quint64>(file.size() - file.pos());
    return recordCount <= remaining / static_cast<quint64>(recordSize);
}

bool readBytes(QFile& file, char* destination, qint64 byteCount)
{
    return byteCount >= 0 && file.read(destination, byteCount) == byteCount;
}

bool readUChar(QFile& file, quint8& value)
{
    char byte = 0;
    if (!readBytes(file, &byte, 1)) {
        return false;
    }
    value = static_cast<quint8>(static_cast<unsigned char>(byte));
    return true;
}

bool readFloat32(QFile& file, float& value)
{
    char bytes[sizeof(float)] = {};
    if (!readBytes(file, bytes, sizeof(bytes))) {
        return false;
    }

    const quint32 bits = static_cast<quint32>(static_cast<unsigned char>(bytes[0]))
        | (static_cast<quint32>(static_cast<unsigned char>(bytes[1])) << 8)
        | (static_cast<quint32>(static_cast<unsigned char>(bytes[2])) << 16)
        | (static_cast<quint32>(static_cast<unsigned char>(bytes[3])) << 24);
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool readInt32(QFile& file, qint32& value)
{
    char bytes[sizeof(qint32)] = {};
    if (!readBytes(file, bytes, sizeof(bytes))) {
        return false;
    }

    const quint32 bits = static_cast<quint32>(static_cast<unsigned char>(bytes[0]))
        | (static_cast<quint32>(static_cast<unsigned char>(bytes[1])) << 8)
        | (static_cast<quint32>(static_cast<unsigned char>(bytes[2])) << 16)
        | (static_cast<quint32>(static_cast<unsigned char>(bytes[3])) << 24);
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool skipScalar(QFile& file, PlyScalarType type)
{
    switch (type) {
    case PlyScalarType::Float32: {
        float value = 0.0f;
        return readFloat32(file, value);
    }
    case PlyScalarType::UChar: {
        quint8 value = 0;
        return readUChar(file, value);
    }
    case PlyScalarType::Int32: {
        qint32 value = 0;
        return readInt32(file, value);
    }
    }
    return false;
}

bool isFinite(const QVector3D& value)
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z());
}

void deriveNormals(MeshData& mesh)
{
    for (MeshVertex& vertex : mesh.vertices) {
        vertex.normal = QVector3D(0.0f, 0.0f, 0.0f);
    }

    for (qsizetype indexPosition = 0; indexPosition + 2 < mesh.indices.size(); indexPosition += 3) {
        const quint32 ia = mesh.indices.at(indexPosition);
        const quint32 ib = mesh.indices.at(indexPosition + 1);
        const quint32 ic = mesh.indices.at(indexPosition + 2);
        const QVector3D& a = mesh.vertices.at(static_cast<qsizetype>(ia)).position;
        const QVector3D& b = mesh.vertices.at(static_cast<qsizetype>(ib)).position;
        const QVector3D& c = mesh.vertices.at(static_cast<qsizetype>(ic)).position;

        const QVector3D faceNormal = QVector3D::crossProduct(b - a, c - a);
        const float lengthSquared = QVector3D::dotProduct(faceNormal, faceNormal);
        if (!isFinite(faceNormal) || !std::isfinite(lengthSquared)
            || lengthSquared <= kNormalLengthSquaredEpsilon) {
            continue;
        }

        mesh.vertices[static_cast<qsizetype>(ia)].normal += faceNormal;
        mesh.vertices[static_cast<qsizetype>(ib)].normal += faceNormal;
        mesh.vertices[static_cast<qsizetype>(ic)].normal += faceNormal;
    }

    for (MeshVertex& vertex : mesh.vertices) {
        const float lengthSquared = QVector3D::dotProduct(vertex.normal, vertex.normal);
        if (!isFinite(vertex.normal) || !std::isfinite(lengthSquared)
            || lengthSquared <= kNormalLengthSquaredEpsilon) {
            vertex.normal = QVector3D(0.0f, 0.0f, 1.0f);
            continue;
        }
        vertex.normal.normalize();
    }

    mesh.hasNormals = true;
    mesh.normalsDerived = true;
}

MeshLoadResult makeFailure(
    MeshLoadError errorCode,
    const QString& message,
    const MeshLoadMetrics& metrics,
    const QElapsedTimer& totalTimer)
{
    MeshLoadResult result;
    result.success = false;
    result.errorCode = errorCode;
    result.message = message;
    result.metrics = metrics;
    result.metrics.totalMilliseconds =
        static_cast<double>(totalTimer.nsecsElapsed()) / 1'000'000.0;
    return result;
}

} // namespace

QString meshLoadErrorToString(MeshLoadError error)
{
    switch (error) {
    case MeshLoadError::None:
        return QStringLiteral("None");
    case MeshLoadError::FileOpenFailed:
        return QStringLiteral("FileOpenFailed");
    case MeshLoadError::InvalidHeader:
        return QStringLiteral("InvalidHeader");
    case MeshLoadError::UnsupportedFormat:
        return QStringLiteral("UnsupportedFormat");
    case MeshLoadError::UnsupportedProperty:
        return QStringLiteral("UnsupportedProperty");
    case MeshLoadError::UnexpectedEOF:
        return QStringLiteral("UnexpectedEOF");
    case MeshLoadError::InvalidVertexIndex:
        return QStringLiteral("InvalidVertexIndex");
    case MeshLoadError::NonTriangleFace:
        return QStringLiteral("NonTriangleFace");
    case MeshLoadError::InvalidData:
        return QStringLiteral("InvalidData");
    }
    return QStringLiteral("Unknown");
}

MeshLoadResult PlyMeshLoader::load(const QString& path)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    MeshLoadMetrics metrics;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return makeFailure(
            MeshLoadError::FileOpenFailed,
            QStringLiteral("cannot open PLY file '%1': %2").arg(path, file.errorString()),
            metrics,
            totalTimer);
    }

    const HeaderParseResult header = parseHeader(file);
    if (!header.success) {
        return makeFailure(header.errorCode, header.message, metrics, totalTimer);
    }

    PlyElement vertexElement;
    PlyElement faceElement;
    bool hasFaceElement = false;
    for (const PlyElement& element : header.elements) {
        if (element.name == QStringLiteral("vertex")) {
            vertexElement = element;
        } else if (element.name == QStringLiteral("face")) {
            faceElement = element;
            hasFaceElement = true;
        }
    }

    if (vertexElement.count == 0) {
        return makeFailure(
            MeshLoadError::InvalidData,
            QStringLiteral("PLY vertex element is empty"),
            metrics,
            totalTimer);
    }

    MeshLoadError validationError = MeshLoadError::None;
    QString validationMessage;
    bool hasStoredNormals = false;
    bool hasStoredColors = false;
    if (!validateVertexProperties(
            vertexElement,
            hasStoredNormals,
            hasStoredColors,
            validationError,
            validationMessage)) {
        return makeFailure(validationError, validationMessage, metrics, totalTimer);
    }

    if (hasFaceElement
        && !validateFaceProperties(faceElement, validationError, validationMessage)) {
        return makeFailure(validationError, validationMessage, metrics, totalTimer);
    }

    const qint64 vertexRecordSize = recordByteSize(vertexElement);
    if (!payloadHasAtLeast(file, vertexElement.count, vertexRecordSize)) {
        return makeFailure(
            MeshLoadError::UnexpectedEOF,
            QStringLiteral("PLY vertex payload is truncated"),
            metrics,
            totalTimer);
    }

    MeshData mesh;
    mesh.vertices.resize(static_cast<qsizetype>(vertexElement.count));
    mesh.boundingBox.reset();

    for (quint64 vertexIndex = 0; vertexIndex < vertexElement.count; ++vertexIndex) {
        MeshVertex& vertex = mesh.vertices[static_cast<qsizetype>(vertexIndex)];
        for (const PlyProperty& property : vertexElement.properties) {
            if (property.isList) {
                return makeFailure(
                    MeshLoadError::UnsupportedProperty,
                    QStringLiteral("vertex list property '%1' is not supported").arg(property.name),
                    metrics,
                    totalTimer);
            }

            if (property.type == PlyScalarType::Float32) {
                float value = 0.0f;
                if (!readFloat32(file, value)) {
                    return makeFailure(
                        MeshLoadError::UnexpectedEOF,
                        QStringLiteral("unexpected end of file in vertex %1 property '%2'")
                            .arg(vertexIndex)
                            .arg(property.name),
                        metrics,
                        totalTimer);
                }
                if (property.name == QStringLiteral("x")) {
                    vertex.position.setX(value);
                } else if (property.name == QStringLiteral("y")) {
                    vertex.position.setY(value);
                } else if (property.name == QStringLiteral("z")) {
                    vertex.position.setZ(value);
                } else if (property.name == QStringLiteral("nx")) {
                    vertex.normal.setX(value);
                } else if (property.name == QStringLiteral("ny")) {
                    vertex.normal.setY(value);
                } else if (property.name == QStringLiteral("nz")) {
                    vertex.normal.setZ(value);
                }
            } else if (property.type == PlyScalarType::UChar) {
                quint8 value = 0;
                if (!readUChar(file, value)) {
                    return makeFailure(
                        MeshLoadError::UnexpectedEOF,
                        QStringLiteral("unexpected end of file in vertex %1 property '%2'")
                            .arg(vertexIndex)
                            .arg(property.name),
                        metrics,
                        totalTimer);
                }
                if (property.name == QStringLiteral("red")) {
                    vertex.color.red = value;
                } else if (property.name == QStringLiteral("green")) {
                    vertex.color.green = value;
                } else if (property.name == QStringLiteral("blue")) {
                    vertex.color.blue = value;
                }
            } else if (property.type == PlyScalarType::Int32) {
                qint32 value = 0;
                if (!readInt32(file, value)) {
                    return makeFailure(
                        MeshLoadError::UnexpectedEOF,
                        QStringLiteral("unexpected end of file in vertex %1 property '%2'")
                            .arg(vertexIndex)
                            .arg(property.name),
                        metrics,
                        totalTimer);
                }
            }
        }

        if (!isFinite(vertex.position)) {
            return makeFailure(
                MeshLoadError::InvalidData,
                QStringLiteral("vertex %1 contains a non-finite position").arg(vertexIndex),
                metrics,
                totalTimer);
        }
        mesh.boundingBox.expand(vertex.position);
    }

    mesh.hasColors = hasStoredColors;
    if (hasStoredNormals) {
        for (MeshVertex& vertex : mesh.vertices) {
            const float lengthSquared = QVector3D::dotProduct(vertex.normal, vertex.normal);
            if (!isFinite(vertex.normal) || !std::isfinite(lengthSquared)
                || lengthSquared <= kNormalLengthSquaredEpsilon) {
                return makeFailure(
                    MeshLoadError::InvalidData,
                    QStringLiteral("stored vertex normal is invalid"),
                    metrics,
                    totalTimer);
            }
            vertex.normal.normalize();
        }
        mesh.hasNormals = true;
        mesh.normalsDerived = false;
    }

    if (hasFaceElement) {
        const qint64 faceRecordSize = recordByteSize(faceElement);
        if (!payloadHasAtLeast(file, faceElement.count, faceRecordSize)) {
            return makeFailure(
                MeshLoadError::UnexpectedEOF,
                QStringLiteral("PLY face payload is truncated"),
                metrics,
                totalTimer);
        }

        if (faceElement.count > 0) {
            const quint64 indexCount = faceElement.count * 3;
            if (indexCount > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
                return makeFailure(
                    MeshLoadError::InvalidData,
                    QStringLiteral("PLY face index allocation would overflow"),
                    metrics,
                    totalTimer);
            }
            mesh.indices.reserve(static_cast<qsizetype>(indexCount));
        }

        for (quint64 faceIndex = 0; faceIndex < faceElement.count; ++faceIndex) {
            for (const PlyProperty& property : faceElement.properties) {
                if (property.isList) {
                    quint8 listCount = 0;
                    if (!readUChar(file, listCount)) {
                        return makeFailure(
                            MeshLoadError::UnexpectedEOF,
                            QStringLiteral("unexpected end of file in face %1 list count")
                                .arg(faceIndex),
                            metrics,
                            totalTimer);
                    }
                    if (listCount != 3) {
                        return makeFailure(
                            MeshLoadError::NonTriangleFace,
                            QStringLiteral("face %1 contains %2 vertices; only triangles are supported")
                                .arg(faceIndex)
                                .arg(listCount),
                            metrics,
                            totalTimer);
                    }

                    for (int listIndex = 0; listIndex < 3; ++listIndex) {
                        qint32 vertexIndex = 0;
                        if (!readInt32(file, vertexIndex)) {
                            return makeFailure(
                                MeshLoadError::UnexpectedEOF,
                                QStringLiteral("unexpected end of file in face %1 indices")
                                    .arg(faceIndex),
                                metrics,
                                totalTimer);
                        }
                        if (vertexIndex < 0
                            || static_cast<quint64>(vertexIndex) >= vertexElement.count) {
                            return makeFailure(
                                MeshLoadError::InvalidVertexIndex,
                                QStringLiteral("face %1 references vertex index %2 outside [0, %3)")
                                    .arg(faceIndex)
                                    .arg(vertexIndex)
                                    .arg(vertexElement.count),
                                metrics,
                                totalTimer);
                        }
                        mesh.indices.append(static_cast<quint32>(vertexIndex));
                    }
                } else if (!skipScalar(file, property.type)) {
                    return makeFailure(
                        MeshLoadError::UnexpectedEOF,
                        QStringLiteral("unexpected end of file in face %1 property '%2'")
                            .arg(faceIndex)
                            .arg(property.name),
                        metrics,
                        totalTimer);
                }
            }
        }
    }

    mesh.primitive = mesh.indices.isEmpty()
        ? MeshPrimitive::Points
        : MeshPrimitive::Triangles;

    metrics.loadMilliseconds =
        static_cast<double>(totalTimer.nsecsElapsed()) / 1'000'000.0;

    if (!mesh.indices.isEmpty() && !mesh.hasNormals) {
        QElapsedTimer normalTimer;
        normalTimer.start();
        deriveNormals(mesh);
        metrics.normalGenerationMilliseconds =
            static_cast<double>(normalTimer.nsecsElapsed()) / 1'000'000.0;
    }

    QString meshValidationMessage;
    if (!mesh.isValid(&meshValidationMessage)) {
        return makeFailure(
            MeshLoadError::InvalidData,
            meshValidationMessage,
            metrics,
            totalTimer);
    }

    metrics.approximateCpuBytes = mesh.approximateCpuBytes();
    metrics.totalMilliseconds =
        static_cast<double>(totalTimer.nsecsElapsed()) / 1'000'000.0;

    MeshLoadResult result;
    result.success = true;
    result.errorCode = MeshLoadError::None;
    result.mesh = std::move(mesh);
    result.metrics = metrics;
    return result;
}

} // namespace vision3d
