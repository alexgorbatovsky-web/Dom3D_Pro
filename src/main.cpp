#include "ui/MainWindow.h"
#include "ui/StartupSplash.h"
#include "ui/LanguageManager.h"
#include "ui/MeasurementUnits.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QSplashScreen>
#include <QSurfaceFormat>
#include <QTimer>

namespace {

class StartupSplash final : public QSplashScreen {
public:
    explicit StartupSplash(const QPixmap& pixmap)
        : QSplashScreen(pixmap, Qt::WindowStaysOnTopHint) {}

protected:
    void mousePressEvent(QMouseEvent* event) override {
        event->accept();
    }
};
}

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setDepthBufferSize(24);
    format.setSamples(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(0);   // <-- off VSync
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setOrganizationName("Dom3D");
    QApplication::setApplicationName("Dom3D Pro");
    QApplication::setApplicationVersion("2026.01");
    QApplication::setApplicationDisplayName("Dom3D Pro 2026.01");
    ApplyNumberInputLocale();
    Themes::Initialize();
    app.setWindowIcon(QIcon(":/icons/app_icon.ico"));

    QString startup_project_path;
    const QStringList arguments = app.arguments();
    for (int index = 1; index < arguments.size(); ++index) {
        const QString candidate = arguments.at(index);
        if (candidate == QStringLiteral("--")) {
            continue;
        }
        const QString lower_candidate = candidate.toLower();
        if (lower_candidate.endsWith(QStringLiteral(".dom3d"))
            || lower_candidate.endsWith(QStringLiteral(".d3dm"))
            || lower_candidate.endsWith(QStringLiteral(".wrk"))
            || lower_candidate.endsWith(QStringLiteral(".step"))
            || lower_candidate.endsWith(QStringLiteral(".stp"))) {
            startup_project_path = QFileInfo(candidate).absoluteFilePath();
            break;
        }
    }

    StartupSplash splash(CreateStartupPixmap());
    splash.show();
    app.processEvents(QEventLoop::AllEvents);
    QElapsedTimer splash_visible_time;
    splash_visible_time.start();
    splash.showMessage(
        DomTranslate("Loading interface..."),
        Qt::AlignLeft | Qt::AlignBottom,
        QColor(220, 232, 250));
    app.processEvents(QEventLoop::AllEvents);

    MainWindow window;
    window.setWindowIcon(QIcon(":/icons/app_icon.ico"));
    splash.showMessage(
        DomTranslate("Preparing 3D viewport..."),
        Qt::AlignLeft | Qt::AlignBottom,
        QColor(220, 232, 250));

    constexpr qint64 minimum_splash_time_ms = 500;
    bool startup_finished = false;
    const auto complete_startup = [&]() {
        if (startup_finished) return;
        startup_finished = true;
        splash.finish(&window);
        QTimer::singleShot(
            0, &window,
            [&window, startup_project_path]() {
                window.CompleteStartup(startup_project_path);
            });
    };
    const auto finish_startup = [&]() {
        if (startup_finished) return;
        const qint64 remaining =
            minimum_splash_time_ms - splash_visible_time.elapsed();
        if (remaining > 0) {
            QTimer::singleShot(
                static_cast<int>(remaining), &app, complete_startup);
        } else {
            complete_startup();
        }
    };
    if (auto* viewport = window.findChild<OpenGLViewport*>()) {
        QObject::connect(
            viewport, &OpenGLViewport::FirstFrameRendered,
            &app, finish_startup, Qt::QueuedConnection);
    }
    // Never leave the splash stranded if the graphics driver cannot produce
    // a frame.  Normal startup closes it through FirstFrameRendered.
    QTimer::singleShot(30000, &app, finish_startup);
    window.showMaximized();
    return app.exec();
}
