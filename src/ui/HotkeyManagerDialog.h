#pragma once

#include <QDialog>
#include <QKeySequence>

#include <vector>

class QAction;
class QMainWindow;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class HotkeyManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit HotkeyManagerDialog(QMainWindow* main_window,
                                 QWidget* parent = nullptr);

    // Registers all permanent menu and toolbar actions, remembers their
    // defaults and applies user overrides from QSettings.
    static void InitializeActions(QMainWindow* main_window);

private:
    struct Entry {
        QAction* action = nullptr;
        QString id;
        QString command;
        QString category;
        QKeySequence default_shortcut;
        QKeySequence shortcut;
        QTreeWidgetItem* item = nullptr;
    };

    void Populate();
    void ApplyFilter(const QString& text);
    void AssignShortcut(QTreeWidgetItem* item, const QKeySequence& shortcut);
    void RestoreDefaults();
    void SaveAndApply();

    QMainWindow* main_window_ = nullptr;
    QLineEdit* search_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    std::vector<Entry> entries_;
};
