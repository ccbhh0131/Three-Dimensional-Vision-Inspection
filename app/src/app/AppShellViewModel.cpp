#include "app/AppShellViewModel.h"

#include "app/AppPreferences.h"
#include "core/device/GaugeStatus.h"
#include "core/project/ProjectManager.h"
#include "core/realtime/RealtimeMonitoringController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace vision3d {

namespace {

QString pageTitleFor(const QString& page)
{
    if (page == QStringLiteral("scene")) {
        return QStringLiteral("三维场景");
    }
    if (page == QStringLiteral("visual")) {
        return QStringLiteral("视觉巡检");
    }
    if (page == QStringLiteral("realtime")) {
        return QStringLiteral("实时监控");
    }
    if (page == QStringLiteral("history")) {
        return QStringLiteral("历史记录");
    }
    if (page == QStringLiteral("reconstruction")) {
        return QStringLiteral("重建任务");
    }
    if (page == QStringLiteral("settings")) {
        return QStringLiteral("设置");
    }
    return QStringLiteral("项目总览");
}

QString stateDisplay(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("completed")) {
        return QStringLiteral("已完成");
    }
    if (normalized == QStringLiteral("running")) {
        return QStringLiteral("进行中");
    }
    if (normalized == QStringLiteral("preparing")) {
        return QStringLiteral("准备中");
    }
    if (normalized == QStringLiteral("failed")) {
        return QStringLiteral("失败");
    }
    if (normalized == QStringLiteral("cancelled")) {
        return QStringLiteral("已取消");
    }
    if (normalized == QStringLiteral("interrupted")) {
        return QStringLiteral("已中断");
    }
    return QStringLiteral("未开始");
}

QString stageDisplay(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("preparing_input")) {
        return QStringLiteral("准备输入");
    }
    if (normalized == QStringLiteral("feature_extraction")) {
        return QStringLiteral("特征提取");
    }
    if (normalized == QStringLiteral("feature_matching")) {
        return QStringLiteral("特征匹配");
    }
    if (normalized == QStringLiteral("sparse_mapping")) {
        return QStringLiteral("稀疏建图");
    }
    if (normalized == QStringLiteral("image_undistortion")) {
        return QStringLiteral("图像校正");
    }
    if (normalized == QStringLiteral("dense_stereo")) {
        return QStringLiteral("稠密立体");
    }
    if (normalized == QStringLiteral("stereo_fusion")) {
        return QStringLiteral("点云融合");
    }
    if (normalized == QStringLiteral("meshing")) {
        return QStringLiteral("网格生成");
    }
    if (normalized == QStringLiteral("model_optimization")) {
        return QStringLiteral("模型优化");
    }
    if (normalized == QStringLiteral("completed")) {
        return QStringLiteral("已完成");
    }
    return QStringLiteral("等待任务");
}

QString formatNumber(double value)
{
    return QString::number(value, 'f', std::abs(value) >= 100.0 ? 1 : 2);
}

QString formatVector(const QVector3D& value)
{
    return QStringLiteral("(%1, %2, %3)")
        .arg(QString::number(value.x(), 'f', 3))
        .arg(QString::number(value.y(), 'f', 3))
        .arg(QString::number(value.z(), 'f', 3));
}

QString sourceDisplay(GaugeDataSource source)
{
    return gaugeDataSourceDisplayName(source);
}

} // namespace

AppShellViewModel::AppShellViewModel(
    ProjectManager* projectManager,
    realtime::RealtimeMonitoringController* realtimeController,
    AppPreferences* preferences,
    QObject* parent)
    : QObject(parent)
    , m_projectManager(projectManager)
    , m_realtimeController(realtimeController)
    , m_preferences(preferences)
{
    if (m_projectManager != nullptr) {
        connect(m_projectManager,
                &ProjectManager::projectChanged,
                this,
                &AppShellViewModel::refresh);
    }
    if (m_realtimeController != nullptr) {
        connect(m_realtimeController,
                &realtime::RealtimeMonitoringController::liveStateChanged,
                this,
                [this](const QString&) { refresh(); });
        connect(m_realtimeController,
                &realtime::RealtimeMonitoringController::monitoringStateChanged,
                this,
                [this](const QString&, realtime::GaugeDataSourceState) { refresh(); });
    }
    refresh();
}

QString AppShellViewModel::currentPage() const { return m_currentPage; }
QString AppShellViewModel::pageTitle() const { return pageTitleFor(m_currentPage); }
bool AppShellViewModel::hasProject() const { return m_hasProject; }
QString AppShellViewModel::projectName() const { return m_projectName; }
QString AppShellViewModel::projectSummary() const { return m_projectSummary; }
int AppShellViewModel::imageCount() const { return m_imageCount; }
int AppShellViewModel::markerCount() const { return m_markerCount; }
int AppShellViewModel::gaugeCount() const { return m_gaugeCount; }
int AppShellViewModel::inspectionCount() const { return m_inspectionCount; }
int AppShellViewModel::alarmCount() const { return m_alarmCount; }
QString AppShellViewModel::reconstructionText() const { return m_reconstructionText; }
QString AppShellViewModel::reconstructionStageText() const { return m_reconstructionStageText; }
int AppShellViewModel::reconstructionProgress() const { return m_reconstructionProgress; }
QString AppShellViewModel::reconstructionArtifactText() const { return m_reconstructionArtifactText; }
QString AppShellViewModel::engineText() const { return m_engineText; }
QString AppShellViewModel::viewerText() const { return m_viewerText; }
bool AppShellViewModel::alignmentEditing() const { return m_alignmentEditing; }
bool AppShellViewModel::canAddMarker() const { return m_canAddMarker; }
bool AppShellViewModel::canDeleteMarker() const { return m_canDeleteMarker; }

QString AppShellViewModel::deviceName() const { return m_deviceName; }
QString AppShellViewModel::gaugeName() const { return m_gaugeName; }
QString AppShellViewModel::gaugeId() const { return m_gaugeId; }
QString AppShellViewModel::currentReadingText() const { return m_currentReadingText; }
QString AppShellViewModel::readingUnit() const { return m_readingUnit; }
QString AppShellViewModel::readingSourceText() const { return m_readingSourceText; }
QString AppShellViewModel::gaugeStatusText() const { return m_gaugeStatusText; }
QString AppShellViewModel::gaugeStatusCode() const { return m_gaugeStatusCode; }
QString AppShellViewModel::rangeText() const { return m_rangeText; }
QString AppShellViewModel::profileText() const { return m_profileText; }
QString AppShellViewModel::markerPositionText() const { return m_markerPositionText; }
QString AppShellViewModel::updatedText() const { return m_updatedText; }
QString AppShellViewModel::historyCountText() const { return m_historyCountText; }

QString AppShellViewModel::realtimeStateText() const { return m_realtimeStateText; }
QString AppShellViewModel::realtimeValueText() const { return m_realtimeValueText; }
QString AppShellViewModel::realtimeSourceText() const { return m_realtimeSourceText; }
QString AppShellViewModel::realtimeConnectionText() const { return m_realtimeConnectionText; }
QString AppShellViewModel::realtimeHostText() const { return m_realtimeHostText; }
QString AppShellViewModel::realtimeRegisterText() const { return m_realtimeRegisterText; }
QString AppShellViewModel::realtimePollingText() const { return m_realtimePollingText; }
bool AppShellViewModel::realtimeRunning() const { return m_realtimeRunning; }

QVariantList AppShellViewModel::historyRows() const { return m_historyRows; }
QString AppShellViewModel::visualImageSource() const { return m_visualImageSource; }
QString AppShellViewModel::statusBarText() const { return m_statusBarText; }

QString AppShellViewModel::defaultProjectDirectory() const
{
    return m_preferences == nullptr ? QString() : m_preferences->defaultProjectDirectory();
}

QString AppShellViewModel::lastProjectDirectory() const
{
    return m_preferences == nullptr ? QString() : m_preferences->lastProjectDirectory();
}

bool AppShellViewModel::restoreLastProject() const
{
    return m_preferences != nullptr && m_preferences->restoreLastProject();
}

bool AppShellViewModel::rememberLastDirectory() const
{
    return m_preferences == nullptr || m_preferences->rememberLastDirectory();
}

bool AppShellViewModel::autoFit() const
{
    return m_preferences == nullptr || m_preferences->autoFit();
}

double AppShellViewModel::orbitSensitivity() const
{
    return m_preferences == nullptr ? 1.0 : m_preferences->orbitSensitivity();
}

bool AppShellViewModel::showMarkers() const
{
    return m_preferences == nullptr || m_preferences->showMarkers();
}

double AppShellViewModel::markerSize() const
{
    return m_preferences == nullptr ? 1.0 : m_preferences->markerSize();
}

int AppShellViewModel::realtimePollIntervalMs() const
{
    return m_preferences == nullptr ? 500 : m_preferences->realtimePollIntervalMs();
}

QString AppShellViewModel::productVersion() const
{
    const QString version = QCoreApplication::applicationVersion().trimmed();
    return version.isEmpty() ? QStringLiteral("Development") : version;
}

QString AppShellViewModel::buildType() const
{
#ifdef QT_DEBUG
    return QStringLiteral("Debug");
#else
    return QStringLiteral("Release");
#endif
}

QString AppShellViewModel::configDirectory() const
{
    return m_preferences == nullptr ? QString() : AppPreferences::configDirectory();
}

void AppShellViewModel::selectPage(const QString& page)
{
    QString normalized = page.trimmed().toLower();
    if (normalized == QStringLiteral("overview") || normalized == QStringLiteral("project")) {
        normalized = QStringLiteral("overview");
    } else if (normalized == QStringLiteral("scene")) {
        normalized = QStringLiteral("scene");
    } else if (normalized == QStringLiteral("visual")
               || normalized == QStringLiteral("inspection")) {
        normalized = QStringLiteral("visual");
    } else if (normalized == QStringLiteral("realtime")
               || normalized == QStringLiteral("monitoring")) {
        normalized = QStringLiteral("realtime");
    } else if (normalized == QStringLiteral("history")) {
        normalized = QStringLiteral("history");
    } else if (normalized == QStringLiteral("reconstruction")) {
        normalized = QStringLiteral("reconstruction");
    } else if (normalized == QStringLiteral("settings")) {
        normalized = QStringLiteral("settings");
    } else {
        normalized = QStringLiteral("overview");
    }
    if (m_currentPage == normalized) {
        emit dataChanged();
        return;
    }
    m_currentPage = normalized;
    emit currentPageChanged();
    emit dataChanged();
}

void AppShellViewModel::requestCreateProject() { emit createProjectRequested(); }
void AppShellViewModel::requestOpenProject() { emit openProjectRequested(); }
void AppShellViewModel::requestImportImages() { emit importImagesRequested(); }
void AppShellViewModel::requestOpenViewer()
{
    if (!m_selectedMarkerId.isEmpty()) {
        emit markerSelectionRequested(m_selectedMarkerId);
    }
    emit openViewerRequested();
}
void AppShellViewModel::requestFitViewer() { emit fitViewerRequested(); }
void AppShellViewModel::requestResetViewer() { emit resetViewerRequested(); }
void AppShellViewModel::requestCameraView(const QString& view)
{
    emit cameraViewRequested(view);
}
void AppShellViewModel::requestBeginSceneAlignment()
{
    emit beginSceneAlignmentRequested();
}
void AppShellViewModel::requestSceneAlignmentRotation(const QString& axis, double degrees)
{
    emit sceneAlignmentRotationRequested(axis, degrees);
}
void AppShellViewModel::requestResetSceneAlignment()
{
    emit resetSceneAlignmentRequested();
}
void AppShellViewModel::requestCancelSceneAlignment()
{
    emit cancelSceneAlignmentRequested();
}
void AppShellViewModel::requestSaveSceneAlignment()
{
    emit saveSceneAlignmentRequested();
}
void AppShellViewModel::requestAddMarker()
{
    if (m_canAddMarker) {
        emit addMarkerRequested();
    }
}
void AppShellViewModel::requestDeleteMarker()
{
    if (m_canDeleteMarker) {
        emit deleteMarkerRequested();
    }
}
void AppShellViewModel::requestVisualReading()
{
    if (!m_selectedMarkerId.isEmpty()) {
        emit markerSelectionRequested(m_selectedMarkerId);
    }
    emit visualReadingRequested();
}
void AppShellViewModel::requestManualReading()
{
    if (!m_selectedMarkerId.isEmpty()) {
        emit markerSelectionRequested(m_selectedMarkerId);
    }
    emit manualReadingRequested();
}
void AppShellViewModel::requestShowHistory() { emit showHistoryRequested(); }
void AppShellViewModel::requestCreateGauge() { emit createGaugeRequested(); }
void AppShellViewModel::requestEditGauge() { emit editGaugeRequested(); }
void AppShellViewModel::requestConfigureRule() { emit configureRuleRequested(); }
void AppShellViewModel::requestStartRealtime()
{
    if (!m_selectedMarkerId.isEmpty()) {
        emit markerSelectionRequested(m_selectedMarkerId);
    }
    emit startRealtimeRequested();
}
void AppShellViewModel::requestStopRealtime() { emit stopRealtimeRequested(); }
void AppShellViewModel::requestRecordRealtime() { emit recordRealtimeRequested(); }
void AppShellViewModel::requestSettings() { emit settingsRequested(); }
void AppShellViewModel::requestSelectDefaultProjectDirectory()
{
    emit selectDefaultProjectDirectoryRequested();
}
void AppShellViewModel::requestOpenThirdPartyLicenses()
{
    emit openThirdPartyLicensesRequested();
}
void AppShellViewModel::requestResetPreferences()
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->reset();
    emit viewerPreferencesChanged();
    refresh();
}
void AppShellViewModel::setRestoreLastProject(bool enabled)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setRestoreLastProject(enabled);
    refresh();
}
void AppShellViewModel::setRememberLastDirectory(bool enabled)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setRememberLastDirectory(enabled);
    refresh();
}
void AppShellViewModel::setAutoFit(bool enabled)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setAutoFit(enabled);
    emit viewerPreferencesChanged();
    refresh();
}
void AppShellViewModel::setOrbitSensitivity(double sensitivity)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setOrbitSensitivity(sensitivity);
    emit viewerPreferencesChanged();
    refresh();
}
void AppShellViewModel::setShowMarkers(bool enabled)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setShowMarkers(enabled);
    emit viewerPreferencesChanged();
    refresh();
}
void AppShellViewModel::setMarkerSize(double scale)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setMarkerSize(scale);
    emit viewerPreferencesChanged();
    refresh();
}
void AppShellViewModel::setRealtimePollIntervalMs(int intervalMs)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setRealtimePollIntervalMs(intervalMs);
    refresh();
}
void AppShellViewModel::setDefaultProjectDirectory(const QString& directory)
{
    if (m_preferences == nullptr) {
        return;
    }
    m_preferences->setDefaultProjectDirectory(directory);
    refresh();
}

void AppShellViewModel::setSelectedMarkerId(const QString& markerId)
{
    const QString normalized = markerId.trimmed();
    if (m_selectedMarkerId == normalized) {
        refresh();
        return;
    }
    m_selectedMarkerId = normalized;
    refresh();
}

void AppShellViewModel::setAlignmentEditing(bool editing)
{
    if (m_alignmentEditing == editing) {
        emit dataChanged();
        return;
    }
    m_alignmentEditing = editing;
    emit dataChanged();
}

void AppShellViewModel::setCanAddMarker(bool canAddMarker)
{
    if (m_canAddMarker == canAddMarker) {
        emit dataChanged();
        return;
    }
    m_canAddMarker = canAddMarker;
    emit dataChanged();
}

void AppShellViewModel::setCanDeleteMarker(bool canDeleteMarker)
{
    if (m_canDeleteMarker == canDeleteMarker) {
        emit dataChanged();
        return;
    }
    m_canDeleteMarker = canDeleteMarker;
    emit dataChanged();
}

void AppShellViewModel::refresh()
{
    m_hasProject = false;
    m_projectName = QStringLiteral("未打开项目");
    m_projectSummary = QStringLiteral("打开或创建项目后，这里会显示项目概览。");
    m_imageCount = 0;
    m_markerCount = 0;
    m_gaugeCount = 0;
    m_inspectionCount = 0;
    m_alarmCount = 0;
    m_reconstructionText = QStringLiteral("未开始");
    m_reconstructionStageText = QStringLiteral("等待任务");
    m_reconstructionProgress = 0;
    m_reconstructionArtifactText = QStringLiteral("暂无有效网格结果");
    m_engineText = QStringLiteral("未检测");
    m_viewerText = QStringLiteral("未加载模型");
    m_canAddMarker = false;
    m_canDeleteMarker = false;
    m_deviceName = QStringLiteral("未选择设备");
    m_gaugeName = QStringLiteral("未绑定仪表");
    m_gaugeId.clear();
    m_currentReadingText = QStringLiteral("—");
    m_readingUnit.clear();
    m_readingSourceText = QStringLiteral("暂无读数");
    m_gaugeStatusText = QStringLiteral("未知");
    m_gaugeStatusCode = QStringLiteral("unknown");
    m_rangeText = QStringLiteral("—");
    m_profileText = QStringLiteral("未绑定");
    m_markerPositionText = QStringLiteral("—");
    m_updatedText = QStringLiteral("—");
    m_historyCountText = QStringLiteral("0 条记录");
    m_realtimeStateText = QStringLiteral("未启动");
    m_realtimeValueText = QStringLiteral("—");
    m_realtimeSourceText = QStringLiteral("—");
    m_realtimeConnectionText = QStringLiteral("未连接");
    m_realtimeHostText = QStringLiteral("—");
    m_realtimeRegisterText = QStringLiteral("—");
    m_realtimePollingText = QStringLiteral("%1 ms").arg(realtimePollIntervalMs());
    m_realtimeRunning = false;
    m_historyRows.clear();
    m_visualImageSource.clear();
    m_statusBarText = QStringLiteral("就绪 · Stage 5A QML Hybrid");

    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        emit dataChanged();
        return;
    }

    m_hasProject = true;
    const std::optional<ProjectManifest>& manifest = m_projectManager->currentManifest();
    if (manifest.has_value()) {
        m_projectName = manifest->name();
        m_projectSummary = QStringLiteral("项目目录 · %1")
                               .arg(QDir::toNativeSeparators(m_projectManager->projectDirectory()));
        m_engineText = manifest->backendVersion().isEmpty()
            ? QStringLiteral("待检测")
            : QStringLiteral("%1 %2").arg(manifest->backend(), manifest->backendVersion());
    }

    QString imageError;
    m_imageCount = static_cast<int>(m_projectManager->imageAssetRecords(&imageError).size());
    const QList<DeviceMarker> markers = m_projectManager->deviceMarkerModel().list();
    const QList<GaugeAsset> gauges = m_projectManager->gaugeAssets();
    const QList<InspectionRecord> records = m_projectManager->inspectionRecords();
    m_markerCount = static_cast<int>(markers.size());
    m_gaugeCount = static_cast<int>(gauges.size());
    m_inspectionCount = static_cast<int>(records.size());
    for (const GaugeAsset& gauge : gauges) {
        if (GaugeStatusEvaluator::evaluate(gauge.latestValue, gauge.statusRule)
            == GaugeStatus::Alarm) {
            ++m_alarmCount;
        }
    }

    const std::optional<ReconstructionTask> task = m_projectManager->latestReconstructionTask();
    if (task.has_value()) {
        m_reconstructionText = stateDisplay(reconstructionStateToString(task->state));
        m_reconstructionStageText = stageDisplay(reconstructionStageToString(task->stage));
        if (task->state == ReconstructionState::Completed) {
            m_reconstructionProgress = 100;
        } else if (task->state == ReconstructionState::Running
                   || task->state == ReconstructionState::Preparing) {
            m_reconstructionProgress = std::clamp(static_cast<int>(task->registrationRatio * 100.0),
                                                  8,
                                                  92);
        }
        m_reconstructionArtifactText = task->meshRelativePath.isEmpty()
            ? QStringLiteral("任务已记录，暂无网格路径")
            : task->meshRelativePath;
    }
    if (m_projectManager->latestPoissonMeshArtifact().isValid()) {
        m_viewerText = QStringLiteral("网格已就绪");
    }

    QString selectedId = m_selectedMarkerId;
    std::optional<DeviceMarker> selectedMarker;
    if (!selectedId.isEmpty()) {
        selectedMarker = m_projectManager->deviceMarkerById(selectedId);
    }
    if (!selectedMarker.has_value() && !markers.isEmpty()) {
        selectedMarker = markers.first();
        selectedId = selectedMarker->id;
    }
    if (selectedMarker.has_value()) {
        m_deviceName = selectedMarker->name;
        m_markerPositionText = formatVector(selectedMarker->worldPosition);
        const std::optional<GaugeAsset> gauge =
            m_projectManager->gaugeAssetForMarker(selectedMarker->id);
        if (gauge.has_value()) {
            m_gaugeId = gauge->id;
            m_gaugeName = gauge->name;
            m_readingUnit = gauge->unit;
            m_rangeText = QStringLiteral("%1 – %2 %3")
                              .arg(formatNumber(gauge->rangeMin))
                              .arg(formatNumber(gauge->rangeMax))
                              .arg(gauge->unit);
            m_profileText = gauge->gaugeProfileId.isEmpty()
                ? QStringLiteral("未绑定")
                : gauge->gaugeProfileId;
            std::optional<double> value = gauge->latestValue;
            GaugeDataSource source = gauge->dataSource;
            std::optional<QDateTime> timestamp = gauge->latestTimestamp;
            std::optional<realtime::GaugeLiveState> liveState;
            if (m_realtimeController != nullptr) {
                liveState = m_realtimeController->liveStateForGauge(gauge->id);
            }
            if (liveState.has_value() && liveState->available) {
                value = liveState->value;
                source = liveState->source;
                timestamp = liveState->timestamp;
            }
            if (value.has_value() && std::isfinite(*value)) {
                m_currentReadingText = formatNumber(*value);
                m_readingSourceText = sourceDisplay(source);
                m_realtimeValueText = QStringLiteral("%1 %2")
                                          .arg(m_currentReadingText, gauge->unit);
            }
            const GaugeStatus status = GaugeStatusEvaluator::evaluate(value, gauge->statusRule);
            m_gaugeStatusText = gaugeStatusDisplayName(status);
            m_gaugeStatusCode = gaugeStatusToString(status);
            if (timestamp.has_value() && timestamp->isValid()) {
                m_updatedText = timestamp->toLocalTime().toString(QStringLiteral("MM-dd HH:mm:ss"));
            }
            const QList<InspectionRecord> gaugeRecords =
                m_projectManager->inspectionRecordsForGauge(gauge->id);
            m_historyCountText = QStringLiteral("%1 条记录").arg(gaugeRecords.size());
            for (const InspectionRecord& record : gaugeRecords) {
                QVariantMap row;
                row.insert(QStringLiteral("timestamp"),
                           record.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
                row.insert(QStringLiteral("value"), formatNumber(record.value));
                row.insert(QStringLiteral("unit"), gauge->unit);
                row.insert(QStringLiteral("source"), sourceDisplay(record.dataSource));
                row.insert(QStringLiteral("status"),
                           gaugeStatusDisplayName(
                               GaugeStatusEvaluator::evaluate(record.value, gauge->statusRule)));
                row.insert(QStringLiteral("statusCode"),
                           gaugeStatusToString(
                               GaugeStatusEvaluator::evaluate(record.value, gauge->statusRule)));
                row.insert(QStringLiteral("image"), record.imageAssetId.isEmpty()
                                                       ? QStringLiteral("无关联图片")
                                                       : QStringLiteral("已关联图片"));
                m_historyRows.append(row);
            }
            std::sort(m_historyRows.begin(),
                      m_historyRows.end(),
                      [](const QVariant& left, const QVariant& right) {
                          return left.toMap().value(QStringLiteral("timestamp")).toString()
                              > right.toMap().value(QStringLiteral("timestamp")).toString();
                      });

            m_realtimeSourceText = liveState.has_value()
                ? sourceDisplay(liveState->source)
                : QStringLiteral("未启动");
            m_realtimeRunning = m_realtimeController != nullptr
                                && m_realtimeController->isRunning();
            if (m_realtimeController != nullptr) {
                m_realtimeStateText = realtime::gaugeDataSourceStateDisplayName(
                    m_realtimeController->state());
            }
            m_realtimeConnectionText = liveState.has_value() && liveState->connected
                ? QStringLiteral("已连接")
                : QStringLiteral("未连接");
            if (gauge->modbusTcpBinding.has_value()) {
                m_realtimeHostText = QStringLiteral("%1:%2")
                                         .arg(gauge->modbusTcpBinding->host)
                                         .arg(gauge->modbusTcpBinding->port);
                m_realtimeRegisterText = QStringLiteral("%1 · %2")
                                             .arg(gauge->modbusTcpBinding->startAddress)
                                             .arg(modbusDataTypeToString(
                                                 gauge->modbusTcpBinding->dataType));
            } else {
                m_realtimeHostText = QStringLiteral("Mock Sensor");
                m_realtimeRegisterText = QStringLiteral("Default sequence");
            }
            const QString selectedAssetId =
                gaugeRecords.isEmpty() ? QString() : gaugeRecords.first().imageAssetId;
            if (!selectedAssetId.isEmpty()) {
                const std::optional<AssetRecord> asset =
                    m_projectManager->assetById(selectedAssetId);
                if (asset.has_value()) {
                    const QString path = m_projectManager->absoluteAssetPath(*asset);
                    if (QFileInfo::exists(path)) {
                        m_visualImageSource = QUrl::fromLocalFile(path).toString();
                    }
                }
            }
        }
    }
    if (m_realtimeController != nullptr && m_realtimeController->isRunning()) {
        m_statusBarText = QStringLiteral("实时监控运行中 · %1").arg(m_deviceName);
    } else {
        m_statusBarText = QStringLiteral("就绪 · %1").arg(m_projectName);
    }
    emit dataChanged();
}

} // namespace vision3d
