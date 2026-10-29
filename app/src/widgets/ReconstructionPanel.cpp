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
    case ReconstructionStage::ModelOptimization:
        return QStringLiteral("模型优化");
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
    , m_progressLabel(new QLabel(QStringLiteral("0/8"), this))
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
    , m_gaugeLabel(new QLabel(QStringLiteral("未绑定仪表"), this))
    , m_createGaugeButton(new QPushButton(QStringLiteral("创建仪表资产"), this))
    , m_editGaugeButton(new QPushButton(QStringLiteral("编辑仪表资产"), this))
    , m_updateGaugeReadingButton(new QPushButton(QStringLiteral("手动更新读数"), this))
    , m_visualGaugeReadingButton(new QPushButton(QStringLiteral("视觉读数"), this))
    , m_viewGaugeHistoryButton(new QPushButton(QStringLiteral("查看巡检历史"), this))
    , m_configureGaugeStatusRuleButton(new QPushButton(QStringLiteral("配置状态规则"), this))
    , m_deleteGaugeButton(new QPushButton(QStringLiteral("删除仪表资产"), this))
    , m_realtimeStateLabel(new QLabel(QStringLiteral("Stopped"), this))
    , m_realtimeValueLabel(new QLabel(QStringLiteral("暂无"), this))
    , m_realtimeTimestampLabel(new QLabel(QStringLiteral("暂无"), this))
    , m_realtimeSourceLabel(new QLabel(QStringLiteral("-"), this))
    , m_realtimeStatusLabel(new QLabel(QStringLiteral("未知"), this))
    , m_startMockSensorButton(new QPushButton(QStringLiteral("启动模拟数据"), this))
    , m_stopMockSensorButton(new QPushButton(QStringLiteral("停止"), this))
    , m_recordCurrentSensorSampleButton(new QPushButton(QStringLiteral("记录当前值"), this))
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

    auto* gaugeGroup = new QGroupBox(QStringLiteral("仪表资产"), this);
    auto* gaugeLayout = new QVBoxLayout(gaugeGroup);
    m_gaugeLabel->setObjectName(QStringLiteral("gaugeAssetDetailsLabel"));
    m_gaugeLabel->setWordWrap(true);
    gaugeLayout->addWidget(m_gaugeLabel);
    auto* gaugeButtons = new QHBoxLayout;
    gaugeButtons->addWidget(m_createGaugeButton);
    gaugeButtons->addWidget(m_editGaugeButton);
    gaugeButtons->addWidget(m_updateGaugeReadingButton);
    gaugeButtons->addWidget(m_visualGaugeReadingButton);
    gaugeButtons->addWidget(m_viewGaugeHistoryButton);
    gaugeButtons->addWidget(m_configureGaugeStatusRuleButton);
    gaugeButtons->addWidget(m_deleteGaugeButton);
    gaugeButtons->addStretch();
    gaugeLayout->addLayout(gaugeButtons);

    auto* realtimeGroup = new QGroupBox(QStringLiteral("实时监控"), this);
    realtimeGroup->setObjectName(QStringLiteral("realtimeMonitoringGroup"));
    auto* realtimeForm = new QFormLayout(realtimeGroup);
    m_realtimeStateLabel->setObjectName(QStringLiteral("realtimeMonitoringStateLabel"));
    m_realtimeValueLabel->setObjectName(QStringLiteral("realtimeGaugeValueLabel"));
    m_realtimeTimestampLabel->setObjectName(QStringLiteral("realtimeGaugeTimestampLabel"));
    m_realtimeSourceLabel->setObjectName(QStringLiteral("realtimeGaugeSourceLabel"));
    m_realtimeStatusLabel->setObjectName(QStringLiteral("realtimeGaugeStatusLabel"));
    realtimeForm->addRow(QStringLiteral("状态:"), m_realtimeStateLabel);
    realtimeForm->addRow(QStringLiteral("实时值:"), m_realtimeValueLabel);
    realtimeForm->addRow(QStringLiteral("实时来源:"), m_realtimeSourceLabel);
    realtimeForm->addRow(QStringLiteral("更新时间:"), m_realtimeTimestampLabel);
    realtimeForm->addRow(QStringLiteral("当前状态:"), m_realtimeStatusLabel);
    auto* realtimeButtons = new QHBoxLayout;
    m_startMockSensorButton->setObjectName(QStringLiteral("startMockSensorButton"));
    m_stopMockSensorButton->setObjectName(QStringLiteral("stopMockSensorButton"));
    m_recordCurrentSensorSampleButton->setObjectName(
        QStringLiteral("recordCurrentSensorSampleButton"));
    realtimeButtons->addWidget(m_startMockSensorButton);
    realtimeButtons->addWidget(m_stopMockSensorButton);
    realtimeButtons->addWidget(m_recordCurrentSensorSampleButton);
    realtimeButtons->addStretch();
    realtimeForm->addRow(realtimeButtons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->addLayout(buttons);
    layout->addWidget(markerGroup);
    layout->addWidget(gaugeGroup);
    layout->addWidget(realtimeGroup);
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
    connect(m_createGaugeButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::createGaugeRequested);
    connect(m_editGaugeButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::editGaugeRequested);
    connect(m_updateGaugeReadingButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::updateGaugeReadingRequested);
    connect(m_visualGaugeReadingButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::visualGaugeReadingRequested);
    connect(m_viewGaugeHistoryButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::viewGaugeHistoryRequested);
    connect(m_configureGaugeStatusRuleButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::configureGaugeStatusRuleRequested);
    connect(m_deleteGaugeButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::deleteGaugeRequested);
    connect(m_startMockSensorButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::startMockSensorRequested);
    connect(m_stopMockSensorButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::stopMockSensorRequested);
    connect(m_recordCurrentSensorSampleButton,
            &QPushButton::clicked,
            this,
            &ReconstructionPanel::recordCurrentSensorSampleRequested);
    updateControls();
}

QPushButton* ReconstructionPanel::startButton() const { return m_startButton; }

QPushButton* ReconstructionPanel::cancelButton() const { return m_cancelButton; }

QPushButton* ReconstructionPanel::viewModelButton() const { return m_viewModelButton; }

QPushButton* ReconstructionPanel::resetViewButton() const { return m_resetViewButton; }

QPushButton* ReconstructionPanel::addMarkerButton() const { return m_addMarkerButton; }

QPushButton* ReconstructionPanel::deleteMarkerButton() const { return m_deleteMarkerButton; }

QPushButton* ReconstructionPanel::createGaugeButton() const { return m_createGaugeButton; }

QPushButton* ReconstructionPanel::editGaugeButton() const { return m_editGaugeButton; }

QPushButton* ReconstructionPanel::updateGaugeReadingButton() const
{
    return m_updateGaugeReadingButton;
}

QPushButton* ReconstructionPanel::visualGaugeReadingButton() const
{
    return m_visualGaugeReadingButton;
}

QPushButton* ReconstructionPanel::viewGaugeHistoryButton() const
{
    return m_viewGaugeHistoryButton;
}

QPushButton* ReconstructionPanel::configureGaugeStatusRuleButton() const
{
    return m_configureGaugeStatusRuleButton;
}

QPushButton* ReconstructionPanel::deleteGaugeButton() const { return m_deleteGaugeButton; }

QPushButton* ReconstructionPanel::startMockSensorButton() const
{
    return m_startMockSensorButton;
}

QPushButton* ReconstructionPanel::stopMockSensorButton() const
{
    return m_stopMockSensorButton;
}

QPushButton* ReconstructionPanel::recordCurrentSensorSampleButton() const
{
    return m_recordCurrentSensorSampleButton;
}

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
    m_progressLabel->setText(QStringLiteral("%1/8").arg(completed));
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
    m_gaugeHistoryCount = 0;
    m_canDeleteMarker = false;
    setSelectedGaugeDetails(std::nullopt, GaugeStatus::Unknown);
    setGaugeActionEnabled(false, false, false, false);
    setVisualGaugeReadingEnabled(false);
    setGaugeStatusRuleEnabled(false);
    setRealtimeMonitoringState(false,
                               realtime::GaugeDataSourceState::Stopped,
                               std::nullopt,
                               GaugeStatus::Unknown,
                               QString());
    updateControls();
}

void ReconstructionPanel::setSelectedGaugeDetails(const std::optional<GaugeAsset>& asset,
                                                  GaugeStatus status)
{
    if (!asset.has_value()) {
        m_gaugeLabel->setText(QStringLiteral("未绑定仪表"));
        return;
    }

    const QString latestValue = asset->latestValue.has_value()
        ? QStringLiteral("%1 %2")
              .arg(QString::number(*asset->latestValue, 'g', 15), asset->unit)
        : QStringLiteral("暂无");
    const QString latestTimestamp = asset->latestTimestamp.has_value()
        ? asset->latestTimestamp->toLocalTime().toString(Qt::ISODateWithMs)
        : QStringLiteral("暂无");
    const QString statusRule = asset->statusRule.has_value() && asset->statusRule->isConfigured()
        ? QStringLiteral("已配置")
        : QStringLiteral("未配置");
    m_gaugeLabel->setText(
        QStringLiteral("ID: %1\n设备: %2\n量程: %3 ~ %4 %5\n视觉 Profile: %6\n最新读数: %7\n数据来源: %8\n更新时间: %9\n历史记录: %10\n当前状态: %11\n状态规则: %12")
            .arg(asset->id)
            .arg(asset->name)
            .arg(QString::number(asset->rangeMin, 'g', 15))
            .arg(QString::number(asset->rangeMax, 'g', 15))
            .arg(asset->unit)
            .arg(asset->gaugeProfileId.isEmpty() ? QStringLiteral("未绑定")
                                                  : asset->gaugeProfileId)
            .arg(latestValue)
            .arg(gaugeDataSourceDisplayName(asset->dataSource))
            .arg(latestTimestamp)
            .arg(QString::number(static_cast<long long>(m_gaugeHistoryCount)))
            .arg(gaugeStatusDisplayName(status))
            .arg(statusRule));
}

void ReconstructionPanel::setSelectedGaugeHistoryCount(qsizetype count)
{
    m_gaugeHistoryCount = count < 0 ? 0 : count;
}

void ReconstructionPanel::setGaugeActionEnabled(bool canCreate,
                                                 bool canEdit,
                                                 bool canUpdateReading,
                                                 bool canDelete)
{
    m_canCreateGauge = canCreate;
    m_canEditGauge = canEdit;
    m_canUpdateGaugeReading = canUpdateReading;
    m_canDeleteGauge = canDelete;
    updateControls();
}

void ReconstructionPanel::setVisualGaugeReadingEnabled(bool enabled)
{
    m_canVisualGaugeReading = enabled;
    m_canViewGaugeHistory = enabled;
    updateControls();
}

void ReconstructionPanel::setGaugeStatusRuleEnabled(bool enabled)
{
    m_canConfigureGaugeStatusRule = enabled;
    updateControls();
}

void ReconstructionPanel::setRealtimeMonitoringState(
    bool hasGauge,
    realtime::GaugeDataSourceState state,
    const std::optional<realtime::GaugeLiveState>& liveState,
    GaugeStatus status,
    const QString& unit)
{
    m_realtimeHasGauge = hasGauge;
    m_realtimeState = state;
    m_realtimeLiveState = liveState;
    m_realtimeStateLabel->setText(realtime::gaugeDataSourceStateDisplayName(state));
    if (liveState.has_value() && liveState->isValid()) {
        m_realtimeValueLabel->setText(
            QStringLiteral("%1 %2")
                .arg(QString::number(liveState->value, 'g', 15), unit));
        m_realtimeTimestampLabel->setText(
            liveState->timestamp.toLocalTime().toString(Qt::ISODateWithMs));
        m_realtimeSourceLabel->setText(gaugeDataSourceDisplayName(liveState->source));
    } else {
        m_realtimeValueLabel->setText(QStringLiteral("暂无"));
        m_realtimeTimestampLabel->setText(QStringLiteral("暂无"));
        m_realtimeSourceLabel->setText(QStringLiteral("-"));
    }
    m_realtimeStatusLabel->setText(gaugeStatusDisplayName(status));
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
    m_createGaugeButton->setEnabled(!m_running && m_viewerLoaded && m_canCreateGauge);
    m_editGaugeButton->setEnabled(!m_running && m_viewerLoaded && m_canEditGauge);
    m_updateGaugeReadingButton->setEnabled(
        !m_running && m_viewerLoaded && m_canUpdateGaugeReading);
    m_visualGaugeReadingButton->setEnabled(
        !m_running && m_viewerLoaded && m_canVisualGaugeReading);
    m_viewGaugeHistoryButton->setEnabled(
        !m_running && m_viewerLoaded && m_canViewGaugeHistory);
    m_configureGaugeStatusRuleButton->setEnabled(
        !m_running && m_viewerLoaded && m_canConfigureGaugeStatusRule);
    m_deleteGaugeButton->setEnabled(!m_running && m_viewerLoaded && m_canDeleteGauge);
    m_startMockSensorButton->setEnabled(
        !m_running && m_viewerLoaded && m_realtimeHasGauge
        && m_realtimeState != realtime::GaugeDataSourceState::Running);
    m_stopMockSensorButton->setEnabled(
        !m_running && m_viewerLoaded && m_realtimeHasGauge
        && m_realtimeState == realtime::GaugeDataSourceState::Running);
    m_recordCurrentSensorSampleButton->setEnabled(
        !m_running && m_viewerLoaded && m_realtimeHasGauge
        && m_realtimeLiveState.has_value() && m_realtimeLiveState->isValid());
}

} // namespace vision3d
