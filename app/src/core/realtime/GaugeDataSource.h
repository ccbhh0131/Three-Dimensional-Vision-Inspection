#pragma once

#include "core/device/GaugeAsset.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace vision3d::realtime {

enum class GaugeDataSourceState
{
    Stopped,
    Running,
    Error,
};

QString gaugeDataSourceStateToString(GaugeDataSourceState state);
QString gaugeDataSourceStateDisplayName(GaugeDataSourceState state);

struct GaugeSample
{
    QString gaugeAssetId;
    double value = 0.0;
    QDateTime timestamp;
    ::vision3d::GaugeDataSource source = ::vision3d::GaugeDataSource::Sensor;

    bool isValid(QString* error = nullptr) const;
    static GaugeSample create(const QString& gaugeAssetId,
                              double value,
                              const QDateTime& timestamp = QDateTime::currentDateTimeUtc());
};

class GaugeDataSource : public QObject
{
    Q_OBJECT

public:
    explicit GaugeDataSource(QObject* parent = nullptr);
    ~GaugeDataSource() override = default;

    virtual bool start(QString* error = nullptr) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual GaugeDataSourceState state() const = 0;

signals:
    void sampleReceived(const GaugeSample& sample);
    void stateChanged(vision3d::realtime::GaugeDataSourceState state);
    void errorOccurred(const QString& error);
};

class MockSensorDataSource final : public GaugeDataSource
{
    Q_OBJECT

public:
    explicit MockSensorDataSource(const QString& gaugeAssetId,
                                  const QVector<double>& sequence = {},
                                  int intervalMs = 500,
                                  QObject* parent = nullptr);

    bool start(QString* error = nullptr) override;
    void stop() override;
    bool isRunning() const override;
    GaugeDataSourceState state() const override;

    QString gaugeAssetId() const;
    QVector<double> sequence() const;
    int intervalMs() const;
    void setSequence(const QVector<double>& sequence);
    void setIntervalMs(int intervalMs);

    static QVector<double> defaultSequence();

private slots:
    void emitNextSample();

private:
    void setState(GaugeDataSourceState state);
    bool fail(const QString& message, QString* error);

    QString m_gaugeAssetId;
    QVector<double> m_sequence;
    int m_intervalMs = 500;
    qsizetype m_sequenceIndex = 0;
    quint64 m_emittedCount = 0;
    GaugeDataSourceState m_state = GaugeDataSourceState::Stopped;
    QTimer m_timer;
};

} // namespace vision3d::realtime

Q_DECLARE_METATYPE(vision3d::realtime::GaugeSample)
Q_DECLARE_METATYPE(vision3d::realtime::GaugeDataSourceState)
