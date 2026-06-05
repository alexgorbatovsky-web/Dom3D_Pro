#include "QtSceneRenderer.h"

#include "../OpenGLCompat.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kGridHalfSize = 12.0f;

void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}

void camera_basis(const Camera& camera, Vec3& eye, Vec3& forward, Vec3& right, Vec3& up) {
    eye = camera_position(camera);
    forward = normalize(camera.target - eye);
    right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    if (std::fabs(right.x) <= 0.00001f && std::fabs(right.y) <= 0.00001f && std::fabs(right.z) <= 0.00001f) {
        right = normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    }
    up = normalize(cross(right, forward));
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
                             ToolMode tool,
                             TransformAxis highlighted_transform_axis,
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
    LookAt(camera_position(camera), camera.target, {0.0f, 1.0f, 0.0f});

    view3d_.Draw(document);
    if (show_coordinate_axes) {
        DrawCoordinateAxes();
    }
    if (tool == ToolMode::Transform) {
        DrawTransformGizmo(document, camera, highlighted_transform_axis);
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
    camera_basis(camera, eye, forward, right, up);

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
    camera_basis(camera, eye, forward, right, up);
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
    const Vec3 eye = camera_position(camera);
    const Vec3 forward = normalize(camera.target - eye);

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

void QtSceneRenderer::DrawCoordinateAxes() const {
    const float length = kGridHalfSize;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
    set_color(1.0f, 0.05f, 0.04f, 0.95f);
    glVertex3f(0.0f, 0.02f, 0.0f);
    glVertex3f(length, 0.02f, 0.0f);

    set_color(0.10f, 1.0f, 0.12f, 0.95f);
    glVertex3f(0.0f, 0.02f, 0.0f);
    glVertex3f(0.0f, 0.02f, length);

    set_color(0.08f, 0.22f, 1.0f, 0.95f);
    glVertex3f(0.0f, 0.02f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);
    glEnd();
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
}

void QtSceneRenderer::DrawTransformGizmo(const CAlfaDoc& document, const Camera& camera, TransformAxis highlighted_axis) const {
    Vec3 center{};
    if (!document.GetSelectionCenter(center)) {
        return;
    }

    const float size = std::max(0.8f, camera.distance * 0.10f);
    const Vec3 x_end = center + Vec3{size, 0.0f, 0.0f};
    const Vec3 y_end = center + Vec3{0.0f, size, 0.0f};
    const Vec3 z_end = center + Vec3{0.0f, 0.0f, size};

    glDisable(GL_DEPTH_TEST);
    glLineWidth(5.0f);
    glBegin(GL_LINES);
    set_color(highlighted_axis == TransformAxis::X ? 1.0f : 0.80f, highlighted_axis == TransformAxis::X ? 0.34f : 0.10f, highlighted_axis == TransformAxis::X ? 0.28f : 0.08f);
    glVertex3f(center.x, center.y, center.z);
    glVertex3f(x_end.x, x_end.y, x_end.z);
    set_color(highlighted_axis == TransformAxis::Y ? 0.34f : 0.10f, highlighted_axis == TransformAxis::Y ? 1.0f : 0.80f, highlighted_axis == TransformAxis::Y ? 0.42f : 0.12f);
    glVertex3f(center.x, center.y, center.z);
    glVertex3f(y_end.x, y_end.y, y_end.z);
    set_color(highlighted_axis == TransformAxis::Z ? 0.34f : 0.10f, highlighted_axis == TransformAxis::Z ? 0.56f : 0.22f, highlighted_axis == TransformAxis::Z ? 1.0f : 0.86f);
    glVertex3f(center.x, center.y, center.z);
    glVertex3f(z_end.x, z_end.y, z_end.z);
    glEnd();
    glEnable(GL_DEPTH_TEST);
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
