#pragma once

#include "Common.h"
#include "CAlfaDoc.h"
#include "ObjIO.h"
#include "ProjectIO.h"
#include "Renderer.h"

#include <string>
#include <vector>

class Dom3DApp {
public:
    Dom3DApp();

    int Run(HINSTANCE instance, int show_command);

private:
    static LRESULT CALLBACK StaticWindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

    bool CreateMainWindow(HINSTANCE instance, int show_command);
    HMENU CreateMainMenu() const;
    void SetupToolbar();
    void ExecuteCommand(int command);
    void UpdateMenuChecks();
    void SetTool(ToolMode tool);
    void NewProject();
    bool SaveProject();
    bool OpenProject();
    bool ImportObj();
    bool ExportObj();
    void DeleteSelectedObject();
    void EditSelectedObject();
    void BeginMeshTool();
    void BeginTransformTool(TransformOperation operation);
    void ResetMeshInput();
    void ResetTransformInput();
    void HandleMeshClick(int x, int y);
    void HandleTransformClick(int x, int y, WPARAM w_param);
    void HandleTransformDrag(int x, int y);
    bool SelectMeshCurveAt(int x, int y);
    Vec3 ScreenDragToWorldVector(POINT start, POINT end) const;
    TransformAxis HitTestTransformGizmo(int x, int y) const;
    Vec3 AxisVector(TransformAxis axis) const;
    float DistanceToScreenSegment(POINT point, POINT start, POINT end) const;
    bool ChooseProjectPath(bool save, std::string& path) const;
    bool ChooseObjPath(bool save, std::string& path) const;
    std::string DefaultProjectPath() const;
    bool IsScenePoint(int x, int y) const;
    int ToolbarHitTest(int x, int y) const;
    void Invalidate();

    HWND window_ = nullptr;
    int width_ = 1280;
    int height_ = 760;
    Camera camera_;
    bool dragging_ = false;
    bool dragging_point_ = false;
    bool panning_scene_ = false;
    bool alt_orbiting_ = false;
    bool mesh_has_first_point_ = false;
    bool dragging_transform_ = false;
    TransformAxis active_transform_axis_ = TransformAxis::None;
    TransformAxis hovered_transform_axis_ = TransformAxis::None;
    TransformOperation transform_operation_ = TransformOperation::Move;
    POINT last_mouse_{};
    POINT mesh_first_point_{};
    ToolMode tool_ = ToolMode::Orbit;
    std::string project_path_;
    std::vector<ToolbarButton> toolbar_;
    CAlfaDoc document_;
    ObjIO obj_io_;
    ProjectIO project_io_;
    Renderer renderer_;
};
