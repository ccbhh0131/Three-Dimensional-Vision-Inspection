#pragma once

#include <QString>

namespace vision3d {

struct BackendProbeResult
{
    bool available = false;
    QString backendName;
    QString version;
    QString executablePath;
    QString rootPath;
    QString message;
    QString rawOutput;
};

} // namespace vision3d
