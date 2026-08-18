#include "ui/MainWindow.h"
#include "ui/LanguageManager.h"

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
QPixmap create_startup_pixmap() {
    constexpr int width = 780;
    constexpr int height = 380;
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::black);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient background(0, 0, width, height);
    background.setColorAt(0.0, QColor(3, 7, 17));
    background.setColorAt(0.58, QColor(7, 16, 35));
    background.setColorAt(1.0, QColor(2, 4, 10));
    painter.fillRect(pixmap.rect(), background);

    // Quiet CAD grid and construction lines keep the temporary splash close
    // to the visual language of the classic Dom-3D startup screen.
    painter.setPen(QPen(QColor(45, 102, 180, 38), 1.0));
    for (int x = -height; x < width + height; x += 42) {
        painter.drawLine(x, height, x + height, 0);
    }
    for (int x = 0; x < width + height; x += 42) {
        painter.drawLine(x, 0, x - height, height);
    }

    painter.setPen(QPen(QColor(34, 112, 255, 115), 1.2));
    painter.drawLine(28, 306, 748, 306);
    painter.drawLine(548, 42, 548, 306);

    const QPixmap icon = QIcon(":/icons/app_icon.ico").pixmap(58, 58);
    painter.drawPixmap(34, 34, icon);

    QFont title_font("Segoe UI", 42, QFont::DemiBold);
    painter.setFont(title_font);
    painter.setPen(QColor(238, 245, 255));
    painter.drawText(QRect(108, 25, 430, 68), Qt::AlignVCenter, "Dom3D");
    painter.setPen(QColor(41, 126, 255));
    painter.drawText(QRect(293, 25, 220, 68), Qt::AlignVCenter, "Pro");

    QFont subtitle_font("Segoe UI", 20, QFont::Normal);
    painter.setFont(subtitle_font);
    painter.setPen(QColor(128, 174, 240));
    painter.drawText(36, 136, "3D constructions");
    painter.setFont(QFont("Segoe UI", 16, QFont::DemiBold));
    painter.setPen(QColor(229, 235, 244));
    painter.drawText(36, 177, "CAD  /  CAM  /  CAE");

    // Isometric wireframe cube.
    const QPointF top(654, 74);
    const QPointF left(584, 112);
    const QPointF right(724, 112);
    const QPointF center(654, 151);
    const QPointF bottom_left(584, 197);
    const QPointF bottom_right(724, 197);
    const QPointF bottom(654, 236);
    painter.setPen(QPen(QColor(47, 132, 255), 2.0));
    painter.drawLine(top, left);
    painter.drawLine(top, right);
    painter.drawLine(left, center);
    painter.drawLine(right, center);
    painter.drawLine(left, bottom_left);
    painter.drawLine(right, bottom_right);
    painter.drawLine(center, bottom);
    painter.drawLine(bottom_left, bottom);
    painter.drawLine(bottom_right, bottom);
    painter.drawLine(bottom_left, QPointF(654, 158));
    painter.drawLine(bottom_right, QPointF(654, 158));
    painter.setPen(QPen(QColor(112, 183, 255, 150), 1.0, Qt::DashLine));
    painter.drawLine(top, QPointF(654, 158));
    painter.drawLine(left, bottom_right);
    painter.drawLine(right, bottom_left);

    painter.setPen(QColor(115, 130, 153));
    painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(36, 286, "Parametric 3D modeling system");

    QLinearGradient progress(0, 0, width, 0);
    progress.setColorAt(0.0, QColor(34, 99, 255));
    progress.setColorAt(0.7, QColor(40, 160, 255));
    progress.setColorAt(1.0, QColor(34, 99, 255, 20));
    painter.fillRect(QRect(0, height - 4, width, 4), progress);
    return pixmap;
}

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

    StartupSplash splash(create_startup_pixmap());
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
