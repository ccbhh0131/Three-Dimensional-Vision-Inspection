#pragma once

#include <QString>

class QApplication;

namespace vision3d {

int runMainWindowIntegrationSmoke(QApplication& application,
                                  const QString& projectPath,
                                  const QString& secondProjectPath,
                                  const QString& captureDirectory,
                                  const QString& logPath);

} // namespace vision3d
