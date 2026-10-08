#pragma once

#include <QDoubleSpinBox>
#include <QLabel>
#include <QMouseEvent>
#include <QSpinBox>
#include <cmath>
#include <algorithm>

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

    DragSpinBoxLabel(const QString& text, QSpinBox* editor, QWidget* parent = nullptr)
        : DragSpinBoxLabel(text, static_cast<QDoubleSpinBox*>(nullptr), parent) {
        integer_editor_ = editor;
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && (editor_ || integer_editor_)) {
            dragging_ = true;
            drag_start_x_ = event->globalPosition().x();
            drag_start_value_ = editor_ ? editor_->value() : integer_editor_->value();
            event->accept();
            return;
        }
        QLabel::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!dragging_ || !(event->buttons() & Qt::LeftButton)) {
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
        if (editor_) {
            editor_->setValue(drag_start_value_ + pixels * editor_->singleStep() * multiplier);
        } else if (integer_editor_) {
            const double value = std::clamp(
                drag_start_value_ + pixels * 0.1 * integer_editor_->singleStep() * multiplier,
                double(integer_editor_->minimum()), double(integer_editor_->maximum()));
            integer_editor_->setValue(static_cast<int>(std::lround(value)));
        }
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
    QSpinBox* integer_editor_ = nullptr;
    bool dragging_ = false;
    double drag_start_x_ = 0.0;
    double drag_start_value_ = 0.0;
};
