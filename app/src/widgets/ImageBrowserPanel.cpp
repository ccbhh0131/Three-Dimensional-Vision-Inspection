#include "widgets/ImageBrowserPanel.h"

#include "core/project/ProjectManifest.h"
#include "widgets/ImageAssetModel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>

namespace vision3d {

ImageBrowserPanel::ImageBrowserPanel(QWidget* parent)
    : QWidget(parent)
    , m_model(new ImageAssetModel(this))
    , m_view(new QListView(this))
    , m_countLabel(new QLabel(this))
    , m_importButton(new QPushButton(QStringLiteral("导入图像"), this))
    , m_removeButton(new QPushButton(QStringLiteral("从项目中删除"), this))
{
    m_view->setModel(m_model);
    m_view->setViewMode(QListView::IconMode);
    m_view->setFlow(QListView::LeftToRight);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setIconSize(QSize(128, 128));
    m_view->setGridSize(QSize(160, 170));
    m_view->setSpacing(4);
    m_view->setWordWrap(true);
    m_view->setMinimumHeight(180);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_importButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(QStringLiteral("图像资产"), this));
    layout->addWidget(m_countLabel);
    layout->addWidget(m_view, 1);
    layout->addLayout(buttons);

    connect(m_importButton, &QPushButton::clicked, this, &ImageBrowserPanel::importRequested);
    connect(m_removeButton, &QPushButton::clicked, this, [this] {
        if (!selectedAssetId().isEmpty()) {
            emit removeRequested(selectedAssetId());
        }
    });
    connect(m_view->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex& current, const QModelIndex&) {
                const std::optional<AssetRecord> asset = m_model->assetAt(current);
                emit assetSelected(asset.has_value() ? asset->id : QString());
                updateControls();
            });

    clearProject();
}

QListView* ImageBrowserPanel::view() const { return m_view; }

ImageAssetModel* ImageBrowserPanel::model() const { return m_model; }

QString ImageBrowserPanel::selectedAssetId() const
{
    const std::optional<AssetRecord> asset = selectedAsset();
    return asset.has_value() ? asset->id : QString();
}

std::optional<AssetRecord> ImageBrowserPanel::selectedAsset() const
{
    return m_model->assetAt(m_view->currentIndex());
}

void ImageBrowserPanel::clearProject()
{
    m_model->clearProject();
    m_hasProject = false;
    m_countLabel->setText(QStringLiteral("未打开项目"));
    updateControls();
}

void ImageBrowserPanel::setProject(const ProjectManifest& manifest,
                                   const QString& projectDirectory)
{
    m_model->setProject(manifest, projectDirectory);
    m_hasProject = true;
    m_countLabel->setText(QStringLiteral("%1 个图像资产").arg(m_model->rowCount()));
    updateControls();
}

void ImageBrowserPanel::setOperationsEnabled(bool enabled)
{
    m_operationsEnabled = enabled;
    updateControls();
}

void ImageBrowserPanel::updateControls()
{
    m_importButton->setEnabled(m_operationsEnabled && m_hasProject);
    m_removeButton->setEnabled(m_operationsEnabled && m_hasProject
                               && !selectedAssetId().isEmpty());
}

} // namespace vision3d
