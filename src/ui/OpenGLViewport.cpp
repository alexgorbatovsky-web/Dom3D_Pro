#include <windows.h>
#include "OpenGLViewport.h"

#include "../CBSpline.h"
#include "../CPolyline.h"
#include "../SmartLine.h"
#include "../solid/Solid.h"
#include "MaterialDrag.h"
#include "MeasurementUnits.h"

#include <QCursor>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <cmath>
#include <limits>


namespace {
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

double parameter_value(const std::vector<ToolParameter>& parameters,
                       const char* id,
                       double fallback) {
    const auto parameter = std::find_if(
        parameters.begin(),
        parameters.end(),
        [id](const ToolParameter& candidate) {
            return candidate.id == id;
        });
    return parameter == parameters.end() ? fallback : parameter->value;
}

double saved_parameter_value(const std::vector<ParametricParameterValue>& parameters,
                             const char* id,
                             double fallback) {
    const auto parameter = std::find_if(
        parameters.begin(),
        parameters.end(),
        [id](const ParametricParameterValue& candidate) {
            return candidate.id == id;
        });
    return parameter == parameters.end() ? fallback : parameter->value;
}

CPoint3d add_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return {
        first.x + second.x,
        first.y + second.y,
        first.z + second.z
    };
}

CPoint3d subtract_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return {
        first.x - second.x,
        first.y - second.y,
        first.z - second.z
    };
}

QCursor captured_point_cursor() {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 170, 255), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(12.0, 12.0), 6.0, 6.0);
    painter.drawPoint(QPointF(12.0, 12.0));
    return QCursor(pixmap, 12, 12);
}

CPoint3d scale_dimension_point(const CPoint3d& point, double factor) {
    return {point.x * factor, point.y * factor, point.z * factor};
}

double dot_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

CPoint3d cross_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x
    };
}

CPoint3d normalized_dimension_point(const CPoint3d& point) {
    const double length = std::sqrt(dot_dimension_point(point, point));
    return length <= 1.0e-12
        ? CPoint3d{}
        : scale_dimension_point(point, 1.0 / length);
}

CPoint3d transform_dimension_point(const CPoint3d& point,
                                   const ParametricFunction& operation,
                                   bool direction) {
    if (operation.ToolId != "SolidTransform") {
        return point;
    }

    const int type = std::clamp(
        static_cast<int>(saved_parameter_value(operation.Parameters, "type", 0.0)),
        0,
        2);
    if (type == 0) {
        if (direction) {
            return point;
        }
        return {
            point.x + saved_parameter_value(operation.Parameters, "dx", 0.0),
            point.y + saved_parameter_value(operation.Parameters, "dy", 0.0),
            point.z + saved_parameter_value(operation.Parameters, "dz", 0.0)
        };
    }

    const CPoint3d center(
        saved_parameter_value(operation.Parameters, "center.x", 0.0),
        saved_parameter_value(operation.Parameters, "center.y", 0.0),
        saved_parameter_value(operation.Parameters, "center.z", 0.0));
    const CPoint3d axis = normalized_dimension_point(CPoint3d(
        saved_parameter_value(operation.Parameters, "axis.x", 0.0),
        saved_parameter_value(operation.Parameters, "axis.y", 0.0),
        saved_parameter_value(operation.Parameters, "axis.z", 1.0)));

    if (type == 1) {
        if (dot_dimension_point(axis, axis) <= 1.0e-12) {
            return point;
        }
        const double angle = saved_parameter_value(operation.Parameters, "angle", 0.0);
        const CPoint3d relative = direction ? point : subtract_dimension_point(point, center);
        const CPoint3d rotated = add_dimension_point(
            add_dimension_point(
                scale_dimension_point(relative, std::cos(angle)),
                scale_dimension_point(cross_dimension_point(axis, relative), std::sin(angle))),
            scale_dimension_point(
                axis,
                dot_dimension_point(axis, relative) * (1.0 - std::cos(angle))));
        return direction ? rotated : add_dimension_point(center, rotated);
    }

    const double factor = saved_parameter_value(operation.Parameters, "factor", 1.0);
    const CPoint3d relative = direction ? point : subtract_dimension_point(point, center);
    CPoint3d scaled;
    if (dot_dimension_point(axis, axis) <= 1.0e-12) {
        scaled = scale_dimension_point(relative, factor);
    } else {
        scaled = add_dimension_point(
            relative,
            scale_dimension_point(
                axis,
                (factor - 1.0) * dot_dimension_point(axis, relative)));
    }
    return direction ? scaled : add_dimension_point(center, scaled);
}

Quaternion orientation_from_forward_right(Vec3 forward, Vec3 desired_right) {
    forward = normalize(forward);
    const Vec3 back = forward * -1.0f;
    Vec3 right = normalize(desired_right - back * dot(desired_right, back));
    if (dot(right, right) <= 0.00001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    Vec3 up = normalize(cross(back, right));
    if (dot(up, up) <= 0.00001f) {
        up = {0.0f, 1.0f, 0.0f};
    }
    right = normalize(cross(up, back));
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
    const Vec3 current_right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
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

    const Vec3 base_axes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
    Vec3 horizontal_axis{};
    float best_horizontal = -1.0f;
    for (const Vec3 axis : base_axes) {
        if (std::fabs(dot(axis, best_axis)) > 0.5f) {
            continue;
        }
        const float alignment = std::fabs(dot(axis, current_right));
        if (alignment > best_horizontal) {
            best_horizontal = alignment;
            horizontal_axis = dot(axis, current_right) < 0.0f ? axis * -1.0f : axis;
        }
    }

    if (dot(horizontal_axis, horizontal_axis) <= 0.00001f) {
        const Vec3 desired_up = std::fabs(best_axis.z) > 0.5f
            ? Vec3{0.0f, 1.0f, 0.0f}
            : Vec3{0.0f, 0.0f, 1.0f};
        camera.orientation = orientation_from_forward_up(best_axis, desired_up);
    } else {
        camera.orientation = orientation_from_forward_right(best_axis, horizontal_axis);
    }
}
}

OpenGLViewport::OpenGLViewport(QWidget* parent)
    : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumSize(640, 420);
    ReloadModelingPreferences();
}

void OpenGLViewport::SetDocument(CAlfaDoc* document) {
    document_ = document;
    update();
}

void OpenGLViewport::SetTool(ToolMode tool) {
    const bool changed = tool_ != tool;
    const bool cancel_sketch_face_selection =
        sketch_waiting_for_face_ && tool != ToolMode::SketchRectangle;
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
    dragging_sketch_handle_ = false;
    selecting_with_rect_ = false;
    active_sketch_handle_kind_ = SketchHandleKind::None;
    curve_point_drag_has_plane_ = false;
    curve_preview_valid_ = false;
    creation_snap_active_ = false;
    if (tool_ != ToolMode::Select) {
        editing_polyline_ = false;
        editing_sketch_ = false;
        highlighted_polyline_handle_ = false;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
    }
    if (tool_ != ToolMode::SketchRectangle
        && tool_ != ToolMode::SolidBoxRectangle
        && tool_ != ToolMode::SolidCylinderCircle) {
        sketch_rectangle_has_first_point_ = false;
        sketch_rectangle_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchRectangle) {
        sketch_waiting_for_face_ = false;
    }
    if (tool_ != ToolMode::SolidBoxRectangle
        && tool_ != ToolMode::SolidCylinderCircle) {
        solid_box_waiting_for_face_ = false;
        solid_box_target_body_id_ = 0;
    }
    if (tool_ != ToolMode::SketchPolyline) {
        sketch_polyline_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchBezier) {
        sketch_bezier_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchConvertArc) {
        sketch_arc_has_line_ = false;
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
        } else if (tool_ == ToolMode::SketchRectangle
                   || tool_ == ToolMode::SketchPolyline
                   || tool_ == ToolMode::SketchBezier
                   || tool_ == ToolMode::SketchConvertBezier
                   || tool_ == ToolMode::SketchConvertArc
                   || tool_ == ToolMode::SolidBoxRectangle
                   || tool_ == ToolMode::SolidCylinderCircle) {
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
    if (cancel_sketch_face_selection) {
        emit SketchFaceSelectionFinished(false);
    }
    update();
}

void OpenGLViewport::SetTransformOperation(TransformOperation operation) {
    transform_operation_ = operation;
    SetTool(ToolMode::Transform);
}

void OpenGLViewport::SetSelectionConfirmationMode(bool enabled) {
    selection_confirmation_mode_ = enabled;
    if (enabled) {
        setFocus();
    }
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
        if (!object || !document_->IsObjectVisible(*object)) {
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
    camera_.distance = std::clamp(radius * 1.5f, 2.0f, 100000.0f);
    update();
}

void OpenGLViewport::ReloadModelingPreferences() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    snapping_enabled_ =
        settings.value("preferences/modeling/snappingEnabled", true).toBool();
    capture_distance_pixels_ = std::clamp(
        settings.value("preferences/modeling/captureDistance", 6).toInt(),
        1,
        50);
    if (!snapping_enabled_) {
        SetCreationSnapCursor(false);
    }
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

void OpenGLViewport::SetXYView() {
    camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, 0.0f);
    camera_.target = {kDefaultSceneSize * 0.5f, kDefaultSceneSize * 0.5f, 0.0f};
    camera_.distance = kDefaultPlanCameraDistance;
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
        camera_.target = {kDefaultSceneSize * 0.5f, kDefaultSceneSize * 0.5f, 0.0f};
        camera_.distance = kDefaultPlanCameraDistance;
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
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    orthographic_projection_ = true;
    xy_plane_view_enabled_ = false;
    sketch_attachment_body_id_ = 0;
    sketch_attachment_face_index_ = -1;

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

void OpenGLViewport::BeginSketchFaceSelection(const QString& name) {
    sketch_active_ = false;
    sketch_waiting_for_face_ = true;
    sketch_name_ = name;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_attachment_body_id_ = 0;
    sketch_attachment_face_index_ = -1;
    SetTool(ToolMode::SketchRectangle);
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(
        QString("%1: select a planar body face").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchOnFace(const QString& name,
                                       Vec3 origin,
                                       Vec3 x_axis,
                                       Vec3 y_axis,
                                       Vec3 normal,
                                       unsigned long body_id,
                                       int face_index) {
    sketch_active_ = true;
    sketch_name_ = name;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    orthographic_projection_ = true;
    xy_plane_view_enabled_ = false;

    sketch_origin_ = origin;
    sketch_u_ = normalize(x_axis);
    sketch_v_ = normalize(y_axis);
    sketch_normal_ = normalize(normal);
    sketch_attachment_body_id_ = body_id;
    sketch_attachment_face_index_ = face_index;
    camera_.target = sketch_origin_;
    camera_.distance = kDefaultPlanCameraDistance;
    camera_.orientation = orientation_from_forward_up(
        sketch_normal_ * -1.0f, sketch_v_);

    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(
        QString("%1: sketch on body face, click first rectangle corner").arg(sketch_name_));
}

void OpenGLViewport::SetSketchRectangleTool() {
    if (!sketch_active_) {
        return;
    }
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(QString("%1: Rectangle, click first corner").arg(sketch_name_));
}

bool OpenGLViewport::BeginEditSelectedSketch() {
    const CSmartLine* sketch = document_ ? document_->GetSelectedSketch() : nullptr;
    if (!sketch) {
        return false;
    }
    const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
    sketch_active_ = true;
    sketch_name_ = QString::fromStdString(sketch->GetName());
    sketch_origin_ = point_to_vec3(system.origin);
    sketch_u_ = point_to_vec3(system.x_axis);
    sketch_v_ = point_to_vec3(system.y_axis);
    sketch_normal_ = point_to_vec3(system.normal);
    if (sketch->HasFaceAttachment()) {
        const SketchFaceAttachment& attachment = sketch->GetFaceAttachment();
        sketch_attachment_body_id_ = attachment.body_id;
        sketch_attachment_face_index_ = attachment.face_index;
    } else {
        sketch_attachment_body_id_ = 0;
        sketch_attachment_face_index_ = -1;
    }
    sketch_rectangle_has_first_point_ = false;
    sketch_polyline_points_.clear();
    sketch_bezier_points_.clear();
    SetTool(ToolMode::Select);
    editing_sketch_ = true;
    emit StatusTextChanged(
        QString("%1: edit sketch or choose a creation tool").arg(sketch_name_));
    return true;
}

void OpenGLViewport::SetSketchPolylineTool() {
    if (!sketch_active_) {
        return;
    }
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    QSettings settings("Dom3D", "Dom3D_Pro");
    sketch_alignment_angle_degrees_ = std::clamp(
        settings.value("preferences/modeling/angleAlignment", 7).toDouble(),
        0.0,
        45.0);
    SetTool(ToolMode::SketchPolyline);
    emit StatusTextChanged(
        QString("%1: Polyline, click first point (alignment %2°)")
            .arg(sketch_name_)
            .arg(sketch_alignment_angle_degrees_, 0, 'f', 0));
}

void OpenGLViewport::SetSketchBezierTool() {
    if (!sketch_active_) {
        return;
    }
    sketch_rectangle_has_first_point_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    SetTool(ToolMode::SketchBezier);
    emit StatusTextChanged(
        QString("%1: Bezier — click start, two controls and end").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchConvertLineToBezier() {
    if (!document_ || !document_->GetSelectedSketch()) {
        emit StatusTextChanged("Convert to Bezier: select a sketch first");
        return;
    }
    SetTool(ToolMode::SketchConvertBezier);
    emit StatusTextChanged(
        QString("%1: click a straight segment to convert it to Bezier")
            .arg(sketch_name_));
}

void OpenGLViewport::BeginSketchConvertLineToArc() {
    if (!document_ || !document_->GetSelectedSketch()) {
        emit StatusTextChanged("Line to Arc: select a sketch first");
        return;
    }
    sketch_arc_has_line_ = false;
    SetTool(ToolMode::SketchConvertArc);
    emit StatusTextChanged(
        QString("%1: select a straight segment").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchFillet(double radius) {
    if (!document_ || (!sketch_active_ && !document_->GetSelectedSketch())) {
        return;
    }
    if (!sketch_active_) {
        sketch_name_ = QString::fromStdString(document_->GetSelectedSketch()->GetName());
    }
    sketch_fillet_radius_ = radius;
    SetTool(ToolMode::SketchFillet);
    emit StatusTextChanged(QString("%1: Fillet R=%2. Укажите вершину полилинии").arg(sketch_name_).arg(sketch_fillet_radius_, 0, 'f', 2));
}

void OpenGLViewport::BeginSolidBoxRectangle(SketchPlane plane) {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = false;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "BOX";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;

    sketch_origin_ = {};
    if (plane == SketchPlane::XY) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 1.0f, 0.0f};
        sketch_normal_ = {0.0f, 0.0f, 1.0f};
    } else if (plane == SketchPlane::XZ) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {0.0f, 1.0f, 0.0f};
    } else {
        sketch_u_ = {0.0f, 1.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {1.0f, 0.0f, 0.0f};
    }

    SetTool(ToolMode::SolidBoxRectangle);
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("BOX: click first rectangle corner");
}

void OpenGLViewport::BeginSolidBoxFaceSelection() {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = true;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "BOX";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    SetTool(ToolMode::SolidBoxRectangle);
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("BOX: select a planar body face");
}

void OpenGLViewport::BeginSolidCylinderCircle(SketchPlane plane) {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = false;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "CYLINDER";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;

    sketch_origin_ = {};
    if (plane == SketchPlane::XY) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 1.0f, 0.0f};
        sketch_normal_ = {0.0f, 0.0f, 1.0f};
    } else if (plane == SketchPlane::XZ) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {0.0f, 1.0f, 0.0f};
    } else {
        sketch_u_ = {0.0f, 1.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {1.0f, 0.0f, 0.0f};
    }

    SetTool(ToolMode::SolidCylinderCircle);
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("CYLINDER: click circle center");
}

void OpenGLViewport::BeginSolidCylinderFaceSelection() {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = true;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "CYLINDER";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    SetTool(ToolMode::SolidCylinderCircle);
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("CYLINDER: select a planar body face");
}

void OpenGLViewport::SetSolidDimensionEdit(
    const ActiveParametricObject& active_object,
    const QString& primary_parameter) {
    const bool supports_dimensions =
        active_object.tool_id == "SolidBox"
        || active_object.tool_id == "SolidCylinder"
        || active_object.tool_id == "SolidPrismTool";
    solid_dimension_object_ = supports_dimensions
        ? active_object
        : ActiveParametricObject{};
    solid_dimensions_.clear();
    solid_dimension_hits_.clear();
    solid_dimension_primary_parameter_ = primary_parameter;

    std::vector<const ParametricFunction*> following_transforms;
    if (document_ && solid_dimension_object_.object_index < document_->GetObjects().size()) {
        const auto* solid = dynamic_cast<const CSolid*>(
            document_->GetObjects()[solid_dimension_object_.object_index].get());
        if (solid) {
            for (size_t operation_index = solid_dimension_object_.operation_index + 1;
                 operation_index < static_cast<size_t>(solid->GetNumOperations());
                 ++operation_index) {
                const ParametricFunction* operation =
                    solid->GetOperation(static_cast<int>(operation_index));
                if (operation && operation->ToolId == "SolidTransform") {
                    following_transforms.push_back(operation);
                }
            }
        }
    }

    const auto transformed_point = [&following_transforms](CPoint3d point) {
        for (const ParametricFunction* operation : following_transforms) {
            point = transform_dimension_point(point, *operation, false);
        }
        return point;
    };
    const auto transformed_direction = [&following_transforms](CPoint3d direction) {
        for (const ParametricFunction* operation : following_transforms) {
            direction = transform_dimension_point(direction, *operation, true);
        }
        return normalized_dimension_point(direction);
    };
    const auto add_dimension = [this, &transformed_point, &transformed_direction](
                                   CPoint3d start,
                                   CPoint3d end,
                                   CPoint3d offset_direction,
                                   double offset,
                                   const char* parameter_id,
                                   const char* label,
                                   double value) {
        solid_dimensions_.emplace_back(
            transformed_point(start),
            transformed_point(end),
            transformed_direction(offset_direction),
            offset,
            parameter_id,
            label,
            value);
    };

    if (solid_dimension_object_.tool_id == "SolidBox") {
        const auto& parameters = solid_dimension_object_.parameters;
        const CPoint3d origin(
            parameter_value(parameters, "origin.x", 0.0),
            parameter_value(parameters, "origin.y", 0.0),
            parameter_value(parameters, "origin.z", 0.0));
        const CPoint3d u(
            parameter_value(parameters, "axis.u.x", 1.0),
            parameter_value(parameters, "axis.u.y", 0.0),
            parameter_value(parameters, "axis.u.z", 0.0));
        const CPoint3d v(
            parameter_value(parameters, "axis.v.x", 0.0),
            parameter_value(parameters, "axis.v.y", 1.0),
            parameter_value(parameters, "axis.v.z", 0.0));
        const CPoint3d n(
            parameter_value(parameters, "axis.n.x", 0.0),
            parameter_value(parameters, "axis.n.y", 0.0),
            parameter_value(parameters, "axis.n.z", 1.0));
        const double width_value = parameter_value(parameters, "width", 1.0);
        const double height_value = parameter_value(parameters, "height", 1.0);
        const double depth_value = parameter_value(parameters, "depth", 1.0);
        const double offset = std::max(
            2.0,
            std::min({std::abs(width_value), std::abs(height_value), std::abs(depth_value)}) * 0.14);
        const auto endpoint = [&origin](const CPoint3d& axis, double value) {
            return CPoint3d(
                origin.x + axis.x * value,
                origin.y + axis.y * value,
                origin.z + axis.z * value);
        };
        add_dimension(
            origin,
            endpoint(u, width_value),
            CPoint3d(-v.x, -v.y, -v.z),
            offset,
            "width",
            "Length",
            width_value);
        add_dimension(
            origin,
            endpoint(v, height_value),
            CPoint3d(-u.x, -u.y, -u.z),
            offset,
            "height",
            "Width",
            height_value);
        add_dimension(
            origin,
            endpoint(n, depth_value),
            u,
            offset,
            "depth",
            "Height",
            depth_value);
    } else if (solid_dimension_object_.tool_id == "SolidCylinder") {
        const auto& parameters = solid_dimension_object_.parameters;
        const CPoint3d origin(
            parameter_value(parameters, "origin.x", 0.0),
            parameter_value(parameters, "origin.y", 0.0),
            parameter_value(parameters, "origin.z", 0.0));
        const CPoint3d u(
            parameter_value(parameters, "axis.u.x", 1.0),
            parameter_value(parameters, "axis.u.y", 0.0),
            parameter_value(parameters, "axis.u.z", 0.0));
        const CPoint3d v(
            parameter_value(parameters, "axis.v.x", 0.0),
            parameter_value(parameters, "axis.v.y", 1.0),
            parameter_value(parameters, "axis.v.z", 0.0));
        const CPoint3d n(
            parameter_value(parameters, "axis.n.x", 0.0),
            parameter_value(parameters, "axis.n.y", 0.0),
            parameter_value(parameters, "axis.n.z", 1.0));
        const double diameter = parameter_value(parameters, "diameter", 10.0);
        const double height = parameter_value(parameters, "height", 5.0);
        const double radius = diameter * 0.5;
        const double offset = std::max(2.0, std::min(std::abs(diameter), std::abs(height)) * 0.14);
        const auto shifted = [&origin](const CPoint3d& axis, double value) {
            return CPoint3d(
                origin.x + axis.x * value,
                origin.y + axis.y * value,
                origin.z + axis.z * value);
        };
        add_dimension(
            shifted(u, -radius),
            shifted(u, radius),
            CPoint3d(-v.x, -v.y, -v.z),
            offset,
            "diameter",
            "Diameter",
            diameter);
        add_dimension(
            origin,
            shifted(n, height),
            u,
            offset,
            "height",
            "Height",
            height);
    } else if (solid_dimension_object_.tool_id == "SolidPrismTool") {
        const auto& parameters = solid_dimension_object_.parameters;
        const double length_value = parameter_value(parameters, "length", 20.0);
        const double height_value = parameter_value(parameters, "height", 40.0);
        const int quantity = std::clamp(
            static_cast<int>(parameter_value(parameters, "qty", 6.0)),
            3,
            128);
        const int axis = std::clamp(
            static_cast<int>(parameter_value(parameters, "axis", 1.0)),
            0,
            2);
        const double half_angle = 3.14159265358979323846 / quantity;
        const double rotation_angle = half_angle * 2.0;
        const CPoint3d first_2d(
            -length_value * 0.5,
            -length_value / std::tan(half_angle) * 0.5,
            0.0);
        const CPoint3d second_2d(
            first_2d.x * std::cos(rotation_angle) - first_2d.y * std::sin(rotation_angle),
            first_2d.x * std::sin(rotation_angle) + first_2d.y * std::cos(rotation_angle),
            0.0);
        const auto map_base_point = [axis](const CPoint3d& point) {
            if (axis == 0) {
                return CPoint3d(0.0, point.y, -point.x);
            }
            if (axis == 1) {
                return CPoint3d(point.x, 0.0, point.y);
            }
            return CPoint3d(point.x, point.y, 0.0);
        };
        const CPoint3d height_direction = axis == 0
            ? CPoint3d(1.0, 0.0, 0.0)
            : (axis == 1 ? CPoint3d(0.0, 1.0, 0.0) : CPoint3d(0.0, 0.0, 1.0));
        const CPoint3d first = map_base_point(first_2d);
        const CPoint3d second = map_base_point(second_2d);
        const CPoint3d side_midpoint = scale_dimension_point(
            add_dimension_point(first_2d, second_2d),
            0.5);
        const CPoint3d side_offset = map_base_point(
            normalized_dimension_point(side_midpoint));
        const CPoint3d height_offset = map_base_point(
            normalized_dimension_point(first_2d));
        const double offset = std::max(
            2.0,
            std::min(std::abs(length_value), std::abs(height_value)) * 0.14);

        add_dimension(
            first,
            second,
            side_offset,
            offset,
            "length",
            "Length",
            length_value);
        add_dimension(
            first,
            add_dimension_point(first, scale_dimension_point(height_direction, height_value)),
            height_offset,
            offset,
            "height",
            "Height",
            height_value);
    }
    update();
}

void OpenGLViewport::ClearSolidDimensionEdit() {
    solid_dimension_object_ = {};
    solid_dimensions_.clear();
    solid_dimension_hits_.clear();
    solid_dimension_primary_parameter_.clear();
    update();
}

void OpenGLViewport::BeginPickXYPoint() {
    picking_xy_point_ = true;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("Pick Pc: click point on XY plane");
}

void OpenGLViewport::BeginMovePointToPoint() {
    SetTool(ToolMode::MovePointToPoint);
    move_point_stage_ = document_ && document_->HasSelection()
        ? MovePointStage::PickSource
        : MovePointStage::SelectObjects;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(move_point_stage_ == MovePointStage::PickSource
        ? "Move Point to Point: pick source point"
        : "Move Point to Point: select object(s), then press Enter");
}

void OpenGLViewport::EndSketch() {
    if (tool_ == ToolMode::SketchPolyline && sketch_polyline_points_.size() >= 2) {
        CommitSketchPolyline(false);
    }
    sketch_active_ = false;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    sketch_attachment_body_id_ = 0;
    sketch_attachment_face_index_ = -1;
    if (tool_ == ToolMode::SketchRectangle
        || tool_ == ToolMode::SketchPolyline
        || tool_ == ToolMode::SketchBezier
        || tool_ == ToolMode::SketchConvertBezier
        || tool_ == ToolMode::SketchConvertArc
        || tool_ == ToolMode::SketchFillet
        || tool_ == ToolMode::SolidBoxRectangle
        || tool_ == ToolMode::SolidCylinderCircle) {
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
    if ((tool_ == ToolMode::SketchRectangle || tool_ == ToolMode::SolidBoxRectangle)
        && sketch_rectangle_has_first_point_
        && sketch_rectangle_preview_valid_) {
        DrawSketchRectanglePreview();
    }
    if (tool_ == ToolMode::SolidCylinderCircle
        && sketch_rectangle_has_first_point_
        && sketch_rectangle_preview_valid_) {
        DrawSolidCylinderCirclePreview();
    }
    if (tool_ == ToolMode::SketchPolyline && !sketch_polyline_points_.empty()) {
        DrawSketchPolylinePreview();
    }
    if (tool_ == ToolMode::SketchBezier && !sketch_bezier_points_.empty()) {
        DrawSketchBezierPreview();
    }
    if (tool_ == ToolMode::EditPoint && document_) {
        DrawSelectedCurvePointHandles();
    }
    if (tool_ == ToolMode::Select && editing_sketch_ && document_) {
        DrawSketchEditHandles();
    }
    if (tool_ == ToolMode::EditPoint && selecting_edit_points_) {
        DrawEditPointSelectionRect();
    }
    if (tool_ == ToolMode::Select && selecting_with_rect_) {
        DrawSelectRubberBandRect();
    }
    if (!solid_dimension_object_.tool_id.empty()) {
        DrawSolidDimensions();
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

    if (tool_ == ToolMode::Select && !solid_dimension_object_.tool_id.empty()) {
        const auto hit = std::find_if(
            solid_dimension_hits_.begin(),
            solid_dimension_hits_.end(),
            [event](const SolidDimensionHit& dimension) {
                return dimension.rect.adjusted(-4, -4, 4, 4).contains(event->pos());
            });
        if (hit != solid_dimension_hits_.end()) {
            emit SolidDimensionEditRequested(hit->parameter_id, hit->value);
            event->accept();
            return;
        }
    }

    if (picking_xy_point_) {
        CPoint3d point{};
        if (ScreenToWorldPlane(event->pos(), {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, point)) {
            picking_xy_point_ = false;
            unsetCursor();
            emit XYPointPicked(point);
            emit StatusTextChanged(QString("Pc picked: X %1, Y %2, Z %3")
                .arg(point.x, 0, 'f', 6)
                .arg(point.y, 0, 'f', 6)
                .arg(point.z, 0, 'f', 6));
            event->accept();
            return;
        }
        emit StatusTextChanged("Pick Pc: point is outside XY plane view");
        event->accept();
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

    if (tool_ == ToolMode::SketchPolyline) {
        HandleSketchPolylineClick(event->pos());
        return;
    }
    if (tool_ == ToolMode::SketchBezier) {
        HandleSketchBezierClick(event->pos());
        return;
    }
    if (tool_ == ToolMode::SketchConvertBezier) {
        HandleSketchConvertLineToBezierClick(event->pos());
        return;
    }
    if (tool_ == ToolMode::SketchConvertArc) {
        HandleSketchConvertLineToArcClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SolidBoxRectangle) {
        HandleSolidBoxRectangleClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SolidCylinderCircle) {
        HandleSolidCylinderCircleClick(event->pos());
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

    if (tool_ == ToolMode::MovePointToPoint) {
        HandleMovePointToPointClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::Select) {
        if (editing_sketch_) {
            SketchHandleKind kind = SketchHandleKind::None;
            size_t index = 0;
            if (HitTestSelectedSketchHandle(event->pos(), kind, index)) {
                active_sketch_handle_kind_ = kind;
                active_sketch_handle_index_ = index;
                dragging_sketch_handle_ = true;
                last_mouse_ = event->pos();
                setCursor(Qt::ClosedHandCursor);
                update();
            }
            return;
        }
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
        selecting_with_rect_ = true;
        rect_selection_action_ = action;
        rect_selection_start_ = event->pos();
        rect_selection_current_ = event->pos();
        ++edge_quick_menu_generation_;
        update();
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
        editing_sketch_ = document_->GetSelectedSketch() != nullptr;
        editing_polyline_ = !editing_sketch_;
        highlighted_polyline_handle_ = false;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
        emit SelectionChanged();
        emit StatusTextChanged(editing_sketch_
            ? "Sketch edit: drag green points or red fillet handles, Esc to finish"
            : "Curve edit: drag handles, Esc to finish");
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

    if (dragging_sketch_handle_ && tool_ == ToolMode::Select && editing_sketch_ && document_) {
        CSmartLine* sketch = document_->GetSelectedSketch();
        if (sketch) {
            const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
            CPoint3d point{};
            if (ScreenToWorldPlane(
                    event->pos(),
                    point_to_vec3(system.origin),
                    point_to_vec3(system.normal),
                    point)) {
                float snap_distance = static_cast<float>(capture_distance_pixels_);
                const bool snapped = SnapSketchGridPoint(
                    event->pos(),
                    point_to_vec3(system.origin),
                    point_to_vec3(system.x_axis),
                    point_to_vec3(system.y_axis),
                    point,
                    snap_distance);
                SetCreationSnapCursor(snapped);
                bool changed = false;
                if (active_sketch_handle_kind_ == SketchHandleKind::Node) {
                    changed = sketch->MoveNodeWorld(active_sketch_handle_index_, point);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::Fillet) {
                    changed = sketch->SetFilletRadiusFromWorld(active_sketch_handle_index_, point);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::BezierControl) {
                    changed = sketch->MoveBezierControlPointWorld(active_sketch_handle_index_, point);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::ArcControl) {
                    changed = sketch->MoveArcGripWorld(active_sketch_handle_index_, point);
                }
                if (changed) {
                    emit DocumentChanged();
                    update();
                }
            }
        }
        last_mouse_ = event->pos();
        return;
    }

    if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline)
        && document_) {
        CPoint3d snap_point{};
        const bool valid = ScreenToCurvePlane(event->pos(), snap_point);
        SetCreationSnapCursor(
            valid && SnapCreationPoint(event->pos(), snap_point, false));
    } else if ((tool_ == ToolMode::SketchRectangle
               || tool_ == ToolMode::SketchPolyline
               || tool_ == ToolMode::SketchBezier
               || (tool_ == ToolMode::SolidBoxRectangle
                   && !solid_box_waiting_for_face_)
               || (tool_ == ToolMode::SolidCylinderCircle
                   && !solid_box_waiting_for_face_))
               && document_) {
        CPoint3d snap_point{};
        const bool valid = ScreenToSketchPlane(event->pos(), snap_point);
        bool snapped = valid
            && SnapCreationPoint(event->pos(), snap_point, true);
        if (tool_ == ToolMode::SketchPolyline
            && IsNearSketchPolylineFirstPoint(event->pos())) {
            snapped = true;
        } else if (tool_ == ToolMode::SketchBezier
                   && sketch_bezier_points_.size() == 3
                   && IsNearSelectedSketchFirstPoint(event->pos())) {
            snapped = true;
        }
        SetCreationSnapCursor(snapped);
    }

    if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline) && document_) {
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToCurvePlane(event->pos(), preview_point);
        if (preview_valid) {
            SnapCreationPoint(event->pos(), preview_point, false);
        }
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

    if ((tool_ == ToolMode::SketchRectangle
         || tool_ == ToolMode::SolidBoxRectangle
         || tool_ == ToolMode::SolidCylinderCircle)
        && sketch_rectangle_has_first_point_
        && document_) {
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid) {
            SnapCreationPoint(event->pos(), preview_point, true);
        }
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

    if (tool_ == ToolMode::SketchPolyline && !sketch_polyline_points_.empty() && document_) {
        CPoint3d preview_point{};
        bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid) {
            if (IsNearSketchPolylineFirstPoint(event->pos())) {
                preview_point = sketch_polyline_points_.front();
            } else if (!SnapCreationPoint(event->pos(), preview_point, true)) {
                preview_point = AlignSketchPolylinePoint(preview_point);
            }
        }
        if (preview_valid != sketch_polyline_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - sketch_polyline_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - sketch_polyline_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - sketch_polyline_preview_point_.z) > 0.0001))) {
            sketch_polyline_preview_valid_ = preview_valid;
            sketch_polyline_preview_point_ = preview_point;
            update();
        }
    }
    if (tool_ == ToolMode::SketchBezier && !sketch_bezier_points_.empty() && document_) {
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid
            && sketch_bezier_points_.size() == 3
            && IsNearSelectedSketchFirstPoint(event->pos())) {
            preview_point = document_->GetSelectedSketch()->GetNodeWorld(0);
        } else if (preview_valid) {
            SnapCreationPoint(event->pos(), preview_point, true);
        }
        if (preview_valid != sketch_bezier_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - sketch_bezier_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - sketch_bezier_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - sketch_bezier_preview_point_.z) > 0.0001))) {
            sketch_bezier_preview_valid_ = preview_valid;
            sketch_bezier_preview_point_ = preview_point;
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

    if (selecting_with_rect_ && tool_ == ToolMode::Select) {
        rect_selection_current_ = event->pos();
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

    if (tool_ == ToolMode::Select && editing_sketch_ && document_) {
        SketchHandleKind kind = SketchHandleKind::None;
        size_t index = 0;
        HitTestSelectedSketchHandle(event->pos(), kind, index);
        if (kind != highlighted_sketch_handle_kind_ || index != highlighted_sketch_handle_index_) {
            highlighted_sketch_handle_kind_ = kind;
            highlighted_sketch_handle_index_ = index;
            if (kind != SketchHandleKind::None) {
                setCursor(Qt::OpenHandCursor);
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
    bool select_click_completed = false;
    if (event->button() == Qt::LeftButton
        && selecting_with_rect_
        && tool_ == ToolMode::Select
        && document_) {
        rect_selection_current_ = event->pos();
        const QPoint drag = rect_selection_current_ - rect_selection_start_;
        const int distance_squared = drag.x() * drag.x() + drag.y() * drag.y();
        if (distance_squared < 5 * 5) {
            SelectAt(event->pos(), rect_selection_action_);
            select_click_completed = true;
        } else {
            const DomRect rect{
                rect_selection_start_.x(),
                rect_selection_start_.y(),
                rect_selection_current_.x(),
                rect_selection_current_.y()
            };
            auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
                return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
            };
            if (selection_mode_ == SelectionMode::Object) {
                document_->SelectObjectsInScreenRect(rect, world_to_screen, rect_selection_action_);
            } else if (selection_mode_ == SelectionMode::Face) {
                document_->SelectSolidFacesInScreenRect(rect, world_to_screen, rect_selection_action_);
            } else if (selection_mode_ == SelectionMode::Edge) {
                document_->SelectSolidEdgesInScreenRect(rect, world_to_screen, rect_selection_action_);
            } else if (selection_mode_ == SelectionMode::Point) {
                document_->SelectCurvePointsInScreenRect(rect, world_to_screen, rect_selection_action_);
            }
            emit SelectionChanged();
        }
        selecting_with_rect_ = false;
        update();
    }

    if (event->button() == Qt::LeftButton) {
        ++edge_quick_menu_generation_;
        const SelectionMode requested_mode = selection_mode_;
        const bool quick_menu_available = tool_ == ToolMode::Select
            && document_
            && ((requested_mode == SelectionMode::Edge
                 && document_->HasSelectedSolidEdge())
                || (requested_mode == SelectionMode::Face
                    && document_->HasSelectedSolidFace())
                || (requested_mode == SelectionMode::Object
                    && document_->HasSelection()));
        if (select_click_completed && quick_menu_available) {
            edge_quick_menu_anchor_ = event->pos();
            const unsigned int generation = edge_quick_menu_generation_;
            QTimer::singleShot(550, this, [this, generation, requested_mode]() {
                const bool selection_is_still_valid = document_
                    && ((requested_mode == SelectionMode::Edge
                         && document_->HasSelectedSolidEdge())
                        || (requested_mode == SelectionMode::Face
                            && document_->HasSelectedSolidFace())
                        || (requested_mode == SelectionMode::Object
                            && document_->HasSelection()));
                if (generation != edge_quick_menu_generation_
                    || tool_ != ToolMode::Select
                    || selection_mode_ != requested_mode
                    || !selection_is_still_valid) {
                    return;
                }
                const QPoint cursor_position = mapFromGlobal(QCursor::pos());
                const QPoint delta = cursor_position - edge_quick_menu_anchor_;
                if (delta.x() * delta.x() + delta.y() * delta.y() > 18 * 18) {
                    return;
                }
                const QPoint menu_position =
                    mapToGlobal(edge_quick_menu_anchor_ + QPoint(10, 10));
                if (requested_mode == SelectionMode::Edge) {
                    emit EdgeQuickMenuRequested(menu_position);
                } else if (requested_mode == SelectionMode::Face) {
                    emit FaceQuickMenuRequested(menu_position);
                } else if (requested_mode == SelectionMode::Object) {
                    emit ObjectQuickMenuRequested(menu_position);
                }
            });
        }
    }
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
    dragging_sketch_handle_ = false;
    creation_snap_active_ = false;
    active_sketch_handle_kind_ = SketchHandleKind::None;
    curve_point_drag_has_plane_ = false;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    dragging_face_extrude_ = false;
    dragging_draft_face_ = false;
    selecting_edit_points_ = false;
    selecting_with_rect_ = false;
    transform_drag_has_preview_ = false;
    transform_drag_move_delta_ = {};
    transform_drag_rotation_angle_ = 0.0f;
    transform_drag_scale_factor_ = 1.0f;
    active_transform_axis_ = TransformAxis::None;
    highlighted_transform_axis_ = TransformAxis::None;
    if (material_interaction_mode_ == MaterialInteractionMode::None) {
        if (tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline || tool_ == ToolMode::EditPoint) {
            setCursor(Qt::CrossCursor);
        } else if (tool_ == ToolMode::SketchRectangle
                   || tool_ == ToolMode::SketchPolyline
                   || tool_ == ToolMode::SketchBezier
                   || tool_ == ToolMode::SketchConvertBezier
                   || tool_ == ToolMode::SketchConvertArc
                   || tool_ == ToolMode::SolidBoxRectangle
                   || tool_ == ToolMode::SolidCylinderCircle) {
            setCursor(Qt::CrossCursor);
        } else {
            unsetCursor();
        }
    }
    update();
}

void OpenGLViewport::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && tool_ == ToolMode::MovePointToPoint
        && move_point_stage_ == MovePointStage::SelectObjects) {
        if (document_ && document_->HasSelection()) {
            move_point_stage_ = MovePointStage::PickSource;
            emit StatusTextChanged("Move Point to Point: pick source point");
        } else {
            emit StatusTextChanged("Move Point to Point: select at least one object");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::MovePointToPoint) {
        SetTool(ToolMode::Select);
        unsetCursor();
        emit StatusTextChanged("Move Point to Point canceled. Select tool is active");
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && tool_ == ToolMode::SketchPolyline) {
        if (!CommitSketchPolyline(false)) {
            emit StatusTextChanged("Sketch Polyline: need at least 2 points");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && tool_ == ToolMode::SketchPolyline) {
        if (!CommitSketchPolyline(true)) {
            emit StatusTextChanged("Sketch Polyline: need at least 3 points to close");
        }
        event->accept();
        return;
    }

    if (selection_confirmation_mode_
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit SelectionConfirmed();
        event->accept();
        return;
    }

    if (selection_confirmation_mode_ && event->key() == Qt::Key_Escape) {
        selection_confirmation_mode_ = false;
        emit SelectionCommandCanceled();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchPolyline) {
        if (!sketch_polyline_points_.empty()) {
            sketch_polyline_points_.clear();
            sketch_polyline_preview_valid_ = false;
            emit StatusTextChanged(QString("%1: Polyline canceled").arg(sketch_name_));
            update();
        } else {
            EndSketch();
            emit StatusTextChanged("Sketch closed");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchBezier) {
        if (!sketch_bezier_points_.empty()) {
            sketch_bezier_points_.clear();
            sketch_bezier_preview_valid_ = false;
            emit StatusTextChanged(QString("%1: Bezier canceled").arg(sketch_name_));
            update();
        } else {
            EndSketch();
            emit StatusTextChanged("Sketch closed");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchConvertBezier) {
        BeginEditSelectedSketch();
        emit StatusTextChanged("Convert to Bezier canceled");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchConvertArc) {
        if (sketch_arc_has_line_) {
            sketch_arc_has_line_ = false;
            emit StatusTextChanged(
                QString("%1: select a straight segment").arg(sketch_name_));
            update();
        } else {
            BeginEditSelectedSketch();
            emit StatusTextChanged("Line to Arc canceled");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchRectangle) {
        if (sketch_waiting_for_face_) {
            sketch_waiting_for_face_ = false;
            SetTool(ToolMode::Select);
            unsetCursor();
            emit SketchFaceSelectionFinished(false);
            emit StatusTextChanged("Sketch face selection canceled");
            event->accept();
            return;
        }
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

    if (event->key() == Qt::Key_Escape
        && (tool_ == ToolMode::SolidBoxRectangle
            || tool_ == ToolMode::SolidCylinderCircle)) {
        if (sketch_rectangle_has_first_point_) {
            sketch_rectangle_has_first_point_ = false;
            sketch_rectangle_preview_valid_ = false;
            emit StatusTextChanged(
                tool_ == ToolMode::SolidBoxRectangle
                    ? "BOX: first point canceled"
                    : "CYLINDER: center canceled");
            update();
        } else {
            SetTool(ToolMode::Select);
            unsetCursor();
            emit StatusTextChanged(
                tool_ == ToolMode::SolidBoxRectangle
                    ? "BOX canceled"
                    : "CYLINDER canceled");
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

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Transform) {
        if (dragging_transform_ && transform_drag_has_preview_) {
            CommitTransformDrag();
        }
        dragging_transform_ = false;
        transform_drag_has_preview_ = false;
        transform_drag_move_delta_ = {};
        transform_drag_axis_ = {};
        transform_drag_rotation_angle_ = 0.0f;
        transform_drag_scale_factor_ = 1.0f;
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Transform finished. Select tool is active");
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

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Select && editing_sketch_) {
        editing_sketch_ = false;
        dragging_sketch_handle_ = false;
        active_sketch_handle_kind_ = SketchHandleKind::None;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
        if (material_interaction_mode_ == MaterialInteractionMode::None) {
            unsetCursor();
        }
        emit StatusTextChanged("Sketch edit finished");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape
        && (tool_ == ToolMode::FaceExtrude
            || tool_ == ToolMode::DraftFace)) {
        const bool cancel_extrude = tool_ == ToolMode::FaceExtrude;
        if (document_) {
            if (cancel_extrude) {
                document_->CancelLiveExtrudeSelectedSolidFace();
            } else {
                document_->CancelLiveDraftFace();
            }
        }
        dragging_face_extrude_ = false;
        dragging_draft_face_ = false;
        face_extrude_distance_ = 0.0f;
        draft_face_angle_degrees_ = 0.0;
        SetTool(ToolMode::Select);
        emit SelectionChanged();
        emit DocumentChanged();
        emit StatusTextChanged(
            cancel_extrude ? "Extrude Face canceled" : "Draft Face canceled");
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
        if (event->mimeData()->hasUrls()) {
            for (const QUrl& url : event->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
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
        if (event->mimeData()->hasUrls()) {
            for (const QUrl& url : event->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
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
        QStringList paths;
        if (event->mimeData()->hasUrls()) {
            for (const QUrl& url : event->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    paths.push_back(url.toLocalFile());
                }
            }
        }
        if (!paths.isEmpty()) {
            material_drag_active_ = false;
            event->acceptProposedAction();
            emit FilesDropped(paths);
            update();
            return;
        }
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

    if (selection_mode_ == SelectionMode::Object
        && document_->SelectPolylineAtScreen(screen_point, world_to_screen, 8.0f, action)) {
        emit SelectionChanged();
        update();
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
    if (CSolid* solid = document_->FindSolidAtScreen(screen_point, project_world)) {
        return solid;
    }

    auto& objects = document_->GetObjects();
    CMesh3D* best_mesh = nullptr;
    float best_depth = std::numeric_limits<float>::max();
    for (const auto& object : objects) {
        auto* mesh = dynamic_cast<CMesh3D*>(object.get());
        if (!mesh || !document_->IsObjectSelectable(*mesh)) {
            continue;
        }
        float depth = 0.0f;
        if (mesh->HitTestMeshScreen(screen_point, project_world, depth) && depth < best_depth) {
            best_mesh = mesh;
            best_depth = depth;
        }
    }
    return best_mesh;
}

void OpenGLViewport::DrawCurveAt(const QPoint& point) {
    CPoint3d scene_point{};
    if (!ScreenToCurvePlane(point, scene_point)) {
        return;
    }
    const bool snapped = SnapCreationPoint(point, scene_point, false);
    SetCreationSnapCursor(snapped);

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
    document_->ClearSelection();
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
    const bool snapped = SnapCreationPoint(point, scene_point, false);
    SetCreationSnapCursor(snapped);

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
    SetTool(ToolMode::Select);
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
    SetTool(ToolMode::Select);
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

    const SelectionAction solid_action = add_to_selection ? SelectionAction::Add : SelectionAction::Replace;
    if (document_->SelectPolylineAtScreen(screen_point, world_to_screen, 8.0f, solid_action)) {
        document_->ExpandSelectedGroups();
        emit SelectionChanged();
        emit DocumentChanged();
        update();
        return;
    }

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

bool OpenGLViewport::HitTestSelectedSketchHandle(
    const QPoint& point,
    SketchHandleKind& kind,
    size_t& index) const {
    kind = SketchHandleKind::None;
    index = 0;
    if (!document_) {
        return false;
    }

    const CSmartLine* sketch = document_->GetSelectedSketch();
    if (!sketch) {
        return false;
    }

    const DomPoint mouse{point.x(), point.y()};
    float best_distance = 12.0f;
    auto consider = [&](CPoint3d world_point, SketchHandleKind candidate_kind, size_t candidate_index) {
        DomPoint screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(world_point),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                screen)) {
            return;
        }
        const float dx = static_cast<float>(mouse.x - screen.x);
        const float dy = static_cast<float>(mouse.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            kind = candidate_kind;
            index = candidate_index;
        }
    };

    for (size_t node_index = 0; node_index < sketch->GetNodeCount(); ++node_index) {
        consider(sketch->GetNodeWorld(node_index), SketchHandleKind::Node, node_index);
    }
    for (size_t fillet_index = 0; fillet_index < sketch->GetNumFillets(); ++fillet_index) {
        const CFillet* fillet = sketch->GetFillet(fillet_index);
        if (!fillet) {
            continue;
        }
        const CLinkLine* first = sketch->GetLine(fillet->GetFirstLineIndex());
        const CLinkLine* second = sketch->GetLine(fillet->GetSecondLineIndex());
        if (first && second && fillet->Calculate(*first, *second).valid) {
            consider(sketch->GetFilletGripWorld(fillet_index), SketchHandleKind::Fillet, fillet_index);
        }
    }
    for (size_t control_index = 0;
         control_index < sketch->GetBezierControlPointCount();
         ++control_index) {
        consider(
            sketch->GetBezierControlPointWorld(control_index),
            SketchHandleKind::BezierControl,
            control_index);
    }
    for (size_t grip_index = 0;
         grip_index < sketch->GetArcGripCount();
         ++grip_index) {
        consider(
            sketch->GetArcGripWorld(grip_index),
            SketchHandleKind::ArcControl,
            grip_index);
    }
    return kind != SketchHandleKind::None;
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
        if (!document_ || !sketch_waiting_for_face_) {
            return;
        }

        const DomPoint screen_point{point.x(), point.y()};
        auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            viewport_camera_basis(camera_, forward, right, up);
            depth = dot(world - camera_position(camera_), forward);
            return depth > 0.0f
                && renderer_.WorldToScreen(
                    world,
                    camera_,
                    orthographic_projection_,
                    width(),
                    height(),
                    screen);
        };
        if (!document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            emit StatusTextChanged(
                QString("%1: planar body face not found").arg(sketch_name_));
            update();
            return;
        }

        Vec3 origin{};
        Vec3 x_axis{};
        Vec3 y_axis{};
        Vec3 normal{};
        unsigned long body_id = 0;
        int face_index = -1;
        if (!document_->GetSelectedSolidFaceSketchPlane(
                origin,
                x_axis,
                y_axis,
                normal,
                body_id,
                face_index)) {
            emit StatusTextChanged(
                QString("%1: selected face is not planar").arg(sketch_name_));
            update();
            return;
        }

        sketch_waiting_for_face_ = false;
        BeginSketchOnFace(
            sketch_name_,
            origin,
            x_axis,
            y_axis,
            normal,
            body_id,
            face_index);
        emit SketchFaceSelectionFinished(true);
        emit SelectionChanged();
        update();
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Rectangle: point is outside sketch plane");
        return;
    }
    SetCreationSnapCursor(SnapCreationPoint(point, sketch_point, true));

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
    const bool created = document_->CreateSketchPolyline(
        points,
        true,
        sketch_name_.toStdString(),
        CPoint3d(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z),
        CPoint3d(sketch_u_.x, sketch_u_.y, sketch_u_.z),
        CPoint3d(sketch_v_.x, sketch_v_.y, sketch_v_.z));
    if (!created) {
        emit StatusTextChanged("Sketch Rectangle: cannot create contour");
        return;
    }
    ApplyPendingSketchAttachment();
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    emit DocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(QString("%1: Rectangle created").arg(sketch_name_));
    update();
}

void OpenGLViewport::HandleSketchPolylineClick(const QPoint& point) {
    if (!document_ || !sketch_active_) {
        return;
    }

    if (IsNearSketchPolylineFirstPoint(point)) {
        CommitSketchPolyline(true);
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Polyline: point is outside sketch plane");
        return;
    }
    const bool snapped = SnapCreationPoint(point, sketch_point, true);
    SetCreationSnapCursor(snapped);
    if (!sketch_polyline_points_.empty()) {
        if (!snapped) {
            sketch_point = AlignSketchPolylinePoint(sketch_point);
        }
        const CPoint3d& last = sketch_polyline_points_.back();
        const double dx = sketch_point.x - last.x;
        const double dy = sketch_point.y - last.y;
        const double dz = sketch_point.z - last.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 1.0e-8) {
            return;
        }
    }

    sketch_polyline_points_.push_back(sketch_point);
    sketch_polyline_preview_point_ = sketch_point;
    sketch_polyline_preview_valid_ = true;
    emit StatusTextChanged(sketch_polyline_points_.size() == 1
        ? QString("%1: click next point").arg(sketch_name_)
        : QString("%1: click next point, first point closes, Enter finishes").arg(sketch_name_));
    update();
}

CPoint3d OpenGLViewport::AlignSketchPolylinePoint(const CPoint3d& point) const {
    if (sketch_polyline_points_.empty()) {
        return point;
    }

    const Vec3 last = point_to_vec3(sketch_polyline_points_.back());
    const Vec3 candidate = point_to_vec3(point);
    const Vec3 delta = candidate - last;
    float u_distance = dot(delta, sketch_u_);
    float v_distance = dot(delta, sketch_v_);
    const double angle = std::atan2(std::abs(v_distance), std::abs(u_distance))
        * 180.0 / 3.14159265358979323846;
    if (angle <= sketch_alignment_angle_degrees_) {
        v_distance = 0.0f;
    } else if (90.0 - angle <= sketch_alignment_angle_degrees_) {
        u_distance = 0.0f;
    }

    const Vec3 aligned = last + sketch_u_ * u_distance + sketch_v_ * v_distance;
    return CPoint3d(aligned.x, aligned.y, aligned.z);
}

bool OpenGLViewport::IsNearSketchPolylineFirstPoint(const QPoint& point) const {
    if (sketch_polyline_points_.size() < 3) {
        return false;
    }
    DomPoint first_screen{};
    if (!renderer_.WorldToScreen(
            point_to_vec3(sketch_polyline_points_.front()),
            camera_,
            orthographic_projection_,
            width(),
            height(),
            first_screen)) {
        return false;
    }
    const float dx = static_cast<float>(point.x() - first_screen.x);
    const float dy = static_cast<float>(point.y() - first_screen.y);
    return snapping_enabled_
        && std::sqrt(dx * dx + dy * dy) <= static_cast<float>(capture_distance_pixels_);
}

bool OpenGLViewport::IsNearSelectedSketchFirstPoint(const QPoint& point) const {
    const CSmartLine* sketch = document_ ? document_->GetSelectedSketch() : nullptr;
    if (!sketch || sketch->IsClosed() || sketch->GetNumLines() == 0) {
        return false;
    }
    DomPoint first_screen{};
    if (!renderer_.WorldToScreen(
            point_to_vec3(sketch->GetNodeWorld(0)),
            camera_,
            orthographic_projection_,
            width(),
            height(),
            first_screen)) {
        return false;
    }
    const float dx = static_cast<float>(point.x() - first_screen.x);
    const float dy = static_cast<float>(point.y() - first_screen.y);
    return snapping_enabled_
        && std::sqrt(dx * dx + dy * dy) <= static_cast<float>(capture_distance_pixels_);
}

bool OpenGLViewport::CommitSketchPolyline(bool closed) {
    const std::size_t minimum_points = closed ? 3 : 2;
    if (!document_ || sketch_polyline_points_.size() < minimum_points) {
        return false;
    }

    const bool created = document_->CreateSketchPolyline(
        sketch_polyline_points_,
        closed,
        sketch_name_.toStdString(),
        CPoint3d(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z),
        CPoint3d(sketch_u_.x, sketch_u_.y, sketch_u_.z),
        CPoint3d(sketch_v_.x, sketch_v_.y, sketch_v_.z));
    if (!created) {
        emit StatusTextChanged("Sketch Polyline: cannot create contour");
        return false;
    }
    ApplyPendingSketchAttachment();

    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    emit DocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(closed
        ? QString("%1: closed polyline created").arg(sketch_name_)
        : QString("%1: open polyline created").arg(sketch_name_));
    update();
    return true;
}

void OpenGLViewport::HandleSketchBezierClick(const QPoint& point) {
    if (!document_ || !sketch_active_) {
        return;
    }
    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Bezier: point is outside sketch plane");
        return;
    }
    bool snapped = false;
    if (sketch_bezier_points_.size() == 3
        && IsNearSelectedSketchFirstPoint(point)) {
        sketch_point = document_->GetSelectedSketch()->GetNodeWorld(0);
        snapped = true;
    } else {
        snapped = SnapCreationPoint(point, sketch_point, true);
    }
    SetCreationSnapCursor(snapped);
    sketch_bezier_points_.push_back(sketch_point);
    sketch_bezier_preview_point_ = sketch_point;
    sketch_bezier_preview_valid_ = true;
    if (sketch_bezier_points_.size() == 4) {
        CommitSketchBezier();
        return;
    }
    static const char* prompts[] = {
        "click first control point",
        "click second control point",
        "click end point"
    };
    emit StatusTextChanged(
        QString("%1: Bezier — %2")
            .arg(sketch_name_)
            .arg(prompts[sketch_bezier_points_.size() - 1]));
    update();
}

bool OpenGLViewport::CommitSketchBezier() {
    if (!document_ || sketch_bezier_points_.size() != 4) {
        return false;
    }

    bool created = false;
    bool closed = false;
    CSmartLine* selected_sketch = document_->GetSelectedSketch();
    if (selected_sketch && !selected_sketch->IsClosed()) {
        const CPoint3d first_point = selected_sketch->GetNodeWorld(0);
        const CPoint3d& end_point = sketch_bezier_points_[3];
        const double dx = end_point.x - first_point.x;
        const double dy = end_point.y - first_point.y;
        const double dz = end_point.z - first_point.z;
        const bool close_requested =
            selected_sketch->GetNumLines() > 0
            && dx * dx + dy * dy + dz * dz <= 1.0e-12;
        created = selected_sketch->AddBezierWorld(
            sketch_bezier_points_[0],
            sketch_bezier_points_[1],
            sketch_bezier_points_[2],
            sketch_bezier_points_[3],
            selected_sketch->GetNumLines() > 0);
        if (created && close_requested) {
            closed = selected_sketch->SetClosed(true);
            created = closed;
        }
    } else {
        created = document_->CreateSketchBezier(
            sketch_bezier_points_,
            sketch_name_.toStdString(),
            CPoint3d(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z),
            CPoint3d(sketch_u_.x, sketch_u_.y, sketch_u_.z),
            CPoint3d(sketch_v_.x, sketch_v_.y, sketch_v_.z));
    }
    if (!created) {
        emit StatusTextChanged("Sketch Bezier: cannot create curve");
        return false;
    }
    ApplyPendingSketchAttachment();

    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    emit DocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(closed
        ? QString("%1: Bezier contour closed").arg(sketch_name_)
        : QString("%1: Bezier created; click next start point").arg(sketch_name_));
    update();
    return true;
}

void OpenGLViewport::ApplyPendingSketchAttachment() {
    if (!document_ || sketch_attachment_body_id_ == 0
        || sketch_attachment_face_index_ < 0) {
        return;
    }
    if (CSmartLine* sketch = document_->GetSelectedSketch()) {
        sketch->SetFaceAttachment(
            sketch_attachment_body_id_, sketch_attachment_face_index_);
    }
}

void OpenGLViewport::HandleSketchConvertLineToBezierClick(const QPoint& point) {
    CSmartLine* sketch = document_ ? document_->GetSelectedSketch() : nullptr;
    if (!sketch) {
        emit StatusTextChanged("Convert to Bezier: select a sketch first");
        return;
    }

    const DomPoint mouse{point.x(), point.y()};
    double best_distance = 12.0;
    std::size_t best_line = sketch->GetNumLines();
    for (std::size_t line_index = 0; line_index < sketch->GetNumLines(); ++line_index) {
        const CLinkLine* line = sketch->GetLine(line_index);
        if (!line || line->GetType() == LinkLineType::Bezier) {
            continue;
        }
        const CPoint3d start = sketch->LocalToWorld(line->GetStart());
        const CPoint3d end = sketch->LocalToWorld(line->GetEnd());
        DomPoint start_screen{};
        DomPoint end_screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(start),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                start_screen)
            || !renderer_.WorldToScreen(
                point_to_vec3(end),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                end_screen)) {
            continue;
        }
        const double distance = DistanceToScreenSegment(mouse, start_screen, end_screen);
        if (distance < best_distance) {
            best_distance = distance;
            best_line = line_index;
        }
    }

    if (best_line >= sketch->GetNumLines() || !sketch->ConvertLineToBezier(best_line)) {
        emit StatusTextChanged("Convert to Bezier: click closer to a straight segment");
        return;
    }

    emit DocumentChanged();
    BeginEditSelectedSketch();
    emit StatusTextChanged(
        QString("%1: segment converted to Bezier; drag the blue control points")
            .arg(sketch_name_));
    update();
}

void OpenGLViewport::HandleSketchConvertLineToArcClick(const QPoint& point) {
    CSmartLine* sketch = document_ ? document_->GetSelectedSketch() : nullptr;
    if (!sketch) {
        emit StatusTextChanged("Line to Arc: select a sketch first");
        return;
    }

    if (!sketch_arc_has_line_) {
        const DomPoint mouse{point.x(), point.y()};
        double best_distance = 12.0;
        std::size_t best_line = sketch->GetNumLines();
        for (std::size_t line_index = 0; line_index < sketch->GetNumLines(); ++line_index) {
            const CLinkLine* line = sketch->GetLine(line_index);
            if (!line || (line->GetType() != LinkLineType::Segment
                          && line->GetType() != LinkLineType::Horizontal
                          && line->GetType() != LinkLineType::Vertical)) {
                continue;
            }
            DomPoint start_screen{};
            DomPoint end_screen{};
            if (!renderer_.WorldToScreen(
                    point_to_vec3(sketch->LocalToWorld(line->GetStart())), camera_,
                    orthographic_projection_, width(), height(), start_screen)
                || !renderer_.WorldToScreen(
                    point_to_vec3(sketch->LocalToWorld(line->GetEnd())), camera_,
                    orthographic_projection_, width(), height(), end_screen)) {
                continue;
            }
            const double distance = DistanceToScreenSegment(mouse, start_screen, end_screen);
            if (distance < best_distance) {
                best_distance = distance;
                best_line = line_index;
            }
        }
        if (best_line >= sketch->GetNumLines()) {
            emit StatusTextChanged("Line to Arc: click closer to a straight segment");
            return;
        }
        sketch_arc_line_index_ = best_line;
        sketch_arc_has_line_ = true;
        emit StatusTextChanged("Line to Arc: click a point on the desired arc");
        return;
    }

    CPoint3d point_on_arc;
    const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
    if (!ScreenToWorldPlane(
            point, point_to_vec3(system.origin), point_to_vec3(system.normal), point_on_arc)
        || !sketch->ConvertLineToArc(sketch_arc_line_index_, point_on_arc)) {
        emit StatusTextChanged(
            "Line to Arc: point is too close to the line; choose a point farther away");
        return;
    }
    sketch_arc_has_line_ = false;
    emit DocumentChanged();
    BeginEditSelectedSketch();
    emit StatusTextChanged(QString("%1: circular arc created").arg(sketch_name_));
    update();
}

void OpenGLViewport::HandleSolidBoxRectangleClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (solid_box_waiting_for_face_) {
        const DomPoint screen_point{point.x(), point.y()};
        auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            viewport_camera_basis(camera_, forward, right, up);
            depth = dot(world - camera_position(camera_), forward);
            return depth > 0.0f
                && renderer_.WorldToScreen(
                    world,
                    camera_,
                    orthographic_projection_,
                    width(),
                    height(),
                    screen);
        };
        if (!document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            emit StatusTextChanged("BOX: planar body face not found");
            update();
            return;
        }

        Vec3 x_axis{};
        Vec3 y_axis{};
        unsigned long body_id = 0;
        int face_index = -1;
        if (!document_->GetSelectedSolidFaceSketchPlane(
                sketch_origin_,
                x_axis,
                y_axis,
                sketch_normal_,
                body_id,
                face_index)) {
            emit StatusTextChanged("BOX: selected face is not planar");
            update();
            return;
        }

        sketch_u_ = x_axis;
        sketch_v_ = y_axis;
        solid_box_target_body_id_ = body_id;
        solid_box_waiting_for_face_ = false;
        emit SelectionChanged();
        emit StatusTextChanged("BOX: click first rectangle corner on the selected face");
        update();
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("BOX: point is outside placement plane");
        return;
    }
    SetCreationSnapCursor(SnapCreationPoint(point, sketch_point, true));

    if (!sketch_rectangle_has_first_point_) {
        sketch_rectangle_first_point_ = sketch_point;
        sketch_rectangle_preview_point_ = sketch_point;
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_preview_valid_ = true;
        emit StatusTextChanged("BOX: click opposite rectangle corner");
        update();
        return;
    }

    std::vector<ToolParameter> parameters = SolidBoxParametersFromRectangle(sketch_rectangle_first_point_, sketch_point);
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    unsetCursor();
    SetTool(ToolMode::Select);
    emit SolidBoxRectangleFinished(parameters);
    emit StatusTextChanged("BOX: rectangle created");
    update();
}

void OpenGLViewport::HandleSolidCylinderCircleClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (solid_box_waiting_for_face_) {
        const DomPoint screen_point{point.x(), point.y()};
        auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            viewport_camera_basis(camera_, forward, right, up);
            depth = dot(world - camera_position(camera_), forward);
            return depth > 0.0f
                && renderer_.WorldToScreen(
                    world, camera_, orthographic_projection_, width(), height(), screen);
        };
        if (!document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            emit StatusTextChanged("CYLINDER: planar body face not found");
            update();
            return;
        }

        Vec3 x_axis{};
        Vec3 y_axis{};
        unsigned long body_id = 0;
        int face_index = -1;
        if (!document_->GetSelectedSolidFaceSketchPlane(
                sketch_origin_, x_axis, y_axis, sketch_normal_, body_id, face_index)) {
            emit StatusTextChanged("CYLINDER: selected face is not planar");
            update();
            return;
        }
        sketch_u_ = x_axis;
        sketch_v_ = y_axis;
        solid_box_target_body_id_ = body_id;
        solid_box_waiting_for_face_ = false;
        emit SelectionChanged();
        emit StatusTextChanged("CYLINDER: click circle center on the selected face");
        update();
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("CYLINDER: point is outside placement plane");
        return;
    }
    SetCreationSnapCursor(SnapCreationPoint(point, sketch_point, true));

    if (!sketch_rectangle_has_first_point_) {
        sketch_rectangle_first_point_ = sketch_point;
        sketch_rectangle_preview_point_ = sketch_point;
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_preview_valid_ = true;
        emit StatusTextChanged("CYLINDER: click circle radius");
        update();
        return;
    }

    std::vector<ToolParameter> parameters =
        SolidCylinderParametersFromCircle(sketch_rectangle_first_point_, sketch_point);
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    unsetCursor();
    SetTool(ToolMode::Select);
    emit SolidCylinderCircleFinished(parameters);
    emit StatusTextChanged("CYLINDER: circle created");
    update();
}

void OpenGLViewport::HandleSketchFilletClick(const QPoint& point) {
    if (!document_) {
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

bool OpenGLViewport::SnapCreationPoint(const QPoint& point,
                                       CPoint3d& result,
                                       bool require_sketch_plane) const {
    if (!snapping_enabled_ || !document_) {
        return false;
    }

    const DomPoint mouse{point.x(), point.y()};
    float best_distance = static_cast<float>(capture_distance_pixels_);
    bool found = require_sketch_plane
        && SnapSketchGridPoint(
            point,
            sketch_origin_,
            sketch_u_,
            sketch_v_,
            result,
            best_distance);
    const CAlfaObject* active_curve = nullptr;
    if (tool_ == ToolMode::DrawCurve) {
        active_curve = &document_->GetActivePolyline();
    } else if (tool_ == ToolMode::DrawBSpline) {
        active_curve = &document_->GetActiveBSpline();
    }

    const auto consider = [&](const CAlfaObject* object,
                              const CPoint3d& candidate,
                              bool is_last_active_point) {
        if (is_last_active_point || !object || !object->IsVisible()) {
            return;
        }
        if (require_sketch_plane) {
            const Vec3 delta = point_to_vec3(candidate) - sketch_origin_;
            if (std::fabs(dot(delta, sketch_normal_)) > 0.001f) {
                return;
            }
        }
        DomPoint screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(candidate),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                screen)) {
            return;
        }
        const float dx = static_cast<float>(mouse.x - screen.x);
        const float dy = static_cast<float>(mouse.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            result = candidate;
            found = true;
        }
    };

    for (const auto& object_ptr : document_->GetObjects()) {
        const CAlfaObject* object = object_ptr.get();
        if (!object || !object->IsVisible()) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
            const auto& points = polyline->GetPoints();
            for (std::size_t index = 0; index < points.size(); ++index) {
                consider(
                    object,
                    points[index],
                    object == active_curve && index + 1 == points.size());
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
            const auto& points = spline->GetPoints();
            for (std::size_t index = 0; index < points.size(); ++index) {
                consider(
                    object,
                    points[index],
                    object == active_curve && index + 1 == points.size());
            }
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(object)) {
            for (std::size_t index = 0; index < sketch->GetNodeCount(); ++index) {
                consider(object, sketch->GetNodeWorld(index), false);
            }
        } else if (const auto* solid = dynamic_cast<const CSolid*>(object)) {
            for (TopExp_Explorer explorer(solid->m_Shape, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
                const gp_Pnt vertex = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
                consider(object, CPoint3d(vertex.X(), vertex.Y(), vertex.Z()), false);
            }
        }
    }
    return found;
}

void OpenGLViewport::HandleMovePointToPointClick(const QPoint& point) {
    if (!document_) {
        return;
    }
    if (move_point_stage_ == MovePointStage::SelectObjects) {
        SelectionAction action = SelectionAction::Replace;
        if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
            action = SelectionAction::Add;
        } else if (QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            action = SelectionAction::Remove;
        }
        SelectAt(point, action);
        emit StatusTextChanged("Move Point to Point: selection ready, press Enter");
        return;
    }

    CPoint3d picked{};
    if (!SnapCreationPoint(point, picked, false)) {
        emit StatusTextChanged("Move Point to Point: click a visible vertex or sketch point");
        return;
    }
    if (move_point_stage_ == MovePointStage::PickSource) {
        move_point_source_ = picked;
        move_point_stage_ = MovePointStage::PickTarget;
        emit StatusTextChanged("Move Point to Point: pick target point");
        return;
    }

    const Vec3 delta{
        static_cast<float>(picked.x - move_point_source_.x),
        static_cast<float>(picked.y - move_point_source_.y),
        static_cast<float>(picked.z - move_point_source_.z)};
    if (document_->MoveSelectedObjects(delta)) {
        emit DocumentChanged();
        emit SelectionChanged();
        emit StatusTextChanged("Move Point to Point completed");
    } else {
        emit StatusTextChanged("Move Point to Point: selected objects cannot be moved");
    }
    SetTool(ToolMode::Select);
    unsetCursor();
    update();
}

bool OpenGLViewport::SnapSketchGridPoint(const QPoint& point,
                                         Vec3 origin,
                                         Vec3 u_axis,
                                         Vec3 v_axis,
                                         CPoint3d& result,
                                         float& best_distance) const {
    if (!snapping_enabled_ || kDefaultGridStep <= 0.0f) {
        return false;
    }

    if (dot(u_axis, u_axis) <= 0.000001f
        || dot(v_axis, v_axis) <= 0.000001f) {
        return false;
    }
    u_axis = normalize(u_axis);
    v_axis = normalize(v_axis);

    const Vec3 source = point_to_vec3(result);
    const Vec3 delta = source - origin;
    const float u_coordinate = dot(delta, u_axis);
    const float v_coordinate = dot(delta, v_axis);
    const Vec3 candidate =
        origin
        + u_axis * (std::round(u_coordinate / kDefaultGridStep) * kDefaultGridStep)
        + v_axis * (std::round(v_coordinate / kDefaultGridStep) * kDefaultGridStep);

    DomPoint screen{};
    if (!renderer_.WorldToScreen(
            candidate,
            camera_,
            orthographic_projection_,
            width(),
            height(),
            screen)) {
        return false;
    }

    const float dx = static_cast<float>(point.x() - screen.x);
    const float dy = static_cast<float>(point.y() - screen.y);
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance > best_distance) {
        return false;
    }

    best_distance = distance;
    result = CPoint3d(candidate.x, candidate.y, candidate.z);
    return true;
}

void OpenGLViewport::SetCreationSnapCursor(bool snapped) {
    if (creation_snap_active_ == snapped) {
        return;
    }
    creation_snap_active_ = snapped;
    if (snapped) {
        setCursor(captured_point_cursor());
    } else if (dragging_sketch_handle_) {
        setCursor(Qt::ClosedHandCursor);
    } else if (tool_ == ToolMode::DrawCurve
               || tool_ == ToolMode::DrawBSpline
               || tool_ == ToolMode::SketchRectangle
               || tool_ == ToolMode::SketchPolyline
               || tool_ == ToolMode::SketchBezier
               || tool_ == ToolMode::SolidBoxRectangle
               || tool_ == ToolMode::SolidCylinderCircle) {
        setCursor(Qt::CrossCursor);
    }
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

std::vector<ToolParameter> OpenGLViewport::SolidBoxParametersFromRectangle(const CPoint3d& first, const CPoint3d& second) const {
    const Vec3 a{static_cast<float>(first.x), static_cast<float>(first.y), static_cast<float>(first.z)};
    const Vec3 b{static_cast<float>(second.x), static_cast<float>(second.y), static_cast<float>(second.z)};
    const Vec3 delta = b - a;
    const float length_signed = dot(delta, sketch_u_);
    const float width_signed = dot(delta, sketch_v_);
    const float length = std::max(std::fabs(length_signed), 0.001f);
    const float width = std::max(std::fabs(width_signed), 0.001f);
    Vec3 origin = a;
    if (length_signed < 0.0f) {
        origin = origin + sketch_u_ * length_signed;
    }
    if (width_signed < 0.0f) {
        origin = origin + sketch_v_ * width_signed;
    }
    const Vec3 u = sketch_u_;
    const Vec3 v = sketch_v_;
    const Vec3 normal = sketch_normal_;
    std::vector<ToolParameter> parameters{
        {"width", "Length", length, 0.001, 900.0, 0.5},
        {"height", "Width", width, 0.001, 900.0, 0.5},
        {"depth", "Height", 5.0, -900.0, 900.0, 0.5},
        {"origin.x", "Origin X", origin.x, -1000000.0, 1000000.0, 0.1},
        {"origin.y", "Origin Y", origin.y, -1000000.0, 1000000.0, 0.1},
        {"origin.z", "Origin Z", origin.z, -1000000.0, 1000000.0, 0.1},
        {"axis.u.x", "U X", u.x, -1.0, 1.0, 0.01},
        {"axis.u.y", "U Y", u.y, -1.0, 1.0, 0.01},
        {"axis.u.z", "U Z", u.z, -1.0, 1.0, 0.01},
        {"axis.v.x", "V X", v.x, -1.0, 1.0, 0.01},
        {"axis.v.y", "V Y", v.y, -1.0, 1.0, 0.01},
        {"axis.v.z", "V Z", v.z, -1.0, 1.0, 0.01},
        {"axis.n.x", "Normal X", normal.x, -1.0, 1.0, 0.01},
        {"axis.n.y", "Normal Y", normal.y, -1.0, 1.0, 0.01},
        {"axis.n.z", "Normal Z", normal.z, -1.0, 1.0, 0.01}
    };
    if (solid_box_target_body_id_ != 0) {
        parameters.push_back({
            "boolean.body_id",
            "Boolean Body",
            static_cast<double>(solid_box_target_body_id_),
            0.0,
            static_cast<double>(std::numeric_limits<unsigned long>::max()),
            1.0});
    }
    return parameters;
}

std::vector<ToolParameter> OpenGLViewport::SolidCylinderParametersFromCircle(
    const CPoint3d& center,
    const CPoint3d& radius_point) const {
    const Vec3 c{
        static_cast<float>(center.x),
        static_cast<float>(center.y),
        static_cast<float>(center.z)};
    const Vec3 p{
        static_cast<float>(radius_point.x),
        static_cast<float>(radius_point.y),
        static_cast<float>(radius_point.z)};
    const Vec3 delta = p - c;
    const double du = dot(delta, sketch_u_);
    const double dv = dot(delta, sketch_v_);
    const double radius = std::max(std::sqrt(du * du + dv * dv), 0.001);
    const Vec3 u = sketch_u_;
    const Vec3 v = sketch_v_;
    const Vec3 normal = sketch_normal_;
    std::vector<ToolParameter> parameters{
        {"diameter", "Diameter", radius * 2.0, 0.001, 900.0, 0.1},
        {"height", "Height", 5.0, -900.0, 900.0, 0.1},
        {"origin.x", "Origin X", c.x, -1000000.0, 1000000.0, 0.1},
        {"origin.y", "Origin Y", c.y, -1000000.0, 1000000.0, 0.1},
        {"origin.z", "Origin Z", c.z, -1000000.0, 1000000.0, 0.1},
        {"axis.u.x", "U X", u.x, -1.0, 1.0, 0.01},
        {"axis.u.y", "U Y", u.y, -1.0, 1.0, 0.01},
        {"axis.u.z", "U Z", u.z, -1.0, 1.0, 0.01},
        {"axis.v.x", "V X", v.x, -1.0, 1.0, 0.01},
        {"axis.v.y", "V Y", v.y, -1.0, 1.0, 0.01},
        {"axis.v.z", "V Z", v.z, -1.0, 1.0, 0.01},
        {"axis.n.x", "Normal X", normal.x, -1.0, 1.0, 0.01},
        {"axis.n.y", "Normal Y", normal.y, -1.0, 1.0, 0.01},
        {"axis.n.z", "Normal Z", normal.z, -1.0, 1.0, 0.01}
    };
    if (solid_box_target_body_id_ != 0) {
        parameters.push_back({
            "boolean.body_id",
            "Boolean Body",
            static_cast<double>(solid_box_target_body_id_),
            0.0,
            static_cast<double>(std::numeric_limits<unsigned long>::max()),
            1.0});
    }
    return parameters;
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

void OpenGLViewport::DrawSolidCylinderCirclePreview() {
    const Vec3 center{
        static_cast<float>(sketch_rectangle_first_point_.x),
        static_cast<float>(sketch_rectangle_first_point_.y),
        static_cast<float>(sketch_rectangle_first_point_.z)};
    const Vec3 radius_point{
        static_cast<float>(sketch_rectangle_preview_point_.x),
        static_cast<float>(sketch_rectangle_preview_point_.y),
        static_cast<float>(sketch_rectangle_preview_point_.z)};
    const Vec3 delta = radius_point - center;
    const float du = dot(delta, sketch_u_);
    const float dv = dot(delta, sketch_v_);
    const float radius = std::sqrt(du * du + dv * dv);
    if (radius <= 0.0001f) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    constexpr int segments = 64;
    glColor4f(1.0f, 0.95f, 0.05f, 0.12f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(center.x, center.y, center.z);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * 3.14159265358979323846f
            * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 point = center
            + sketch_u_ * (std::cos(angle) * radius)
            + sketch_v_ * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();

    glLineWidth(2.0f);
    glColor4f(1.0f, 0.95f, 0.05f, 0.95f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float angle = 2.0f * 3.14159265358979323846f
            * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 point = center
            + sketch_u_ * (std::cos(angle) * radius)
            + sketch_v_ * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchPolylinePreview() {
    if (sketch_polyline_points_.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(2.5f);
    glColor4f(0.95f, 0.15f, 0.75f, 1.0f);
    glBegin(GL_LINE_STRIP);
    for (const CPoint3d& point : sketch_polyline_points_) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    if (sketch_polyline_preview_valid_) {
        glVertex3f(
            static_cast<float>(sketch_polyline_preview_point_.x),
            static_cast<float>(sketch_polyline_preview_point_.y),
            static_cast<float>(sketch_polyline_preview_point_.z));
    }
    glEnd();

    glPointSize(8.0f);
    glColor4f(0.0f, 1.0f, 0.2f, 1.0f);
    glBegin(GL_POINTS);
    for (const CPoint3d& point : sketch_polyline_points_) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    glPointSize(1.0f);
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchBezierPreview() {
    if (sketch_bezier_points_.empty()) {
        return;
    }

    std::vector<CPoint3d> controls = sketch_bezier_points_;
    while (controls.size() < 4) {
        controls.push_back(
            sketch_bezier_preview_valid_
                ? sketch_bezier_preview_point_
                : controls.back());
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glColor4f(0.35f, 0.8f, 1.0f, 0.75f);
    glBegin(GL_LINE_STRIP);
    for (const CPoint3d& control : controls) {
        glVertex3d(control.x, control.y, control.z);
    }
    glEnd();

    glLineWidth(2.5f);
    glColor4f(0.95f, 0.15f, 0.75f, 1.0f);
    glBegin(GL_LINE_STRIP);
    for (int index = 0; index <= 32; ++index) {
        const double t = static_cast<double>(index) / 32.0;
        const double u = 1.0 - t;
        const double b0 = u * u * u;
        const double b1 = 3.0 * u * u * t;
        const double b2 = 3.0 * u * t * t;
        const double b3 = t * t * t;
        glVertex3d(
            b0 * controls[0].x + b1 * controls[1].x
                + b2 * controls[2].x + b3 * controls[3].x,
            b0 * controls[0].y + b1 * controls[1].y
                + b2 * controls[2].y + b3 * controls[3].y,
            b0 * controls[0].z + b1 * controls[1].z
                + b2 * controls[2].z + b3 * controls[3].z);
    }
    glEnd();

    glPointSize(8.0f);
    glColor4f(0.0f, 1.0f, 0.2f, 1.0f);
    glBegin(GL_POINTS);
    for (const CPoint3d& control : sketch_bezier_points_) {
        glVertex3d(control.x, control.y, control.z);
    }
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);
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

void OpenGLViewport::DrawSketchEditHandles() {
    const CSmartLine* sketch = document_ ? document_->GetSelectedSketch() : nullptr;
    if (!sketch) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glPointSize(10.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetNodeCount(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::Node
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CPoint3d point = sketch->GetNodeWorld(index);
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    glColor3f(0.2f, 0.75f, 1.0f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetBezierControlPointCount(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::BezierControl
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CPoint3d point = sketch->GetBezierControlPointWorld(index);
        glVertex3d(point.x, point.y, point.z);
    }
    glEnd();

    glColor3f(0.1f, 0.45f, 1.0f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetArcGripCount(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::ArcControl
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CPoint3d point = sketch->GetArcGripWorld(index);
        glVertex3d(point.x, point.y, point.z);
    }
    glEnd();

    glColor3f(1.0f, 0.05f, 0.05f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetNumFillets(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::Fillet
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CFillet* fillet = sketch->GetFillet(index);
        if (!fillet) {
            continue;
        }
        const CLinkLine* first = sketch->GetLine(fillet->GetFirstLineIndex());
        const CLinkLine* second = sketch->GetLine(fillet->GetSecondLineIndex());
        if (!first || !second || !fillet->Calculate(*first, *second).valid) {
            continue;
        }
        const CPoint3d point = sketch->GetFilletGripWorld(index);
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    if (highlighted_sketch_handle_kind_ != SketchHandleKind::None) {
        CPoint3d point{};
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::Node) {
            point = sketch->GetNodeWorld(highlighted_sketch_handle_index_);
        } else if (highlighted_sketch_handle_kind_ == SketchHandleKind::Fillet) {
            point = sketch->GetFilletGripWorld(highlighted_sketch_handle_index_);
        } else if (highlighted_sketch_handle_kind_ == SketchHandleKind::ArcControl) {
            point = sketch->GetArcGripWorld(highlighted_sketch_handle_index_);
        } else {
            point = sketch->GetBezierControlPointWorld(highlighted_sketch_handle_index_);
        }
        glPointSize(14.0f);
        glColor3f(1.0f, 0.9f, 0.0f);
        glBegin(GL_POINTS);
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
        glEnd();
    }

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

void OpenGLViewport::DrawSelectRubberBandRect() {
    const QRect rect = QRect(rect_selection_start_, rect_selection_current_).normalized();
    if (rect.width() <= 0 && rect.height() <= 0) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width(), height(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_INVERT);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(rect.left(), rect.top());
    glVertex2i(rect.right(), rect.top());
    glVertex2i(rect.right(), rect.bottom());
    glVertex2i(rect.left(), rect.bottom());
    glEnd();
    glDisable(GL_COLOR_LOGIC_OP);
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

void OpenGLViewport::DrawSolidDimensions() {
    solid_dimension_hits_.clear();
    if (solid_dimensions_.empty()) {
        return;
    }

    const GLboolean lighting_enabled = glIsEnabled(GL_LIGHTING);
    const GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    const auto vertex = [](const CPoint3d& point) {
        glVertex3d(point.x, point.y, point.z);
    };
    const auto line = [&vertex](const CPoint3d& start, const CPoint3d& end) {
        vertex(start);
        vertex(end);
    };
    const auto add_point = [](const CPoint3d& first, const CPoint3d& second) {
        return CPoint3d(
            first.x + second.x,
            first.y + second.y,
            first.z + second.z);
    };
    const auto subtract_point = [](const CPoint3d& first, const CPoint3d& second) {
        return CPoint3d(
            first.x - second.x,
            first.y - second.y,
            first.z - second.z);
    };
    const auto scale_point = [](const CPoint3d& point, double scale) {
        return CPoint3d(point.x * scale, point.y * scale, point.z * scale);
    };
    const auto normalized_point = [&scale_point](const CPoint3d& point) {
        const double length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
        return length <= 1.0e-12 ? CPoint3d(0.0, 1.0, 0.0) : scale_point(point, 1.0 / length);
    };
    for (const CDimens3D& dimension : solid_dimensions_) {
        if (!dimension.IsVisible()) {
            continue;
        }
        const bool primary = !solid_dimension_primary_parameter_.isEmpty()
            && QString::fromStdString(dimension.GetParameterId())
                   == solid_dimension_primary_parameter_;
        if (primary) {
            glColor3f(0.05f, 0.62f, 1.0f);
            glLineWidth(2.5f);
        } else {
            glColor3f(0.76f, 0.80f, 0.84f);
            glLineWidth(1.2f);
        }
        glBegin(GL_LINES);
        const double measured_length = dimension.GetMeasuredLength();
        const DimensionGeometry3D geometry = dimension.GetGeometry(
            std::max(0.5, measured_length * 0.025));
        line(geometry.source_start, geometry.extension_start);
        line(geometry.source_end, geometry.extension_end);
        line(geometry.dimension_start, geometry.dimension_end);

        const CPoint3d direction = normalized_point(
            subtract_point(geometry.dimension_end, geometry.dimension_start));
        const CPoint3d wing = normalized_point(
            subtract_point(geometry.dimension_start, geometry.source_start));
        const double arrow_length = std::clamp(measured_length * 0.08, 0.6, 6.0);
        const double arrow_width = arrow_length * 0.38;
        const auto draw_arrow = [&](const CPoint3d& tip, const CPoint3d& inward) {
            const CPoint3d base = add_point(tip, scale_point(inward, arrow_length));
            line(tip, add_point(base, scale_point(wing, arrow_width)));
            line(tip, add_point(base, scale_point(wing, -arrow_width)));
        };
        draw_arrow(geometry.dimension_start, direction);
        draw_arrow(geometry.dimension_end, scale_point(direction, -1.0));
        glEnd();
    }
    glLineWidth(1.0f);
    if (depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (lighting_enabled) {
        glEnable(GL_LIGHTING);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);

    const DisplayLengthUnit display_unit = LoadDisplayLengthUnit();
    const QString suffix = DisplayLengthUnitSuffix(display_unit);
    const QColor outline(4, 12, 22, 220);
    const auto project = [this](const CPoint3d& point, QPointF& result) {
        DomPoint screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(point),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                screen)) {
            return false;
        }
        result = QPointF(screen.x, screen.y);
        return true;
    };

    for (const CDimens3D& dimension : solid_dimensions_) {
        if (!dimension.IsVisible()) {
            continue;
        }
        const bool primary = !solid_dimension_primary_parameter_.isEmpty()
            && QString::fromStdString(dimension.GetParameterId())
                   == solid_dimension_primary_parameter_;
        const QColor color = primary
            ? QColor(20, 145, 255)
            : QColor(225, 229, 233);
        const DimensionGeometry3D geometry = dimension.GetGeometry(
            std::max(0.5, dimension.GetMeasuredLength() * 0.025));
        QPointF source_start;
        QPointF source_end;
        QPointF dimension_start;
        QPointF dimension_end;
        QPointF extension_start;
        QPointF extension_end;
        QPointF text_position;
        if (!project(geometry.source_start, source_start)
            || !project(geometry.source_end, source_end)
            || !project(geometry.dimension_start, dimension_start)
            || !project(geometry.dimension_end, dimension_end)
            || !project(geometry.extension_start, extension_start)
            || !project(geometry.extension_end, extension_end)
            || !project(geometry.text_position, text_position)) {
            continue;
        }

        QPointF direction = dimension_end - dimension_start;
        const double screen_length = std::hypot(direction.x(), direction.y());
        if (screen_length < 8.0) {
            continue;
        }
        direction /= screen_length;
        const QPointF perpendicular(-direction.y(), direction.x());

        const auto draw_thick_segment = [&painter](
                                            QPointF start,
                                            QPointF end,
                                            double thickness,
                                            const QColor& segment_color) {
            QPointF segment = end - start;
            const double length = std::hypot(segment.x(), segment.y());
            if (length <= 0.01) {
                return;
            }
            segment /= length;
            const QPointF normal(-segment.y(), segment.x());
            const QPointF half_width = normal * (thickness * 0.5);
            QPolygonF polygon;
            polygon << start + half_width
                    << end + half_width
                    << end - half_width
                    << start - half_width;
            painter.setPen(Qt::NoPen);
            painter.setBrush(segment_color);
            painter.drawPolygon(polygon);
        };
        const auto draw_dimension_lines = [&draw_thick_segment,
                                           &source_start,
                                           &source_end,
                                           &extension_start,
                                           &extension_end,
                                           &dimension_start,
                                           &dimension_end](
                                              double thickness,
                                              const QColor& segment_color) {
            draw_thick_segment(source_start, extension_start, thickness, segment_color);
            draw_thick_segment(source_end, extension_end, thickness, segment_color);
            draw_thick_segment(dimension_start, dimension_end, thickness, segment_color);
        };
        draw_dimension_lines(primary ? 3.5 : 2.5, outline);
        draw_dimension_lines(primary ? 2.0 : 1.25, color);

        const auto arrow_polygon = [&perpendicular](
                                    QPointF tip,
                                    QPointF inward) {
            QPolygonF arrow;
            arrow << tip
                  << tip + inward * 12.0 + perpendicular * 5.0
                  << tip + inward * 12.0 - perpendicular * 5.0;
            return arrow;
        };
        const QPolygonF first_arrow = arrow_polygon(dimension_start, direction);
        const QPolygonF second_arrow = arrow_polygon(dimension_end, -direction);
        painter.setPen(QPen(outline, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(outline);
        painter.drawPolygon(first_arrow);
        painter.drawPolygon(second_arrow);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(first_arrow);
        painter.drawPolygon(second_arrow);

        const QRect arrow_hit_rect = QRectF(dimension_start, dimension_end)
            .normalized()
            .adjusted(-9.0, -9.0, 9.0, 9.0)
            .toAlignedRect();
        solid_dimension_hits_.push_back(
            {arrow_hit_rect,
             QString::fromStdString(dimension.GetParameterId()),
             dimension.GetValue()});

        const QString text = QString("%1%2")
            .arg(MillimetersToDisplay(dimension.GetValue(), display_unit), 0, 'f', 1)
            .arg(suffix);
        const QSize text_size = painter.fontMetrics().size(Qt::TextSingleLine, text);
        const QRect label_rect(
            qRound(text_position.x() - text_size.width() * 0.5 - 4.0),
            qRound(text_position.y() - text_size.height() * 0.5 - 2.0),
            text_size.width() + 8,
            text_size.height() + 4);
        painter.fillRect(label_rect.adjusted(-1, -1, 1, 1), outline);
        painter.fillRect(
            label_rect,
            primary ? QColor(235, 245, 255, 245)
                    : QColor(246, 246, 246, 238));
        painter.setPen(color);
        painter.drawText(label_rect, Qt::AlignCenter, text);
        solid_dimension_hits_.push_back(
            {label_rect,
             QString::fromStdString(dimension.GetParameterId()),
             dimension.GetValue()});
    }
}

void OpenGLViewport::DrawCoordinateAxisLabels() {
    DomPoint x_screen{};
    DomPoint y_screen{};
    DomPoint z_screen{};
    const float lift = 0.02f;
    const float axis_length =
        xy_plane_view_enabled_ ? kDefaultSceneSize : kDefaultGridHalfSize;
    const Vec3 x_label{axis_length, 0.0f, lift};
    const Vec3 y_label{0.0f, axis_length, lift};
    const bool has_x = renderer_.WorldToScreen(x_label, camera_, orthographic_projection_, width(), height(), x_screen);
    const bool has_y = renderer_.WorldToScreen(y_label, camera_, orthographic_projection_, width(), height(), y_screen);
    const bool has_z = renderer_.WorldToScreen(
        {0.0f, 0.0f, axis_length},
        camera_,
        orthographic_projection_,
        width(),
        height(),
        z_screen);

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
