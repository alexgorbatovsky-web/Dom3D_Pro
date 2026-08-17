#include "HotkeyManagerDialog.h"

#include <QAction>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <vector>

namespace {
constexpr auto kHotkeyId = "dom3dHotkeyId";
constexpr auto kHotkeyCategory = "dom3dHotkeyCategory";
constexpr auto kDefaultShortcut = "dom3dDefaultShortcut";

QString clean_text(QString text) {
    text.remove('&');
    while (text.endsWith('.')) {
        text.chop(1);
    }
    return text.trimmed();
}

QString id_part(const QString& text) {
    QString result = clean_text(text).toLower();
    result.replace(QRegularExpression("[^a-z0-9]+"), "_");
    result.remove(QRegularExpression("^_+|_+$"));
    return result.isEmpty() ? QStringLiteral("command") : result;
}

class HotkeyTree final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    std::function<void(QTreeWidgetItem*, const QKeySequence&)> shortcut_pressed;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        QTreeWidgetItem* item = currentItem();
        if (!item || !shortcut_pressed) {
            QTreeWidget::keyPressEvent(event);
            return;
        }
        if ((event->key() == Qt::Key_Backspace
             || event->key() == Qt::Key_Delete)
            && event->modifiers() == Qt::NoModifier) {
            shortcut_pressed(item, {});
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Control
            || event->key() == Qt::Key_Shift
            || event->key() == Qt::Key_Alt
            || event->key() == Qt::Key_Meta
            || event->key() == Qt::Key_Escape) {
            event->accept();
            return;
        }
        const QKeyCombination combination(
            event->modifiers(), static_cast<Qt::Key>(event->key()));
        shortcut_pressed(item, QKeySequence(combination));
        event->accept();
    }
};
}

void HotkeyManagerDialog::InitializeActions(QMainWindow* main_window) {
    if (!main_window || !main_window->menuBar()) {
        return;
    }

    QSettings settings;
    settings.beginGroup("HotKeys");
    QSet<QString> used_ids;
    QSet<QAction*> registered;

    const auto register_action = [&](QAction* action,
                                     const QString& category,
                                     QString id) {
        if (!action || action->isSeparator() || clean_text(action->text()).isEmpty()
            || registered.contains(action)) {
            return;
        }
        const QString base_id = id;
        int suffix = 2;
        while (used_ids.contains(id)) {
            id = base_id + '_' + QString::number(suffix++);
        }
        used_ids.insert(id);
        registered.insert(action);
        action->setProperty(kHotkeyId, id);
        action->setProperty(kHotkeyCategory, clean_text(category));
        action->setProperty(
            kDefaultShortcut,
            action->shortcut().toString(QKeySequence::PortableText));
        if (settings.contains(id)) {
            action->setShortcut(QKeySequence::fromString(
                settings.value(id).toString(), QKeySequence::PortableText));
        }
    };

    std::function<void(QMenu*, const QString&, const QString&)> collect_menu;
    collect_menu = [&](QMenu* menu, const QString& category,
                       const QString& id_prefix) {
        if (!menu) {
            return;
        }
        for (QAction* action : menu->actions()) {
            if (!action || action->isSeparator()) {
                continue;
            }
            const QString text = clean_text(action->text());
            if (action->menu()) {
                collect_menu(
                    action->menu(),
                    category + " / " + text,
                    id_prefix + '/' + id_part(text));
            } else {
                register_action(
                    action, category, id_prefix + '/' + id_part(text));
            }
        }
    };

    for (QAction* menu_action : main_window->menuBar()->actions()) {
        if (!menu_action || !menu_action->menu()) {
            continue;
        }
        const QString category = clean_text(menu_action->text());
        collect_menu(menu_action->menu(), category, id_part(category));
    }

    // Include permanent toolbar-only actions, such as selection modes.
    for (QAction* action : main_window->findChildren<QAction*>()) {
        if (!action || action->menu() || registered.contains(action)
            || action->isSeparator() || clean_text(action->text()).isEmpty()) {
            continue;
        }
        register_action(
            action, "Toolbar", "toolbar/" + id_part(action->text()));
    }
    settings.endGroup();
}

HotkeyManagerDialog::HotkeyManagerDialog(QMainWindow* main_window,
                                         QWidget* parent)
    : QDialog(parent), main_window_(main_window) {
    setWindowTitle("Hot Keys Manager");
    resize(780, 620);

    auto* layout = new QVBoxLayout(this);
    auto* instructions = new QLabel(
        "Select a command and press a key combination to redefine it. "
        "Press Backspace/Delete to remove the hot key.", this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);

    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search commands...");
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);

    auto* hotkey_tree = new HotkeyTree(this);
    tree_ = hotkey_tree;
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({"Command", "Category", "Hot Key"});
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setSortingEnabled(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(tree_, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
            | QDialogButtonBox::RestoreDefaults,
        this);
    layout->addWidget(buttons);

    Populate();
    hotkey_tree->shortcut_pressed = [this](QTreeWidgetItem* item,
                                           const QKeySequence& shortcut) {
        AssignShortcut(item, shortcut);
    };
    connect(search_, &QLineEdit::textChanged,
            this, &HotkeyManagerDialog::ApplyFilter);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        SaveAndApply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),
            &QPushButton::clicked,
            this, &HotkeyManagerDialog::RestoreDefaults);
}

void HotkeyManagerDialog::Populate() {
    entries_.clear();
    tree_->clear();
    if (!main_window_) {
        return;
    }
    for (QAction* action : main_window_->findChildren<QAction*>()) {
        const QString id = action ? action->property(kHotkeyId).toString() : QString();
        if (!action || id.isEmpty()) {
            continue;
        }
        Entry entry;
        entry.action = action;
        entry.id = id;
        entry.command = clean_text(action->text());
        entry.category = action->property(kHotkeyCategory).toString();
        entry.default_shortcut = QKeySequence::fromString(
            action->property(kDefaultShortcut).toString(),
            QKeySequence::PortableText);
        entry.shortcut = action->shortcut();
        entry.item = new QTreeWidgetItem({
            entry.command,
            entry.category,
            entry.shortcut.toString(QKeySequence::NativeText)});
        entry.item->setData(0, Qt::UserRole, static_cast<int>(entries_.size()));
        tree_->addTopLevelItem(entry.item);
        entries_.push_back(std::move(entry));
    }
    tree_->sortItems(0, Qt::AscendingOrder);
    if (tree_->topLevelItemCount() > 0) {
        tree_->setCurrentItem(tree_->topLevelItem(0));
    }
}

void HotkeyManagerDialog::ApplyFilter(const QString& text) {
    const QString needle = text.trimmed();
    for (Entry& entry : entries_) {
        const bool match = needle.isEmpty()
            || entry.command.contains(needle, Qt::CaseInsensitive)
            || entry.category.contains(needle, Qt::CaseInsensitive)
            || entry.shortcut.toString(QKeySequence::NativeText)
                .contains(needle, Qt::CaseInsensitive);
        entry.item->setHidden(!match);
    }
}

void HotkeyManagerDialog::AssignShortcut(
    QTreeWidgetItem* item, const QKeySequence& shortcut) {
    if (!item) {
        return;
    }
    const int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= static_cast<int>(entries_.size())) {
        return;
    }
    if (!shortcut.isEmpty()) {
        const auto conflict = std::find_if(
            entries_.begin(), entries_.end(),
            [index, &shortcut, this](const Entry& entry) {
                return &entry != &entries_[static_cast<size_t>(index)]
                    && entry.shortcut == shortcut;
            });
        if (conflict != entries_.end()) {
            const auto answer = QMessageBox::question(
                this,
                "Hot Key Conflict",
                QString("%1 is already assigned to ‘%2’.\n\n"
                        "Reassign it to ‘%3’?")
                    .arg(shortcut.toString(QKeySequence::NativeText),
                         conflict->command,
                         entries_[static_cast<size_t>(index)].command));
            if (answer != QMessageBox::Yes) {
                return;
            }
            conflict->shortcut = {};
            conflict->item->setText(2, {});
        }
    }
    Entry& entry = entries_[static_cast<size_t>(index)];
    entry.shortcut = shortcut;
    entry.item->setText(2, shortcut.toString(QKeySequence::NativeText));
}

void HotkeyManagerDialog::RestoreDefaults() {
    for (Entry& entry : entries_) {
        entry.shortcut = entry.default_shortcut;
        entry.item->setText(
            2, entry.shortcut.toString(QKeySequence::NativeText));
    }
    ApplyFilter(search_->text());
}

void HotkeyManagerDialog::SaveAndApply() {
    QSettings settings;
    settings.beginGroup("HotKeys");
    for (const Entry& entry : entries_) {
        if (!entry.action) {
            continue;
        }
        entry.action->setShortcut(entry.shortcut);
        if (entry.shortcut == entry.default_shortcut) {
            settings.remove(entry.id);
        } else {
            settings.setValue(
                entry.id,
                entry.shortcut.toString(QKeySequence::PortableText));
        }
    }
    settings.endGroup();
}
