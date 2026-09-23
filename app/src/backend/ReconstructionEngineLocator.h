#pragma once

#include <QString>

namespace vision3d {

// Resolves product-owned and developer-only engine candidates without
// exposing implementation paths to the ordinary reconstruction UI.
class ReconstructionEngineLocator final
{
public:
    static QString internalEngineRoot(const QString& applicationDirectory);
    static QString internalEngineRoot();

    static QString savedDevelopmentBackendRoot();
    static void saveDevelopmentBackendRoot(const QString& root);

    static QString preferredRoot(const QString& applicationDirectory,
                                 const QString& developmentFallbackRoot);
    static QString preferredRoot();

    static bool isInternalRoot(const QString& candidate,
                               const QString& applicationDirectory);
    static bool isInternalRoot(const QString& candidate);
};

} // namespace vision3d
