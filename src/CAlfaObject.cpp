#include "CAlfaObject.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace {
unsigned long pack_color(Color color) {
    const auto channel = [](float value) {
        return static_cast<unsigned long>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (channel(color.r) << 16) | (channel(color.g) << 8) | channel(color.b);
}

Color unpack_color(unsigned long color) {
    return {
        static_cast<float>((color >> 16) & 0xffUL) / 255.0f,
        static_cast<float>((color >> 8) & 0xffUL) / 255.0f,
        static_cast<float>(color & 0xffUL) / 255.0f
    };
}
}

#if defined(_WIN32)
namespace {
constexpr int kMaterialOk = 1;
constexpr int kMaterialCancel = 2;

struct MaterialDialogState {
    Material material{};
    bool accepted = false;
    HWND alpha_edit = nullptr;
    HWND specular_edit = nullptr;
    HWND shininess_edit = nullptr;
};

std::string FloatText(float value) {
    char buffer[32]{};
    sprintf_s(buffer, "%.3f", value);
    return buffer;
}
#endif

float ReadFloat(HWND edit, float fallback) {
    char buffer[64]{};
    GetWindowTextA(edit, buffer, static_cast<int>(sizeof(buffer)));
    char* end = nullptr;
    const float value = std::strtof(buffer, &end);
    if (end == buffer) {
        return fallback;
    }
    return value;
}



LRESULT CALLBACK MaterialDialogProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* state = reinterpret_cast<MaterialDialogState*>(GetWindowLongPtr(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(l_param);
        state = static_cast<MaterialDialogState*>(create->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        CreateWindowExA(0, "STATIC", "Alpha", WS_CHILD | WS_VISIBLE, 18, 18, 90, 22, window, nullptr, nullptr, nullptr);
        state->alpha_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", FloatText(state->material.alpha).c_str(),
                                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 116, 16, 110, 24, window, nullptr, nullptr, nullptr);

        CreateWindowExA(0, "STATIC", "Specular", WS_CHILD | WS_VISIBLE, 18, 52, 90, 22, window, nullptr, nullptr, nullptr);
        state->specular_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", FloatText(state->material.specular).c_str(),
                                               WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 116, 50, 110, 24, window, nullptr, nullptr, nullptr);

        CreateWindowExA(0, "STATIC", "Shininess", WS_CHILD | WS_VISIBLE, 18, 86, 90, 22, window, nullptr, nullptr, nullptr);
        state->shininess_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", FloatText(state->material.shininess).c_str(),
                                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 116, 84, 110, 24, window, nullptr, nullptr, nullptr);

        CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 44, 128, 82, 28,
                        window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMaterialOk)), nullptr, nullptr);
        CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, 142, 128, 82, 28,
                        window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMaterialCancel)), nullptr, nullptr);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(w_param) == kMaterialOk) {
            state->material.alpha = std::clamp(ReadFloat(state->alpha_edit, state->material.alpha), 0.0f, 1.0f);
            state->material.specular = std::clamp(ReadFloat(state->specular_edit, state->material.specular), 0.0f, 1.0f);
            state->material.shininess = std::clamp(ReadFloat(state->shininess_edit, state->material.shininess), 1.0f, 256.0f);
            state->accepted = true;
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(w_param) == kMaterialCancel) {
            DestroyWindow(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    }

    return DefWindowProc(window, message, w_param, l_param);
}

bool EditMaterialNumbers(HWND parent_window, Material& material) {
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(parent_window, GWLP_HINSTANCE));
    const char* class_name = "Dom3DMaterialDialog";

    WNDCLASSA wc{};
    wc.lpfnWndProc = MaterialDialogProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = class_name;
    RegisterClassA(&wc);

    MaterialDialogState state{};
    state.material = material;

    RECT parent_rect{};
    GetWindowRect(parent_window, &parent_rect);
    const int width = 264;
    const int height = 210;
    const int x = parent_rect.left + ((parent_rect.right - parent_rect.left) - width) / 2;
    const int y = parent_rect.top + ((parent_rect.bottom - parent_rect.top) - height) / 2;

    EnableWindow(parent_window, FALSE);
    HWND dialog = CreateWindowExA(WS_EX_DLGMODALFRAME, class_name, "Material",
                                  WS_CAPTION | WS_SYSMENU | WS_POPUP,
                                  x, y, width, height,
                                  parent_window, nullptr, instance, &state);
    if (!dialog) {
        EnableWindow(parent_window, TRUE);
        return false;
    }

    ShowWindow(dialog, SW_SHOW);
    MSG message{};
    while (IsWindow(dialog) && GetMessage(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessage(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

    EnableWindow(parent_window, TRUE);
    SetForegroundWindow(parent_window);

    if (state.accepted) {
        material = state.material;
    }
    return state.accepted;
}
}




CAlfaObject::CAlfaObject()
    : name_("Object") {
    m_col = pack_color(material_.diffuse);
}

CAlfaObject::CAlfaObject(std::string name)
    : name_(std::move(name)) {
    m_col = pack_color(material_.diffuse);
}

void CAlfaObject::Edit(NativeWindowHandle parent_window) {
#if defined(_WIN32)
    auto* hwnd = static_cast<HWND>(parent_window);
    Material material = GetMaterial();
    static COLORREF custom_colors[16]{};
    CHOOSECOLORA choose_color{};
    choose_color.lStructSize = sizeof(choose_color);
    choose_color.hwndOwner = hwnd;
    choose_color.lpCustColors = custom_colors;
    choose_color.rgbResult = RGB(
        static_cast<BYTE>(std::clamp(material.diffuse.r, 0.0f, 1.0f) * 255.0f),
        static_cast<BYTE>(std::clamp(material.diffuse.g, 0.0f, 1.0f) * 255.0f),
        static_cast<BYTE>(std::clamp(material.diffuse.b, 0.0f, 1.0f) * 255.0f));
    choose_color.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorA(&choose_color)) {
        material.diffuse.r = static_cast<float>(GetRValue(choose_color.rgbResult)) / 255.0f;
        material.diffuse.g = static_cast<float>(GetGValue(choose_color.rgbResult)) / 255.0f;
        material.diffuse.b = static_cast<float>(GetBValue(choose_color.rgbResult)) / 255.0f;
    }

    if (EditMaterialNumbers(hwnd, material)) {
        SetMaterial(material);
    }
#else
    (void)parent_window;
#endif
}

void CAlfaObject::Render3d(bool selected, bool, size_t) const {
    Render3d(selected);
}

void CAlfaObject::Mirror(Vec3 plane_point, Vec3 plane_normal) {
    if (dot(plane_normal, plane_normal) <= 0.000001f) {
        return;
    }
    Scale(plane_point, normalize(plane_normal), -1.0f);
}

std::unique_ptr<CAlfaObject> CAlfaObject::Clone() const {
    return nullptr;
}

const std::string& CAlfaObject::GetName() const {
    return name_;
}

void CAlfaObject::SetName(std::string name) {
    name_ = std::move(name);
}

const std::string& CAlfaObject::GetGroupName() const {
    return group_name_;
}

void CAlfaObject::SetGroupName(std::string group_name) {
    group_name_ = std::move(group_name);
}

bool CAlfaObject::IsVisible() const {
    return visible_;
}

void CAlfaObject::SetVisible(bool visible) {
    visible_ = visible;
}

Color CAlfaObject::GetColor() const {
    return unpack_color(m_col);
}

void CAlfaObject::SetColor(Color color) {
    material_.diffuse = color;
    m_col = pack_color(color);
}

Material CAlfaObject::GetMaterial() const {
    Material material = material_;
    material.diffuse = GetColor();
    return material;
}

void CAlfaObject::SetMaterial(Material material) {
    material_ = material;
    material_id_ = material.id;
    m_col = pack_color(material_.diffuse);
}

unsigned long CAlfaObject::GetMaterialId() const {
    return material_id_;
}

void CAlfaObject::SetMaterialId(unsigned long id) {
    material_id_ = id;
}

bool CAlfaObject::IsParametric() const {
    return !parametric_tool_id_.empty();
}

const std::string& CAlfaObject::GetParametricToolId() const {
    return parametric_tool_id_;
}

const std::vector<ParametricParameterValue>& CAlfaObject::GetParametricParameters() const {
    return parametric_parameters_;
}

void CAlfaObject::SetParametricDefinition(std::string tool_id, std::vector<ParametricParameterValue> parameters) {
    parametric_tool_id_ = std::move(tool_id);
    parametric_parameters_ = std::move(parameters);
}

void CAlfaObject::ClearParametricDefinition() {
    parametric_tool_id_.clear();
    parametric_parameters_.clear();
}
