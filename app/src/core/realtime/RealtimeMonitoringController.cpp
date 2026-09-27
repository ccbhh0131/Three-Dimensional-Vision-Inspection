#include "core/realtime/RealtimeMonitoringController.h"

#include "core/realtime/ModbusTcpGaugeDataSource.h"

#include <QDateTime>

namespace vision3d::realtime {

RealtimeMonitoringController::RealtimeMonitoringController(ProjectManager* projectManager,
                                                           QObject* parent)
    : QObject(parent)
    , m_projectManager(projectManager)
{
    if (m_projectManager != nullptr) {
        connect(m_projectManager,
                &ProjectManager::projectChanged,
                this,
                &RealtimeMonitoringController::onProjectChanged);
    }
}

RealtimeMonitoringController::~RealtimeMonitoringController()
{
    stop();
}

bool RealtimeMonitoringController::startMockSensor(const QString& gaugeAssetId,
                                                   QString* error)
{
    return startMockSensorInternal(gaugeAssetId,
                                   MockSensorDataSource::defaultSequence(),
                                   500,
                                   error);
}

bool RealtimeMonitoringController::startMockSensor(const QString& gaugeAssetId,
                                                   const QVector<double>& sequence,
                                                   int intervalMs,
                                                   QString* error)
{
    return startMockSensorInternal(gaugeAssetId, sequence, intervalMs, error);
}

bool RealtimeMonitoringController::startModbusTcp(
    const ::vision3d::ModbusTcpBinding& binding,
    QString* error)
{
    QString validationError;
    if (!binding.isValid(&validationError)) {
        return fail(validationError, error);
    }
    const QString normalizedId = binding.gaugeAssetId.trimmed();
    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        return fail(QStringLiteral("当前没有打开的项目，无法启动实时监控。"), error);
    }
    if (!m_projectManager->gaugeAssetById(normalizedId).has_value()) {
        return fail(QStringLiteral("未找到要监控的 GaugeAsset: %1").arg(normalizedId), error);
    }
    return startSource(new ModbusTcpGaugeDataSource(binding, this), normalizedId, error);
}

void RealtimeMonitoringController::stop()
{
    stopInternal(true);
}

void RealtimeMonitoringController::stopForGauge(const QString& gaugeAssetId)
{
    const QString normalizedId = gaugeAssetId.trimmed();
    if (normalizedId.isEmpty()) {
        return;
    }
    if (m_activeGaugeAssetId == normalizedId) {
        stop();
        return;
    }
    if (m_liveStateModel.contains(normalizedId)) {
        m_liveStateModel.clearGauge(normalizedId);
        emit liveStateChanged(normalizedId);
    }
}

bool RealtimeMonitoringController::isRunning() const
{
    return m_source != nullptr && m_source->isRunning()
        && m_state == GaugeDataSourceState::Running;
}

GaugeDataSourceState RealtimeMonitoringController::state() const
{
    return m_state;
}

QString RealtimeMonitoringController::activeGaugeAssetId() const
{
    return m_activeGaugeAssetId;
}

QString RealtimeMonitoringController::activeProjectDirectory() const
{
    return m_activeProjectDirectory;
}

std::optional<GaugeLiveState> RealtimeMonitoringController::liveStateForGauge(
    const QString& gaugeAssetId) const
{
    if (!isRunning()) {
        return std::nullopt;
    }
    return m_liveStateModel.stateForGauge(gaugeAssetId);
}

const GaugeLiveStateModel& RealtimeMonitoringController::liveStateModel() const
{
    return m_liveStateModel;
}

bool RealtimeMonitoringController::acceptSample(const GaugeSample& sample, QString* error)
{
    QString sampleError;
    if (!sample.isValid(&sampleError)) {
        return fail(QStringLiteral("拒绝实时样本: %1").arg(sampleError), error);
    }
    if (!isRunning()) {
        return fail(QStringLiteral("实时数据源未运行，拒绝实时样本。"), error);
    }
    if (sample.gaugeAssetId.trimmed() != m_activeGaugeAssetId) {
        return fail(QStringLiteral("实时样本 GaugeAsset 不属于当前监控目标: %1")
                       .arg(sample.gaugeAssetId),
                   error);
    }
    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        return fail(QStringLiteral("当前没有打开的项目，拒绝实时样本。"), error);
    }
    if (!m_projectManager->gaugeAssetById(sample.gaugeAssetId).has_value()) {
        return fail(QStringLiteral("实时样本 GaugeAsset 不存在: %1").arg(sample.gaugeAssetId),
                   error);
    }

    QString liveError;
    if (!m_liveStateModel.updateSample(sample, &liveError)) {
        return fail(QStringLiteral("实时样本未能更新 Live State: %1").arg(liveError), error);
    }
    if (error != nullptr) {
        error->clear();
    }
    emit liveStateChanged(sample.gaugeAssetId.trimmed());
    return true;
}

bool RealtimeMonitoringController::recordCurrentValue(const QString& gaugeAssetId,
                                                      QString* error)
{
    const QString normalizedId = gaugeAssetId.trimmed();
    if (!isRunning()) {
        return fail(QStringLiteral("实时数据源未运行，无法记录当前值。"), error);
    }
    const std::optional<GaugeLiveState> live = m_liveStateModel.stateForGauge(normalizedId);
    if (!live.has_value() || !live->isValid()) {
        return fail(QStringLiteral("当前 Gauge 没有有效的实时值。"), error);
    }
    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        return fail(QStringLiteral("当前没有打开的项目，无法记录当前值。"), error);
    }
    if (!m_projectManager->gaugeAssetById(normalizedId).has_value()) {
        return fail(QStringLiteral("未找到要记录的 GaugeAsset: %1").arg(normalizedId), error);
    }
    if (!m_projectManager->recordGaugeReading(normalizedId,
                                              live->value,
                                              live->timestamp,
                                              ::vision3d::GaugeDataSource::Sensor,
                                              error)) {
        return false;
    }
    return true;
}

bool RealtimeMonitoringController::startMockSensorInternal(const QString& gaugeAssetId,
                                                            const QVector<double>& sequence,
                                                            int intervalMs,
                                                            QString* error)
{
    const QString normalizedId = gaugeAssetId.trimmed();
    if (m_projectManager == nullptr || !m_projectManager->hasProject()) {
        return fail(QStringLiteral("当前没有打开的项目，无法启动实时监控。"), error);
    }
    if (normalizedId.isEmpty()) {
        return fail(QStringLiteral("启动实时监控必须指定 GaugeAsset。"), error);
    }
    if (!m_projectManager->gaugeAssetById(normalizedId).has_value()) {
        return fail(QStringLiteral("未找到要监控的 GaugeAsset: %1").arg(normalizedId), error);
    }

    return startSource(new MockSensorDataSource(normalizedId, sequence, intervalMs, this),
                       normalizedId,
                       error);
}

bool RealtimeMonitoringController::startSource(GaugeDataSource* source,
                                                const QString& gaugeAssetId,
                                                QString* error)
{
    if (source == nullptr) {
        return fail(QStringLiteral("实时数据源不能为空。"), error);
    }
    const QString normalizedId = gaugeAssetId.trimmed();
    if (normalizedId.isEmpty()) {
        source->deleteLater();
        return fail(QStringLiteral("启动实时监控必须指定 GaugeAsset。"), error);
    }

    stop();
    m_activeGaugeAssetId = normalizedId;
    m_activeProjectDirectory = m_projectManager->projectDirectory();
    m_source = source;
    connect(m_source,
            &GaugeDataSource::sampleReceived,
            this,
            &RealtimeMonitoringController::onSourceSampleReceived);
    connect(m_source,
            &GaugeDataSource::stateChanged,
            this,
            &RealtimeMonitoringController::onSourceStateChanged);
    connect(m_source,
            &GaugeDataSource::errorOccurred,
            this,
            &RealtimeMonitoringController::onSourceError);

    QString startError;
    if (!m_source->start(&startError)) {
        if (error != nullptr) {
            *error = startError;
        }
        m_state = m_source->state();
        emit monitoringStateChanged(m_activeGaugeAssetId, m_state);
        return false;
    }
    m_state = m_source->state();
    emit monitoringStateChanged(m_activeGaugeAssetId, m_state);
    return true;
}

void RealtimeMonitoringController::onSourceSampleReceived(const GaugeSample& sample)
{
    acceptSample(sample, nullptr);
}

void RealtimeMonitoringController::onSourceStateChanged(GaugeDataSourceState state)
{
    m_state = state;
    if (!m_activeGaugeAssetId.isEmpty()) {
        emit monitoringStateChanged(m_activeGaugeAssetId, m_state);
    }
}

void RealtimeMonitoringController::onSourceError(const QString& error)
{
    m_state = GaugeDataSourceState::Error;
    emit errorOccurred(error);
    if (!m_activeGaugeAssetId.isEmpty()) {
        emit monitoringStateChanged(m_activeGaugeAssetId, m_state);
    }
}

void RealtimeMonitoringController::onProjectChanged()
{
    if (m_activeGaugeAssetId.isEmpty() || m_projectManager == nullptr) {
        return;
    }
    if (!m_projectManager->hasProject()
        || m_projectManager->projectDirectory() != m_activeProjectDirectory
        || !m_projectManager->gaugeAssetById(m_activeGaugeAssetId).has_value()) {
        stop();
    }
}

void RealtimeMonitoringController::stopInternal(bool clearActiveGauge)
{
    const QString gaugeId = m_activeGaugeAssetId;
    if (m_source != nullptr) {
        m_source->stop();
        delete m_source;
        m_source = nullptr;
    }
    if (!gaugeId.isEmpty()) {
        m_liveStateModel.clearGauge(gaugeId);
    } else {
        m_liveStateModel.clearAll();
    }
    m_state = GaugeDataSourceState::Stopped;
    if (!gaugeId.isEmpty()) {
        emit liveStateChanged(gaugeId);
        emit monitoringStateChanged(gaugeId, m_state);
    }
    if (clearActiveGauge) {
        m_activeGaugeAssetId.clear();
        m_activeProjectDirectory.clear();
    }
}

bool RealtimeMonitoringController::fail(const QString& message, QString* error)
{
    if (error != nullptr) {
        *error = message;
    }
    emit errorOccurred(message);
    return false;
}

} // namespace vision3d::realtime
