#include "ui/LanguageManager.h"
#include <QAction>
#include <QApplication>
#include <QHelpEvent>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <QToolTip>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    auto& language = LanguageManager::Instance();
    check(language.SetLanguage("English"), "Missing English catalog");
    QWidget window;
    window.resize(500, 500);
    QPushButton button(&window);
    button.setGeometry(10, 10, 42, 36);
    button.setProperty("toolKey", "BSplineCurve");
    button.setIcon(QIcon(":/icons/BSplineCurve.png"));
    QAction shortcut(&window);
    shortcut.setProperty("toolKey", "BSplineCurve");
    shortcut.setShortcut(QKeySequence("Ctrl+Alt+B"));
    window.show();
    app.processEvents();
    const auto hover = [&](QWidget& target, QPoint point = QPoint(5, 5)) {
        QToolTip::hideText();
        QHelpEvent event(QEvent::ToolTip, point, target.mapToGlobal(point));
        QApplication::sendEvent(&target, &event);
        app.processEvents();
    };
    hover(button);
    check(QToolTip::text().contains("control points"), "Missing authored hint");
    check(QToolTip::text().contains("data:image/png;base64,"), "Missing full-size icon");
    check(QToolTip::text().contains("Ctrl+Alt+B"), "Shortcut did not follow action");
    shortcut.setShortcut(QKeySequence("Ctrl+Alt+N"));
    hover(button);
    check(QToolTip::text().contains("Ctrl+Alt+N") && !QToolTip::text().contains("Ctrl+Alt+B"), "Stale shortcut");
    check(language.SetLanguage("Russian"), "Missing Russian catalog");
    hover(button);
    check(QToolTip::text().contains(QString::fromUtf8("управляющие точки")), "Language change did not update hint");
    if (argc > 1) for (auto* widget : app.topLevelWidgets())
        if (widget->objectName() == "qtooltip_label")
            check(widget->grab().save(QString::fromLocal8Bit(argv[1])), "Cannot save tooltip preview");
    check(language.SetLanguage("Spanish"), "Missing fallback language");
    hover(button);
    check(QToolTip::text().contains("control points"), "Missing English fallback");
    QToolButton toolbar(&window);
    toolbar.setDefaultAction(&shortcut);
    hover(toolbar);
    check(QToolTip::text().contains("control points"), "Toolbar action hint missing");
    QMenu menu;
    menu.addAction(&shortcut);
    menu.show();
    app.processEvents();
    hover(menu, menu.actionGeometry(&shortcut).center());
    check(QToolTip::text().contains("control points"), "Menu action hint missing");
    menu.hide();
    button.setProperty("toolKey", "NoAuthoredHint");
    button.setToolTip("Existing tooltip");
    hover(button);
    check(QToolTip::text() == "Existing tooltip", "Existing tooltip was lost");
    std::cout << "Tool hints: language, fallback, icons, shortcuts, buttons and menus passed\n";
}
