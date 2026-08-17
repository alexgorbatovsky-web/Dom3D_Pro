#include "CommandSearchPopup.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString clean_text(QString text) {
    text.remove('&');
    while (text.endsWith('.')) {
        text.chop(1);
    }
    return text.trimmed();
}

QIcon search_icon() {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(185, 192, 200), 2.0,
                        Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(4.0, 4.0, 10.0, 10.0));
    painter.drawLine(QPointF(12.0, 12.0), QPointF(19.0, 19.0));
    return QIcon(pixmap);
}
}

void CommandSearchPopup::Show(QMainWindow* main_window) {
    if (!main_window) {
        return;
    }
    CommandSearchPopup popup(main_window);
    popup.cursor_anchor_ = QCursor::pos();
    popup.RepositionNearCursor();
    popup.search_->setFocus();
    popup.search_->selectAll();
    popup.exec();
}

CommandSearchPopup::CommandSearchPopup(QMainWindow* main_window)
    : QDialog(main_window, Qt::Popup | Qt::FramelessWindowHint),
      main_window_(main_window) {
    setObjectName("Dom3DCommandSearch");
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(
        "QDialog#Dom3DCommandSearch { background: #242629; border: 1px solid #51555a; }"
        "QLineEdit { background: #303236; color: #f0f2f4; border: 1px solid #656a70;"
        " padding: 6px 8px; selection-background-color: #2779c7; }"
        "QListWidget { background: #242629; color: #d7dade; border: 1px solid #464a4f;"
        " outline: none; padding: 2px; }"
        "QListWidget::item { padding: 5px 7px; }"
        "QListWidget::item:selected { background: #4a4e54; color: white; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(4);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search command...");
    search_->addAction(search_icon(), QLineEdit::LeadingPosition);
    search_->installEventFilter(this);
    layout->addWidget(search_);

    results_ = new QListWidget(this);
    results_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    results_->installEventFilter(this);
    results_->hide();
    layout->addWidget(results_);

    for (QAction* action : main_window_->findChildren<QAction*>()) {
        if (!action
            || action->property("dom3dHotkeyId").toString().isEmpty()
            || action->isSeparator()
            || !action->isEnabled()) {
            continue;
        }
        const QString command = clean_text(action->text());
        if (command.isEmpty()) {
            continue;
        }
        const QString category =
            action->property("dom3dHotkeyCategory").toString();
        Command entry;
        entry.action = action;
        entry.label = category.isEmpty()
            ? command : category + QStringLiteral("  →  ") + command;
        entry.search_text = (category + ' ' + command).toLower();
        commands_.push_back(std::move(entry));
    }
    std::sort(commands_.begin(), commands_.end(),
              [](const Command& first, const Command& second) {
        return first.label.compare(second.label, Qt::CaseInsensitive) < 0;
    });

    connect(search_, &QLineEdit::textChanged,
            this, &CommandSearchPopup::UpdateResults);
    connect(results_, &QListWidget::itemClicked,
            this, &CommandSearchPopup::ExecuteItem);
    connect(results_, &QListWidget::itemActivated,
            this, &CommandSearchPopup::ExecuteItem);
    resize(470, 44);
}

bool CommandSearchPopup::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress) {
        return QDialog::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
        reject();
        return true;
    }
    if (watched == search_) {
        if (key->key() == Qt::Key_Down && results_->count() > 0) {
            results_->setFocus();
            results_->setCurrentRow(0);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            ExecuteFirstResult();
            return true;
        }
    } else if (watched == results_) {
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            ExecuteItem(results_->currentItem());
            return true;
        }
        if (key->key() == Qt::Key_Up && results_->currentRow() <= 0) {
            search_->setFocus();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandSearchPopup::UpdateResults(const QString& text) {
    results_->clear();
    const QStringList words = text.toLower().split(
        QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (words.empty()) {
        results_->hide();
        UpdatePopupSize();
        return;
    }

    int count = 0;
    for (size_t index = 0; index < commands_.size() && count < 40; ++index) {
        const Command& command = commands_[index];
        const bool match = std::all_of(
            words.begin(), words.end(), [&command](const QString& word) {
                return command.search_text.contains(word);
            });
        if (!match) {
            continue;
        }
        auto* item = new QListWidgetItem(
            command.action->icon(), command.label, results_);
        item->setData(Qt::UserRole, static_cast<qulonglong>(index));
        ++count;
    }
    results_->setVisible(results_->count() > 0);
    if (results_->count() > 0) {
        results_->setCurrentRow(0);
    }
    UpdatePopupSize();
}

void CommandSearchPopup::ExecuteItem(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    const size_t index = static_cast<size_t>(
        item->data(Qt::UserRole).toULongLong());
    if (index >= commands_.size() || !commands_[index].action) {
        return;
    }
    QAction* action = commands_[index].action;
    accept();
    QTimer::singleShot(0, action, [action]() {
        if (action->isEnabled()) {
            action->trigger();
        }
    });
}

void CommandSearchPopup::ExecuteFirstResult() {
    QListWidgetItem* item = results_->currentItem();
    if (!item && results_->count() > 0) {
        item = results_->item(0);
    }
    ExecuteItem(item);
}

void CommandSearchPopup::UpdatePopupSize() {
    const int visible_rows = std::min(12, results_->count());
    const int list_height = visible_rows > 0
        ? visible_rows * results_->sizeHintForRow(0) + 8 : 0;
    results_->setFixedHeight(list_height);
    resize(560, 44 + list_height);
    RepositionNearCursor();
}

void CommandSearchPopup::RepositionNearCursor() {
    constexpr int cursor_gap = 12;
    constexpr int screen_margin = 8;
    QScreen* screen = QGuiApplication::screenAt(cursor_anchor_);
    if (!screen) {
        screen = main_window_ ? main_window_->screen() : QGuiApplication::primaryScreen();
    }
    if (!screen) {
        move(cursor_anchor_ + QPoint(cursor_gap, cursor_gap));
        return;
    }

    const QRect available = screen->availableGeometry().adjusted(
        screen_margin, screen_margin, -screen_margin, -screen_margin);
    QPoint position = cursor_anchor_ + QPoint(cursor_gap, cursor_gap);
    if (position.x() + width() > available.right() + 1) {
        position.setX(cursor_anchor_.x() - width() - cursor_gap);
    }
    if (position.y() + height() > available.bottom() + 1) {
        position.setY(cursor_anchor_.y() - height() - cursor_gap);
    }
    position.setX(std::clamp(
        position.x(), available.left(),
        std::max(available.left(), available.right() - width() + 1)));
    position.setY(std::clamp(
        position.y(), available.top(),
        std::max(available.top(), available.bottom() - height() + 1)));
    move(position);
}
