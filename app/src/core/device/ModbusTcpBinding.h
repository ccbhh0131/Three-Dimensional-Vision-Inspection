#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <optional>

namespace vision3d {

enum class ModbusRegisterType
{
    HoldingRegisters,
    InputRegisters,
};

enum class ModbusDataType
{
    UInt16,
    Int16,
    UInt32,
    Int32,
    Float32,
};

enum class ModbusWordOrder
{
    ABCD,
    CDAB,
};

inline QString modbusRegisterTypeToString(ModbusRegisterType type)
{
    switch (type) {
    case ModbusRegisterType::HoldingRegisters:
        return QStringLiteral("HoldingRegisters");
    case ModbusRegisterType::InputRegisters:
        return QStringLiteral("InputRegisters");
    }
    return QStringLiteral("HoldingRegisters");
}

inline std::optional<ModbusRegisterType> modbusRegisterTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("holdingregisters")
        || normalized == QStringLiteral("holding_registers")) {
        return ModbusRegisterType::HoldingRegisters;
    }
    if (normalized == QStringLiteral("inputregisters")
        || normalized == QStringLiteral("input_registers")) {
        return ModbusRegisterType::InputRegisters;
    }
    return std::nullopt;
}

inline QString modbusDataTypeToString(ModbusDataType type)
{
    switch (type) {
    case ModbusDataType::UInt16:
        return QStringLiteral("UInt16");
    case ModbusDataType::Int16:
        return QStringLiteral("Int16");
    case ModbusDataType::UInt32:
        return QStringLiteral("UInt32");
    case ModbusDataType::Int32:
        return QStringLiteral("Int32");
    case ModbusDataType::Float32:
        return QStringLiteral("Float32");
    }
    return QStringLiteral("UInt16");
}

inline std::optional<ModbusDataType> modbusDataTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("uint16")) {
        return ModbusDataType::UInt16;
    }
    if (normalized == QStringLiteral("int16")) {
        return ModbusDataType::Int16;
    }
    if (normalized == QStringLiteral("uint32")) {
        return ModbusDataType::UInt32;
    }
    if (normalized == QStringLiteral("int32")) {
        return ModbusDataType::Int32;
    }
    if (normalized == QStringLiteral("float32")) {
        return ModbusDataType::Float32;
    }
    return std::nullopt;
}

inline QString modbusWordOrderToString(ModbusWordOrder order)
{
    switch (order) {
    case ModbusWordOrder::ABCD:
        return QStringLiteral("ABCD");
    case ModbusWordOrder::CDAB:
        return QStringLiteral("CDAB");
    }
    return QStringLiteral("ABCD");
}

inline std::optional<ModbusWordOrder> modbusWordOrderFromString(const QString& value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized == QStringLiteral("ABCD")) {
        return ModbusWordOrder::ABCD;
    }
    if (normalized == QStringLiteral("CDAB")) {
        return ModbusWordOrder::CDAB;
    }
    return std::nullopt;
}

struct ModbusTcpBinding
{
    QString gaugeAssetId;
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 502;
    int serverAddress = 1;
    ModbusRegisterType registerType = ModbusRegisterType::HoldingRegisters;
    int startAddress = 0;
    ModbusDataType dataType = ModbusDataType::UInt16;
    ModbusWordOrder wordOrder = ModbusWordOrder::ABCD;
    double scale = 1.0;
    double offset = 0.0;
    int pollIntervalMs = 500;

    int registerCount() const
    {
        switch (dataType) {
        case ModbusDataType::UInt16:
        case ModbusDataType::Int16:
            return 1;
        case ModbusDataType::UInt32:
        case ModbusDataType::Int32:
        case ModbusDataType::Float32:
            return 2;
        }
        return 0;
    }

    bool isValid(QString* error = nullptr) const
    {
        const auto fail = [error](const QString& message) {
            if (error != nullptr) {
                *error = message;
            }
            return false;
        };

        if (gaugeAssetId.trimmed().isEmpty()) {
            return fail(QStringLiteral("ModbusTcpBinding gaugeAssetId 不能为空。"));
        }
        if (host.trimmed().isEmpty()) {
            return fail(QStringLiteral("ModbusTcpBinding host 不能为空。"));
        }
        if (port == 0) {
            return fail(QStringLiteral("ModbusTcpBinding port 必须位于 1-65535。"));
        }
        if (serverAddress < 1 || serverAddress > 247) {
            return fail(QStringLiteral("ModbusTcpBinding serverAddress 必须位于 1-247。"));
        }
        if (startAddress < 0 || startAddress > 65535
            || startAddress + registerCount() > 65536) {
            return fail(QStringLiteral("ModbusTcpBinding startAddress 超出寄存器范围。"));
        }
        if (registerCount() == 0) {
            return fail(QStringLiteral("ModbusTcpBinding dataType 无效。"));
        }
        if (!std::isfinite(scale) || !std::isfinite(offset)) {
            return fail(QStringLiteral("ModbusTcpBinding scale/offset 必须是有限数字。"));
        }
        if (pollIntervalMs < 100 || pollIntervalMs > 60000) {
            return fail(QStringLiteral("ModbusTcpBinding pollIntervalMs 必须位于 100-60000。"));
        }
        return true;
    }

    QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("gaugeAssetId"), gaugeAssetId},
            {QStringLiteral("host"), host},
            {QStringLiteral("port"), static_cast<int>(port)},
            {QStringLiteral("serverAddress"), serverAddress},
            {QStringLiteral("registerType"), modbusRegisterTypeToString(registerType)},
            {QStringLiteral("startAddress"), startAddress},
            {QStringLiteral("dataType"), modbusDataTypeToString(dataType)},
            {QStringLiteral("wordOrder"), modbusWordOrderToString(wordOrder)},
            {QStringLiteral("scale"), scale},
            {QStringLiteral("offset"), offset},
            {QStringLiteral("pollIntervalMs"), pollIntervalMs},
        };
    }

    static std::optional<ModbusTcpBinding> fromJson(const QJsonObject& json,
                                                    QString* error = nullptr)
    {
        const auto fail = [error](const QString& message)
            -> std::optional<ModbusTcpBinding> {
            if (error != nullptr) {
                *error = message;
            }
            return std::nullopt;
        };
        const auto readText = [&json, &fail](const QString& field)
            -> std::optional<QString> {
            const QJsonValue value = json.value(field);
            if (!value.isString() || value.toString().trimmed().isEmpty()) {
                fail(QStringLiteral("ModbusTcpBinding %1 必须是非空字符串。").arg(field));
                return std::nullopt;
            }
            return value.toString().trimmed();
        };
        const auto readInteger = [&json, &fail](const QString& field)
            -> std::optional<int> {
            const QJsonValue value = json.value(field);
            if (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() != std::floor(value.toDouble())
                || value.toDouble() < std::numeric_limits<int>::min()
                || value.toDouble() > std::numeric_limits<int>::max()) {
                fail(QStringLiteral("ModbusTcpBinding %1 必须是整数。").arg(field));
                return std::nullopt;
            }
            return static_cast<int>(value.toDouble());
        };
        const auto readNumber = [&json, &fail](const QString& field)
            -> std::optional<double> {
            const QJsonValue value = json.value(field);
            if (!value.isDouble() || !std::isfinite(value.toDouble())) {
                fail(QStringLiteral("ModbusTcpBinding %1 必须是有限数字。").arg(field));
                return std::nullopt;
            }
            return value.toDouble();
        };

        const std::optional<QString> gaugeAssetId = readText(QStringLiteral("gaugeAssetId"));
        const std::optional<QString> host = readText(QStringLiteral("host"));
        const std::optional<int> port = readInteger(QStringLiteral("port"));
        const std::optional<int> serverAddress = readInteger(QStringLiteral("serverAddress"));
        const std::optional<QString> registerType = readText(QStringLiteral("registerType"));
        const std::optional<int> startAddress = readInteger(QStringLiteral("startAddress"));
        const std::optional<QString> dataType = readText(QStringLiteral("dataType"));
        const std::optional<QString> wordOrder = readText(QStringLiteral("wordOrder"));
        const std::optional<double> scale = readNumber(QStringLiteral("scale"));
        const std::optional<double> offset = readNumber(QStringLiteral("offset"));
        const std::optional<int> pollIntervalMs = readInteger(QStringLiteral("pollIntervalMs"));
        if (!gaugeAssetId.has_value() || !host.has_value() || !port.has_value()
            || !serverAddress.has_value() || !registerType.has_value()
            || !startAddress.has_value() || !dataType.has_value() || !wordOrder.has_value()
            || !scale.has_value() || !offset.has_value() || !pollIntervalMs.has_value()) {
            return std::nullopt;
        }
        if (*port < 1 || *port > 65535) {
            return fail(QStringLiteral("ModbusTcpBinding port 必须位于 1-65535。"));
        }

        const std::optional<ModbusRegisterType> parsedRegisterType =
            modbusRegisterTypeFromString(*registerType);
        const std::optional<ModbusDataType> parsedDataType = modbusDataTypeFromString(*dataType);
        const std::optional<ModbusWordOrder> parsedWordOrder = modbusWordOrderFromString(*wordOrder);
        if (!parsedRegisterType.has_value() || !parsedDataType.has_value()
            || !parsedWordOrder.has_value()) {
            return fail(QStringLiteral("ModbusTcpBinding registerType/dataType/wordOrder 无效。"));
        }

        ModbusTcpBinding binding;
        binding.gaugeAssetId = *gaugeAssetId;
        binding.host = *host;
        binding.port = static_cast<quint16>(*port);
        binding.serverAddress = *serverAddress;
        binding.registerType = *parsedRegisterType;
        binding.startAddress = *startAddress;
        binding.dataType = *parsedDataType;
        binding.wordOrder = *parsedWordOrder;
        binding.scale = *scale;
        binding.offset = *offset;
        binding.pollIntervalMs = *pollIntervalMs;
        QString validationError;
        if (!binding.isValid(&validationError)) {
            return fail(validationError);
        }
        return binding;
    }
};

} // namespace vision3d
