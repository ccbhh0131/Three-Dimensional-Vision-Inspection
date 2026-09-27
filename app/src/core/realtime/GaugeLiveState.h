#pragma once

#include "core/realtime/GaugeDataSource.h"

#include <QHash>

#include <optional>

namespace vision3d::realtime {

struct GaugeLiveState
{
    QString gaugeAssetId;
    double value = 0.0;
    QDateTime timestamp;
    ::vision3d::GaugeDataSource source = ::vision3d::GaugeDataSource::Sensor;
    bool connected = false;
    bool available = false;
    quint64 sequence = 0;

    bool isValid(QString* error = nullptr) const;
    static GaugeLiveState fromSample(const GaugeSample& sample, quint64 sequence);
};

class GaugeLiveStateModel final
{
public:
    bool updateSample(const GaugeSample& sample, QString* error = nullptr);
    std::optional<GaugeLiveState> stateForGauge(const QString& gaugeAssetId) const;
    bool contains(const QString& gaugeAssetId) const;
    qsizetype size() const;
    void clearGauge(const QString& gaugeAssetId);
    void clearAll();

private:
    QHash<QString, GaugeLiveState> m_states;
};

} // namespace vision3d::realtime
