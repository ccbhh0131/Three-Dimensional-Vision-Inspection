#include "core/realtime/GaugeLiveState.h"

#include <cmath>

namespace vision3d::realtime {

bool GaugeLiveState::isValid(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (gaugeAssetId.trimmed().isEmpty()) {
        return fail(QStringLiteral("GaugeLiveState gaugeAssetId 不能为空。"));
    }
    if (!std::isfinite(value)) {
        return fail(QStringLiteral("GaugeLiveState value 必须是有限数字。"));
    }
    if (!timestamp.isValid()) {
        return fail(QStringLiteral("GaugeLiveState timestamp 必须是有效时间。"));
    }
    if (source == ::vision3d::GaugeDataSource::None) {
        return fail(QStringLiteral("GaugeLiveState source 不能为 None。"));
    }
    if (!connected || !available) {
        return fail(QStringLiteral("GaugeLiveState 必须处于 connected/available 状态。"));
    }
    return true;
}

GaugeLiveState GaugeLiveState::fromSample(const GaugeSample& sample, quint64 sequence)
{
    GaugeLiveState state;
    state.gaugeAssetId = sample.gaugeAssetId.trimmed();
    state.value = sample.value;
    state.timestamp = sample.timestamp.toUTC();
    state.source = sample.source;
    state.connected = true;
    state.available = true;
    state.sequence = sequence;
    return state;
}

bool GaugeLiveStateModel::updateSample(const GaugeSample& sample, QString* error)
{
    QString sampleError;
    if (!sample.isValid(&sampleError)) {
        if (error != nullptr) {
            *error = sampleError;
        }
        return false;
    }

    const QString gaugeAssetId = sample.gaugeAssetId.trimmed();
    const quint64 sequence = m_states.contains(gaugeAssetId)
        ? m_states.value(gaugeAssetId).sequence + 1
        : 1;
    const GaugeLiveState state = GaugeLiveState::fromSample(sample, sequence);
    QString stateError;
    if (!state.isValid(&stateError)) {
        if (error != nullptr) {
            *error = stateError;
        }
        return false;
    }
    m_states.insert(gaugeAssetId, state);
    return true;
}

std::optional<GaugeLiveState> GaugeLiveStateModel::stateForGauge(
    const QString& gaugeAssetId) const
{
    const QString normalizedId = gaugeAssetId.trimmed();
    if (normalizedId.isEmpty() || !m_states.contains(normalizedId)) {
        return std::nullopt;
    }
    return m_states.value(normalizedId);
}

bool GaugeLiveStateModel::contains(const QString& gaugeAssetId) const
{
    return m_states.contains(gaugeAssetId.trimmed());
}

qsizetype GaugeLiveStateModel::size() const
{
    return m_states.size();
}

void GaugeLiveStateModel::clearGauge(const QString& gaugeAssetId)
{
    m_states.remove(gaugeAssetId.trimmed());
}

void GaugeLiveStateModel::clearAll()
{
    m_states.clear();
}

} // namespace vision3d::realtime
