#include "Dom3DApp.h"

#include <windowsx.h>
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cstring>

Dom3DApp::Dom3DApp() {
    SetupToolbar();
}

int Dom3DApp::Run(HINSTANCE instance, int show_command) {
    if (!CreateMainWindow(instance, show_command)) {
        return 1;
    }

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK Dom3DApp::StaticWindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    Dom3DApp* app = nullptr;
    if (message == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCT*>(l_param);
        app = static_cast<Dom3DApp*>(create_struct->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->window_ = window;
    } else {
        app = reinterpret_cast<Dom3DApp*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }

    if (app) {
        return app->WindowProc(window, message, w_param, l_param);
    }

    return DefWindowProc(window, message, w_param, l_param);
}

LRESULT Dom3DApp::WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE:
        if (!renderer_.Initialize(window)) {
            MessageBoxA(window, "Could not initialize OpenGL.", "Dom3D Pro", MB_ICONERROR);
            return -1;
        }
        SetTimer(window, 1, 16, nullptr);
        UpdateMenuChecks();
        return 0;

    case WM_COMMAND:
        ExecuteCommand(LOWORD(w_param));
        return 0;

    case WM_SIZE:
        width_ = LOWORD(l_param);
        height_ = HIWORD(l_param);
        return 0;

    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(l_param);
        const int y = GET_Y_LPARAM(l_param);
        const int toolbar_command = ToolbarHitTest(x, y);
        if (toolbar_command) {
            ExecuteCommand(toolbar_command);
            return 0;
        }

        if ((GetKeyState(VK_MENU) & 0x8000) != 0 && IsScenePoint(x, y)) {
            alt_orbiting_ = true;
            dragging_ = false;
            dragging_point_ = false;
            dragging_transform_ = false;
            active_transform_axis_ = TransformAxis::None;
            last_mouse_.x = x;
            last_mouse_.y = y;
            SetCapture(window);
            return 0;
        }

        if (tool_ == ToolMode::Mesh && IsScenePoint(x, y)) {
            HandleMeshClick(x, y);
            return 0;
        }

        if (tool_ == ToolMode::Transform && IsScenePoint(x, y)) {
            HandleTransformClick(x, y, w_param);
            return 0;
        }

        if (tool_ == ToolMode::DrawCurve && IsScenePoint(x, y)) {
            CurvePoint point{};
            if (renderer_.ScreenToFloor(x, y, width_, height_, camera_, point)) {
                document_.AddCurvePoint(point);
                Invalidate();
            }
            return 0;
        }

        if (tool_ == ToolMode::Select && IsScenePoint(x, y)) {
            CurvePoint point{};
            if (renderer_.ScreenToFloor(x, y, width_, height_, camera_, point)) {
                if ((w_param & MK_CONTROL) != 0) {
                    document_.ToggleObjectSelectionAt(point, 0.35f);
                    document_.ClearPointSelection();
                } else if (!document_.SelectPointAt(point, 0.28f)) {
                    document_.SelectObjectAt(point, 0.35f);
                    document_.SelectPointAt(point, 0.28f);
                }
                dragging_point_ = document_.HasSelectedPoint();
                if (dragging_point_) {
                    SetCapture(window);
                }
                Invalidate();
            }
            return 0;
        }

        if (tool_ == ToolMode::Orbit && IsScenePoint(x, y)) {
            dragging_ = true;
            last_mouse_.x = x;
            last_mouse_.y = y;
            SetCapture(window);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        const int x = GET_X_LPARAM(l_param);
        const int y = GET_Y_LPARAM(l_param);
        if (IsScenePoint(x, y)) {
            CurvePoint point{};
            if (renderer_.ScreenToFloor(x, y, width_, height_, camera_, point)
                && document_.SelectObjectAt(point, 0.35f)) {
                EditSelectedObject();
            }
            Invalidate();
        }
        return 0;
    }

    case WM_MBUTTONDOWN: {
        const int x = GET_X_LPARAM(l_param);
        const int y = GET_Y_LPARAM(l_param);
        if (IsScenePoint(x, y)) {
            panning_scene_ = true;
            last_mouse_.x = x;
            last_mouse_.y = y;
            SetCapture(window);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        alt_orbiting_ = false;
        dragging_ = false;
        dragging_point_ = false;
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        hovered_transform_axis_ = TransformAxis::None;
        ReleaseCapture();
        return 0;

    case WM_MBUTTONUP:
        panning_scene_ = false;
        ReleaseCapture();
        return 0;

    case WM_MOUSEMOVE:
        if (alt_orbiting_ && (w_param & MK_LBUTTON) != 0) {
            const int x = GET_X_LPARAM(l_param);
            const int y = GET_Y_LPARAM(l_param);
            orbit_camera(camera_,
                         -static_cast<float>(x - last_mouse_.x) * 0.35f,
                         static_cast<float>(y - last_mouse_.y) * 0.25f);
            last_mouse_.x = x;
            last_mouse_.y = y;
            Invalidate();
            return 0;
        }

        if (panning_scene_) {
            const int x = GET_X_LPARAM(l_param);
            const int y = GET_Y_LPARAM(l_param);
            const float dx = static_cast<float>(x - last_mouse_.x);
            const float dy = static_cast<float>(y - last_mouse_.y);
            const float scale = camera_.distance * 0.0018f;
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            camera_basis(camera_, forward, right, up);
            right = normalize({right.x, 0.0f, right.z});
            Vec3 forward_ground = normalize({forward.x, 0.0f, forward.z});
            if (dot(right, right) <= 0.00001f) {
                right = {1.0f, 0.0f, 0.0f};
            }
            if (dot(forward_ground, forward_ground) <= 0.00001f) {
                forward_ground = {0.0f, 0.0f, -1.0f};
            }
            camera_.target = camera_.target - right * (dx * scale) + forward_ground * (dy * scale);
            last_mouse_.x = x;
            last_mouse_.y = y;
            Invalidate();
            return 0;
        }

        if (dragging_transform_ && tool_ == ToolMode::Transform) {
            const int x = GET_X_LPARAM(l_param);
            const int y = GET_Y_LPARAM(l_param);
            HandleTransformDrag(x, y);
            return 0;
        }

        if (tool_ == ToolMode::Transform && IsScenePoint(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param))) {
            const TransformAxis hovered_axis = HitTestTransformGizmo(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            if (hovered_axis != hovered_transform_axis_) {
                hovered_transform_axis_ = hovered_axis;
                Invalidate();
            }
            return 0;
        }

        if (dragging_point_ && tool_ == ToolMode::Select) {
            const int x = GET_X_LPARAM(l_param);
            const int y = GET_Y_LPARAM(l_param);
            CurvePoint point{};
            if (IsScenePoint(x, y) && renderer_.ScreenToFloor(x, y, width_, height_, camera_, point)) {
                document_.MoveSelectedPoint(point);
                Invalidate();
            }
            return 0;
        }

        if (dragging_ && tool_ == ToolMode::Orbit) {
            const int x = GET_X_LPARAM(l_param);
            const int y = GET_Y_LPARAM(l_param);
            orbit_camera(camera_,
                         -static_cast<float>(x - last_mouse_.x) * 0.35f,
                         static_cast<float>(y - last_mouse_.y) * 0.25f);
            last_mouse_.x = x;
            last_mouse_.y = y;
            Invalidate();
        }
        return 0;

    case WM_KEYDOWN:
        if (w_param == VK_ESCAPE && (tool_ == ToolMode::DrawCurve || tool_ == ToolMode::Mesh || tool_ == ToolMode::Transform)) {
            SetTool(ToolMode::Orbit);
            return 0;
        }
        if (w_param == VK_DELETE) {
            DeleteSelectedObject();
            return 0;
        }
        return DefWindowProc(window, message, w_param, l_param);

    case WM_MOUSEWHEEL:
        camera_.distance -= static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / 120.0f;
        camera_.distance = std::clamp(camera_.distance, 6.0f, 32.0f);
        Invalidate();
        return 0;

    case WM_TIMER:
        Invalidate();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(window, &ps);
        renderer_.Render(document_, camera_, tool_, hovered_transform_axis_, transform_operation_, project_path_, toolbar_, width_, height_);
        EndPaint(window, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(window, 1);
        renderer_.Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(window, message, w_param, l_param);
    }
}

bool Dom3DApp::CreateMainWindow(HINSTANCE instance, int show_command) {
    WNDCLASSA wc{};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = StaticWindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = "Dom3DProWindow";

    if (!RegisterClassA(&wc)) {
        MessageBoxA(nullptr, "Could not register window class.", "Dom3D Pro", MB_ICONERROR);
        return false;
    }

    window_ = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Dom3D Pro - OpenGL Class Prototype",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width_,
        height_,
        nullptr,
        CreateMainMenu(),
        instance,
        this);

    if (!window_) {
        MessageBoxA(nullptr, "Could not create window.", "Dom3D Pro", MB_ICONERROR);
        return false;
    }

    ShowWindow(window_, show_command);
    UpdateWindow(window_);
    return true;
}

HMENU Dom3DApp::CreateMainMenu() const {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU tools = CreatePopupMenu();
    HMENU curve = CreatePopupMenu();

    AppendMenuA(file, MF_STRING, ID_FILE_NEW, "&New");
    AppendMenuA(file, MF_STRING, ID_FILE_OPEN, "&Open...");
    AppendMenuA(file, MF_STRING, ID_FILE_SAVE, "&Save...");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, ID_FILE_IMPORT_OBJ, "Import &OBJ...");
    AppendMenuA(file, MF_STRING, ID_FILE_EXPORT_OBJ, "Export O&BJ...");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, ID_FILE_EXIT, "E&xit");

    AppendMenuA(tools, MF_STRING, ID_TOOL_ORBIT, "&Orbit camera");
    AppendMenuA(tools, MF_STRING, ID_TOOL_SELECT, "&Select object");
    AppendMenuA(tools, MF_STRING, ID_TOOL_CURVE, "&Draw curve");
    AppendMenuA(tools, MF_STRING, ID_TOOL_MESH, "&Mesh");
    AppendMenuA(tools, MF_STRING, ID_TOOL_TRANSFORM, "&Transform");
    AppendMenuA(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(tools, MF_STRING, ID_TRANSFORM_MOVE, "Transform &Move");
    AppendMenuA(tools, MF_STRING, ID_TRANSFORM_ROTATE, "Transform &Rotate");
    AppendMenuA(tools, MF_STRING, ID_TRANSFORM_SCALE, "Transform &Scale");

    AppendMenuA(curve, MF_STRING, ID_CURVE_CLEAR, "&Clear curve");
    AppendMenuA(curve, MF_STRING, ID_OBJECT_DELETE, "&Delete selected");

    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), "&File");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), "&Tools");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(curve), "&Curve");
    return menu;
}

void Dom3DApp::SetupToolbar() {
    toolbar_ = {
        {{16, 10, 90, 44}, ID_TOOL_ORBIT, "Orbit"},
        {{102, 10, 194, 44}, ID_TOOL_SELECT, "Select"},
        {{206, 10, 296, 44}, ID_TOOL_CURVE, "Curve"},
        {{312, 10, 394, 44}, ID_TOOL_MESH, "Mesh"},
        {{410, 10, 520, 44}, ID_TOOL_TRANSFORM, "Transform"},
        {{536, 10, 606, 44}, ID_TRANSFORM_MOVE, "Move"},
        {{622, 10, 704, 44}, ID_TRANSFORM_ROTATE, "Rotate"},
        {{720, 10, 794, 44}, ID_TRANSFORM_SCALE, "Scale"},
        {{810, 10, 890, 44}, ID_FILE_SAVE, "Save"},
        {{906, 10, 986, 44}, ID_FILE_OPEN, "Open"},
        {{1002, 10, 1088, 44}, ID_CURVE_CLEAR, "Clear"}
    };
}

void Dom3DApp::ExecuteCommand(int command) {
    switch (command) {
    case ID_FILE_NEW:
        NewProject();
        break;
    case ID_FILE_SAVE:
        SaveProject();
        break;
    case ID_FILE_OPEN:
        OpenProject();
        break;
    case ID_FILE_IMPORT_OBJ:
        ImportObj();
        break;
    case ID_FILE_EXPORT_OBJ:
        ExportObj();
        break;
    case ID_FILE_EXIT:
        PostMessage(window_, WM_CLOSE, 0, 0);
        break;
    case ID_TOOL_ORBIT:
        SetTool(ToolMode::Orbit);
        break;
    case ID_TOOL_SELECT:
        SetTool(ToolMode::Select);
        break;
    case ID_TOOL_CURVE:
        document_.CreatePolyline();
        SetTool(ToolMode::DrawCurve);
        break;
    case ID_TOOL_MESH:
        BeginMeshTool();
        break;
    case ID_TOOL_TRANSFORM:
        BeginTransformTool(TransformOperation::Move);
        break;
    case ID_TRANSFORM_MOVE:
        BeginTransformTool(TransformOperation::Move);
        break;
    case ID_TRANSFORM_ROTATE:
        BeginTransformTool(TransformOperation::Rotate);
        break;
    case ID_TRANSFORM_SCALE:
        BeginTransformTool(TransformOperation::Scale);
        break;
    case ID_CURVE_CLEAR:
        document_.ClearActivePolyline();
        Invalidate();
        break;
    case ID_OBJECT_DELETE:
        DeleteSelectedObject();
        break;
    default:
        break;
    }
}

void Dom3DApp::UpdateMenuChecks() {
    HMENU menu = GetMenu(window_);
    if (!menu) {
        return;
    }

    CheckMenuItem(menu, ID_TOOL_ORBIT, MF_BYCOMMAND | (tool_ == ToolMode::Orbit ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TOOL_CURVE, MF_BYCOMMAND | (tool_ == ToolMode::DrawCurve ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TOOL_SELECT, MF_BYCOMMAND | (tool_ == ToolMode::Select ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TOOL_MESH, MF_BYCOMMAND | (tool_ == ToolMode::Mesh ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TOOL_TRANSFORM, MF_BYCOMMAND | (tool_ == ToolMode::Transform ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TRANSFORM_MOVE, MF_BYCOMMAND | (tool_ == ToolMode::Transform && transform_operation_ == TransformOperation::Move ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TRANSFORM_ROTATE, MF_BYCOMMAND | (tool_ == ToolMode::Transform && transform_operation_ == TransformOperation::Rotate ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_TRANSFORM_SCALE, MF_BYCOMMAND | (tool_ == ToolMode::Transform && transform_operation_ == TransformOperation::Scale ? MF_CHECKED : MF_UNCHECKED));
}

void Dom3DApp::SetTool(ToolMode tool) {
    tool_ = tool;
    dragging_ = false;
    dragging_point_ = false;
    panning_scene_ = false;
    alt_orbiting_ = false;
    dragging_transform_ = false;
    active_transform_axis_ = TransformAxis::None;
    hovered_transform_axis_ = TransformAxis::None;
    ResetMeshInput();
    ResetTransformInput();
    UpdateMenuChecks();
    Invalidate();
}

void Dom3DApp::NewProject() {
    document_.Clear();
    project_path_.clear();
    Invalidate();
}

bool Dom3DApp::SaveProject() {
    std::string path = project_path_;
    if (!ChooseProjectPath(true, path)) {
        return false;
    }

    std::string error;
    if (!project_io_.Save(path, document_, error)) {
        MessageBoxA(window_, error.c_str(), "Dom3D Pro", MB_ICONERROR);
        return false;
    }

    project_path_ = path;
    Invalidate();
    return true;
}

bool Dom3DApp::OpenProject() {
    std::string path = project_path_;
    if (!ChooseProjectPath(false, path)) {
        return false;
    }

    std::string error;
    if (!project_io_.Load(path, document_, error)) {
        MessageBoxA(window_, error.c_str(), "Dom3D Pro", MB_ICONERROR);
        return false;
    }

    project_path_ = path;
    Invalidate();
    return true;
}

bool Dom3DApp::ImportObj() {
    std::string path;
    if (!ChooseObjPath(false, path)) {
        return false;
    }

    std::unique_ptr<CMesh3D> mesh;
    std::string error;
    if (!obj_io_.Import(path, mesh, error)) {
        MessageBoxA(window_, error.c_str(), "OBJ Import", MB_ICONERROR);
        return false;
    }

    document_.AddMesh(std::move(mesh));
    SetTool(ToolMode::Select);
    Invalidate();
    return true;
}

bool Dom3DApp::ExportObj() {
    std::string path;
    if (!ChooseObjPath(true, path)) {
        return false;
    }

    std::string error;
    if (!obj_io_.Export(path, document_, error)) {
        MessageBoxA(window_, error.c_str(), "OBJ Export", MB_ICONERROR);
        return false;
    }

    return true;
}

void Dom3DApp::DeleteSelectedObject() {
    if (document_.DeleteSelectedPoint() || document_.DeleteSelectedObject()) {
        Invalidate();
    }
}

void Dom3DApp::EditSelectedObject() {
    CAlfaObject* object = document_.GetSelectedObject();
    if (object) {
        object->Edit(window_);
    }
}

void Dom3DApp::BeginMeshTool() {
    SetTool(ToolMode::Mesh);
    if (document_.GetSelectedPolyline()) {
        MessageBoxA(window_, "Click two points in the scene to set mesh direction and distance.", "Mesh", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(window_, "Select a curve, then click two points to set mesh direction and distance.", "Mesh", MB_OK | MB_ICONINFORMATION);
    }
}

void Dom3DApp::BeginTransformTool(TransformOperation operation) {
    transform_operation_ = operation;
    SetTool(ToolMode::Transform);
    if (!document_.HasSelection()) {
        MessageBoxA(window_, "Select one or more objects. Use Ctrl+click to add objects to the selection.", "Transform", MB_OK | MB_ICONINFORMATION);
    }
}

void Dom3DApp::ResetMeshInput() {
    mesh_has_first_point_ = false;
    mesh_first_point_ = {};
}

void Dom3DApp::ResetTransformInput() {
    dragging_transform_ = false;
    active_transform_axis_ = TransformAxis::None;
    hovered_transform_axis_ = TransformAxis::None;
}

void Dom3DApp::HandleMeshClick(int x, int y) {
    if (!document_.GetSelectedPolyline()) {
        if (!SelectMeshCurveAt(x, y)) {
            MessageBoxA(window_, "Please select a curve for mesh creation.", "Mesh", MB_OK | MB_ICONINFORMATION);
            return;
        }
        MessageBoxA(window_, "Curve selected. Click the first point for mesh direction.", "Mesh", MB_OK | MB_ICONINFORMATION);
        Invalidate();
        return;
    }

    if (!mesh_has_first_point_) {
        mesh_first_point_.x = x;
        mesh_first_point_.y = y;
        mesh_has_first_point_ = true;
        MessageBoxA(window_, "Click the second point to define direction and distance.", "Mesh", MB_OK | MB_ICONINFORMATION);
        return;
    }

    POINT second_point{};
    second_point.x = x;
    second_point.y = y;
    const Vec3 vector = ScreenDragToWorldVector(mesh_first_point_, second_point);
    const float dist = std::sqrt(dot(vector, vector));
    if (dist <= 0.00001f || !document_.CreateMeshFromSelectedPolyline(vector, dist)) {
        MessageBoxA(window_, "Could not create mesh from the selected curve.", "Mesh", MB_OK | MB_ICONERROR);
        ResetMeshInput();
        return;
    }

    ResetMeshInput();
    SetTool(ToolMode::Select);
    Invalidate();
}

void Dom3DApp::HandleTransformClick(int x, int y, WPARAM w_param) {
    if (document_.HasSelection()) {
        const TransformAxis axis = HitTestTransformGizmo(x, y);
        if (axis != TransformAxis::None) {
            active_transform_axis_ = axis;
            hovered_transform_axis_ = axis;
            dragging_transform_ = true;
            last_mouse_.x = x;
            last_mouse_.y = y;
            SetCapture(window_);
            return;
        }
    }

    CurvePoint point{};
    if (!renderer_.ScreenToFloor(x, y, width_, height_, camera_, point)) {
        return;
    }

    const bool add_to_selection = (w_param & MK_CONTROL) != 0;
    if (add_to_selection) {
        document_.ToggleObjectSelectionAt(point, 0.35f);
    } else {
        document_.SelectObjectAt(point, 0.35f);
    }
    Invalidate();
}

void Dom3DApp::HandleTransformDrag(int x, int y) {
    if (active_transform_axis_ == TransformAxis::None) {
        return;
    }

    Vec3 center{};
    if (!document_.GetSelectionCenter(center)) {
        ResetTransformInput();
        return;
    }

    const Vec3 axis = AxisVector(active_transform_axis_);
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    POINT center_screen{};
    POINT axis_screen{};
    if (!renderer_.WorldToScreen(center, camera_, width_, height_, center_screen)
        || !renderer_.WorldToScreen(center + axis * gizmo_size, camera_, width_, height_, axis_screen)) {
        return;
    }

    const float axis_dx = static_cast<float>(axis_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(axis_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    if (axis_len_sq <= 0.0001f) {
        return;
    }

    const float mouse_dx = static_cast<float>(x - last_mouse_.x);
    const float mouse_dy = static_cast<float>(y - last_mouse_.y);
    const float pixels_along_axis = (mouse_dx * axis_dx + mouse_dy * axis_dy) / std::sqrt(axis_len_sq);
    const float pixels_per_world = std::sqrt(axis_len_sq) / gizmo_size;
    const float world_delta = pixels_along_axis / pixels_per_world;

    bool transformed = false;
    if (transform_operation_ == TransformOperation::Move) {
        transformed = document_.MoveSelectedObjects(axis * world_delta);
    } else if (transform_operation_ == TransformOperation::Rotate) {
        transformed = document_.RotateSelectedObjects(center, axis, pixels_along_axis * 0.01f);
    } else {
        const float factor = std::clamp(1.0f + pixels_along_axis * 0.01f, 0.05f, 20.0f);
        transformed = document_.ScaleSelectedObjects(center, axis, factor);
    }

    if (std::fabs(pixels_along_axis) > 0.0001f && transformed) {
        last_mouse_.x = x;
        last_mouse_.y = y;
        Invalidate();
    }
}

bool Dom3DApp::SelectMeshCurveAt(int x, int y) {
    CurvePoint point{};
    if (!renderer_.ScreenToFloor(x, y, width_, height_, camera_, point)) {
        return false;
    }

    return document_.SelectPolylineAt(point, 0.35f);
}

Vec3 Dom3DApp::ScreenDragToWorldVector(POINT start, POINT end) const {
    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    camera_basis(camera_, forward, right, up);
    const float scale = camera_.distance * 0.0025f;
    return right * (dx * scale) - up * (dy * scale);
}

TransformAxis Dom3DApp::HitTestTransformGizmo(int x, int y) const {
    Vec3 center{};
    if (!document_.GetSelectionCenter(center)) {
        return TransformAxis::None;
    }

    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    POINT point{};
    point.x = x;
    point.y = y;

    POINT center_screen{};
    if (!renderer_.WorldToScreen(center, camera_, width_, height_, center_screen)) {
        return TransformAxis::None;
    }

    TransformAxis best_axis = TransformAxis::None;
    float best_distance = 12.0f;
    const TransformAxis axes[] = {TransformAxis::X, TransformAxis::Y, TransformAxis::Z};
    for (TransformAxis axis : axes) {
        POINT axis_end{};
        if (!renderer_.WorldToScreen(center + AxisVector(axis) * gizmo_size, camera_, width_, height_, axis_end)) {
            continue;
        }

        const float distance = DistanceToScreenSegment(point, center_screen, axis_end);
        if (distance < best_distance) {
            best_distance = distance;
            best_axis = axis;
        }
    }

    return best_axis;
}

Vec3 Dom3DApp::AxisVector(TransformAxis axis) const {
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

float Dom3DApp::DistanceToScreenSegment(POINT point, POINT start, POINT end) const {
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

bool Dom3DApp::ChooseProjectPath(bool save, std::string& path) const {
    char file_name[MAX_PATH]{};
    const std::string initial = path.empty() ? DefaultProjectPath() : path;
    strncpy_s(file_name, initial.c_str(), MAX_PATH - 1);

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window_;
    ofn.lpstrFilter = "Dom3D Pro Project (*.d3dm)\0*.d3dm\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "d3dm";
    ofn.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    if (!ok) {
        return false;
    }

    path = file_name;
    return true;
}

bool Dom3DApp::ChooseObjPath(bool save, std::string& path) const {
    char file_name[MAX_PATH]{};
    if (!path.empty()) {
        strncpy_s(file_name, path.c_str(), MAX_PATH - 1);
    }

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window_;
    ofn.lpstrFilter = "Wavefront OBJ (*.obj)\0*.obj\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "obj";
    ofn.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    if (!ok) {
        return false;
    }

    path = file_name;
    return true;
}

std::string Dom3DApp::DefaultProjectPath() const {
    return "C:\\My_projects\\Dom3D_Pro\\curve_project.d3dm";
}

bool Dom3DApp::IsScenePoint(int x, int y) const {
    return x >= kPanelWidth && y >= kToolbarHeight && x < width_ && y < height_;
}

int Dom3DApp::ToolbarHitTest(int x, int y) const {
    if (x < kPanelWidth || y > kToolbarHeight) {
        return 0;
    }

    const int local_x = x - kPanelWidth;
    for (const ToolbarButton& button : toolbar_) {
        if (local_x >= button.rect.left && local_x <= button.rect.right
            && y >= button.rect.top && y <= button.rect.bottom) {
            return button.command;
        }
    }

    return 0;
}

void Dom3DApp::Invalidate() {
    if (window_) {
        InvalidateRect(window_, nullptr, FALSE);
    }
}
