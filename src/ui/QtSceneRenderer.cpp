#include "QtSceneRenderer.h"

#include "../OpenGLCompat.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kGridHalfSize = 12.0f;

void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}

Vec3 axis_vector(TransformAxis axis) {
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

void set_axis_color(TransformAxis axis, bool highlighted) {
    if (axis == TransformAxis::X) {
        set_color(highlighted ? 1.0f : 0.80f, highlighted ? 0.34f : 0.10f, highlighted ? 0.28f : 0.08f);
    } else if (axis == TransformAxis::Y) {
        set_color(highlighted ? 0.34f : 0.10f, highlighted ? 1.0f : 0.80f, highlighted ? 0.42f : 0.12f);
    } else if (axis == TransformAxis::Z) {
        set_color(highlighted ? 0.34f : 0.10f, highlighted ? 0.56f : 0.22f, highlighted ? 1.0f : 0.86f);
    }
}

void qt_camera_basis(const Camera& camera, Vec3& eye, Vec3& forward, Vec3& right, Vec3& up) {
    eye = camera_position(camera);
    forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    up = normalize(rotate(camera.orientation, {0.0f, 1.0f, 0.0f}));
}

void draw_arrow_head(Vec3 tip, Vec3 axis, Vec3 camera_forward, float size) {
    const Vec3 backward = axis * -1.0f;
    Vec3 side = normalize(cross(axis, camera_forward));
    if (std::fabs(side.x) <= 0.00001f && std::fabs(side.y) <= 0.00001f && std::fabs(side.z) <= 0.00001f) {
        side = normalize(cross(axis, {0.0f, 1.0f, 0.0f}));
    }
    if (std::fabs(side.x) <= 0.00001f && std::fabs(side.y) <= 0.00001f && std::fabs(side.z) <= 0.00001f) {
        side = {1.0f, 0.0f, 0.0f};
    }

    const Vec3 base = tip + backward * size;
    const Vec3 left = base + side * (size * 0.48f);
    const Vec3 right = base - side * (size * 0.48f);
    glVertex3f(tip.x, tip.y, tip.z);
    glVertex3f(left.x, left.y, left.z);
    glVertex3f(tip.x, tip.y, tip.z);
    glVertex3f(right.x, right.y, right.z);
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

void draw_rotate_arc(Vec3 center, Vec3 axis, Vec3 camera_forward, float radius) {
    constexpr int kSegments = 36;
    Vec3 tangent{};
    Vec3 bitangent{};
    rotation_arc_basis(axis, camera_forward, tangent, bitangent);

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= kSegments; ++i) {
        const float angle = static_cast<float>(i) * 3.14159265f / static_cast<float>(kSegments);
        const Vec3 point = center + tangent * (std::cos(angle) * radius) + bitangent * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();
}

void draw_rotate_ring(Vec3 center, Vec3 axis, Vec3 camera_forward, float radius) {
    constexpr int kSegments = 96;
    Vec3 tangent{};
    Vec3 bitangent{};
    rotation_arc_basis(axis, camera_forward, tangent, bitangent);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < kSegments; ++i) {
        const float angle = static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(kSegments);
        const Vec3 point = center + tangent * (std::cos(angle) * radius) + bitangent * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();
}

void draw_center_ring(Vec3 center, Vec3 camera_right, Vec3 camera_up, float radius, bool highlighted) {
    constexpr int kSegments = 48;
    glBegin(GL_LINE_LOOP);
    if (highlighted) {
        set_color(1.0f, 0.08f, 0.06f, 0.98f);
    } else {
        set_color(1.0f, 1.0f, 1.0f, 0.94f);
    }
    for (int i = 0; i < kSegments; ++i) {
        const float angle = static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(kSegments);
        const Vec3 point = center + camera_right * (std::cos(angle) * radius) + camera_up * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();
}

void draw_center_cube(Vec3 center, float half_size, bool highlighted) {
    const float x0 = center.x - half_size;
    const float x1 = center.x + half_size;
    const float y0 = center.y - half_size;
    const float y1 = center.y + half_size;
    const float z0 = center.z - half_size;
    const float z1 = center.z + half_size;

    glBegin(GL_QUADS);
    set_color(highlighted ? 1.00f : 0.72f, highlighted ? 0.30f : 0.78f, highlighted ? 0.26f : 0.84f, 1.0f);
    glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    set_color(highlighted ? 0.78f : 0.48f, highlighted ? 0.12f : 0.56f, highlighted ? 0.10f : 0.64f, 1.0f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0);
    set_color(highlighted ? 0.88f : 0.58f, highlighted ? 0.18f : 0.66f, highlighted ? 0.14f : 0.74f, 1.0f);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    set_color(highlighted ? 0.68f : 0.40f, highlighted ? 0.08f : 0.48f, highlighted ? 0.08f : 0.56f, 1.0f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x0, y0, z1);
    set_color(highlighted ? 0.96f : 0.66f, highlighted ? 0.24f : 0.72f, highlighted ? 0.20f : 0.78f, 1.0f);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    set_color(highlighted ? 0.82f : 0.52f, highlighted ? 0.14f : 0.60f, highlighted ? 0.12f : 0.68f, 1.0f);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glEnd();
}
}

void QtSceneRenderer::Initialize() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
}

void QtSceneRenderer::Render(const CAlfaDoc& document,
                             const Camera& camera,
                             bool orthographic,
                             bool show_coordinate_axes,
                             bool show_floor_grid,
                             bool xy_plane_view,
                             ToolMode tool,
                             TransformOperation transform_operation,
                             TransformAxis highlighted_transform_axis,
                             bool highlighted_draft_face_gizmo,
                             int width,
                             int height) const {
    const int viewport_width = std::max(1, width);
    const int viewport_height = std::max(1, height);
    glViewport(0, 0, viewport_width, viewport_height);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.055f, 0.065f, 0.080f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float z_near = 0.1f;
    float z_far = 100.0f;
    CalculateClipPlanes(document, camera, z_near, z_far);
    const float aspect = static_cast<float>(viewport_width) / viewport_height;
    if (orthographic) {
        Orthographic(camera, aspect, z_near, z_far);
    } else {
        Perspective(48.0f, aspect, z_near, z_far);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Vec3 view_eye{};
    Vec3 view_forward{};
    Vec3 view_right{};
    Vec3 view_up{};
    qt_camera_basis(camera, view_eye, view_forward, view_right, view_up);
    LookAt(view_eye, camera.target, view_up);

    view3d_.Draw(document, xy_plane_view, show_floor_grid);
    if (show_coordinate_axes) {
        DrawCoordinateAxes(xy_plane_view);
    }
    if (tool == ToolMode::Transform) {
        DrawTransformGizmo(document, camera, transform_operation, highlighted_transform_axis);
    } else if (tool == ToolMode::FaceExtrude) {
        Vec3 face_center{};
        Vec3 face_normal{};
        if (document.GetSelectedSolidFaceCenterAndNormal(face_center, face_normal)) {
            const float size = std::max(0.8f, camera.distance * 0.10f);
            Vec3 eye{};
            Vec3 camera_forward{};
            Vec3 camera_right{};
            Vec3 camera_up{};
            qt_camera_basis(camera, eye, camera_forward, camera_right, camera_up);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
            glLineWidth(5.0f);
            glBegin(GL_LINES);
            set_color(1.0f, 0.60f, 0.10f, 0.98f);
            glVertex3f(face_center.x, face_center.y, face_center.z);
            const Vec3 end = face_center + face_normal * size;
            glVertex3f(end.x, end.y, end.z);
            glEnd();

            glLineWidth(3.2f);
            glBegin(GL_LINES);
            draw_arrow_head(face_center + face_normal * size, face_normal, camera_forward, size * 0.18f);
            glEnd();
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glLineWidth(1.0f);
        }
    } else if (tool == ToolMode::DraftFace) {
        Vec3 axis_center{};
        Vec3 axis_dir{};
        if (document.GetDraftFaceAxis(axis_center, axis_dir)) {
            const float size = std::max(0.8f, camera.distance * 0.10f);
            Vec3 eye{};
            Vec3 camera_forward{};
            Vec3 camera_right{};
            Vec3 camera_up{};
            qt_camera_basis(camera, eye, camera_forward, camera_right, camera_up);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
            glLineWidth(4.0f);
            set_color(1.0f, 0.92f, 0.20f, 0.98f);
            glBegin(GL_LINES);
            const Vec3 axis_start = axis_center - axis_dir * size;
            const Vec3 axis_end = axis_center + axis_dir * size;
            glVertex3f(axis_start.x, axis_start.y, axis_start.z);
            glVertex3f(axis_end.x, axis_end.y, axis_end.z);
            glEnd();

            glLineWidth(3.2f);
            if (highlighted_draft_face_gizmo) {
                set_color(1.0f, 0.08f, 0.06f, 0.98f);
            } else {
                set_color(1.0f, 0.56f, 0.10f, 0.98f);
            }
            draw_rotate_ring(axis_center, axis_dir, camera_forward, size * 1.26f);
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glLineWidth(1.0f);
        }
    }
}

bool QtSceneRenderer::ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, bool orthographic, CurvePoint& point) const {
    const int viewport_width = std::max(1, width);
    const int viewport_height = std::max(1, height);

    const float ndc_x = (2.0f * static_cast<float>(screen_x) / viewport_width) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * static_cast<float>(screen_y) / viewport_height);
    const float aspect = static_cast<float>(viewport_width) / viewport_height;
    const float tan_half = std::tan(deg_to_rad(48.0f) * 0.5f);

    Vec3 eye{};
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    qt_camera_basis(camera, eye, forward, right, up);

    Vec3 ray_origin = eye;
    Vec3 ray = normalize(forward + right * (ndc_x * aspect * tan_half) + up * (ndc_y * tan_half));
    if (orthographic) {
        const float ortho_half_height = std::max(0.25f, camera.distance * 0.42f);
        const float ortho_half_width = ortho_half_height * aspect;
        ray_origin = camera.target + right * (ndc_x * ortho_half_width) + up * (ndc_y * ortho_half_height);
        ray = forward;
    }
    if (std::fabs(ray.y) < 0.0001f) {
        return false;
    }

    const float t = -ray_origin.y / ray.y;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray * t;
    point = {hit.x, hit.z};
    return true;
}

bool QtSceneRenderer::WorldToScreen(Vec3 point, const Camera& camera, bool orthographic, int width, int height, DomPoint& screen_point) const {
    const int viewport_width = std::max(1, width);
    const int viewport_height = std::max(1, height);

    Vec3 eye{};
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    qt_camera_basis(camera, eye, forward, right, up);
    const Vec3 local = point - eye;

    const float camera_x = dot(local, right);
    const float camera_y = dot(local, up);
    const float camera_z = dot(local, forward);
    if (camera_z <= 0.0001f) {
        return false;
    }

    const float tan_half = std::tan(deg_to_rad(48.0f) * 0.5f);
    const float aspect = static_cast<float>(viewport_width) / viewport_height;
    float ndc_x = camera_x / (camera_z * tan_half * aspect);
    float ndc_y = camera_y / (camera_z * tan_half);
    if (orthographic) {
        const float ortho_half_height = std::max(0.25f, camera.distance * 0.42f);
        const float ortho_half_width = ortho_half_height * aspect;
        ndc_x = camera_x / ortho_half_width;
        ndc_y = camera_y / ortho_half_height;
    }

    screen_point.x = static_cast<int>((ndc_x + 1.0f) * 0.5f * viewport_width);
    screen_point.y = static_cast<int>((1.0f - ndc_y) * 0.5f * viewport_height);
    return true;
}

void QtSceneRenderer::CalculateClipPlanes(const CAlfaDoc& document, const Camera& camera, float& z_near, float& z_far) const {
    Vec3 eye{};
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    qt_camera_basis(camera, eye, forward, right, up);

    float min_depth = camera.distance;
    float max_depth = camera.distance;
    bool has_scene_bounds = false;

    for (const auto& object : document.GetObjects()) {
        if (!object || !object->IsVisible()) {
            continue;
        }

        Vec3 min_point{};
        Vec3 max_point{};
        if (!object->GetBounds(min_point, max_point)) {
            continue;
        }

        has_scene_bounds = true;
        const Vec3 corners[] = {
            {min_point.x, min_point.y, min_point.z},
            {max_point.x, min_point.y, min_point.z},
            {min_point.x, max_point.y, min_point.z},
            {max_point.x, max_point.y, min_point.z},
            {min_point.x, min_point.y, max_point.z},
            {max_point.x, min_point.y, max_point.z},
            {min_point.x, max_point.y, max_point.z},
            {max_point.x, max_point.y, max_point.z}
        };

        for (const Vec3& corner : corners) {
            const float depth = dot(corner - eye, forward);
            min_depth = std::min(min_depth, depth);
            max_depth = std::max(max_depth, depth);
        }
    }

    const float scene_scale = std::max(1.0f, camera.distance);
    if (!has_scene_bounds) {
        z_near = std::max(0.02f, camera.distance * 0.01f);
        z_far = std::max(100.0f, camera.distance * 8.0f);
        return;
    }

    z_near = std::max(0.02f, std::min(scene_scale * 0.01f, std::max(0.02f, min_depth - scene_scale)));
    z_far = std::max(z_near + 10.0f, max_depth + scene_scale * 2.0f);
}

void QtSceneRenderer::DrawCoordinateAxes(bool) const {
    const float length = kGridHalfSize;
    const float lift = 0.02f;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
    set_color(1.0f, 0.05f, 0.04f, 0.95f);
    glVertex3f(0.0f, 0.0f, lift);
    glVertex3f(length, 0.0f, lift);

    set_color(0.10f, 1.0f, 0.12f, 0.95f);
    glVertex3f(0.0f, 0.0f, lift);
    glVertex3f(0.0f, length, lift);

    set_color(0.08f, 0.22f, 1.0f, 0.95f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, length);
    glEnd();
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
}

void QtSceneRenderer::DrawTransformGizmo(const CAlfaDoc& document, const Camera& camera, TransformOperation operation, TransformAxis highlighted_axis) const {
    Vec3 center{};
    if (!document.GetSelectionCenter(center)) {
        return;
    }

    const float size = std::max(0.8f, camera.distance * 0.10f);
    const float arrow_size = size * 0.18f;
    const TransformAxis axes[] = {TransformAxis::X, TransformAxis::Y, TransformAxis::Z};
    Vec3 eye{};
    Vec3 camera_forward{};
    Vec3 camera_right{};
    Vec3 camera_up{};
    qt_camera_basis(camera, eye, camera_forward, camera_right, camera_up);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glLineWidth(5.0f);
    glBegin(GL_LINES);
    for (TransformAxis axis : axes) {
        const Vec3 direction = axis_vector(axis);
        const Vec3 end = center + direction * size;
        set_axis_color(axis, highlighted_axis == axis);
        glVertex3f(center.x, center.y, center.z);
        glVertex3f(end.x, end.y, end.z);
    }
    glEnd();

    if (operation == TransformOperation::Move) {
        glLineWidth(3.2f);
        glBegin(GL_LINES);
        for (TransformAxis axis : axes) {
            const Vec3 direction = axis_vector(axis);
            set_axis_color(axis, highlighted_axis == axis);
            draw_arrow_head(center + direction * size, direction, camera_forward, arrow_size);
        }
        glEnd();

        glLineWidth(2.8f);
        draw_center_ring(center, camera_right, camera_up, size * 0.14f, highlighted_axis == TransformAxis::ScreenPlane);
    } else if (operation == TransformOperation::Scale) {
        draw_center_cube(center, size * 0.075f, highlighted_axis == TransformAxis::UniformScale);
    } else if (operation == TransformOperation::Rotate) {
        glLineWidth(4.0f);
        for (TransformAxis axis : axes) {
            const Vec3 direction = axis_vector(axis);
            set_axis_color(axis, highlighted_axis == axis);
            draw_rotate_arc(center + direction * size, direction, camera_forward, size * 0.46f);
        }
    }

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
}

void QtSceneRenderer::Perspective(float fov_y, float aspect, float z_near, float z_far) const {
    const float top = std::tan(deg_to_rad(fov_y) * 0.5f) * z_near;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, z_near, z_far);
}

void QtSceneRenderer::Orthographic(const Camera& camera, float aspect, float z_near, float z_far) const {
    const float half_height = std::max(0.25f, camera.distance * 0.42f);
    const float half_width = half_height * aspect;
    glOrtho(-half_width, half_width, -half_height, half_height, z_near, z_far);
}

void QtSceneRenderer::LookAt(Vec3 eye, Vec3 center, Vec3 up) const {
    const Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    if (std::fabs(s.x) <= 0.00001f && std::fabs(s.y) <= 0.00001f && std::fabs(s.z) <= 0.00001f) {
        s = normalize(cross(f, {0.0f, 0.0f, 1.0f}));
    }
    const Vec3 u = normalize(cross(s, f));

    const GLfloat matrix[16] = {
        s.x, u.x, -f.x, 0.0f,
        s.y, u.y, -f.y, 0.0f,
        s.z, u.z, -f.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    glMultMatrixf(matrix);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}
