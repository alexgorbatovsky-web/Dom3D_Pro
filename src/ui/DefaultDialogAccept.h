#pragma once

#include <QAbstractSpinBox>
#include <QApplication>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QPointer>
#include <QPushButton>

// Keep the normal OK click/validation path when focus returns to the scene.
// The filter belongs to the button, so rebuilding a parameter form removes it.
class DefaultDialogAccept final : public QObject {
public:
    DefaultDialogAccept(QWidget* scope, QPushButton* button)
        : QObject(button), scope_(scope), button_(button) {
        for (auto* other : scope->findChildren<QPushButton*>()) {
            other->setAutoDefault(false);
            other->setDefault(false);
        }
        button->setAutoDefault(true);
        button->setDefault(true);
        qApp->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)
            return false;
        auto* key = static_cast<QKeyEvent*>(event);
        if (!scope_ || !button_ || !scope_->isVisible() || !button_->isVisible()
            || (key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter)
            || (key->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier
            || QApplication::activePopupWidget()) return false;
        auto* modal = QApplication::activeModalWidget();
        if (modal && scope_->window() != modal) return false;
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) return false;
        const bool editor = widget == scope_ || scope_->isAncestorOf(widget);
        if (!editor && !widget->inherits("OpenGLViewport")) return false;
        const auto root = [](QWidget* w) {
            while (w->parentWidget()) w = w->parentWidget();
            return w;
        };
        if (root(widget) != root(scope_)) return false;
        event->accept();
        if (event->type() == QEvent::ShortcutOverride || key->isAutoRepeat()) return true;
        // Parameter changes can rebuild the form or delete its OK button.
        QPointer<QPushButton> button = button_;
        QList<QPointer<QAbstractSpinBox>> spins;
        for (auto* spin : scope_->findChildren<QAbstractSpinBox*>()) spins.append(spin);
        for (const auto& spin : spins) if (spin) spin->interpretText();
        if (button && button->isVisible() && button->isEnabled()) button->click();
        return true;
    }

private:
    QPointer<QWidget> scope_;
    QPointer<QPushButton> button_;
};

inline void SetDefaultDialogAccept(QWidget* scope, QPushButton* button) {
    if (button) new DefaultDialogAccept(scope, button);
}

inline void SetDefaultDialogAccept(QWidget* scope, QDialogButtonBox* buttons) {
    SetDefaultDialogAccept(scope, buttons->button(QDialogButtonBox::Ok));
}
