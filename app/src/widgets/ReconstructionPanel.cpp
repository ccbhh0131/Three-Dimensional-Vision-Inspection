#include "widgets/ReconstructionPanel.h"

#include "core/reconstruction/ReconstructionController.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVector3D>
#include <QVBoxLayout>

namespace vision3d {

namespace {

QString stageText(ReconstructionStage stage)
{
    switch (stage) {
    case ReconstructionStage::PreparingInput:
        return QStringLiteral("准备输入");
    case ReconstructionStage::FeatureExtraction:
        return QStringLiteral("特征提取");
    case ReconstructionStage::FeatureMatching:
        return QStringLiteral("特征匹配");
    case ReconstructionStage::SparseMapping:
        return QStringLiteral("稀疏重建");
    case ReconstructionStage::ImageUndistortion:
        return QStringLiteral("图像去畸变");
    case ReconstructionStage::DenseStereo:
        return QStringLiteral("稠密重建");
    case ReconstructionStage::StereoFusion:
        return QStringLiteral("点云融合");
    case ReconstructionStage::Meshing:
        return QStringLiteral("网格生成");
    case ReconstructionStage::Completed:
        return QStringLiteral("已完成");
    case ReconstructionStage::None:
        return QStringLiteral("-");
    }
    return QStringLiteral("-");
}

QString stateText(ReconstructionState state)
{
    switch (state) {
    case ReconstructionState::Idle:
        return QStringLiteral("就绪");
    case ReconstructionState::Preparing:
        return QStringLiteral("准备中");
    case ReconstructionState::Running:
        return QStringLiteral("运行中");
    case ReconstructionState::Completed:
        return QStringLiteral("重建完成");
    case ReconstructionState::Failed:
        return QStringLiteral("失败");
    case ReconstructionState::Cancelled:
        return QStringLiteral("已取消");
    case ReconstructionState::Interrupted:
        return QStringLiteral("异常中断");
    }
    return QStringLiteral("-");
}

} // namespace

ReconstructionPanel::ReconstructionPanel(QWidget* parent)
    : QWidget(parent)
    , m_stateLabel(new QLabel(QStringLiteral("就绪"), this))
    , m_engineLabel(new QLabel(QStringLiteral("未检测"), this))
    , m_gpuLabel(new QLabel(QStringLiteral("未检测"), this))
    , m_inputLabel(new QLabel(QStringLiteral("0"), this))
    , m_stageLabel(new QLabel(QStringLiteral("-"), this))
    , m_progressLabel(new QLabel(QStringLiteral("0/7"), this))
    , m_registeredLabel(new QLabel(QStringLiteral("-"), this))
    , m_jobLabel(new QLabel(QStringLiteral("-"), this))
    , m_meshLabel(new QLabel(QStringLiteral("-"), this))
    , m_startButton(new QPushButton(QStringLiteral("开始三维重建"), this))
    , m_cancelButton(new QPushButton(QStringLiteral("取消"), this))
    , m_viewModelButton(new QPushButton(QStringLiteral("查看三维模型"), this))
    , m_resetViewButton(new QPushButton(QStringLiteral("重置三维视图"), this))
    , m_markerLabel(new QLabel(QStringLiteral("暂无选中设备标记"), this))
    , m_addMarkerButton(new QPushButton(QStringLiteral("添加设备标记"), this))
    , m_deleteMarkerButton(new QPushButton(QStringLiteral("删除设备标记"), this))
{
    auto* group = new QGroupBox(QStringLiteral("三维重建"), this);
    auto* form = new QFormLayout(group);
    form->addRow(QStringLiteral("状态:"), m_stateLabel);
    form->addRow(QStringLiteral("重建引擎:"), m_engineLabel);
    form->addRow(QStringLiteral("GPU 加速:"), m_gpuLabel);
    form->addRow(QStringLiteral("输入图片:"), m_inputLabel);
    form->addRow(QStringLiteral("当前阶段:"), m_stageLabel);
    form->addRow(QStringLiteral("阶段进度:"), m_progressLabel);
    form->addRow(QStringLiteral("注册图片:"), m_registeredLabel);
    form->addRow(QStringLiteral("Job:"), m_jobLabel);
    form->addRow(QStringLiteral("Mesh:"), m_meshLabel);
    m_jobLabel->setWordWrap(true);
    m_meshLabel->setWordWrap(true);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_cancelButton);
    buttons->addWidget(m_viewModelButton);
    buttons->addWidget(m_resetViewButton);
    buttons->addStretch();

    auto* markerGroup = new QGroupBox(QStringLiteral("设备标记"), this);
    auto* markerLayout = new QVBoxLayout(markerGroup);
    m_markerLabel->setObjectName(QStringLiteral("deviceMarkerDetailsLabel"));
    m_markerLabel->setWordWrap(true);
    markerLayout->addWidget(m_markerLabel);
    auto* markerButtons = new QHBoxLayout;
    markerButtons->addWidget(m_addMarkerButton);
    markerButtons->addWidget(m_deleteMarkerButton);
    markerButtons->addStretch();
    markerLayout->addLayout(markerButtons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->addLayout(buttons);
    layout->addWidget(markerGroup);
    layout->addStretch();

    connect(m_startButton, &QPushButton::clicked, this, &ReconstructionPanel::startRequested);
    connect(m_cancelButton, &QPushButton::clicked, this, &ReconstructionPanel::cancelRequested);
    connect(m_viewModelButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::viewModelRequested);
    connect(m_resetViewButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::resetViewRequested);
    connect(m_addMarkerButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::addMarkerRequested);
    connect(m_deleteMarkerButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::deleteMarkerRequested);
    updateControls();
}

QPushButton* ReconstructionPanel::startButton() const { return m_startButton; }

QPushButton* ReconstructionPanel::cancelButton() const { return m_cancelButton; }

QPushButton* ReconstructionPanel::viewModelButton() const { return m_viewModelButton; }

QPushButton* ReconstructionPanel::resetViewButton() const { return m_resetViewButton; }

QPushButton* ReconstructionPanel::addMarkerButton() const { return m_addMarkerButton; }

QPushButton* ReconstructionPanel::deleteMarkerButton() const { return m_deleteMarkerButton; }

void ReconstructionPanel::setProjectContext(bool hasProject, int imageCount)
{
    m_hasProject = hasProject;
    m_imageCount = imageCount;
    m_inputLabel->setText(QString::number(imageCount));
    updateControls();
}

void ReconstructionPanel::setEngineContext(bool engineAvailable, bool gpuAvailable)
{
    m_engineAvailable = engineAvailable;
    m_gpuAvailable = gpuAvailable;
    m_engineLabel->setText(engineAvailable ? QStringLiteral("可用") : QStringLiteral("不可用"));
    m_gpuLabel->setText(gpuAvailable ? QStringLiteral("可用") : QStringLiteral("不可用"));
    updateControls();
}

void ReconstructionPanel::setBackendContext(const QString& backend,
                                              const QString& version,
                                              bool verified)
{
    Q_UNUSED(backend)
    Q_UNUSED(version)
    setEngineContext(verified, verified);
}

void ReconstructionPanel::setTask(const ReconstructionTask& task)
{
    m_stateLabel->setText(stateText(task.state));
    m_stageLabel->setText(stageText(task.stage));
    const int completed = ReconstructionController::completedStageCount(task.stage);
    m_progressLabel->setText(QStringLiteral("%1/7").arg(completed));
    m_registeredLabel->setText(task.registeredImageCount > 0
                                   ? QStringLiteral("%1/%2")
                                         .arg(task.registeredImageCount)
                                         .arg(task.inputImageCount)
                                   : QStringLiteral("-"));
    m_jobLabel->setText(task.workspaceRelativePath.isEmpty() ? QStringLiteral("-")
                                                               : task.workspaceRelativePath);
    m_meshLabel->setText(task.meshRelativePath.isEmpty() ? QStringLiteral("-")
                                                          : task.meshRelativePath);
    updateControls();
}

void ReconstructionPanel::setRunning(bool running)
{
    m_running = running;
    updateControls();
}

void ReconstructionPanel::setMeshAvailable(bool available)
{
    m_meshAvailable = available;
    updateControls();
}

void ReconstructionPanel::setViewerLoaded(bool loaded)
{
    m_viewerLoaded = loaded;
    updateControls();
}

void ReconstructionPanel::setMarkerActionEnabled(bool canAdd, bool canDelete)
{
    m_canAddMarker = canAdd;
    m_canDeleteMarker = canDelete;
    updateControls();
}

void ReconstructionPanel::setSelectedMarkerDetails(const QString& id,
                                                    const QString& name,
                                                    const QVector3D& worldPosition)
{
    m_markerLabel->setText(
        QStringLiteral("ID: %1\nName: %2\nWorld: (%3, %4, %5)")
            .arg(id)
            .arg(name)
            .arg(worldPosition.x(), 0, 'f', 6)
            .arg(worldPosition.y(), 0, 'f', 6)
            .arg(worldPosition.z(), 0, 'f', 6));
    updateControls();
}

void ReconstructionPanel::clearSelectedMarkerDetails()
{
    m_markerLabel->setText(QStringLiteral("暂无选中设备标记"));
    m_canDeleteMarker = false;
    updateControls();
}

void ReconstructionPanel::updateControls()
{
    m_startButton->setEnabled(!m_running && m_hasProject && m_imageCount >= 2
                              && m_engineAvailable);
    m_cancelButton->setEnabled(m_running);
    m_viewModelButton->setEnabled(!m_running && m_meshAvailable);
    m_resetViewButton->setEnabled(!m_running && m_viewerLoaded);
    m_addMarkerButton->setEnabled(!m_running && m_viewerLoaded && m_canAddMarker);
    m_deleteMarkerButton->setEnabled(!m_running && m_viewerLoaded && m_canDeleteMarker);
}

} // namespace vision3d
