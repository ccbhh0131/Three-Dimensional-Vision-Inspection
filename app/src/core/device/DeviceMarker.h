#pragma once

#include "core/device/GaugeStatus.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector3D>

#include <optional>

namespace vision3d {

enum class DeviceMarkerVisualState
{
    Default,
    Unknown,
    Normal,
    Warning,
    Alarm,
};

struct DeviceMarker
{
    QString id;
    QString name;
    QVector3D worldPosition{0.0f, 0.0f, 0.0f};
    QString reconstructionTaskId;
    QString createdAt;

    bool isValid(QString* error = nullptr) const;
    QJsonObject toJson() const;
    static std::optional<DeviceMarker> fromJson(const QJsonObject& json,
                                                QString* error = nullptr);
    static DeviceMarker create(const QString& name,
                               const QVector3D& worldPosition,
                               const QString& reconstructionTaskId);
};

struct DeviceMarkerView
{
    QString id;
    QString label;
    QVector3D worldPosition{0.0f, 0.0f, 0.0f};
    bool selected = false;
    DeviceMarkerVisualState visualState = DeviceMarkerVisualState::Default;
};

class DeviceMarkerModel
{
public:
    bool add(const DeviceMarker& marker, QString* error = nullptr);
    bool remove(const QString& id, QString* error = nullptr);
    std::optional<DeviceMarker> find(const QString& id) const;

    QList<DeviceMarker> list() const;
    QList<DeviceMarker> forReconstruction(const QString& reconstructionTaskId) const;
    QJsonArray toJson() const;
    static std::optional<DeviceMarkerModel> fromJson(const QJsonArray& json,
                                                     QString* error = nullptr);

    bool contains(const QString& id) const;
    qsizetype size() const;
    void clear();

private:
    QList<DeviceMarker> m_markers;
};

} // namespace vision3d
