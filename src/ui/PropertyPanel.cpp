#include "PropertyPanel.h"

#include "MeasurementUnits.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace {
constexpr int kParameterSliderSteps = 2000;

class ScaleGraphEditor final : public QWidget {
public:
    ScaleGraphEditor(std::vector<double> values,
                     double minimum,
                     double maximum,
                     QWidget* parent = nullptr)
        : QWidget(parent),
          values_(std::move(values)),
          minimum_(minimum),
          maximum_(maximum) {
        setMinimumSize(520, 280);
        setMouseTracking(true);
    }

    std::function<void(const std::vector<double>&)> values_changed;
    std::function<void(const std::vector<double>&)> editing_finished;

protected:
    QRectF graph_rect() const {
        return QRectF(46.0, 18.0, std::max(10, width() - 64),
                      std::max(10, height() - 54));
    }

    QPointF point_position(int index) const {
        const QRectF area = graph_rect();
        const double x = values_.size() <= 1
            ? area.left()
            : area.left() + area.width() * index / (values_.size() - 1);
        const double ratio = (values_[static_cast<size_t>(index)] - minimum_)
            / std::max(1.0e-9, maximum_ - minimum_);
        return {x, area.bottom() - area.height() * ratio};
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(36, 39, 43));
        const QRectF area = graph_rect();
        painter.fillRect(area, QColor(48, 51, 55));

        painter.setPen(QPen(QColor(78, 82, 87), 1.0));
        for (int line = 0; line <= 10; ++line) {
            const double x = area.left() + area.width() * line / 10.0;
            painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        }
        for (int line = 0; line <= 8; ++line) {
            const double y = area.top() + area.height() * line / 8.0;
            painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        }

        const double one_ratio = (1.0 - minimum_)
            / std::max(1.0e-9, maximum_ - minimum_);
        const double one_y = area.bottom() - area.height() * one_ratio;
        painter.setPen(QPen(QColor(88, 145, 194), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(area.left(), one_y), QPointF(area.right(), one_y));
        painter.setPen(QColor(175, 180, 185));
        painter.drawText(QRectF(2.0, one_y - 9.0, 40.0, 18.0),
                         Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("1.00"));
        painter.drawText(QRectF(area.left(), area.bottom() + 8.0, area.width(), 20.0),
                         Qt::AlignCenter, QStringLiteral("Position along guide  0 — 100%"));

        if (values_.empty()) return;
        QPainterPath curve(point_position(0));
        for (int index = 0; index + 1 < static_cast<int>(values_.size()); ++index) {
            const QPointF p0 = point_position(std::max(0, index - 1));
            const QPointF p1 = point_position(index);
            const QPointF p2 = point_position(index + 1);
            const QPointF p3 = point_position(
                std::min(static_cast<int>(values_.size()) - 1, index + 2));
            const QPointF c1 = p1 + (p2 - p0) / 6.0;
            const QPointF c2 = p2 - (p3 - p1) / 6.0;
            curve.cubicTo(c1, c2, p2);
        }
        painter.setPen(QPen(QColor(222, 224, 226), 2.0));
        painter.drawPath(curve);
        for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
            const QPointF point = point_position(index);
            painter.setBrush(index == active_point_ ? QColor(43, 183, 255)
                                                     : QColor(220, 222, 224));
            painter.setPen(QPen(QColor(24, 26, 28), 1.0));
            painter.drawEllipse(point, 5.0, 5.0);
            if (index == active_point_) {
                painter.setPen(QColor(230, 234, 238));
                painter.drawText(QRectF(point.x() - 35.0, point.y() - 27.0, 70.0, 20.0),
                                 Qt::AlignCenter,
                                 QString::number(values_[static_cast<size_t>(index)], 'f', 2));
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        active_point_ = -1;
        for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
            const QPointF delta = point_position(index) - event->position();
            if (delta.x() * delta.x() + delta.y() * delta.y() <= 144.0) {
                active_point_ = index;
                break;
            }
        }
        if (active_point_ >= 0) {
            apply_mouse_value(event->position().y());
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (active_point_ >= 0 && (event->buttons() & Qt::LeftButton)) {
            apply_mouse_value(event->position().y());
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && active_point_ >= 0) {
            apply_mouse_value(event->position().y());
            if (editing_finished) editing_finished(values_);
            active_point_ = -1;
            update();
            event->accept();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
            const QPointF delta = point_position(index) - event->position();
            if (delta.x() * delta.x() + delta.y() * delta.y() <= 144.0) {
                values_[static_cast<size_t>(index)] = 1.0;
                if (values_changed) values_changed(values_);
                if (editing_finished) editing_finished(values_);
                update();
                event->accept();
                return;
            }
        }
    }

private:
    void apply_mouse_value(double y) {
        const QRectF area = graph_rect();
        const double ratio = std::clamp((area.bottom() - y) / area.height(), 0.0, 1.0);
        values_[static_cast<size_t>(active_point_)] =
            minimum_ + ratio * (maximum_ - minimum_);
        if (values_changed) values_changed(values_);
        update();
    }

    std::vector<double> values_;
    double minimum_ = 0.05;
    double maximum_ = 3.0;
    int active_point_ = -1;
};

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
        || id == "radius_start"
        || id == "radius_end"
        || id.rfind("radius.point.", 0) == 0
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
        || id == "drawer_count"
        || id == "vertical_bars"
        || id == "horizontal_bars"
        || id == "spline.degree";
}

bool IsInternalPlacementParameter(const ToolParameter& parameter) {
    return parameter.id.rfind("origin.", 0) == 0
        || parameter.id.rfind("axis.", 0) == 0
        || parameter.id.rfind("width.scale.", 0) == 0
        || parameter.id.rfind("height.scale.", 0) == 0
        || parameter.id == "profile.id"
        || parameter.id == "section.id"
        || parameter.id == "guide.id"
        || (parameter.id.rfind("slx.", 0) == 0
            && parameter.type != ToolParameterType::CatalogSketch
            && parameter.type != ToolParameterType::CatalogProduct)
        || parameter.id == "cutter.id"
        || parameter.id == "surface.id"
        || parameter.id == "host.wall.id"
        || parameter.id == "face.index"
        || parameter.id == "boolean.body_id"
        || parameter.id == "boolean.tool_index";
}

bool IsCatalogParameterVisible(const ActiveParametricObject& active,
                               const ToolParameter& parameter) {
    if (active.tool_id != "cabinet_advanced_slx") {
        return true;
    }
    const auto value_of = [&active](const char* id, double fallback) {
        const auto found = std::find_if(
            active.parameters.begin(), active.parameters.end(),
            [id](const ToolParameter& candidate) { return candidate.id == id; });
        return found == active.parameters.end() ? fallback : found->value;
    };
    const int facade_style = static_cast<int>(value_of("facade_style", 0.0));
    if (parameter.id == "slx.profile.id" || parameter.id == "slx.panel.id") {
        return facade_style == 1;
    }
    if (parameter.id == "slx.milling.profile.id"
        || parameter.id == "slx.milling.guide.id"
        || parameter.id == "milling_depth") {
        return facade_style == 3;
    }
    if (parameter.id == "slx.handle.id") {
        return static_cast<int>(value_of("handle_type", 0.0)) == 3;
    }
    return true;
}

bool IsFurnitureAssemblyParameterVisible(
    const ActiveParametricObject& active,
    const ToolParameter& parameter) {
    if (active.tool_id != "kitchen_nika_260"
        && active.tool_id != "kitchen_corner") {
        return true;
    }
    // Kitchen dimensions and animation angles are construction constants.
    // Keep them in the parametric object for rebuild/save/door interaction,
    // but present the compact dialog used by the original Dom-3D kitchen.
    return parameter.id == "facade_style"
        || parameter.id == "handle_type"
        || parameter.type == ToolParameterType::Material;
}

bool IsCompactCabinetTool(const std::string& tool_id) {
    return tool_id == "cabinet"
        || tool_id == "cabinet_advanced"
        || tool_id == "cabinet_advanced_slx"
        || tool_id == "cabinet_showcase";
}

bool IsPrimaryCabinetParameter(const ToolParameter& parameter) {
    // The fourth item is reserved for wall cabinets.  It becomes visible in
    // the compact panel automatically when the corresponding cabinet tool
    // exposes one of these placement parameters.
    return parameter.id == "width"
        || parameter.id == "height"
        || parameter.id == "depth"
        || parameter.id == "body_type"
        || parameter.id == "facade_type"
        || parameter.id == "facade_style"
        || parameter.id == "showcase_facade_type"
        || parameter.id == "facade_showcase"
        || parameter.id == "showcase_fill"
        || parameter.id == "facade_material_id"
        || parameter.id == "overhead"
        || parameter.id == "mounting_height"
        || parameter.id == "height_above_floor"
        || parameter.id == "overhead_height";
}

bool IsCabinetPlacementParameterVisible(
    const ActiveParametricObject& active,
    const ToolParameter& parameter) {
    if (!IsCompactCabinetTool(active.tool_id)) {
        return true;
    }
    const auto checked = [&active](const char* id) {
        const auto found = std::find_if(
            active.parameters.begin(), active.parameters.end(),
            [id](const ToolParameter& candidate) {
                return candidate.id == id;
            });
        return found != active.parameters.end() && found->value >= 0.5;
    };
    if (parameter.id == "mounting_height") {
        return checked("overhead");
    }
    if (parameter.id == "showcase_fill") {
        return active.tool_id == "cabinet_showcase"
            && checked("facade_showcase");
    }
    return true;
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

bool IsFilletParameterVisible(const ActiveParametricObject& active,
                              const ToolParameter& parameter) {
    if (active.tool_id != "fillet_edge" && active.tool_id != "fillet_all_edges") {
        return true;
    }
    const auto radius_type = std::find_if(
        active.parameters.begin(), active.parameters.end(),
        [](const ToolParameter& candidate) { return candidate.id == "radius_type"; });
    const int mode = radius_type == active.parameters.end()
        ? 0 : std::clamp(static_cast<int>(radius_type->value), 0, 1);
    if (parameter.id == "radius") {
        return mode == 0;
    }
    if (parameter.id == "radius_start" || parameter.id == "radius_end") {
        return mode == 1;
    }
    if (parameter.id == "radius.graph"
        || parameter.id.rfind("radius.point.", 0) == 0) {
        return false;
    }
    return true;
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
        || tool_id == "SolidSketchFeature"
        || tool_id == "SolidExtrudeTool"
        || tool_id == "SolidSweptTool"
        || tool_id == "SolidExtrudeFace"
        || tool_id == "SolidDraft"
        || tool_id == "SolidSheetBend"
        || tool_id == "fillet_edge"
        || tool_id == "fillet_all_edges"
        || tool_id == "ChamferSolid"
        || tool_id == "ThickSolidTool"
        || tool_id == "SurfaceOfRevolution"
        || tool_id == "SurfaceRuled"
        || tool_id == "SolidShell"
        || tool_id == "cabinet"
        || tool_id == "cabinet_advanced"
        || tool_id == "cabinet_advanced_slx"
        || tool_id == "cabinet_showcase"
        || tool_id == "room"
        || tool_id == "window"
        || tool_id == "door"
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

PropertyPanel::PropertyPanel(QWidget* parent, bool additional_parameters)
    : QWidget(parent),
      form_(new QFormLayout(this)),
      additional_parameters_(additional_parameters) {
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
    if (active_object_.tool_id == "cabinet_advanced"
        || active_object_.tool_id == "cabinet_advanced_slx") {
        const auto body_type = std::find_if(
            active_object_.parameters.begin(), active_object_.parameters.end(),
            [](const ToolParameter& parameter) {
                return parameter.id == "body_type";
            });
        const auto facade_style = std::find_if(
            active_object_.parameters.begin(), active_object_.parameters.end(),
            [](const ToolParameter& parameter) {
                return parameter.id == "facade_style";
            });
        if (body_type != active_object_.parameters.end()
            && facade_style != active_object_.parameters.end()) {
            const int body = static_cast<int>(std::lround(body_type->value));
            const bool radius_body =
                body == 2 || body == 4 || body == 5 || body == 6;
            if (radius_body) {
                facade_style->options = {"Plain", "Milano"};
                facade_style->option_values = {0.0, 4.0};
                facade_style->value = facade_style->value == 4.0 ? 4.0 : 0.0;
            } else {
                facade_style->options = {
                    "Plain", "Frame", "Screen", "Milled", "Milano"};
                facade_style->option_values = {0.0, 1.0, 2.0, 3.0, 4.0};
            }
        }
    }
    if (active_object_.tool_id == "cabinet_advanced_slx") {
        auto thickness = std::find_if(
            active_object_.parameters.begin(), active_object_.parameters.end(),
            [](const ToolParameter& parameter) {
                return parameter.id == "panel_thickness";
            });
        auto milling_depth = std::find_if(
            active_object_.parameters.begin(), active_object_.parameters.end(),
            [](const ToolParameter& parameter) {
                return parameter.id == "milling_depth";
            });
        if (thickness != active_object_.parameters.end()
            && milling_depth != active_object_.parameters.end()) {
            milling_depth->maximum = std::max(
                milling_depth->minimum, thickness->value - 0.5);
            milling_depth->value = std::clamp(
                milling_depth->value,
                milling_depth->minimum, milling_depth->maximum);
        }
    }
    if (active_object_.tool_id == "fillet_edge"
        || active_object_.tool_id == "fillet_all_edges") {
        const auto radius_type = std::find_if(
            active_object_.parameters.begin(), active_object_.parameters.end(),
            [](const ToolParameter& parameter) {
                return parameter.id == "radius_type";
            });
        if (radius_type != active_object_.parameters.end()) {
            radius_type->value = std::clamp(radius_type->value, 0.0, 1.0);
        }
    }
    RebuildForm();

    for (int i = 0; i < static_cast<int>(active_object_.parameters.size()); ++i) {
        ToolParameter& parameter = active_object_.parameters[static_cast<size_t>(i)];
        if (IsInternalPlacementParameter(parameter)
            || !IsPlaneParameterVisible(active_object_, parameter)
            || !IsFilletParameterVisible(active_object_, parameter)
            || !IsCatalogParameterVisible(active_object_, parameter)
            || !IsFurnitureAssemblyParameterVisible(active_object_, parameter)
            || !IsCabinetPlacementParameterVisible(active_object_, parameter)
            || (IsCompactCabinetTool(active_object_.tool_id)
                && additional_parameters_
                == IsPrimaryCabinetParameter(parameter))) {
            continue;
        }

        if (parameter.type == ToolParameterType::CatalogSketch
            || parameter.type == ToolParameterType::CatalogProduct) {
            auto* editor = new QWidget(this);
            auto* row = new QHBoxLayout(editor);
            row->setContentsMargins(0, 0, 0, 0);
            auto* from_catalog = new QCheckBox("From Catalog", editor);
            auto* choose = new QPushButton(
                parameter.value > 0.0 ? "Choose another..." : "Choose...", editor);
            auto* axes = new QPushButton("Axes...", editor);
            axes->setToolTip("Show the required local coordinate system");
            from_catalog->setChecked(parameter.value > 0.0);
            choose->setEnabled(from_catalog->isChecked());
            connect(from_catalog, &QCheckBox::toggled, this,
                    [this, i, choose](bool checked) {
                choose->setEnabled(checked);
                if (!checked) {
                    active_object_.parameters[static_cast<size_t>(i)].value = 0.0;
                    emit ParametersChanged();
                }
            });
            connect(choose, &QPushButton::clicked, this, [this, i]() {
                const ToolParameter& selected =
                    active_object_.parameters[static_cast<size_t>(i)];
                emit CatalogSelectionRequested(
                    QString::fromStdString(selected.id),
                    selected.type == ToolParameterType::CatalogProduct);
            });
            connect(axes, &QPushButton::clicked, this, [this, i]() {
                const ToolParameter& selected =
                    active_object_.parameters[static_cast<size_t>(i)];
                emit CatalogOrientationHelpRequested(
                    QString::fromStdString(selected.id),
                    selected.type == ToolParameterType::CatalogProduct);
            });
            row->addWidget(from_catalog);
            row->addWidget(choose);
            row->addWidget(axes);
            form_->addRow(QString::fromStdString(parameter.label), editor);
            continue;
        }

        if (active_object_.tool_id == "PlaneTool" && parameter.id == "p1.x") {
            const DisplayLengthUnit display_unit = LoadDisplayLengthUnit();
            const double display_factor = MillimetersToDisplay(1.0, display_unit);
            for (int point = 1; point <= 3; ++point) {
                std::array<size_t, 3> parameter_indices{};
                std::array<double, 3> displayed_values{};
                int axis_index = 0;
                for (char axis : {'x', 'y', 'z'}) {
                    const std::string id = "p" + std::to_string(point) + "." + axis;
                    const auto found = std::find_if(
                        active_object_.parameters.begin(), active_object_.parameters.end(),
                        [&id](const ToolParameter& candidate) { return candidate.id == id; });
                    if (found == active_object_.parameters.end()) continue;
                    parameter_indices[static_cast<size_t>(axis_index)] = static_cast<size_t>(
                        std::distance(active_object_.parameters.begin(), found));
                    displayed_values[static_cast<size_t>(axis_index)] =
                        found->value * display_factor;
                    ++axis_index;
                }
                auto* editor = new QLineEdit(this);
                editor->setText(QString("%1 %2 %3")
                    .arg(displayed_values[0], 0, 'f', 3)
                    .arg(displayed_values[1], 0, 'f', 3)
                    .arg(displayed_values[2], 0, 'f', 3));
                editor->setToolTip(QString("Point %1: X Y Z (%2)")
                    .arg(point).arg(DisplayLengthUnitSuffix(display_unit)));
                connect(editor, &QLineEdit::editingFinished, this,
                        [this, editor, parameter_indices, display_factor]() {
                    QString text = editor->text();
                    text.replace(',', '.');
                    const QStringList values = text.split(
                        QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                    if (values.size() != 3) {
                        editor->setStyleSheet("QLineEdit { background: #ffd6d6; }");
                        return;
                    }
                    std::array<double, 3> parsed{};
                    for (int axis = 0; axis < 3; ++axis) {
                        bool ok = false;
                        parsed[static_cast<size_t>(axis)] = values[axis].toDouble(&ok);
                        if (!ok) {
                            editor->setStyleSheet("QLineEdit { background: #ffd6d6; }");
                            return;
                        }
                    }
                    editor->setStyleSheet({});
                    for (size_t axis = 0; axis < 3; ++axis) {
                        active_object_.parameters[parameter_indices[axis]].value =
                            parsed[axis] / display_factor;
                    }
                    emit ParametersChanged();
                });
                form_->addRow(QString("Point %1  X Y Z").arg(point), editor);
            }
            i += 8;
            continue;
        }

        if (parameter.type == ToolParameterType::Checkbox) {
            auto* editor = new QCheckBox(this);
            editor->setChecked(parameter.value >= 0.5);
            connect(editor, &QCheckBox::toggled, this, [this, i](bool checked) {
                ToolParameter& changed =
                    active_object_.parameters[static_cast<size_t>(i)];
                changed.value = checked ? 1.0 : 0.0;
                emit ParametersChanged();
                if (IsCompactCabinetTool(active_object_.tool_id)
                    && (changed.id == "overhead"
                        || changed.id == "facade_showcase")) {
                    QTimer::singleShot(0, this, [this]() {
                        SetActiveObject(active_object_);
                    });
                }
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
            const int material_library_index = editor->count();
            if (parameter.type == ToolParameterType::Material) {
                editor->insertSeparator(material_library_index);
                editor->addItem(QStringLiteral("Library…"));
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
            connect(editor, &QComboBox::currentIndexChanged, this,
                    [this, i, editor, material_library_index](int index) {
                ToolParameter& parameter =
                    active_object_.parameters[static_cast<size_t>(i)];
                if (parameter.type == ToolParameterType::Material
                    && index > material_library_index) {
                    int previous_index = 0;
                    const auto selected = std::find(
                        parameter.option_values.begin(),
                        parameter.option_values.end(),
                        parameter.value);
                    if (selected != parameter.option_values.end()) {
                        previous_index = static_cast<int>(std::distance(
                            parameter.option_values.begin(), selected));
                    }
                    // MaterialLibraryRequested is delivered synchronously.
                    // Its receiver may rebuild this panel (the nested
                    // "Other Parameters" panel does), deleting `editor`.
                    // Destroy the blocker before emitting so its destructor
                    // never dereferences an already deleted QObject.
                    {
                        const QSignalBlocker blocker(editor);
                        editor->setCurrentIndex(previous_index);
                    }
                    emit MaterialLibraryRequested(
                        QString::fromStdString(parameter.id));
                    return;
                }
                const bool rebuild_plane_form =
                    active_object_.tool_id == "PlaneTool"
                    && parameter.id == "mode";
                const bool rebuild_catalog_form =
                    ((active_object_.tool_id == "cabinet_advanced"
                      || active_object_.tool_id == "cabinet_advanced_slx")
                     && parameter.id == "body_type")
                    || (active_object_.tool_id == "cabinet_advanced_slx"
                        && (parameter.id == "facade_style"
                            || parameter.id == "handle_type"));
                const bool rebuild_fillet_form =
                    (active_object_.tool_id == "fillet_edge"
                     || active_object_.tool_id == "fillet_all_edges")
                    && parameter.id == "radius_type";
                parameter.value = parameter.option_values.size() == parameter.options.size()
                    && index >= 0
                    && static_cast<size_t>(index) < parameter.option_values.size()
                    ? parameter.option_values[static_cast<size_t>(index)]
                    : static_cast<double>(index);
                emit ParametersChanged();
                if (rebuild_plane_form || rebuild_catalog_form || rebuild_fillet_form) {
                    QTimer::singleShot(0, this, [this]() {
                        SetActiveObject(active_object_);
                    });
                }
            });
            form_->addRow(QString::fromStdString(parameter.label), editor);
            continue;
        }

        if (parameter.type == ToolParameterType::Graph) {
            auto* edit_graph = new QPushButton(QStringLiteral("Edit…"), this);
            const bool fillet_law = active_object_.tool_id == "fillet_edge"
                || active_object_.tool_id == "fillet_all_edges";
            edit_graph->setToolTip(fillet_law
                ? QStringLiteral("Edit radius along the edge from 0 to 100%.")
                : QStringLiteral("Edit scale along the guide. Double-click a point to reset it to 1.0."));
            connect(edit_graph, &QPushButton::clicked, this, [this, i]() {
                const ToolParameter& graph_parameter =
                    active_object_.parameters[static_cast<size_t>(i)];
                const bool fillet_law = active_object_.tool_id == "fillet_edge"
                    || active_object_.tool_id == "fillet_all_edges";
                const std::string prefix = fillet_law
                    ? "radius.point."
                    : graph_parameter.id.substr(
                        0, graph_parameter.id.find(".graph")) + ".scale.";
                std::vector<size_t> parameter_indices;
                std::vector<double> original_values;
                const size_t point_count = fillet_law ? 6 : 5;
                for (size_t point = 0; point < point_count; ++point) {
                    const std::string point_id = prefix + std::to_string(point);
                    const auto found = std::find_if(
                        active_object_.parameters.begin(),
                        active_object_.parameters.end(),
                        [&point_id](const ToolParameter& candidate) {
                            return candidate.id == point_id;
                        });
                    if (found == active_object_.parameters.end()) return;
                    parameter_indices.push_back(static_cast<size_t>(
                        std::distance(active_object_.parameters.begin(), found)));
                    original_values.push_back(found->value);
                }

                QDialog dialog(this);
                dialog.setWindowTitle(QString::fromStdString(graph_parameter.label));
                auto* layout = new QVBoxLayout(&dialog);
                auto* graph = new ScaleGraphEditor(
                    original_values, graph_parameter.minimum,
                    graph_parameter.maximum, &dialog);
                const auto store_values = [this, parameter_indices](
                                              const std::vector<double>& values) {
                    for (size_t point = 0; point < parameter_indices.size(); ++point) {
                        active_object_.parameters[parameter_indices[point]].value = values[point];
                    }
                };
                graph->values_changed = [this, store_values, fillet_law](
                                            const std::vector<double>& values) {
                    store_values(values);
                    // A radius-law fillet is much more expensive than the
                    // generic scale graphs. Keep the graph responsive while
                    // dragging and rebuild the solid once on mouse release.
                    if (!fillet_law) emit ParametersChanged();
                };
                if (fillet_law) {
                    graph->editing_finished = [this, store_values](
                                                  const std::vector<double>& values) {
                        store_values(values);
                        emit ParametersChanged();
                    };
                }
                layout->addWidget(graph);
                auto* buttons = new QDialogButtonBox(
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                layout->addWidget(buttons);
                if (dialog.exec() != QDialog::Accepted) {
                    for (size_t point = 0; point < parameter_indices.size(); ++point) {
                        active_object_.parameters[parameter_indices[point]].value =
                            original_values[point];
                    }
                    emit ParametersChanged();
                }
            });
            form_->addRow(QString::fromStdString(parameter.label), edit_graph);
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
            ToolParameter& changed =
                active_object_.parameters[static_cast<size_t>(i)];
            changed.value = value / display_factor;
            if (active_object_.tool_id == "cabinet_advanced_slx"
                && changed.id == "panel_thickness") {
                const auto milling_depth = std::find_if(
                    active_object_.parameters.begin(),
                    active_object_.parameters.end(),
                    [](const ToolParameter& parameter) {
                        return parameter.id == "milling_depth";
                    });
                if (milling_depth != active_object_.parameters.end()) {
                    milling_depth->maximum = std::max(
                        milling_depth->minimum, changed.value - 0.5);
                    milling_depth->value = std::clamp(
                        milling_depth->value,
                        milling_depth->minimum, milling_depth->maximum);
                    if (auto* depth_editor = findChild<QDoubleSpinBox*>(
                            QStringLiteral("parameter_milling_depth"))) {
                        depth_editor->blockSignals(true);
                        depth_editor->setMaximum(
                            milling_depth->maximum * display_factor);
                        depth_editor->setValue(
                            milling_depth->value * display_factor);
                        depth_editor->blockSignals(false);
                    }
                }
            }
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
                // A linear parameter must drag in its declared step, not in a
                // fraction of its full range.  The old normalized mapping made
                // a [-1'000'000, +1'000'000] offset jump by hundreds of mm per
                // mouse pixel.  Ten substeps preserve Shift precision.
                const double drag_quantum = std::max(parameter.step * 0.1, 1.0e-9);
                const double position_count =
                    (parameter.maximum - parameter.minimum) / drag_quantum;
                if (!UseLogarithmicSlider(parameter.minimum, parameter.maximum)
                    && position_count >= 1.0
                    && position_count <= static_cast<double>(std::numeric_limits<int>::max())) {
                    const int maximum_position = static_cast<int>(
                        std::floor(position_count + 0.5));
                    drag_label->drag_speed = 10.0;
                    drag_label->maximum_position = maximum_position;
                    drag_label->current_position =
                        [editor, display_factor, minimum = parameter.minimum,
                         drag_quantum, maximum_position]() {
                            return std::clamp(
                                static_cast<int>(std::lround(
                                    (editor->value() / display_factor - minimum)
                                    / drag_quantum)),
                                0, maximum_position);
                        };
                    drag_label->apply_position =
                        [editor, display_factor, minimum = parameter.minimum,
                         drag_quantum](int position) {
                            editor->setValue(
                                (minimum + static_cast<double>(position) * drag_quantum)
                                * display_factor);
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
            }
            editor->setMinimumWidth(90);
            editor->setMaximumWidth(130);
            form_->addRow(drag_label, editor);
        } else {
            form_->addRow(QString::fromStdString(parameter.label), editor);
        }
    }

    if (IsCompactCabinetTool(active_object_.tool_id)
        && !additional_parameters_) {
        auto* other_parameters = new QPushButton("Other Params...", this);
        other_parameters->setObjectName("other_parameters_button");
        other_parameters->setToolTip(
            "Open facade, construction, hardware and material parameters");
        connect(other_parameters, &QPushButton::clicked, this, [this]() {
            const ActiveParametricObject original_object = active_object_;
            QDialog dialog(this);
            dialog.setWindowTitle("Other Parameters");
            dialog.resize(430, 650);
            auto* layout = new QVBoxLayout(&dialog);
            layout->setContentsMargins(8, 8, 8, 8);
            auto* scroll = new QScrollArea(&dialog);
            scroll->setWidgetResizable(true);
            auto* additional = new PropertyPanel(scroll, true);
            additional->SetActiveObject(active_object_);
            scroll->setWidget(additional);
            layout->addWidget(scroll);

            QString requested_material_id;
            connect(additional, &PropertyPanel::MaterialLibraryRequested,
                    &dialog, [this, additional, &dialog,
                              &requested_material_id](const QString& parameter_id) {
                active_object_ = additional->ActiveObject();
                requested_material_id = parameter_id;
                // Finish the modal callback before the library can assign a
                // material and rebuild the outer compact panel.
                dialog.accept();
            });
            connect(additional, &PropertyPanel::CatalogSelectionRequested,
                    this, [this, additional](const QString& parameter_id,
                                             bool product) {
                active_object_ = additional->ActiveObject();
                emit CatalogSelectionRequested(parameter_id, product);
                additional->SetActiveObject(active_object_);
            });
            connect(additional,
                    &PropertyPanel::CatalogOrientationHelpRequested,
                    this, [this](const QString& parameter_id, bool product) {
                emit CatalogOrientationHelpRequested(parameter_id, product);
            });
            connect(additional, &PropertyPanel::Accepted, &dialog,
                    [this, additional, &dialog]() {
                active_object_ = additional->ActiveObject();
                dialog.accept();
            });
            connect(additional, &PropertyPanel::Canceled,
                    &dialog, &QDialog::reject);

            const int result = dialog.exec();
            if (!requested_material_id.isEmpty()) {
                emit MaterialLibraryRequested(requested_material_id);
                return;
            }
            if (result == QDialog::Accepted) {
                // Rebuild the compact form after this button's callback has
                // returned; rebuilding it synchronously would delete the
                // button that is currently emitting clicked().
                QTimer::singleShot(0, this, [this]() {
                    emit ParametersChanged();
                    SetActiveObject(active_object_);
                });
            } else {
                active_object_ = original_object;
            }
        });
        form_->addRow(other_parameters);
    }

    auto* buttons = new QWidget(this);
    auto* button_layout = new QHBoxLayout(buttons);
    button_layout->setContentsMargins(0, 8, 0, 0);
    button_layout->addStretch();

    auto* ok = new QPushButton("OK", buttons);
    QPushButton* apply = nullptr;
    if (!additional_parameters_
        && (active_object_.tool_id == "cabinet"
        || active_object_.tool_id == "cabinet_advanced"
        || active_object_.tool_id == "cabinet_advanced_slx"
        || active_object_.tool_id == "cabinet_showcase")) {
        apply = new QPushButton("Apply", buttons);
        apply->setToolTip("Build the cabinet and keep this panel open");
        connect(apply, &QPushButton::clicked, this, &PropertyPanel::Applied);
    }
    auto* cancel = new QPushButton("Cancel", buttons);
    connect(ok, &QPushButton::clicked, this, &PropertyPanel::Accepted);
    connect(cancel, &QPushButton::clicked, this, &PropertyPanel::Canceled);
    if (apply) {
        button_layout->addWidget(apply);
    }
    button_layout->addWidget(ok);
    button_layout->addWidget(cancel);
    form_->addRow(buttons);
}

void PropertyPanel::SetMaterialParameterValue(
    const std::string& parameter_id,
    double value,
    const std::string& material_name) {
    const auto found = std::find_if(
        active_object_.parameters.begin(), active_object_.parameters.end(),
        [&parameter_id](const ToolParameter& parameter) {
            return parameter.id == parameter_id
                && parameter.type == ToolParameterType::Material;
        });
    if (found == active_object_.parameters.end()) {
        return;
    }

    const auto existing = std::find(
        found->option_values.begin(), found->option_values.end(), value);
    if (existing == found->option_values.end()) {
        found->options.push_back(material_name);
        found->option_values.push_back(value);
    } else {
        const size_t index = static_cast<size_t>(std::distance(
            found->option_values.begin(), existing));
        if (index < found->options.size()) {
            found->options[index] = material_name;
        }
    }
    found->value = value;
    SetActiveObject(active_object_);
    emit MaterialParameterChanged(QString::fromStdString(parameter_id));
}

void PropertyPanel::SetCatalogParameterValue(
    const std::string& parameter_id, double value) {
    const auto parameter = std::find_if(
        active_object_.parameters.begin(), active_object_.parameters.end(),
        [&parameter_id](const ToolParameter& candidate) {
            return candidate.id == parameter_id;
        });
    if (parameter == active_object_.parameters.end()) {
        return;
    }
    parameter->value = value;
    SetActiveObject(active_object_);
    emit ParametersChanged();
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
