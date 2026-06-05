#include "OpenGLViewport.h"

#include "../solid/Solid.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kGridHalfSize = 12.0f;
}

OpenGLViewport::OpenGLViewport(QWidget* parent)
    : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(640, 420);
}

void OpenGLViewport::SetDocument(CAlfaDoc* document) {
    document_ = document;
    update();
}

void OpenGLViewport::SetTool(ToolMode tool) {
    tool_ = tool;
    if (tool_ != ToolMode::Boolean) {
        has_boolean_body_ = false;
        boolean_body_index_ = 0;
    }
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    dragging_transform_ = false;
    active_transform_axis_ = TransformAxis::None;
    highlighted_transform_axis_ = TransformAxis::None;
    update();
}

void OpenGLViewport::SetTransformOperation(TransformOperation operation) {
    transform_operation_ = operation;
    SetTool(ToolMode::Transform);
}

void OpenGLViewport::BeginBooleanTool(BooleanOperation operation) {
    boolean_operation_ = operation;
    has_boolean_body_ = false;
    boolean_body_index_ = 0;
    SetTool(ToolMode::Boolean);
    emit StatusTextChanged("Boolean: выбери body");
}

void OpenGLViewport::FitToDocument() {
    if (!document_) {
        return;
    }

    Vec3 min_point{};
    Vec3 max_point{};
    bool has_bounds = false;
    for (const auto& object : document_->GetObjects()) {
        if (!object || !object->IsVisible()) {
            continue;
        }

        Vec3 object_min{};
        Vec3 object_max{};
        if (!object->GetBounds(object_min, object_max)) {
            continue;
        }

        if (!has_bounds) {
            min_point = object_min;
            max_point = object_max;
            has_bounds = true;
        } else {
            min_point.x = std::min(min_point.x, object_min.x);
            min_point.y = std::min(min_point.y, object_min.y);
            min_point.z = std::min(min_point.z, object_min.z);
            max_point.x = std::max(max_point.x, object_max.x);
            max_point.y = std::max(max_point.y, object_max.y);
            max_point.z = std::max(max_point.z, object_max.z);
        }
    }

    if (!has_bounds) {
        return;
    }

    const Vec3 center = (min_point + max_point) * 0.5f;
    const Vec3 size = max_point - min_point;
    const float radius = std::max(1.0f, std::sqrt(dot(size, size)) * 0.5f);
    camera_.target = center;
    camera_.distance = std::clamp(radius * 3.0f, 2.0f, 100000.0f);
    update();
}

ToolMode OpenGLViewport::CurrentTool() const {
    return tool_;
}

bool OpenGLViewport::IsOrthographicProjection() const {
    return orthographic_projection_;
}

void OpenGLViewport::SetOrthographicProjection(bool enabled) {
    if (orthographic_projection_ == enabled) {
        return;
    }
    orthographic_projection_ = enabled;
    update();
}

OrbitMode OpenGLViewport::GetOrbitMode() const {
    return orbit_mode_;
}

void OpenGLViewport::SetOrbitMode(OrbitMode mode) {
    orbit_mode_ = mode;
    if (orbit_mode_ == OrbitMode::Architectural) {
        camera_.pitch = std::clamp(camera_.pitch, -10.0f, 75.0f);
    }
    update();
}

bool OpenGLViewport::IsCoordinateAxesVisible() const {
    return show_coordinate_axes_;
}

void OpenGLViewport::SetCoordinateAxesVisible(bool visible) {
    if (show_coordinate_axes_ == visible) {
        return;
    }
    show_coordinate_axes_ = visible;
    update();
}

void OpenGLViewport::initializeGL() {
    renderer_.Initialize();
}

void OpenGLViewport::resizeGL(int, int) {
}

void OpenGLViewport::paintGL() {
    if (!document_) {
        return;
    }
    renderer_.Render(*document_, camera_, orthographic_projection_, show_coordinate_axes_, tool_, highlighted_transform_axis_, width(), height());
    if (show_coordinate_axes_) {
        DrawCoordinateAxisLabels();
    }
}

void OpenGLViewport::mousePressEvent(QMouseEvent* event) {
    last_mouse_ = event->pos();

    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        return;
    }

    if (event->button() != Qt::LeftButton || !document_) {
        return;
    }

    if (event->modifiers().testFlag(Qt::AltModifier)) {
        alt_orbiting_ = true;
        orbiting_ = false;
        panning_ = false;
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        highlighted_transform_axis_ = TransformAxis::None;
        return;
    }

    if (tool_ == ToolMode::DrawCurve) {
        DrawCurveAt(event->pos());
        return;
    }

    if (tool_ == ToolMode::Boolean) {
        HandleBooleanClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::Transform) {
        HandleTransformClick(event->pos(), event->modifiers().testFlag(Qt::ControlModifier));
        return;
    }

    if (tool_ == ToolMode::Select) {
        SelectionAction action = SelectionAction::Replace;
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            action = SelectionAction::Add;
        } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
            action = SelectionAction::Remove;
        }
        SelectAt(event->pos(), action);
        return;
    }

    orbiting_ = true;
}

void OpenGLViewport::mouseMoveEvent(QMouseEvent* event) {
    const QPoint delta = event->pos() - last_mouse_;

    if (dragging_transform_ && tool_ == ToolMode::Transform) {
        HandleTransformDrag(event->pos());
        return;
    }

    if (tool_ == ToolMode::Transform && document_ && document_->HasSelection()) {
        const TransformAxis hovered_axis = HitTestTransformGizmo(event->pos());
        if (hovered_axis != highlighted_transform_axis_) {
            highlighted_transform_axis_ = hovered_axis;
            update();
        }
    }

    if (orbiting_ || alt_orbiting_) {
        camera_.yaw -= static_cast<float>(delta.x()) * 0.35f;
        camera_.pitch += static_cast<float>(delta.y()) * 0.25f;
        if (orbit_mode_ == OrbitMode::Architectural) {
            camera_.pitch = std::clamp(camera_.pitch, -10.0f, 75.0f);
        } else if (std::fabs(camera_.pitch) > 360.0f) {
            camera_.pitch = std::fmod(camera_.pitch, 360.0f);
        }
        last_mouse_ = event->pos();
        update();
        return;
    }

    if (panning_) {
        const float yaw = deg_to_rad(camera_.yaw);
        const float scale = camera_.distance * 0.0018f;
        const Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
        const Vec3 forward_ground{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        camera_.target = camera_.target - right * (static_cast<float>(delta.x()) * scale) + forward_ground * (static_cast<float>(delta.y()) * scale);
        last_mouse_ = event->pos();
        update();
    }
}

void OpenGLViewport::mouseReleaseEvent(QMouseEvent*) {
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    dragging_transform_ = false;
    active_transform_axis_ = TransformAxis::None;
    highlighted_transform_axis_ = TransformAxis::None;
    update();
}

void OpenGLViewport::wheelEvent(QWheelEvent* event) {
    const float wheel_steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float zoom_factor = std::pow(1.12f, -wheel_steps);
    camera_.distance *= zoom_factor;
    camera_.distance = std::clamp(camera_.distance, 0.25f, 100000.0f);
    update();
}

void OpenGLViewport::SelectAt(const QPoint& point, SelectionAction action) {
    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    if (document_->SelectSolidEdgeAtScreen(screen_point, world_to_screen, 10.0f, action)) {
        emit SelectionChanged();
        emit DocumentChanged();
        update();
        return;
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }

    if (action == SelectionAction::Add) {
        document_->AddObjectToSelectionAt(scene_point, 0.35f);
    } else if (action == SelectionAction::Remove) {
        document_->RemoveObjectFromSelectionAt(scene_point, 0.35f);
    } else if (!document_->SelectPointAt(scene_point, 0.28f)) {
        document_->SelectObjectAt(scene_point, 0.35f);
        document_->SelectPointAt(scene_point, 0.28f);
    }

    emit SelectionChanged();
    emit DocumentChanged();
    update();
}

void OpenGLViewport::DrawCurveAt(const QPoint& point) {
    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }

    document_->AddCurvePoint(scene_point);
    emit DocumentChanged();
    update();
}

void OpenGLViewport::HandleBooleanClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        emit StatusTextChanged(has_boolean_body_ ? "Boolean: выбери tool body" : "Boolean: выбери body");
        return;
    }

    if (!document_->SelectObjectAt(scene_point, 0.35f) || !dynamic_cast<CSolid*>(document_->GetSelectedObject())) {
        emit SelectionChanged();
        emit DocumentChanged();
        emit StatusTextChanged(has_boolean_body_ ? "Boolean: tool должен быть телом" : "Boolean: body должен быть телом");
        update();
        return;
    }

    const size_t clicked_index = document_->GetSelectedObjectIndex();
    emit SelectionChanged();
    emit DocumentChanged();

    if (!has_boolean_body_) {
        boolean_body_index_ = clicked_index;
        has_boolean_body_ = true;
        emit StatusTextChanged("Boolean: выбери tool body");
        update();
        return;
    }

    if (clicked_index == boolean_body_index_) {
        emit StatusTextChanged("Boolean: выбери другое тело для tool");
        update();
        return;
    }

    const bool applied = document_->ApplyBooleanToSolids(boolean_body_index_, clicked_index, boolean_operation_);
    if (!applied) {
        emit StatusTextChanged("Boolean: операция не выполнена");
        update();
        return;
    }

    has_boolean_body_ = false;
    boolean_body_index_ = 0;
    tool_ = ToolMode::Select;
    emit SelectionChanged();
    emit DocumentChanged();
    emit BooleanFinished();
    emit StatusTextChanged("Boolean: операция выполнена");
    update();
}

void OpenGLViewport::HandleTransformClick(const QPoint& point, bool add_to_selection) {
    if (document_->HasSelection()) {
        const TransformAxis axis = HitTestTransformGizmo(point);
        if (axis != TransformAxis::None) {
            active_transform_axis_ = axis;
            highlighted_transform_axis_ = axis;
            dragging_transform_ = true;
            last_mouse_ = point;
            update();
            return;
        }
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }

    if (add_to_selection) {
        document_->ToggleObjectSelectionAt(scene_point, 0.35f);
    } else {
        document_->SelectObjectAt(scene_point, 0.35f);
    }

    emit SelectionChanged();
    emit DocumentChanged();
    update();
}

void OpenGLViewport::HandleTransformDrag(const QPoint& point) {
    if (!document_ || active_transform_axis_ == TransformAxis::None) {
        return;
    }

    Vec3 center{};
    if (!document_->GetSelectionCenter(center)) {
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        return;
    }

    const Vec3 axis = AxisVector(active_transform_axis_);
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    DomPoint center_screen{};
    DomPoint axis_screen{};
    if (!renderer_.WorldToScreen(center, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(center + axis * gizmo_size, camera_, orthographic_projection_, width(), height(), axis_screen)) {
        return;
    }

    const float axis_dx = static_cast<float>(axis_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(axis_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    if (axis_len_sq <= 0.0001f) {
        return;
    }

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());
    const float pixels_along_axis = (mouse_dx * axis_dx + mouse_dy * axis_dy) / std::sqrt(axis_len_sq);
    const float pixels_per_world = std::sqrt(axis_len_sq) / gizmo_size;
    const float world_delta = pixels_along_axis / pixels_per_world;

    bool transformed = false;
    if (transform_operation_ == TransformOperation::Move) {
        transformed = document_->MoveSelectedObjects(axis * world_delta);
    } else if (transform_operation_ == TransformOperation::Rotate) {
        transformed = document_->RotateSelectedObjects(center, axis, pixels_along_axis * 0.01f);
    } else {
        const float factor = std::clamp(1.0f + pixels_along_axis * 0.01f, 0.05f, 20.0f);
        transformed = document_->ScaleSelectedObjects(center, axis, factor);
    }

    if (std::fabs(pixels_along_axis) > 0.0001f && transformed) {
        last_mouse_ = point;
        emit DocumentChanged();
        update();
    }
}

TransformAxis OpenGLViewport::HitTestTransformGizmo(const QPoint& point) const {
    if (!document_) {
        return TransformAxis::None;
    }

    Vec3 center{};
    if (!document_->GetSelectionCenter(center)) {
        return TransformAxis::None;
    }

    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    DomPoint center_screen{};
    if (!renderer_.WorldToScreen(center, camera_, orthographic_projection_, width(), height(), center_screen)) {
        return TransformAxis::None;
    }

    TransformAxis best_axis = TransformAxis::None;
    float best_distance = 12.0f;
    const TransformAxis axes[] = {TransformAxis::X, TransformAxis::Y, TransformAxis::Z};
    for (TransformAxis axis : axes) {
        DomPoint axis_end{};
        if (!renderer_.WorldToScreen(center + AxisVector(axis) * gizmo_size, camera_, orthographic_projection_, width(), height(), axis_end)) {
            continue;
        }

        const DomPoint mouse_point{point.x(), point.y()};
        const float distance = DistanceToScreenSegment(mouse_point, center_screen, axis_end);
        if (distance < best_distance) {
            best_distance = distance;
            best_axis = axis;
        }
    }

    return best_axis;
}

Vec3 OpenGLViewport::AxisVector(TransformAxis axis) const {
    if (axis == TransformAxis::X) {
        return {1.0f, 0.0f, 0.0f};
    }
    if (axis == TransformAxis::Y) {
        return {0.0f, 1.0f, 0.0f};
    }
    if (axis == TransformAxis::Z) {
        return {0.0f, 0.0f, 1.0f};
    }
    return {};
}

float OpenGLViewport::DistanceToScreenSegment(DomPoint point, DomPoint start, DomPoint end) const {
    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    const float length_sq = dx * dx + dy * dy;
    if (length_sq <= 0.0001f) {
        const float px = static_cast<float>(point.x - start.x);
        const float py = static_cast<float>(point.y - start.y);
        return std::sqrt(px * px + py * py);
    }

    const float t = std::clamp((static_cast<float>(point.x - start.x) * dx + static_cast<float>(point.y - start.y) * dy) / length_sq, 0.0f, 1.0f);
    const float closest_x = static_cast<float>(start.x) + t * dx;
    const float closest_y = static_cast<float>(start.y) + t * dy;
    const float px = static_cast<float>(point.x) - closest_x;
    const float py = static_cast<float>(point.y) - closest_y;
    return std::sqrt(px * px + py * py);
}

void OpenGLViewport::DrawCoordinateAxisLabels() {
    DomPoint x_screen{};
    DomPoint y_screen{};
    DomPoint z_screen{};
    const bool has_x = renderer_.WorldToScreen({kGridHalfSize, 0.02f, 0.0f}, camera_, orthographic_projection_, width(), height(), x_screen);
    const bool has_y = renderer_.WorldToScreen({0.0f, 0.02f, kGridHalfSize}, camera_, orthographic_projection_, width(), height(), y_screen);
    const bool has_z = renderer_.WorldToScreen({0.0f, kGridHalfSize, 0.0f}, camera_, orthographic_projection_, width(), height(), z_screen);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont label_font = painter.font();
    label_font.setBold(true);
    label_font.setPointSize(9);
    painter.setFont(label_font);

    const auto draw_label = [&painter](const DomPoint& point, const QColor& color, const QString& text) {
        painter.setPen(color);
        painter.drawText(QPoint(point.x + 5, point.y - 5), text);
    };

    if (has_x) {
        draw_label(x_screen, QColor(255, 20, 18), "X");
    }
    if (has_y) {
        draw_label(y_screen, QColor(40, 255, 45), "Y");
    }
    if (has_z) {
        draw_label(z_screen, QColor(55, 85, 255), "Z");
    }
}
