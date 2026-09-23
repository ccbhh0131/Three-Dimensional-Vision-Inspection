#include "widgets/BackendPanel.h"

#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace vision3d {

BackendPanel::BackendPanel(QWidget* parent)
    : QWidget(parent)
    , m_engineStatusLabel(new QLabel(QStringLiteral("未检测"), this))
    , m_gpuStatusLabel(new QLabel(QStringLiteral("未检测"), this))
    , m_statusLabel(new QLabel(QStringLiteral("未检测"), this))
    , m_messageLabel(new QLabel(QStringLiteral("正在等待三维重建引擎检测。"), this))
    , m_probeButton(new QPushButton(QStringLiteral("检测重建引擎"), this))
    , m_developerSettingsButton(new QPushButton(QStringLiteral("开发设置..."), this))
{
    auto* group = new QGroupBox(QStringLiteral("三维重建引擎"), this);
    auto* form = new QFormLayout(group);
    form->addRow(QStringLiteral("重建引擎:"), m_engineStatusLabel);
    form->addRow(QStringLiteral("GPU 加速:"), m_gpuStatusLabel);
    form->addRow(QStringLiteral("状态:"), m_statusLabel);
    form->addRow(QStringLiteral("信息:"), m_messageLabel);
    m_messageLabel->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_probeButton);
    buttonLayout->addWidget(m_developerSettingsButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    layout->addStretch();

    connect(m_probeButton, &QPushButton::clicked, this, [this] {
        emit probeRequested(m_backendRoot);
    });
    connect(m_developerSettingsButton,
            &QPushButton::clicked,
            this,
            &BackendPanel::developerSettingsRequested);
}

QString BackendPanel::backendRoot() const { return m_backendRoot; }

bool BackendPanel::isVerified() const
{
    return m_lastResult.available && m_lastResult.version == QStringLiteral("3.11.1");
}

const BackendProbeResult& BackendPanel::lastResult() const { return m_lastResult; }

bool BackendPanel::isGpuAvailable() const { return m_gpuAvailable; }

void BackendPanel::setBackendRoot(const QString& root)
{
    m_backendRoot = root.trimmed();
    m_lastResult = BackendProbeResult();
    m_lastResult.rootPath = m_backendRoot;
    m_lastResult.executablePath = m_backendRoot.isEmpty()
                                      ? QString()
                                      : QDir(m_backendRoot).filePath(QStringLiteral("bin/colmap.exe"));
    m_lastResult.backendName = QStringLiteral("COLMAP");
    m_gpuAvailable = false;
    m_engineStatusLabel->setText(QStringLiteral("未检测"));
    m_gpuStatusLabel->setText(QStringLiteral("未检测"));
    m_statusLabel->setText(QStringLiteral("未检测"));
    m_messageLabel->setText(m_backendRoot.isEmpty()
                                ? QStringLiteral("未发现可用的三维重建引擎。")
                                : QStringLiteral("正在等待三维重建引擎检测。"));
}

void BackendPanel::setChecking()
{
    m_engineStatusLabel->setText(QStringLiteral("检测中"));
    m_gpuStatusLabel->setText(QStringLiteral("检测中"));
    m_statusLabel->setText(QStringLiteral("检测中..."));
    m_messageLabel->setText(QStringLiteral("正在检测三维重建引擎。"));
}

void BackendPanel::setResult(const BackendProbeResult& result)
{
    m_lastResult = result;
    const bool verified = isVerified();
    m_gpuAvailable = verified;
    m_engineStatusLabel->setText(verified ? QStringLiteral("可用") : QStringLiteral("不可用"));
    m_gpuStatusLabel->setText(verified ? QStringLiteral("可用") : QStringLiteral("不可用"));
    if (verified) {
        m_statusLabel->setText(QStringLiteral("就绪"));
    } else {
        m_statusLabel->setText(QStringLiteral("不可用"));
    }
    if (verified) {
        m_messageLabel->setText(QStringLiteral("三维重建引擎已就绪。"));
    } else if (result.rootPath.isEmpty()) {
        m_messageLabel->setText(QStringLiteral("未发现可用的三维重建引擎。"));
    } else {
        m_messageLabel->setText(QStringLiteral("三维重建引擎检测未通过版本验证。"));
    }
}

void BackendPanel::setProbeEnabled(bool enabled)
{
    m_probeButton->setEnabled(enabled);
    m_developerSettingsButton->setEnabled(enabled);
}

} // namespace vision3d
