#include "CMesh3D.h"

#include "CPolyline.h"
#include "FillContour.h"
//#include "Line2D.h"
#include "OpenGLCompat.h"
#include "SurfaceUVMapping.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"
#include "CAlfaDoc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
#include <limits>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

void Step(const char* text);

namespace {
constexpr GLint kGlClampToEdge = 0x812F;
QString resolve_texture_path(const std::string& texture_path) {
    const QString path = QString::fromStdString(texture_path).trimmed();
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    if (info.isAbsolute() && info.exists()) {
        return info.absoluteFilePath();
    }

    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString material_path = app_dir.filePath("materials/" + path);
    if (QFileInfo::exists(material_path)) {
        return QFileInfo(material_path).absoluteFilePath();
    }

    const QString app_relative_path = app_dir.filePath(path);
    if (QFileInfo::exists(app_relative_path)) {
        return QFileInfo(app_relative_path).absoluteFilePath();
    }

    QDir build_dir(app_dir);
    if (build_dir.cdUp()) {
        const QString shared_release_path = build_dir.filePath("Release/" + path);
        if (QFileInfo::exists(shared_release_path)) {
            return QFileInfo(shared_release_path).absoluteFilePath();
        }
    }

    if (info.exists()) {
        return info.absoluteFilePath();
    }

    return {};
}

GLuint texture_id_for_path(const std::string& texture_path) {
    const QString resolved = resolve_texture_path(texture_path);
    if (resolved.isEmpty()) {
        return 0;
    }

    static std::unordered_map<std::string, GLuint> texture_cache;
    const std::string key = QDir::toNativeSeparators(resolved).toStdString();
    const auto found = texture_cache.find(key);
    if (found != texture_cache.end()) {
        return found->second;
    }

    QImage image(resolved);
    if (image.isNull()) {
        texture_cache[key] = 0;
        return 0;
    }

    image = image.convertToFormat(QImage::Format_RGBA8888).mirrored(false, true);
    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 image.width(),
                 image.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image.constBits());
    glBindTexture(GL_TEXTURE_2D, 0);

    texture_cache[key] = texture_id;
    return texture_id;
}

UV projected_uv_for_face(Vec3 vertex, Vec3 face_normal) {
    const Vec3 n = normalize(face_normal);
    const float ax = std::fabs(n.x);
    const float ay = std::fabs(n.y);
    const float az = std::fabs(n.z);

    if (ay >= ax && ay >= az) {
        return {vertex.x, vertex.z};
    }
    if (az >= ax) {
        return {vertex.x, vertex.y};
    }
    return {vertex.z, vertex.y};
}

UV transform_uv(UV uv, const Material& material) {
    const float scale_u = std::fabs(material.texture_scale_u) <= 0.00001f ? 1.0f : material.texture_scale_u;
    const float scale_v = std::fabs(material.texture_scale_v) <= 0.00001f ? 1.0f : material.texture_scale_v;
    const float angle = deg_to_rad(material.texture_rotation_degrees);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float u = (uv.u - 0.5f) * scale_u;
    const float v = (uv.v - 0.5f) * scale_v;

    return {
        u * c - v * s + 0.5f + material.texture_offset_u,
        u * s + v * c + 0.5f + material.texture_offset_v
    };
}

UV fit_uv_to_bounds(UV uv, UV minimum, UV maximum) {
    const float width = std::max(maximum.u - minimum.u, 0.00001f);
    const float height = std::max(maximum.v - minimum.v, 0.00001f);
    return {(uv.u - minimum.u) / width, (uv.v - minimum.v) / height};
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Color shaded_color(Color base,
                   Vec3 normal,
                   Vec3 direction_to_eye,
                   Vec3 view_up,
                   float specular_strength,
                   float shininess,
                   bool selected) {
    Vec3 view_dir = normalize(direction_to_eye);
    if (dot(view_dir, view_dir) <= 0.000001f) {
        view_dir = {0.0f, 0.0f, 1.0f};
    }
    Vec3 up = normalize(view_up);
    if (dot(up, up) <= 0.000001f) {
        up = {0.0f, 1.0f, 0.0f};
    }
    Vec3 right = normalize(cross(view_dir, up));
    if (dot(right, right) <= 0.000001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    up = normalize(cross(right, view_dir));

    // Surfaces are intentionally rendered without back-face culling.  Light
    // the side that is visible to the camera; otherwise a correctly smooth
    // open surface becomes almost black solely because its BRep orientation
    // happens to point away from the viewer.
    Vec3 n = normalize(normal);
    if (dot(n, view_dir) < 0.0f) {
        n = n * -1.0f;
    }

    // A soft camera-relative studio rig.  Keeping the key and fill lights in
    // view space makes the model readable while orbiting and avoids the hard
    // black patches produced by fixed world-space light directions.
    const std::array<Vec3, 3> lights{
        normalize(view_dir + up * 0.58f + right * 0.38f),
        normalize(view_dir + up * 0.10f - right * 0.82f),
        normalize(up + view_dir * 0.42f)
    };
    const std::array<float, 3> light_strengths{0.48f, 0.24f, 0.14f};
    float diffuse_light = 0.0f;
    float gloss = 0.0f;
    float strongest_light = 0.0f;
    for (size_t i = 0; i < lights.size(); ++i) {
        const float incidence = std::max(0.0f, dot(n, lights[i]));
        diffuse_light += incidence * light_strengths[i];
        strongest_light = std::max(strongest_light, incidence);
        const Vec3 half_vector = normalize(lights[i] + view_dir);
        gloss = std::max(
            gloss,
            std::pow(std::max(0.0f, dot(n, half_vector)),
                     std::max(4.0f, shininess)) * light_strengths[i]);
    }

    constexpr float kStudioAmbient = 0.68f;
    const float shade = std::min(1.42f, kStudioAmbient + diffuse_light);
    const float rim = std::pow(
        std::max(0.0f, 1.0f - std::fabs(dot(n, view_dir))), 3.0f)
        * strongest_light;
    const float highlight = std::min(0.32f, specular_strength * (gloss * 0.68f + rim * 0.12f));
    const float selected_boost = selected ? 1.08f : 1.0f;

    return {
        clamp01(base.r * shade * selected_boost + strongest_light * 0.04f + highlight),
        clamp01(base.g * shade * selected_boost + strongest_light * 0.04f + highlight),
        clamp01(base.b * shade * selected_boost + strongest_light * 0.05f + highlight)
    };
}

Color normal_rgb_color(Vec3 normal, bool selected) {
    const Vec3 n = normalize(normal);
    const float direction = selected ? -1.0f : 1.0f;
    constexpr float kDom3dChannelScale = 127.0f / 255.0f;
    return {
        clamp01((direction * n.x + 1.0f) * kDom3dChannelScale),
        clamp01((direction * n.y + 1.0f) * kDom3dChannelScale),
        clamp01((direction * n.z + 1.0f) * kDom3dChannelScale)
    };
}

GLuint zebra_texture_id() {
    static GLuint texture_id = 0;
    if (texture_id != 0) {
        return texture_id;
    }

    constexpr int kTextureSize = 128;
    std::array<unsigned char, kTextureSize * 4> pixels{};
    const auto smooth_step = [](float first, float second, float value) {
        const float t = std::clamp(
            (value - first) / (second - first), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    for (int i = 0; i < kTextureSize; ++i) {
        const float phase = (static_cast<float>(i) + 0.5f) / kTextureSize;
        const float rising = smooth_step(0.46f, 0.50f, phase);
        const float falling = 1.0f - smooth_step(0.96f, 1.0f, phase);
        const float white = std::min(rising, falling);
        const unsigned char value = static_cast<unsigned char>(
            std::round((0.025f + white * 0.95f) * 255.0f));
        pixels[static_cast<size_t>(i) * 4 + 0] = value;
        pixels[static_cast<size_t>(i) * 4 + 1] = value;
        pixels[static_cast<size_t>(i) * 4 + 2] = value;
        pixels[static_cast<size_t>(i) * 4 + 3] = 255;
    }

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_1D, texture_id);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexImage1D(
        GL_TEXTURE_1D, 0, GL_RGBA, kTextureSize, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_1D, 0);
    return texture_id;
}

QOpenGLShaderProgram* zebra_shader_program() {
    static QOpenGLContext* shader_context = nullptr;
    static std::unique_ptr<QOpenGLShaderProgram> shader_program;
    QOpenGLContext* current_context = QOpenGLContext::currentContext();
    if (!current_context) {
        return nullptr;
    }
    if (shader_program && shader_context == current_context) {
        return shader_program.get();
    }

    auto candidate = std::make_unique<QOpenGLShaderProgram>();
    constexpr const char* vertex_shader = R"GLSL(
#version 120
varying vec3 zebraNormalEye;
varying vec3 zebraPositionEye;

void main() {
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    zebraPositionEye = eyePosition.xyz;
    zebraNormalEye = gl_NormalMatrix * gl_Normal;
    gl_Position = gl_ProjectionMatrix * eyePosition;
}
)GLSL";
    constexpr const char* fragment_shader = R"GLSL(
#version 120
varying vec3 zebraNormalEye;
varying vec3 zebraPositionEye;
uniform float zebraStripeCount;

void main() {
    vec3 normal = normalize(zebraNormalEye);
    vec3 directionToEye = normalize(-zebraPositionEye);
    vec3 reflection = reflect(-directionToEye, normal);
    float coordinate = reflection.y * 0.5 + 0.5;
    float phase = coordinate * zebraStripeCount * 6.28318530718;
    float wave = sin(phase);
    float transition = max(fwidth(phase) * 0.35, 0.035);
    float white = smoothstep(-transition, transition, wave);
    float value = mix(0.025, 0.975, white);
    gl_FragColor = vec4(value, value, value, 1.0);
}
)GLSL";
    if (!candidate->addShaderFromSourceCode(
            QOpenGLShader::Vertex, vertex_shader)
        || !candidate->addShaderFromSourceCode(
            QOpenGLShader::Fragment, fragment_shader)
        || !candidate->link()) {
        return nullptr;
    }
    shader_context = current_context;
    shader_program = std::move(candidate);
    return shader_program.get();
}

Color wire_color(Color base, MeshDisplayMode mode, bool selected) {
    if (mode == MeshDisplayMode::SurfaceGray
        || mode == MeshDisplayMode::SurfaceColored) {
        return selected ? Color{0.045f, 0.050f, 0.052f} : Color{0.075f, 0.083f, 0.087f};
    }
    if (mode == MeshDisplayMode::Wire) {
        const float luminance = base.r * 0.30f + base.g * 0.59f + base.b * 0.11f;
        const float lift = std::max(0.0f, 0.42f - luminance);
        const Color visible{
            clamp01(base.r + lift),
            clamp01(base.g + lift),
            clamp01(base.b + lift)
        };
        return selected
            ? Color{clamp01(visible.r * 1.18f), clamp01(visible.g * 1.18f), clamp01(visible.b * 1.18f)}
            : visible;
    }

    const float luminance = base.r * 0.30f + base.g * 0.59f + base.b * 0.11f;
    const float shade = selected ? 0.20f : (luminance > 0.58f ? 0.27f : 0.34f);
    const float neutral = selected ? 0.022f : (luminance > 0.58f ? 0.035f : 0.050f);
    return {
        clamp01(base.r * shade + neutral),
        clamp01(base.g * shade + neutral),
        clamp01(base.b * shade + neutral)
    };
}

Material colored_mesh_material() {
    return Material::ImportedMesh();
}

constexpr double TRIM_CLOSURE_EPS = EPS2D * 10.0;

std::filesystem::path obj_output_path(const std::string& name) {
    std::filesystem::path path(name);
    if (!path.has_extension()) {
        path += ".obj";
    }
    return path;
}

cVec2 mesh_vertex_2d(const std::vector<Vec3>& vertices, size_t index)
{
    const Vec3& vertex = vertices[index];
    return {vertex.x, vertex.y};
}

Face2D make_face_2d(const CMesh3D::Face& face, const std::vector<Vec3>& vertices)
{
    Face2D face_2d;
    face_2d.verts.reserve(face.corners.size());
    for (const MeshCorner& corner : face.corners)
        face_2d.verts.push_back(mesh_vertex_2d(vertices, corner.v));
    return face_2d;
}

std::vector<cVec2> make_cut_2d(const CPolyline* line)
{
    std::vector<cVec2> cut;
    if (!line)
        return cut;
    const std::vector<CPoint3d>& points = line->GetPoints();
    cut.reserve(points.size() + 1);
    for (const CPoint3d& point : points)
        cut.emplace_back(point.x, point.y);
    if (points.size() > 1 && (line->IsClosed() || EqualPoint2(cut.front(), cut.back(), TRIM_CLOSURE_EPS))) {
        if (EqualPoint2(cut.front(), cut.back(), TRIM_CLOSURE_EPS))
            cut.back() = cut.front();
        else
            cut.push_back(cut.front());
    }
    return cut;
}

bool cut_is_closed(const std::vector<cVec2>& cut)
{
    return cut.size() > 2 && EqualPoint2(cut.front(), cut.back(), TRIM_CLOSURE_EPS);
}

cVec2 face_center_2d(const CMesh3D::Face& face, const std::vector<Vec3>& vertices)
{
    cVec2 center;
    int count = 0;
    for (const MeshCorner& corner : face.corners) {
        if (corner.v >= vertices.size())
            continue;
        center.x += vertices[corner.v].x;
        center.y += vertices[corner.v].y;
        ++count;
    }
    if (count > 0) {
        center.x /= static_cast<double>(count);
        center.y /= static_cast<double>(count);
    }
    return center;
}

bool should_delete_closed_trim_face(const CMesh3D::Face& face,
                                    const std::vector<Vec3>& vertices,
                                    const Face2D& trim_polygon,
                                    bool keep_inside)
{
    const PointFacePos center_pos = ClassifyPointInFace2(trim_polygon, face_center_2d(face, vertices), EPS2D);
    if (center_pos != PFP_BOUNDARY) {
        const bool center_inside = center_pos != PFP_OUTSIDE;
        return center_inside != keep_inside;
    }

    bool has_delete_sample = false;
    int delete_samples = 0;
    int keep_samples = 0;
    for (const MeshCorner& corner : face.corners) {
        if (corner.v >= vertices.size())
            continue;

        const PointFacePos pos = ClassifyPointInFace2(trim_polygon, mesh_vertex_2d(vertices, corner.v), EPS2D);
        if (pos == PFP_BOUNDARY)
            continue;

        const bool sample_inside = pos != PFP_OUTSIDE;
        if (sample_inside == keep_inside) {
            ++keep_samples;
        } else {
            has_delete_sample = true;
            ++delete_samples;
        }
    }

    return has_delete_sample && delete_samples >= keep_samples;
}

bool face_has_closed_trim_delete_sample(const CMesh3D::Face& face,
                                        const std::vector<Vec3>& vertices,
                                        const Face2D& trim_polygon,
                                        bool keep_inside)
{
    const PointFacePos center_pos = ClassifyPointInFace2(trim_polygon, face_center_2d(face, vertices), EPS2D);
    if (center_pos != PFP_BOUNDARY && ((center_pos != PFP_OUTSIDE) != keep_inside))
        return true;

    for (const MeshCorner& corner : face.corners) {
        if (corner.v >= vertices.size())
            continue;

        const PointFacePos pos = ClassifyPointInFace2(trim_polygon, mesh_vertex_2d(vertices, corner.v), EPS2D);
        if (pos == PFP_BOUNDARY)
            continue;
        if ((pos != PFP_OUTSIDE) != keep_inside)
            return true;
    }
    return false;
}

double distance2(const cVec2& a, const cVec2& b)
{
    return Length2(a - b);
}

struct CutProjection {
    cVec2 point;
    double dist2 = std::numeric_limits<double>::max();
    int segment = -1;
};

CutProjection project_point_to_cut(const cVec2& point, const std::vector<cVec2>& cut)
{
    CutProjection best;
    for (size_t i = 0; i + 1 < cut.size(); ++i) {
        const cVec2 a = cut[i];
        const cVec2 b = cut[i + 1];
        const cVec2 ab = b - a;
        const double ab2 = Length2(ab);
        double t = 0.0;
        if (ab2 > 1.0e-20)
            t = std::clamp(((point.x - a.x) * ab.x + (point.y - a.y) * ab.y) / ab2, 0.0, 1.0);
        const cVec2 projected = a + ab * t;
        const double dist = distance2(point, projected);
        if (dist < best.dist2) {
            best.point = projected;
            best.dist2 = dist;
            best.segment = static_cast<int>(i);
        }
    }
    return best;
}

size_t add_or_find_vertex_2d(std::vector<Vec3>& vertices, const cVec2& point, double eps)
{
    const double eps2 = eps * eps;
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (distance2(mesh_vertex_2d(vertices, i), point) <= eps2)
            return i;
    }

    const size_t index = vertices.size();
    vertices.push_back({static_cast<float>(point.x), static_cast<float>(point.y), 0.0f});
    return index;
}

bool point_on_cut_segment(const cVec2& point, const cVec2& a, const cVec2& b, double eps)
{
    const cVec2 ab = b - a;
    const cVec2 ap = point - a;
    const double ab2 = Length2(ab);
    if (ab2 <= eps * eps)
        return EqualPoint2(point, a, eps);
    const double cross = std::fabs(ab.x * ap.y - ab.y * ap.x);
    if (cross > eps)
        return false;
    const double dot_value = ap.x * ab.x + ap.y * ab.y;
    return dot_value >= -eps && dot_value <= ab2 + eps;
}

bool edge_on_cut(const cVec2& a, const cVec2& b, const std::vector<cVec2>& cut, double eps)
{
    for (size_t i = 0; i + 1 < cut.size(); ++i) {
        if (point_on_cut_segment(a, cut[i], cut[i + 1], eps)
            && point_on_cut_segment(b, cut[i], cut[i + 1], eps)) {
            return true;
        }
    }
    return false;
}

double cross2d(const cVec2& a, const cVec2& b)
{
    return a.x * b.y - a.y * b.x;
}

bool cut_crosses_edge_interior(const cVec2& edge_a,
                               const cVec2& edge_b,
                               const cVec2& cut_a,
                               const cVec2& cut_b,
                               double eps)
{
    const cVec2 edge = edge_b - edge_a;
    const cVec2 cut = cut_b - cut_a;
    const double denom = cross2d(edge, cut);

    if (std::fabs(denom) <= eps) {
        if (!point_on_cut_segment(edge_a, cut_a, cut_b, eps)
            && !point_on_cut_segment(edge_b, cut_a, cut_b, eps)) {
            return false;
        }

        const double edge_len2 = Length2(edge);
        if (edge_len2 <= eps * eps)
            return false;
        const double t0 = ((cut_a.x - edge_a.x) * edge.x + (cut_a.y - edge_a.y) * edge.y) / edge_len2;
        const double t1 = ((cut_b.x - edge_a.x) * edge.x + (cut_b.y - edge_a.y) * edge.y) / edge_len2;
        const double lo = std::max(0.0, std::min(t0, t1));
        const double hi = std::min(1.0, std::max(t0, t1));
        return hi - lo > eps && hi > eps && lo < 1.0 - eps;
    }

    const cVec2 delta = cut_a - edge_a;
    const double t = cross2d(delta, cut) / denom;
    const double u = cross2d(delta, edge) / denom;
    return t > eps && t < 1.0 - eps && u >= -eps && u <= 1.0 + eps;
}

bool edge_blocked_by_cut(const cVec2& a, const cVec2& b, const std::vector<cVec2>& cut, double eps)
{
    if (edge_on_cut(a, b, cut, eps))
        return true;

    for (size_t i = 0; i + 1 < cut.size(); ++i) {
        if (cut_crosses_edge_interior(a, b, cut[i], cut[i + 1], eps))
            return true;
    }
    return false;
}

std::pair<size_t, size_t> normalized_edge(size_t a, size_t b)
{
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

struct EdgeCoordKey {
    long long ax = 0;
    long long ay = 0;
    long long bx = 0;
    long long by = 0;

    bool operator<(const EdgeCoordKey& other) const
    {
        if (ax != other.ax)
            return ax < other.ax;
        if (ay != other.ay)
            return ay < other.ay;
        if (bx != other.bx)
            return bx < other.bx;
        return by < other.by;
    }
};

std::pair<long long, long long> point_key_2d(const cVec2& point, double eps)
{
    const double scale = 1.0 / eps;
    return {
        static_cast<long long>(std::llround(point.x * scale)),
        static_cast<long long>(std::llround(point.y * scale))
    };
}

EdgeCoordKey edge_coord_key(const cVec2& a, const cVec2& b, double eps)
{
    auto first = point_key_2d(a, eps);
    auto second = point_key_2d(b, eps);
    if (second < first)
        std::swap(first, second);
    return {first.first, first.second, second.first, second.second};
}

void PrepareAndMoveVertexToTrimLinePro(std::vector<CMesh3D::Face>& faces,
                                    std::vector<Vec3>& vertices,
                                    const std::vector<size_t>& affected_faces,
                                    const std::vector<cVec2>& cut,
                                    const Face2D* trim_polygon,
                                    bool keep_inside)
{
    struct IndAndDist {
        size_t cut_index = 0;
        size_t face_index = 0;
        size_t vert_pos = 0;
        double dist2 = std::numeric_limits<double>::max();
        bool need_move = true;
    };

    std::vector<cVec2> cut_nodes;
    cut_nodes.reserve(cut.size());
    for (const cVec2& point : cut) {
        if (cut_nodes.empty() || !EqualPoint2(cut_nodes.back(), point, EPS2D))
            cut_nodes.push_back(point);
    }
    if (cut_nodes.size() > 1 && EqualPoint2(cut_nodes.front(), cut_nodes.back(), EPS2D))
        cut_nodes.pop_back();
    if (cut_nodes.empty())
        return;

    if (trim_polygon && trim_polygon->verts.size() >= 3) {
        struct VertexMove {
            cVec2 point;
            double dist2 = std::numeric_limits<double>::max();
        };
        std::unordered_map<size_t, VertexMove> moves;
        moves.reserve(affected_faces.size() * 2);

        for (size_t face_index : affected_faces) {
            if (face_index >= faces.size())
                continue;
            const CMesh3D::Face& face = faces[face_index];
            if (face.deleted || face.corners.size() < 3)
                continue;

            for (const MeshCorner& corner : face.corners) {
                if (corner.v >= vertices.size())
                    continue;

                const cVec2 point = mesh_vertex_2d(vertices, corner.v);
                const PointFacePos pos = ClassifyPointInFace2(*trim_polygon, point, EPS2D);
                if (pos == PFP_BOUNDARY)
                    continue;

                const bool vertex_inside = pos != PFP_OUTSIDE;
                if (vertex_inside == keep_inside)
                    continue;

                const CutProjection projection = project_point_to_cut(point, cut);
                if (projection.segment < 0)
                    continue;

                VertexMove& move = moves[corner.v];
                if (projection.dist2 < move.dist2) {
                    move.point = projection.point;
                    move.dist2 = projection.dist2;
                }
            }
        }

        for (const auto& entry : moves) {
            const size_t vertex = entry.first;
            if (vertex >= vertices.size())
                continue;
            vertices[vertex].x = static_cast<float>(entry.second.point.x);
            vertices[vertex].y = static_cast<float>(entry.second.point.y);
            vertices[vertex].z = 0.0f;
        }
        return;
    }

    std::vector<IndAndDist> ind_and_dist_arr;
    ind_and_dist_arr.reserve(affected_faces.size() * 2);

    for (size_t face_index : affected_faces) {
        if (face_index >= faces.size())
            continue;
        const CMesh3D::Face& face = faces[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;

        std::vector<IndAndDist> face_candidates;
        face_candidates.reserve(face.corners.size());
        for (size_t vert_pos = 0; vert_pos < face.corners.size(); ++vert_pos) {
            const MeshCorner& corner = face.corners[vert_pos];
            if (corner.v >= vertices.size())
                continue;

            IndAndDist best;
            best.face_index = face_index;
            best.vert_pos = vert_pos;
            for (size_t cut_index = 0; cut_index < cut_nodes.size(); ++cut_index) {
                const double dist = distance2(mesh_vertex_2d(vertices, corner.v), cut_nodes[cut_index]);
                if (best.dist2 > dist) {
                    best.dist2 = dist;
                    best.cut_index = cut_index;
                }
            }
            face_candidates.push_back(best);
        }

        std::sort(face_candidates.begin(), face_candidates.end(), [](const IndAndDist& a, const IndAndDist& b) {
            return a.dist2 < b.dist2;
        });
        const size_t move_count = std::min<size_t>(2, face_candidates.size());
        for (size_t i = 0; i < move_count; ++i)
            ind_and_dist_arr.push_back(face_candidates[i]);
    }

    for (size_t j = 0; j < ind_and_dist_arr.size(); ++j) {
        for (size_t i = 0; i < ind_and_dist_arr.size(); ++i) {
            if (j == i)
                continue;
            if (ind_and_dist_arr[j].cut_index == ind_and_dist_arr[i].cut_index) {
                if (ind_and_dist_arr[j].dist2 < ind_and_dist_arr[i].dist2) {
                    ind_and_dist_arr[j].need_move = true;
                    ind_and_dist_arr[i].need_move = false;
                } else {
                    ind_and_dist_arr[j].need_move = false;
                    ind_and_dist_arr[i].need_move = true;
                }
            }
        }
    }

    for (const IndAndDist& ind_and_dist : ind_and_dist_arr) {
        if (!ind_and_dist.need_move)
            continue;
        if (ind_and_dist.face_index >= faces.size() || ind_and_dist.cut_index >= cut_nodes.size())
            continue;
        CMesh3D::Face& face = faces[ind_and_dist.face_index];
        if (face.deleted || ind_and_dist.vert_pos >= face.corners.size())
            continue;

        const size_t vertex = face.corners[ind_and_dist.vert_pos].v;
        if (vertex >= vertices.size())
            continue;

        const cVec2& point = cut_nodes[ind_and_dist.cut_index];
        vertices[vertex].x = static_cast<float>(point.x);
        vertices[vertex].y = static_cast<float>(point.y);
        vertices[vertex].z = 0.0f;
    }
}

} // namespace

bool CMesh3D::PrepareAndMoveVertexToTrimLine(CPolyline* pLine, std::vector<DataToMoveVerts*>& Data)
{
    std::vector<DataToMoveVerts*> prepared_data;
    prepared_data.reserve(Data.size());
    for (int j = 0; j < static_cast<int>(Data.size()); j++) {
        DataToMoveVerts* data = Data[j];
        if (FindVertexToMove(pLine, data))
            prepared_data.push_back(data);
    }
    std::vector<IndAndDist> indAndDistArr;
    for (int j = 0; j < static_cast<int>(prepared_data.size()); j++) {
        IndAndDist indAndDist1 = prepared_data[j]->IndAndDistArr[0];
        indAndDistArr.push_back(indAndDist1);
        IndAndDist indAndDist2 = prepared_data[j]->IndAndDistArr[1];
        indAndDistArr.push_back(indAndDist2);
    }
    for (int j = 0; j < static_cast<int>(indAndDistArr.size()); j++) {
        for (int i = 0; i < static_cast<int>(indAndDistArr.size()); i++) {
            if (j == i)
                continue;
            if (indAndDistArr[j].ind == indAndDistArr[i].ind) {

                IndAndDist indAndDist1 = indAndDistArr[j];
                IndAndDist indAndDist2 = indAndDistArr[i];
                if (indAndDist1.dist < indAndDist2.dist) {
                    indAndDistArr[j].needMove = true;
                    indAndDistArr[i].needMove = false;
                }

                else {
                    indAndDistArr[j].needMove = false;
                    indAndDistArr[i].needMove = true;
                }
            }
        }
    }

    for (int j = 0; j < static_cast<int>(indAndDistArr.size()); j++) {
        if (indAndDistArr[j].needMove) {
            MeshFace* face = indAndDistArr[j].pf;
            int ind_pLine = indAndDistArr[j].ind;
            int vertInd = indAndDistArr[j].vertInd;
            if (!face || ind_pLine < 0 || vertInd < 0 || vertInd >= static_cast<int>(face->corners.size()))
                continue;

            const size_t vertex_index = face->corners[static_cast<size_t>(vertInd)].v;
            if (vertex_index >= vertices_.size())
                continue;

            vertices_[vertex_index].x = static_cast<float>(pLine->P(ind_pLine)->x);
            vertices_[vertex_index].y = static_cast<float>(pLine->P(ind_pLine)->y);
        }
    }

    return true;
}


bool CMesh3D::FindVertexToMove(CPolyline* pLine, DataToMoveVerts* data)
{
    if (!pLine || !data)
        return false;
    CMesh3D::Face* face = data->pf;
    if (!face || face->corners.size() < 2)
        return false;

    std::vector<IndAndDist> indAndDist;
    indAndDist.reserve(face->corners.size());

    for (size_t vert_pos = 0; vert_pos < face->corners.size(); ++vert_pos) {
        const size_t vertex_index = face->corners[vert_pos].v;
        if (vertex_index >= vertices_.size())
            continue;

        IndAndDist candidate;
        candidate.pf = face;
        candidate.dist = 1e15;
        candidate.ind = -1;
        candidate.vertInd = static_cast<int>(vert_pos);

        CPoint3d p(vertices_[vertex_index].x, vertices_[vertex_index].y, 0);
        for (int j = 0; j < pLine->np(); j++) {
            double dist = p.DistTo(pLine->P(j));
            if (candidate.dist > dist) {
                candidate.dist = dist;
                candidate.ind = j;
            }
        }
        indAndDist.push_back(candidate);
    }

    if (indAndDist.size() < 2)
        return false;

    std::sort(indAndDist.begin(), indAndDist.end(), [](const IndAndDist& a, const IndAndDist& b) {
        return a.dist < b.dist;
        });

    data->IndAndDistArr[0] = indAndDist[0];
    data->IndAndDistArr[1] = indAndDist[1];
    data->IndAndDistArr[0].vertInd = indAndDist[0].vertInd;
    data->IndAndDistArr[1].vertInd = indAndDist[1].vertInd;

    return true;
}

namespace {

bool SplitFaceByLine(std::vector<CMesh3D::Face>& faces, size_t face_index, int pos_a, int pos_b)
{
    if (face_index >= faces.size())
        return false;
    CMesh3D::Face source = faces[face_index];
    const int count = static_cast<int>(source.corners.size());
    if (source.deleted || count < 4 || pos_a < 0 || pos_b < 0 || pos_a >= count || pos_b >= count || pos_a == pos_b)
        return false;

    const int diff = std::abs(pos_a - pos_b);
    if (diff == 1 || diff == count - 1)
        return false;

    std::vector<MeshCorner> seq1;
    std::vector<MeshCorner> seq2;
    for (int k = pos_a;; k = (k + 1) % count) {
        seq1.push_back(source.corners[static_cast<size_t>(k)]);
        if (k == pos_b)
            break;
    }
    for (int k = pos_b;; k = (k + 1) % count) {
        seq2.push_back(source.corners[static_cast<size_t>(k)]);
        if (k == pos_a)
            break;
    }
    if (seq1.size() < 3 || seq2.size() < 3)
        return false;

    CMesh3D::Face new_face = source;
    new_face.corners = std::move(seq1);
    faces[face_index].corners = std::move(seq2);
    faces.push_back(std::move(new_face));
    return true;
}

std::vector<int> find_cut_touched_positions(const CMesh3D::Face& face,
                                            const std::vector<Vec3>& vertices,
                                            const std::vector<cVec2>& cut)
{
    std::vector<int> touched_positions;
    for (int i = 0; i < static_cast<int>(face.corners.size()); ++i) {
        const size_t vertex = face.corners[static_cast<size_t>(i)].v;
        if (vertex >= vertices.size())
            continue;
        const cVec2 point = mesh_vertex_2d(vertices, vertex);
        for (size_t segment = 0; segment + 1 < cut.size(); ++segment) {
            if (point_on_cut_segment(point, cut[segment], cut[segment + 1], EPS2D)) {
                touched_positions.push_back(i);
                break;
            }
        }
    }

    std::sort(touched_positions.begin(), touched_positions.end());
    touched_positions.erase(std::unique(touched_positions.begin(), touched_positions.end()), touched_positions.end());
    return touched_positions;
}

}

bool CMesh3D::SplitFaceByPoint(int face_index, int ind1, int ind2, const cVec2& pm)
{
    MeshFace& face = faces_[face_index];
    const int count = static_cast<int>(face.corners.size());
    if (face.deleted || count < 3 || ind1 < 0 || ind2 < 0 || ind1 >= count || ind2 >= count || ind1 == ind2)
        return false;
    
    int diff = abs(ind1 - ind2);
    if (diff == 1 || diff == face.corners.size() - 1)
        return SplitFaceByPointVar4(face_index, ind1, ind2, pm);
    if (ind1 > ind2)
        std::swap(ind1, ind2);
    if (ind1 == 1)
        return SplitFaceByPointVar3(face_index, ind1, ind2, pm);
    if (face.corners.size() < 4)
        return false;
 
    const size_t middle_vertex = add_or_find_vertex_2d(vertices_, pm, 0.0001);
    const MeshCorner middle = { middle_vertex, 0, 0 };

    CPolyline polygon;
    for (int i = 0; i < face.corners.size(); i++) {
        Vec3 pv =  vertices_[face.corners[i].v];
        CPoint3d pnt(pv.x, pv.y, 0);
        polygon.AddPoint(pnt);
    }
    Vec3 pv = vertices_[face.corners[0].v];
    CPoint3d pv3d(pv.x, pv.y, 0);
    polygon.AddPoint(pv3d);
    polygon.P(3)->x = pm.x;
    polygon.P(3)->y = pm.y;

    bool Concave = polygon.IsConcavePolygonOnXY();
    std::vector<MeshCorner> seq1;
    std::vector<MeshCorner> seq2;
    if (Concave) {
        seq1.push_back(face.corners[0]);
        seq1.push_back(face.corners[1]);
        seq1.push_back(middle);
        seq2.push_back(face.corners[1]);
        seq2.push_back(face.corners[2]);
        seq2.push_back(middle);
    }
    else {
        seq1.push_back(face.corners[0]);
        seq1.push_back(middle);
        seq1.push_back(face.corners[3]);
        seq2.push_back(middle);
        seq2.push_back(face.corners[2]);
        seq2.push_back(face.corners[3]);
    }
// =========  At now need to Add 2 new Face to this CMesh3D
    if (Concave)
        face.corners[1] = middle;
    else
        face.corners[3] = middle;
    face.normal = FaceNormal(face);

    MeshFace face1 = face;
    face1.corners = std::move(seq1);
    face1.normal = FaceNormal(face1);
    faces_.push_back(std::move(face1));

    MeshFace face2 = face;
    face2.corners = std::move(seq2);
    face2.normal = FaceNormal(face2);
    faces_.push_back(std::move(face2));

    return true;
}
bool CMesh3D::SplitFaceByPointVar4(int face_index, int ind1, int ind2, const cVec2& pm)
{
    MeshFace& face = faces_[face_index];
    const int count = static_cast<int>(face.corners.size());
    if (face.deleted || count < 3 || ind1 < 0 || ind2 < 0 || ind1 >= count || ind2 >= count || ind1 == ind2)
        return false;
    if (count < 4)
        return false;

    if (ind1 > ind2)
        std::swap(ind1, ind2);
    if (ind1 == 0 && ind2 == 3)
        std::swap(ind1, ind2);
    
    const size_t middle_vertex = add_or_find_vertex_2d(vertices_, pm, 0.0001);
    const MeshCorner middle = { middle_vertex, 0, 0 };
 
    int ind3 = 2;
    int ind4 = 3;
    if (ind1 == 3) {
        ind3 = 1;
        ind4 = 2;
    }
    if (ind1 == 2) {
        ind3 = 0;
        ind4 = 1;
    }
    if (ind1 == 1) {
        ind3 = 3;
        ind4 = 0;
    }

    std::vector<MeshCorner> seq1;
    std::vector<MeshCorner> seq2;
    std::vector<MeshCorner> seq3;

    seq1.push_back(face.corners[ind1]);
    seq1.push_back(middle);
    seq1.push_back(face.corners[ind4]);

    seq2.push_back(face.corners[ind3]);
    seq2.push_back(face.corners[ind4]);
    seq2.push_back(middle);

    seq3.push_back(face.corners[ind2]);
    seq3.push_back(face.corners[ind3]);
    seq3.push_back(middle);

    if (ind1 == 0)
        face.corners[ind3] = middle;
    if (ind1 == 1)
        face.corners[0] = middle;
    if (ind1 == 2) {
        face.corners[0] = face.corners[3];
        face.corners[1] = middle;
    }

    if (ind1 == 3) {
        face.corners[1] = middle;
        face.corners[2] = face.corners[3];
    }
    face.corners.resize(3);
    face.normal = FaceNormal(face);

    MeshFace face1 = face;
    face1.corners = std::move(seq1);
    face1.normal = FaceNormal(face1);
    faces_.push_back(std::move(face1));

    MeshFace face2 = face;
    face2.corners = std::move(seq2);
    face2.normal = FaceNormal(face2);
    faces_.push_back(std::move(face2));


    MeshFace face3 = face;
    face3.corners = std::move(seq3);
    face3.normal = FaceNormal(face3);
    faces_.push_back(std::move(face3));

    return true;
}
bool CMesh3D::SplitFaceByPointVar3(int face_index, int ind1, int ind2, const cVec2& pm)
{
    MeshFace& face = faces_[face_index];
    const int count = static_cast<int>(face.corners.size());
    if (face.deleted || count < 3 || ind1 < 0 || ind2 < 0 || ind1 >= count || ind2 >= count || ind1 == ind2)
        return false;
    if (count < 4)
        return false;

 /*   CAlfaDoc* pDoc = GetAlfaDoc();
    pDoc->AddLayer("Faces");
    auto pLine01 = std::make_unique<CPolyline>();
    MakePolyline(face_index, *pLine01);
    pLine01->printToFile("Face_f1.txt");
    pLine01->SetName("Face_f1");
    pDoc->AddObject(std::move(pLine01));*/


    const size_t middle_vertex = add_or_find_vertex_2d(vertices_, pm, 0.0001);
    const MeshCorner middle = { middle_vertex, 0, 0 };

    CPolyline polygon;
    for (int i = 0; i < face.corners.size(); i++) {
        Vec3 pv = vertices_[face.corners[i].v];
        CPoint3d pnt(pv.x, pv.y, 0);
        polygon.AddPoint(pnt);
    }
    Vec3 pv = vertices_[face.corners[0].v];
    CPoint3d pv3d(pv.x, pv.y, 0);
    polygon.AddPoint(pv3d);
    polygon.P(2)->x = pm.x;
    polygon.P(2)->y = pm.y;
    bool Concave = polygon.IsConcavePolygonOnXY();
 //   polygon.printToFile(" polygon");
 //   if (Concave)
 //       Step("Concave");

    std::vector<MeshCorner> seq1;
    std::vector<MeshCorner> seq2;

    if (Concave) {
        seq1.push_back(face.corners[0]);
        seq1.push_back(face.corners[1]);
        seq1.push_back(middle);
        seq2.push_back(face.corners[0]);
        seq2.push_back(middle);
        seq2.push_back(face.corners[3]);
    }
    else {
        seq1.push_back(face.corners[1]);
        seq1.push_back(face.corners[2]);
        seq1.push_back(middle);
        seq2.push_back(middle);
        seq2.push_back(face.corners[2]);
        seq2.push_back(face.corners[3]);
    }

    if (Concave)
        face.corners[0] = middle;
    else
        face.corners[2] = middle;

    MeshFace face1 = face;
    face1.corners = std::move(seq1);
    face1.normal = FaceNormal(face1);
    faces_.push_back(std::move(face1));

    MeshFace face2 = face;
    face2.corners = std::move(seq2);
    face2.normal = FaceNormal(face2);
    faces_.push_back(std::move(face2));

    return true;
}

bool CMesh3D::SplitFaceByVar5(int f1, int v1i, int edgeIndex, cVec2& pm)
{
    if(f1 == -1)
		return false;
    if (edgeIndex == -1)
        return false;
//	 Step("SplitFaceByVar5");
	 char str[100];
//	 sprintf(str, "f1=%d, v1i=%d, edgeIndex=%d", f1, v1i, edgeIndex);
 //    Step(str);
    MeshFace& face = faces_[f1];
    if (face.m_Trimmed)
        return true;
  /*  CAlfaDoc* pDoc = GetAlfaDoc();
    pDoc->AddLayer("Faces");
    auto pLine01 = std::make_unique<CPolyline>();
    MakePolyline(f1, *pLine01);
	pLine01->printToFile("Face_f1.txt");
	pLine01->SetName("Face_f1");
    pDoc->AddObject(std::move(pLine01));*/

    int ev1 = (int)face.corners[edgeIndex].v;
    int VrtIndex2 = edgeIndex + 1;
    if (edgeIndex == 3)
        VrtIndex2 = 0;
    int ev2 = (int)face.corners[VrtIndex2].v;
    Edge ed(ev1, ev2);
//    int f1 = FindFirstFace3d(ed);
    int f2 = FindSecondCFace3d(f1, ed);
    if (f2 == -1)
        return true;
    MeshFace& face2 = faces_[f2];
 /*   auto pLine02 = std::make_unique<CPolyline>();
    MakePolyline(f2, *pLine02);
    pLine02->printToFile("Face_f2.txt");
    pLine02->SetName("Face_f2");
    pDoc->AddObject(std::move(pLine02));*/

    const size_t middle_vertex = add_or_find_vertex_2d(vertices_, pm, 0.0001);
    const MeshCorner middle = { middle_vertex, 0, 0 };
    cVec2 pm2(face2.pm.x, face2.pm.y) ;
    const size_t middle_vertex2 = add_or_find_vertex_2d(vertices_, pm2, 0.0001);
    const MeshCorner middle2 = { middle_vertex2, 0, 0 };

 //   sprintf(str, "pm=<%f, %f> pm2=<%f, %f>", pm.x, pm.y, pm2.x, pm2.y);
 //   Step(str);


    int ind1 = 0;
    int ind2 = 0;
    if (edgeIndex == 0) {
        ind1 = 0;
        ind2 = 1;
    }
    if (edgeIndex == 3) {
        ind1 = 3;
        ind2 = 0;
    }
    if (edgeIndex == 2) {
        ind1 = 2;
        ind2 = 3;
    }
    if (edgeIndex == 1) {
        ind1 = 1;
        ind2 = 2;
    }
    std::vector<MeshCorner> seq4;
    std::vector<MeshCorner> seq5;

    seq4.push_back(middle);
    seq4.push_back(face.corners[ind1]);
    seq4.push_back(middle2);

    MeshFace face4 = face;
    face4.corners = std::move(seq4);
    face4.normal = FaceNormal(face4);
    faces_.push_back(std::move(face4));

//	int f4 = faces_.size() - 1;
 //   auto pLine4 = std::make_unique<CPolyline>();
 //   MakePolyline(f4, *pLine4);
 //   pLine4->SetName("Face_f4");
 //   pDoc->AddObject(std::move(pLine4));

    seq5.push_back(middle);
    seq5.push_back(middle2);
    seq5.push_back(faces_[f1].corners[ind2]);

    MeshFace face5 = face;
    face5.corners = std::move(seq5);
    face5.normal = FaceNormal(face5);
    faces_.push_back(std::move(face5));

 //   int f5 = faces_.size() - 1;
 //   auto pLine5 = std::make_unique<CPolyline>();
 //   MakePolyline(f5, *pLine5);
 //   pLine5->SetName("Face_f5");
 //   pDoc->AddObject(std::move(pLine5));

    std::vector < size_t> indV1;
    std::vector < size_t> indV2;
	size_t v0 = faces_[f1].corners[0].v;
	size_t v1 = faces_[f1].corners[1].v;
	size_t v2 = faces_[f1].corners[2].v;
    size_t v3 = faces_[f1].corners[3].v;

    if (edgeIndex == 0) {
        indV1.push_back(v2);
        indV1.push_back(v3);
        indV1.push_back(middle_vertex);
        indV2.push_back(v3);
        indV2.push_back(v0);
        indV2.push_back(middle_vertex);
    }
    if (edgeIndex == 1) {
        indV1.push_back(v3);
        indV1.push_back(v0);
        indV1.push_back(middle_vertex);
        indV2.push_back(v2);
        indV2.push_back(v3);
        indV2.push_back(middle_vertex);
    }
     if (edgeIndex == 2) {
        indV1.push_back(v3);
        indV1.push_back(v0);
        indV1.push_back(middle_vertex);
        indV2.push_back(v0);
        indV2.push_back(v1);
        indV2.push_back(middle_vertex);
    } 
     if (edgeIndex == 3) {
         indV1.push_back(v2);   
         indV1.push_back(v3);
         indV1.push_back(middle_vertex);
		 indV2.push_back(v1);
         indV2.push_back(v2);
		 indV2.push_back(middle_vertex);
     }
    int f6 = MakeFace(indV1);
 //   auto pLine6 = std::make_unique<CPolyline>();
 //   MakePolyline(f6, *pLine6);
 //   pLine6->SetName("Face_f6");
 //   pDoc->AddObject(std::move(pLine6));

    int f7 = MakeFace(indV2);
 //   auto pLine7 = std::make_unique<CPolyline>();
 //   MakePolyline(f7, *pLine7);
 //   pLine7->SetName("Face_f7");
 //   pDoc->AddObject(std::move(pLine7));

    std::vector < size_t> F2indV1;
    std::vector < size_t> F2indV2;
    size_t f2v0 = faces_[f2].corners[0].v;
    size_t f2v1 = faces_[f2].corners[1].v;
    size_t f2v2 = faces_[f2].corners[2].v;
    size_t f2v3 = faces_[f2].corners[3].v;

    if (faces_[f2].edgeIndex == 0) {
        F2indV1.push_back(f2v3);
        F2indV1.push_back(f2v0);
        F2indV1.push_back(middle_vertex2);
        F2indV2.push_back(f2v2);
        F2indV2.push_back(f2v3);
        F2indV2.push_back(middle_vertex2);
	}
    if (faces_[f2].edgeIndex == 1) {
        F2indV1.push_back(f2v3);
        F2indV1.push_back(f2v0);
        F2indV1.push_back(middle_vertex2);
        F2indV2.push_back(f2v2);
        F2indV2.push_back(f2v3);
        F2indV2.push_back(middle_vertex2);
    }
    if (faces_[f2].edgeIndex == 2) {
        F2indV1.push_back(f2v0);
        F2indV1.push_back(f2v1);
        F2indV1.push_back(middle_vertex2);
        F2indV2.push_back(f2v3);
        F2indV2.push_back(f2v0);
        F2indV2.push_back(middle_vertex2);
	}
    if (faces_[f2].edgeIndex == 3) {
        F2indV1.push_back(f2v2);
        F2indV1.push_back(f2v3);
        F2indV1.push_back(middle_vertex2);
        F2indV2.push_back(f2v1);
        F2indV2.push_back(f2v2);
        F2indV2.push_back(middle_vertex2);
	}

   int f8 = MakeFace(F2indV1);
 /*    auto pLine8 = std::make_unique<CPolyline>();
    MakePolyline(f8, *pLine8);
    pLine8->SetName("Face_f8");
    pDoc->AddObject(std::move(pLine8));*/
    int f9 = MakeFace(F2indV2);
 //   auto pLine9 = std::make_unique<CPolyline>();
 //   MakePolyline(f9, *pLine9);
  //  pLine9->SetName("Face_f9");
  //  pDoc->AddObject(std::move(pLine9));

    if (edgeIndex == 0) {
        faces_[f1].corners[0] = middle;
    }
    if (edgeIndex == 1) {
       faces_[f1].corners[2] = middle;
    }
    if (edgeIndex == 2)
        faces_[f1].corners[0] = middle;

    if (edgeIndex == 3) {
        faces_[f1].corners[2] = middle;
    }
    faces_[f1].corners.resize(3);
    faces_[f1].m_Trimmed = true;

 //   auto pLine1 = std::make_unique<CPolyline>();
 //   MakePolyline(f1, *pLine1);
 //   pLine1->SetName("Face_f1");
 //  pDoc->AddObject(std::move(pLine1));


    if (faces_[f2].edgeIndex == 0)
		faces_[f2].corners[0] = middle2;
    if (faces_[f2].edgeIndex == 1)
        faces_[f2].corners[2] = middle2;
    if (faces_[f2].edgeIndex == 2)
		faces_[f2].corners[0] = middle2;
    if (faces_[f2].edgeIndex == 3)
        faces_[f2].corners[2] = middle2;

    faces_[f2].corners.resize(3);
    faces_[f2].m_Trimmed = true;

 //   auto pLine2 = std::make_unique<CPolyline>();
 //   MakePolyline(f2, *pLine2);
 //   pLine2->SetName("Face_f2");
 //   pDoc->AddObject(std::move(pLine2));

    return true;
}
bool CMesh3D::SplitFaceByVar6(int face_index, int v1, int edgeIndex, cVec2& pm)
{
    return true;
}
bool CMesh3D::SplitFaceByVar7(int f1, int v1, int edgeIndex)
{
  //  Step("SplitFaceByVar7");
    if (edgeIndex == -1)
        return false;
    MeshFace& face = faces_[f1];
    if (face.m_Trimmed)
        return true;
    if(face.corners.size() != 4)
        return false;
    CAlfaDoc* pDoc = GetAlfaDoc();
    pDoc->AddLayer("Faces");
//    auto pLine1 = std::make_unique<CPolyline>();
//    MakePolyline(f1, *pLine1);
 //   pLine1->SetName("Face_f1");
 //  pDoc->AddObject(std::move(pLine1));

    int ev1 = face.corners[edgeIndex].v;
    int VrtIndex2 = edgeIndex + 1;
    if (edgeIndex == 3)
        VrtIndex2 = 0;
    int ev2 = face.corners[VrtIndex2].v;

    Edge ed(ev1, ev2);
 //   int f1 = FindFirstFace3d(ed);
    int f2 = FindSecondCFace3d(f1, ed);
    if (f2 == -1)
        return true;
    MeshFace& face2 = faces_[f2];
    if (face2.corners.size() != 4)
        return false;
 
 //   auto pLine2 = std::make_unique<CPolyline>();
 //   MakePolyline(f2, *pLine2);
 //   pLine2->SetName("Face_f2");
 //   pDoc->AddObject(std::move(pLine2));
    
    int ind1 = v1;
    int ind2 = 0;
    if (edgeIndex == 0) {
        ind1 = 0;
        ind2 = 1;
    }
    if (edgeIndex == 3) {
        ind1 = 3;
        ind2 = 0;
    }
    if (edgeIndex == 2) {
        ind1 = 2;
        ind2 = 3;
    }
    if (edgeIndex == 1) {
        ind1 = 1;
        ind2 = 2;
    }

    int ind3 = 0;
    int ind4 = 0;
    if (edgeIndex == 0) {
        ind3 = 2;
        ind4 = 3;
    }
    if (edgeIndex == 3) {
        ind3 = 1;
        ind4 = 2;
    }
    if (edgeIndex == 2) {
        ind3 = 0;
        ind4 = 1;
    }
    if (edgeIndex == 1) {
        ind3 = 3;
        ind4 = 0;
    }

    int ind5 = 0;
    int ind6 = 0;
	int edgeIndex2 = face2.edgeIndex;
    if (edgeIndex2 == 0) {
        ind5 = 2;
        ind6 = 3;
    }
    if (edgeIndex2 == 3) {
        ind5 = 1;
        ind6 = 2;
    }
    if (edgeIndex2 == 2) {
        ind5 = 0;
        ind6 = 1;
    }
    if (edgeIndex2 == 1) {
        ind5 = 3;
        ind6 = 0;
    }
    bool MakeVar1 = true;
    if (v1 == ind4)
        MakeVar1 = false;

    std::vector<MeshCorner> seq4;
    seq4.push_back(face.corners[ind1]);
    seq4.push_back(face2.corners[ind6]);
    seq4.push_back(face.corners[ind3]);

    MeshFace face4 = face;
    face4.corners = std::move(seq4);
    face4.normal = FaceNormal(face4);
    faces_.push_back(std::move(face4));
    std::vector<MeshCorner> seq3;
    if (MakeVar1) {
        seq3.push_back(faces_[f2].corners[ind6]);
        seq3.push_back(faces_[f1].corners[ind2]);
        seq3.push_back(faces_[f1].corners[ind3]);
    }
    else {
        seq3.push_back(faces_[f2].corners[ind5]);
        seq3.push_back(faces_[f1].corners[ind2]);
        seq3.push_back(faces_[f1].corners[ind4]);
	}
    MeshFace face3 = faces_[f1];
    face3.corners = std::move(seq3);
    face3.normal = FaceNormal(face3);
    faces_.push_back(std::move(face3));

 //   face = faces_[face_index];
    if (MakeVar1) {
        if (edgeIndex == 0) {
            faces_[f1].corners[1] = faces_[f1].corners[2];
            faces_[f1].corners[2] = faces_[f1].corners[3];
        }
        if (edgeIndex == 3) {
            faces_[f1].corners[0] = faces_[f1].corners[3];
		}
        if (edgeIndex == 1) {
            faces_[f1].corners[2] = faces_[f1].corners[3];
		}
    }
    else
        {
        if (edgeIndex == 0) {
            faces_[f1].corners[0] = faces_[f1].corners[3];
        }
        if (edgeIndex == 1) {
            faces_[f1].corners[1] = faces_[f1].corners[2];
            faces_[f1].corners[2] = faces_[f1].corners[3];
        }
        if (edgeIndex == 2) {
            faces_[f1].corners[2] = faces_[f1].corners[3];
        }
	}

    faces_[f1].corners.resize(3);
	faces_[f1].m_Trimmed = true;

    
    if (MakeVar1) {
        if (edgeIndex == 0) {
            faces_[f2].corners[1] = faces_[f2].corners[2];
            faces_[f2].corners[2] = faces_[f2].corners[3];
        }
        if (edgeIndex == 1) {
            faces_[f2].corners[1] = faces_[f2].corners[2];
            faces_[f2].corners[2] = faces_[f2].corners[3];
		}
        if (edgeIndex == 2) 
			faces_[f2].corners[2] = faces_[f2].corners[3];
    }
    else {
         if (edgeIndex == 0) {
            faces_[f2].corners[1] = faces_[f2].corners[2];
            faces_[f2].corners[2] = faces_[f2].corners[3];
        }
        if (edgeIndex == 1) {
            faces_[f2].corners[2] = faces_[f2].corners[3];
        }
        if (edgeIndex == 3) {
            faces_[f2].corners[0] = faces_[f2].corners[3];
		}
	}
    faces_[f2].corners.resize(3);
    faces_[f2].m_Trimmed = true;
    return true;
}
bool CMesh3D::SplitFaceByVar8(int face_index, int vertexToMove, cVec2 moveTarget)
{
    if(vertexToMove == -1)
        return false;
    MeshFace& face = faces_[face_index];
	vertices_[face.corners[vertexToMove].v].x = moveTarget.x;
	vertices_[face.corners[vertexToMove].v].y = moveTarget.y;
    return true;
}

bool CMesh3D::SplitFaceByVar11(int f, int vi1, int vi2, std::vector<int> Pnt, CPolyline* pTrimLine)
{
  /* char bufer[120];
    sprintf(bufer, "SplitFaceByVar11  vi1= %d,  vi2= %d", vi1, vi2);
    Step(bufer);
    for (int i = 0; i < Pnt.size(); i++) {
        sprintf(bufer, "  p= %d,  ", Pnt[i]);
        Step(bufer);
    }*/ 
    
    if (Pnt.size() < 2)
        return false;
    CPolyline* pLine = new CPolyline;
    for (int j = 0; j < Pnt.size(); j++) {
        CPoint3d* p3d = pTrimLine->P(Pnt[j]);
        bool NeedAdd = true;
        for (int i = 0; i < pLine->np(); i++) {
            double dist = p3d->DistTo(pLine->P(i));
            if (dist < 0.00001) {
                NeedAdd = false;
                break;
            }
        }
        if (NeedAdd)
            pLine->AddPoint(p3d);
    }

    MeshFace& face = faces_[f];
    std::sort(Pnt.begin(), Pnt.end());

    std::vector<cVec2> P2ds;
    for (int i = 0; i < pLine->np(); i++) {
        cVec2 P2d;
        CPoint3d* p3d = pLine->P(i);
        if (p3d) {
            P2d.x = p3d->x;
            P2d.y = p3d->y;
            P2ds.push_back(P2d);
        }
    }
    if (P2ds.size() < 2) {
        if (P2ds.size() == 1) {
            cVec2 pm(P2ds[0].x, P2ds[0].y);
            return SplitFaceByPoint(f, vi1, vi2, pm);
        }
        return false;
    }


    if (vi1 > vi2)
        std::swap(vi1, vi2);
    int vi3 = 0;
    int vi4 = 0;
    if (vi1 == 0 && vi2 == 2) {
        vi3 = 1;
        vi4 = 3;
    }
    if (vi1 == 1 && vi2 == 3) {
        vi1 = 3;
        vi2 = 1;
        vi3 = 0;
        vi4 = 2;
    }
    
    size_t v1 = face.corners[vi1].v;
    size_t v2 = face.corners[vi2].v;
    size_t v3 = face.corners[vi3].v;
    size_t v4 = face.corners[vi4].v;

    CPoint3d* p1l = pLine->P(0);
    CPoint3d* p2l = pLine->P(1);


 //   CPoint3d p1f1(vertices_[face.corners[v1].v].x, vertices_[face.corners[v1].v].y, vertices_[face.corners[v1].v].z);
    CPoint3d p1f(vertices_[face.corners[vi1].v].x, vertices_[face.corners[vi1].v].y, vertices_[face.corners[vi1].v].z);

    double dist1 = p1f.DistTo(p1l);
    double dist2 = p1f.DistTo(p2l);
    if (dist1 > dist2)
        std::reverse(P2ds.begin(), P2ds.end());


    std::vector<size_t> vms;
    for (int i = 0; i < P2ds.size(); i++) {
        const cVec2 pm(P2ds[i].x, P2ds[i].y);
        const size_t vm = add_or_find_vertex_2d(vertices_, pm, 0.0001);
        vms.push_back(vm);
    }

    if (vi1 == 0 && vi2 == 2)
        face.corners[vi2].v = vms[0];
    else {
        face.corners[vi2].v = vms[0];
        face.corners[vi4].v = face.corners[vi1].v;
    }

    int f2 = MakeFace(v3, v2, vms[P2ds.size() - 1]);
    int f3 = MakeFace(v1, vms[0], v4);
    int f5 = MakeFace(v4, vms[P2ds.size() - 1], v2);

 /*   CAlfaDoc* pDoc = GetAlfaDoc();
    pDoc->AddLayer("Faces");
    auto pLine2 = std::make_unique<CPolyline>();
    MakePolyline(f2, *pLine2);
    pLine2->SetName("Face_f2");
    pDoc->AddObject(std::move(pLine2));
    auto pLine3 = std::make_unique<CPolyline>();
    MakePolyline(f3, *pLine3);
    pLine3->SetName("Face_f3");
    pDoc->AddObject(std::move(pLine3));
    auto pLine5 = std::make_unique<CPolyline>();
    MakePolyline(f5, *pLine5);
    pLine5->SetName("Face_f5");
    pDoc->AddObject(std::move(pLine5));
*/

    for (size_t i = 0; i < P2ds.size() - 1; i++) {
        int f4 = MakeFace(v4, vms[i], vms[i + 1]);
        int f1 = MakeFace(v3, vms[i + 1], vms[i]);

       /* auto pLine4 = std::make_unique<CPolyline>();
        MakePolyline(f4, *pLine4);
        pLine4->SetName("Face_f4");
        pDoc->AddObject(std::move(pLine4));
        auto pLine1 = std::make_unique<CPolyline>();
        MakePolyline(f1, *pLine1);
        pLine1->SetName("Face_f1");
        pDoc->AddObject(std::move(pLine1));
*/
    }

    faces_[f].corners.resize(3);

    delete pLine;
    return true;
}



int CMesh3D::MakeFace(std::vector < size_t> indV)
{
    if(indV.size() < 3)
        return -1;
    MeshFace face;
    std::vector<MeshCorner> seq;
    for (size_t vertex_index : indV) {
        if(vertex_index < 0)
            return -1;
        const size_t index = vertex_index;
        if(index >= vertices_.size())
			return -1;
        MeshCorner mc;
		mc.v = index;
        mc.uv = index;
        mc.n = index;
        seq.push_back(mc);
    }
	face.corners = std::move(seq);
    face.normal = FaceNormal(face);
    faces_.push_back(std::move(face));
	return static_cast<int>(faces_.size() - 1);
}

int CMesh3D::MakeFace(size_t ind1, size_t ind2, size_t ind3)
{
    std::vector < size_t> indV;
    indV.push_back(ind1);
    indV.push_back(ind2);
    indV.push_back(ind3);
    return MakeFace(indV);

}

MeshFace::MeshFace(std::initializer_list<size_t> vertex_indices) {
    corners.reserve(vertex_indices.size());
    for (size_t index : vertex_indices) {
        corners.push_back({index, index, index});
    }
}

CMesh3D::CMesh3D()
    : CAlfaObject("Mesh3D") {
    SetMaterial(colored_mesh_material());
    SetColor(kDefaultMeshObjectColor);
}

Material CMesh3D::material_Defailt = Material::DefaultMesh();
float CMesh3D::s_SurfaceOpacity = 1.0f;
MeshDisplayMode CMesh3D::s_DisplayMode = MeshDisplayMode::SurfaceGray;
bool CMesh3D::s_ZebraAnalysisEnabled = false;
bool CMesh3D::s_ZebraAnalysisTarget = false;
bool CMesh3D::s_ZebraOrthographic = false;
Vec3 CMesh3D::s_ZebraEye{};
Vec3 CMesh3D::s_ZebraForward{0.0f, 0.0f, -1.0f};
Vec3 CMesh3D::s_ZebraUp{0.0f, 1.0f, 0.0f};

CMesh3D::CMesh3D(std::string name)
    : CAlfaObject(std::move(name)) {
    SetMaterial(colored_mesh_material());
    SetColor(kDefaultMeshObjectColor);
}

const std::vector<Vec3>& CMesh3D::GetVertices() const {
    return vertices_;
}

std::vector<Vec3>& CMesh3D::GetVertices() {
    return vertices_;
}

const std::vector<CMesh3D::Face>& CMesh3D::GetFaces() const {
    return faces_;
}

std::vector<CMesh3D::Face>& CMesh3D::GetFaces() {
    return faces_;
}

const std::vector<UV>& CMesh3D::GetUVs() const {
    return uvs_;
}

const std::vector<Vec3>& CMesh3D::GetNormals() const {
    return normals_;
}

int CMesh3D::SynchronizeBoundaryVertices(const std::vector<Vec3>& master_points, float tolerance) {
    if (master_points.size() < 2 || vertices_.empty() || faces_.empty() || !(tolerance > 0.0f)) {
        return 0;
    }

    struct BoundaryEdge {
        size_t face_index = 0;
        size_t corner_index = 0;
        size_t first = 0;
        size_t second = 0;
    };
    const auto edge_key = [](size_t first, size_t second) {
        return std::minmax(first, second);
    };
    const auto distance_sq = [](Vec3 first, Vec3 second) {
        return dot(first - second, first - second);
    };

    int changes = 0;
    const float tolerance_sq = tolerance * tolerance;
    for (size_t master_index = 0; master_index < master_points.size(); ++master_index) {
        const Vec3 master = master_points[master_index];
        if (master_index + 1 == master_points.size()
            && distance_sq(master, master_points.front()) <= tolerance_sq * 0.01f) {
            continue;
        }

        std::map<std::pair<size_t, size_t>, std::vector<BoundaryEdge>> edge_uses;
        for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
            const Face& face = faces_[face_index];
            if (!IsValidFace(face, vertices_.size()))
                continue;
            for (size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
                const size_t first = face.corners[corner_index].v;
                const size_t second = face.corners[(corner_index + 1) % face.corners.size()].v;
                edge_uses[edge_key(first, second)].push_back(
                    {face_index, corner_index, first, second});
            }
        }

        std::vector<BoundaryEdge> boundary_edges;
        std::vector<size_t> boundary_vertices;
        for (const auto& entry : edge_uses) {
            if (entry.second.size() != 1)
                continue;
            const BoundaryEdge& edge = entry.second.front();
            boundary_edges.push_back(edge);
            boundary_vertices.push_back(edge.first);
            boundary_vertices.push_back(edge.second);
        }
        std::sort(boundary_vertices.begin(), boundary_vertices.end());
        boundary_vertices.erase(
            std::unique(boundary_vertices.begin(), boundary_vertices.end()),
            boundary_vertices.end());

        size_t nearest_vertex = vertices_.size();
        float nearest_vertex_dist_sq = std::numeric_limits<float>::max();
        for (size_t vertex_index : boundary_vertices) {
            const float dist_sq = distance_sq(master, vertices_[vertex_index]);
            if (dist_sq < nearest_vertex_dist_sq) {
                nearest_vertex_dist_sq = dist_sq;
                nearest_vertex = vertex_index;
            }
        }
        if (nearest_vertex < vertices_.size() && nearest_vertex_dist_sq <= tolerance_sq * 0.16f) {
            vertices_[nearest_vertex] = master;
            ++changes;
            continue;
        }

        size_t nearest_edge_index = boundary_edges.size();
        float nearest_edge_dist_sq = std::numeric_limits<float>::max();
        float nearest_edge_alpha = 0.0f;
        for (size_t edge_index = 0; edge_index < boundary_edges.size(); ++edge_index) {
            const BoundaryEdge& edge = boundary_edges[edge_index];
            const Vec3 first = vertices_[edge.first];
            const Vec3 second = vertices_[edge.second];
            const Vec3 direction = second - first;
            const float length_sq = dot(direction, direction);
            if (length_sq <= 1.0e-20f)
                continue;
            const float alpha = std::clamp(dot(master - first, direction) / length_sq, 0.0f, 1.0f);
            const float dist_sq = distance_sq(master, first + direction * alpha);
            if (dist_sq < nearest_edge_dist_sq) {
                nearest_edge_dist_sq = dist_sq;
                nearest_edge_index = edge_index;
                nearest_edge_alpha = alpha;
            }
        }
        if (nearest_edge_index >= boundary_edges.size()
            || nearest_edge_dist_sq > tolerance_sq
            || nearest_edge_alpha <= 1.0e-4f
            || nearest_edge_alpha >= 1.0f - 1.0e-4f) {
            continue;
        }

        const BoundaryEdge edge = boundary_edges[nearest_edge_index];
        if (edge.face_index >= faces_.size())
            continue;
        Face& face = faces_[edge.face_index];
        if (!IsValidFace(face, vertices_.size()) || edge.corner_index >= face.corners.size())
            continue;

        const size_t old_vertex_count = vertices_.size();
        const MeshCorner first_corner = face.corners[edge.corner_index];
        const MeshCorner second_corner = face.corners[(edge.corner_index + 1) % face.corners.size()];
        vertices_.push_back(master);

        size_t uv_index = 0;
        if (uvs_.size() == old_vertex_count
            && first_corner.uv < uvs_.size() && second_corner.uv < uvs_.size()) {
            const UV& first_uv = uvs_[first_corner.uv];
            const UV& second_uv = uvs_[second_corner.uv];
            uvs_.push_back({
                first_uv.u + (second_uv.u - first_uv.u) * nearest_edge_alpha,
                first_uv.v + (second_uv.v - first_uv.v) * nearest_edge_alpha
            });
            uv_index = uvs_.size() - 1;
        }

        size_t normal_index = 0;
        if (normals_.size() == old_vertex_count
            && first_corner.n < normals_.size() && second_corner.n < normals_.size()) {
            normals_.push_back(normalize(
                normals_[first_corner.n] * (1.0f - nearest_edge_alpha)
                + normals_[second_corner.n] * nearest_edge_alpha));
            normal_index = normals_.size() - 1;
        }
        const MeshCorner inserted{vertices_.size() - 1, uv_index, normal_index};

        std::vector<MeshCorner> rotated;
        rotated.reserve(face.corners.size());
        for (size_t i = 0; i < face.corners.size(); ++i)
            rotated.push_back(face.corners[(edge.corner_index + i) % face.corners.size()]);

        Face second_face = face;
        face.corners = {rotated.front(), inserted, rotated.back()};
        second_face.corners.clear();
        second_face.corners.push_back(inserted);
        second_face.corners.insert(second_face.corners.end(), rotated.begin() + 1, rotated.end());
        face.normal = FaceNormal(face);
        second_face.normal = FaceNormal(second_face);
        faces_.push_back(std::move(second_face));
        ++changes;
    }
    return changes;
}

size_t CMesh3D::GetFaceVertexIndex(const Face& face, size_t i) {
    return face.corners[i].v;
}

void CMesh3D::SetFaceVertexIndex(Face& face, size_t i, size_t v) {
    face.corners[i].v = v;
}

size_t CMesh3D::FaceVertexCount(const Face& face) {
    return face.corners.size();
}

bool CMesh3D::RestoreTo3DFromUVSurface(CSurfaceFace* surface)
{
    if (!surface || vertices_.empty()) {
        return false;
    }

    if (uvs_.size() != vertices_.size()) {
        uvs_.assign(vertices_.size(), {});
    }
    if (normals_.size() != vertices_.size()) {
        normals_.assign(vertices_.size(), {});
    }

    for (size_t i = 0; i < vertices_.size(); ++i) {
        const UV surface_uv{vertices_[i].x, vertices_[i].y};
        CPoint8d point;
        if (!surface->GetPoint(surface_uv.u, surface_uv.v, &point)) {
            return false;
        }
        uvs_[i] = surface_uv;
        vertices_[i] = {
            static_cast<float>(point.x),
            static_cast<float>(point.y),
            static_cast<float>(point.z)
        };
        normals_[i] = normalize({
            static_cast<float>(point.l),
            static_cast<float>(point.m),
            static_cast<float>(point.n)
        });
    }
    for (Face& face : faces_) {
        for (MeshCorner& corner : face.corners) {
            corner.uv = corner.v;
            corner.n = corner.v;
        }
        face.normal = FaceNormal(face);
    }
    return true;
}

bool CMesh3D::CreateFromBoundary(CPolyline* bond, float Density)
{
    Clear();
    if (!bond || bond->GetPointCount() < 3) {
        return false;
    }

    const float grid_step = std::max(Density, 0.0001f);
    const std::vector<CPoint3d>& source_points = bond->GetPoints();
    std::vector<Vec3> contour;
    contour.reserve(source_points.size());

    const auto to_vec3 = [](const CPoint3d& point) {
        return Vec3{
            static_cast<float>(point.x),
            static_cast<float>(point.y),
            static_cast<float>(point.z)
        };
    };
    const auto append_point = [&contour](Vec3 point) {
        if (contour.empty() || dot(point - contour.back(), point - contour.back()) > 0.00000001f) {
            contour.push_back(point);
        }
    };

    const size_t point_count = source_points.size();
    for (size_t i = 0; i < point_count; ++i) {
        append_point(to_vec3(source_points[i]));
    }

    if (contour.size() < 3) {
        Clear();
        return false;
    }

    struct GridPoint {
        double x = 0.0;
        double y = 0.0;
    };

    double average_z = 0.0;
    std::vector<GridPoint> polygon;
    polygon.reserve(contour.size());
    for (Vec3 point : contour) {
        polygon.push_back({point.x, point.y});
        average_z += point.z;
    }
    average_z /= static_cast<double>(contour.size());

    double polygon_area = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const GridPoint& a = polygon[i];
        const GridPoint& b = polygon[(i + 1) % polygon.size()];
        polygon_area += a.x * b.y - b.x * a.y;
    }
    Vec3 normal = polygon_area >= 0.0 ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 0.0f, -1.0f};
    if (polygon_area < 0.0) {
        std::reverse(polygon.begin(), polygon.end());
        std::reverse(contour.begin(), contour.end());
        normal = {0.0f, 0.0f, 1.0f};
    }

    double min_x = polygon.front().x;
    double max_x = polygon.front().x;
    double min_y = polygon.front().y;
    double max_y = polygon.front().y;
    for (const GridPoint& point : polygon) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }
    if (max_x - min_x <= 0.000001 || max_y - min_y <= 0.000001) {
        Clear();
        return false;
    }

    const double eps = std::max<double>(grid_step * 0.00001, 0.000001);
    const auto add_sorted_unique = [](std::vector<double>& values, double value, double merge_eps) {
        values.push_back(value);
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end(), [merge_eps](double a, double b) {
            return std::fabs(a - b) <= merge_eps;
        }), values.end());
    };

    std::vector<double> x_lines;
    std::vector<double> y_lines;
    add_sorted_unique(x_lines, min_x, eps);
    add_sorted_unique(x_lines, max_x, eps);
    add_sorted_unique(y_lines, min_y, eps);
    add_sorted_unique(y_lines, max_y, eps);
    for (double x = std::ceil(min_x / grid_step) * grid_step; x < max_x; x += grid_step) {
        if (x > min_x + eps)
            add_sorted_unique(x_lines, x, eps);
    }
    for (double y = std::ceil(min_y / grid_step) * grid_step; y < max_y; y += grid_step) {
        if (y > min_y + eps)
            add_sorted_unique(y_lines, y, eps);
    }

    const auto add_unique_point = [&](std::vector<GridPoint>& points, GridPoint point) {
        for (const GridPoint& existing : points) {
            const double dx = existing.x - point.x;
            const double dy = existing.y - point.y;
            if (dx * dx + dy * dy <= eps * eps) {
                return;
            }
        }
        points.push_back(point);
    };

    const auto clipped_cell = [&](double x0, double y0, double x1, double y1) {
        enum class ClipSide {
            Left,
            Right,
            Bottom,
            Top
        };

        const auto inside = [&](const GridPoint& point, ClipSide side) {
            switch (side) {
            case ClipSide::Left:
                return point.x >= x0 - eps;
            case ClipSide::Right:
                return point.x <= x1 + eps;
            case ClipSide::Bottom:
                return point.y >= y0 - eps;
            case ClipSide::Top:
                return point.y <= y1 + eps;
            }
            return false;
        };

        const auto intersection = [&](const GridPoint& a, const GridPoint& b, ClipSide side) {
            GridPoint result = a;
            const double dx = b.x - a.x;
            const double dy = b.y - a.y;
            if (side == ClipSide::Left || side == ClipSide::Right) {
                const double x = side == ClipSide::Left ? x0 : x1;
                const double t = std::fabs(dx) <= eps ? 0.0 : (x - a.x) / dx;
                result.x = x;
                result.y = a.y + dy * std::clamp(t, 0.0, 1.0);
            } else {
                const double y = side == ClipSide::Bottom ? y0 : y1;
                const double t = std::fabs(dy) <= eps ? 0.0 : (y - a.y) / dy;
                result.x = a.x + dx * std::clamp(t, 0.0, 1.0);
                result.y = y;
            }
            return result;
        };

        const auto clip_side = [&](const std::vector<GridPoint>& input, ClipSide side) {
            std::vector<GridPoint> output;
            if (input.empty()) {
                return output;
            }
            GridPoint previous = input.back();
            bool previous_inside = inside(previous, side);
            for (const GridPoint& current : input) {
                const bool current_inside = inside(current, side);
                if (current_inside) {
                    if (!previous_inside) {
                        add_unique_point(output, intersection(previous, current, side));
                    }
                    add_unique_point(output, current);
                } else if (previous_inside) {
                    add_unique_point(output, intersection(previous, current, side));
                }
                previous = current;
                previous_inside = current_inside;
            }
            return output;
        };

        std::vector<GridPoint> clipped = polygon;
        clipped = clip_side(clipped, ClipSide::Left);
        clipped = clip_side(clipped, ClipSide::Right);
        clipped = clip_side(clipped, ClipSide::Bottom);
        clipped = clip_side(clipped, ClipSide::Top);
        return clipped;
    };

    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    std::vector<Vec3> normals;
    std::map<std::pair<long long, long long>, size_t> vertex_map;
    const double key_scale = 1000000.0;
    const auto add_vertex = [&](double x, double y) {
        const auto key = std::make_pair(
            static_cast<long long>(std::llround(x * key_scale)),
            static_cast<long long>(std::llround(y * key_scale)));
        const auto found = vertex_map.find(key);
        if (found != vertex_map.end()) {
            return found->second;
        }
        const Vec3 point{static_cast<float>(x), static_cast<float>(y), static_cast<float>(average_z)};
        const size_t index = vertices.size();
        vertices.push_back(point);
        normals.push_back(normal);
        vertex_map[key] = index;
        return index;
    };

    const auto add_clipped_face = [&](std::vector<GridPoint> clipped) {
        if (clipped.size() < 3) {
            return;
        }
        double area = 0.0;
        for (size_t i = 0; i < clipped.size(); ++i) {
            const GridPoint& a = clipped[i];
            const GridPoint& b = clipped[(i + 1) % clipped.size()];
            area += a.x * b.y - b.x * a.y;
        }
        if (std::fabs(area) <= eps * eps) {
            return;
        }
        if (area < 0.0) {
            std::reverse(clipped.begin(), clipped.end());
        }

        Face face;
        face.corners.reserve(clipped.size());
        for (const GridPoint& point : clipped) {
            const size_t vertex = add_vertex(point.x, point.y);
            face.corners.push_back({vertex, vertex, vertex});
        }
        faces.push_back(std::move(face));
    };

    for (size_t row = 0; row + 1 < y_lines.size(); ++row) {
        const double y0 = y_lines[row];
        const double y1 = y_lines[row + 1];
        if (y1 - y0 <= eps)
            continue;
        for (size_t column = 0; column + 1 < x_lines.size(); ++column) {
            const double x0 = x_lines[column];
            const double x1 = x_lines[column + 1];
            if (x1 - x0 <= eps)
                continue;
            std::vector<GridPoint> clipped = clipped_cell(x0, y0, x1, y1);
            if (clipped.size() < 3)
                continue;
            add_clipped_face(std::move(clipped));
        }
    }

    if (!vertices.empty() && !faces.empty() && SetGeometry(std::move(vertices), std::move(faces), {}, std::move(normals))) {
        return true;
    }

    CMesh3D triangle_mesh;
    if (!FillContorByTriangles(&triangle_mesh, contour, normal)) {
        Clear();
        return false;
    }
    ContourQuadrangulator quadrangulator;
    quadrangulator.CreateFromMesh(&triangle_mesh);
    quadrangulator.Quadrangulate(this);
    return !faces_.empty();
}

bool CMesh3D::PutOnSurface(CSurfaceFace* surface) {
    if (!surface || vertices_.empty() || faces_.empty()) {
        return false;
    }

    SurfaceUVMapping mapping(surface);
    if (!mapping.IsValid()) {
        return false;
    }

    std::vector<SurfaceUVPoint> projected(vertices_.size());
    for (size_t i = 0; i < vertices_.size(); ++i) {
        if (!mapping.Project(vertices_[i], projected[i])) {
            return false;
        }
    }

    using VertexKey = std::tuple<size_t, long long, long long>;
    constexpr double kUvKeyScale = 1000000000.0;
    std::map<VertexKey, size_t> vertex_map;
    std::vector<Vec3> uv_vertices;
    std::vector<UV> uv_coordinates;
    std::vector<Face> uv_faces = faces_;
    std::vector<SurfaceUVPoint> vertex_references(vertices_.size());
    std::vector<bool> has_vertex_reference(vertices_.size(), false);
    uv_vertices.reserve(vertices_.size());
    uv_coordinates.reserve(vertices_.size());

    for (Face& face : uv_faces) {
        if (face.corners.empty()) {
            continue;
        }

        size_t anchor = 0;
        for (size_t i = 0; i < face.corners.size(); ++i) {
            const size_t source_index = face.corners[i].v;
            if (source_index >= projected.size()) {
                return false;
            }
            if (has_vertex_reference[source_index]) {
                anchor = i;
                break;
            }
        }

        std::vector<SurfaceUVPoint> face_uvs(face.corners.size());
        const size_t anchor_vertex = face.corners[anchor].v;
        face_uvs[anchor] = projected[anchor_vertex];
        if (has_vertex_reference[anchor_vertex]) {
            face_uvs[anchor] = mapping.UnwrapNear(face_uvs[anchor], vertex_references[anchor_vertex]);
        }
        for (size_t step = 1; step < face.corners.size(); ++step) {
            const size_t previous_corner = (anchor + step - 1) % face.corners.size();
            const size_t current_corner = (anchor + step) % face.corners.size();
            const size_t source_index = face.corners[current_corner].v;
            face_uvs[current_corner] = mapping.UnwrapNear(projected[source_index], face_uvs[previous_corner]);
        }

        for (size_t i = 0; i < face.corners.size(); ++i) {
            MeshCorner& corner = face.corners[i];
            const size_t source_index = corner.v;
            const SurfaceUVPoint uv = face_uvs[i];
            if (!has_vertex_reference[source_index]) {
                vertex_references[source_index] = uv;
                has_vertex_reference[source_index] = true;
            }

            const VertexKey key{
                source_index,
                std::llround(uv.u * kUvKeyScale),
                std::llround(uv.v * kUvKeyScale)
            };
            auto [it, inserted] = vertex_map.emplace(key, uv_vertices.size());
            if (inserted) {
                uv_vertices.push_back({
                    static_cast<float>(uv.u),
                    static_cast<float>(uv.v),
                    0.0f
                });
                uv_coordinates.push_back({
                    static_cast<float>(uv.u),
                    static_cast<float>(uv.v)
                });
            }

            corner.v = it->second;
            corner.n = 0;
            corner.uv = it->second;
        }
    }

    return SetGeometry(std::move(uv_vertices),
                       std::move(uv_faces),
                       std::move(uv_coordinates),
                       {});
}

bool CMesh3D::ExportToObj(const std::string& name) const {
    if (name.empty() || vertices_.empty()) {
        return false;
    }

    std::ofstream stream(obj_output_path(name), std::ios::out | std::ios::trunc);
    if (!stream) {
        return false;
    }

    stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    stream << "# Dom3D Pro CMesh3D export\n";
    stream << "o " << (GetName().empty() ? "Mesh3D" : GetName()) << "\n";
    for (const Vec3& vertex : vertices_) {
        stream << "v " << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
    }
    for (const UV& uv : uvs_) {
        stream << "vt " << uv.u << " " << uv.v << "\n";
    }
    for (const Vec3& normal : normals_) {
        stream << "vn " << normal.x << " " << normal.y << " " << normal.z << "\n";
    }

    for (const Face& face : faces_) {
        if (face.deleted || face.corners.size() < 3) {
            continue;
        }

        stream << "f";
        for (const MeshCorner& corner : face.corners) {
            if (corner.v >= vertices_.size()
                || (!uvs_.empty() && corner.uv >= uvs_.size())
                || (!normals_.empty() && corner.n >= normals_.size())) {
                return false;
            }

            stream << " " << corner.v + 1;
            if (!uvs_.empty() || !normals_.empty()) {
                stream << "/";
                if (!uvs_.empty()) {
                    stream << corner.uv + 1;
                }
                if (!normals_.empty()) {
                    stream << "/" << corner.n + 1;
                }
            }
        }
        stream << "\n";
    }

    return static_cast<bool>(stream);
}

bool CMesh3D::SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces) {
    return SetGeometry(std::move(vertices), std::move(faces), {}, {});
}

bool CMesh3D::SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces, std::vector<UV> uvs) {
    return SetGeometry(std::move(vertices), std::move(faces), std::move(uvs), {});
}

bool CMesh3D::SetGeometry(std::vector<Vec3> vertices,
                          std::vector<Face> faces,
                          std::vector<UV> uvs,
                          std::vector<Vec3> normals) {
    if (vertices.empty() || faces.empty()) {
        return false;
    }

    for (const Face& face : faces) {
        if (face.deleted) {
            continue;
        }
        if (!IsValidFace(face, vertices.size())) {
            return false;
        }
        for (const MeshCorner& corner : face.corners) {
            if ((!uvs.empty() && corner.uv >= uvs.size())
                || (!normals.empty() && corner.n >= normals.size())) {
                return false;
            }
        }
    }

    vertices_ = std::move(vertices);
    faces_ = std::move(faces);
    uvs_ = std::move(uvs);
    normals_ = std::move(normals);
    if (uvs_.empty()) {
        GeneratePlanarUVs();
    }
    if (!normals_.empty()) {
        for (Vec3& normal : normals_) {
            normal = normalize(normal);
        }
    }
    for (Face& face : faces_) {
        face.normal = FaceNormal(face);
    }
    return true;
}

void CMesh3D::GeneratePlanarUVs() {
    uvs_.clear();
    uvs_.resize(vertices_.size());
    if (vertices_.empty()) {
        return;
    }

    float min_x = vertices_.front().x;
    float max_x = vertices_.front().x;
    float min_z = vertices_.front().z;
    float max_z = vertices_.front().z;
    for (const Vec3& vertex : vertices_) {
        min_x = std::min(min_x, vertex.x);
        max_x = std::max(max_x, vertex.x);
        min_z = std::min(min_z, vertex.z);
        max_z = std::max(max_z, vertex.z);
    }

    const float width = std::max(max_x - min_x, 0.0001f);
    const float depth = std::max(max_z - min_z, 0.0001f);
    for (size_t i = 0; i < vertices_.size(); ++i) {
        uvs_[i].u = (vertices_[i].x - min_x) / width;
        uvs_[i].v = (vertices_[i].z - min_z) / depth;
    }
    for (Face& face : faces_) {
        for (MeshCorner& corner : face.corners) {
            corner.uv = corner.v;
        }
    }
}

void CMesh3D::Render() {
    Render3d(false);
}

void CMesh3D::Render3d(bool selected) const {
    const MeshDisplayMode mode = GetDisplayMode();
    const bool zebra = IsZebraAnalysisTarget();
    const SolidDisplayMode solid_mode = zebra
        ? SolidDisplayMode::SurfacesAndEdges
        : CSolid::GetDisplayMode();
    if (solid_mode == SolidDisplayMode::HiddenLine) {
        const Color background = CSolid::GetHiddenLineBackgroundColor();
        RenderHiddenLineDepth(background);
        RenderHiddenLineEdges(false, background, selected);
        return;
    }

    const bool solid_requests_fill = solid_mode == SolidDisplayMode::SurfacesAndEdges
        || solid_mode == SolidDisplayMode::SurfacesAndRaisedMesh;
    const bool wire_only = solid_mode == SolidDisplayMode::Wireframe
        || solid_mode == SolidDisplayMode::MeshOnly;
    if (!zebra && mode == MeshDisplayMode::Wire && !solid_requests_fill) {
        RenderWire(selected, true, nullptr);
        return;
    }
    if (wire_only) {
        RenderWire(selected, true, nullptr);
        return;
    }

    Material material = mode == MeshDisplayMode::SurfaceGray ? material_Defailt : GetMaterial();
    if (mode == MeshDisplayMode::SurfaceColored) {
        material.color_texture_path.clear();
        material.light_texture_path.clear();
        material.bump_texture_path.clear();
    }

    // A standalone imported mesh has no separate BRep edge collection: its
    // polygon edges are the equivalent of Solid edges.  Therefore the Solid
    // display flyout must explicitly control them too.
    const bool draw_edges = mode != MeshDisplayMode::SurfaceMaterial
        || solid_mode == SolidDisplayMode::SurfacesAndEdges
        || solid_mode == SolidDisplayMode::SurfacesAndRaisedMesh;
    RenderFaces(selected, draw_edges, &material,
                mode == MeshDisplayMode::SurfaceColored);
    if (draw_edges) {
        RenderWire(selected, true, nullptr);
    }
}

void CMesh3D::RenderFaces(bool selected,
                          bool offset_fill,
                          const Material* material_override,
                          bool diagnostic_rgb,
                          bool flat_color) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Material material = material_override ? *material_override : GetMaterial();
    const Color color = material.diffuse;
    const float specular_strength = std::clamp(material.specular <= 0.0f ? 0.18f : material.specular, 0.0f, 1.0f);
    const float shininess = std::clamp(material.shininess <= 0.0f ? 36.0f : material.shininess, 4.0f, 96.0f);
    const float material_alpha = selected
        ? std::min(material.alpha + 0.04f, 1.0f)
        : material.alpha;
    const bool zebra = IsZebraAnalysisTarget() && !flat_color;
    const float alpha = zebra
        ? 1.0f
        : flat_color
        ? std::clamp(material_alpha, 0.0f, 1.0f)
        : std::clamp(material_alpha * s_SurfaceOpacity, 0.0f, 1.0f);
    // Selection is rendered in diagnostic RGB by surface normal.  A bound
    // color texture would modulate (and usually hide) those RGB colors, so it
    // must be suppressed for this draw pass without changing the material.
    const GLuint color_texture = (zebra || selected || diagnostic_rgb)
        ? 0
        : texture_id_for_path(material.color_texture_path);
    const bool has_texture = color_texture != 0;
    UV texture_uv_min{};
    UV texture_uv_max{};
    if (material.texture_fit_to_surface && !uvs_.empty()) {
        texture_uv_min = uvs_.front();
        texture_uv_max = uvs_.front();
        for (const UV& uv : uvs_) {
            texture_uv_min.u = std::min(texture_uv_min.u, uv.u);
            texture_uv_min.v = std::min(texture_uv_min.v, uv.v);
            texture_uv_max.u = std::max(texture_uv_max.u, uv.u);
            texture_uv_max.v = std::max(texture_uv_max.v, uv.v);
        }
    }

    std::vector<Vec3> vertex_normals;
    if (normals_.size() == vertices_.size()) {
        vertex_normals = normals_;
    } else {
        vertex_normals.assign(vertices_.size(), {0.0f, 0.0f, 0.0f});
        for (const Face& face : faces_) {
            if (!IsValidFace(face, vertices_.size())) {
                continue;
            }
            const Vec3 normal = FaceNormal(face);
            for (const MeshCorner& corner : face.corners) {
                const size_t index = corner.v;
                vertex_normals[index] = vertex_normals[index] + normal;
            }
        }
        for (Vec3& normal : vertex_normals) {
            normal = normalize(normal);
            if (std::fabs(normal.x) <= 0.00001f && std::fabs(normal.y) <= 0.00001f && std::fabs(normal.z) <= 0.00001f) {
                normal = {0.0f, 1.0f, 0.0f};
            }
        }
    }

    if (alpha < 0.999f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    glDisable(GL_LIGHTING);
    QOpenGLShaderProgram* zebra_program =
        zebra ? zebra_shader_program() : nullptr;
    const bool zebra_shader_active =
        zebra_program && zebra_program->bind();
    if (zebra_shader_active) {
        zebra_program->setUniformValue("zebraStripeCount", 10.0f);
    } else if (zebra) {
        glEnable(GL_TEXTURE_1D);
        glBindTexture(GL_TEXTURE_1D, zebra_texture_id());
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    } else if (has_texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
            material.texture_fit_to_surface ? kGlClampToEdge : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
            material.texture_fit_to_surface ? kGlClampToEdge : GL_REPEAT);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    if (offset_fill) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.55f, 0.55f);
    }

    const GLboolean cull_face_was_enabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glBegin(GL_TRIANGLES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }
        const Vec3 face_normal = FaceNormal(face);

        for (size_t i = 1; i + 1 < face.corners.size(); ++i) {
            const MeshCorner corners[] = {face.corners[0], face.corners[i], face.corners[i + 1]};
            for (const MeshCorner& corner : corners) {
                const size_t vertex_index = corner.v;
                const Vec3& normal = normals_.empty() || corner.n >= normals_.size()
                    ? vertex_normals[vertex_index]
                    : normals_[corner.n];
                const Color shade = zebra
                    ? Color{1.0f, 1.0f, 1.0f}
                    : flat_color
                    ? color
                    : (((selected || diagnostic_rgb) && !has_texture)
                        ? normal_rgb_color(normal, selected)
                        : shaded_color(
                            color,
                            normal,
                            s_ZebraOrthographic
                                ? s_ZebraForward * -1.0f
                                : normalize(s_ZebraEye - vertices_[vertex_index]),
                            s_ZebraUp,
                            specular_strength,
                            shininess,
                            selected));
                const Vec3& vertex = vertices_[vertex_index];
                glNormal3f(normal.x, normal.y, normal.z);
                glColor4f(shade.r, shade.g, shade.b, alpha);
                if (zebra && !zebra_shader_active) {
                    const Vec3 unit_normal = normalize(normal);
                    const Vec3 direction_to_eye = s_ZebraOrthographic
                        ? s_ZebraForward * -1.0f
                        : normalize(s_ZebraEye - vertex);
                    const Vec3 reflection = normalize(
                        unit_normal
                            * (2.0f * dot(unit_normal, direction_to_eye))
                        - direction_to_eye);
                    constexpr float kStripeCount = 10.0f;
                    const float reflection_height =
                        std::clamp(dot(reflection, s_ZebraUp), -1.0f, 1.0f);
                    glTexCoord1f(
                        (reflection_height * 0.5f + 0.5f) * kStripeCount);
                } else if (has_texture) {
                    UV base_uv = corner.uv < uvs_.size()
                        ? uvs_[corner.uv]
                        : projected_uv_for_face(vertex, face_normal);
                    if (material.texture_fit_to_surface && !uvs_.empty()) {
                        base_uv = fit_uv_to_bounds(base_uv, texture_uv_min, texture_uv_max);
                    }
                    const UV uv = transform_uv(base_uv, material);
                    glTexCoord2f(uv.u, uv.v);
                }
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
        }
    }
    glEnd();
    if (cull_face_was_enabled) {
        glEnable(GL_CULL_FACE);
    }

    if (zebra_shader_active) {
        zebra_program->release();
    } else if (zebra) {
        glBindTexture(GL_TEXTURE_1D, 0);
        glDisable(GL_TEXTURE_1D);
    } else if (has_texture) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
    if (offset_fill) {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (alpha < 0.999f) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void CMesh3D::RenderWire(bool selected,
                         bool draw_on_top,
                         const Color* color_override,
                         bool hidden) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Color base_color = color_override ? *color_override : GetColor();
    const MeshDisplayMode mode = GetDisplayMode();

    GLboolean depth_write_enabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_enabled);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(hidden ? GL_GREATER : GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(draw_on_top ? (selected ? 1.50f : 1.28f)
                            : (selected ? 1.20f : 1.05f));
    if (hidden) {
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x0F0F);
    }
    const Color wire = color_override ? base_color : wire_color(base_color, mode, selected);
    const float alpha = draw_on_top
        ? (selected ? 0.98f : 0.92f)
        : (selected ? 0.88f : 0.82f);
    glColor4f(wire.r, wire.g, wire.b, alpha);

    std::unordered_set<unsigned long long> drawn_edges;
    drawn_edges.reserve(faces_.size() * 3);

    glBegin(GL_LINES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        for (size_t i = 0; i < face.corners.size(); ++i) {
            const size_t a_index = face.corners[i].v;
            const size_t b_index = face.corners[(i + 1) % face.corners.size()].v;
            const size_t edge_min = std::min(a_index, b_index);
            const size_t edge_max = std::max(a_index, b_index);
            const unsigned long long key = (static_cast<unsigned long long>(edge_min) << 32)
                | static_cast<unsigned long long>(edge_max);
            if (!drawn_edges.insert(key).second) {
                continue;
            }

            const Vec3& a = vertices_[a_index];
            const Vec3& b_vertex = vertices_[b_index];
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b_vertex.x, b_vertex.y, b_vertex.z);
        }
    }
    glEnd();

    if (hidden) {
        glDisable(GL_LINE_STIPPLE);
    }
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDepthMask(depth_write_enabled);
}

void CMesh3D::RenderHiddenLineDepth(const Color& background) const {
    Material material;
    material.diffuse = background;
    material.alpha = 1.0f;
    material.specular = 0.0f;
    material.shininess = 4.0f;
    // Bias the depth fill behind coplanar boundary lines so a visible line
    // cannot leak into the dashed hidden-line pass through depth precision.
    RenderFaces(false, true, &material, false, true);
}

void CMesh3D::RenderHiddenLineEdges(bool hidden,
                                    const Color& background,
                                    bool selected) const {
    const float luminance = background.r * 0.2126f
        + background.g * 0.7152f + background.b * 0.0722f;
    const Color line_color = hidden
        ? (luminance > 0.5f ? Color{0.62f, 0.62f, 0.62f}
                            : Color{0.42f, 0.42f, 0.42f})
        : wire_color(GetColor(), GetDisplayMode(), selected);
    RenderWire(selected, false, &line_color, hidden);
}

float CMesh3D::GetSurfaceOpacity() {
    return s_SurfaceOpacity;
}

void CMesh3D::SetSurfaceOpacity(float opacity) {
    s_SurfaceOpacity = std::clamp(opacity, 0.0f, 1.0f);
}

MeshDisplayMode CMesh3D::GetDisplayMode() {
    return s_DisplayMode;
}

void CMesh3D::SetDisplayMode(MeshDisplayMode mode) {
    s_DisplayMode = mode;
}

bool CMesh3D::IsZebraAnalysisEnabled() {
    return s_ZebraAnalysisEnabled;
}

bool CMesh3D::IsZebraAnalysisTarget() {
    return s_ZebraAnalysisEnabled && s_ZebraAnalysisTarget;
}

void CMesh3D::SetZebraAnalysisEnabled(bool enabled) {
    s_ZebraAnalysisEnabled = enabled;
    if (!enabled) {
        s_ZebraAnalysisTarget = false;
    }
}

void CMesh3D::SetZebraAnalysisTarget(bool target) {
    s_ZebraAnalysisTarget = target;
}

void CMesh3D::SetZebraAnalysisView(Vec3 eye,
                                   Vec3 forward,
                                   Vec3 up,
                                   bool orthographic) {
    s_ZebraEye = eye;
    s_ZebraForward = normalize(forward);
    s_ZebraUp = normalize(up);
    s_ZebraOrthographic = orthographic;
}

void CMesh3D::Render2d(float center_x, float center_y, float scale) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Color color = GetColor();
    glLineWidth(1.5f);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        for (size_t i = 0; i < face.corners.size(); ++i) {
            const Vec3& a = vertices_[face.corners[i].v];
            const Vec3& b = vertices_[face.corners[(i + 1) % face.corners.size()].v];
            glVertex2f(center_x + a.x * scale, center_y + a.z * scale);
            glVertex2f(center_x + b.x * scale, center_y + b.z * scale);
        }
    }
    glEnd();
}

bool CMesh3D::HitTest(CurvePoint point, float tolerance) const {
    bool has_projected_face_hit = false;
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        if (PointInFace2d(point, face)) {
            has_projected_face_hit = true;
        }

        for (size_t i = 0; i < face.corners.size(); ++i) {
            const Vec3& a = vertices_[face.corners[i].v];
            const Vec3& b = vertices_[face.corners[(i + 1) % face.corners.size()].v];
            if (DistanceToSegment2d(point, a, b) <= tolerance) {
                return true;
            }
        }
    }

    if (has_projected_face_hit) {
        return true;
    }

    Vec3 min_point{};
    Vec3 max_point{};
    if (!GetBounds(min_point, max_point)) {
        return false;
    }

    return point.x >= min_point.x - tolerance
        && point.x <= max_point.x + tolerance
        && point.z >= min_point.z - tolerance
        && point.z <= max_point.z + tolerance;
}

namespace {
float edge_function(DomPoint a, DomPoint b, DomPoint c)
{
    return static_cast<float>((c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x));
}

bool point_in_screen_triangle(DomPoint point, DomPoint a, DomPoint b, DomPoint c)
{
    const float area = edge_function(a, b, c);
    if (std::abs(area) <= 0.0001f) {
        return false;
    }

    const float w0 = edge_function(b, c, point);
    const float w1 = edge_function(c, a, point);
    const float w2 = edge_function(a, b, point);
    if (area < 0.0f) {
        return w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f;
    }
    return w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f;
}
}

bool CMesh3D::HitTestMeshScreen(DomPoint point,
                                const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                float& depth) const
{
    bool hit = false;
    float best_depth = std::numeric_limits<float>::max();

    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size()) || face.corners.size() < 3) {
            continue;
        }

        std::vector<DomPoint> projected;
        std::vector<float> depths;
        projected.reserve(face.corners.size());
        depths.reserve(face.corners.size());
        bool face_visible = true;
        for (const MeshCorner& corner : face.corners) {
            const size_t index = corner.v;
            DomPoint screen{};
            float vertex_depth = 0.0f;
            if (!project_world(vertices_[index], screen, vertex_depth)) {
                face_visible = false;
                break;
            }
            projected.push_back(screen);
            depths.push_back(vertex_depth);
        }

        if (!face_visible) {
            continue;
        }

        for (size_t i = 1; i + 1 < projected.size(); ++i) {
            if (!point_in_screen_triangle(point, projected[0], projected[i], projected[i + 1])) {
                continue;
            }

            const float triangle_depth = (depths[0] + depths[i] + depths[i + 1]) / 3.0f;
            if (triangle_depth < best_depth) {
                best_depth = triangle_depth;
                hit = true;
            }
        }
    }

    if (!hit) {
        return false;
    }

    depth = best_depth;
    return true;
}

std::unique_ptr<CAlfaObject> CMesh3D::Clone() const {
    auto copy = std::make_unique<CMesh3D>(GetName() + " Copy");
    copy->vertices_ = vertices_;
    copy->uvs_ = uvs_;
    copy->normals_ = normals_;
    copy->faces_ = faces_;
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    return copy;
}

void CMesh3D::Translate(Vec3 delta) {
    for (Vec3& vertex : vertices_) {
        vertex = vertex + delta;
    }
}

void CMesh3D::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (Vec3& vertex : vertices_) {
        vertex = rotate_around_axis(vertex - center, axis, angle) + center;
    }
    for (Vec3& normal : normals_) {
        normal = normalize(rotate_around_axis(normal, axis, angle));
    }
    for (Face& face : faces_) {
        face.normal = normalize(rotate_around_axis(face.normal, axis, angle));
    }
}

void CMesh3D::Scale(Vec3 center, Vec3 axis, float factor) {
    for (Vec3& vertex : vertices_) {
        const Vec3 local = vertex - center;
        if (dot(axis, axis) <= 0.000001f) {
            vertex = scale_uniform(local, factor) + center;
        } else {
            vertex = scale_along_axis(local, axis, factor) + center;
        }
    }
    if (std::fabs(factor) > 0.000001f && dot(axis, axis) > 0.000001f) {
        const Vec3 unit_axis = normalize(axis);
        for (Vec3& normal : normals_) {
            const Vec3 parallel = unit_axis * dot(normal, unit_axis);
            const Vec3 perpendicular = normal - parallel;
            normal = normalize(perpendicular + parallel * (1.0f / factor));
        }
    }
    for (Face& face : faces_) {
        face.normal = FaceNormal(face);
    }
}

void CMesh3D::Mirror(Vec3 plane_point, Vec3 plane_normal) {
    if (dot(plane_normal, plane_normal) <= 0.000001f) {
        return;
    }

    const Vec3 unit_normal = normalize(plane_normal);
    for (Vec3& vertex : vertices_) {
        const float distance = dot(vertex - plane_point, unit_normal);
        vertex = vertex - unit_normal * (2.0f * distance);
    }
    for (Vec3& normal : normals_) {
        normal = normalize(normal - unit_normal * (2.0f * dot(normal, unit_normal)));
    }
    for (Face& face : faces_) {
        std::reverse(face.corners.begin(), face.corners.end());
        face.normal = FaceNormal(face);
    }
}

bool CMesh3D::ApplyAffineTransform(const std::array<double, 16>& matrix) {
    const double a = matrix[0], b = matrix[1], c = matrix[2];
    const double d = matrix[4], e = matrix[5], f = matrix[6];
    const double g = matrix[8], h = matrix[9], i = matrix[10];
    const double determinant =
        a * (e * i - f * h)
        - b * (d * i - f * g)
        + c * (d * h - e * g);
    if (std::fabs(determinant) <= 1.0e-15) {
        return false;
    }

    for (Vec3& vertex : vertices_) {
        const double x = vertex.x;
        const double y = vertex.y;
        const double z = vertex.z;
        vertex = {
            static_cast<float>(a * x + b * y + c * z + matrix[3]),
            static_cast<float>(d * x + e * y + f * z + matrix[7]),
            static_cast<float>(g * x + h * y + i * z + matrix[11])};
    }

    const double inverse_transpose[9]{
        (e * i - f * h) / determinant,
        (f * g - d * i) / determinant,
        (d * h - e * g) / determinant,
        (c * h - b * i) / determinant,
        (a * i - c * g) / determinant,
        (b * g - a * h) / determinant,
        (b * f - c * e) / determinant,
        (c * d - a * f) / determinant,
        (a * e - b * d) / determinant};
    for (Vec3& normal : normals_) {
        const double x = normal.x;
        const double y = normal.y;
        const double z = normal.z;
        normal = normalize({
            static_cast<float>(inverse_transpose[0] * x + inverse_transpose[1] * y + inverse_transpose[2] * z),
            static_cast<float>(inverse_transpose[3] * x + inverse_transpose[4] * y + inverse_transpose[5] * z),
            static_cast<float>(inverse_transpose[6] * x + inverse_transpose[7] * y + inverse_transpose[8] * z)});
    }
    if (determinant < 0.0) {
        for (Face& face : faces_) {
            std::reverse(face.corners.begin(), face.corners.end());
        }
    }
    for (Face& face : faces_) {
        face.normal = FaceNormal(face);
    }
    return true;
}

bool CMesh3D::GetBounds(Vec3& min_point, Vec3& max_point) const {
    if (vertices_.empty()) {
        return false;
    }

    min_point = vertices_[0];
    max_point = vertices_[0];
    for (const Vec3& vertex : vertices_) {
        min_point.x = std::min(min_point.x, vertex.x);
        min_point.y = std::min(min_point.y, vertex.y);
        min_point.z = std::min(min_point.z, vertex.z);
        max_point.x = std::max(max_point.x, vertex.x);
        max_point.y = std::max(max_point.y, vertex.y);
        max_point.z = std::max(max_point.z, vertex.z);
    }

    return true;
}

bool CMesh3D::Save(std::ostream& stream) const {
    const Material material = GetMaterial();
    stream << "Mesh3D \"" << GetName() << "\" "
           << material.diffuse.r << " " << material.diffuse.g << " " << material.diffuse.b << " "
           << material.alpha << " " << material.specular << " " << material.shininess << " "
           << vertices_.size() << " " << faces_.size() << "\n";
    for (const Vec3& vertex : vertices_) {
        stream << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
    }
    for (const Face& face : faces_) {
        stream << face.corners.size();
        for (const MeshCorner& corner : face.corners) {
            stream << " " << corner.v;
        }
        stream << "\n";
    }

    return static_cast<bool>(stream);
}

bool CMesh3D::Load(std::istream& stream) {
    std::string keyword;
    stream >> keyword;
    if (keyword != "Mesh3D") {
        return false;
    }

    stream >> std::ws;
    if (stream.peek() != '"') {
        return false;
    }

    stream.get();
    std::string name;
    std::getline(stream, name, '"');
    SetName(name);

    size_t vertex_count = 0;
    size_t face_count = 0;
    Material material{};
    stream >> material.diffuse.r >> material.diffuse.g >> material.diffuse.b;
    stream >> std::ws;
    std::string header_line;
    std::getline(stream, header_line);
    std::istringstream header_stream(header_line);
    std::vector<float> values;
    float value = 0.0f;
    while (header_stream >> value) {
        values.push_back(value);
    }
    if (values.size() == 2) {
        vertex_count = static_cast<size_t>(values[0]);
        face_count = static_cast<size_t>(values[1]);
    } else if (values.size() == 5) {
        material.alpha = values[0];
        material.specular = values[1];
        material.shininess = values[2];
        vertex_count = static_cast<size_t>(values[3]);
        face_count = static_cast<size_t>(values[4]);
    } else {
        return false;
    }
    if (!stream) {
        return false;
    }
    SetMaterial(material);

    std::vector<Vec3> loaded_vertices;
    loaded_vertices.reserve(vertex_count);
    for (size_t i = 0; i < vertex_count; ++i) {
        Vec3 vertex{};
        stream >> vertex.x >> vertex.y >> vertex.z;
        if (!stream) {
            return false;
        }
        loaded_vertices.push_back(vertex);
    }

    stream >> std::ws;

    std::vector<Face> loaded_faces;
    loaded_faces.reserve(face_count);
    std::string line;
    for (size_t i = 0; i < face_count; ++i) {
        if (!std::getline(stream, line)) {
            return false;
        }

        std::istringstream line_stream(line);
        std::vector<size_t> face_values;
        size_t face_value = 0;
        while (line_stream >> face_value) {
            face_values.push_back(face_value);
        }

        if (face_values.empty()) {
            return false;
        }

        Face face;
        if (face_values.size() == face_values[0] + 1 && face_values[0] > 2) {
            face.corners.reserve(face_values[0]);
            for (auto it = face_values.begin() + 1; it != face_values.end(); ++it) {
                face.corners.push_back({*it, *it, *it});
            }
        } else if (face_values.size() == 3) {
            for (size_t index : face_values) {
                face.corners.push_back({index, index, index});
            }
        } else {
            return false;
        }

        if (!IsValidFace(face, loaded_vertices.size())) {
            return false;
        }
        loaded_faces.push_back(std::move(face));
    }

    return SetGeometry(std::move(loaded_vertices), std::move(loaded_faces));
}

bool CMesh3D::Create(CPolyline* pline, CVector3d dir, float dist) {
    if (!pline || pline->GetPointCount() < 2 || dist <= 0.0f) {
        Clear();
        return false;
    }

    const Vec3 offset = normalize(dir) * dist;
    if (std::fabs(offset.x) <= 0.00001f && std::fabs(offset.y) <= 0.00001f && std::fabs(offset.z) <= 0.00001f) {
        Clear();
        return false;
    }

    const auto& points = pline->GetPoints();
    std::vector<Vec3> created_vertices;
    created_vertices.reserve(points.size() * 2);
    for (const CPoint3d& point : points) {
        created_vertices.push_back({static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)});
    }
    for (const CPoint3d& point : points) {
        created_vertices.push_back({
            static_cast<float>(point.x) + offset.x,
            static_cast<float>(point.y) + offset.y,
            static_cast<float>(point.z) + offset.z
        });
    }

    std::vector<Face> created_faces;
    created_faces.reserve(points.size() - 1 + (pline->IsClosed() ? 1 : 0));
    const size_t upper_offset = points.size();
    for (size_t i = 1; i < points.size(); ++i) {
        const size_t a = i - 1;
        const size_t b = i;
        const size_t c = upper_offset + i;
        const size_t d = upper_offset + i - 1;
        created_faces.push_back({a, b, c, d});
    }
    if (pline->IsClosed()) {
        created_faces.push_back({points.size() - 1, 0, upper_offset, upper_offset + points.size() - 1});
    }

    SetName(pline->GetName() + " Mesh");
    SetMaterial(colored_mesh_material());
    return SetGeometry(std::move(created_vertices), std::move(created_faces));
}

bool CMesh3D::TrimByPline(CPolyline* pLine, CPoint3d pc) {
    if (!pLine || pLine->GetPointCount() < 2 || vertices_.empty() || faces_.empty()) {
        return false;
    }

    const std::vector<cVec2> cut = make_cut_2d(pLine);
    if (cut.size() < 2) {
        return false;
    }

    const cVec2 keep_point(pc.x, pc.y);
    Face2D trim_polygon;
    bool has_trim_polygon = false;
    bool keep_inside = false;
    if (cut_is_closed(cut)) {
        trim_polygon.verts = cut;
        if (trim_polygon.verts.size() > 1 && EqualPoint2(trim_polygon.verts.front(), trim_polygon.verts.back(), EPS2D))
            trim_polygon.verts.pop_back();
        if (trim_polygon.verts.size() < 3)
            return false;
        keep_inside = ClassifyPointInFace2(trim_polygon, keep_point, EPS2D) != PFP_OUTSIDE;
        has_trim_polygon = true;
    }

    std::vector<size_t> affected_faces;

    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;
        const Face2D face_2d = make_face_2d(face, vertices_);
        CellCutInfo info;
        if (!AnalyzeFaceCut(face_2d, cut, info, EPS2D))
            continue;
        ClassifyFaceCut(face_2d, cut, info, EPS2D);
        if (info.hits.size() >= 2 || info.boundaryContactCount >= 2 || info.hasBorderOverlap) {
            affected_faces.push_back(face_index);
        }
    }

    if (affected_faces.empty()) {
        return false;
    }

    const std::vector<Vec3> original_vertices = vertices_;
    const std::vector<Face> original_faces = faces_;

    std::vector<DataToMoveVerts> data_to_move_storage;
    data_to_move_storage.reserve(affected_faces.size());
    std::vector<DataToMoveVerts*> data_to_move;
    data_to_move.reserve(affected_faces.size());
    for (size_t face_index : affected_faces) {
        if (face_index >= faces_.size())
            continue;
        Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 2)
            continue;

        DataToMoveVerts data;
        data.pf = &face;
        data_to_move_storage.push_back(data);
        data_to_move.push_back(&data_to_move_storage.back());
    }
    PrepareAndMoveVertexToTrimLine(pLine, data_to_move);
    std::vector<TrimFaceData> FacesData;
    for (size_t face_index : affected_faces) {
        if (face_index >= faces_.size())
            continue;

        Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;

        const Face2D face_2d = make_face_2d(face, vertices_);
        CellCutInfo info;
        if (!AnalyzeFaceCut(face_2d, cut, info, EPS2D))
            continue;
        ClassifyFaceCut(face_2d, cut, info, EPS2D);
	//	char buffer[256];
	//	sprintf(buffer, "Face %d: VariantCut=%d, edgeIndex1=%d, vertexToMove=%d", static_cast<int>(face_index), info.VariantCut, info.edgeIndex1, info.vertexToMove);
   //     Step(buffer);
        TrimFaceData face_data;

        int pos_a = 0;
        int pos_b = 0;
        if(info.touchedFaceVertices.size())
            pos_a =info.touchedFaceVertices[0];
		if (info.touchedFaceVertices.size() > 1)
            pos_b = info.touchedFaceVertices[1];

        face_data.FaceID = static_cast<int>(face_index);
        face_data.m_Trimmed = false;
        face_data.VariantCut = info.VariantCut;
        face_data.v1 = pos_a;
        face_data.v2 = pos_b;
        if (info.PntInFace.size() > 0) {
            CPoint3d pm(cut[info.PntInFace[0]].x, cut[info.PntInFace[0]].y, 0);
            face_data.pm = pm;
            face.pm = pm;
        }
        face_data.edgeIndex = info.edgeIndex1;
        face.edgeIndex = info.edgeIndex1;
        if (info.VariantCut == 8) {
            face_data.vertexToMove = info.vertexToMove;
            face_data.moveTarget = info.moveTarget;
        }
        if (info.VariantCut == 11) {
            face_data.v1 = info.touchedFaceVertices[0];
            face_data.v2 = info.touchedFaceVertices[1];
            face_data.Pnt = info.PntInFace;
        }
        FacesData.push_back(face_data);
    }

    for (int j = 0; j < FacesData.size(); j++) {
        TrimFaceData& faceData = FacesData[j];
    //    char buffer[256];
    //    sprintf(buffer, "Face %d: VariantCut=%d, edgeIndex=%d ", faceData.FaceID, faceData.VariantCut, faceData.edgeIndex);
    //    Step(buffer);
        cVec2 point(faceData.pm.x, faceData.pm.y);
        switch (faceData.VariantCut) {
        case 2:
            SplitFaceByLine(faces_, faceData.FaceID, faceData.v1, faceData.v2);
            break;
        case 3:
            SplitFaceByPoint(faceData.FaceID, faceData.v1, faceData.v2, point);
            break;
		case 5:       
            SplitFaceByVar5(faceData.FaceID, faceData.v1, faceData.edgeIndex, point);
			break;
        case 6:
            SplitFaceByVar6(faceData.FaceID, faceData.v1, faceData.edgeIndex, point);
            break;
        case 7:
            SplitFaceByVar7(faceData.FaceID, faceData.v1, faceData.edgeIndex);
            break;
        case 8:
            SplitFaceByVar8(faceData.FaceID, faceData.vertexToMove, faceData.moveTarget);
            break;
        case 11:
            SplitFaceByVar11(faceData.FaceID, faceData.v1, faceData.v2, faceData.Pnt, pLine);
            break;
        }
    }
    return true;
    if (cut_is_closed(cut)) {
        bool changed = false;
        for (Face& face : faces_) {
            if (face.deleted || face.corners.size() < 3)
                continue;

            const Face2D face_2d = make_face_2d(face, vertices_);
            CellCutInfo info;
            const bool face_touches_cut = AnalyzeFaceCut(face_2d, cut, info, EPS2D);
            const PointFacePos center_pos = ClassifyPointInFace2(trim_polygon, face_center_2d(face, vertices_), EPS2D);
            const bool center_delete_side = center_pos != PFP_BOUNDARY
                && ((center_pos != PFP_OUTSIDE) != keep_inside);
            if (should_delete_closed_trim_face(face, vertices_, trim_polygon, keep_inside)
                || (face_touches_cut && (center_delete_side
                    || face_has_closed_trim_delete_sample(face, vertices_, trim_polygon, keep_inside)))) {
                face.deleted = true;
                changed = true;
            }
        }
        return changed || !affected_faces.empty();
    }

    struct EdgeAdjacency {
        cVec2 a{};
        cVec2 b{};
        std::vector<size_t> faces;
    };
    std::map<EdgeCoordKey, EdgeAdjacency> edge_faces;
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        const Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;
        for (size_t i = 0; i < face.corners.size(); ++i) {
            const size_t a_index = face.corners[i].v;
            const size_t b_index = face.corners[(i + 1) % face.corners.size()].v;
            if (a_index >= vertices_.size() || b_index >= vertices_.size())
                continue;
            const cVec2 a = mesh_vertex_2d(vertices_, a_index);
            const cVec2 b = mesh_vertex_2d(vertices_, b_index);
            EdgeAdjacency& adjacency = edge_faces[edge_coord_key(a, b, EPS2D)];
            if (adjacency.faces.empty()) {
                adjacency.a = a;
                adjacency.b = b;
            }
            adjacency.faces.push_back(face_index);
        }
    }

    std::vector<std::vector<size_t>> neighbors(faces_.size());
    for (const auto& entry : edge_faces) {
        const std::vector<size_t>& adjacent = entry.second.faces;
        if (adjacent.size() != 2)
            continue;
        const cVec2 a = entry.second.a;
        const cVec2 b = entry.second.b;
        if (edge_blocked_by_cut(a, b, cut, EPS2D))
            continue;
        neighbors[adjacent[0]].push_back(adjacent[1]);
        neighbors[adjacent[1]].push_back(adjacent[0]);
    }

    size_t start_face = faces_.size();
    double best_distance = std::numeric_limits<double>::max();
    double best_inside_distance = std::numeric_limits<double>::max();
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        const Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;

        const cVec2 center = face_center_2d(face, vertices_);
        const double dist = distance2(center, keep_point);
        const Face2D face_2d = make_face_2d(face, vertices_);
        const PointFacePos pos = ClassifyPointInFace2(face_2d, keep_point, EPS2D);
        if (pos == PFP_INSIDE && dist < best_inside_distance) {
            best_inside_distance = dist;
            start_face = face_index;
        }

        if (dist < best_distance) {
            best_distance = dist;
            if (best_inside_distance == std::numeric_limits<double>::max())
                start_face = face_index;
        }
    }

    if (start_face >= faces_.size()) {
        return false;
    }

    std::vector<bool> keep(faces_.size(), false);
    std::deque<size_t> queue;
    keep[start_face] = true;
    queue.push_back(start_face);
    while (!queue.empty()) {
        const size_t current = queue.front();
        queue.pop_front();
        for (size_t next : neighbors[current]) {
            if (keep[next] || faces_[next].deleted)
                continue;
            keep[next] = true;
            queue.push_back(next);
        }
    }

    bool changed = false;
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        Face& face = faces_[face_index];
        if (face.deleted)
            continue;
        if (!keep[face_index]) {
            face.deleted = true;
            changed = true;
        }
    }
    return changed;
}

bool CMesh3D::KeepConnectedComponentAt(CPoint3d pc) {
    if (faces_.empty() || vertices_.empty()) {
        return false;
    }

    std::map<EdgeCoordKey, std::vector<size_t>> edge_faces;
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        const Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;
        for (size_t i = 0; i < face.corners.size(); ++i) {
            const size_t a_index = face.corners[i].v;
            const size_t b_index = face.corners[(i + 1) % face.corners.size()].v;
            if (a_index >= vertices_.size() || b_index >= vertices_.size())
                continue;
            edge_faces[edge_coord_key(mesh_vertex_2d(vertices_, a_index),
                                      mesh_vertex_2d(vertices_, b_index),
                                      EPS2D)].push_back(face_index);
        }
    }

    std::vector<std::vector<size_t>> neighbors(faces_.size());
    for (const auto& entry : edge_faces) {
        const std::vector<size_t>& adjacent = entry.second;
        if (adjacent.size() != 2)
            continue;
        neighbors[adjacent[0]].push_back(adjacent[1]);
        neighbors[adjacent[1]].push_back(adjacent[0]);
    }

    const cVec2 keep_point(pc.x, pc.y);
    size_t start_face = faces_.size();
    double best_inside_distance = std::numeric_limits<double>::max();
    double best_distance = std::numeric_limits<double>::max();
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        const Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;

        const cVec2 center = face_center_2d(face, vertices_);
        const double dist = distance2(center, keep_point);
        const Face2D face_2d = make_face_2d(face, vertices_);
        const PointFacePos pos = ClassifyPointInFace2(face_2d, keep_point, EPS2D);
        if (pos == PFP_INSIDE && dist < best_inside_distance) {
            best_inside_distance = dist;
            start_face = face_index;
        } else if (best_inside_distance == std::numeric_limits<double>::max() && dist < best_distance) {
            best_distance = dist;
            start_face = face_index;
        }
    }

    if (start_face >= faces_.size()) {
        return false;
    }

    std::vector<bool> keep(faces_.size(), false);
    std::deque<size_t> queue;
    keep[start_face] = true;
    queue.push_back(start_face);
    while (!queue.empty()) {
        const size_t current = queue.front();
        queue.pop_front();
        for (size_t next : neighbors[current]) {
            if (keep[next] || faces_[next].deleted)
                continue;
            keep[next] = true;
            queue.push_back(next);
        }
    }

    bool changed = false;
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        Face& face = faces_[face_index];
        if (face.deleted)
            continue;
        if (!keep[face_index]) {
            face.deleted = true;
            changed = true;
        }
    }
    return changed;
}

bool CMesh3D::TrimByPlineTest(CPolyline* pLine, CPoint3d pc) {
 /*
    const std::vector<cVec2> cut = make_cut_2d(pLine);
    if (cut.size() < 2) {
        return false;
    }
    const cVec2 keep_point(pc.x, pc.y);
    Face2D trim_polygon;
    bool has_trim_polygon = false;
    bool keep_inside = false;
    if (cut_is_closed(cut)) {
        trim_polygon.verts = cut;
        if (trim_polygon.verts.size() > 1 && EqualPoint2(trim_polygon.verts.front(), trim_polygon.verts.back(), EPS2D))
            trim_polygon.verts.pop_back();
        if (trim_polygon.verts.size() < 3)
            return false;
        keep_inside = ClassifyPointInFace2(trim_polygon, keep_point, EPS2D) != PFP_OUTSIDE;
        has_trim_polygon = true;
    }
    std::vector<size_t> affected_faces;
    for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
        Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;
        const Face2D face_2d = make_face_2d(face, vertices_);
        CellCutInfo info;
        if (!AnalyzeFaceCut(face_2d, cut, info, EPS2D))
            continue;
        ClassifyFaceCut(face_2d, cut, info, EPS2D);
        if (info.hits.size() >= 2 || info.boundaryContactCount >= 2 || info.hasBorderOverlap) {
            affected_faces.push_back(face_index);
        }
    }
    if (affected_faces.empty()) {
        return false;
    }
    const std::vector<Vec3> original_vertices = vertices_;
    const std::vector<Face> original_faces = faces_;
    std::vector<DataToMoveVerts> data_to_move_storage;
    data_to_move_storage.reserve(affected_faces.size());
    std::vector<DataToMoveVerts*> data_to_move;
    data_to_move.reserve(affected_faces.size());
    for (size_t face_index : affected_faces) {
        if (face_index >= faces_.size())
            continue;
        Face& face = faces_[face_index];
        if (face.deleted || face.corners.size() < 2)
            continue;

        DataToMoveVerts data;
        data.pf = &face;
        data_to_move_storage.push_back(data);
        data_to_move.push_back(&data_to_move_storage.back());
    }
    PrepareAndMoveVertexToTrimLine(pLine, data_to_move);
   
	return true;
*/
    return TrimByPline(pLine, pc);
}

void CMesh3D::Clear() {
    vertices_.clear();
    uvs_.clear();
    normals_.clear();
    faces_.clear();
}

bool CMesh3D::IsValidFace(const Face& face, size_t vertex_count) const {
    if (face.deleted || face.corners.size() < 3) {
        return false;
    }

    for (const MeshCorner& corner : face.corners) {
        if (corner.v >= vertex_count) {
            return false;
        }
    }

    return true;
}

Vec3 CMesh3D::FaceNormal(const Face& face) const {
    if (!IsValidFace(face, vertices_.size())) {
        return {0.0f, 1.0f, 0.0f};
    }

    const Vec3& origin = vertices_[face.corners[0].v];
    for (size_t i = 1; i + 1 < face.corners.size(); ++i) {
        const Vec3 edge_a = vertices_[face.corners[i].v] - origin;
        const Vec3 edge_b = vertices_[face.corners[i + 1].v] - origin;
        const Vec3 normal = normalize(cross(edge_a, edge_b));
        if (std::fabs(normal.x) > 0.00001f || std::fabs(normal.y) > 0.00001f || std::fabs(normal.z) > 0.00001f) {
            return normal;
        }
    }

    return {0.0f, 1.0f, 0.0f};
}

bool CMesh3D::PointInFace2d(CurvePoint point, const Face& face) const {
    if (!IsValidFace(face, vertices_.size())) {
        return false;
    }

    bool inside = false;
    bool has_area = false;
    for (size_t i = 0, j = face.corners.size() - 1; i < face.corners.size(); j = i++) {
        const Vec3& a = vertices_[face.corners[i].v];
        const Vec3& b = vertices_[face.corners[j].v];
        if (std::fabs((a.x - b.x) * (a.z + b.z)) > 0.00001f) {
            has_area = true;
        }

        const bool crosses = ((a.z > point.z) != (b.z > point.z))
            && (point.x < (b.x - a.x) * (point.z - a.z) / (b.z - a.z) + a.x);
        if (crosses) {
            inside = !inside;
        }
    }

    return has_area && inside;
}

float CMesh3D::DistanceToSegment2d(CurvePoint point, Vec3 start, Vec3 end) const {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float length_sq = dx * dx + dz * dz;
    if (length_sq <= 0.00001f) {
        const float px = point.x - start.x;
        const float pz = point.z - start.z;
        return std::sqrt(px * px + pz * pz);
    }

    const float t = std::clamp(((point.x - start.x) * dx + (point.z - start.z) * dz) / length_sq, 0.0f, 1.0f);
    const float closest_x = start.x + t * dx;
    const float closest_z = start.z + t * dz;
    const float px = point.x - closest_x;
    const float pz = point.z - closest_z;
    return std::sqrt(px * px + pz * pz);
}

int CMesh3D::FindFirstFace3d(Edge ed)
{
    for (size_t j = 0; j < faces_.size(); j++) {
        MeshFace& face = faces_[j];
        for (size_t i = 0; i < face.corners.size() - 1; i++)
        {
            Edge edi((int)face.corners[i].v, (int)face.corners[i + 1].v);
            if (ed == edi)
                return (int)j;
        }
        Edge edi((int)face.corners[0].v, (int)face.corners[face.corners.size() - 1].v);
        if (ed == edi)
            return (int)j;
    }
    return -1;
}

int CMesh3D::FindSecondCFace3d(int first_face_index, Edge ed)
{
    for (size_t j = 0; j < faces_.size(); j++) {
        if (first_face_index == j)
            continue;
        MeshFace& face = faces_[j];
        for (size_t i = 0; i < face.corners.size() - 1; i++)
        {
            Edge edi(face.corners[i].v, face.corners[i + 1].v);
            if (ed == edi)
                return j;
        }
        Edge edi(face.corners[0].v, face.corners[face.corners.size() - 1].v);
        if (ed == edi)
            return j;

    }
    return -1;
}

bool CMesh3D::MakePolyline(int nf, CPolyline& pLine)
{
    if(nf < 0 || nf >= faces_.size())
        return false;
    MeshFace& face = faces_[nf];
	pLine.Clear();
    for (size_t i = 0; i < face.corners.size(); i++)
    {
        Vec3& v = vertices_[face.corners[i].v];
        pLine.AddPoint(CPoint3d(v.x, v.y, v.z));
    }

    pLine.MakeClosed();
	return true;

}

