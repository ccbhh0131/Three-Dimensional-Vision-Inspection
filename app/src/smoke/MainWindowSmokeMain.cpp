#include "smoke/MainWindowIntegrationSmoke.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Vision3DInspector"));
    QCoreApplication::setApplicationName(QStringLiteral("Vision3DMainWindowSmoke"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Developer smoke for the integrated MainWindow 3D viewer."));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        QStringLiteral("project"),
        QStringLiteral("Project directory or project.json."),
        QStringLiteral("path")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("second-project"),
        QStringLiteral("Second project used for stale-viewer validation."),
        QStringLiteral("path")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("capture-dir"),
        QStringLiteral("Controlled capture directory."),
        QStringLiteral("path")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("smoke-log"),
        QStringLiteral("Controlled smoke log path."),
        QStringLiteral("path")));
    parser.process(application);

    const QStringList requiredOptions{
        QStringLiteral("project"),
        QStringLiteral("second-project"),
        QStringLiteral("capture-dir"),
        QStringLiteral("smoke-log"),
    };
    for (const QString& option : requiredOptions) {
        if (!parser.isSet(option)) {
            QTextStream errorStream(stderr);
            errorStream << "ERROR: --" << option
                        << " is required for the developer smoke.\n";
            return 2;
        }
    }

    // The visual-reading flow writes its diagnostic overlay through this
    // process-local capture directory. Keep the CLI option and the dialog's
    // capture path in sync so the final assertion checks the same artifact.
    // Use the wide Windows API because the controlled D: path contains
    // Chinese characters and must survive the environment-variable boundary.
    const QString captureDirectory = parser.value(QStringLiteral("capture-dir"));
#ifdef Q_OS_WIN
    const std::wstring captureDirectoryWide = captureDirectory.toStdWString();
    if (SetEnvironmentVariableW(L"VISION3DINSPECTOR_STAGE4D_CAPTURE_DIR",
                                captureDirectoryWide.c_str()) == 0) {
        QTextStream errorStream(stderr);
        errorStream << "ERROR: could not set the visual capture directory environment.\n";
        return 2;
    }
#else
    if (qputenv("VISION3DINSPECTOR_STAGE4D_CAPTURE_DIR",
                captureDirectory.toUtf8()) != 0) {
        QTextStream errorStream(stderr);
        errorStream << "ERROR: could not set the visual capture directory environment.\n";
        return 2;
    }
#endif

    return vision3d::runMainWindowIntegrationSmoke(
        application,
        parser.value(QStringLiteral("project")),
        parser.value(QStringLiteral("second-project")),
        parser.value(QStringLiteral("capture-dir")),
        parser.value(QStringLiteral("smoke-log")));
}
