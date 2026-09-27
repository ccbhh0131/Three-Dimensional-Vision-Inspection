#include "smoke/MainWindowIntegrationSmoke.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QTextStream>

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

    return vision3d::runMainWindowIntegrationSmoke(
        application,
        parser.value(QStringLiteral("project")),
        parser.value(QStringLiteral("second-project")),
        parser.value(QStringLiteral("capture-dir")),
        parser.value(QStringLiteral("smoke-log")));
}
