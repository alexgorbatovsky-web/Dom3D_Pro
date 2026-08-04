#include "PropertyPanel.h"

#include "MeasurementUnits.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
constexpr int kParameterSliderSteps = 2000;

class DragValueLabel final : public QLabel {
public:
    explicit DragValueLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(QString::fromUtf8("◀%1▶").arg(text), parent) {
        setCursor(Qt::SizeHorCursor);
        setToolTip(QString("%1: drag horizontally; Shift — precise, Ctrl — fast")
            .arg(text));
    }

    std::function<int()> current_position;
    std::function<void(int)> apply_position;
    double drag_speed = 2.0;
    int maximum_position = kParameterSliderSteps;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && current_position) {
            dragging_ = true;
            drag_start_x_ = event->globalPosition().x();
            drag_start_position_ = current_position();
            event->accept();
            return;
        }
        QLabel::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!dragging_ || !(event->buttons() & Qt::LeftButton)
            || !apply_position) {
            QLabel::mouseMoveEvent(event);
            return;
        }

        double speed = drag_speed;
        const Qt::KeyboardModifiers modifiers = event->modifiers();
        if (modifiers.testFlag(Qt::ShiftModifier)
            && !modifiers.testFlag(Qt::ControlModifier)) {
            speed *= 0.1;
        } else if (modifiers.testFlag(Qt::ControlModifier)
                   && !modifiers.testFlag(Qt::ShiftModifier)) {
            speed *= 10.0;
        }
        const double pixels = event->globalPosition().x() - drag_start_x_;
        apply_position(std::clamp(
            drag_start_position_ + static_cast<int>(std::lround(pixels * speed)),
            0,
            maximum_position));
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = false;
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }

private:
    bool dragging_ = false;
    double drag_start_x_ = 0.0;
    int drag_start_position_ = 0;
};

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

bool IsIntegerParameter(const ToolParameter& parameter) {
    const std::string& id = parameter.id;
    return id == "qty"
        || id == "shelf_count"
        || id == "drawer_count";
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

bool IsSliderParameter(const ActiveParametricObject& active,
                       const ToolParameter& parameter) {
    if (parameter.type != ToolParameterType::Number) {
        return false;
    }
    const std::string& tool_id = active.tool_id;
    return tool_id == "SolidBeamTool"
        || tool_id == "SolidBox"
        || tool_id == "SolidCylinder"
        || tool_id == "SolidSphereTool"
        || tool_id == "SolidTorusTool"
        || tool_id == "SolidPrismTool"
        || tool_id == "fillet_edge"
        || tool_id == "fillet_all_edges"
        || tool_id == "ChamferSolid"
        || tool_id == "ThickSolidTool"
        || tool_id == "SurfaceOfRevolution"
        || tool_id == "cabinet"
        || tool_id == "table"
        || tool_id == "desk"
        || tool_id == "drawer_box";
}

bool UseLogarithmicSlider(double minimum, double maximum) {
    return minimum > 0.0
        && maximum > minimum
        && maximum / minimum >= 100.0;
}

int ValueToSliderPosition(double value, double minimum, double maximum) {
    if (maximum <= minimum) {
        return 0;
    }
    const double clamped = std::clamp(value, minimum, maximum);
    const double ratio = UseLogarithmicSlider(minimum, maximum)
        ? std::log(clamped / minimum) / std::log(maximum / minimum)
        : (clamped - minimum) / (maximum - minimum);
    return std::clamp(
        static_cast<int>(std::lround(ratio * kParameterSliderSteps)),
        0,
        kParameterSliderSteps);
}

double SliderPositionToValue(int position, double minimum, double maximum) {
    if (maximum <= minimum) {
        return minimum;
    }
    const double ratio = static_cast<double>(position) / kParameterSliderSteps;
    return UseLogarithmicSlider(minimum, maximum)
        ? minimum * std::pow(maximum / minimum, ratio)
        : minimum + (maximum - minimum) * ratio;
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

        if (parameter.type == ToolParameterType::Combo
            || parameter.type == ToolParameterType::Material) {
            auto* editor = new QComboBox(this);
            editor->setObjectName(
                QStringLiteral("parameter_%1").arg(QString::fromStdString(parameter.id)));
            for (const std::string& option : parameter.options) {
                editor->addItem(QString::fromStdString(option));
            }
            int index = static_cast<int>(parameter.value);
            if (parameter.option_values.size() == parameter.options.size()) {
                const auto selected = std::find(
                    parameter.option_values.begin(),
                    parameter.option_values.end(),
                    parameter.value);
                index = selected == parameter.option_values.end()
                    ? 0
                    : static_cast<int>(std::distance(parameter.option_values.begin(), selected));
            }
            index = std::clamp(index, 0, std::max(0, editor->count() - 1));
            editor->setCurrentIndex(index);
            connect(editor, &QComboBox::currentIndexChanged, this, [this, i](int index) {
                ToolParameter& parameter =
                    active_object_.parameters[static_cast<size_t>(i)];
                const bool rebuild_plane_form =
                    active_object_.tool_id == "PlaneTool"
                    && parameter.id == "mode";
                parameter.value = parameter.option_values.size() == parameter.options.size()
                    && index >= 0
                    && static_cast<size_t>(index) < parameter.option_values.size()
                    ? parameter.option_values[static_cast<size_t>(index)]
                    : static_cast<double>(index);
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
        const bool integer_parameter = IsIntegerParameter(parameter);
        const DisplayLengthUnit display_unit = LoadDisplayLengthUnit();
        const double display_factor = solid_transform_angle
            ? radians_to_degrees
            : (length_parameter ? MillimetersToDisplay(1.0, display_unit) : 1.0);
        auto* editor = new QDoubleSpinBox(this);
        editor->setObjectName(
            QStringLiteral("parameter_%1").arg(QString::fromStdString(parameter.id)));
        editor->setRange(parameter.minimum * display_factor, parameter.maximum * display_factor);
        editor->setSingleStep(parameter.step * display_factor);
        editor->setDecimals(integer_parameter ? 0 : ((solid_transform_angle || degree_parameter) ? 1 : (length_parameter ? 3 : (parameter.step < 0.1 ? 2 : 1))));
        editor->setSuffix((solid_transform_angle || degree_parameter) ? QString::fromUtf8("°") : (length_parameter ? DisplayLengthUnitSuffix(display_unit) : QString()));
        editor->setValue(parameter.value * display_factor);
        const bool use_drag_label = IsSliderParameter(active_object_, parameter);
        connect(editor, &QDoubleSpinBox::valueChanged, this, [this, i, display_factor](double value) {
            active_object_.parameters[static_cast<size_t>(i)].value = value / display_factor;
            emit ParametersChanged();
        });
        if (use_drag_label) {
            auto* drag_label = new DragValueLabel(
                QString::fromStdString(parameter.label), this);
            if (integer_parameter) {
                const int minimum = static_cast<int>(std::lround(parameter.minimum));
                const int maximum = static_cast<int>(std::lround(parameter.maximum));
                drag_label->drag_speed = 0.15;
                drag_label->maximum_position = std::max(0, maximum - minimum);
                drag_label->current_position = [editor, display_factor, minimum]() {
                    return static_cast<int>(std::lround(
                        editor->value() / display_factor)) - minimum;
                };
                drag_label->apply_position = [editor, display_factor, minimum](int position) {
                    editor->setValue(
                        static_cast<double>(minimum + position) * display_factor);
                };
            } else {
                drag_label->current_position =
                    [editor, display_factor, minimum = parameter.minimum,
                     maximum = parameter.maximum]() {
                        return ValueToSliderPosition(
                            editor->value() / display_factor, minimum, maximum);
                    };
                drag_label->apply_position =
                    [editor, display_factor, minimum = parameter.minimum,
                     maximum = parameter.maximum](int position) {
                        editor->setValue(
                            SliderPositionToValue(position, minimum, maximum)
                            * display_factor);
                    };
            }
            editor->setMinimumWidth(90);
            editor->setMaximumWidth(130);
            form_->addRow(drag_label, editor);
        } else {
            form_->addRow(QString::fromStdString(parameter.label), editor);
        }
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

void PropertyPanel::UpdateParameterValue(
    const std::string& parameter_id,
    double value) {
    const auto parameter = std::find_if(
        active_object_.parameters.begin(),
        active_object_.parameters.end(),
        [&parameter_id](const ToolParameter& candidate) {
            return candidate.id == parameter_id;
        });
    if (parameter == active_object_.parameters.end()) {
        return;
    }
    parameter->value = value;

    auto* editor = findChild<QDoubleSpinBox*>(
        QStringLiteral("parameter_%1").arg(
            QString::fromStdString(parameter_id)));
    if (!editor) {
        return;
    }

    constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
    const bool solid_transform_angle =
        active_object_.tool_id == "SolidTransform" && parameter->id == "angle";
    const bool length_parameter = IsLengthParameter(*parameter);
    const DisplayLengthUnit display_unit = LoadDisplayLengthUnit();
    const double display_factor = solid_transform_angle
        ? radians_to_degrees
        : (length_parameter ? MillimetersToDisplay(1.0, display_unit) : 1.0);
    const QSignalBlocker blocker(editor);
    editor->setValue(value * display_factor);
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
