#include "core/device/DeviceMarker.h"
#include "core/device/GaugeStatus.h"
#include "core/project/ProjectManager.h"
#include "core/realtime/ModbusTcpGaugeDataSource.h"
#include "core/realtime/RealtimeMonitoringController.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QModbusDataUnit>
#include <QModbusDevice>
#include <QModbusTcpServer>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

#include <cmath>
#include <optional>

namespace {

constexpr quint16 kLoopbackPort = 15020;

struct ProjectFixture
{
    QString root;
    QString projectPath;
    QString markerId;
    QString gaugeId;
};

vision3d::GaugeStatusRule stage4hRule()
{
    vision3d::GaugeStatusRule rule;
    rule.warningHigh = 2.0;
    rule.alarmHigh = 2.3;
    return rule;
}

std::optional<ProjectFixture> makeFixture(vision3d::ProjectManager& manager,
                                          const QString& projectName = QStringLiteral("Project"))
{
    ProjectFixture fixture;
    fixture.root = QDir(QDir::tempPath()).filePath(
        QStringLiteral("stage4h_modbus_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(fixture.root)) {
        return std::nullopt;
    }

    QString error;
    if (!manager.newProject(fixture.root, projectName, &error)) {
        QDir(fixture.root).removeRecursively();
        return std::nullopt;
    }
    fixture.projectPath = QDir(fixture.root).filePath(
        projectName + QStringLiteral("/project.json"));

    vision3d::DeviceMarker marker;
    if (!manager.addDeviceMarker(QStringLiteral("P01"),
                                 QVector3D(0.0F, 0.0F, 0.0F),
                                 QStringLiteral("stage4h-task"),
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

void removeFixture(vision3d::ProjectManager& manager, const ProjectFixture& fixture)
{
    manager.closeProject();
    QDir(fixture.root).removeRecursively();
}

vision3d::ModbusTcpBinding bindingFor(const QString& gaugeId,
                                      vision3d::ModbusRegisterType registerType =
                                          vision3d::ModbusRegisterType::HoldingRegisters,
                                      vision3d::ModbusDataType dataType =
                                          vision3d::ModbusDataType::UInt16)
{
    vision3d::ModbusTcpBinding binding;
    binding.gaugeAssetId = gaugeId;
    binding.host = QStringLiteral("127.0.0.1");
    binding.port = kLoopbackPort;
    binding.serverAddress = 1;
    binding.registerType = registerType;
    binding.startAddress = 0;
    binding.dataType = dataType;
    binding.wordOrder = vision3d::ModbusWordOrder::ABCD;
    binding.scale = 0.01;
    binding.offset = 0.0;
    binding.pollIntervalMs = 100;
    return binding;
}

bool persistBinding(vision3d::ProjectManager& manager,
                    const QString& gaugeId,
                    const vision3d::ModbusTcpBinding& binding,
                    QString* error = nullptr)
{
    const std::optional<vision3d::GaugeAsset> existing = manager.gaugeAssetById(gaugeId);
    if (!existing.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("gauge fixture missing");
        }
        return false;
    }
    vision3d::GaugeAsset updated = *existing;
    updated.modbusTcpBinding = binding;
    updated.statusRule = stage4hRule();
    return manager.updateGaugeAsset(updated, error);
}

class LoopbackModbusServer final
{
public:
    bool start(QString* error = nullptr)
    {
        QModbusDataUnitMap map;
        map.insert(QModbusDataUnit::HoldingRegisters,
                   QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, 8));
        map.insert(QModbusDataUnit::InputRegisters,
                   QModbusDataUnit(QModbusDataUnit::InputRegisters, 0, 8));
        if (!m_server.setMap(map)) {
            if (error != nullptr) {
                *error = QStringLiteral("QModbusTcpServer::setMap failed");
            }
            return false;
        }
        m_server.setServerAddress(1);
        m_server.setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                        QStringLiteral("127.0.0.1"));
        m_server.setConnectionParameter(QModbusDevice::NetworkPortParameter,
                                        static_cast<int>(kLoopbackPort));
        if (!m_server.connectDevice()) {
            if (error != nullptr) {
                *error = QStringLiteral("QModbusTcpServer::connectDevice failed: %1")
                             .arg(m_server.errorString());
            }
            return false;
        }
        if (!waitForState(QModbusDevice::ConnectedState, 1000)) {
            if (error != nullptr) {
                *error = QStringLiteral("QModbusTcpServer did not reach ConnectedState: %1")
                             .arg(m_server.errorString());
            }
            return false;
        }
        return true;
    }

    bool setRegisters(QModbusDataUnit::RegisterType type,
                      const QList<quint16>& values)
    {
        return m_server.setData(QModbusDataUnit(type, 0, values));
    }

    void stop()
    {
        if (m_server.state() != QModbusDevice::UnconnectedState) {
            m_server.disconnectDevice();
        }
    }

private:
    bool waitForState(QModbusDevice::State expected, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (m_server.state() == expected) {
                return true;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        return m_server.state() == expected;
    }

    QModbusTcpServer m_server;
};

bool waitForSample(QSignalSpy& spy, int expectedCount, int timeoutMs = 2000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (spy.count() >= expectedCount) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return spy.count() >= expectedCount;
}

bool waitForLiveValue(vision3d::realtime::RealtimeMonitoringController& controller,
                      const QString& gaugeId,
                      double expected,
                      int timeoutMs = 2000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const std::optional<vision3d::realtime::GaugeLiveState> live =
            controller.liveStateForGauge(gaugeId);
        if (live.has_value() && std::abs(live->value - expected) < 0.0001) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    const std::optional<vision3d::realtime::GaugeLiveState> live =
        controller.liveStateForGauge(gaugeId);
    return live.has_value() && std::abs(live->value - expected) < 0.0001;
}

} // namespace

class ModbusTcpDataSourceTest final : public QObject
{
    Q_OBJECT

private slots:
    void bindingValidationAndJson();
    void decodeSignedAndFloat32();
    void loopbackHoldingRegisters();
    void loopbackInputRegisters();
    void connectionFailure();
    void controllerStatusSequenceAndSnapshot();
    void persistenceRestoresBindingWithoutStarting();
    void lifecycleDisconnectsAndClearsLiveState();
};

void ModbusTcpDataSourceTest::bindingValidationAndJson()
{
    vision3d::ModbusTcpBinding binding = bindingFor(QStringLiteral("gauge-1"));
    QString error;
    QVERIFY2(binding.isValid(&error), qPrintable(error));

    const QJsonObject json = binding.toJson();
    const std::optional<vision3d::ModbusTcpBinding> parsed =
        vision3d::ModbusTcpBinding::fromJson(json, &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->gaugeAssetId, binding.gaugeAssetId);
    QCOMPARE(parsed->host, binding.host);
    QCOMPARE(parsed->port, binding.port);
    QCOMPARE(parsed->serverAddress, binding.serverAddress);
    QCOMPARE(parsed->registerType, binding.registerType);
    QCOMPARE(parsed->dataType, binding.dataType);
    QCOMPARE(parsed->wordOrder, binding.wordOrder);
    QCOMPARE(parsed->pollIntervalMs, binding.pollIntervalMs);

    vision3d::GaugeAsset asset = vision3d::GaugeAsset::create(
        QStringLiteral("marker-1"), QStringLiteral("Gauge"), 0.0, 2.5, QStringLiteral("MPa"));
    asset.id = binding.gaugeAssetId;
    asset.modbusTcpBinding = binding;
    const std::optional<vision3d::GaugeAsset> parsedAsset =
        vision3d::GaugeAsset::fromJson(asset.toJson(), &error);
    QVERIFY2(parsedAsset.has_value(), qPrintable(error));
    QVERIFY(parsedAsset->modbusTcpBinding.has_value());
    QCOMPARE(parsedAsset->modbusTcpBinding->gaugeAssetId, QStringLiteral("gauge-1"));
}

void ModbusTcpDataSourceTest::decodeSignedAndFloat32()
{
    QString error;
    const std::optional<double> int16Value =
        vision3d::realtime::ModbusTcpGaugeDataSource::decodeRegisters(
            {0xFFFE},
            vision3d::ModbusDataType::Int16,
            vision3d::ModbusWordOrder::ABCD,
            1.0,
            0.0,
            &error);
    QVERIFY2(int16Value.has_value(), qPrintable(error));
    QVERIFY(std::abs(*int16Value + 2.0) < 0.0001);

    const std::optional<double> int32Value =
        vision3d::realtime::ModbusTcpGaugeDataSource::decodeRegisters(
            {0xFFFF, 0xFFFE},
            vision3d::ModbusDataType::Int32,
            vision3d::ModbusWordOrder::ABCD,
            1.0,
            0.0,
            &error);
    QVERIFY2(int32Value.has_value(), qPrintable(error));
    QVERIFY(std::abs(*int32Value + 2.0) < 0.0001);

    const std::optional<double> floatAbcd =
        vision3d::realtime::ModbusTcpGaugeDataSource::decodeRegisters(
            {0x3F80, 0x0000},
            vision3d::ModbusDataType::Float32,
            vision3d::ModbusWordOrder::ABCD,
            1.0,
            0.0,
            &error);
    QVERIFY2(floatAbcd.has_value(), qPrintable(error));
    QVERIFY(std::abs(*floatAbcd - 1.0) < 0.0001);

    const std::optional<double> floatCdab =
        vision3d::realtime::ModbusTcpGaugeDataSource::decodeRegisters(
            {0x0000, 0x3F80},
            vision3d::ModbusDataType::Float32,
            vision3d::ModbusWordOrder::CDAB,
            2.0,
            0.5,
            &error);
    QVERIFY2(floatCdab.has_value(), qPrintable(error));
    QVERIFY(std::abs(*floatCdab - 2.5) < 0.0001);
}

void ModbusTcpDataSourceTest::loopbackHoldingRegisters()
{
    LoopbackModbusServer server;
    QString serverError;
    QVERIFY2(server.start(&serverError), qPrintable(serverError));
    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {120}));

    vision3d::ModbusTcpBinding binding = bindingFor(QStringLiteral("gauge-1"));
    vision3d::realtime::ModbusTcpGaugeDataSource source(binding);
    QSignalSpy sampleSpy(&source, &vision3d::realtime::GaugeDataSource::sampleReceived);
    QSignalSpy errorSpy(&source, &vision3d::realtime::GaugeDataSource::errorOccurred);
    QString error;
    QVERIFY2(source.start(&error), qPrintable(error));
    QVERIFY(waitForSample(sampleSpy, 1));
    const vision3d::realtime::GaugeSample first =
        qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(0).at(0));
    QVERIFY(std::abs(first.value - 1.20) < 0.0001);
    QCOMPARE(first.source, vision3d::GaugeDataSource::Sensor);

    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {210}));
    QVERIFY(waitForSample(sampleSpy, 2));
    const vision3d::realtime::GaugeSample second =
        qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(1).at(0));
    QVERIFY(std::abs(second.value - 2.10) < 0.0001);
    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {240}));
    QVERIFY(waitForSample(sampleSpy, 3));
    const vision3d::realtime::GaugeSample third =
        qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(2).at(0));
    QVERIFY(std::abs(third.value - 2.40) < 0.0001);
    QCOMPARE(errorSpy.count(), 0);
    QVERIFY(source.isRunning());
    source.stop();
    QCOMPARE(source.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(!source.isRunning());
    server.stop();
}

void ModbusTcpDataSourceTest::loopbackInputRegisters()
{
    LoopbackModbusServer server;
    QString serverError;
    QVERIFY2(server.start(&serverError), qPrintable(serverError));
    QVERIFY(server.setRegisters(QModbusDataUnit::InputRegisters, {321}));

    const vision3d::ModbusTcpBinding binding = bindingFor(
        QStringLiteral("gauge-input"),
        vision3d::ModbusRegisterType::InputRegisters);
    vision3d::realtime::ModbusTcpGaugeDataSource source(binding);
    QSignalSpy sampleSpy(&source, &vision3d::realtime::GaugeDataSource::sampleReceived);
    QString error;
    QVERIFY2(source.start(&error), qPrintable(error));
    QVERIFY(waitForSample(sampleSpy, 1));
    const vision3d::realtime::GaugeSample sample =
        qvariant_cast<vision3d::realtime::GaugeSample>(sampleSpy.at(0).at(0));
    QVERIFY(std::abs(sample.value - 3.21) < 0.0001);
    source.stop();
    server.stop();
}

void ModbusTcpDataSourceTest::connectionFailure()
{
    vision3d::ModbusTcpBinding binding = bindingFor(QStringLiteral("gauge-failure"));
    binding.port = 15021;
    vision3d::realtime::ModbusTcpGaugeDataSource source(binding);
    QSignalSpy errorSpy(&source, &vision3d::realtime::GaugeDataSource::errorOccurred);
    QString error;
    QVERIFY2(source.start(&error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(source.state(),
                              vision3d::realtime::GaugeDataSourceState::Error,
                              5000);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(!source.isRunning());
    source.stop();
}

void ModbusTcpDataSourceTest::controllerStatusSequenceAndSnapshot()
{
    vision3d::ProjectManager manager;
    const std::optional<ProjectFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());

    vision3d::ModbusTcpBinding binding = bindingFor(fixture->gaugeId);
    QString error;
    QVERIFY2(persistBinding(manager, fixture->gaugeId, binding, &error), qPrintable(error));

    LoopbackModbusServer server;
    QVERIFY2(server.start(&error), qPrintable(error));
    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {120}));

    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QVERIFY2(controller.startModbusTcp(binding, &error), qPrintable(error));
    QVERIFY(waitForLiveValue(controller, fixture->gaugeId, 1.20));
    const std::optional<vision3d::GaugeAsset> gauge = manager.gaugeAssetById(fixture->gaugeId);
    QVERIFY(gauge.has_value());
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(1.20, gauge->statusRule),
             vision3d::GaugeStatus::Normal);

    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {210}));
    QVERIFY(waitForLiveValue(controller, fixture->gaugeId, 2.10));
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.10, gauge->statusRule),
             vision3d::GaugeStatus::Warning);

    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {240}));
    QVERIFY(waitForLiveValue(controller, fixture->gaugeId, 2.40));
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.40, gauge->statusRule),
             vision3d::GaugeStatus::Alarm);
    QVERIFY(controller.liveStateForGauge(fixture->gaugeId).has_value());
    QCOMPARE(manager.inspectionRecordsForGauge(fixture->gaugeId).size(), qsizetype(0));

    QVERIFY2(controller.recordCurrentValue(fixture->gaugeId, &error), qPrintable(error));
    QCOMPARE(manager.inspectionRecordsForGauge(fixture->gaugeId).size(), qsizetype(1));
    QCOMPARE(manager.inspectionRecordsForGauge(fixture->gaugeId).first().dataSource,
             vision3d::GaugeDataSource::Sensor);
    QVERIFY(waitForLiveValue(controller, fixture->gaugeId, 2.40));
    QCOMPARE(manager.inspectionRecordsForGauge(fixture->gaugeId).size(), qsizetype(1));

    controller.stop();
    QVERIFY(!controller.liveStateForGauge(fixture->gaugeId).has_value());
    QCOMPARE(controller.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    server.stop();
    removeFixture(manager, *fixture);
}

void ModbusTcpDataSourceTest::persistenceRestoresBindingWithoutStarting()
{
    vision3d::ProjectManager manager;
    const std::optional<ProjectFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    const vision3d::ModbusTcpBinding binding = bindingFor(fixture->gaugeId);
    QString error;
    QVERIFY2(persistBinding(manager, fixture->gaugeId, binding, &error), qPrintable(error));
    QVERIFY2(manager.recordGaugeReading(fixture->gaugeId,
                                        1.20,
                                        QDateTime::currentDateTimeUtc(),
                                        vision3d::GaugeDataSource::Sensor,
                                        &error),
             qPrintable(error));

    manager.closeProject();
    QVERIFY2(manager.openProject(fixture->projectPath, &error), qPrintable(error));
    const std::optional<vision3d::GaugeAsset> reopened = manager.gaugeAssetById(fixture->gaugeId);
    QVERIFY(reopened.has_value());
    QVERIFY(reopened->modbusTcpBinding.has_value());
    QCOMPARE(reopened->modbusTcpBinding->host, QStringLiteral("127.0.0.1"));
    QCOMPARE(reopened->modbusTcpBinding->port, kLoopbackPort);
    QCOMPARE(reopened->modbusTcpBinding->pollIntervalMs, 100);
    QVERIFY(reopened->latestValue.has_value());
    QVERIFY(std::abs(*reopened->latestValue - 1.20) < 0.0001);

    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QCOMPARE(controller.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(!controller.liveStateForGauge(fixture->gaugeId).has_value());
    removeFixture(manager, *fixture);
}

void ModbusTcpDataSourceTest::lifecycleDisconnectsAndClearsLiveState()
{
    vision3d::ProjectManager manager;
    const std::optional<ProjectFixture> fixture = makeFixture(manager);
    QVERIFY(fixture.has_value());
    const vision3d::ModbusTcpBinding binding = bindingFor(fixture->gaugeId);
    QString error;
    QVERIFY2(persistBinding(manager, fixture->gaugeId, binding, &error), qPrintable(error));

    LoopbackModbusServer server;
    QVERIFY2(server.start(&error), qPrintable(error));
    QVERIFY(server.setRegisters(QModbusDataUnit::HoldingRegisters, {120}));
    vision3d::realtime::RealtimeMonitoringController controller(&manager);
    QVERIFY2(controller.startModbusTcp(binding, &error), qPrintable(error));
    QVERIFY(waitForLiveValue(controller, fixture->gaugeId, 1.20));

    manager.closeProject();
    QCOMPARE(controller.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(controller.activeGaugeAssetId().isEmpty());
    QVERIFY(!controller.liveStateForGauge(fixture->gaugeId).has_value());

    QVERIFY2(manager.openProject(fixture->projectPath, &error), qPrintable(error));
    const std::optional<vision3d::GaugeAsset> reopened = manager.gaugeAssetById(fixture->gaugeId);
    QVERIFY(reopened.has_value());
    QVERIFY(reopened->modbusTcpBinding.has_value());

    QVERIFY2(controller.startModbusTcp(*reopened->modbusTcpBinding, &error), qPrintable(error));
    QVERIFY(waitForLiveValue(controller, fixture->gaugeId, 1.20));
    QVERIFY2(manager.removeGaugeAsset(fixture->gaugeId, &error), qPrintable(error));
    QCOMPARE(controller.state(), vision3d::realtime::GaugeDataSourceState::Stopped);
    QVERIFY(controller.activeGaugeAssetId().isEmpty());
    QVERIFY(!controller.liveStateForGauge(fixture->gaugeId).has_value());
    server.stop();
    removeFixture(manager, *fixture);
}

QTEST_MAIN(ModbusTcpDataSourceTest)
#include "test_modbus_tcp_data_source.moc"
