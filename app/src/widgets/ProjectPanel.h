#pragma once

#include "core/project/ProjectManifest.h"

#include <QWidget>

class QLabel;

namespace vision3d {

class ProjectPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectPanel(QWidget* parent = nullptr);

public slots:
    void clearProject();
    void setProject(const ProjectManifest& manifest, const QString& directory);

private:
    QLabel* m_nameLabel;
    QLabel* m_idLabel;
    QLabel* m_schemaLabel;
    QLabel* m_pathLabel;
    QLabel* m_stateLabel;
    QLabel* m_assetsLabel;
};

} // namespace vision3d
