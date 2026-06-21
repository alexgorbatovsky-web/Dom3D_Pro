#include "PropertyPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <algorithm>
#include <cmath>

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
        if (parameter.type == ToolParameterType::Checkbox) {
            auto* editor = new QCheckBox(this);
            editor->setChecked(parameter.value >= 0.5);
            connect(editor, &QCheckBox::toggled, this, [this, i](bool checked) {
                active_object_.parameters[static_cast<size_t>(i)].value = checked ? 1.0 : 0.0;
                emit ParametersChanged();
            });
            form_->addRow(QString::fromStdString(parameter.label), editor);
            continue;
        }

        if (parameter.type == ToolParameterType::Combo) {
            auto* editor = new QComboBox(this);
            for (const std::string& option : parameter.options) {
                editor->addItem(QString::fromStdString(option));
            }
            const int index = std::clamp(static_cast<int>(parameter.value), 0, std::max(0, editor->count() - 1));
            editor->setCurrentIndex(index);
            connect(editor, &QComboBox::currentIndexChanged, this, [this, i](int index) {
                active_object_.parameters[static_cast<size_t>(i)].value = static_cast<double>(index);
                emit ParametersChanged();
            });
            form_->addRow(QString::fromStdString(parameter.label), editor);
            continue;
        }

        constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
        const bool solid_transform_angle = active_object_.tool_id == "SolidTransform" && parameter.id == "angle";
        const double display_factor = solid_transform_angle ? radians_to_degrees : 1.0;
        auto* editor = new QDoubleSpinBox(this);
        editor->setRange(parameter.minimum * display_factor, parameter.maximum * display_factor);
        editor->setSingleStep(parameter.step * display_factor);
        editor->setDecimals(solid_transform_angle ? 1 : (parameter.step < 0.1 ? 2 : 1));
        editor->setSuffix(solid_transform_angle ? QString::fromUtf8("°") : QString());
        editor->setValue(parameter.value * display_factor);
        connect(editor, &QDoubleSpinBox::valueChanged, this, [this, i, display_factor](double value) {
            active_object_.parameters[static_cast<size_t>(i)].value = value / display_factor;
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
