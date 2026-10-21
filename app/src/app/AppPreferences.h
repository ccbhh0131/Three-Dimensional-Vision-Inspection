#pragma once

#include <QString>

namespace vision3d {

// Small application-level preference store.  It deliberately uses an explicit
// INI file next to the running application so user preferences never fall
// back to the Windows registry.
class AppPreferences final
{
public:
    AppPreferences() = default;

    static QString settingsFilePath();
    static QString configDirectory();

    QString defaultProjectDirectory() const;
    void setDefaultProjectDirectory(const QString& directory);
    QString lastProjectDirectory() const;
    QString lastProjectManifest() const;
    void setLastProjectManifest(const QString& manifestPath);
    QString projectDialogDirectory() const;
    void rememberProjectDirectory(const QString& directory);

    bool restoreLastProject() const;
    void setRestoreLastProject(bool enabled);
    bool rememberLastDirectory() const;
    void setRememberLastDirectory(bool enabled);

    bool autoFit() const;
    void setAutoFit(bool enabled);
    double orbitSensitivity() const;
    void setOrbitSensitivity(double sensitivity);
    bool showMarkers() const;
    void setShowMarkers(bool enabled);
    double markerSize() const;
    void setMarkerSize(double scale);

    int realtimePollIntervalMs() const;
    void setRealtimePollIntervalMs(int intervalMs);

    void reset();

private:
    static bool isAllowedPollInterval(int intervalMs);
    static double clampOrbitSensitivity(double sensitivity);
    static double clampMarkerSize(double scale);
};

} // namespace vision3d
