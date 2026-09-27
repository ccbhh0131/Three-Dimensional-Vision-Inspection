#pragma once

#include "core/project/ProjectManager.h"
#include "core/realtime/GaugeLiveState.h"

#include <QObject>
#include <QVector>

#include <optional>

namespace vision3d::realtime {

class RealtimeMonitoringController final : public QObject
{
    Q_OBJECT

public:
    explicit RealtimeMonitoringController(ProjectManager* projectManager,
                                           QObject* parent = nullptr);
    ~RealtimeMonitoringController() override;

    bool startMockSensor(const QString& gaugeAssetId, QString* error = nullptr);
    bool startMockSensor(const QString& gaugeAssetId,
                         const QVector<double>& sequence,
                         int intervalMs,
                         QString* error = nullptr);
    bool startModbusTcp(const ::vision3d::ModbusTcpBinding& binding,
                        QString* error = nullptr);
    void stop();
    void stopForGauge(const QString& gaugeAssetId);

    bool isRunning() const;
    GaugeDataSourceState state() const;
    QString activeGaugeAssetId() const;
    QString activeProjectDirectory() const;
    std::optional<GaugeLiveState> liveStateForGauge(const QString& gaugeAssetId) const;
    const GaugeLiveStateModel& liveStateModel() const;

    bool acceptSample(const GaugeSample& sample, QString* error = nullptr);
    bool recordCurrentValue(const QString& gaugeAssetId, QString* error = nullptr);

signals:
    void liveStateChanged(const QString& gaugeAssetId);
    void monitoringStateChanged(const QString& gaugeAssetId,
                                vision3d::realtime::GaugeDataSourceState state);
    void errorOccurred(const QString& error);

private slots:
    void onSourceSampleReceived(const GaugeSample& sample);
    void onSourceStateChanged(GaugeDataSourceState state);
    void onSourceError(const QString& error);
    void onProjectChanged();

private:
    bool startMockSensorInternal(const QString& gaugeAssetId,
                                 const QVector<double>& sequence,
                                 int intervalMs,
                                 QString* error);
    bool startSource(GaugeDataSource* source,
                     const QString& gaugeAssetId,
                     QString* error);
    void stopInternal(bool clearActiveGauge);
    bool fail(const QString& message, QString* error);

    ProjectManager* m_projectManager = nullptr;
    GaugeDataSource* m_source = nullptr;
    GaugeLiveStateModel m_liveStateModel;
    GaugeDataSourceState m_state = GaugeDataSourceState::Stopped;
    QString m_activeGaugeAssetId;
    QString m_activeProjectDirectory;
};

} // namespace vision3d::realtime
