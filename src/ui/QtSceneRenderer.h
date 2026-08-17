#pragma once

#include "../CAlfaDoc.h"
#include "../CView3d.h"
#include "../Common.h"

class QtSceneRenderer {
public:
    void Initialize();
    void SetBackgroundColor(Vec3 color);
    Vec3 GetBackgroundColor() const { return background_color_; }
    void Render(const CAlfaDoc& document,
                const Camera& camera,
                bool orthographic,
                bool show_coordinate_axes,
                bool show_floor_grid,
                bool xy_plane_view,
                float grid_size,
                float grid_step,
                int grid_subdivisions,
                ToolMode tool,
                TransformOperation transform_operation,
                TransformAxis highlighted_transform_axis,
                float transform_dialog_rotation_angle_degrees,
                Vec3 transform_dialog_rotation_axis,
                bool highlighted_draft_face_gizmo,
                int width,
                int height) const;

    bool ScreenToFloor(int screen_x, int screen_y, int width, int height, const Camera& camera, bool orthographic, CurvePoint& point) const;
    bool WorldToScreen(Vec3 point, const Camera& camera, bool orthographic, int width, int height, DomPoint& screen_point) const;

private:
    void CalculateClipPlanes(const CAlfaDoc& document,
                             const Camera& camera,
                             bool orthographic,
                             bool show_floor_grid,
                             float grid_size,
                             float& z_near,
                             float& z_far) const;
    void DrawCoordinateAxes(bool xy_plane_view, float grid_size) const;
    void DrawTransformGizmo(const CAlfaDoc& document,
                            const Camera& camera,
                            TransformOperation operation,
                            TransformAxis highlighted_axis,
                            float rotation_guide_angle_degrees,
                            Vec3 rotation_guide_axis) const;
    void Perspective(float fov_y, float aspect, float z_near, float z_far) const;
    void Orthographic(const Camera& camera, float aspect, float z_near, float z_far) const;
    void LookAt(Vec3 eye, Vec3 center, Vec3 up) const;

    CView3d view3d_;
    Vec3 background_color_{0.055f, 0.065f, 0.080f};
};
