#include "QtSceneRenderer.h"

#include "../OpenGLCompat.h"

#include <algorithm>
#include <cmath>

namespace {
void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}
}

void QtSceneRenderer::Initialize() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void QtSceneRenderer::Render(const CAlfaDoc& document,
                             const Camera& camera,
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
    Perspective(48.0f, static_cast<float>(viewport_width) / viewport_height, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    LookAt(camera_position(camera), camera.target, {0.0f, 1.0f, 0.0f});

    view3d_.Draw(document);
    if (tool == ToolMode::Transform) {
        DrawTransformGizmo(document, camera, highlighted_transform_axis);
    }
}

bool QtSceneRenderer::ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, CurvePoint& point) const {
    const int viewport_width = std::max(1, width);
    const int viewport_height = std::max(1, height);

    const float ndc_x = (2.0f * static_cast<float>(screen_x) / viewport_width) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * static_cast<float>(screen_y) / viewport_height);
    const float aspect = static_cast<float>(viewport_width) / viewport_height;
    const float tan_half = std::tan(deg_to_rad(48.0f) * 0.5f);

    const Vec3 eye = camera_position(camera);
    const Vec3 forward = normalize(camera.target - eye);
    const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    const Vec3 up = cross(right, forward);

    const Vec3 ray = normalize(forward + right * (ndc_x * aspect * tan_half) + up * (ndc_y * tan_half));
    if (std::fabs(ray.y) < 0.0001f) {
        return false;
    }

    const float t = -eye.y / ray.y;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = eye + ray * t;
    point = {hit.x, hit.z};
    return true;
}

bool QtSceneRenderer::WorldToScreen(Vec3 point, const Camera& camera, int width, int height, DomPoint& screen_point) const {
    const int viewport_width = std::max(1, width);
    const int viewport_height = std::max(1, height);

    const Vec3 eye = camera_position(camera);
    const Vec3 forward = normalize(camera.target - eye);
    const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    const Vec3 up = cross(right, forward);
    const Vec3 local = point - eye;

    const float camera_x = dot(local, right);
    const float camera_y = dot(local, up);
    const float camera_z = dot(local, forward);
    if (camera_z <= 0.0001f) {
        return false;
    }

    const float tan_half = std::tan(deg_to_rad(48.0f) * 0.5f);
    const float aspect = static_cast<float>(viewport_width) / viewport_height;
    const float ndc_x = camera_x / (camera_z * tan_half * aspect);
    const float ndc_y = camera_y / (camera_z * tan_half);

    screen_point.x = static_cast<int>((ndc_x + 1.0f) * 0.5f * viewport_width);
    screen_point.y = static_cast<int>((1.0f - ndc_y) * 0.5f * viewport_height);
    return true;
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

void QtSceneRenderer::LookAt(Vec3 eye, Vec3 center, Vec3 up) const {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    const GLfloat matrix[16] = {
        s.x, u.x, -f.x, 0.0f,
        s.y, u.y, -f.y, 0.0f,
        s.z, u.z, -f.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    glMultMatrixf(matrix);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}
