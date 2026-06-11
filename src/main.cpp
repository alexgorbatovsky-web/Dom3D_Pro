#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(0);   // <-- выключить VSync
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setOrganizationName("Dom3D");
    QApplication::setApplicationName("Dom3D Pro");
    app.setWindowIcon(QIcon(":/icons/app_icon.ico"));
    MainWindow window;
    window.setWindowIcon(QIcon(":/icons/app_icon.ico"));
    window.showMaximized();
    return app.exec();
}
