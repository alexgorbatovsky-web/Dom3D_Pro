#include "LanguageManager.h"

#include <QAction>
#include <QAbstractButton>
#include <QActionEvent>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTabBar>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>
#include <QWidget>
#include <QXmlStreamReader>

#include <algorithm>

namespace {
constexpr auto kTextIdProperty = "dom3dTextId";
constexpr auto kTextFallbackProperty = "dom3dTextFallback";
constexpr auto kToolTipIdProperty = "dom3dToolTipId";
constexpr auto kToolTipFallbackProperty = "dom3dToolTipFallback";
constexpr auto kTranslationGenerationProperty = "dom3dTranslationGeneration";
constexpr auto kTranslationFingerprintProperty = "dom3dTranslationFingerprint";

void append_fingerprint(QString& fingerprint, const QString& value) {
    fingerprint += value;
    fingerprint += QChar(0x1f);
}

QString object_text_fingerprint(QObject* object) {
    QString fingerprint;
    if (auto* action = qobject_cast<QAction*>(object)) {
        append_fingerprint(fingerprint, action->text());
        append_fingerprint(fingerprint, action->toolTip());
        append_fingerprint(fingerprint, action->statusTip());
    }
    if (auto* widget = qobject_cast<QWidget*>(object)) {
        append_fingerprint(fingerprint, widget->windowTitle());
        append_fingerprint(fingerprint, widget->toolTip());
        append_fingerprint(fingerprint, widget->statusTip());
    }
    if (auto* label = qobject_cast<QLabel*>(object)) {
        append_fingerprint(fingerprint, label->text());
    } else if (auto* button = qobject_cast<QAbstractButton*>(object)) {
        append_fingerprint(fingerprint, button->text());
    } else if (auto* group = qobject_cast<QGroupBox*>(object)) {
        append_fingerprint(fingerprint, group->title());
    }
    if (auto* edit = qobject_cast<QLineEdit*>(object)) {
        append_fingerprint(fingerprint, edit->placeholderText());
    }
    if (auto* tabs = qobject_cast<QTabBar*>(object)) {
        for (int index = 0; index < tabs->count(); ++index) {
            append_fingerprint(fingerprint, tabs->tabText(index));
            append_fingerprint(fingerprint, tabs->tabToolTip(index));
        }
    }
    if (auto* combo = qobject_cast<QComboBox*>(object)) {
        for (int index = 0; index < combo->count(); ++index) {
            append_fingerprint(fingerprint, combo->itemText(index));
        }
    }
    if (auto* tree = qobject_cast<QTreeWidget*>(object)) {
        if (QTreeWidgetItem* header = tree->headerItem()) {
            for (int column = 0; column < header->columnCount(); ++column) {
                append_fingerprint(fingerprint, header->text(column));
            }
        }
    }
    if (auto* menu = qobject_cast<QMenu*>(object)) {
        for (QAction* action : menu->actions()) {
            append_fingerprint(fingerprint, action->text());
            append_fingerprint(fingerprint, action->toolTip());
            append_fingerprint(fingerprint, action->statusTip());
        }
    }
    return fingerprint;
}

void set_widget_text(QWidget* widget, const QString& text) {
    if (auto* label = qobject_cast<QLabel*>(widget)) {
        label->setText(text);
    } else if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
        button->setText(text);
    } else if (auto* group = qobject_cast<QGroupBox*>(widget)) {
        group->setTitle(text);
    } else {
        widget->setWindowTitle(text);
    }
}
}

LanguageManager& LanguageManager::Instance() {
    static LanguageManager manager;
    return manager;
}

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent) {
    const QString english_path = QDir(LanguagesPath()).filePath("English.xml");
    LoadFile(english_path, english_values_);
    QSettings settings("Dom3D", "Dom3D_Pro");
    const QString saved = settings.value("ui/language", "English").toString();
    if (!SetLanguage(saved)) {
        SetLanguage("English");
    }
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

QString LanguageManager::LanguagesPath() const {
    return QDir(QCoreApplication::applicationDirPath()).filePath("Languages");
}

bool LanguageManager::LoadFile(const QString& path,
                               QHash<QString, QString>& values) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QDomDocument document;
    if (!document.setContent(&file)) {
        return false;
    }
    const QDomNodeList items = document.elementsByTagName("TextItem");
    for (int i = 0; i < items.size(); ++i) {
        const QDomElement item = items.at(i).toElement();
        const QString id = item.firstChildElement("ID").text().trimmed();
        const QString text = item.firstChildElement("Text").text();
        if (!id.isEmpty()) {
            values.insert(id, text);
        }
    }
    return true;
}

QString LanguageManager::Text(const QString& id, const QString& fallback) const {
    const auto found = values_.constFind(id);
    return found == values_.constEnd() || found->isEmpty() ? fallback : *found;
}

QString LanguageManager::Translate(const QString& source) const {
    if (source.isEmpty()) {
        return source;
    }
    const auto exact = source_ids_.constFind(source);
    if (exact != source_ids_.constEnd()) {
        return values_.value(*exact, english_values_.value(*exact, source));
    }

    static const QRegularExpression placeholder("%([1-9][0-9]*)");
    for (const TextTemplate& item : templates_) {
        const auto match = item.expression.match(source);
        if (!match.hasMatch()) {
            continue;
        }
        QString translated = values_.value(
            item.id, english_values_.value(item.id, item.source));
        auto translated_placeholders = placeholder.globalMatch(translated);
        QVector<QPair<QString, QString>> replacements;
        while (translated_placeholders.hasNext()) {
            const auto translated_match = translated_placeholders.next();
            const int number = translated_match.captured(1).toInt();
            const int capture_number = item.capture_for_placeholder.value(number);
            if (capture_number > 0) {
                replacements.push_back({
                    translated_match.captured(0), match.captured(capture_number)});
            }
        }
        std::sort(replacements.begin(), replacements.end(),
                  [](const auto& left, const auto& right) {
                      return left.first.size() > right.first.size();
                  });
        for (const auto& replacement : replacements) {
            translated.replace(replacement.first, replacement.second);
        }
        return translated;
    }
    return source;
}

QString LanguageManager::CurrentLanguage() const {
    return current_language_;
}

QStringList LanguageManager::AvailableLanguages() const {
    QDir directory(LanguagesPath());
    QStringList result;
    for (const QFileInfo& file : directory.entryInfoList(
             {"*.xml"}, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        result.push_back(file.completeBaseName());
    }
    return result;
}

QString LanguageManager::LanguageDisplayName(const QString& file_stem) const {
    const QString path = QDir(LanguagesPath()).filePath(file_stem + ".xml");
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QXmlStreamReader xml(&file);
        while (xml.readNextStartElement()) {
            if (xml.name() != QStringView(u"ClassArray.TextItem")) {
                xml.skipCurrentElement();
                continue;
            }
            while (xml.readNextStartElement()) {
                if (xml.name() != QStringView(u"TextItem")) {
                    xml.skipCurrentElement();
                    continue;
                }
                QString id;
                QString text;
                while (xml.readNextStartElement()) {
                    if (xml.name() == QStringView(u"ID")) {
                        id = xml.readElementText().trimmed();
                    } else if (xml.name() == QStringView(u"Text")) {
                        text = xml.readElementText();
                    } else {
                        xml.skipCurrentElement();
                    }
                }
                if (id == "LanguageName" && !text.trimmed().isEmpty()) {
                    return text.trimmed();
                }
            }
        }
    }
    return file_stem;
}

bool LanguageManager::SetLanguage(const QString& file_stem) {
    QHash<QString, QString> next_values;
    const QString path = QDir(LanguagesPath()).filePath(file_stem + ".xml");
    if (!LoadFile(path, next_values)) {
        return false;
    }
    const QHash<QString, QString> previous_values = values_;
    current_language_ = file_stem;
    values_ = std::move(next_values);
    if (english_values_.isEmpty()) {
        LoadFile(QDir(LanguagesPath()).filePath("English.xml"), english_values_);
    }
    RebuildSourceIndex(previous_values);
    ++translation_generation_;
    QSettings settings("Dom3D", "Dom3D_Pro");
    settings.setValue("ui/language", current_language_);
    emit LanguageChanged();
    QTimer::singleShot(0, this, [this]() {
        if (!qApp) return;
        for (QWidget* widget : qApp->topLevelWidgets()) {
            Apply(widget);
        }
    });
    return true;
}

void LanguageManager::RebuildSourceIndex(
    const QHash<QString, QString>& previous_values) {
    source_ids_.clear();
    templates_.clear();
    QSet<QString> template_sources;
    const auto add_catalog = [this, &template_sources](
        const QHash<QString, QString>& catalog) {
        static const QRegularExpression placeholder("%([1-9][0-9]*)");
        for (auto it = catalog.constBegin(); it != catalog.constEnd(); ++it) {
            // Language names are proper names in the selector, not UI source
            // strings.  Translating them would make every language action use
            // the name of the currently selected language.
            if (it.key() != "LanguageName" && !it.value().isEmpty()) {
                source_ids_.insert(it.value(), it.key());
                if (it.value().contains(placeholder)
                    && !template_sources.contains(it.value())) {
                    TextTemplate item;
                    item.id = it.key();
                    item.source = it.value();
                    QString pattern = "^";
                    int capture = 0;
                    int offset = 0;
                    auto matches = placeholder.globalMatch(item.source);
                    while (matches.hasNext()) {
                        const auto match = matches.next();
                        pattern += QRegularExpression::escape(
                            item.source.mid(
                                offset, match.capturedStart() - offset));
                        pattern += "(.*?)";
                        item.capture_for_placeholder.insert(
                            match.captured(1).toInt(), ++capture);
                        offset = match.capturedEnd();
                    }
                    pattern += QRegularExpression::escape(
                        item.source.mid(offset)) + "$";
                    item.expression = QRegularExpression(
                        pattern,
                        QRegularExpression::DotMatchesEverythingOption);
                    templates_.push_back(std::move(item));
                    template_sources.insert(it.value());
                }
            }
        }
    };
    add_catalog(english_values_);
    add_catalog(previous_values);
    add_catalog(values_);
    std::sort(templates_.begin(), templates_.end(),
              [](const TextTemplate& left, const TextTemplate& right) {
                  return left.source.size() > right.source.size();
              });
}

void LanguageManager::TranslateObject(QObject* object) const {
    if (!object || translating_) return;
    const QString input_fingerprint = object_text_fingerprint(object);
    if (object->property(kTranslationGenerationProperty).toULongLong()
            == translation_generation_
        && object->property(kTranslationFingerprintProperty).toString()
            == input_fingerprint) {
        return;
    }
    translating_ = true;
    const auto set_if_changed = [](QString current, const QString& translated,
                                   const auto& setter) {
        if (current != translated) setter(translated);
    };

    if (auto* action = qobject_cast<QAction*>(object)) {
        set_if_changed(action->text(), Translate(action->text()),
                       [action](const QString& text) { action->setText(text); });
        set_if_changed(action->toolTip(), Translate(action->toolTip()),
                       [action](const QString& text) { action->setToolTip(text); });
        set_if_changed(action->statusTip(), Translate(action->statusTip()),
                       [action](const QString& text) { action->setStatusTip(text); });
    }
    if (auto* widget = qobject_cast<QWidget*>(object)) {
        set_if_changed(widget->windowTitle(), Translate(widget->windowTitle()),
                       [widget](const QString& text) { widget->setWindowTitle(text); });
        set_if_changed(widget->toolTip(), Translate(widget->toolTip()),
                       [widget](const QString& text) { widget->setToolTip(text); });
        set_if_changed(widget->statusTip(), Translate(widget->statusTip()),
                       [widget](const QString& text) { widget->setStatusTip(text); });
    }
    if (auto* label = qobject_cast<QLabel*>(object)) {
        set_if_changed(label->text(), Translate(label->text()),
                       [label](const QString& text) { label->setText(text); });
    } else if (auto* button = qobject_cast<QAbstractButton*>(object)) {
        set_if_changed(button->text(), Translate(button->text()),
                       [button](const QString& text) { button->setText(text); });
    } else if (auto* group = qobject_cast<QGroupBox*>(object)) {
        set_if_changed(group->title(), Translate(group->title()),
                       [group](const QString& text) { group->setTitle(text); });
    }
    if (auto* edit = qobject_cast<QLineEdit*>(object)) {
        set_if_changed(edit->placeholderText(), Translate(edit->placeholderText()),
                       [edit](const QString& text) { edit->setPlaceholderText(text); });
    }
    if (auto* tabs = qobject_cast<QTabBar*>(object)) {
        for (int index = 0; index < tabs->count(); ++index) {
            const QString translated = Translate(tabs->tabText(index));
            if (translated != tabs->tabText(index)) tabs->setTabText(index, translated);
            const QString translated_tip = Translate(tabs->tabToolTip(index));
            if (translated_tip != tabs->tabToolTip(index)) {
                tabs->setTabToolTip(index, translated_tip);
            }
        }
    }
    if (auto* combo = qobject_cast<QComboBox*>(object)) {
        for (int index = 0; index < combo->count(); ++index) {
            const QString translated = Translate(combo->itemText(index));
            if (translated != combo->itemText(index)) combo->setItemText(index, translated);
        }
    }
    if (auto* tree = qobject_cast<QTreeWidget*>(object)) {
        if (QTreeWidgetItem* header = tree->headerItem()) {
            for (int column = 0; column < header->columnCount(); ++column) {
                header->setText(column, Translate(header->text(column)));
            }
        }
    }
    if (auto* menu = qobject_cast<QMenu*>(object)) {
        for (QAction* action : menu->actions()) {
            set_if_changed(action->text(), Translate(action->text()),
                           [action](const QString& text) { action->setText(text); });
            set_if_changed(action->toolTip(), Translate(action->toolTip()),
                           [action](const QString& text) { action->setToolTip(text); });
            set_if_changed(action->statusTip(), Translate(action->statusTip()),
                           [action](const QString& text) { action->setStatusTip(text); });
        }
    }
    object->setProperty(
        kTranslationGenerationProperty,
        QVariant::fromValue<qulonglong>(translation_generation_));
    object->setProperty(
        kTranslationFingerprintProperty,
        object_text_fingerprint(object));
    translating_ = false;
}

bool LanguageManager::eventFilter(QObject* watched, QEvent* event) {
    if (!event || translating_) return QObject::eventFilter(watched, event);
    switch (event->type()) {
        case QEvent::Paint:
        case QEvent::Show:
        case QEvent::Polish:
        case QEvent::ActionChanged:
            TranslateObject(watched);
            break;
        case QEvent::ChildAdded: {
            auto* child_event = static_cast<QChildEvent*>(event);
            QPointer<QObject> child(child_event->child());
            QTimer::singleShot(0, this, [this, child]() {
                if (child) TranslateObject(child);
            });
            break;
        }
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void LanguageManager::BindText(QAction* action,
                               const QString& id,
                               const QString& fallback) {
    action->setProperty(kTextIdProperty, id);
    action->setProperty(kTextFallbackProperty, fallback);
    action->setText(Text(id, fallback));
}

void LanguageManager::BindText(QMenu* menu,
                               const QString& id,
                               const QString& fallback) {
    BindText(menu->menuAction(), id, fallback);
}

void LanguageManager::BindText(QWidget* widget,
                               const QString& id,
                               const QString& fallback) {
    widget->setProperty(kTextIdProperty, id);
    widget->setProperty(kTextFallbackProperty, fallback);
    set_widget_text(widget, Text(id, fallback));
}

void LanguageManager::BindToolTip(QWidget* widget,
                                  const QString& id,
                                  const QString& fallback) {
    widget->setProperty(kToolTipIdProperty, id);
    widget->setProperty(kToolTipFallbackProperty, fallback);
    widget->setToolTip(Text(id, fallback));
}

void LanguageManager::Apply(QObject* root) const {
    if (!root) return;
    QList<QObject*> objects;
    objects.push_back(root);
    objects.append(root->findChildren<QObject*>());
    for (QObject* object : objects) {
        TranslateObject(object);
        const QString text_id = object->property(kTextIdProperty).toString();
        if (!text_id.isEmpty()) {
            const QString fallback = object->property(kTextFallbackProperty).toString();
            if (auto* action = qobject_cast<QAction*>(object)) {
                action->setText(Text(text_id, fallback));
            } else if (auto* widget = qobject_cast<QWidget*>(object)) {
                set_widget_text(widget, Text(text_id, fallback));
            }
        }
        const QString tooltip_id = object->property(kToolTipIdProperty).toString();
        if (!tooltip_id.isEmpty()) {
            if (auto* widget = qobject_cast<QWidget*>(object)) {
                widget->setToolTip(Text(
                    tooltip_id,
                    object->property(kToolTipFallbackProperty).toString()));
            }
        }
    }
}
