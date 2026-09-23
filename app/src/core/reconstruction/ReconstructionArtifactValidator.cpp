#include "core/reconstruction/ReconstructionArtifactValidator.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <limits>

namespace vision3d {

namespace {

bool fail(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool hasNonEmptyModelFile(const QDir& model, const QString& stem)
{
    return QFileInfo(model.filePath(stem + QStringLiteral(".bin"))).size() > 0
           || QFileInfo(model.filePath(stem + QStringLiteral(".txt"))).size() > 0;
}

int binaryImageCount(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 8) {
        return 0;
    }
    const QByteArray header = file.read(8);
    if (header.size() != 8) {
        return 0;
    }
    quint64 count = 0;
    for (int index = 0; index < 8; ++index) {
        count |= static_cast<quint64>(static_cast<unsigned char>(header.at(index))) << (8 * index);
    }
    return count > static_cast<quint64>(std::numeric_limits<int>::max())
               ? 0
               : static_cast<int>(count);
}

int textImageCount(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    int nonCommentLines = 0;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
            ++nonCommentLines;
        }
    }
    if (nonCommentLines == 0) {
        return 0;
    }
    // COLMAP images.txt stores one pose line followed by one 2D-points line per image.
    return nonCommentLines % 2 == 0 ? nonCommentLines / 2 : nonCommentLines;
}

} // namespace

bool ReconstructionArtifactValidator::validateDatabase(const QString& path, QString* error)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0) {
        return fail(error, QStringLiteral("A1/A2 数据库不存在或为空: %1").arg(path));
    }
    return true;
}

int ReconstructionArtifactValidator::registeredImageCount(const QString& modelDirectory)
{
    const QDir model(modelDirectory);
    const QString binaryPath = model.filePath(QStringLiteral("images.bin"));
    if (QFileInfo(binaryPath).isFile()) {
        return binaryImageCount(binaryPath);
    }
    return textImageCount(model.filePath(QStringLiteral("images.txt")));
}

SparseModelSelection ReconstructionArtifactValidator::selectPrimarySparseModel(
    const QString& sparseDirectory)
{
    SparseModelSelection selection;
    const QDir sparse(sparseDirectory);
    if (!sparse.exists()) {
        selection.message = QStringLiteral("A3 sparse 目录不存在: %1").arg(sparseDirectory);
        return selection;
    }
    const QFileInfoList models = sparse.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                      QDir::Name);
    for (const QFileInfo& modelInfo : models) {
        const QDir model(modelInfo.absoluteFilePath());
        if (!hasNonEmptyModelFile(model, QStringLiteral("cameras"))
            || !hasNonEmptyModelFile(model, QStringLiteral("images"))
            || !hasNonEmptyModelFile(model, QStringLiteral("points3D"))) {
            continue;
        }
        const int count = registeredImageCount(model.absolutePath());
        if (!selection.valid || count > selection.registeredImages) {
            selection.valid = count > 0;
            selection.relativePath = QStringLiteral("sparse/") + modelInfo.fileName();
            selection.registeredImages = count;
        }
    }
    if (!selection.valid) {
        selection.message = QStringLiteral("A3 未找到包含注册图像的有效 sparse model。 ").trimmed();
    } else {
        selection.message = QStringLiteral("Primary sparse model: %1, registered=%2")
                                .arg(selection.relativePath)
                                .arg(selection.registeredImages);
    }
    return selection;
}

bool ReconstructionArtifactValidator::validateUndistortion(const QString& denseDirectory,
                                                           QString* error)
{
    const QDir dense(denseDirectory);
    if (!QDir(dense.filePath(QStringLiteral("images"))).exists()) {
        return fail(error, QStringLiteral("A4 缺少 dense/images: %1").arg(dense.filePath("images")));
    }
    if (!QDir(dense.filePath(QStringLiteral("sparse"))).exists()) {
        return fail(error, QStringLiteral("A4 缺少 dense/sparse: %1").arg(dense.filePath("sparse")));
    }
    return true;
}

bool ReconstructionArtifactValidator::validatePatchMatch(const QString& denseDirectory,
                                                         QString* error)
{
    QDirIterator iterator(denseDirectory,
                          QDir::Files | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    bool hasDepthMap = false;
    bool hasGeometricOutput = false;
    while (iterator.hasNext()) {
        const QFileInfo fileInfo(iterator.next());
        const QString normalized = QDir::fromNativeSeparators(fileInfo.absoluteFilePath());
        if (normalized.contains(QStringLiteral("/depth_maps/"), Qt::CaseInsensitive)
            && fileInfo.size() > 0) {
            hasDepthMap = true;
        }
        if (fileInfo.fileName().contains(QStringLiteral("geometric"), Qt::CaseInsensitive)
            && fileInfo.size() > 0) {
            hasGeometricOutput = true;
        }
    }
    if (!hasDepthMap) {
        return fail(error, QStringLiteral("A5 缺少非空 depth_maps 输出。"));
    }
    if (!hasGeometricOutput) {
        return fail(error, QStringLiteral("A5 缺少 geometric depth 输出。"));
    }
    return true;
}

bool ReconstructionArtifactValidator::validateNonEmptyFile(const QString& path, QString* error)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0) {
        return fail(error, QStringLiteral("缺少非空 artifact: %1").arg(path));
    }
    return true;
}

} // namespace vision3d
