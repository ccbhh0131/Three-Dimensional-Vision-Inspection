#pragma once

#include "core/assets/AssetRecord.h"

#include <QImage>
#include <QString>

namespace vision3d {

class ThumbnailCache final
{
public:
    static constexpr int MaxThumbnailEdge = 256;

    static QString cacheDirectory(const QString& projectDirectory);
    static QString cachePath(const QString& projectDirectory, const QString& assetId);
    static QImage loadOrCreate(const AssetRecord& asset, const QString& projectDirectory);
    static bool remove(const QString& projectDirectory,
                       const QString& assetId,
                       QString* error = nullptr);

private:
    static QImage missingImage(const QString& message);
};

} // namespace vision3d
