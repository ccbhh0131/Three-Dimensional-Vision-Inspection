#include "core/device/GaugeStatus.h"
#include "core/project/ProjectManager.h"
#include "core/realtime/GaugeDataSource.h"
#include "core/realtime/GaugeLiveState.h"
#include "core/realtime/RealtimeMonitoringController.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

#include <cmath>
#include <limits>
#include <optional>

namespace {

struct RealtimeFixture
{
    QString root;
    QString projectPath;
    QString markerId;
    QString gaugeId;
};

vision3d::GaugeStatusRule fullRule()
{
    vision3d::GaugeStatusRule rule;
    rule.alarmLow = 0.2;
    rule.warningLow = 0.4;
    rule.warningHigh = 2.0;
    rule.alarmHigh = 2.3;
    return rule;
}

std::optional<RealtimeFixture> makeFixture(vision3d::ProjectManager& manager,
                                           const QString& projectName = QStringLiteral("Project"))
{
    RealtimeFixture fixture;
    fixture.root = QDir(QDir::tempPath()).filePath(
        QStringLiteral("stage4g_realtime_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(fixture.root)) {
        return std::nullopt;
    }

    QString error;
    if (!manager.newProject(fixture.root, projectName, &error)) {
        QDir(fixture.root).removeRecursively();
        return std::nullopt;
    }
    fixture.projectPath = QDir(fixture.root).filePath(projectName + QStringLiteral("/project.json"));

    vision3d::DeviceMarker marker;
    if (!manager.addDeviceMarker(QStringLiteral("P01"),
                                 QVector3D(0.0F, 0.0F, 0.0F),
                                 QStringLiteral("task"),
                                 &marker,
                                 &error)) {
        manager.closeProject();
        QDir(fixture.root).removeRecursively();
        return std::nullopt;
    }
    fixture.markerId = marker.id;

    vision3d::GaugeAsset gauge;
    if (!manager.addGaugeAsset(fixture.markerId,
                               QStringLiteral("入口压力表"),
                               0.0,
                               2.5,
                               QStringLiteral("MPa"),
                               &gauge,
                               &error)) {
        manager.closeProject();
        QDir(fixture.root).removeRecursively();
        return std::nullopt;
    }
    fixture.gaugeId = gauge.id;
    return fixture;
}

void removeFixture(vision3d::ProjectManager& manager, const RealtimeFixture& fixture)
{
    manager.closeProject();
    QDir(fixture.root).removeRecursively();
}

bool waitForLive(vision3d::realtime::RealtimeMonitoringController& controller,
                const QString& gaugeId,
                int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (controller.liveStateForGauge(gaugeId).has_value()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return controller.liveStateForGauge(gaugeId).has_value();
}

bool setPersistedValue(vision3d::ProjectManager& manager,
                       const QString& gaugeId,
                       double value,
                       vision3d::GaugeDataSource source = vision3d::GaugeDataSource::Manual)
{
    const std::optional<vision3d::GaugeAsset> existing = manager.gaugeAssetById(gaugeId);
    if (!existing.has_value()) {
        return false;
    }
    vision3d::GaugeAsset updated = *existing;
    updated.latestValue = value;
    updated.latestTimestamp = QDateTime::currentDateTimeUtc();
    updated.dataSource = source;
    updated.statusRule = fullRule();
    QString error;
    return manager.updateGaugeAsset(updated, &error);
}

} // namespace

class RealtimeMonitoringTest final : public QObject
{
    Q_OBJECT

private slots:
    void gaugeSampleValidation();
    void sourceStateStrings();
    void mockSourceStartStop();
    void deterministicSequence();
    void mockRejectsNonFiniteSequence();
    void liveStateModel();
    void controllerStartDoesNotRecord();
    void controllerRecordsSensorSnapshot();
    void sensorSnapshotReopens();
    void liveValueDrivesStatus();
    void persistedFallbackDrivesStatus();
    void stopClearsLiveState();
    void projectSwitchStopsMonitoring();
    void projectCloseStopsMonitoring();
    void gaugeDeleteStopsMonitoring();
    void markerDeleteStopsMonitoring();
    void invalidSampleRejected();
};

void RealtimeMonitoringTest::gaugeSampleValidation()
{
    const QDateTime timestamp = QDateTime::fromString(
        QStringLiteral("2026-09-27T12:00:00.000Z"), Qt::ISODateWithMs);
    const vision3d::realtime::GaugeSample valid =
        vision3d::realtime::GaugeSample::create(QStringLiteral("gauge-1"), 1.2, timestamp);
    QVERIFY(valid.isValid());
    QCOMPARE(valid.source, vision3d::GaugeDataSource::Sensor);

    vision3d::realtime::GaugeSample missingId = valid;
    missingId.gaugeAssetId.clear();
    QVERIFY(!missingId.isValid());

    vision3d::realtime::GaugeSample nanValue = valid;
    nanValue.value = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!nanValue.isValid());

    vision3d::realtime::GaugeSample invalidTimestamp = valid;
    invalidTimestamp.timestamp = QDateTime();
    QVERIFY(!invalidTimestamp.isValid());
}

void RealtimeMonitoringTest::sourceStateStrings()
{
    QCOMPARE(vision3d::realtime::gaugeDataSourceStateToString(
                 vision3d::realtime::GaugeDataSourceState::Stopped),
             QStringLiteral("stopped"));
    QCOMPARE(vision3d::realtime::gaugeDataSourceStateDisplayName(
                 vision3d::realtime::GaugeDataSourceState::Running),
             QStringLiteral("Running"));
    QCOMPARE(vision3d::realtime::gaugeDataSourceStateDisplayName(
                 vision3d::realtime::GaugeDataSourceState::Error),
             QStringLiteral("Error"));
}

void RealtimeMonitoringTest::mockSourceStartStop()
{
    vision3d::realtime::MockSensorDataSource source(
        QStringLiteral("gauge-1"), QVector<double>{1.2, 2.1}, 10);
    QSignalSpy sampleSpy(&source, &vision3d::realtime::GaugeDataSource::sampleReceived);
    QSignalSpy stateSpy(&source, &vision3d::realtime::GaugeDataSource::stateChanged);
    QString error;
    QVERIFY2(source.start(&error), qPrintable(error));
    QVERIFY(source.isRunning());
    QCOMPARE(source.state(), vision3d::realtime::GaugeDataSourceState::Running);
    QVERIFY(sampleSpy.count() >= 1);
    source.stop();
    QVERIFY(!source.isRunning());
    QCOMPARE(source.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(stateSpy.count() >= 2);
}

void RealtimeMonitoringTest::deterministicSequence()
{
    vision3d::realtime::MockSensorDataSource source(
        QStringLiteral("gauge-1"), QVector<double>{1.2, 2.1, 2.4}, 5);
    QSignalSpy sampleSpy(&source, &vision3d::realtime::GaugeDataSource::sampleReceived);
    QString error;
    QVERIFY2(source.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(sampleSpy.count() >= 3, 500);
    QCOMPARE(qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(0).at(0)).value, 1.2);
    QCOMPARE(qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(1).at(0)).value, 2.1);
    QCOMPARE(qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(2).at(0)).value, 2.4);
    source.stop();
}

void RealtimeMonitoringTest::mockRejectsNonFiniteSequence()
{
    vision3d::realtime::MockSensorDataSource source(
        QStringLiteral("gauge-1"),
        QVector<double>{std::numeric_limits<double>::quiet_NaN()},
        5);
    QSignalSpy errorSpy(&source, &vision3d::realtime::GaugeDataSource::errorOccurred);
    QString error;
    QVERIFY(!source.start(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(source.state(), vision3d::realtime::GaugeDataSourceState::Error);
    QCOMPARE(errorSpy.count(), 1);
}

void RealtimeMonitoringTest::liveStateModel()
{
    vision3d::realtime::GaugeLiveStateModel model;
    const vision3d::realtime::GaugeSample first =
        vision3d::realtime::GaugeSample::create(QStringLiteral("gauge-1"), 1.2);
    const vision3d::realtime::GaugeSample second =
        vision3d::realtime::GaugeSample::create(QStringLiteral("gauge-1"), 2.1);
    QString error;
    QVERIFY(model.updateSample(first, &error));
    QVERIFY(model.updateSample(second, &error));
    QCOMPARE(model.size(), qsizetype(1));
    const std::optional<vision3d::realtime::GaugeLiveState> state =
        model.stateForGauge(QStringLiteral("gauge-1"));
    QVERIFY(state.has_value());
    QCOMPARE(state->value, 2.1);
    QCOMPARE(state->sequence, quint64(2));
    QVERIFY(state->connected);
    QVERIFY(state->available);
    model.clearGauge(QStringLiteral("gauge-1"));
    QVERIFY(!model.contains(QStringLiteral("gauge-1")));
    model.clearAll();
    QCOMPARE(model.size(), qsizetype(0));
}

void RealtimeMonitoringTest::controllerStartDoesNotRecord()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY2(controller.startMockSensor(fixture->gaugeId, QVector<double>{2.4}, 20, &error),
             qPrintable(error));
    QVERIFY(waitForLive(controller, fixture->gaugeId));
    QCOMPARE(manager.inspectionRecordsForGauge(fixture->gaugeId).size(), qsizetype(0));
    QVERIFY(!manager.gaugeAssetById(fixture->gaugeId)->latestValue.has_value());
    controller.stop();
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::controllerRecordsSensorSnapshot()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY2(controller.startMockSensor(fixture->gaugeId, QVector<double>{2.4}, 20, &error),
             qPrintable(error));
    QVERIFY(waitForLive(controller, fixture->gaugeId));
    QVERIFY2(controller.recordCurrentValue(fixture->gaugeId, &error), qPrintable(error));
    const QList<vision3d::InspectionRecord> records =
        manager.inspectionRecordsForGauge(fixture->gaugeId);
    QCOMPARE(records.size(), qsizetype(1));
    QCOMPARE(records.first().dataSource, vision3d::GaugeDataSource::Sensor);
    QVERIFY(records.first().imageAssetId.isEmpty());
    QVERIFY(records.first().sourceImagePath.isEmpty());
    QCOMPARE(manager.gaugeAssetById(fixture->gaugeId)->dataSource,
             vision3d::GaugeDataSource::Sensor);
    QCOMPARE(*manager.gaugeAssetById(fixture->gaugeId)->latestValue, 2.4);
    controller.stop();
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::sensorSnapshotReopens()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{2.4}, 20, &error));
    QVERIFY(waitForLive(controller, fixture->gaugeId));
    QVERIFY(controller.recordCurrentValue(fixture->gaugeId, &error));
    controller.stop();
    manager.closeProject();
    QVERIFY2(manager.openProject(fixture->projectPath, &error), qPrintable(error));
    QCOMPARE(manager.inspectionRecordsForGauge(fixture->gaugeId).size(), qsizetype(1));
    const std::optional<vision3d::GaugeAsset> restored = manager.gaugeAssetById(fixture->gaugeId);
    QVERIFY(restored.has_value());
    QCOMPARE(restored->dataSource, vision3d::GaugeDataSource::Sensor);
    QCOMPARE(*restored->latestValue, 2.4);
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::liveValueDrivesStatus()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    QVERIFY(setPersistedValue(manager, fixture->gaugeId, 1.2));
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{2.4}, 20, &error));
    QVERIFY(waitForLive(controller, fixture->gaugeId));
    const std::optional<vision3d::realtime::GaugeLiveState> live =
        controller.liveStateForGauge(fixture->gaugeId);
    QVERIFY(live.has_value());
    const vision3d::GaugeAsset gauge = *manager.gaugeAssetById(fixture->gaugeId);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(live->value, gauge.statusRule),
             vision3d::GaugeStatus::Alarm);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(gauge.latestValue, gauge.statusRule),
             vision3d::GaugeStatus::Normal);
    controller.stop();
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::persistedFallbackDrivesStatus()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    QVERIFY(setPersistedValue(manager, fixture->gaugeId, 2.4));
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QVERIFY(!controller.liveStateForGauge(fixture->gaugeId).has_value());
    const vision3d::GaugeAsset gauge = *manager.gaugeAssetById(fixture->gaugeId);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(gauge.latestValue, gauge.statusRule),
             vision3d::GaugeStatus::Alarm);
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::stopClearsLiveState()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{2.1}, 20, &error));
    QVERIFY(waitForLive(controller, fixture->gaugeId));
    controller.stop();
    QVERIFY(!controller.isRunning());
    QCOMPARE(controller.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(!controller.liveStateForGauge(fixture->gaugeId).has_value());
    QCOMPARE(controller.liveStateModel().size(), qsizetype(0));
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::projectSwitchStopsMonitoring()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> first = makeFixture(manager, QStringLiteral("ProjectA"));
    QVERIFY(first.has_value());
    vision3d::ProjectManager secondManager;
    const std::optional<RealtimeFixture> second =
        makeFixture(secondManager, QStringLiteral("ProjectB"));
    QVERIFY(second.has_value());
    secondManager.closeProject();

    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(first->gaugeId, QVector<double>{1.2}, 20, &error));
    QVERIFY(controller.isRunning());
    QVERIFY2(manager.openProject(second->projectPath, &error), qPrintable(error));
    QVERIFY(!controller.isRunning());
    QCOMPARE(controller.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(controller.activeGaugeAssetId().isEmpty());
    QCOMPARE(controller.liveStateModel().size(), qsizetype(0));
    removeFixture(manager, *first);
    removeFixture(secondManager, *second);
}

void RealtimeMonitoringTest::projectCloseStopsMonitoring()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{1.2}, 20, &error));
    QVERIFY(controller.isRunning());
    manager.closeProject();
    QVERIFY(!controller.isRunning());
    QVERIFY(controller.activeGaugeAssetId().isEmpty());
    QCOMPARE(controller.liveStateModel().size(), qsizetype(0));
    QDir(fixture->root).removeRecursively();
}

void RealtimeMonitoringTest::gaugeDeleteStopsMonitoring()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{1.2}, 20, &error));
    QVERIFY(controller.isRunning());
    QVERIFY2(manager.removeGaugeAsset(fixture->gaugeId, &error), qPrintable(error));
    QVERIFY(!controller.isRunning());
    QVERIFY(controller.activeGaugeAssetId().isEmpty());
    QVERIFY(controller.liveStateModel().size() == 0);
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::markerDeleteStopsMonitoring()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{1.2}, 20, &error));
    QVERIFY(controller.isRunning());
    QVERIFY2(manager.removeDeviceMarker(fixture->markerId, &error), qPrintable(error));
    QVERIFY(!controller.isRunning());
    QVERIFY(controller.activeGaugeAssetId().isEmpty());
    QVERIFY(!manager.gaugeAssetById(fixture->gaugeId).has_value());
    QCOMPARE(controller.liveStateModel().size(), qsizetype(0));
    removeFixture(manager, *fixture);
}

void RealtimeMonitoringTest::invalidSampleRejected()
{
    vision3d::ProjectManager manager;
    const std::optional<RealtimeFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QString error;
    QVERIFY(controller.startMockSensor(fixture->gaugeId, QVector<double>{1.2}, 20, &error));
    QVERIFY(waitForLive(controller, fixture->gaugeId));
    QSignalSpy errorSpy(&controller,
                        &vision3d::realtime::RealtimeMonitoringController::errorOccurred);
    vision3d::realtime::GaugeSample invalid =
        vision3d::realtime::GaugeSample::create(fixture->gaugeId, 1.2);
    invalid.value = std::numeric_limits<double>::infinity();
    QVERIFY(!controller.acceptSample(invalid, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(controller.liveStateForGauge(fixture->gaugeId).has_value());
    controller.stop();
    removeFixture(manager, *fixture);
}

QTEST_MAIN(RealtimeMonitoringTest)

#include "test_realtime_monitoring.moc"
