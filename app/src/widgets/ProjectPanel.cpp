#include "widgets/ProjectPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

namespace vision3d {

ProjectPanel::ProjectPanel(QWidget* parent)
    : QWidget(parent)
    , m_nameLabel(new QLabel(this))
    , m_idLabel(new QLabel(this))
    , m_schemaLabel(new QLabel(this))
    , m_pathLabel(new QLabel(this))
    , m_stateLabel(new QLabel(this))
    , m_assetsLabel(new QLabel(this))
{
    auto* group = new QGroupBox(QStringLiteral("项目"), this);
    auto* form = new QFormLayout(group);
    form->addRow(QStringLiteral("项目名称:"), m_nameLabel);
    form->addRow(QStringLiteral("Project ID:"), m_idLabel);
    form->addRow(QStringLiteral("Schema Version:"), m_schemaLabel);
    form->addRow(QStringLiteral("项目路径:"), m_pathLabel);
    form->addRow(QStringLiteral("重建状态:"), m_stateLabel);
    form->addRow(QStringLiteral("图像资产:"), m_assetsLabel);

    m_pathLabel->setWordWrap(true);
    m_idLabel->setWordWrap(true);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->addStretch();

    clearProject();
}

void ProjectPanel::clearProject()
{
    m_nameLabel->setText(QStringLiteral("未打开项目"));
    m_idLabel->setText(QStringLiteral("-"));
    m_schemaLabel->setText(QStringLiteral("-"));
    m_pathLabel->setText(QStringLiteral("-"));
    m_stateLabel->setText(QStringLiteral("idle"));
    m_assetsLabel->setText(QStringLiteral("0"));
}

void ProjectPanel::setProject(const ProjectManifest& manifest, const QString& directory)
{
    m_nameLabel->setText(manifest.name());
    m_idLabel->setText(manifest.projectId());
    m_schemaLabel->setText(QString::number(manifest.schemaVersion()));
    m_pathLabel->setText(directory);
    m_stateLabel->setText(manifest.reconstructionState());
    m_assetsLabel->setText(QString::number(manifest.imageAssetRecords().size()));
}

} // namespace vision3d
