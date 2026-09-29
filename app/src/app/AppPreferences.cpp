#include "app/AppPreferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace vision3d {
namespace {

constexpr char kSettingsFileName[] = "Vision3DInspector.ini";
constexpr char kDefaultProjectDirectoryKey[] = "paths/defaultProjectDirectory";
constexpr char kLastProjectDirectoryKey[] = "paths/lastProjectDirectory";
constexpr char kLastProjectManifestKey[] = "paths/lastProjectManifest";
constexpr char kRestoreLastProjectKey[] = "projects/restoreLastProject";
constexpr char kRememberLastDirectoryKey[] = "projects/rememberLastDirectory";
constexpr char kAutoFitKey[] = "viewer/autoFit";
constexpr char kOrbitSensitivityKey[] = "viewer/orbitSensitivity";
constexpr char kShowMarkersKey[] = "viewer/showMarkers";
constexpr char kMarkerSizeKey[] = "viewer/markerSize";
constexpr char kRealtimePollIntervalKey[] = "inspection/realtimePollIntervalMs";

QSettings settings()
{
    return QSettings(AppPreferences::settingsFilePath(), QSettings::IniFormat);
}

QString existingDirectoryValue(QSettings& store, const char* key)
{
    const QString value = store.value(QLatin1String(key)).toString().trimmed();
    if (value.isEmpty() || !QDir(value).exists()) {
        return QString();
    }
    return QDir(value).absolutePath();
}

void writeAndSync(QSettings& store, const char* key, const QVariant& value)
{
    store.setValue(QLatin1String(key), value);
    store.sync();
}

} // namespace

QString AppPreferences::settingsFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QString::fromLatin1(kSettingsFileName));
}

QString AppPreferences::configDirectory()
{
    return QFileInfo(settingsFilePath()).absolutePath();
}

QString AppPreferences::defaultProjectDirectory() const
{
    QSettings store = settings();
    return existingDirectoryValue(store, kDefaultProjectDirectoryKey);
}

void AppPreferences::setDefaultProjectDirectory(const QString& directory)
{
    const QString normalized = directory.trimmed().isEmpty()
        ? QString()
        : QDir(directory).absolutePath();
    QSettings store = settings();
    writeAndSync(store, kDefaultProjectDirectoryKey, normalized);
}

QString AppPreferences::lastProjectDirectory() const
{
    QSettings store = settings();
    return existingDirectoryValue(store, kLastProjectDirectoryKey);
}

QString AppPreferences::lastProjectManifest() const
{
    QSettings store = settings();
    const QString value = store.value(QLatin1String(kLastProjectManifestKey))
                              .toString()
                              .trimmed();
    return value.isEmpty() ? QString() : QDir::cleanPath(value);
}

void AppPreferences::setLastProjectManifest(const QString& manifestPath)
{
    const QString normalized = manifestPath.trimmed().isEmpty()
        ? QString()
        : QFileInfo(manifestPath).absoluteFilePath();
    QSettings store = settings();
    writeAndSync(store, kLastProjectManifestKey, normalized);
}

QString AppPreferences::projectDialogDirectory() const
{
    const QString preferred = defaultProjectDirectory();
    if (!preferred.isEmpty()) {
        return preferred;
    }
    const QString remembered = lastProjectDirectory();
    if (!remembered.isEmpty()) {
        return remembered;
    }
    return QDir::homePath();
}

void AppPreferences::rememberProjectDirectory(const QString& directory)
{
    if (!rememberLastDirectory() || directory.trimmed().isEmpty()
        || !QDir(directory).exists()) {
        return;
    }
    QSettings store = settings();
    writeAndSync(store,
                 kLastProjectDirectoryKey,
                 QDir(directory).absolutePath());
}

bool AppPreferences::restoreLastProject() const
{
    return settings().value(QLatin1String(kRestoreLastProjectKey), false).toBool();
}

void AppPreferences::setRestoreLastProject(bool enabled)
{
    QSettings store = settings();
    writeAndSync(store, kRestoreLastProjectKey, enabled);
}

bool AppPreferences::rememberLastDirectory() const
{
    return settings().value(QLatin1String(kRememberLastDirectoryKey), true).toBool();
}

void AppPreferences::setRememberLastDirectory(bool enabled)
{
    QSettings store = settings();
    writeAndSync(store, kRememberLastDirectoryKey, enabled);
}

bool AppPreferences::autoFit() const
{
    return settings().value(QLatin1String(kAutoFitKey), true).toBool();
}

void AppPreferences::setAutoFit(bool enabled)
{
    QSettings store = settings();
    writeAndSync(store, kAutoFitKey, enabled);
}

double AppPreferences::orbitSensitivity() const
{
    return clampOrbitSensitivity(
        settings().value(QLatin1String(kOrbitSensitivityKey), 1.0).toDouble());
}

void AppPreferences::setOrbitSensitivity(double sensitivity)
{
    QSettings store = settings();
    writeAndSync(store, kOrbitSensitivityKey, clampOrbitSensitivity(sensitivity));
}

bool AppPreferences::showMarkers() const
{
    return settings().value(QLatin1String(kShowMarkersKey), true).toBool();
}

void AppPreferences::setShowMarkers(bool enabled)
{
    QSettings store = settings();
    writeAndSync(store, kShowMarkersKey, enabled);
}

double AppPreferences::markerSize() const
{
    return clampMarkerSize(
        settings().value(QLatin1String(kMarkerSizeKey), 1.0).toDouble());
}

void AppPreferences::setMarkerSize(double scale)
{
    QSettings store = settings();
    writeAndSync(store, kMarkerSizeKey, clampMarkerSize(scale));
}

int AppPreferences::realtimePollIntervalMs() const
{
    const int value = settings()
                          .value(QLatin1String(kRealtimePollIntervalKey), 500)
                          .toInt();
    return isAllowedPollInterval(value) ? value : 500;
}

void AppPreferences::setRealtimePollIntervalMs(int intervalMs)
{
    QSettings store = settings();
    writeAndSync(store,
                 kRealtimePollIntervalKey,
                 isAllowedPollInterval(intervalMs) ? intervalMs : 500);
}

void AppPreferences::reset()
{
    QSettings store = settings();
    store.remove(QLatin1String(kDefaultProjectDirectoryKey));
    store.remove(QLatin1String(kLastProjectManifestKey));
    writeAndSync(store, kRestoreLastProjectKey, false);
    writeAndSync(store, kRememberLastDirectoryKey, true);
    writeAndSync(store, kAutoFitKey, true);
    writeAndSync(store, kOrbitSensitivityKey, 1.0);
    writeAndSync(store, kShowMarkersKey, true);
    writeAndSync(store, kMarkerSizeKey, 1.0);
    writeAndSync(store, kRealtimePollIntervalKey, 500);
}

bool AppPreferences::isAllowedPollInterval(int intervalMs)
{
    return intervalMs == 250 || intervalMs == 500
        || intervalMs == 1000 || intervalMs == 2000;
}

double AppPreferences::clampOrbitSensitivity(double sensitivity)
{
    if (!std::isfinite(sensitivity)) {
        return 1.0;
    }
    return std::clamp(sensitivity, 0.5, 2.0);
}

double AppPreferences::clampMarkerSize(double scale)
{
    if (!std::isfinite(scale)) {
        return 1.0;
    }
    return std::clamp(scale, 0.75, 1.5);
}

} // namespace vision3d
