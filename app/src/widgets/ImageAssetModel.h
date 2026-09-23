#pragma once

#include "core/assets/AssetRecord.h"
#include "core/project/ProjectManifest.h"

#include <QAbstractListModel>
#include <QList>
#include <QVariant>

namespace vision3d {

class ImageAssetModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        AssetIdRole = Qt::UserRole + 1,
        RelativePathRole,
        WidthRole,
        HeightRole,
        FileSizeRole,
        ExistsRole,
    };

    explicit ImageAssetModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clearProject();
    void setProject(const ProjectManifest& manifest, const QString& projectDirectory);

    bool hasProject() const;
    QString selectedPath(const QString& assetId) const;
    std::optional<AssetRecord> assetAt(const QModelIndex& index) const;
    std::optional<AssetRecord> assetById(const QString& assetId) const;
    const QList<AssetRecord>& assets() const;

private:
    QList<AssetRecord> m_assets;
    QString m_projectDirectory;
    bool m_hasProject = false;
};

} // namespace vision3d
