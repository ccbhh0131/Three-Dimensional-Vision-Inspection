#pragma once

#include <QString>

namespace vision3d {

struct SparseModelSelection
{
    bool valid = false;
    QString relativePath;
    int registeredImages = 0;
    QString message;
};

class ReconstructionArtifactValidator final
{
public:
    static bool validateDatabase(const QString& path, QString* error = nullptr);
    static SparseModelSelection selectPrimarySparseModel(const QString& sparseDirectory);
    static bool validateUndistortion(const QString& denseDirectory, QString* error = nullptr);
    static bool validatePatchMatch(const QString& denseDirectory, QString* error = nullptr);
    static bool validateNonEmptyFile(const QString& path, QString* error = nullptr);
    static int registeredImageCount(const QString& modelDirectory);
};

} // namespace vision3d
