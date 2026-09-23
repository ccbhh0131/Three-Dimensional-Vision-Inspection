#pragma once

#include "core/assets/AssetRecord.h"

#include <QWidget>

class QListView;
class QLabel;
class QPushButton;

namespace vision3d {

class ImageAssetModel;
class ProjectManifest;

class ImageBrowserPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ImageBrowserPanel(QWidget* parent = nullptr);

    QListView* view() const;
    ImageAssetModel* model() const;
    QString selectedAssetId() const;
    std::optional<AssetRecord> selectedAsset() const;
    void setOperationsEnabled(bool enabled);

public slots:
    void clearProject();
    void setProject(const ProjectManifest& manifest, const QString& projectDirectory);

signals:
    void importRequested();
    void removeRequested(const QString& assetId);
    void assetSelected(const QString& assetId);

private:
    void updateControls();

    ImageAssetModel* m_model;
    QListView* m_view;
    QLabel* m_countLabel;
    QPushButton* m_importButton;
    QPushButton* m_removeButton;
    bool m_hasProject = false;
    bool m_operationsEnabled = true;
};

} // namespace vision3d
