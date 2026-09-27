#pragma once

#include "core/device/ModbusTcpBinding.h"
#include "core/realtime/GaugeDataSource.h"

#include <QList>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QTimer>

#include <optional>

namespace vision3d::realtime {

class ModbusTcpGaugeDataSource final : public GaugeDataSource
{
    Q_OBJECT

public:
    explicit ModbusTcpGaugeDataSource(const ::vision3d::ModbusTcpBinding& binding,
                                      QObject* parent = nullptr);

    bool start(QString* error = nullptr) override;
    void stop() override;
    bool isRunning() const override;
    GaugeDataSourceState state() const override;

    const ::vision3d::ModbusTcpBinding& binding() const;

    static std::optional<double> decodeRegisters(
        const QList<quint16>& registers,
        ::vision3d::ModbusDataType dataType,
        ::vision3d::ModbusWordOrder wordOrder,
        double scale,
        double offset,
        QString* error = nullptr);

private slots:
    void onDeviceStateChanged(QModbusDevice::State state);
    void onDeviceError(QModbusDevice::Error error);
    void pollOnce();
    void onReplyFinished(QModbusReply* reply);

private:
    void setState(GaugeDataSourceState state);
    bool fail(const QString& message, QString* error = nullptr);
    QModbusDataUnit::RegisterType qtRegisterType() const;
    QString deviceErrorText(QModbusDevice::Error error) const;

    ::vision3d::ModbusTcpBinding m_binding;
    QModbusTcpClient m_client;
    QTimer m_pollTimer;
    QModbusReply* m_pendingReply = nullptr;
    GaugeDataSourceState m_state = GaugeDataSourceState::Stopped;
    bool m_started = false;
};

} // namespace vision3d::realtime
