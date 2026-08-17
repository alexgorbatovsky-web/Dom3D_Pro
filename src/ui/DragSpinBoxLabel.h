#pragma once

#include <QDoubleSpinBox>
#include <QLabel>
#include <QMouseEvent>

class DragSpinBoxLabel final : public QLabel {
public:
    DragSpinBoxLabel(const QString& text,
                     QDoubleSpinBox* editor,
                     QWidget* parent = nullptr)
        : QLabel(QString::fromUtf8("◀%1▶").arg(text), parent),
          editor_(editor) {
        setCursor(Qt::SizeHorCursor);
        setToolTip(QString("%1: drag horizontally; Shift — precise, Ctrl — fast")
            .arg(text));
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && editor_) {
            dragging_ = true;
            drag_start_x_ = event->globalPosition().x();
            drag_start_value_ = editor_->value();
            event->accept();
            return;
        }
        QLabel::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!dragging_ || !editor_ || !(event->buttons() & Qt::LeftButton)) {
            QLabel::mouseMoveEvent(event);
            return;
        }

        double multiplier = 1.0;
        if (event->modifiers().testFlag(Qt::ShiftModifier)
            && !event->modifiers().testFlag(Qt::ControlModifier)) {
            multiplier = 0.1;
        } else if (event->modifiers().testFlag(Qt::ControlModifier)
                   && !event->modifiers().testFlag(Qt::ShiftModifier)) {
            multiplier = 10.0;
        }
        const double pixels = event->globalPosition().x() - drag_start_x_;
        editor_->setValue(
            drag_start_value_ + pixels * editor_->singleStep() * multiplier);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }

private:
    QDoubleSpinBox* editor_ = nullptr;
    bool dragging_ = false;
    double drag_start_x_ = 0.0;
    double drag_start_value_ = 0.0;
};
