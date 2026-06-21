#include <windows.h>
#include "OpenGLViewport.h"


#include "../solid/Solid.h"
#include "MaterialDrag.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>


namespace {
constexpr float kGridHalfSize = 12.0f;
constexpr double kCurvePlaneY = 0.08;

void viewport_camera_basis(const Camera& camera, Vec3& forward, Vec3& right, Vec3& up) {
    forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    up = normalize(rotate(camera.orientation, {0.0f, 1.0f, 0.0f}));
}

void rotation_arc_basis(Vec3 axis, Vec3 camera_forward, Vec3& tangent, Vec3& bitangent) {
    tangent = normalize(cross(axis, camera_forward));
    if (std::fabs(tangent.x) <= 0.00001f && std::fabs(tangent.y) <= 0.00001f && std::fabs(tangent.z) <= 0.00001f) {
        tangent = normalize(cross(axis, {0.0f, 1.0f, 0.0f}));
    }
    if (std::fabs(tangent.x) <= 0.00001f && std::fabs(tangent.y) <= 0.00001f && std::fabs(tangent.z) <= 0.00001f) {
        tangent = normalize(cross(axis, {0.0f, 0.0f, 1.0f}));
    }
    bitangent = normalize(cross(axis, tangent));
}

Vec3 point_to_vec3(const CPoint3d& point) {
    return {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
}

bool plane_from_points(const std::vector<CPoint3d>& points, Vec3& plane_point, Vec3& plane_normal) {
    if (points.size() < 3) {
        return false;
    }

    Vec3 normal{};
    for (size_t i = 0; i < points.size(); ++i) {
        const CPoint3d& current = points[i];
        const CPoint3d& next = points[(i + 1) % points.size()];
        normal.x += static_cast<float>((current.y - next.y) * (current.z + next.z));
        normal.y += static_cast<float>((current.z - next.z) * (current.x + next.x));
        normal.z += static_cast<float>((current.x - next.x) * (current.y + next.y));
    }

    normal = normalize(normal);
    if (std::fabs(normal.x) <= 0.00001f && std::fabs(normal.y) <= 0.00001f && std::fabs(normal.z) <= 0.00001f) {
        return false;
    }

    plane_point = point_to_vec3(points.front());
    plane_normal = normal;
    return true;
}

Quaternion quaternion_from_basis(Vec3 right, Vec3 up, Vec3 back) {
    const float m00 = right.x;
    const float m01 = up.x;
    const float m02 = back.x;
    const float m10 = right.y;
    const float m11 = up.y;
    const float m12 = back.y;
    const float m20 = right.z;
    const float m21 = up.z;
    const float m22 = back.z;
    const float trace = m00 + m11 + m22;

    Quaternion q{};
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return normalize_quaternion(q);
}

Quaternion z_up_orientation_from_forward(Vec3 forward, Vec3 fallback_right) {
    constexpr Vec3 kWorldUp{0.0f, 0.0f, 1.0f};
    forward = normalize(forward);
    const Vec3 back = forward * -1.0f;
    Vec3 right = normalize(cross(kWorldUp, back));
    if (dot(right, right) <= 0.00001f) {
        right = normalize(fallback_right - back * dot(fallback_right, back));
    }
    if (dot(right, right) <= 0.00001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 up = normalize(cross(back, right));
    return quaternion_from_basis(right, up, back);
}

Quaternion orientation_from_forward_up(Vec3 forward, Vec3 desired_up) {
    forward = normalize(forward);
    const Vec3 back = forward * -1.0f;
    Vec3 up = normalize(desired_up - back * dot(desired_up, back));
    if (dot(up, up) <= 0.00001f) {
        up = {0.0f, 1.0f, 0.0f};
    }
    Vec3 right = normalize(cross(up, back));
    if (dot(right, right) <= 0.00001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    up = normalize(cross(back, right));
    return quaternion_from_basis(right, up, back);
}

void cad_orbit_camera(Camera& camera, float yaw_delta_degrees, float pitch_delta_degrees) {
    const Vec3 right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    const Vec3 up = normalize(rotate(camera.orientation, {0.0f, 1.0f, 0.0f}));
    const Quaternion yaw_delta = quaternion_from_axis_angle(up, deg_to_rad(yaw_delta_degrees));
    const Quaternion pitch_delta = quaternion_from_axis_angle(right, deg_to_rad(-pitch_delta_degrees));
    camera.orientation = normalize_quaternion(pitch_delta * yaw_delta * camera.orientation);
}

void architectural_orbit_camera(Camera& camera, float yaw_delta_degrees, float pitch_delta_degrees) {
    constexpr Vec3 kWorldUp{0.0f, 0.0f, 1.0f};
    Vec3 forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    const Vec3 current_right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));

    forward = normalize(rotate_around_axis(forward, kWorldUp, deg_to_rad(yaw_delta_degrees)));
    const float horizontal = std::sqrt(forward.x * forward.x + forward.y * forward.y);
    const float current_pitch = std::atan2(-forward.z, horizontal) * 180.0f / kPi;
    const float target_pitch = std::clamp(current_pitch + pitch_delta_degrees, -10.0f, 89.0f);
    Vec3 horizontal_forward{forward.x, forward.y, 0.0f};
    if (dot(horizontal_forward, horizontal_forward) <= 0.00001f) {
        horizontal_forward = normalize(cross(kWorldUp, current_right));
    } else {
        horizontal_forward = normalize(horizontal_forward);
    }
    if (dot(horizontal_forward, horizontal_forward) <= 0.00001f) {
        horizontal_forward = {0.0f, -1.0f, 0.0f};
    }

    const float pitch = deg_to_rad(target_pitch);
    forward = normalize(horizontal_forward * std::cos(pitch) + Vec3{0.0f, 0.0f, -1.0f} * std::sin(pitch));
    camera.orientation = z_up_orientation_from_forward(forward, current_right);
}

void set_view_by_camera_ray(Camera& camera) {
    const Vec3 forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    const Vec3 axes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, -1.0f}
    };

    Vec3 best_axis = axes[0];
    float best_dot = dot(forward, axes[0]);
    for (size_t i = 1; i < std::size(axes); ++i) {
        const float value = dot(forward, axes[i]);
        if (value > best_dot) {
            best_dot = value;
            best_axis = axes[i];
        }
    }

    const Vec3 desired_up = std::fabs(best_axis.z) > 0.5f
        ? Vec3{0.0f, 1.0f, 0.0f}
        : Vec3{0.0f, 0.0f, 1.0f};
    camera.orientation = orientation_from_forward_up(best_axis, desired_up);
}
}

OpenGLViewport::OpenGLViewport(QWidget* parent)
    : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumSize(640, 420);
}

void OpenGLViewport::SetDocument(CAlfaDoc* document) {
    document_ = document;
    update();
}

void OpenGLViewport::SetTool(ToolMode tool) {
    const bool changed = tool_ != tool;
    tool_ = tool;
    if (tool_ != ToolMode::Boolean) {
        has_boolean_body_ = false;
        boolean_body_index_ = 0;
    }
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    dragging_transform_ = false;
    dragging_face_extrude_ = false;
    dragging_draft_face_ = false;
    dragging_polyline_point_ = false;
    curve_point_drag_has_plane_ = false;
    curve_preview_valid_ = false;
    if (tool_ != ToolMode::Select) {
        editing_polyline_ = false;
        highlighted_polyline_handle_ = false;
    }
    if (tool_ != ToolMode::SketchRectangle) {
        sketch_rectangle_has_first_point_ = false;
        sketch_rectangle_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchFillet) {
        highlighted_sketch_fillet_point_ = false;
    }
    highlighted_draft_face_gizmo_ = false;
    face_extrude_distance_ = 0.0f;
    draft_face_angle_degrees_ = 0.0;
    active_transform_axis_ = TransformAxis::None;
    highlighted_transform_axis_ = TransformAxis::None;
    if (material_interaction_mode_ == MaterialInteractionMode::None) {
        if (tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline || tool_ == ToolMode::EditPoint) {
            setCursor(Qt::CrossCursor);
        } else if (tool_ == ToolMode::SketchRectangle) {
            setCursor(Qt::CrossCursor);
        } else if (tool_ == ToolMode::SketchFillet) {
            setCursor(Qt::CrossCursor);
        } else {
            unsetCursor();
        }
    }
    if (changed) {
        emit ToolModeChanged(tool_);
    }
    update();
}

void OpenGLViewport::SetTransformOperation(TransformOperation operation) {
    transform_operation_ = operation;
    SetTool(ToolMode::Transform);
}

void OpenGLViewport::BeginFaceExtrudeTool(double taper_angle_degrees) {
    face_extrude_taper_angle_degrees_ = taper_angle_degrees;
    SetTool(ToolMode::FaceExtrude);
    if (document_ && document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_)) {
        emit StatusTextChanged("Extrude Face: drag the orange normal gizmo");
    } else {
        emit StatusTextChanged("Extrude Face: select a planar face");
    }
}

void OpenGLViewport::BeginDraftFaceTool() {
    SetTool(ToolMode::DraftFace);
    if (document_ && document_->BeginDraftFaceFromSelectedFace()) {
        emit StatusTextChanged("Draft Face: choose a straight edge axis");
    } else {
        emit StatusTextChanged("Draft Face: select a planar face");
    }
}

void OpenGLViewport::BeginThickSolidTool(double thickness) {
    thick_solid_thickness_ = thickness;
    SetTool(ToolMode::ThickSolid);
    if (document_ && document_->BeginLiveThickSolidFromSelectedFaces(thick_solid_thickness_)) {
        emit StatusTextChanged("ThickSolid: меняй Thick, OK оставит результат");
    } else if (document_ && document_->GetSelectedSolid()) {
        document_->BeginLiveThickSolidFromSelectedSolid(thick_solid_thickness_);
        emit StatusTextChanged("ThickSolid: выбери Face");
    } else {
        emit StatusTextChanged("ThickSolid: выбери CSolid");
    }
}

void OpenGLViewport::SetThickSolidThickness(double thickness) {
    thick_solid_thickness_ = thickness;
    if (tool_ == ToolMode::ThickSolid && document_ && document_->HasLiveThickSolid()) {
        document_->UpdateLiveThickSolid(thickness);
    }
}

void OpenGLViewport::BeginBooleanTool(BooleanOperation operation) {
    boolean_operation_ = operation;
    has_boolean_body_ = false;
    boolean_body_index_ = 0;
    SetTool(ToolMode::Boolean);
    emit StatusTextChanged("Boolean: выбери body");
}

SelectionMode OpenGLViewport::GetSelectionMode() const {
    return selection_mode_;
}

void OpenGLViewport::SetSelectionMode(SelectionMode mode) {
    if (selection_mode_ == mode) {
        return;
    }
    const bool converted_faces_to_edges =
        document_
        && selection_mode_ == SelectionMode::Face
        && mode == SelectionMode::Edge
        && document_->SelectEdgesOfSelectedFaces();
    selection_mode_ = mode;
    if (document_) {
        if (!converted_faces_to_edges) {
            document_->ClearSelection();
        }
        emit SelectionChanged();
    }
    update();
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

Camera OpenGLViewport::GetCamera() const {
    return camera_;
}

void OpenGLViewport::SetCamera(const Camera& camera) {
    camera_ = camera;
    update();
}

void OpenGLViewport::SetOrthographicProjection(bool enabled) {
    if (xy_plane_view_enabled_ && !enabled) {
        enabled = true;
    }
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
    update();
}

bool OpenGLViewport::IsXYPlaneViewEnabled() const {
    return xy_plane_view_enabled_;
}

void OpenGLViewport::SetXYPlaneViewEnabled(bool enabled) {
    if (xy_plane_view_enabled_ == enabled) {
        return;
    }
    xy_plane_view_enabled_ = enabled;
    orbiting_ = false;
    alt_orbiting_ = false;
    zooming_ = false;
    if (enabled) {
        orthographic_projection_ = true;
        camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, 0.0f);
        camera_.target = {kGridHalfSize * 0.5f, kGridHalfSize * 0.5f, 0.0f};
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

bool OpenGLViewport::IsFloorGridVisible() const {
    return show_floor_grid_;
}

void OpenGLViewport::SetFloorGridVisible(bool visible) {
    if (show_floor_grid_ == visible) {
        return;
    }
    show_floor_grid_ = visible;
    update();
}

void OpenGLViewport::BeginMaterialPaint(const Material& material) {
    active_paint_material_ = material;
    material_interaction_mode_ = MaterialInteractionMode::Paint;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(QString("Material brush: click object to paint with %1").arg(QString::fromStdString(material.name)));
}

void OpenGLViewport::BeginMaterialPick() {
    material_interaction_mode_ = MaterialInteractionMode::Pick;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("Material picker: click object");
}

void OpenGLViewport::CancelMaterialInteraction() {
    if (material_interaction_mode_ == MaterialInteractionMode::None) {
        return;
    }

    material_interaction_mode_ = MaterialInteractionMode::None;
    unsetCursor();
    emit StatusTextChanged("Material tool canceled");
    update();
}

void OpenGLViewport::BeginSketch(const QString& name, SketchPlane plane) {
    sketch_active_ = true;
    sketch_name_ = name;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    orthographic_projection_ = true;
    xy_plane_view_enabled_ = false;

    sketch_origin_ = {};
    if (plane == SketchPlane::XY) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 1.0f, 0.0f};
        sketch_normal_ = {0.0f, 0.0f, 1.0f};
        camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, 0.0f);
    } else if (plane == SketchPlane::XZ) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {0.0f, 1.0f, 0.0f};
        camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, -89.9f);
    } else {
        sketch_u_ = {0.0f, 1.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {1.0f, 0.0f, 0.0f};
        camera_.orientation = camera_orientation_from_yaw_pitch(-90.0f, 0.0f);
    }

    camera_.target = sketch_origin_;
    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(QString("%1: Rectangle, click first corner").arg(sketch_name_));
}

void OpenGLViewport::SetSketchRectangleTool() {
    if (!sketch_active_) {
        return;
    }
    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(QString("%1: Rectangle, click first corner").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchFillet(double radius) {
    if (!sketch_active_) {
        return;
    }
    sketch_fillet_radius_ = radius;
    SetTool(ToolMode::SketchFillet);
    emit StatusTextChanged(QString("%1: Fillet R=%2. Укажите вершину полилинии").arg(sketch_name_).arg(sketch_fillet_radius_, 0, 'f', 2));
}

void OpenGLViewport::EndSketch() {
    sketch_active_ = false;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    if (tool_ == ToolMode::SketchRectangle || tool_ == ToolMode::SketchFillet) {
        SetTool(ToolMode::Select);
    } else {
        update();
    }
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
   UpdateFPS();
    renderer_.Render(*document_, camera_, orthographic_projection_, show_coordinate_axes_, show_floor_grid_, xy_plane_view_enabled_, tool_, transform_operation_, highlighted_transform_axis_, highlighted_draft_face_gizmo_, width(), height());
    if (show_coordinate_axes_) {
        DrawCoordinateAxisLabels();
    }
    if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline) && curve_preview_valid_) {
        DrawCurveRubberBand();
    }
    if (tool_ == ToolMode::SketchRectangle && sketch_rectangle_has_first_point_ && sketch_rectangle_preview_valid_) {
        DrawSketchRectanglePreview();
    }
    if (tool_ == ToolMode::EditPoint && document_) {
        DrawSelectedCurvePointHandles();
    }
    if (tool_ == ToolMode::EditPoint && selecting_edit_points_) {
        DrawEditPointSelectionRect();
    }
    if (material_drag_active_) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPixmap sphere = MaterialDrag::SpherePixmap(material_drag_preview_, 58, true);
        painter.drawPixmap(material_drag_pos_ - QPoint(sphere.width() / 2, sphere.height() / 2), sphere);
    }
    DrawFPS();

}

void OpenGLViewport::mousePressEvent(QMouseEvent* event) {
    last_mouse_ = event->pos();

    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        return;
    }

    if (event->button() == Qt::RightButton) {
        zooming_ = true;
        orbiting_ = false;
        alt_orbiting_ = false;
        panning_ = false;
        dragging_transform_ = false;
        return;
    }

    if (event->button() != Qt::LeftButton || !document_) {
        return;
    }

    if (material_interaction_mode_ != MaterialInteractionMode::None) {
        CAlfaObject* object = FindObjectForMaterialAt(event->pos());
        if (!object) {
            emit StatusTextChanged(material_interaction_mode_ == MaterialInteractionMode::Paint
                ? "Material brush: click object to paint"
                : "Material picker: click object");
            return;
        }

        if (material_interaction_mode_ == MaterialInteractionMode::Paint) {
            const Material& document_material = document_->UpsertMaterial(active_paint_material_);
            object->SetMaterial(document_material);
            object->SetMaterialId(document_material.id);
            document_->ClearSelection();
            emit SelectionChanged();
            emit DocumentChanged();
            emit StatusTextChanged(QString("Material applied: %1").arg(QString::fromStdString(document_material.name)));
            update();
            return;
        } else {
            const Material picked_material = object->GetMaterial();
            emit MaterialPicked(picked_material);
            document_->ClearSelection();
            emit SelectionChanged();
            emit StatusTextChanged(QString("Material picked: %1").arg(QString::fromStdString(picked_material.name)));
        }

        material_interaction_mode_ = MaterialInteractionMode::None;
        unsetCursor();
        update();
        return;
    }

    if (event->modifiers().testFlag(Qt::AltModifier) && !xy_plane_view_enabled_ && !sketch_active_) {
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

    if (tool_ == ToolMode::DrawBSpline) {
        DrawBSplineAt(event->pos());
        return;
    }

    if (tool_ == ToolMode::SketchRectangle) {
        HandleSketchRectangleClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SketchFillet) {
        HandleSketchFilletClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::EditPoint) {
        const DomPoint screen_point{event->pos().x(), event->pos().y()};
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        CPoint3d point{};
        const bool hit_selected_point = document_->PickSelectedCurvePointAtScreen(screen_point, world_to_screen, 11.0f, point);
        if (hit_selected_point || document_->SelectCurvePointAtScreen(screen_point, world_to_screen, 11.0f, SelectionAction::Replace)) {
            if (hit_selected_point || document_->GetSelectedPointPosition(point)) {
                BeginCurvePointDrag(point);
                emit SelectionChanged();
                update();
            }
        } else {
            selecting_edit_points_ = true;
            edit_point_selection_action_ = event->modifiers().testFlag(Qt::ShiftModifier)
                ? SelectionAction::Add
                : SelectionAction::Replace;
            edit_point_selection_start_ = event->pos();
            edit_point_selection_current_ = event->pos();
        }
        return;
    }

    if (tool_ == ToolMode::Boolean) {
        HandleBooleanClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::FaceExtrude) {
        HandleFaceExtrudeClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::DraftFace) {
        HandleDraftFaceClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::ThickSolid) {
        HandleThickSolidClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::Transform) {
        HandleTransformClick(event->pos(), event->modifiers().testFlag(Qt::ControlModifier));
        return;
    }

    if (tool_ == ToolMode::Select) {
        if (editing_polyline_ && HitTestSelectedPolylineHandle(event->pos())) {
            const DomPoint screen_point{event->pos().x(), event->pos().y()};
            auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
                return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
            };
            document_->SelectCurvePointAtScreen(screen_point, world_to_screen, 10.0f);
            CPoint3d point{};
            if (document_->GetSelectedPointPosition(point)) {
                BeginCurvePointDrag(point);
                last_mouse_ = event->pos();
                emit SelectionChanged();
                update();
                return;
            }
        }

        SelectionAction action = SelectionAction::Replace;
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            action = SelectionAction::Add;
        } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
            action = SelectionAction::Remove;
        }
        SelectAt(event->pos(), action);
        return;
    }

    if (!xy_plane_view_enabled_ && !sketch_active_) {
        orbiting_ = true;
    }
}

void OpenGLViewport::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!document_ || (tool_ != ToolMode::Select && tool_ != ToolMode::Orbit)
        || event->button() != Qt::LeftButton) {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    const DomPoint screen_point{event->pos().x(), event->pos().y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (tool_ == ToolMode::Select
        && document_->SelectPolylineAtScreen(screen_point, world_to_screen, 8.0f, SelectionAction::Replace)) {
        editing_polyline_ = true;
        highlighted_polyline_handle_ = false;
        emit SelectionChanged();
        emit StatusTextChanged("Curve edit: drag handles, Esc to finish");
        update();
        event->accept();
        return;
    }

    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    bool selected_object = document_->SelectSolidMeshAtScreen(screen_point, project_world, SelectionAction::Replace)
        || document_->SelectMeshAtScreen(screen_point, project_world, SelectionAction::Replace);
    if (!selected_object) {
        CurvePoint scene_point{};
        selected_object = renderer_.ScreenToFloor(event->pos().x(), event->pos().y(), width(), height(), camera_, orthographic_projection_, scene_point)
            && document_->SelectObjectAt(scene_point, 0.35f, false);
    }

    if (selected_object) {
        emit SelectionChanged();
        emit ObjectDoubleClicked();
        update();
        event->accept();
        return;
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OpenGLViewport::mouseMoveEvent(QMouseEvent* event) {
    const QPoint delta = event->pos() - last_mouse_;

    if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline) && document_) {
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToCurvePlane(event->pos(), preview_point);
        if (preview_valid != curve_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - curve_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - curve_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - curve_preview_point_.z) > 0.0001))) {
            curve_preview_valid_ = preview_valid;
            curve_preview_point_ = preview_point;
            update();
        }
    }

    if (tool_ == ToolMode::SketchRectangle && sketch_rectangle_has_first_point_ && document_) {
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid != sketch_rectangle_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - sketch_rectangle_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - sketch_rectangle_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - sketch_rectangle_preview_point_.z) > 0.0001))) {
            sketch_rectangle_preview_valid_ = preview_valid;
            sketch_rectangle_preview_point_ = preview_point;
            update();
        }
    }

    if (tool_ == ToolMode::SketchFillet && document_) {
        const DomPoint screen_point{event->pos().x(), event->pos().y()};
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        size_t object_index = 0;
        size_t point_index = 0;
        const bool hovered = document_->FindPolylinePointAtScreen(screen_point, world_to_screen, 40.0f, object_index, point_index);
        if (hovered != highlighted_sketch_fillet_point_) {
            highlighted_sketch_fillet_point_ = hovered;
            if (hovered) {
                setCursor(Qt::PointingHandCursor);
            } else {
                setCursor(Qt::CrossCursor);
            }
        }
    }

    if (dragging_polyline_point_
        && ((tool_ == ToolMode::Select && editing_polyline_) || tool_ == ToolMode::EditPoint)
        && document_) {
        CPoint3d point{};
        bool has_point = false;
        if (curve_point_drag_has_plane_) {
            has_point = ScreenToWorldPlane(event->pos(), curve_point_drag_plane_point_, curve_point_drag_plane_normal_, point);
        }
        if (!has_point) {
            has_point = xy_plane_view_enabled_
                ? ScreenToPlaneY(event->pos(), polyline_drag_plane_y_, point)
                : ScreenToViewPlane(event->pos(), curve_point_drag_anchor_, point);
        }
        if (has_point) {
            const Vec3 move_delta{
                static_cast<float>(point.x - curve_point_drag_last_.x),
                static_cast<float>(point.y - curve_point_drag_last_.y),
                static_cast<float>(point.z - curve_point_drag_last_.z)
            };
            const std::vector<CPoint3d> selected_points = document_->GetSelectedCurvePointPositions();
            const bool moved = selected_points.size() > 1
                ? document_->MoveSelectedCurvePoints(move_delta)
                : document_->MoveSelectedPoint(point);
            if (moved) {
                curve_point_drag_last_ = point;
                emit DocumentChanged();
                update();
            }
        }
        last_mouse_ = event->pos();
        return;
    }

    if (selecting_edit_points_ && tool_ == ToolMode::EditPoint) {
        edit_point_selection_current_ = event->pos();
        update();
        last_mouse_ = event->pos();
        return;
    }

    if (dragging_face_extrude_ && tool_ == ToolMode::FaceExtrude) {
        HandleFaceExtrudeDrag(event->pos());
        return;
    }

    if (dragging_draft_face_ && tool_ == ToolMode::DraftFace) {
        HandleDraftFaceDrag(event->pos());
        return;
    }

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

    if (tool_ == ToolMode::Select && editing_polyline_ && document_) {
        const bool hovered = HitTestSelectedPolylineHandle(event->pos());
        if (hovered != highlighted_polyline_handle_) {
            highlighted_polyline_handle_ = hovered;
            if (hovered) {
                setCursor(Qt::CrossCursor);
            } else if (material_interaction_mode_ == MaterialInteractionMode::None) {
                unsetCursor();
            }
            update();
        }
    }

    if (tool_ == ToolMode::DraftFace && document_ && document_->HasDraftFaceAxis()) {
        const bool hovered = HitTestDraftFaceGizmo(event->pos());
        if (hovered != highlighted_draft_face_gizmo_) {
            highlighted_draft_face_gizmo_ = hovered;
            update();
        }
    }

    if (!xy_plane_view_enabled_ && (orbiting_ || alt_orbiting_)) {
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            set_view_by_camera_ray(camera_);
        } else {
            const float yaw_delta = -static_cast<float>(delta.x()) * 0.35f;
            const float pitch_delta = static_cast<float>(delta.y()) * 0.25f;
            if (orbit_mode_ == OrbitMode::Architectural) {
                architectural_orbit_camera(camera_, yaw_delta, pitch_delta);
            } else {
                cad_orbit_camera(camera_, yaw_delta, pitch_delta);
            }
        }
        last_mouse_ = event->pos();
        update();
        return;
    }

    if (panning_) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        const int viewport_height = std::max(1, height());
        float world_per_pixel = camera_.distance * 0.0018f;
        if (orthographic_projection_) {
            const float half_height = std::max(0.25f, camera_.distance * 0.42f);
            world_per_pixel = (2.0f * half_height) / static_cast<float>(viewport_height);
        } else {
            const float depth = std::max(0.001f, dot(camera_.target - camera_position(camera_), forward));
            world_per_pixel = (2.0f * depth * std::tan(deg_to_rad(48.0f) * 0.5f)) / static_cast<float>(viewport_height);
        }
        camera_.target = camera_.target - right * (static_cast<float>(delta.x()) * world_per_pixel)
            + up * (static_cast<float>(delta.y()) * world_per_pixel);
        last_mouse_ = event->pos();
        update();
        return;
    }

    if (zooming_) {
        const float zoom_factor = std::pow(1.01f, -static_cast<float>(delta.y()));
        camera_.distance *= zoom_factor;
        camera_.distance = std::clamp(camera_.distance, 0.25f, 100000.0f);
        last_mouse_ = event->pos();
        update();
    }
}

void OpenGLViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (selecting_edit_points_ && tool_ == ToolMode::EditPoint && document_) {
        edit_point_selection_current_ = event->pos();
        const DomRect rect{
            edit_point_selection_start_.x(),
            edit_point_selection_start_.y(),
            edit_point_selection_current_.x(),
            edit_point_selection_current_.y()
        };
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        document_->SelectCurvePointsInScreenRect(rect, world_to_screen, edit_point_selection_action_);
        selecting_edit_points_ = false;
        emit SelectionChanged();
        update();
    }
    if (dragging_face_extrude_ && tool_ == ToolMode::FaceExtrude) {
        CommitFaceExtrudeDrag();
    }
    if (dragging_draft_face_ && tool_ == ToolMode::DraftFace) {
        CommitDraftFaceDrag();
    }
    if (dragging_transform_ && tool_ == ToolMode::Transform) {
        CommitTransformDrag();
    }
    dragging_polyline_point_ = false;
    curve_point_drag_has_plane_ = false;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    dragging_face_extrude_ = false;
    dragging_draft_face_ = false;
    selecting_edit_points_ = false;
    transform_drag_has_preview_ = false;
    transform_drag_move_delta_ = {};
    transform_drag_rotation_angle_ = 0.0f;
    transform_drag_scale_factor_ = 1.0f;
    active_transform_axis_ = TransformAxis::None;
    highlighted_transform_axis_ = TransformAxis::None;
    if (material_interaction_mode_ == MaterialInteractionMode::None) {
        if (tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline || tool_ == ToolMode::EditPoint) {
            setCursor(Qt::CrossCursor);
        } else if (tool_ == ToolMode::SketchRectangle) {
            setCursor(Qt::CrossCursor);
        } else {
            unsetCursor();
        }
    }
    update();
}

void OpenGLViewport::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchRectangle) {
        if (sketch_rectangle_has_first_point_) {
            sketch_rectangle_has_first_point_ = false;
            sketch_rectangle_preview_valid_ = false;
            emit StatusTextChanged("Sketch Rectangle: first point canceled");
            update();
        } else {
            EndSketch();
            emit StatusTextChanged("Sketch closed");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && material_interaction_mode_ != MaterialInteractionMode::None) {
        CancelMaterialInteraction();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && document_ && tool_ == ToolMode::DrawCurve) {
        if (document_->CloseSelectedOrActivePolyline()) {
            emit DocumentChanged();
            emit StatusTextChanged("Polyline closed");
            update();
        } else {
            emit StatusTextChanged("Polyline: need at least 3 points");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && document_ && tool_ == ToolMode::DrawBSpline) {
        if (document_->CloseSelectedOrActiveBSpline()) {
            curve_preview_valid_ = false;
            SetTool(ToolMode::Select);
            emit DocumentChanged();
            emit StatusTextChanged("B-Spline closed");
            update();
        } else {
            emit StatusTextChanged("B-Spline: need at least 3 points");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && (tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline)) {
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Curve tool canceled");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::EditPoint) {
        document_->ClearPointSelection();
        SetTool(ToolMode::Select);
        emit SelectionChanged();
        emit StatusTextChanged("Edit Point finished");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Select && editing_polyline_) {
        editing_polyline_ = false;
        dragging_polyline_point_ = false;
        highlighted_polyline_handle_ = false;
        if (material_interaction_mode_ == MaterialInteractionMode::None) {
            unsetCursor();
        }
        emit StatusTextChanged("Polyline edit finished");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && document_) {
        if (dragging_face_extrude_) {
            document_->CancelLiveExtrudeSelectedSolidFace();
            dragging_face_extrude_ = false;
            face_extrude_distance_ = 0.0f;
        } else if (dragging_draft_face_) {
            document_->CancelLiveDraftFace();
            dragging_draft_face_ = false;
            draft_face_angle_degrees_ = 0.0;
        } else if (tool_ == ToolMode::ThickSolid && document_->HasLiveThickSolid()) {
            document_->CancelLiveThickSolid();
            emit StatusTextChanged("ThickSolid canceled");
        } else {
            document_->ClearSelection();
        }
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        highlighted_transform_axis_ = TransformAxis::None;
        emit SelectionChanged();
        emit StatusTextChanged("Selection cleared");
        update();
        event->accept();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

void OpenGLViewport::wheelEvent(QWheelEvent* event) {
    const float wheel_steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float zoom_factor = std::pow(1.12f, -wheel_steps);
    camera_.distance *= zoom_factor;
    camera_.distance = std::clamp(camera_.distance, 0.25f, 100000.0f);
    update();
}

void OpenGLViewport::dragEnterEvent(QDragEnterEvent* event) {
    Material material;
    if (!MaterialDrag::Decode(event->mimeData(), material)) {
        event->ignore();
        return;
    }

    material_drag_preview_ = material;
    material_drag_pos_ = event->position().toPoint();
    material_drag_active_ = true;
    event->acceptProposedAction();
    update();
}

void OpenGLViewport::dragMoveEvent(QDragMoveEvent* event) {
    Material material;
    if (!MaterialDrag::Decode(event->mimeData(), material)) {
        event->ignore();
        return;
    }

    material_drag_preview_ = material;
    material_drag_pos_ = event->position().toPoint();
    event->acceptProposedAction();
    update();
}

void OpenGLViewport::dragLeaveEvent(QDragLeaveEvent* event) {
    material_drag_active_ = false;
    event->accept();
    update();
}

void OpenGLViewport::dropEvent(QDropEvent* event) {
    Material material;
    if (!MaterialDrag::Decode(event->mimeData(), material)) {
        event->ignore();
        return;
    }

    material_drag_active_ = false;
    if (ApplyMaterialDrop(event->position().toPoint(), material)) {
        if (document_) {
            document_->ClearSelection();
        }
        event->acceptProposedAction();
        emit SelectionChanged();
        emit DocumentChanged();
        emit StatusTextChanged(QString("Material applied: %1").arg(QString::fromStdString(material.name)));
    } else {
        event->ignore();
        emit StatusTextChanged("Material: drop on an object");
    }
    update();
}

void OpenGLViewport::SelectAt(const QPoint& point, SelectionAction action) {
    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (selection_mode_ == SelectionMode::Face) {
        if (document_->SelectSolidFaceAtScreen(screen_point, project_world, false, action)) {
            emit SelectionChanged();
            update();
            return;
        }
        emit StatusTextChanged("Select Face: face not found");
        return;
    }

    if (selection_mode_ == SelectionMode::Edge) {
        if (document_->SelectSolidEdgeAtScreen(screen_point, world_to_screen, 10.0f, action)) {
            emit SelectionChanged();
            update();
            return;
        }
        emit StatusTextChanged("Select Edge: edge not found");
        return;
    }

    if (selection_mode_ == SelectionMode::Object && document_->SelectSolidMeshAtScreen(screen_point, project_world, action)) {
        emit SelectionChanged();
        update();
        return;
    }

    if (selection_mode_ == SelectionMode::Object && document_->SelectMeshAtScreen(screen_point, project_world, action)) {
        emit SelectionChanged();
        update();
        return;
    }

    if (selection_mode_ == SelectionMode::Object
        && document_->SelectPolylineAtScreen(screen_point, world_to_screen, 8.0f, action)) {
        emit SelectionChanged();
        update();
        return;
    }

    if (selection_mode_ == SelectionMode::Point
        && document_->SelectPolylinePointAtScreen(screen_point, world_to_screen, 8.0f)) {
        emit SelectionChanged();
        update();
        return;
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }

    if (selection_mode_ == SelectionMode::Point) {
        document_->SelectPointAt(scene_point, 0.28f);
    } else {
        if (action == SelectionAction::Add) {
            document_->AddObjectToSelectionAt(scene_point, 0.35f, false);
        } else if (action == SelectionAction::Remove) {
            document_->RemoveObjectFromSelectionAt(scene_point, 0.35f, false);
        } else {
            document_->SelectObjectAt(scene_point, 0.35f, false);
        }
    }

    emit SelectionChanged();
    update();
}

bool OpenGLViewport::ApplyMaterialDrop(const QPoint& point, const Material& material) {
    if (!document_) {
        return false;
    }

    const auto apply_to_object = [this, &material](CAlfaObject* object) {
        if (!object) {
            return false;
        }
        const Material& document_material = document_->UpsertMaterial(material);
        object->SetMaterial(document_material);
        object->SetMaterialId(document_material.id);
        return true;
    };

    return apply_to_object(FindObjectForMaterialAt(point));
}

CAlfaObject* OpenGLViewport::FindObjectForMaterialAt(const QPoint& point) {
    if (!document_) {
        return nullptr;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    if (document_->SelectSolidMeshAtScreen(screen_point, project_world, SelectionAction::Replace)
        || document_->SelectMeshAtScreen(screen_point, project_world, SelectionAction::Replace)) {
        return document_->GetSelectedObject();
    }

    CurvePoint scene_point{};
    if (renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)
        && document_->SelectObjectAt(scene_point, 0.35f, false)) {
        return document_->GetSelectedObject();
    }

    auto& objects = document_->GetObjects();
    for (size_t i = objects.size(); i > 0; --i) {
        CAlfaObject* object = objects[i - 1].get();
        if (!object || !object->IsVisible()) {
            continue;
        }

        Vec3 min_point{};
        Vec3 max_point{};
        if (!object->GetBounds(min_point, max_point)) {
            continue;
        }

        const Vec3 corners[] = {
            {min_point.x, min_point.y, min_point.z},
            {max_point.x, min_point.y, min_point.z},
            {min_point.x, max_point.y, min_point.z},
            {max_point.x, max_point.y, min_point.z},
            {min_point.x, min_point.y, max_point.z},
            {max_point.x, min_point.y, max_point.z},
            {min_point.x, max_point.y, max_point.z},
            {max_point.x, max_point.y, max_point.z},
        };

        QRect screen_rect;
        bool has_projected_corner = false;
        for (const Vec3& corner : corners) {
            DomPoint screen{};
            if (!renderer_.WorldToScreen(corner, camera_, orthographic_projection_, width(), height(), screen)) {
                continue;
            }

            const QPoint corner_screen_point(screen.x, screen.y);
            if (!has_projected_corner) {
                screen_rect = QRect(corner_screen_point, QSize(1, 1));
                has_projected_corner = true;
            } else {
                screen_rect = screen_rect.united(QRect(corner_screen_point, QSize(1, 1)));
            }
        }

        if (has_projected_corner && screen_rect.adjusted(-12, -12, 12, 12).contains(point)) {
            return object;
        }
    }

    return nullptr;
}

void OpenGLViewport::DrawCurveAt(const QPoint& point) {
    CPoint3d scene_point{};
    if (!ScreenToCurvePlane(point, scene_point)) {
        return;
    }

    document_->AddCurvePoint(scene_point);
    curve_preview_point_ = scene_point;
    curve_preview_valid_ = true;
    emit DocumentChanged();
    update();
}

void OpenGLViewport::HandleBooleanClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    bool selected_solid = document_->SelectSolidMeshAtScreen(screen_point, project_world, SelectionAction::Replace);
    if (!selected_solid) {
        CurvePoint scene_point{};
        if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
            emit StatusTextChanged(has_boolean_body_ ? "Boolean: выбери tool body" : "Boolean: выбери body");
            return;
        }
        selected_solid = document_->SelectObjectAt(scene_point, 0.35f, false) && dynamic_cast<CSolid*>(document_->GetSelectedObject());
    }

    if (!selected_solid) {
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

void OpenGLViewport::DrawBSplineAt(const QPoint& point) {
    CPoint3d scene_point{};
    if (!ScreenToCurvePlane(point, scene_point)) {
        return;
    }

    document_->AddBSplinePoint(scene_point);
    curve_preview_point_ = scene_point;
    curve_preview_valid_ = true;
    emit DocumentChanged();
    update();
}

void OpenGLViewport::HandleFaceExtrudeClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_) && HitTestFaceExtrudeGizmo(point)) {
        if (!document_->BeginLiveExtrudeSelectedSolidFace(face_extrude_taper_angle_degrees_)) {
            emit StatusTextChanged("Extrude Face: operation failed");
            return;
        }
        dragging_face_extrude_ = true;
        face_extrude_distance_ = 0.0f;
        last_mouse_ = point;
        update();
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
        document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_);
        emit SelectionChanged();
        emit DocumentChanged();
        emit StatusTextChanged("Extrude Face: drag the orange normal gizmo");
        update();
        return;
    }

    emit StatusTextChanged("Extrude Face: select a planar face");
    update();
}

void OpenGLViewport::HandleFaceExtrudeDrag(const QPoint& point) {
    if (!document_ || !document_->IsLiveExtrudeSelectedSolidFaceActive()) {
        dragging_face_extrude_ = false;
        return;
    }

    DomPoint center_screen{};
    DomPoint end_screen{};
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    if (!renderer_.WorldToScreen(face_extrude_center_, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(face_extrude_center_ + face_extrude_normal_ * gizmo_size, camera_, orthographic_projection_, width(), height(), end_screen)) {
        return;
    }

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());
    const float axis_dx = static_cast<float>(end_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(end_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    if (axis_len_sq <= 0.0001f) {
        return;
    }

    const float axis_len = std::sqrt(axis_len_sq);
    const float pixels_along_axis = (mouse_dx * axis_dx + mouse_dy * axis_dy) / axis_len;
    const float pixels_per_world = axis_len / gizmo_size;
    const float world_delta = pixels_along_axis / pixels_per_world;
    const float next_distance = face_extrude_distance_ + world_delta;
    if (std::fabs(world_delta) > 0.0001f && document_->UpdateLiveExtrudeSelectedSolidFace(next_distance)) {
        face_extrude_center_ = face_extrude_center_ + face_extrude_normal_ * world_delta;
        face_extrude_distance_ += world_delta;
        last_mouse_ = point;
        emit DocumentChanged();
        update();
    }
}

void OpenGLViewport::CommitFaceExtrudeDrag() {
    if (!document_) {
        return;
    }

    document_->FinishLiveExtrudeSelectedSolidFace();
    face_extrude_distance_ = 0.0f;
    emit SelectionChanged();
    emit DocumentChanged();
    emit StatusTextChanged("Extrude Face: done");
}

bool OpenGLViewport::HitTestFaceExtrudeGizmo(const QPoint& point) const {
    if (!document_ || !document_->HasSelectedSolidFace()) {
        return false;
    }

    DomPoint center_screen{};
    DomPoint end_screen{};
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    if (!renderer_.WorldToScreen(face_extrude_center_, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(face_extrude_center_ + face_extrude_normal_ * gizmo_size, camera_, orthographic_projection_, width(), height(), end_screen)) {
        return false;
    }

    return DistanceToScreenSegment({point.x(), point.y()}, center_screen, end_screen) <= 12.0f;
}

void OpenGLViewport::HandleDraftFaceClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (document_->GetDraftFaceAxis(draft_face_axis_center_, draft_face_axis_dir_) && HitTestDraftFaceGizmo(point)) {
        if (!document_->BeginLiveDraftFace()) {
            emit StatusTextChanged("Draft Face: operation failed");
            return;
        }
        DomPoint center_screen{};
        if (renderer_.WorldToScreen(draft_face_axis_center_, camera_, orthographic_projection_, width(), height(), center_screen)) {
            draft_face_start_mouse_angle_ = std::atan2(static_cast<double>(point.y() - center_screen.y),
                                                       static_cast<double>(point.x() - center_screen.x));
        } else {
            draft_face_start_mouse_angle_ = 0.0;
        }
        dragging_draft_face_ = true;
        draft_face_angle_degrees_ = 0.0;
        last_mouse_ = point;
        update();
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (!document_->HasDraftFace()) {
        if (document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            if (document_->BeginDraftFaceFromSelectedFace()) {
                emit SelectionChanged();
                emit DocumentChanged();
                emit StatusTextChanged("Draft Face: choose a straight edge axis");
                update();
                return;
            }
            emit StatusTextChanged("Draft Face: unavailable after Non Uniform Scale");
            update();
            return;
        }
        emit StatusTextChanged("Draft Face: select a planar face");
        update();
        return;
    }

    if (document_->SelectDraftFaceAxisEdgeAtScreen(screen_point, world_to_screen, 10.0f)) {
        document_->GetDraftFaceAxis(draft_face_axis_center_, draft_face_axis_dir_);
        emit SelectionChanged();
        emit DocumentChanged();
        emit StatusTextChanged("Draft Face: drag the rotation gizmo");
        update();
        return;
    }

    emit StatusTextChanged("Draft Face: choose a straight edge on selected face");
    update();
}

void OpenGLViewport::HandleThickSolidClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (!document_->HasLiveThickSolid()) {
        if (document_->SelectSolidMeshAtScreen(screen_point, project_world, SelectionAction::Replace)
            && document_->BeginLiveThickSolidFromSelectedSolid(thick_solid_thickness_)) {
            emit SelectionChanged();
            emit DocumentChanged();
            emit StatusTextChanged("ThickSolid: выбери Face");
            update();
            return;
        }
        emit StatusTextChanged("ThickSolid: выбери CSolid");
        update();
        return;
    }

    if (document_->SelectLiveThickSolidFaceAtScreen(screen_point, project_world)) {
        emit SelectionChanged();
        emit DocumentChanged();
        emit StatusTextChanged(QString("ThickSolid: Faces %1, Thick %2")
            .arg(static_cast<int>(document_->GetLiveThickSolidFaceCount()))
            .arg(thick_solid_thickness_, 0, 'f', 2));
        update();
        return;
    }

    emit StatusTextChanged("ThickSolid: выбери Face");
    update();
}

void OpenGLViewport::HandleDraftFaceDrag(const QPoint& point) {
    if (!document_ || !document_->IsLiveDraftFaceActive()) {
        dragging_draft_face_ = false;
        return;
    }

    DomPoint center_screen{};
    DomPoint axis_screen{};
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    if (!renderer_.WorldToScreen(draft_face_axis_center_, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(draft_face_axis_center_ + draft_face_axis_dir_ * gizmo_size, camera_, orthographic_projection_, width(), height(), axis_screen)) {
        return;
    }

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());
    const float axis_dx = static_cast<float>(axis_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(axis_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    if (axis_len_sq <= 0.0001f) {
        return;
    }

    const float axis_len = std::sqrt(axis_len_sq);
    const float pixels_around_axis = (mouse_dx * -axis_dy + mouse_dy * axis_dx) / axis_len;
    const double delta_degrees = static_cast<double>(pixels_around_axis) * 0.08;
    const double next_angle = std::clamp(draft_face_angle_degrees_ + delta_degrees, -89.0, 89.0);
    if (std::fabs(delta_degrees) <= 0.0001) {
        return;
    }

    if (document_->UpdateLiveDraftFace(next_angle)) {
        draft_face_angle_degrees_ = next_angle;
        last_mouse_ = point;
        emit DocumentChanged();
        emit StatusTextChanged(QString("Draft Face: angle %1").arg(draft_face_angle_degrees_, 0, 'f', 2));
        update();
    } else {
        last_mouse_ = point;
        emit StatusTextChanged("Draft Face: angle failed");
    }
}

void OpenGLViewport::CommitDraftFaceDrag() {
    if (!document_) {
        return;
    }

    document_->FinishLiveDraftFace();
    draft_face_angle_degrees_ = 0.0;
    emit SelectionChanged();
    emit DocumentChanged();
    emit StatusTextChanged("Draft Face: done");
}

bool OpenGLViewport::HitTestDraftFaceGizmo(const QPoint& point) const {
    if (!document_) {
        return false;
    }

    Vec3 center{};
    Vec3 axis{};
    if (!document_->GetDraftFaceAxis(center, axis)) {
        return false;
    }

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);
    Vec3 tangent{};
    Vec3 bitangent{};
    rotation_arc_basis(axis, forward, tangent, bitangent);

    constexpr int kSegments = 96;
    const float radius = std::max(0.8f, camera_.distance * 0.10f) * 1.26f;
    const DomPoint mouse_point{point.x(), point.y()};
    DomPoint previous{};
    bool has_previous = false;
    float best_distance = 18.0f;
    for (int i = 0; i <= kSegments; ++i) {
        const float angle = static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(kSegments);
        const Vec3 world_point = center + tangent * (std::cos(angle) * radius) + bitangent * (std::sin(angle) * radius);
        DomPoint screen_point{};
        if (!renderer_.WorldToScreen(world_point, camera_, orthographic_projection_, width(), height(), screen_point)) {
            has_previous = false;
            continue;
        }
        if (has_previous) {
            best_distance = std::min(best_distance, DistanceToScreenSegment(mouse_point, previous, screen_point));
        }
        previous = screen_point;
        has_previous = true;
    }
    return best_distance < 18.0f;
}

void OpenGLViewport::HandleTransformClick(const QPoint& point, bool add_to_selection) {
    if (document_->HasSelection()) {
        const TransformAxis axis = HitTestTransformGizmo(point);
        if (axis != TransformAxis::None) {
            active_transform_axis_ = axis;
            highlighted_transform_axis_ = axis;
            dragging_transform_ = true;
            transform_drag_has_preview_ = false;
            transform_drag_move_delta_ = {};
            transform_drag_rotation_angle_ = 0.0f;
            transform_drag_scale_factor_ = 1.0f;
            document_->GetSelectionCenter(transform_drag_center_);
            transform_drag_axis_ = AxisVector(axis);
            last_mouse_ = point;
            update();
            return;
        }
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    const SelectionAction solid_action = add_to_selection ? SelectionAction::Add : SelectionAction::Replace;
    if (document_->SelectSolidMeshAtScreen(screen_point, project_world, solid_action)) {
        document_->ExpandSelectedGroups();
        emit SelectionChanged();
        emit DocumentChanged();
        update();
        return;
    }

    if (document_->SelectMeshAtScreen(screen_point, project_world, solid_action)) {
        document_->ExpandSelectedGroups();
        emit SelectionChanged();
        emit DocumentChanged();
        update();
        return;
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }
    if (add_to_selection) {
        document_->ToggleObjectSelectionAt(scene_point, 0.35f, false);
    } else {
        document_->SelectObjectAt(scene_point, 0.35f, false);
    }
    document_->ExpandSelectedGroups();

    emit SelectionChanged();
    emit DocumentChanged();
    update();
}

void OpenGLViewport::HandleTransformDrag(const QPoint& point) {
    if (!document_ || active_transform_axis_ == TransformAxis::None) {
        return;
    }

    if (!document_->HasSelection()) {
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        return;
    }
    const Vec3 center = transform_drag_center_;

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());

    if (transform_operation_ == TransformOperation::Move && active_transform_axis_ == TransformAxis::ScreenPlane) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);

        const int viewport_height = std::max(1, height());
        float world_per_pixel = 1.0f;
        if (orthographic_projection_) {
            world_per_pixel = (std::max(0.25f, camera_.distance * 0.42f) * 2.0f) / static_cast<float>(viewport_height);
        } else {
            const float depth = std::max(0.001f, dot(center - camera_position(camera_), forward));
            world_per_pixel = (2.0f * depth * std::tan(deg_to_rad(48.0f) * 0.5f)) / static_cast<float>(viewport_height);
        }

        const Vec3 delta = right * (mouse_dx * world_per_pixel) - up * (mouse_dy * world_per_pixel);
        if (std::sqrt(dot(delta, delta)) > 0.000001f && document_->PreviewMoveSelectedObjects(delta)) {
            transform_drag_move_delta_ = transform_drag_move_delta_ + delta;
            transform_drag_has_preview_ = true;
            last_mouse_ = point;
            emit DocumentChanged();
            update();
        }
        return;
    }

    if (transform_operation_ == TransformOperation::Scale && active_transform_axis_ == TransformAxis::UniformScale) {
        const float pixels = mouse_dx - mouse_dy;
        const float factor = std::clamp(1.0f + pixels * 0.01f, 0.05f, 20.0f);
        if (std::fabs(pixels) > 0.0001f && document_->PreviewUniformScaleSelectedObjects(center, factor)) {
            transform_drag_scale_factor_ *= factor;
            transform_drag_has_preview_ = true;
            last_mouse_ = point;
            emit DocumentChanged();
            update();
        }
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

    const float axis_len = std::sqrt(axis_len_sq);
    const float pixels_along_axis = (mouse_dx * axis_dx + mouse_dy * axis_dy) / axis_len;
    const float pixels_per_world = axis_len / gizmo_size;
    const float world_delta = pixels_along_axis / pixels_per_world;
    const float pixels_around_axis = (mouse_dx * -axis_dy + mouse_dy * axis_dx) / axis_len;

    bool transformed = false;
    if (transform_operation_ == TransformOperation::Move) {
        const Vec3 delta = axis * world_delta;
        transformed = document_->PreviewMoveSelectedObjects(delta);
        if (transformed) {
            transform_drag_move_delta_ = transform_drag_move_delta_ + delta;
        }
    } else if (transform_operation_ == TransformOperation::Rotate) {
        const float angle = pixels_around_axis * 0.01f;
        transformed = document_->PreviewRotateSelectedObjects(center, axis, angle);
        if (transformed) {
            transform_drag_rotation_angle_ += angle;
            transform_drag_axis_ = axis;
        }
    } else {
        const float factor = std::clamp(1.0f + pixels_along_axis * 0.01f, 0.05f, 20.0f);
        transformed = document_->PreviewScaleSelectedObjects(center, axis, factor);
        if (transformed) {
            transform_drag_scale_factor_ *= factor;
            transform_drag_axis_ = axis;
        }
    }

    const float active_pixels = transform_operation_ == TransformOperation::Rotate ? pixels_around_axis : pixels_along_axis;
    if (std::fabs(active_pixels) > 0.0001f && transformed) {
        transform_drag_has_preview_ = true;
        last_mouse_ = point;
        emit DocumentChanged();
        update();
    }
}

void OpenGLViewport::CommitTransformDrag() {
    if (!document_ || !transform_drag_has_preview_) {
        return;
    }

    bool committed = false;
    if (transform_operation_ == TransformOperation::Move) {
        committed = document_->CommitMoveSelectedSolids(transform_drag_move_delta_);
    } else if (transform_operation_ == TransformOperation::Rotate) {
        committed = document_->CommitRotateSelectedSolids(transform_drag_center_, transform_drag_axis_, transform_drag_rotation_angle_);
    } else if (active_transform_axis_ == TransformAxis::UniformScale) {
        committed = document_->CommitUniformScaleSelectedSolids(transform_drag_center_, transform_drag_scale_factor_);
    } else {
        committed = document_->CommitScaleSelectedSolids(transform_drag_center_, transform_drag_axis_, transform_drag_scale_factor_);
    }

    if (committed) {
        emit DocumentChanged();
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

    if (transform_operation_ == TransformOperation::Move) {
        const float ring_radius = 0.14f * gizmo_size;
        DomPoint ring_edge{};
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        if (renderer_.WorldToScreen(center + right * ring_radius, camera_, orthographic_projection_, width(), height(), ring_edge)) {
            const float ring_dx = static_cast<float>(ring_edge.x - center_screen.x);
            const float ring_dy = static_cast<float>(ring_edge.y - center_screen.y);
            const float ring_radius_px = std::sqrt(ring_dx * ring_dx + ring_dy * ring_dy);
            const float mouse_dx = static_cast<float>(point.x() - center_screen.x);
            const float mouse_dy = static_cast<float>(point.y() - center_screen.y);
            const float mouse_radius_px = std::sqrt(mouse_dx * mouse_dx + mouse_dy * mouse_dy);
            if (ring_radius_px > 1.0f && std::fabs(mouse_radius_px - ring_radius_px) <= 8.0f) {
                return TransformAxis::ScreenPlane;
            }
        }
    } else if (transform_operation_ == TransformOperation::Scale) {
        const float cube_half_size = 0.075f * gizmo_size;
        DomPoint cube_edge{};
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        if (renderer_.WorldToScreen(center + right * cube_half_size, camera_, orthographic_projection_, width(), height(), cube_edge)) {
            const float cube_dx = static_cast<float>(cube_edge.x - center_screen.x);
            const float cube_dy = static_cast<float>(cube_edge.y - center_screen.y);
            const float cube_radius_px = std::max(8.0f, std::sqrt(cube_dx * cube_dx + cube_dy * cube_dy) + 5.0f);
            const float mouse_dx = static_cast<float>(point.x() - center_screen.x);
            const float mouse_dy = static_cast<float>(point.y() - center_screen.y);
            if (std::sqrt(mouse_dx * mouse_dx + mouse_dy * mouse_dy) <= cube_radius_px) {
                return TransformAxis::UniformScale;
            }
        }
    } else if (transform_operation_ == TransformOperation::Rotate) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);

        TransformAxis best_arc_axis = TransformAxis::None;
        float best_arc_distance = 18.0f;
        constexpr int kSegments = 72;
        const float arc_radius = gizmo_size * 0.46f;
        const TransformAxis axes[] = {TransformAxis::X, TransformAxis::Y, TransformAxis::Z};
        const DomPoint mouse_point{point.x(), point.y()};
        for (TransformAxis axis : axes) {
            const Vec3 direction = AxisVector(axis);
            const Vec3 arc_center = center + direction * gizmo_size;
            Vec3 tangent{};
            Vec3 bitangent{};
            rotation_arc_basis(direction, forward, tangent, bitangent);

            DomPoint previous{};
            bool has_previous = false;
            for (int i = 0; i <= kSegments; ++i) {
                const float angle = static_cast<float>(i) * 3.14159265f / static_cast<float>(kSegments);
                const Vec3 world_point = arc_center + tangent * (std::cos(angle) * arc_radius) + bitangent * (std::sin(angle) * arc_radius);
                DomPoint screen_point{};
                if (!renderer_.WorldToScreen(world_point, camera_, orthographic_projection_, width(), height(), screen_point)) {
                    has_previous = false;
                    continue;
                }

                if (has_previous) {
                    const float distance = DistanceToScreenSegment(mouse_point, previous, screen_point);
                    if (distance < best_arc_distance) {
                        best_arc_distance = distance;
                        best_arc_axis = axis;
                    }
                }
                previous = screen_point;
                has_previous = true;
            }
        }

        return best_arc_axis;
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

bool OpenGLViewport::HitTestSelectedPolylineHandle(const QPoint& point, size_t* point_index) const {
    if (!document_) {
        return false;
    }

    const CPolyline* polyline = document_->GetSelectedPolyline();
    const CBSpline* spline = document_->GetSelectedBSpline();
    const std::vector<CPoint3d>* points = nullptr;
    if (polyline) {
        points = &polyline->GetPoints();
    } else if (spline) {
        points = &spline->GetPoints();
    }
    if (!points) {
        return false;
    }

    const DomPoint mouse_point{point.x(), point.y()};
    bool found = false;
    size_t best_index = 0;
    float best_distance = 10.0f;
    for (size_t i = 0; i < points->size(); ++i) {
        const CPoint3d& curve_point = (*points)[i];
        const Vec3 world{
            static_cast<float>(curve_point.x),
            static_cast<float>(curve_point.y),
            static_cast<float>(curve_point.z)
        };

        DomPoint screen{};
        if (!renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen)) {
            continue;
        }

        const float dx = static_cast<float>(mouse_point.x - screen.x);
        const float dy = static_cast<float>(mouse_point.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            best_index = i;
            found = true;
        }
    }

    if (found && point_index) {
        *point_index = best_index;
    }
    return found;
}

void OpenGLViewport::BeginCurvePointDrag(const CPoint3d& point) {
    dragging_polyline_point_ = true;
    polyline_drag_plane_y_ = point.y;
    curve_point_drag_anchor_ = point_to_vec3(point);
    curve_point_drag_last_ = point;
    curve_point_drag_has_plane_ = CurrentSelectedCurvePlane(curve_point_drag_plane_point_, curve_point_drag_plane_normal_);
}

bool OpenGLViewport::CurrentSelectedCurvePlane(Vec3& plane_point, Vec3& plane_normal) const {
    if (!document_) {
        return false;
    }

    if (const CPolyline* polyline = document_->GetSelectedPolyline()) {
        if (polyline->GetLockedPlane(plane_point, plane_normal)) {
            return true;
        }
        return plane_from_points(polyline->GetPoints(), plane_point, plane_normal);
    }
    if (const CBSpline* spline = document_->GetSelectedBSpline()) {
        return plane_from_points(spline->GetPoints(), plane_point, plane_normal);
    }
    return false;
}

void OpenGLViewport::HandleSketchRectangleClick(const QPoint& point) {
    if (!document_ || !sketch_active_) {
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Rectangle: point is outside sketch plane");
        return;
    }

    if (!sketch_rectangle_has_first_point_) {
        sketch_rectangle_first_point_ = sketch_point;
        sketch_rectangle_preview_point_ = sketch_point;
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_preview_valid_ = true;
        emit StatusTextChanged("Sketch Rectangle: click opposite corner");
        update();
        return;
    }

    const std::vector<CPoint3d> points = SketchRectanglePoints(sketch_rectangle_first_point_, sketch_point);
    document_->CreateSketchRectangle(points, sketch_name_.toStdString());
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    emit DocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(QString("%1: Rectangle created").arg(sketch_name_));
    update();
}

void OpenGLViewport::HandleSketchFilletClick(const QPoint& point) {
    if (!document_ || !sketch_active_) {
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    size_t object_index = 0;
    size_t point_index = 0;
    if (!document_->FindPolylinePointAtScreen(screen_point, world_to_screen, 40.0f, object_index, point_index)) {
        highlighted_sketch_fillet_point_ = false;
        setCursor(Qt::CrossCursor);
        emit StatusTextChanged(QString("%1: Вершина не найдена. Подведите курсор к зеленой точке").arg(sketch_name_));
        update();
        return;
    }

    if (!document_->ApplyFilletToPolylinePointAtScreen(screen_point, world_to_screen, 40.0f, sketch_fillet_radius_)) {
        emit StatusTextChanged(QString("%1: Fillet не выполнен. Уменьшите радиус").arg(sketch_name_));
        update();
        return;
    }

    emit DocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(QString("%1: Fillet R=%2 построен").arg(sketch_name_).arg(sketch_fillet_radius_, 0, 'f', 2));
    update();
}

bool OpenGLViewport::ScreenToSketchPlane(const QPoint& point, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(0.25f, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(deg_to_rad(48.0f) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const float denominator = dot(ray_direction, sketch_normal_);
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float t = dot(sketch_origin_ - ray_origin, sketch_normal_) / denominator;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, hit.y, hit.z);
    return true;
}

std::vector<CPoint3d> OpenGLViewport::SketchRectanglePoints(const CPoint3d& first, const CPoint3d& second) const {
    const Vec3 a{static_cast<float>(first.x), static_cast<float>(first.y), static_cast<float>(first.z)};
    const Vec3 b{static_cast<float>(second.x), static_cast<float>(second.y), static_cast<float>(second.z)};
    const Vec3 delta = b - a;
    const Vec3 u_part = sketch_u_ * dot(delta, sketch_u_);
    const Vec3 v_part = sketch_v_ * dot(delta, sketch_v_);
    const Vec3 p0 = a;
    const Vec3 p1 = a + u_part;
    const Vec3 p2 = a + u_part + v_part;
    const Vec3 p3 = a + v_part;
    return {
        CPoint3d(p0.x, p0.y, p0.z),
        CPoint3d(p1.x, p1.y, p1.z),
        CPoint3d(p2.x, p2.y, p2.z),
        CPoint3d(p3.x, p3.y, p3.z)
    };
}

bool OpenGLViewport::ScreenToWorldPlane(const QPoint& point, Vec3 plane_point, Vec3 plane_normal, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(0.25f, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(deg_to_rad(48.0f) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const Vec3 normal = normalize(plane_normal);
    const float denominator = dot(ray_direction, normal);
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float t = dot(plane_point - ray_origin, normal) / denominator;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, hit.y, hit.z);
    return true;
}

bool OpenGLViewport::ScreenToCurvePlane(const QPoint& point, CPoint3d& result) {
    if (xy_plane_view_enabled_) {
        return ScreenToWorldPlane(point, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, result);
    }

    const std::vector<CPoint3d>& points = tool_ == ToolMode::DrawBSpline
        ? document_->GetActiveBSpline().GetPoints()
        : document_->GetActivePolyline().GetPoints();
    if (points.empty()) {
        CurvePoint floor_point{};
        if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, floor_point)) {
            return false;
        }
        result = CPoint3d(floor_point.x, kCurvePlaneY, floor_point.z);
        return true;
    }

    const CPoint3d& anchor = points.back();
    return ScreenToViewPlane(point,
                             {static_cast<float>(anchor.x), static_cast<float>(anchor.y), static_cast<float>(anchor.z)},
                             result);
}

bool OpenGLViewport::ScreenToPlaneY(const QPoint& point, double y, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(0.25f, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(deg_to_rad(48.0f) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    if (std::fabs(ray_direction.y) <= 0.000001f) {
        return false;
    }

    const float t = (static_cast<float>(y) - ray_origin.y) / ray_direction.y;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, y, hit.z);
    return true;
}

bool OpenGLViewport::ScreenToViewPlane(const QPoint& point, Vec3 plane_point, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(0.25f, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(deg_to_rad(48.0f) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const float denominator = dot(ray_direction, forward);
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float t = dot(plane_point - ray_origin, forward) / denominator;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, hit.y, hit.z);
    return true;
}

void OpenGLViewport::DrawCurveRubberBand() {
    if (!document_) {
        return;
    }

    const std::vector<CPoint3d>& points = tool_ == ToolMode::DrawBSpline
        ? document_->GetActiveBSpline().GetPoints()
        : document_->GetActivePolyline().GetPoints();
    if ((tool_ == ToolMode::DrawCurve && document_->GetActivePolyline().IsClosed()) || points.empty()) {
        return;
    }

    const CPoint3d& last = points.back();
    const Vec3 last_world{static_cast<float>(last.x), static_cast<float>(last.y), static_cast<float>(last.z)};
    const Vec3 preview_world{
        static_cast<float>(curve_preview_point_.x),
        static_cast<float>(curve_preview_point_.y),
        static_cast<float>(curve_preview_point_.z)
    };

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glLineWidth(2.5f);
    glColor4f(1.0f, 0.90f, 0.20f, 0.95f);
    glBegin(GL_LINES);
    glVertex3f(last_world.x, last_world.y, last_world.z);
    glVertex3f(preview_world.x, preview_world.y, preview_world.z);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchRectanglePreview() {
    const std::vector<CPoint3d> points = SketchRectanglePoints(sketch_rectangle_first_point_, sketch_rectangle_preview_point_);
    if (points.size() != 4) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 0.95f, 0.05f, 0.12f);
    glBegin(GL_QUADS);
    for (const CPoint3d& point : points) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    glLineWidth(2.0f);
    glColor4f(1.0f, 0.95f, 0.05f, 0.95f);
    glBegin(GL_LINE_LOOP);
    for (const CPoint3d& point : points) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSelectedCurvePointHandles() {
    const std::vector<CPoint3d> points = document_->GetSelectedCurvePointPositions();
    if (points.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glPointSize(10.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_POINTS);
    for (const CPoint3d& point : points) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();
    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawEditPointSelectionRect() {
    const QRect rect = QRect(edit_point_selection_start_, edit_point_selection_current_).normalized();
    if (rect.width() <= 0 && rect.height() <= 0) {
        return;
    }

    const float left = static_cast<float>(rect.left());
    const float right = static_cast<float>(rect.right());
    const float top = static_cast<float>(rect.top());
    const float bottom = static_cast<float>(rect.bottom());

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width(), height(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 0.95f, 0.05f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    glLineWidth(0.75f);
    glColor4f(1.0f, 0.95f, 0.05f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
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
    const float lift = 0.02f;
    const Vec3 x_label{kGridHalfSize, 0.0f, lift};
    const Vec3 y_label{0.0f, kGridHalfSize, lift};
    const bool has_x = renderer_.WorldToScreen(x_label, camera_, orthographic_projection_, width(), height(), x_screen);
    const bool has_y = renderer_.WorldToScreen(y_label, camera_, orthographic_projection_, width(), height(), y_screen);
    const bool has_z = renderer_.WorldToScreen({0.0f, 0.0f, kGridHalfSize}, camera_, orthographic_projection_, width(), height(), z_screen);

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
void OpenGLViewport::UpdateFPS()
{
    int now = static_cast<int>(GetTickCount64());

    if (m_lastFpsTime == 0)
        m_lastFpsTime = now;

    m_frameCounter++;

    int dt = now - m_lastFpsTime;
    if (dt >= 500) // обновлять 2 раза в секунду
    {
        m_fps = 1000.0f * m_frameCounter / float(dt);
        m_frameCounter = 0;
        m_lastFpsTime = now;
    }
}

void OpenGLViewport::DrawFPS()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont fps_font = painter.font();
    fps_font.setBold(true);
    fps_font.setPointSize(10);
    painter.setFont(fps_font);
    QString fps_text = QString("FPS: %1").arg(m_fps, 0, 'f', 1);
    painter.setPen(Qt::yellow);
	painter.drawText(QPoint(10, 20), fps_text);
}
