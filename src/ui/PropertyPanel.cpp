#include "PropertyPanel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent),
      form_(new QFormLayout(this)) {
    form_->setContentsMargins(10, 8, 10, 8);
    form_->setHorizontalSpacing(16);
    Clear();
}

void PropertyPanel::Clear() {
    active_object_ = {};
    RebuildForm();
    form_->addRow(new QLabel("No parametric object selected", this));
}

void PropertyPanel::SetActiveObject(const ActiveParametricObject& active_object) {
    active_object_ = active_object;
    RebuildForm();

    for (int i = 0; i < static_cast<int>(active_object_.parameters.size()); ++i) {
        ToolParameter& parameter = active_object_.parameters[static_cast<size_t>(i)];
        auto* editor = new QDoubleSpinBox(this);
        editor->setRange(parameter.minimum, parameter.maximum);
        editor->setSingleStep(parameter.step);
        editor->setDecimals(parameter.step < 0.1 ? 2 : 1);
        editor->setValue(parameter.value);
        connect(editor, &QDoubleSpinBox::valueChanged, this, [this, i](double value) {
            active_object_.parameters[static_cast<size_t>(i)].value = value;
            emit ParametersChanged();
        });
        form_->addRow(QString::fromStdString(parameter.label), editor);
    }

    auto* buttons = new QWidget(this);
    auto* button_layout = new QHBoxLayout(buttons);
    button_layout->setContentsMargins(0, 8, 0, 0);
    button_layout->addStretch();

    auto* ok = new QPushButton("OK", buttons);
    auto* cancel = new QPushButton("Cancel", buttons);
    connect(ok, &QPushButton::clicked, this, &PropertyPanel::Accepted);
    connect(cancel, &QPushButton::clicked, this, &PropertyPanel::Canceled);
    button_layout->addWidget(ok);
    button_layout->addWidget(cancel);
    form_->addRow(buttons);
}

const ActiveParametricObject& PropertyPanel::ActiveObject() const {
    return active_object_;
}

void PropertyPanel::RebuildForm() {
    while (form_->count() > 0) {
        QLayoutItem* item = form_->takeAt(0);
        delete item->widget();
        delete item;
    }
}
