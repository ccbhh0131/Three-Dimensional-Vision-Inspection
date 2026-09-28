#pragma once

#include "core/assets/AssetRecord.h"

#include <QList>
#include <QString>

namespace vision3d {

struct ReconstructionJobPaths
{
    QString taskId;
    QString projectDirectory;
    QString relativeRoot;
    QString root;
    QString inputDirectory;
    QString databasePath;
    QString sparseDirectory;
    QString denseDirectory;
    QString refinementDirectory;
    QString logsDirectory;
    QString taskJsonPath;
    QString inputManifestPath;
};

struct ReconstructionInputMapping
{
    QString assetId;
    QString jobFile;
    QString originalFileName;
};

class ReconstructionWorkspace final
{
public:
    static bool create(const QString& projectDirectory,
                       const QString& taskId,
                       ReconstructionJobPaths* paths,
                       QString* error = nullptr);

    static bool stageInputs(const ReconstructionJobPaths& paths,
                            const QList<AssetRecord>& assets,
                            QList<ReconstructionInputMapping>* mappings,
                            QString* error = nullptr);

    static bool writeInputManifest(const ReconstructionJobPaths& paths,
                                   const QList<ReconstructionInputMapping>& mappings,
                                   QString* error = nullptr);

private:
    static bool copyFileAndVerify(const QString& sourcePath,
                                  const QString& targetPath,
                                  QString* error = nullptr);
};

} // namespace vision3d
