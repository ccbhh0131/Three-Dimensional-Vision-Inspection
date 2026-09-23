#pragma once

#include "backend/ReconstructionBackend.h"
#include "core/process/ProcessRunner.h"

#include <QProcess>
#include <QStringList>

namespace vision3d {

struct ReconstructionConfig
{
    bool useGpuFeature = true;
    bool useGpuMatching = true;
    int gpuIndex = 0;
};

struct ProcessCommand
{
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QString description;
};

class ColmapBackend final : public ReconstructionBackend
{
public:
    QString backendName() const override;
    BackendProbeResult probe(const QString& backendRoot) override;

    BackendProbeResult validateRoot(const QString& backendRoot) const;
    BackendProbeResult parseProbeResult(const QString& backendRoot,
                                        int exitCode,
                                        QProcess::ExitStatus exitStatus,
                                        const QString& standardOutput,
                                        const QString& standardError,
                                        const QString& processError = QString()) const;

    static QString program();
    static QStringList probeArguments();
    static QString parseVersion(const QString& output);

    ProcessCommand probeCommand(const QString& backendRoot,
                                const QString& workingDirectory) const;
    ProcessCommand featureExtractionCommand(const QString& backendRoot,
                                            const QString& jobRoot,
                                            const ReconstructionConfig& config) const;
    ProcessCommand featureMatchingCommand(const QString& backendRoot,
                                          const QString& jobRoot,
                                          const ReconstructionConfig& config) const;
    ProcessCommand sparseMappingCommand(const QString& backendRoot,
                                        const QString& jobRoot) const;
    ProcessCommand undistortionCommand(const QString& backendRoot,
                                       const QString& jobRoot,
                                       const QString& primaryModelRelativePath) const;
    ProcessCommand patchMatchCommand(const QString& backendRoot,
                                     const QString& jobRoot,
                                     const ReconstructionConfig& config) const;
    ProcessCommand fusionCommand(const QString& backendRoot,
                                 const QString& jobRoot) const;
    ProcessCommand meshingCommand(const QString& backendRoot,
                                  const QString& jobRoot) const;

    static QString commandLine(const ProcessCommand& command);

private:
    ProcessCommand makeCommand(const QString& backendRoot,
                               const QString& jobRoot,
                               const QString& subcommand,
                               const QStringList& arguments,
                               const QString& description) const;
};

} // namespace vision3d
