#pragma once

#include <cmath>

constexpr int kPanelWidth = 280;
constexpr int kToolbarHeight = 54;
constexpr float kPi = 3.14159265358979323846f;

constexpr int ID_FILE_NEW = 1001;
constexpr int ID_FILE_SAVE = 1002;
constexpr int ID_FILE_OPEN = 1003;
constexpr int ID_FILE_EXIT = 1004;
constexpr int ID_FILE_IMPORT_OBJ = 1005;
constexpr int ID_FILE_EXPORT_OBJ = 1006;
constexpr int ID_TOOL_ORBIT = 1101;
constexpr int ID_TOOL_CURVE = 1102;
constexpr int ID_TOOL_SELECT = 1103;
constexpr int ID_TOOL_MESH = 1104;
constexpr int ID_TOOL_TRANSFORM = 1105;
constexpr int ID_TRANSFORM_MOVE = 1106;
constexpr int ID_TRANSFORM_ROTATE = 1107;
constexpr int ID_TRANSFORM_SCALE = 1108;
constexpr int ID_CURVE_CLEAR = 1201;
constexpr int ID_OBJECT_DELETE = 1301;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

using CVector3d = Vec3;

struct CurvePoint {
    float x = 0.0f;
    float z = 0.0f;
};

struct UV {
    float u = 0.0f;
    float v = 0.0f;
};

struct Color {
    float r = 0.98f;
    float g = 0.77f;
    float b = 0.30f;
};

struct Camera {
    Vec3 target{0.0f, 1.2f, 0.0f};
    float yaw = -35.0f;
    float pitch = 24.0f;
    float distance = 15.0f;
};

struct DomRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct DomPoint {
    int x = 0;
    int y = 0;
};

using NativeWindowHandle = void*;

struct ToolbarButton {
    DomRect rect{};
    int command = 0;
    const char* label = "";
};

enum class ToolMode {
    Orbit,
    DrawCurve,
    Select,
    Mesh,
    Transform,
    Boolean,
    FaceExtrude,
    DraftFace,
    ThickSolid,
    DrawBSpline,
    EditPoint,
    SketchRectangle,
    SketchFillet
};

enum class BooleanOperation {
    Union,
    Cut,
    Common
};

enum class SelectionAction {
    Replace,
    Add,
    Remove
};

enum class SelectionMode {
    Object,
    Face,
    Edge,
    Point
};

enum class TransformAxis {
    None,
    X,
    Y,
    Z,
    ScreenPlane,
    UniformScale
};

enum class TransformOperation {
    Move,
    Rotate,
    Scale
};

enum class SolidDisplayMode {
    SurfacesAndEdges,
    MeshOnly,
    SurfacesAndRaisedMesh
};

enum class MeshDisplayMode {
    SurfaceGray,
    SurfaceColored,
    Wire
};

enum class OrbitMode {
    CAD,
    Architectural
};

inline float deg_to_rad(float value) {
    return value * kPi / 180.0f;
}

inline Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(Vec3 a, float value) {
    return {a.x * value, a.y * value, a.z * value};
}

inline Vec3 operator/(Vec3 a, float value) {
    return {a.x / value, a.y / value, a.z / value};
}

inline float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vec3 normalize(Vec3 value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.00001f) {
        return {};
    }
    return value * (1.0f / length);
}

inline Vec3 rotate_around_axis(Vec3 value, Vec3 axis, float angle) {
    const Vec3 unit_axis = normalize(axis);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return value * c + cross(unit_axis, value) * s + unit_axis * (dot(unit_axis, value) * (1.0f - c));
}

inline Vec3 scale_along_axis(Vec3 value, Vec3 axis, float factor) {
    const Vec3 unit_axis = normalize(axis);
    const float component = dot(value, unit_axis);
    return value + unit_axis * (component * (factor - 1.0f));
}

inline Vec3 scale_uniform(Vec3 value, float factor) {
    return value * factor;
}

inline Vec3 camera_position(const Camera& camera) {
    const float yaw = deg_to_rad(camera.yaw);
    const float pitch = deg_to_rad(camera.pitch);
    const Vec3 offset{
        std::cos(pitch) * std::sin(yaw) * camera.distance,
        std::sin(pitch) * camera.distance,
        std::cos(pitch) * std::cos(yaw) * camera.distance
    };

    return camera.target + offset;
}
