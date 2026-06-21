#include "Renderer.h"

#include <gl/GL.h>

#include <algorithm>
#include <cstring>
#include <sstream>

namespace {
void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}
}

bool Renderer::Initialize(HWND window) {
    window_ = window;
    dc_ = GetDC(window);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format = ChoosePixelFormat(dc_, &pfd);
    if (!pixel_format || !SetPixelFormat(dc_, pixel_format, &pfd)) {
        return false;
    }

    gl_ = wglCreateContext(dc_);
    if (!gl_ || !wglMakeCurrent(dc_, gl_)) {
        return false;
    }

    InitFont();
    return true;
}

void Renderer::Shutdown() {
    if (font_base_) {
        glDeleteLists(font_base_, 256);
        font_base_ = 0;
    }
    if (gl_) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(gl_);
        gl_ = nullptr;
    }
    if (dc_ && window_) {
        ReleaseDC(window_, dc_);
        dc_ = nullptr;
    }
}

void Renderer::Render(const CAlfaDoc& document,
                      const Camera& camera,
                      ToolMode tool,
                      TransformAxis highlighted_transform_axis,
                      TransformOperation transform_operation,
                      const std::string& project_path,
                      const std::vector<ToolbarButton>& toolbar,
                      int width,
                      int height) {
    DrawScene(document, camera, tool, highlighted_transform_axis, width, height);
    DrawOverlay(document, tool, transform_operation, project_path, toolbar, width, height);
    SwapBuffers(dc_);
}

bool Renderer::ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, CurvePoint& point) const {
    const int scene_width = std::max(1, width - kPanelWidth);
    const int scene_height = std::max(1, height - kToolbarHeight);

    const float ndc_x = (2.0f * static_cast<float>(screen_x - kPanelWidth) / scene_width) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * static_cast<float>(screen_y - kToolbarHeight) / scene_height);
    const float aspect = static_cast<float>(scene_width) / scene_height;
    const float tan_half = std::tan(deg_to_rad(48.0f) * 0.5f);

    Vec3 eye{};
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    camera_basis(camera, eye, forward, right, up);

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

bool Renderer::WorldToScreen(Vec3 point, const Camera& camera, int width, int height, POINT& screen_point) const {
    const int scene_width = std::max(1, width - kPanelWidth);
    const int scene_height = std::max(1, height - kToolbarHeight);

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
    const float aspect = static_cast<float>(scene_width) / scene_height;
    const float ndc_x = camera_x / (camera_z * tan_half * aspect);
    const float ndc_y = camera_y / (camera_z * tan_half);

    screen_point.x = kPanelWidth + static_cast<LONG>((ndc_x + 1.0f) * 0.5f * scene_width);
    screen_point.y = kToolbarHeight + static_cast<LONG>((1.0f - ndc_y) * 0.5f * scene_height);
    return true;
}

void Renderer::InitFont() {
    HFONT font = CreateFontA(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");

    SelectObject(dc_, font);
    font_base_ = glGenLists(256);
    wglUseFontBitmapsA(dc_, 0, 256, font_base_);
    DeleteObject(font);
}

void Renderer::DrawScene(const CAlfaDoc& document, const Camera& camera, ToolMode tool, TransformAxis highlighted_transform_axis, int width, int height) {
    const int scene_width = std::max(1, width - kPanelWidth);
    const int scene_height = std::max(1, height - kToolbarHeight);
    glViewport(kPanelWidth, 0, scene_width, scene_height);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.055f, 0.065f, 0.080f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Perspective(48.0f, static_cast<float>(scene_width) / scene_height, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Vec3 eye{};
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    camera_basis(camera, eye, forward, right, up);
    LookAt(eye, camera.target, up);

    view3d_.Draw(document);
    if (tool == ToolMode::Transform) {
        DrawTransformGizmo(document, camera, highlighted_transform_axis);
    }
}

void Renderer::DrawTransformGizmo(const CAlfaDoc& document, const Camera& camera, TransformAxis highlighted_axis) const {
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

    glPointSize(highlighted_axis == TransformAxis::None ? 9.0f : 12.0f);
    glBegin(GL_POINTS);
    set_color(highlighted_axis == TransformAxis::X ? 1.0f : 0.80f, highlighted_axis == TransformAxis::X ? 0.34f : 0.10f, highlighted_axis == TransformAxis::X ? 0.28f : 0.08f);
    glVertex3f(x_end.x, x_end.y, x_end.z);
    set_color(highlighted_axis == TransformAxis::Y ? 0.34f : 0.10f, highlighted_axis == TransformAxis::Y ? 1.0f : 0.80f, highlighted_axis == TransformAxis::Y ? 0.42f : 0.12f);
    glVertex3f(y_end.x, y_end.y, y_end.z);
    set_color(highlighted_axis == TransformAxis::Z ? 0.34f : 0.10f, highlighted_axis == TransformAxis::Z ? 0.56f : 0.22f, highlighted_axis == TransformAxis::Z ? 1.0f : 0.86f);
    glVertex3f(z_end.x, z_end.y, z_end.z);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawOverlay(const CAlfaDoc& document,
                           ToolMode tool,
                           TransformOperation transform_operation,
                           const std::string& project_path,
                           const std::vector<ToolbarButton>& toolbar,
                           int width,
                           int height) {
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    DrawToolbar(toolbar, tool, transform_operation, width);

    DrawRect(0, 0, static_cast<float>(kPanelWidth), static_cast<float>(height), 0.09f, 0.105f, 0.13f);
    DrawRect(static_cast<float>(kPanelWidth), 0, 1, static_cast<float>(height), 0.22f, 0.27f, 0.34f);

    DrawText(24, 42, "Dom3D Pro", 0.91f, 0.94f, 0.98f);
    DrawText(24, 72, "C++ class architecture", 0.53f, 0.62f, 0.72f);

    DrawText(24, 124, "Tool", 0.91f, 0.94f, 0.98f);
    const char* tool_text = "Orbit camera";
    if (tool == ToolMode::DrawCurve) {
        tool_text = "Draw curve";
    } else if (tool == ToolMode::Select) {
        tool_text = "Select object";
    } else if (tool == ToolMode::Mesh) {
        tool_text = "Mesh";
    } else if (tool == ToolMode::Transform) {
        if (transform_operation == TransformOperation::Move) {
            tool_text = "Transform: move";
        } else if (transform_operation == TransformOperation::Rotate) {
            tool_text = "Transform: rotate";
        } else {
            tool_text = "Transform: scale";
        }
    }
    DrawText(24, 154, tool_text, 0.72f, 0.82f, 0.92f);

    DrawText(24, 210, "Curve", 0.91f, 0.94f, 0.98f);
    std::ostringstream point_count;
    point_count << "Points: " << document.GetTotalPointCount();
    DrawText(24, 242, point_count.str().c_str(), 0.62f, 0.70f, 0.78f);
    std::ostringstream selected_text;
    selected_text << "Selected: " << document.GetSelectedObjectCount();
    DrawText(24, 268, document.HasSelection() ? selected_text.str().c_str() : "Selected: none", 0.62f, 0.70f, 0.78f);

    DrawText(24, 292, "2D View", 0.91f, 0.94f, 0.98f);
    view2d_.DrawPreview(document, 24.0f, 314.0f, 232.0f, 150.0f);

    DrawText(24, 500, "Project", 0.91f, 0.94f, 0.98f);
    DrawText(24, 532, project_path.empty() ? "Unsaved" : "Saved file selected", 0.62f, 0.70f, 0.78f);

    DrawText(24, static_cast<float>(height - 126), "Curve mode: click floor", 0.50f, 0.58f, 0.67f);
    DrawText(24, static_cast<float>(height - 98), "Mesh: curve, then 2 clicks", 0.50f, 0.58f, 0.67f);
    DrawText(24, static_cast<float>(height - 70), "Transform: drag gizmo axis", 0.50f, 0.58f, 0.67f);
    DrawText(24, static_cast<float>(height - 42), "Wheel: zoom", 0.50f, 0.58f, 0.67f);
}

void Renderer::DrawToolbar(const std::vector<ToolbarButton>& toolbar, ToolMode tool, TransformOperation transform_operation, int width) const {
    DrawRect(static_cast<float>(kPanelWidth), 0, static_cast<float>(width - kPanelWidth), static_cast<float>(kToolbarHeight), 0.075f, 0.088f, 0.110f);

    for (const ToolbarButton& button : toolbar) {
        const bool active = (button.command == ID_TOOL_ORBIT && tool == ToolMode::Orbit)
            || (button.command == ID_TOOL_CURVE && tool == ToolMode::DrawCurve)
            || (button.command == ID_TOOL_SELECT && tool == ToolMode::Select)
            || (button.command == ID_TOOL_MESH && tool == ToolMode::Mesh)
            || (button.command == ID_TOOL_TRANSFORM && tool == ToolMode::Transform)
            || (button.command == ID_TRANSFORM_MOVE && tool == ToolMode::Transform && transform_operation == TransformOperation::Move)
            || (button.command == ID_TRANSFORM_ROTATE && tool == ToolMode::Transform && transform_operation == TransformOperation::Rotate)
            || (button.command == ID_TRANSFORM_SCALE && tool == ToolMode::Transform && transform_operation == TransformOperation::Scale);
        const float x = static_cast<float>(kPanelWidth + button.rect.left);
        const float y = static_cast<float>(button.rect.top);
        const float w = static_cast<float>(button.rect.right - button.rect.left);
        const float h = static_cast<float>(button.rect.bottom - button.rect.top);

        DrawRect(x, y, w, h, active ? 0.20f : 0.13f, active ? 0.28f : 0.16f, active ? 0.35f : 0.20f);
        DrawRectLine(x, y, w, h, active ? 0.50f : 0.24f, active ? 0.68f : 0.30f, active ? 0.84f : 0.38f);
        DrawText(x + 16.0f, y + 24.0f, button.label, active ? 0.94f : 0.68f, active ? 0.97f : 0.76f, active ? 1.0f : 0.84f);
    }
}

void Renderer::DrawBox(float x, float y, float z, float w, float h, float d, float r, float g, float b) const {
    const float x2 = x + w;
    const float y2 = y + h;
    const float z2 = z + d;

    glBegin(GL_QUADS);
    set_color(r, g, b);
    glVertex3f(x, y, z);
    glVertex3f(x2, y, z);
    glVertex3f(x2, y2, z);
    glVertex3f(x, y2, z);

    set_color(r * 0.85f, g * 0.85f, b * 0.85f);
    glVertex3f(x2, y, z);
    glVertex3f(x2, y, z2);
    glVertex3f(x2, y2, z2);
    glVertex3f(x2, y2, z);

    set_color(r * 0.75f, g * 0.75f, b * 0.75f);
    glVertex3f(x, y, z2);
    glVertex3f(x, y, z);
    glVertex3f(x, y2, z);
    glVertex3f(x, y2, z2);

    set_color(r * 0.92f, g * 0.92f, b * 0.92f);
    glVertex3f(x, y2, z);
    glVertex3f(x2, y2, z);
    glVertex3f(x2, y2, z2);
    glVertex3f(x, y2, z2);
    glEnd();
}

void Renderer::DrawRect(float x, float y, float w, float h, float r, float g, float b) const {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void Renderer::DrawRectLine(float x, float y, float w, float h, float r, float g, float b) const {
    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void Renderer::DrawText(float x, float y, const char* text, float r, float g, float b) const {
    if (!font_base_ || !text) {
        return;
    }

    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(font_base_);
    glCallLists(static_cast<GLsizei>(std::strlen(text)), GL_UNSIGNED_BYTE, text);
    glPopAttrib();
}

void Renderer::Perspective(float fov_y, float aspect, float z_near, float z_far) const {
    const float top = std::tan(deg_to_rad(fov_y) * 0.5f) * z_near;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, z_near, z_far);
}

void Renderer::LookAt(Vec3 eye, Vec3 center, Vec3 up) const {
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
