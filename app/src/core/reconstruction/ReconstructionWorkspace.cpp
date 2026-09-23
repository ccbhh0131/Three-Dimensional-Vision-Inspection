#include "core/reconstruction/ReconstructionWorkspace.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

namespace vision3d {

namespace {

QString normalizeRelative(const QString& path)
{
    return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

bool sha256ForFile(const QString& path, QByteArray* digest, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法读取输入文件: %1").arg(file.errorString());
        }
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && !file.atEnd()) {
            if (error != nullptr) {
                *error = QStringLiteral("读取输入文件失败: %1").arg(file.errorString());
            }
            return false;
        }
        hash.addData(chunk);
    }
    if (digest != nullptr) {
        *digest = hash.result();
    }
    return true;
}

} // namespace

bool ReconstructionWorkspace::create(const QString& projectDirectory,
                                      const QString& taskId,
                                      ReconstructionJobPaths* paths,
                                      QString* error)
{
    if (paths == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("Job 路径输出参数为空。 ").trimmed();
        }
        return false;
    }
    if (taskId.trimmed().isEmpty() || taskId.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")))) {
        if (error != nullptr) {
            *error = QStringLiteral("taskId 必须是 ASCII 文件名。 ");
        }
        return false;
    }
    const QDir project(QDir(projectDirectory).absolutePath());
    if (!project.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("项目目录不存在: %1").arg(projectDirectory);
        }
        return false;
    }

    ReconstructionJobPaths result;
    result.taskId = taskId;
    result.projectDirectory = project.absolutePath();
    result.relativeRoot = QStringLiteral("reconstruction/jobs/%1").arg(taskId);
    result.root = project.filePath(result.relativeRoot);
    result.inputDirectory = QDir(result.root).filePath(QStringLiteral("input"));
    result.databasePath = QDir(result.root).filePath(QStringLiteral("database.db"));
    result.sparseDirectory = QDir(result.root).filePath(QStringLiteral("sparse"));
    result.denseDirectory = QDir(result.root).filePath(QStringLiteral("dense"));
    result.logsDirectory = QDir(result.root).filePath(QStringLiteral("logs"));
    result.taskJsonPath = QDir(result.root).filePath(QStringLiteral("task.json"));
    result.inputManifestPath = QDir(result.root).filePath(QStringLiteral("input_manifest.json"));

    if (QFileInfo::exists(result.root)) {
        if (error != nullptr) {
            *error = QStringLiteral("Job 目录已存在，拒绝覆盖: %1").arg(result.root);
        }
        return false;
    }
    const QDir root(result.root);
    const QStringList directories = {
        result.relativeRoot,
        result.relativeRoot + QStringLiteral("/input"),
        result.relativeRoot + QStringLiteral("/sparse"),
        result.relativeRoot + QStringLiteral("/dense"),
        result.relativeRoot + QStringLiteral("/logs"),
    };
    for (const QString& directory : directories) {
        if (!project.mkpath(directory)) {
            if (error != nullptr) {
                *error = QStringLiteral("无法创建 Job 子目录: %1").arg(project.filePath(directory));
            }
            return false;
        }
    }
    *paths = result;
    return true;
}

bool ReconstructionWorkspace::copyFileAndVerify(const QString& sourcePath,
                                                const QString& targetPath,
                                                QString* error)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("输入资产不存在: %1").arg(sourcePath);
        }
        return false;
    }
    QByteArray sourceDigest;
    if (!sha256ForFile(sourcePath, &sourceDigest, error)) {
        return false;
    }
    const QString temporaryPath = QFileInfo(targetPath).dir().filePath(
        QStringLiteral(".%1.importing").arg(QFileInfo(targetPath).fileName()));
    QFile::remove(temporaryPath);
    if (!QFile::copy(sourcePath, temporaryPath)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法复制输入资产到 Job: %1").arg(temporaryPath);
        }
        return false;
    }
    QByteArray copiedDigest;
    const bool verified = sha256ForFile(temporaryPath, &copiedDigest, error)
                          && sourceInfo.size() == QFileInfo(temporaryPath).size()
                          && sourceDigest == copiedDigest;
    if (!verified) {
        QFile::remove(temporaryPath);
        if (error != nullptr && error->isEmpty()) {
            *error = QStringLiteral("输入快照校验失败: %1").arg(targetPath);
        }
        return false;
    }
    if (!QFile::rename(temporaryPath, targetPath)) {
        QFile::remove(temporaryPath);
        if (error != nullptr) {
            *error = QStringLiteral("无法提交输入快照: %1").arg(targetPath);
        }
        return false;
    }
    return true;
}

bool ReconstructionWorkspace::stageInputs(const ReconstructionJobPaths& paths,
                                          const QList<AssetRecord>& assets,
                                          QList<ReconstructionInputMapping>* mappings,
                                          QString* error)
{
    if (mappings == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("输入映射输出参数为空。 ").trimmed();
        }
        return false;
    }
    mappings->clear();
    for (const AssetRecord& asset : assets) {
        QString validationError;
        if (!asset.isValid(&validationError)) {
            if (error != nullptr) {
                *error = QStringLiteral("输入资产无效: %1").arg(validationError);
            }
            return false;
        }
        const QString sourcePath = QDir(paths.projectDirectory).filePath(
            QDir::fromNativeSeparators(QDir::cleanPath(asset.relativePath)));
        QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.isFile()) {
            if (error != nullptr) {
                *error = QStringLiteral("输入资产文件不存在: %1").arg(sourcePath);
            }
            return false;
        }
        QString extension = sourceInfo.suffix().toLower();
        if (extension.isEmpty() || extension.contains(QRegularExpression(QStringLiteral("[^a-z0-9]")))) {
            extension = QStringLiteral("img");
        }
        const QString fileName = asset.id + QLatin1Char('.') + extension;
        const QString targetPath = QDir(paths.inputDirectory).filePath(fileName);
        QString copyError;
        if (!copyFileAndVerify(sourcePath, targetPath, &copyError)) {
            if (error != nullptr) {
                *error = copyError;
            }
            return false;
        }
        mappings->append({asset.id,
                          QStringLiteral("input/") + fileName,
                          asset.originalFileName});
    }
    return writeInputManifest(paths, *mappings, error);
}

bool ReconstructionWorkspace::writeInputManifest(
    const ReconstructionJobPaths& paths,
    const QList<ReconstructionInputMapping>& mappings,
    QString* error)
{
    QJsonArray array;
    for (const ReconstructionInputMapping& mapping : mappings) {
        array.append(QJsonObject{
            {QStringLiteral("assetId"), mapping.assetId},
            {QStringLiteral("jobFile"), normalizeRelative(mapping.jobFile)},
            {QStringLiteral("originalFileName"), mapping.originalFileName},
        });
    }
    QSaveFile file(paths.inputManifestPath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法写入 input_manifest.json: %1").arg(file.errorString());
        }
        return false;
    }
    const QByteArray payload = QJsonDocument(array).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("保存 input_manifest.json 失败: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace vision3d
