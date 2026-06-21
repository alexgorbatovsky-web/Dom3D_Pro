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

struct Quaternion {
    float w = 1.0f;
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
    Quaternion orientation{0.924396f, 0.198992f, -0.299310f, 0.064417f};
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
    SurfacesAndRaisedMesh,
    Wireframe
};

enum class MeshDisplayMode {
    SurfaceGray,
    SurfaceColored,
    Wire,
    SurfaceMaterial
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

inline Quaternion normalize_quaternion(Quaternion value) {
    const float length = std::sqrt(value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.00001f) {
        return {};
    }
    const float inv_length = 1.0f / length;
    return {value.w * inv_length, value.x * inv_length, value.y * inv_length, value.z * inv_length};
}

inline Quaternion operator*(Quaternion a, Quaternion b) {
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
}

inline Quaternion quaternion_from_axis_angle(Vec3 axis, float angle) {
    const Vec3 unit_axis = normalize(axis);
    const float half_angle = angle * 0.5f;
    const float s = std::sin(half_angle);
    return normalize_quaternion({std::cos(half_angle), unit_axis.x * s, unit_axis.y * s, unit_axis.z * s});
}

inline Vec3 rotate(Quaternion orientation, Vec3 value) {
    const Quaternion q = normalize_quaternion(orientation);
    const Vec3 qv{q.x, q.y, q.z};
    const Vec3 t = cross(qv, value) * 2.0f;
    return value + t * q.w + cross(qv, t);
}

inline Quaternion camera_orientation_from_yaw_pitch(float yaw_degrees, float pitch_degrees) {
    const Quaternion yaw = quaternion_from_axis_angle({0.0f, 1.0f, 0.0f}, deg_to_rad(yaw_degrees));
    const Quaternion pitch = quaternion_from_axis_angle({1.0f, 0.0f, 0.0f}, deg_to_rad(-pitch_degrees));
    return normalize_quaternion(yaw * pitch);
}

inline void orbit_camera(Camera& camera, float yaw_delta_degrees, float pitch_delta_degrees) {
    const Quaternion yaw_delta = quaternion_from_axis_angle({0.0f, 1.0f, 0.0f}, deg_to_rad(yaw_delta_degrees));
    const Vec3 right = rotate(camera.orientation, {1.0f, 0.0f, 0.0f});
    const Quaternion pitch_delta = quaternion_from_axis_angle(right, deg_to_rad(-pitch_delta_degrees));
    camera.orientation = normalize_quaternion(pitch_delta * yaw_delta * camera.orientation);
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
    const Vec3 forward = rotate(camera.orientation, {0.0f, 0.0f, -1.0f});
    return camera.target - forward * camera.distance;
}

inline void camera_basis(const Camera& camera, Vec3& forward, Vec3& right, Vec3& up) {
    forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    up = normalize(rotate(camera.orientation, {0.0f, 1.0f, 0.0f}));
}

inline void camera_basis(const Camera& camera, Vec3& eye, Vec3& forward, Vec3& right, Vec3& up) {
    eye = camera_position(camera);
    camera_basis(camera, forward, right, up);
}
