#include "backend/ColmapBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace vision3d {

QString ColmapBackend::backendName() const
{
    return QStringLiteral("COLMAP");
}

BackendProbeResult ColmapBackend::validateRoot(const QString& backendRoot) const
{
    BackendProbeResult result;
    result.backendName = backendName();
    result.rootPath = QDir(backendRoot).absolutePath();
    result.executablePath = QDir(result.rootPath).filePath(QStringLiteral("bin/colmap.exe"));

    if (backendRoot.trimmed().isEmpty() || !QDir(backendRoot).exists()) {
        result.message = QStringLiteral("COLMAP 根目录不存在。请选择包含 COLMAP.bat 的目录。");
        return result;
    }

    const QString batchPath = QDir(result.rootPath).filePath(QStringLiteral("COLMAP.bat"));
    if (!QFileInfo(batchPath).isFile()) {
        result.message = QStringLiteral("缺少 COLMAP.bat: %1").arg(batchPath);
        return result;
    }
    if (!QFileInfo(result.executablePath).isFile()) {
        result.message = QStringLiteral("缺少 bin/colmap.exe: %1").arg(result.executablePath);
        return result;
    }

    result.message = QStringLiteral("COLMAP 根目录结构有效，准备执行 Probe。");
    return result;
}

BackendProbeResult ColmapBackend::probe(const QString& backendRoot)
{
    const BackendProbeResult validation = validateRoot(backendRoot);
    if (!validation.message.startsWith(QStringLiteral("COLMAP 根目录结构有效"))) {
        return validation;
    }

    ProcessRunner runner;
    const ProcessResult process = runner.runBlocking(program(), probeArguments(), validation.rootPath);
    return parseProbeResult(validation.rootPath,
                            process.exitCode,
                            process.exitStatus,
                            process.standardOutput,
                            process.standardError,
                            process.processError);
}

BackendProbeResult ColmapBackend::parseProbeResult(const QString& backendRoot,
                                                   int exitCode,
                                                   QProcess::ExitStatus exitStatus,
                                                   const QString& standardOutput,
                                                   const QString& standardError,
                                                   const QString& processError) const
{
    BackendProbeResult result = validateRoot(backendRoot);
    result.rawOutput = standardOutput;
    if (!standardError.isEmpty()) {
        if (!result.rawOutput.isEmpty()) {
            result.rawOutput.append(QLatin1Char('\n'));
        }
        result.rawOutput.append(standardError);
    }
    result.version = parseVersion(result.rawOutput);

    if (!processError.isEmpty()) {
        result.message = QStringLiteral("COLMAP Probe 进程错误: %1").arg(processError);
        return result;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        result.message = QStringLiteral("COLMAP Probe 失败，exit code=%1。").arg(exitCode);
        return result;
    }
    if (result.version.isEmpty()) {
        result.message = QStringLiteral("COLMAP Probe 完成，但未能从输出解析版本。");
        return result;
    }

    result.available = true;
    if (result.version == QStringLiteral("3.11.1")) {
        result.message = QStringLiteral("COLMAP 3.11.1 可用，版本已验证。");
    } else {
        result.message = QStringLiteral("检测到 COLMAP %1，但当前项目仅验证 3.11.1；版本未验证。")
                             .arg(result.version);
    }
    return result;
}

QString ColmapBackend::program()
{
    return QStringLiteral("cmd.exe");
}

QStringList ColmapBackend::probeArguments()
{
    return {QStringLiteral("/d"), QStringLiteral("/c"), QStringLiteral("COLMAP.bat"), QStringLiteral("-h")};
}

QString ColmapBackend::parseVersion(const QString& output)
{
    const QRegularExpression expression(
        QStringLiteral("\\bCOLMAP\\s+([0-9]+\\.[0-9]+\\.[0-9]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(output);
    return match.hasMatch() ? match.captured(1) : QString();
}

ProcessCommand ColmapBackend::makeCommand(const QString& backendRoot,
                                          const QString& jobRoot,
                                          const QString& subcommand,
                                          const QStringList& arguments,
                                          const QString& description) const
{
    // A project/job may live under a Unicode Windows path. Launch the verified
    // executable directly so cmd.exe does not reinterpret a Unicode absolute
    // batch path through its active code page. COLMAP receives only ASCII-
    // relative database/image/output paths; the Job root is the working dir.
    const QString executablePath = QDir(QDir(backendRoot).absolutePath()).filePath(
        QStringLiteral("bin/colmap.exe"));
    ProcessCommand command;
    command.program = executablePath;
    command.arguments = subcommand.isEmpty() ? arguments
                                             : (QStringList{ subcommand } + arguments);
    command.workingDirectory = QDir(jobRoot).absolutePath();
    command.description = description;
    return command;
}

ProcessCommand ColmapBackend::probeCommand(const QString& backendRoot,
                                           const QString& workingDirectory) const
{
    return makeCommand(backendRoot,
                       workingDirectory,
                       QString(),
                       {QStringLiteral("-h")},
                       QStringLiteral("COLMAP backend probe"));
}

ProcessCommand ColmapBackend::featureExtractionCommand(const QString& backendRoot,
                                                       const QString& jobRoot,
                                                       const ReconstructionConfig& config) const
{
    return makeCommand(
        backendRoot,
        jobRoot,
        QStringLiteral("feature_extractor"),
        {QStringLiteral("--database_path"),
         QStringLiteral("database.db"),
         QStringLiteral("--image_path"),
         QStringLiteral("input"),
         QStringLiteral("--ImageReader.camera_model"),
         QStringLiteral("SIMPLE_RADIAL"),
         QStringLiteral("--ImageReader.single_camera"),
         QStringLiteral("1"),
         QStringLiteral("--SiftExtraction.use_gpu"),
         config.useGpuFeature ? QStringLiteral("1") : QStringLiteral("0"),
         QStringLiteral("--SiftExtraction.gpu_index"),
         QString::number(config.gpuIndex)},
        QStringLiteral("A1 Feature Extraction"));
}

ProcessCommand ColmapBackend::featureMatchingCommand(const QString& backendRoot,
                                                     const QString& jobRoot,
                                                     const ReconstructionConfig& config) const
{
    return makeCommand(backendRoot,
                       jobRoot,
                       QStringLiteral("exhaustive_matcher"),
                       {QStringLiteral("--database_path"),
                        QStringLiteral("database.db"),
                        QStringLiteral("--SiftMatching.use_gpu"),
                        config.useGpuMatching ? QStringLiteral("1") : QStringLiteral("0"),
                        QStringLiteral("--SiftMatching.gpu_index"),
                        QString::number(config.gpuIndex)},
                       QStringLiteral("A2 Feature Matching"));
}

ProcessCommand ColmapBackend::sparseMappingCommand(const QString& backendRoot,
                                                   const QString& jobRoot) const
{
    return makeCommand(backendRoot,
                       jobRoot,
                       QStringLiteral("mapper"),
                       {QStringLiteral("--database_path"),
                        QStringLiteral("database.db"),
                        QStringLiteral("--image_path"),
                        QStringLiteral("input"),
                        QStringLiteral("--output_path"),
                        QStringLiteral("sparse")},
                       QStringLiteral("A3 Sparse Mapping"));
}

ProcessCommand ColmapBackend::undistortionCommand(const QString& backendRoot,
                                                  const QString& jobRoot,
                                                  const QString& primaryModelRelativePath) const
{
    return makeCommand(backendRoot,
                       jobRoot,
                       QStringLiteral("image_undistorter"),
                       {QStringLiteral("--image_path"),
                        QStringLiteral("input"),
                        QStringLiteral("--input_path"),
                        QDir::fromNativeSeparators(primaryModelRelativePath),
                        QStringLiteral("--output_path"),
                        QStringLiteral("dense"),
                        QStringLiteral("--output_type"),
                        QStringLiteral("COLMAP")},
                       QStringLiteral("A4 Image Undistortion"));
}

ProcessCommand ColmapBackend::patchMatchCommand(const QString& backendRoot,
                                                const QString& jobRoot,
                                                const ReconstructionConfig& config) const
{
    return makeCommand(backendRoot,
                       jobRoot,
                       QStringLiteral("patch_match_stereo"),
                       {QStringLiteral("--workspace_path"),
                        QStringLiteral("dense"),
                        QStringLiteral("--workspace_format"),
                        QStringLiteral("COLMAP"),
                        QStringLiteral("--PatchMatchStereo.gpu_index"),
                        QString::number(config.gpuIndex)},
                       QStringLiteral("A5 CUDA PatchMatch Stereo"));
}

ProcessCommand ColmapBackend::fusionCommand(const QString& backendRoot,
                                            const QString& jobRoot) const
{
    return makeCommand(backendRoot,
                       jobRoot,
                       QStringLiteral("stereo_fusion"),
                       {QStringLiteral("--workspace_path"),
                        QStringLiteral("dense"),
                        QStringLiteral("--workspace_format"),
                        QStringLiteral("COLMAP"),
                        QStringLiteral("--input_type"),
                        QStringLiteral("geometric"),
                        QStringLiteral("--output_path"),
                        QStringLiteral("dense/fused.ply")},
                       QStringLiteral("A6 Stereo Fusion"));
}

ProcessCommand ColmapBackend::meshingCommand(const QString& backendRoot,
                                             const QString& jobRoot) const
{
    return makeCommand(backendRoot,
                       jobRoot,
                       QStringLiteral("poisson_mesher"),
                       {QStringLiteral("--input_path"),
                        QStringLiteral("dense/fused.ply"),
                        QStringLiteral("--output_path"),
                        QStringLiteral("dense/meshed-poisson.ply")},
                       QStringLiteral("A7 Poisson Meshing"));
}

QString ColmapBackend::commandLine(const ProcessCommand& command)
{
    return command.program + QLatin1Char(' ') + command.arguments.join(QLatin1Char(' '));
}

} // namespace vision3d
