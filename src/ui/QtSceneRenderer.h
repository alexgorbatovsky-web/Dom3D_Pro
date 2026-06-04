#pragma once

#include "../CAlfaDoc.h"
#include "../CView3d.h"
#include "../Common.h"

class QtSceneRenderer {
public:
    void Initialize();
    void Render(const CAlfaDoc& document,
                const Camera& camera,
                ToolMode tool,
                TransformAxis highlighted_transform_axis,
                int width,
                int height) const;

    bool ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, CurvePoint& point) const;
    bool WorldToScreen(Vec3 point, const Camera& camera, int width, int height, DomPoint& screen_point) const;

private:
    void DrawTransformGizmo(const CAlfaDoc& document, const Camera& camera, TransformAxis highlighted_axis) const;
    void Perspective(float fov_y, float aspect, float z_near, float z_far) const;
    void LookAt(Vec3 eye, Vec3 center, Vec3 up) const;

    CView3d view3d_;
};
