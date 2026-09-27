#include "core/realtime/GaugeDataSource.h"

#include <QDateTime>

#include <cmath>

namespace vision3d::realtime {

QString gaugeDataSourceStateToString(GaugeDataSourceState state)
{
    switch (state) {
    case GaugeDataSourceState::Stopped:
        return QStringLiteral("stopped");
    case GaugeDataSourceState::Running:
        return QStringLiteral("running");
    case GaugeDataSourceState::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("stopped");
}

QString gaugeDataSourceStateDisplayName(GaugeDataSourceState state)
{
    switch (state) {
    case GaugeDataSourceState::Stopped:
        return QStringLiteral("Stopped");
    case GaugeDataSourceState::Running:
        return QStringLiteral("Running");
    case GaugeDataSourceState::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Stopped");
}

bool GaugeSample::isValid(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (gaugeAssetId.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeSample gaugeAssetId 不能为空。"));
    }
    if (!std::isfinite(value)) {
        return fail(QStringLiteral("GaugeSample value 必须是有限数字。"));
    }
    if (!timestamp.isValid()) {
        return fail(QStringLiteral("GaugeSample timestamp 必须是有效时间。"));
    }
    if (source == ::vision3d::GaugeDataSource::None) {
        return fail(QStringLiteral("GaugeSample source 不能为 None。"));
    }
    return true;
}

GaugeSample GaugeSample::create(const QString& gaugeAssetId,
                                double value,
                                const QDateTime& timestamp)
{
    GaugeSample sample;
    sample.gaugeAssetId = gaugeAssetId.trimmed();
    sample.value = value;
    sample.timestamp = timestamp.toUTC();
    sample.source = ::vision3d::GaugeDataSource::Sensor;
    return sample;
}

GaugeDataSource::GaugeDataSource(QObject* parent)
    : QObject(parent)
{
}

MockSensorDataSource::MockSensorDataSource(const QString& gaugeAssetId,
                                           const QVector<double>& sequence,
                                           int intervalMs,
                                           QObject* parent)
    : GaugeDataSource(parent)
    , m_gaugeAssetId(gaugeAssetId.trimmed())
    , m_sequence(sequence)
    , m_intervalMs(intervalMs)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &MockSensorDataSource::emitNextSample);
}

bool MockSensorDataSource::start(QString* error)
{
    if (isRunning()) {
        return true;
    }
    if (m_gaugeAssetId.isEmpty()) {
        return fail(QStringLiteral("MockSensorDataSource gaugeAssetId 不能为空。"), error);
    }
    if (m_sequence.isEmpty()) {
        return fail(QStringLiteral("MockSensorDataSource sequence 不能为空。"), error);
    }
    if (m_intervalMs <= 0) {
        return fail(QStringLiteral("MockSensorDataSource interval 必须大于 0。"), error);
    }
    for (const double value : m_sequence) {
        if (!std::isfinite(value)) {
            return fail(QStringLiteral("MockSensorDataSource sequence 包含非有限读数。"), error);
        }
    }

    if (error != nullptr) {
        error->clear();
    }
    m_sequenceIndex = 0;
    m_emittedCount = 0;
    setState(GaugeDataSourceState::Running);
    m_timer.start(m_intervalMs);
    emitNextSample();
    return true;
}

void MockSensorDataSource::stop()
{
    m_timer.stop();
    m_sequenceIndex = 0;
    if (m_state != GaugeDataSourceState::Stopped) {
        setState(GaugeDataSourceState::Stopped);
    }
}

bool MockSensorDataSource::isRunning() const
{
    return m_state == GaugeDataSourceState::Running && m_timer.isActive();
}

GaugeDataSourceState MockSensorDataSource::state() const
{
    return m_state;
}

QString MockSensorDataSource::gaugeAssetId() const
{
    return m_gaugeAssetId;
}

QVector<double> MockSensorDataSource::sequence() const
{
    return m_sequence;
}

int MockSensorDataSource::intervalMs() const
{
    return m_intervalMs;
}

void MockSensorDataSource::setSequence(const QVector<double>& sequence)
{
    if (isRunning()) {
        return;
    }
    m_sequence = sequence;
}

void MockSensorDataSource::setIntervalMs(int intervalMs)
{
    if (isRunning()) {
        return;
    }
    m_intervalMs = intervalMs;
}

QVector<double> MockSensorDataSource::defaultSequence()
{
    return {1.20, 1.50, 1.90, 2.10, 2.25, 2.40, 2.45, 2.10, 1.80};
}

void MockSensorDataSource::emitNextSample()
{
    if (!isRunning() || m_sequence.isEmpty()) {
        return;
    }

    const double value = m_sequence.at(m_sequenceIndex);
    m_sequenceIndex = (m_sequenceIndex + 1) % m_sequence.size();
    const GaugeSample sample = GaugeSample::create(
        m_gaugeAssetId, value, QDateTime::currentDateTimeUtc());
    QString error;
    if (!sample.isValid(&error)) {
        m_timer.stop();
        setState(GaugeDataSourceState::Error);
        emit errorOccurred(error);
        return;
    }
    ++m_emittedCount;
    emit sampleReceived(sample);
}

void MockSensorDataSource::setState(GaugeDataSourceState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(m_state);
}

bool MockSensorDataSource::fail(const QString& message, QString* error)
{
    if (error != nullptr) {
        *error = message;
    }
    m_timer.stop();
    if (m_state != GaugeDataSourceState::Error) {
        setState(GaugeDataSourceState::Error);
    }
    emit errorOccurred(message);
    return false;
}

} // namespace vision3d::realtime
