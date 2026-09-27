#include "core/realtime/ModbusTcpGaugeDataSource.h"

#include <QDateTime>
#include <QModbusDataUnit>
#include <QModbusDevice>

#include <cmath>
#include <cstring>

namespace vision3d::realtime {
namespace {

QString errorOrFallback(const QString& error, const QString& fallback)
{
    const QString normalized = error.trimmed();
    return normalized.isEmpty() ? fallback : normalized;
}

bool is32BitType(::vision3d::ModbusDataType dataType)
{
    return dataType == ::vision3d::ModbusDataType::UInt32
        || dataType == ::vision3d::ModbusDataType::Int32
        || dataType == ::vision3d::ModbusDataType::Float32;
}

} // namespace

ModbusTcpGaugeDataSource::ModbusTcpGaugeDataSource(
    const ::vision3d::ModbusTcpBinding& binding,
    QObject* parent)
    : GaugeDataSource(parent)
    , m_binding(binding)
    , m_client(this)
{
    m_pollTimer.setSingleShot(false);
    connect(&m_client,
            &QModbusDevice::stateChanged,
            this,
            &ModbusTcpGaugeDataSource::onDeviceStateChanged);
    connect(&m_client,
            &QModbusDevice::errorOccurred,
            this,
            &ModbusTcpGaugeDataSource::onDeviceError);
    connect(&m_pollTimer,
            &QTimer::timeout,
            this,
            &ModbusTcpGaugeDataSource::pollOnce);
}

bool ModbusTcpGaugeDataSource::start(QString* error)
{
    if (isRunning() || m_started) {
        return true;
    }

    QString validationError;
    if (!m_binding.isValid(&validationError)) {
        return fail(validationError, error);
    }

    stop();
    m_client.setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                    m_binding.host.trimmed());
    m_client.setConnectionParameter(QModbusDevice::NetworkPortParameter,
                                    static_cast<int>(m_binding.port));
    m_client.setTimeout(qMax(1000, m_binding.pollIntervalMs * 2));
    m_client.setNumberOfRetries(0);
    m_started = true;
    setState(GaugeDataSourceState::Running);

    if (!m_client.connectDevice()) {
        const QString message = QStringLiteral("Modbus TCP 连接失败: %1")
                                    .arg(errorOrFallback(m_client.errorString(),
                                                         QStringLiteral("connectDevice 返回 false")));
        return fail(message, error);
    }
    if (m_client.state() == QModbusDevice::ConnectedState) {
        onDeviceStateChanged(QModbusDevice::ConnectedState);
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void ModbusTcpGaugeDataSource::stop()
{
    m_started = false;
    m_pollTimer.stop();
    if (m_pendingReply != nullptr) {
        disconnect(m_pendingReply, nullptr, this, nullptr);
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
    if (m_client.state() != QModbusDevice::UnconnectedState) {
        m_client.disconnectDevice();
    }
    setState(GaugeDataSourceState::Stopped);
}

bool ModbusTcpGaugeDataSource::isRunning() const
{
    return m_started && m_state == GaugeDataSourceState::Running;
}

GaugeDataSourceState ModbusTcpGaugeDataSource::state() const
{
    return m_state;
}

const ::vision3d::ModbusTcpBinding& ModbusTcpGaugeDataSource::binding() const
{
    return m_binding;
}

std::optional<double> ModbusTcpGaugeDataSource::decodeRegisters(
    const QList<quint16>& registers,
    ::vision3d::ModbusDataType dataType,
    ::vision3d::ModbusWordOrder wordOrder,
    double scale,
    double offset,
    QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<double> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    const int requiredRegisters = is32BitType(dataType) ? 2 : 1;
    if (registers.size() < requiredRegisters) {
        return fail(QStringLiteral("Modbus 返回寄存器数量不足。"));
    }
    if (!std::isfinite(scale) || !std::isfinite(offset)) {
        return fail(QStringLiteral("Modbus scale/offset 必须是有限数字。"));
    }

    double rawValue = 0.0;
    switch (dataType) {
    case ::vision3d::ModbusDataType::UInt16:
        rawValue = static_cast<double>(registers.at(0));
        break;
    case ::vision3d::ModbusDataType::Int16:
        rawValue = static_cast<double>(static_cast<qint16>(registers.at(0)));
        break;
    case ::vision3d::ModbusDataType::UInt32:
    case ::vision3d::ModbusDataType::Int32:
    case ::vision3d::ModbusDataType::Float32: {
        const quint32 first = static_cast<quint32>(registers.at(0));
        const quint32 second = static_cast<quint32>(registers.at(1));
        const quint32 combined = wordOrder == ::vision3d::ModbusWordOrder::ABCD
            ? (first << 16U) | second
            : (second << 16U) | first;
        if (dataType == ::vision3d::ModbusDataType::UInt32) {
            rawValue = static_cast<double>(combined);
        } else if (dataType == ::vision3d::ModbusDataType::Int32) {
            rawValue = static_cast<double>(static_cast<qint32>(combined));
        } else {
            float floatValue = 0.0F;
            static_assert(sizeof(float) == sizeof(quint32));
            std::memcpy(&floatValue, &combined, sizeof(floatValue));
            if (!std::isfinite(floatValue)) {
                return fail(QStringLiteral("Modbus Float32 原始值不是有限数字。"));
            }
            rawValue = static_cast<double>(floatValue);
        }
        break;
    }
    }

    const double engineeringValue = rawValue * scale + offset;
    if (!std::isfinite(engineeringValue)) {
        return fail(QStringLiteral("Modbus 转换后的读数不是有限数字。"));
    }
    if (error != nullptr) {
        error->clear();
    }
    return engineeringValue;
}

void ModbusTcpGaugeDataSource::onDeviceStateChanged(QModbusDevice::State state)
{
    if (!m_started) {
        return;
    }
    if (state == QModbusDevice::ConnectedState) {
        setState(GaugeDataSourceState::Running);
        if (!m_pollTimer.isActive()) {
            m_pollTimer.start(m_binding.pollIntervalMs);
        }
        pollOnce();
        return;
    }
    if (state == QModbusDevice::UnconnectedState
        && m_state != GaugeDataSourceState::Error) {
        fail(QStringLiteral("Modbus TCP 连接已断开。"));
    }
}

void ModbusTcpGaugeDataSource::onDeviceError(QModbusDevice::Error error)
{
    if (!m_started) {
        return;
    }
    fail(QStringLiteral("Modbus TCP 设备错误: %1").arg(deviceErrorText(error)));
}

void ModbusTcpGaugeDataSource::pollOnce()
{
    if (!isRunning() || m_client.state() != QModbusDevice::ConnectedState
        || m_pendingReply != nullptr) {
        return;
    }

    const QModbusDataUnit request(qtRegisterType(),
                                  m_binding.startAddress,
                                  static_cast<quint16>(m_binding.registerCount()));
    QModbusReply* reply = m_client.sendReadRequest(request, m_binding.serverAddress);
    if (reply == nullptr) {
        fail(QStringLiteral("Modbus TCP 无法创建读取请求: %1")
                 .arg(errorOrFallback(m_client.errorString(),
                                      QStringLiteral("sendReadRequest 返回 null"))));
        return;
    }

    m_pendingReply = reply;
    connect(reply,
            &QModbusReply::finished,
            this,
            [this, reply]() { onReplyFinished(reply); });
    if (reply->isFinished()) {
        onReplyFinished(reply);
    }
}

void ModbusTcpGaugeDataSource::onReplyFinished(QModbusReply* reply)
{
    if (reply == nullptr) {
        return;
    }
    if (reply != m_pendingReply) {
        reply->deleteLater();
        return;
    }
    m_pendingReply = nullptr;

    if (!m_started) {
        reply->deleteLater();
        return;
    }
    if (reply->error() != QModbusDevice::NoError) {
        const QString message = QStringLiteral("Modbus TCP 读取失败: %1")
                                    .arg(errorOrFallback(reply->errorString(),
                                                         QStringLiteral("未知读取错误")));
        reply->deleteLater();
        fail(message);
        return;
    }

    const QList<quint16> values = reply->result().values();
    QString decodeError;
    const std::optional<double> decoded = decodeRegisters(values,
                                                           m_binding.dataType,
                                                           m_binding.wordOrder,
                                                           m_binding.scale,
                                                           m_binding.offset,
                                                           &decodeError);
    reply->deleteLater();
    if (!decoded.has_value()) {
        fail(QStringLiteral("Modbus TCP 读数转换失败: %1").arg(decodeError));
        return;
    }

    const GaugeSample sample = GaugeSample::create(m_binding.gaugeAssetId,
                                                   *decoded,
                                                   QDateTime::currentDateTimeUtc());
    QString sampleError;
    if (!sample.isValid(&sampleError)) {
        fail(QStringLiteral("Modbus TCP 样本无效: %1").arg(sampleError));
        return;
    }
    emit sampleReceived(sample);
}

void ModbusTcpGaugeDataSource::setState(GaugeDataSourceState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(m_state);
}

bool ModbusTcpGaugeDataSource::fail(const QString& message, QString* error)
{
    if (error != nullptr) {
        *error = message;
    }
    m_started = false;
    m_pollTimer.stop();
    if (m_pendingReply != nullptr) {
        disconnect(m_pendingReply, nullptr, this, nullptr);
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
    if (m_client.state() != QModbusDevice::UnconnectedState) {
        m_client.disconnectDevice();
    }
    setState(GaugeDataSourceState::Error);
    emit errorOccurred(message);
    return false;
}

QModbusDataUnit::RegisterType ModbusTcpGaugeDataSource::qtRegisterType() const
{
    return m_binding.registerType == ::vision3d::ModbusRegisterType::InputRegisters
        ? QModbusDataUnit::InputRegisters
        : QModbusDataUnit::HoldingRegisters;
}

QString ModbusTcpGaugeDataSource::deviceErrorText(QModbusDevice::Error error) const
{
    const QString detail = m_client.errorString().trimmed();
    if (!detail.isEmpty()) {
        return detail;
    }
    switch (error) {
    case QModbusDevice::NoError:
        return QStringLiteral("NoError");
    case QModbusDevice::ReadError:
        return QStringLiteral("ReadError");
    case QModbusDevice::WriteError:
        return QStringLiteral("WriteError");
    case QModbusDevice::ConnectionError:
        return QStringLiteral("ConnectionError");
    case QModbusDevice::ConfigurationError:
        return QStringLiteral("ConfigurationError");
    case QModbusDevice::TimeoutError:
        return QStringLiteral("TimeoutError");
    case QModbusDevice::ProtocolError:
        return QStringLiteral("ProtocolError");
    case QModbusDevice::ReplyAbortedError:
        return QStringLiteral("ReplyAbortedError");
    case QModbusDevice::UnknownError:
        return QStringLiteral("UnknownError");
    case QModbusDevice::InvalidResponseError:
        return QStringLiteral("InvalidResponseError");
    }
    return QStringLiteral("UnknownError");
}

} // namespace vision3d::realtime
