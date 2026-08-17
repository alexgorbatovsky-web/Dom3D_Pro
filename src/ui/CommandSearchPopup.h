#pragma once

#include <QDialog>
#include <QPoint>

#include <vector>

class QAction;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMainWindow;

class CommandSearchPopup final : public QDialog {
    Q_OBJECT

public:
    static void Show(QMainWindow* main_window);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Command {
        QAction* action = nullptr;
        QString label;
        QString search_text;
    };

    explicit CommandSearchPopup(QMainWindow* main_window);
    void UpdateResults(const QString& text);
    void ExecuteItem(QListWidgetItem* item);
    void ExecuteFirstResult();
    void UpdatePopupSize();
    void RepositionNearCursor();

    QMainWindow* main_window_ = nullptr;
    QLineEdit* search_ = nullptr;
    QListWidget* results_ = nullptr;
    std::vector<Command> commands_;
    QPoint cursor_anchor_;
};
