#pragma once

#include "CAlfaDoc.h"
#include "CView2d.h"
#include "CView3d.h"
#include "Common.h"

#include <gl/GL.h>

#include <string>
#include <vector>

class Renderer {
public:
    bool Initialize(HWND window);
    void Shutdown();
    void Render(const CAlfaDoc& document,
                const Camera& camera,
                ToolMode tool,
                TransformAxis highlighted_transform_axis,
                TransformOperation transform_operation,
                const std::string& project_path,
                const std::vector<ToolbarButton>& toolbar,
                int width,
                int height);
    bool ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, CurvePoint& point) const;
    bool WorldToScreen(Vec3 point, const Camera& camera, int width, int height, POINT& screen_point) const;

private:
    void InitFont();
    void DrawScene(const CAlfaDoc& document, const Camera& camera, ToolMode tool, TransformAxis highlighted_transform_axis, int width, int height);
    void DrawTransformGizmo(const CAlfaDoc& document, const Camera& camera, TransformAxis highlighted_axis) const;
    void DrawOverlay(const CAlfaDoc& document,
                     ToolMode tool,
                     TransformOperation transform_operation,
                     const std::string& project_path,
                     const std::vector<ToolbarButton>& toolbar,
                     int width,
                     int height);
    void DrawToolbar(const std::vector<ToolbarButton>& toolbar, ToolMode tool, TransformOperation transform_operation, int width) const;
    void DrawBox(float x, float y, float z, float w, float h, float d, float r, float g, float b) const;
    void DrawRect(float x, float y, float w, float h, float r, float g, float b) const;
    void DrawRectLine(float x, float y, float w, float h, float r, float g, float b) const;
    void DrawText(float x, float y, const char* text, float r, float g, float b) const;
    void Perspective(float fov_y, float aspect, float z_near, float z_far) const;
    void LookAt(Vec3 eye, Vec3 center, Vec3 up) const;

    HWND window_ = nullptr;
    HDC dc_ = nullptr;
    HGLRC gl_ = nullptr;
    GLuint font_base_ = 0;
    CView3d view3d_;
    CView2d view2d_;
};
