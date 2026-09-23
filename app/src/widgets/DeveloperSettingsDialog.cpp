#include "widgets/DeveloperSettingsDialog.h"

#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace vision3d {

DeveloperSettingsDialog::DeveloperSettingsDialog(const BackendProbeResult& probeResult,
                                                 const QString& internalRoot,
                                                 const QString& developmentFallbackRoot,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_fallbackRootEdit(new QLineEdit(developmentFallbackRoot, this))
{
    setWindowTitle(QStringLiteral("开发设置"));
    setModal(true);

    auto* technicalGroup = new QGroupBox(QStringLiteral("内部引擎信息"), this);
    auto* technicalForm = new QFormLayout(technicalGroup);
    auto* backendLabel = new QLabel(
        probeResult.backendName.isEmpty() ? QStringLiteral("-") : probeResult.backendName,
        technicalGroup);
    auto* versionLabel = new QLabel(
        probeResult.version.isEmpty() ? QStringLiteral("-") : probeResult.version,
        technicalGroup);
    auto* rootLabel = new QLabel(
        probeResult.rootPath.isEmpty() ? internalRoot : probeResult.rootPath,
        technicalGroup);
    auto* executableLabel = new QLabel(probeResult.executablePath, technicalGroup);
    rootLabel->setWordWrap(true);
    executableLabel->setWordWrap(true);
    technicalForm->addRow(QStringLiteral("Internal backend:"), backendLabel);
    technicalForm->addRow(QStringLiteral("Version:"), versionLabel);
    technicalForm->addRow(QStringLiteral("Root:"), rootLabel);
    technicalForm->addRow(QStringLiteral("Executable:"), executableLabel);

    auto* fallbackGroup = new QGroupBox(QStringLiteral("开发 Backend 回退"), this);
    auto* fallbackForm = new QFormLayout(fallbackGroup);
    auto* fallbackLayout = new QHBoxLayout;
    auto* chooseButton = new QPushButton(QStringLiteral("选择..."), fallbackGroup);
    fallbackLayout->addWidget(m_fallbackRootEdit, 1);
    fallbackLayout->addWidget(chooseButton);
    fallbackForm->addRow(QStringLiteral("开发路径:"), fallbackLayout);
    m_fallbackRootEdit->setPlaceholderText(QStringLiteral("仅用于本机开发回退"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存并检测"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(technicalGroup);
    layout->addWidget(fallbackGroup);
    layout->addWidget(buttons);

    connect(chooseButton, &QPushButton::clicked, this, &DeveloperSettingsDialog::chooseFallbackRoot);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString DeveloperSettingsDialog::developmentFallbackRoot() const
{
    return m_fallbackRootEdit->text().trimmed();
}

void DeveloperSettingsDialog::chooseFallbackRoot()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择开发 Backend 路径"),
        m_fallbackRootEdit->text().trimmed().isEmpty() ? QDir::homePath()
                                                        : m_fallbackRootEdit->text().trimmed());
    if (!selected.isEmpty()) {
        m_fallbackRootEdit->setText(selected);
    }
}

} // namespace vision3d
