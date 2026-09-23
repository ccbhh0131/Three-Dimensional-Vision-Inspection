#include "backend/ReconstructionEngineLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace vision3d {

namespace {

QString cleanAbsolute(const QString& path)
{
    return path.trimmed().isEmpty() ? QString() : QDir(path).absolutePath();
}

} // namespace

QString ReconstructionEngineLocator::internalEngineRoot(const QString& applicationDirectory)
{
    return QDir(cleanAbsolute(applicationDirectory))
        .filePath(QStringLiteral("runtime/reconstruction/engine"));
}

QString ReconstructionEngineLocator::internalEngineRoot()
{
    return internalEngineRoot(QCoreApplication::applicationDirPath());
}

QString ReconstructionEngineLocator::savedDevelopmentBackendRoot()
{
    QSettings settings;
    QString root = settings.value(QStringLiteral("reconstruction/developmentBackendRoot"))
                       .toString()
                       .trimmed();
    if (root.isEmpty()) {
        // Backward-compatible read for the Stage 1/2 developer setting.
        root = settings.value(QStringLiteral("colmap/backendRoot")).toString().trimmed();
    }
    return root;
}

void ReconstructionEngineLocator::saveDevelopmentBackendRoot(const QString& root)
{
    QSettings settings;
    const QString trimmed = root.trimmed();
    settings.setValue(QStringLiteral("reconstruction/developmentBackendRoot"), trimmed);
    // Keep the previous key readable by existing Stage 1/2 developer tools.
    settings.setValue(QStringLiteral("colmap/backendRoot"), trimmed);
}

QString ReconstructionEngineLocator::preferredRoot(const QString& applicationDirectory,
                                                    const QString& developmentFallbackRoot)
{
    const QString internal = internalEngineRoot(applicationDirectory);
    if (QDir(internal).exists()) {
        return internal;
    }
    return developmentFallbackRoot.trimmed();
}

QString ReconstructionEngineLocator::preferredRoot()
{
    return preferredRoot(QCoreApplication::applicationDirPath(),
                         savedDevelopmentBackendRoot());
}

bool ReconstructionEngineLocator::isInternalRoot(const QString& candidate,
                                                  const QString& applicationDirectory)
{
    const QString left = cleanAbsolute(candidate);
    const QString right = cleanAbsolute(internalEngineRoot(applicationDirectory));
    return !left.isEmpty() && left.compare(right, Qt::CaseInsensitive) == 0;
}

bool ReconstructionEngineLocator::isInternalRoot(const QString& candidate)
{
    return isInternalRoot(candidate, QCoreApplication::applicationDirPath());
}

} // namespace vision3d
