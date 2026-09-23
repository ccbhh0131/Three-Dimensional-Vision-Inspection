#pragma once

#include "backend/BackendProbeResult.h"

namespace vision3d {

class ReconstructionBackend
{
public:
    virtual ~ReconstructionBackend() = default;

    virtual QString backendName() const = 0;
    virtual BackendProbeResult probe(const QString& backendRoot) = 0;
};

} // namespace vision3d
