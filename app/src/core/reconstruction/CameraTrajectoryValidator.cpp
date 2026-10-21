#include "core/reconstruction/CameraTrajectoryValidator.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVector>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace vision3d {

namespace {

struct Pose
{
    QString name;
    int frameNumber = -1;
    std::array<double, 4> q{{1.0, 0.0, 0.0, 0.0}};
    std::array<double, 3> t{{0.0, 0.0, 0.0}};
    std::array<double, 3> center{{0.0, 0.0, 0.0}};
};

bool readImageBinary(const QString& path, QVector<Pose>* poses)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    quint64 count = 0;
    stream >> count;
    if (stream.status() != QDataStream::Ok || count > 1000000) {
        return false;
    }
    poses->clear();
    poses->reserve(static_cast<qsizetype>(count));
    for (quint64 index = 0; index < count; ++index) {
        qint32 imageId = 0;
        qint32 cameraId = 0;
        stream >> imageId;
        Pose pose;
        for (double& value : pose.q) {
            stream >> value;
        }
        for (double& value : pose.t) {
            stream >> value;
        }
        stream >> cameraId;
        Q_UNUSED(imageId)
        Q_UNUSED(cameraId)
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
        QByteArray name;
        while (true) {
            char character = 0;
            if (stream.readRawData(&character, 1) != 1) {
                return false;
            }
            if (character == '\0') {
                break;
            }
            name.append(character);
            if (name.size() > 32768) {
                return false;
            }
        }
        quint64 pointCount = 0;
        stream >> pointCount;
        if (stream.status() != QDataStream::Ok
            || pointCount > static_cast<quint64>(std::numeric_limits<int>::max() / 24)
            || pointCount > static_cast<quint64>(file.size() / 24)) {
            return false;
        }
        const qint64 remaining = file.size() - file.pos();
        if (pointCount > static_cast<quint64>(remaining / 24)
            || stream.skipRawData(static_cast<int>(pointCount * 24))
                   != static_cast<int>(pointCount * 24)) {
            return false;
        }
        pose.name = QString::fromUtf8(name);
        const QRegularExpression framePattern(QStringLiteral("frame_(\\d+)\\."),
                                              QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = framePattern.match(pose.name);
        if (match.hasMatch()) {
            pose.frameNumber = match.captured(1).toInt();
        }
        poses->append(pose);
    }
    return !poses->isEmpty();
}

std::array<double, 3> cameraCenter(const Pose& pose)
{
    const double w = pose.q[0];
    const double x = -pose.q[1];
    const double y = -pose.q[2];
    const double z = -pose.q[3];
    const double vx = -pose.t[0];
    const double vy = -pose.t[1];
    const double vz = -pose.t[2];
    const double tx = 2.0 * (y * vz - z * vy);
    const double ty = 2.0 * (z * vx - x * vz);
    const double tz = 2.0 * (x * vy - y * vx);
    return {{vx + w * tx + (y * tz - z * ty),
             vy + w * ty + (z * tx - x * tz),
             vz + w * tz + (x * ty - y * tx)}};
}

double translationDistance(const std::array<double, 3>& lhs,
                           const std::array<double, 3>& rhs)
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double orientationDegrees(const Pose& lhs, const Pose& rhs)
{
    double dot = 0.0;
    double lhsNorm = 0.0;
    double rhsNorm = 0.0;
    for (int index = 0; index < 4; ++index) {
        dot += lhs.q[static_cast<size_t>(index)] * rhs.q[static_cast<size_t>(index)];
        lhsNorm += qPow(lhs.q[static_cast<size_t>(index)], 2.0);
        rhsNorm += qPow(rhs.q[static_cast<size_t>(index)], 2.0);
    }
    if (lhsNorm <= 0.0 || rhsNorm <= 0.0) {
        return 0.0;
    }
    const double cosine = qBound(0.0, qAbs(dot) / std::sqrt(lhsNorm * rhsNorm), 1.0);
    return qRadiansToDegrees(2.0 * std::acos(cosine));
}

double median(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const qsizetype middle = values.size() / 2;
    return values.size() % 2 == 0
               ? (values.at(middle - 1) + values.at(middle)) / 2.0
               : values.at(middle);
}

} // namespace

CameraTrajectoryResult CameraTrajectoryValidator::inspect(const QString& modelDirectory,
                                                          double sampleFps,
                                                          double maximumGapSeconds)
{
    CameraTrajectoryResult result;
    QVector<Pose> poses;
    const QString binaryPath = QDir(modelDirectory).filePath(QStringLiteral("images.bin"));
    if (!QFileInfo(binaryPath).isFile() || !readImageBinary(binaryPath, &poses)) {
        result.warnings.append(QStringLiteral("无法解析 COLMAP images.bin，未完成相机轨迹连续性检查。"));
        return result;
    }
    result.readable = true;
    result.registeredImages = poses.size();
    for (Pose& pose : poses) {
        pose.center = cameraCenter(pose);
    }
    std::sort(poses.begin(), poses.end(), [](const Pose& lhs, const Pose& rhs) {
        if (lhs.frameNumber >= 0 && rhs.frameNumber >= 0 && lhs.frameNumber != rhs.frameNumber) {
            return lhs.frameNumber < rhs.frameNumber;
        }
        return lhs.name < rhs.name;
    });

    QVector<double> translations;
    for (qsizetype index = 1; index < poses.size(); ++index) {
        translations.append(translationDistance(poses.at(index - 1).center,
                                                poses.at(index).center));
    }
    const double typicalTranslation = median(translations);
    for (qsizetype index = 1; index < poses.size(); ++index) {
        const Pose& previous = poses.at(index - 1);
        const Pose& current = poses.at(index);
        const double translation = translationDistance(previous.center, current.center);
        const double rotation = orientationDegrees(previous, current);
        if (typicalTranslation > 0.0 && translation > typicalTranslation * 5.0) {
            ++result.translationJumpCount;
        }
        if (rotation > 120.0) {
            ++result.orientationJumpCount;
        }
        if (sampleFps > 0.0 && previous.frameNumber >= 0 && current.frameNumber >= 0
            && static_cast<double>(current.frameNumber - previous.frameNumber) / sampleFps
                   > maximumGapSeconds) {
            ++result.frameGapCount;
        }
    }
    if (result.translationJumpCount > 0) {
        result.warnings.append(QStringLiteral("相机轨迹存在 %1 处相邻平移突跳（超过轨迹中位步长 5 倍）。")
                                   .arg(result.translationJumpCount));
    }
    if (result.orientationJumpCount > 0) {
        result.warnings.append(QStringLiteral("相机轨迹存在 %1 处相邻旋转突跳（超过 120°）。")
                                   .arg(result.orientationJumpCount));
    }
    if (result.frameGapCount > 0) {
        result.warnings.append(QStringLiteral("注册相机之间存在 %1 处超过 %2 秒的时间间隔。")
                                   .arg(result.frameGapCount)
                                   .arg(maximumGapSeconds, 0, 'f', 1));
    }
    return result;
}

} // namespace vision3d
