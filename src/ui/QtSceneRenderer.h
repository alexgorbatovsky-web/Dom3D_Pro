#pragma once

#include "../CAlfaDoc.h"
#include "../CView3d.h"
#include "../Common.h"

class QtSceneRenderer {
public:
    void Initialize();
    void Render(const CAlfaDoc& document,
                const Camera& camera,
                bool orthographic,
                bool show_coordinate_axes,
                ToolMode tool,
                TransformOperation transform_operation,
                TransformAxis highlighted_transform_axis,
                bool highlighted_draft_face_gizmo,
                int width,
                int height) const;

    bool ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, bool orthographic, CurvePoint& point) const;
    bool WorldToScreen(Vec3 point, const Camera& camera, bool orthographic, int width, int height, DomPoint& screen_point) const;

private:
    void CalculateClipPlanes(const CAlfaDoc& document, const Camera& camera, float& z_near, float& z_far) const;
    void DrawCoordinateAxes() const;
    void DrawTransformGizmo(const CAlfaDoc& document, const Camera& camera, TransformOperation operation, TransformAxis highlighted_axis) const;
    void Perspective(float fov_y, float aspect, float z_near, float z_far) const;
    void Orthographic(const Camera& camera, float aspect, float z_near, float z_far) const;
    void LookAt(Vec3 eye, Vec3 center, Vec3 up) const;

    CView3d view3d_;
};
