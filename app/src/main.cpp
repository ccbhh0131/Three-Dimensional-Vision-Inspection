#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Vision3DInspector"));
    QCoreApplication::setApplicationName(QStringLiteral("Vision3DInspector"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    vision3d::MainWindow window;
    window.resize(1120, 720);
    window.show();

    return application.exec();
}
