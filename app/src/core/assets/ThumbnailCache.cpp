#include "core/assets/ThumbnailCache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QStringList>

namespace vision3d {

QString ThumbnailCache::cacheDirectory(const QString& projectDirectory)
{
    return QDir(projectDirectory).filePath(QStringLiteral("cache/thumbnails"));
}

QString ThumbnailCache::cachePath(const QString& projectDirectory, const QString& assetId)
{
    return QDir(cacheDirectory(projectDirectory)).filePath(assetId + QStringLiteral(".jpg"));
}

QImage ThumbnailCache::loadOrCreate(const AssetRecord& asset, const QString& projectDirectory)
{
    QString validationError;
    if (!asset.isValid(&validationError)) {
        return missingImage(QStringLiteral("Invalid asset"));
    }

    const QString sourcePath = QDir(projectDirectory).filePath(
        QDir::fromNativeSeparators(QDir::cleanPath(asset.relativePath)));
    if (!QFileInfo(sourcePath).isFile()) {
        return missingImage(QStringLiteral("Missing image"));
    }

    const QString targetPath = cachePath(projectDirectory, asset.id);
    if (QFileInfo(targetPath).isFile()) {
        QImage cached(targetPath);
        if (!cached.isNull() && cached.width() <= MaxThumbnailEdge
            && cached.height() <= MaxThumbnailEdge) {
            return cached;
        }
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    QSize sourceSize = reader.size();
    if (!sourceSize.isValid() || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
        const QImage decoded = reader.read();
        if (decoded.isNull()) {
            return missingImage(QStringLiteral("Unreadable image"));
        }
        sourceSize = decoded.size();
    }

    const QSize targetSize = sourceSize.scaled(QSize(MaxThumbnailEdge, MaxThumbnailEdge),
                                               Qt::KeepAspectRatio);
    reader.setScaledSize(targetSize);
    QImage thumbnail = reader.read();
    if (thumbnail.isNull()) {
        QImageReader fallbackReader(sourcePath);
        fallbackReader.setAutoTransform(true);
        const QImage decoded = fallbackReader.read();
        if (decoded.isNull()) {
            return missingImage(QStringLiteral("Unreadable image"));
        }
        thumbnail = decoded.scaled(QSize(MaxThumbnailEdge, MaxThumbnailEdge),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    }
    if (thumbnail.isNull()) {
        return missingImage(QStringLiteral("Unreadable image"));
    }

    QDir().mkpath(cacheDirectory(projectDirectory));
    if (!thumbnail.save(targetPath, "JPG", 85)) {
        // A thumbnail is a cache, so a write failure should not make the image unavailable.
        return thumbnail;
    }
    return thumbnail;
}

bool ThumbnailCache::remove(const QString& projectDirectory,
                            const QString& assetId,
                            QString* error)
{
    const QDir directory(cacheDirectory(projectDirectory));
    QStringList failures;
    const QStringList paths = {
        directory.filePath(assetId + QStringLiteral(".jpg")),
        directory.filePath(assetId + QStringLiteral(".png")),
    };
    for (const QString& path : paths) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            failures.append(path);
        }
    }
    if (!failures.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("无法删除缩略图缓存: %1")
                         .arg(failures.join(QStringLiteral(", ")));
        }
        return false;
    }
    return true;
}

QImage ThumbnailCache::missingImage(const QString& message)
{
    QImage image(QSize(MaxThumbnailEdge, 160), QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#e5e7eb")));
    QPainter painter(&image);
    painter.setPen(QColor(QStringLiteral("#4b5563")));
    painter.drawText(image.rect(), Qt::AlignCenter, message);
    return image;
}

} // namespace vision3d
