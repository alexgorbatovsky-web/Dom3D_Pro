#include "ThemeManager.h"
#include "LanguageManager.h"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <array>
#include <cmath>

namespace Themes {
namespace {
struct Field { const char* key; const char* label; QColor Scheme::*member; };
const std::array<Field, 7> fields{{
    {"window", "Panels", &Scheme::window}, {"base", "Input fields", &Scheme::base},
    {"button", "Buttons", &Scheme::button}, {"text", "Text", &Scheme::text},
    {"accent", "Accent", &Scheme::accent}, {"border", "Borders", &Scheme::border},
    {"activeButton", "Active buttons", &Scheme::activeButton}
}};
Scheme current;
QColor contrastingText(const QColor& color) {
    auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
    return luminance > .179 ? QColor("#101010") : QColor("#ffffff");
}
}

QVector<Scheme> Presets() {
    return {
        {"light", "Studio Light", QColor("#ededf0"), QColor("#ffffff"), QColor("#e2e3e8"), QColor("#242730"), QColor("#276ac4"), QColor("#a5a9b2"), QColor("#276ac4")},
        {"graphite", "Graphite", QColor("#242527"), QColor("#191a1c"), QColor("#343638"), QColor("#e2e4e6"), QColor("#009e78"), QColor("#515458"), QColor("#38c9a1")},
        {"midnight", "Midnight", QColor("#222a38"), QColor("#171e2a"), QColor("#303c50"), QColor("#e0e7f2"), QColor("#6ca8f5"), QColor("#4c5d75"), QColor("#82b8ff")},
        {"warm", "Warm Gray", QColor("#302d2a"), QColor("#24211f"), QColor("#423d37"), QColor("#eee7de"), QColor("#dca45e"), QColor("#62594f"), QColor("#efb96e")}
    };
}

QPalette Palette(const Scheme& s) {
    QPalette p;
    p.setColor(QPalette::Window, s.window);
    p.setColor(QPalette::Base, s.base);
    p.setColor(QPalette::AlternateBase, s.window);
    p.setColor(QPalette::Button, s.button);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::ToolTipText})
        p.setColor(role, s.text);
    p.setColor(QPalette::ToolTipBase, s.base);
    p.setColor(QPalette::Highlight, s.accent);
    p.setColor(QPalette::HighlightedText, contrastingText(s.accent));
    p.setColor(QPalette::Link, s.accent);
    p.setColor(QPalette::LinkVisited, s.accent);
    p.setColor(QPalette::Accent, s.accent);
    p.setColor(QPalette::Mid, s.border);
    p.setColor(QPalette::Dark, s.border.darker(125));
    p.setColor(QPalette::Shadow, s.base.darker(140));
    p.setColor(QPalette::Light, s.button.lighter(130));
    p.setColor(QPalette::Midlight, s.button.lighter(115));
    const QColor muted((s.text.red() + s.window.red()) / 2,
                       (s.text.green() + s.window.green()) / 2,
                       (s.text.blue() + s.window.blue()) / 2);
    p.setColor(QPalette::PlaceholderText, muted);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, muted);
    return p;
}

Scheme Load(QSettings& settings) {
    Scheme result = Presets().front();
    const QString id = settings.value("appearance/theme", "light").toString();
    for (const auto& preset : Presets()) if (preset.id == id) result = preset;
    for (const auto& field : fields) {
        const QColor color(settings.value(QString("appearance/colors/") + field.key).toString());
        if (color.isValid()) result.*(field.member) = color;
    }
    return result;
}

void Save(QSettings& settings, const Scheme& s) {
    settings.setValue("appearance/theme", s.id);
    for (const auto& field : fields)
        settings.setValue(QString("appearance/colors/") + field.key, (s.*(field.member)).name());
}

bool SaveFile(const QString& path, const Scheme& scheme, QString& error) {
    error.clear();
    QJsonObject colors;
    for (const auto& field : fields)
        colors[field.key] = (scheme.*(field.member)).name();
    const QByteArray data = QJsonDocument(QJsonObject{
        {"format", "Dom3D.Theme"}, {"version", 1},
        {"preset", scheme.id}, {"colors", colors}}).toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool LoadFile(const QString& path, Scheme& scheme, QString& error) {
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    if (file.size() > 65536) { error = DomTranslate("Invalid theme file."); return false; }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto object = doc.object();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || object["format"].toString() != "Dom3D.Theme"
        || object["version"].toDouble() != 1.0 || !object["colors"].isObject()) {
        error = DomTranslate("Invalid theme file."); return false;
    }
    Scheme loaded = Presets().front();
    for (const auto& preset : Presets())
        if (preset.id == object["preset"].toString()) loaded = preset;
    const auto colors = object["colors"].toObject();
    for (const auto& field : fields) {
        // Version 1 files saved before active-button colors remain compatible.
        if (field.member == &Scheme::activeButton && !colors.contains(field.key)) continue;
        const QColor color(colors[field.key].toString());
        if (!color.isValid() || color.alpha() != 255) {
            error = DomTranslate("Invalid theme file."); return false;
        }
        loaded.*(field.member) = color;
    }
    scheme = loaded;
    return true;
}

void Apply(const Scheme& s) {
    current = s;
    // Qt's stylesheet proxy caches the palette inherited by existing widgets.
    // Remove local sheets before changing the application palette, then rebuild
    // them against the new palette (including sheets inherited by dialogs).
    QVector<QPair<QPointer<QWidget>, QString>> sheets;
    for (QWidget* widget : QApplication::allWidgets()) {
        if (!widget->styleSheet().isEmpty())
            sheets.append({widget, widget->styleSheet()});
    }
    for (const auto& sheet : sheets)
        if (sheet.first) sheet.first->setStyleSheet({});
    qApp->setStyleSheet({});
    qApp->setPalette(Palette(s));
    const QColor active = s.activeButton.isValid() ? s.activeButton : s.accent;
    qApp->setStyleSheet(QString(
        "QPushButton[checkable=\"true\"], QToolButton[checkable=\"true\"] {"
        " background-color: palette(button); color: palette(button-text);"
        " border: 1px solid palette(mid); border-radius: 2px; padding: 3px 6px; }"
        "QPushButton[checkable=\"true\"]:enabled:hover, QToolButton[checkable=\"true\"]:enabled:hover {"
        " background-color: palette(midlight); }"
        "QPushButton:checked:enabled, QToolButton:checked:enabled {"
        " background-color: %1; color: %2;"
        " border: 1px solid %1; border-radius: 2px; }"
        "QPushButton:checked:enabled:hover, QToolButton:checked:enabled:hover {"
        " background-color: %3; color: %4; border-color: %2; }"
        "QPushButton:checked:enabled:pressed, QToolButton:checked:enabled:pressed {"
        " background-color: %5; color: %6; }"
        "QPushButton[checkable=\"true\"]:focus, QToolButton[checkable=\"true\"]:focus {"
        " border-color: palette(text); }"
        "QPushButton:checked:enabled:focus, QToolButton:checked:enabled:focus {"
        " border-color: %2; }")
        .arg(active.name(), contrastingText(active).name(),
             active.lighter(110).name(), contrastingText(active.lighter(110)).name(),
             active.darker(110).name(), contrastingText(active.darker(110)).name()));
    for (const auto& sheet : sheets)
        if (sheet.first) sheet.first->setStyleSheet(sheet.second);
}

void Initialize() {
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QSettings settings("Dom3D", "Dom3D_Pro");
    Apply(Load(settings));
}

void ShowDialog(QWidget* parent) {
    Scheme committed = current.id.isEmpty() ? Presets().front() : current;
    Scheme draft = committed;
    QDialog dialog(parent);
    dialog.setObjectName("ThemesDialog");
    dialog.setWindowTitle(DomTranslate("Themes"));
    dialog.setMinimumWidth(460);
    auto* root = new QVBoxLayout(&dialog);
    auto* description = new QLabel(DomTranslate("Choose a color scheme or customize its colors. Changes are previewed immediately."));
    description->setWordWrap(true);
    root->addWidget(description);
    auto* form = new QFormLayout;
    root->addLayout(form);
    auto* presets = new QComboBox;
    presets->setObjectName("ThemePresets");
    const auto schemes = Presets();
    for (const auto& s : schemes) presets->addItem(DomTranslate(s.name), s.id);
    presets->setCurrentIndex(presets->findData(draft.id));
    form->addRow(DomTranslate("Color scheme"), presets);
    std::array<QPushButton*, fields.size()> swatches{};
    auto refresh = [&]() {
        for (size_t i = 0; i < fields.size(); ++i) {
            const QColor color = draft.*(fields[i].member);
            swatches[i]->setText(color.name().toUpper());
            swatches[i]->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid palette(mid); padding: 7px; }")
                .arg(color.name(), contrastingText(color).name()));
        }
        Apply(draft);
    };
    for (size_t i = 0; i < fields.size(); ++i) {
        auto* button = new QPushButton;
        swatches[i] = button;
        button->setObjectName(QString("ThemeColor_") + fields[i].key);
        form->addRow(DomTranslate(fields[i].label), button);
        QObject::connect(button, &QPushButton::clicked, &dialog, [&, i]() {
            const QColor color = QColorDialog::getColor(draft.*(fields[i].member), &dialog, DomTranslate(fields[i].label));
            if (color.isValid()) { draft.*(fields[i].member) = color; refresh(); }
        });
    }
    QObject::connect(presets, &QComboBox::currentIndexChanged, &dialog, [&](int index) {
        if (index >= 0) { draft = schemes[index]; refresh(); }
    });
    auto* reset = new QPushButton(DomTranslate("Reset scheme colors"));
    root->addWidget(reset);
    QObject::connect(reset, &QPushButton::clicked, &dialog, [&]() {
        draft = schemes[presets->currentIndex()]; refresh();
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    auto* save = buttons->addButton(DomTranslate("Save"), QDialogButtonBox::ActionRole);
    auto* load = buttons->addButton(DomTranslate("Load"), QDialogButtonBox::ActionRole);
    save->setObjectName("SaveThemeFile");
    load->setObjectName("LoadThemeFile");
    save->setToolTip(DomTranslate("Save theme to file"));
    load->setToolTip(DomTranslate("Load theme from file"));
    QObject::connect(save, &QPushButton::clicked, &dialog, [&]() {
        const QString path = QFileDialog::getSaveFileName(&dialog, DomTranslate("Save theme to file"),
            draft.id + ".dom3dtheme", "Dom3D Theme (*.dom3dtheme)");
        if (path.isEmpty()) return;
        QString error;
        if (!SaveFile(path, draft, error))
            QMessageBox::warning(&dialog, DomTranslate("Save theme to file"), error);
    });
    QObject::connect(load, &QPushButton::clicked, &dialog, [&]() {
        const QString path = QFileDialog::getOpenFileName(&dialog, DomTranslate("Load theme from file"),
            {}, "Dom3D Theme (*.dom3dtheme)");
        if (path.isEmpty()) return;
        QString error;
        if (!LoadFile(path, draft, error)) {
            QMessageBox::warning(&dialog, DomTranslate("Load theme from file"), error);
            return;
        }
        const QSignalBlocker blocker(presets);
        presets->setCurrentIndex(presets->findData(draft.id));
        refresh();
    });
    root->addWidget(buttons);
    auto commit = [&]() {
        committed = draft;
        QSettings settings("Dom3D", "Dom3D_Pro");
        Save(settings, committed);
    };
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, commit);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() { commit(); dialog.accept(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    refresh();
    dialog.exec();
    Apply(committed);
}
}
