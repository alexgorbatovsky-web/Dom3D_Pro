#include "ui/ThemeManager.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    // The offscreen platform does not enumerate the Windows font database.
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
#endif
    app.setFont(QFont("Segoe UI", 10));
    QTemporaryDir directory;
    require(directory.isValid(), "Temporary settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QSettings settings("Dom3D", "Dom3D_Pro");
    const auto presets = Themes::Presets();
    require(presets.size() == 4, "Four presets");
    for (auto scheme : presets) {
        scheme.accent = QColor("#abcdef");
        Themes::Save(settings, scheme);
        const auto loaded = Themes::Load(settings);
        require(loaded.id == scheme.id && loaded.accent == scheme.accent && loaded.base == scheme.base, "Settings round trip");
    }
    settings.clear();
    const QString themePath = directory.filePath("custom.dom3dtheme");
    QString error;
    auto custom = presets[1];
    custom.accent = QColor("#04259e");
    require(Themes::SaveFile(themePath, custom, error), "Export custom theme");
    auto imported = presets[0];
    require(Themes::LoadFile(themePath, imported, error), "Import custom theme");
    require(imported.id == custom.id && Themes::Palette(imported) == Themes::Palette(custom), "All exported colors round trip");
    require(!Themes::SaveFile(directory.path(), custom, error) && !error.isEmpty(), "Export errors reported");
    require(!Themes::LoadFile(directory.filePath("missing"), imported, error), "Missing file rejected");
    for (const QByteArray invalid : {QByteArray("not json"),
            QByteArray("{\"format\":\"Dom3D.Theme\",\"version\":2,\"colors\":{}}"),
            QByteArray("{\"format\":\"Dom3D.Theme\",\"version\":1,\"colors\":{\"window\":\"#123456\"}}")}) {
        QFile file(themePath);
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Create invalid theme fixture");
        file.write(invalid); file.close();
        require(!Themes::LoadFile(themePath, imported, error) && !error.isEmpty(), "Invalid theme rejected");
        require(Themes::Palette(imported) == Themes::Palette(custom), "Failed import leaves colors intact");
    }
    settings.setValue("appearance/theme", "unknown");
    settings.setValue("appearance/colors/text", "invalid");
    require(Themes::Load(settings).text == presets[0].text, "Corrupt settings fallback");
    settings.clear();
    Themes::Initialize();
    QMainWindow window;
    window.setStyleSheet("QMainWindow::separator { width: 1px; }");
    auto* menu = window.menuBar()->addMenu("Edit");
    menu->addAction("Themes...");
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    auto* label = new QLabel("Scene Tree", panel);
    auto* tree = new QTreeWidget(panel);
    tree->setHeaderLabel("Object");
    tree->addTopLevelItem(new QTreeWidgetItem({"Default"}));
    auto* button = new QPushButton("Tool", panel);
    button->setStyleSheet("QPushButton { background: palette(button); color: palette(button-text); }");
    layout->addWidget(label); layout->addWidget(tree); layout->addWidget(button);
    window.setCentralWidget(panel);
    window.show();
    app.processEvents();
    const QPalette initial = app.palette();
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget());
        require(dialog != nullptr, "Themes dialog opens");
        dialog->findChild<QComboBox*>("ThemePresets")->setCurrentIndex(1);
        require(app.palette().color(QPalette::Window) == presets[1].window, "Live preview");
        app.processEvents();
        require(dialog->findChild<QLabel*>()->palette().color(QPalette::WindowText) == presets[1].text, "Existing dialog text updates");
        require(label->palette().color(QPalette::WindowText) == presets[1].text, "Existing panel text updates");
        require(tree->palette().color(QPalette::Base) == presets[1].base, "Existing tree background updates");
        require(button->palette().color(QPalette::Button) == presets[1].button, "Stylesheet button updates");
        require(window.menuBar()->palette().color(QPalette::WindowText) == presets[1].text, "Menu bar text updates");
        require(menu->palette().color(QPalette::WindowText) == presets[1].text, "Popup menu text updates");
        dialog->reject();
    });
    Themes::ShowDialog(&window);
    require(app.palette() == initial, "Cancel restores palette");
    require(!settings.contains("appearance/theme"), "Preview does not persist");
    require(label->palette().color(QPalette::WindowText) == presets[0].text, "Cancel restores existing text");
    for (const auto& scheme : presets) {
        Themes::Apply(scheme);
        app.processEvents();
        require(label->palette().color(QPalette::WindowText) == scheme.text, "Repeated switching updates text");
        require(button->palette().color(QPalette::Button) == scheme.button, "Repeated switching updates styled controls");
        QLabel createdLater("New dialog label", &window);
        createdLater.ensurePolished();
        require(createdLater.palette().color(QPalette::WindowText) == scheme.text, "New widgets inherit current theme");
    }
    Themes::Apply(presets[0]);
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget());
        auto* combo = dialog->findChild<QComboBox*>("ThemePresets");
        combo->setCurrentIndex(2);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        combo->setCurrentIndex(3);
        dialog->reject();
    });
    Themes::ShowDialog(nullptr);
    require(app.palette().color(QPalette::Window) == presets[2].window, "Cancel preserves last Apply");
    require(Themes::Load(settings).id == presets[2].id, "Apply persists");
    Themes::Apply(presets[0]);
    Themes::Initialize();
    require(app.palette().color(QPalette::Window) == presets[2].window, "Startup restores theme");
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget());
        dialog->findChild<QComboBox*>("ThemePresets")->setCurrentIndex(1);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    Themes::ShowDialog(nullptr);
    require(Themes::Load(settings).id == presets[1].id, "OK persists");
    const QString capturePath = qEnvironmentVariable("DOM3D_THEME_PREVIEWS");
    if (!capturePath.isEmpty()) {
        QDir().mkpath(capturePath);
        for (const auto& scheme : presets) {
            Themes::Apply(presets[0]);
            QTimer::singleShot(50, [&]() {
                auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget());
                auto* combo = dialog->findChild<QComboBox*>("ThemePresets");
                combo->setCurrentIndex(combo->findData(scheme.id));
                app.processEvents();
                require(dialog->grab().save(capturePath + "/" + scheme.id + ".png"), "Save theme preview");
                require(window.grab().save(capturePath + "/" + scheme.id + "-window.png"), "Save parent window preview");
                dialog->reject();
            });
            Themes::ShowDialog(&window);
        }
    }
    std::cout << "Theme tests passed\n";
}
