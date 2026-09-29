#include "app/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QSurfaceFormat>

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
    QCoreApplication::setApplicationName(QStringLiteral("Vision3DInspector"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    application.setWindowIcon(
        QIcon(QStringLiteral(":/branding/icons/Vision3DInspector.ico")));

    vision3d::MainWindow window;
    window.resize(1120, 720);
    window.show();

    return application.exec();
}
