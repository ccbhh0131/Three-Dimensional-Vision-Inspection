#include "widgets/ImageAssetModel.h"

#include "core/assets/ThumbnailCache.h"

#include <QFileInfo>
#include <QDir>
#include <QPixmap>

namespace vision3d {

ImageAssetModel::ImageAssetModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ImageAssetModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_assets.size();
}

QVariant ImageAssetModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_assets.size()) {
        return {};
    }

    const AssetRecord& asset = m_assets.at(index.row());
    const QString path = selectedPath(asset.id);
    switch (role) {
    case Qt::DisplayRole:
        return asset.originalFileName;
    case Qt::DecorationRole:
        return QPixmap::fromImage(ThumbnailCache::loadOrCreate(asset, m_projectDirectory));
    case Qt::ToolTipRole:
        return QStringLiteral("%1\n%2 × %3\n%4")
            .arg(asset.originalFileName)
            .arg(asset.width)
            .arg(asset.height)
            .arg(QFileInfo(path).isFile() ? QStringLiteral("就绪") : QStringLiteral("Missing"));
    case AssetIdRole:
        return asset.id;
    case RelativePathRole:
        return asset.relativePath;
    case WidthRole:
        return asset.width;
    case HeightRole:
        return asset.height;
    case FileSizeRole:
        return asset.fileSize;
    case ExistsRole:
        return QFileInfo(path).isFile();
    default:
        return {};
    }
}

QHash<int, QByteArray> ImageAssetModel::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractListModel::roleNames();
    names.insert(AssetIdRole, "assetId");
    names.insert(RelativePathRole, "relativePath");
    names.insert(WidthRole, "width");
    names.insert(HeightRole, "height");
    names.insert(FileSizeRole, "fileSize");
    names.insert(ExistsRole, "exists");
    return names;
}

void ImageAssetModel::clearProject()
{
    beginResetModel();
    m_assets.clear();
    m_projectDirectory.clear();
    m_hasProject = false;
    endResetModel();
}

void ImageAssetModel::setProject(const ProjectManifest& manifest,
                                 const QString& projectDirectory)
{
    QString error;
    const QList<AssetRecord> records = manifest.imageAssetRecords(&error);
    beginResetModel();
    m_assets = records;
    m_projectDirectory = projectDirectory;
    m_hasProject = true;
    endResetModel();
}

bool ImageAssetModel::hasProject() const { return m_hasProject; }

QString ImageAssetModel::selectedPath(const QString& assetId) const
{
    const std::optional<AssetRecord> asset = assetById(assetId);
    if (!asset.has_value() || m_projectDirectory.isEmpty()) {
        return {};
    }
    return QDir(m_projectDirectory).filePath(
        QDir::fromNativeSeparators(QDir::cleanPath(asset->relativePath)));
}

std::optional<AssetRecord> ImageAssetModel::assetAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_assets.size()) {
        return std::nullopt;
    }
    return m_assets.at(index.row());
}

std::optional<AssetRecord> ImageAssetModel::assetById(const QString& assetId) const
{
    for (const AssetRecord& asset : m_assets) {
        if (asset.id == assetId) {
            return asset;
        }
    }
    return std::nullopt;
}

const QList<AssetRecord>& ImageAssetModel::assets() const { return m_assets; }

} // namespace vision3d
