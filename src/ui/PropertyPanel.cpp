#include "PropertyPanel.h"

#include "MeasurementUnits.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
bool IsLengthParameter(const ToolParameter& parameter) {
    if (parameter.unit == ToolParameterUnit::Length) {
        return true;
    }

    const std::string& id = parameter.id;
    return id == "width"
        || id == "height"
        || id == "depth"
        || id == "length"
        || id == "radius"
        || id == "diameter"
        || id == "distance"
        || id == "thick"
        || id == "dx"
        || id == "dy"
        || id == "dz"
        || id == "center.x"
        || id == "center.y"
        || id == "center.z";
}

bool IsInternalPlacementParameter(const ToolParameter& parameter) {
    return parameter.id.rfind("origin.", 0) == 0
        || parameter.id.rfind("axis.", 0) == 0
        || parameter.id == "profile.id"
        || parameter.id == "section.id"
        || parameter.id == "guide.id"
        || parameter.id == "cutter.id"
        || parameter.id == "face.index"
        || parameter.id == "boolean.body_id"
        || parameter.id == "boolean.tool_index";
}

bool IsPlaneParameterVisible(const ActiveParametricObject& active,
                             const ToolParameter& parameter) {
    if (active.tool_id != "PlaneTool"
        || parameter.id == "mode"
        || parameter.id == "size") {
        return true;
    }
    const auto method = std::find_if(
        active.parameters.begin(),
        active.parameters.end(),
        [](const ToolParameter& candidate) {
            return candidate.id == "mode";
        });
    const int mode = method == active.parameters.end()
        ? 1
        : std::clamp(static_cast<int>(method->value), 0, 5);
    if (mode == 0) {
        return parameter.id == "a"
            || parameter.id == "b"
            || parameter.id == "c"
            || parameter.id == "d";
    }
    if (mode == 1) {
        return parameter.id.rfind("plane.origin.", 0) == 0
            || parameter.id.rfind("plane.normal.", 0) == 0;
    }
    if (mode == 2) {
        return parameter.id.rfind("p1.", 0) == 0
            || parameter.id.rfind("p2.", 0) == 0
            || parameter.id.rfind("p3.", 0) == 0;
    }
    return parameter.id == "offset";
}
}

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
        if (IsInternalPlacementParameter(parameter)
            || !IsPlaneParameterVisible(active_object_, parameter)) {
            continue;
        }

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
                const bool rebuild_plane_form =
                    active_object_.tool_id == "PlaneTool"
                    && active_object_.parameters[static_cast<size_t>(i)].id == "mode";
                active_object_.parameters[static_cast<size_t>(i)].value = static_cast<double>(index);
                emit ParametersChanged();
                if (rebuild_plane_form) {
                    QTimer::singleShot(0, this, [this]() {
                        SetActiveObject(active_object_);
                    });
                }
            });
            form_->addRow(QString::fromStdString(parameter.label), editor);
            continue;
        }

        constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
        const bool solid_transform_angle = active_object_.tool_id == "SolidTransform" && parameter.id == "angle";
        const bool degree_parameter = parameter.unit == ToolParameterUnit::Angle;
        const bool length_parameter = IsLengthParameter(parameter);
        const DisplayLengthUnit display_unit = LoadDisplayLengthUnit();
        const double display_factor = solid_transform_angle
            ? radians_to_degrees
            : (length_parameter ? MillimetersToDisplay(1.0, display_unit) : 1.0);
        auto* editor = new QDoubleSpinBox(this);
        editor->setObjectName(
            QStringLiteral("parameter_%1").arg(QString::fromStdString(parameter.id)));
        editor->setRange(parameter.minimum * display_factor, parameter.maximum * display_factor);
        editor->setSingleStep(parameter.step * display_factor);
        editor->setDecimals((solid_transform_angle || degree_parameter) ? 1 : (length_parameter ? 3 : (parameter.step < 0.1 ? 2 : 1)));
        editor->setSuffix((solid_transform_angle || degree_parameter) ? QString::fromUtf8("°") : (length_parameter ? DisplayLengthUnitSuffix(display_unit) : QString()));
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

void PropertyPanel::FocusParameter(const std::string& parameter_id) {
    auto* editor = findChild<QDoubleSpinBox*>(
        QStringLiteral("parameter_%1").arg(QString::fromStdString(parameter_id)));
    if (!editor) {
        return;
    }
    QTimer::singleShot(0, editor, [editor]() {
        editor->setFocus(Qt::OtherFocusReason);
        editor->selectAll();
    });
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
