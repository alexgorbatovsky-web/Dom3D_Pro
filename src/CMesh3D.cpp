#include "CMesh3D.h"

#include "Line2D.h"
#include "OpenGLCompat.h"
#include "SurfaceUVMapping.h"
#include "solid/SurfaceFace.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#include <algorithm>
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

namespace {
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
    const float u = uv.u * scale_u;
    const float v = uv.v * scale_v;

    return {
        u * c - v * s + material.texture_offset_u,
        u * s + v * c + material.texture_offset_v
    };
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Color shaded_color(Color base, Vec3 normal, float specular_strength, float shininess, bool selected) {
    const Vec3 n = normalize(normal);
    const Vec3 key_light = normalize({-0.45f, 0.82f, 0.36f});
    const Vec3 fill_light = normalize({0.72f, 0.42f, -0.58f});
    const Vec3 camera_fill_light = normalize({0.28f, 0.28f, 0.92f});
    const Vec3 view_dir = normalize({0.30f, 0.45f, 0.84f});
    const Vec3 half_vector = normalize(key_light + view_dir);
    const float key = std::max(0.0f, dot(n, key_light));
    const float fill = std::max(0.0f, dot(n, fill_light));
    const float camera_fill = std::max(0.0f, dot(n, camera_fill_light));
    const float sky = std::max(0.0f, n.y);
    const float side = 1.0f - std::fabs(n.y);
    const float shade = std::min(1.38f, 0.40f + key * 0.56f + fill * 0.18f + camera_fill * 0.14f + sky * 0.13f + side * 0.07f);
    const float gloss = std::pow(std::max(0.0f, dot(n, half_vector)), std::max(4.0f, shininess));
    const float rim = std::pow(std::max(0.0f, 1.0f - std::fabs(dot(n, view_dir))), 3.0f) * std::max(key, camera_fill * 0.45f);
    const float highlight = std::min(0.32f, specular_strength * (gloss * 0.68f + rim * 0.12f));
    const float selected_boost = selected ? 1.08f : 1.0f;

    return {
        clamp01(base.r * shade * selected_boost + key * 0.06f + highlight),
        clamp01(base.g * shade * selected_boost + key * 0.06f + highlight),
        clamp01(base.b * shade * selected_boost + key * 0.08f + highlight)
    };
}

Color normal_rgb_color(Vec3 normal) {
    const Vec3 n = normalize(normal);
    const float x = std::fabs(n.x);
    const float y = std::fabs(n.y);
    const float z = std::fabs(n.z);
    const float sum = std::max(0.0001f, x + y + z);
    const Color x_color{1.00f, 0.42f, 0.38f};
    const Color y_color{0.42f, 1.00f, 0.30f};
    const Color z_color{0.42f, 0.68f, 1.00f};
    const float ambient = 0.16f;

    return {
        clamp01(ambient + (x_color.r * x + y_color.r * y + z_color.r * z) / sum * 0.84f),
        clamp01(ambient + (x_color.g * x + y_color.g * y + z_color.g * z) / sum * 0.84f),
        clamp01(ambient + (x_color.b * x + y_color.b * y + z_color.b * z) / sum * 0.84f)
    };
}

Color wire_color(Color base, MeshDisplayMode mode, bool selected) {
    if (mode == MeshDisplayMode::SurfaceGray) {
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

void PrepareAndMoveVertexToTrimLine(std::vector<CMesh3D::Face>& faces,
                                    std::vector<Vec3>& vertices,
                                    const std::vector<size_t>& affected_faces,
                                    const std::vector<cVec2>& cut,
                                    const Face2D* trim_polygon,
                                    bool keep_inside)
{
    struct NodeCandidate {
        size_t cut_index = 0;
        size_t vertex = 0;
        double dist2 = std::numeric_limits<double>::max();
    };

    std::vector<size_t> candidate_vertices;
    for (size_t face_index : affected_faces) {
        if (face_index >= faces.size())
            continue;
        const CMesh3D::Face& face = faces[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;

        for (const MeshCorner& corner : face.corners) {
            if (corner.v >= vertices.size())
                continue;
            candidate_vertices.push_back(corner.v);
        }
    }

    std::sort(candidate_vertices.begin(), candidate_vertices.end());
    candidate_vertices.erase(std::unique(candidate_vertices.begin(), candidate_vertices.end()), candidate_vertices.end());
    if (candidate_vertices.empty())
        return;

    std::vector<cVec2> cut_nodes;
    cut_nodes.reserve(cut.size());
    for (const cVec2& point : cut) {
        if (cut_nodes.empty() || !EqualPoint2(cut_nodes.back(), point, EPS2D))
            cut_nodes.push_back(point);
    }
    if (cut_nodes.size() > 1 && EqualPoint2(cut_nodes.front(), cut_nodes.back(), EPS2D))
        cut_nodes.pop_back();

    std::vector<NodeCandidate> candidates;
    candidates.reserve(cut_nodes.size() * candidate_vertices.size());
    for (size_t cut_index = 0; cut_index < cut_nodes.size(); ++cut_index) {
        for (size_t vertex : candidate_vertices) {
            candidates.push_back({cut_index, vertex, distance2(mesh_vertex_2d(vertices, vertex), cut_nodes[cut_index])});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const NodeCandidate& a, const NodeCandidate& b) {
        return a.dist2 < b.dist2;
    });

    std::set<size_t> used_cut_nodes;
    std::set<size_t> used_vertices;
    for (const NodeCandidate& candidate : candidates) {
        if (used_cut_nodes.count(candidate.cut_index) || used_vertices.count(candidate.vertex))
            continue;
        if (candidate.vertex >= vertices.size() || candidate.cut_index >= cut_nodes.size())
            continue;
        const cVec2& point = cut_nodes[candidate.cut_index];
        vertices[candidate.vertex].x = static_cast<float>(point.x);
        vertices[candidate.vertex].y = static_cast<float>(point.y);
        vertices[candidate.vertex].z = 0.0f;
        used_cut_nodes.insert(candidate.cut_index);
        used_vertices.insert(candidate.vertex);
        if (used_cut_nodes.size() == cut_nodes.size())
            break;
    }

    if (!trim_polygon || trim_polygon->verts.size() < 3)
        return;

    for (size_t vertex : candidate_vertices) {
        if (used_vertices.count(vertex) || vertex >= vertices.size())
            continue;

        const cVec2 point = mesh_vertex_2d(vertices, vertex);
        const PointFacePos pos = ClassifyPointInFace2(*trim_polygon, point, EPS2D);
        if (pos == PFP_BOUNDARY)
            continue;

        const bool vertex_inside = pos == PFP_INSIDE;
        if (vertex_inside == keep_inside)
            continue;

        const CutProjection projection = project_point_to_cut(point, cut);
        if (projection.segment < 0)
            continue;

        vertices[vertex].x = static_cast<float>(projection.point.x);
        vertices[vertex].y = static_cast<float>(projection.point.y);
        vertices[vertex].z = 0.0f;
    }
}

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
 
    const size_t middle_vertex = vertices_.size();
    vertices_.push_back({ static_cast<float>(pm.x), static_cast<float>(pm.y), 0.0f }); 
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
    
    const size_t middle_vertex = vertices_.size();
    vertices_.push_back({ static_cast<float>(pm.x), static_cast<float>(pm.y), 0.0f });
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

    seq2.push_back(face.corners[ind2]);
    seq2.push_back(face.corners[ind3]);
    seq2.push_back(middle);

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

    const size_t middle_vertex = vertices_.size();
    vertices_.push_back({ static_cast<float>(pm.x), static_cast<float>(pm.y), 0.0f });
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
    polygon.P(3)->x = pm.x;
    polygon.P(3)->y = pm.y;
    bool Concave = polygon.IsConcavePolygonOnXY();

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


MeshFace::MeshFace(std::initializer_list<size_t> vertex_indices) {
    corners.reserve(vertex_indices.size());
    for (size_t index : vertex_indices) {
        corners.push_back({index, index, index});
    }
}

CMesh3D::CMesh3D()
    : CAlfaObject("Mesh3D") {
    SetMaterial(colored_mesh_material());
}

Material CMesh3D::material_Defailt = Material::DefaultMesh();
float CMesh3D::s_WireOpacity = 0.76f;
MeshDisplayMode CMesh3D::s_DisplayMode = MeshDisplayMode::SurfaceGray;

CMesh3D::CMesh3D(std::string name)
    : CAlfaObject(std::move(name)) {
    SetMaterial(colored_mesh_material());
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

const std::vector<UV>& CMesh3D::GetUVs() const {
    return uvs_;
}

const std::vector<Vec3>& CMesh3D::GetNormals() const {
    return normals_;
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

    if (normals_.size() != vertices_.size()) {
        normals_.assign(vertices_.size(), {});
    }

    for (size_t i = 0; i < vertices_.size(); ++i) {
        CPoint8d point;
        if (!surface->GetPoint(vertices_[i].x, vertices_[i].y, &point)) {
            return false;
        }
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
    return true;
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
    if (mode == MeshDisplayMode::Wire) {
        const Color color = GetColor();
        RenderWire(selected, true, &color);
        return;
    }

    const Material material = mode == MeshDisplayMode::SurfaceGray ? material_Defailt : GetMaterial();

    const bool draw_edges = mode != MeshDisplayMode::SurfaceMaterial;
    RenderFaces(selected, draw_edges, &material);
    if (draw_edges) {
        RenderWire(selected, true, &material.diffuse);
    }
}

void CMesh3D::RenderFaces(bool selected, bool offset_fill, const Material* material_override) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Material material = material_override ? *material_override : GetMaterial();
    const Color color = material.diffuse;
    const float specular_strength = std::clamp(material.specular <= 0.0f ? 0.18f : material.specular, 0.0f, 1.0f);
    const float shininess = std::clamp(material.shininess <= 0.0f ? 36.0f : material.shininess, 4.0f, 96.0f);
    const float alpha = selected ? std::min(material.alpha + 0.04f, 1.0f) : material.alpha;
    const GLuint color_texture = texture_id_for_path(material.color_texture_path);
    const bool has_texture = color_texture != 0;

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
    if (has_texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, color_texture);
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
                const Color shade = (selected && !has_texture) ? normal_rgb_color(normal) : shaded_color(color, normal, specular_strength, shininess, selected);
                const Vec3& vertex = vertices_[vertex_index];
                glNormal3f(normal.x, normal.y, normal.z);
                glColor4f(shade.r, shade.g, shade.b, alpha);
                if (has_texture) {
                    const UV base_uv = corner.uv < uvs_.size()
                        ? uvs_[corner.uv]
                        : projected_uv_for_face(vertex, face_normal);
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

    if (has_texture) {
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

void CMesh3D::RenderWire(bool selected, bool draw_on_top, const Color* color_override) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Color base_color = color_override ? *color_override : GetColor();
    const MeshDisplayMode mode = GetDisplayMode();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(draw_on_top ? (selected ? 1.16f : 1.02f) : (selected ? 1.02f : 0.92f));
    const float opacity = std::clamp(s_WireOpacity, 0.05f, 1.0f);
    const Color wire = wire_color(base_color, mode, selected);
    const float alpha = opacity * (draw_on_top ? (selected ? 0.98f : 0.92f) : (selected ? 0.88f : 0.82f));
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

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
}

float CMesh3D::GetWireOpacity() {
    return s_WireOpacity;
}

void CMesh3D::SetWireOpacity(float opacity) {
    s_WireOpacity = std::clamp(opacity, 0.05f, 1.0f);
}

MeshDisplayMode CMesh3D::GetDisplayMode() {
    return s_DisplayMode;
}

void CMesh3D::SetDisplayMode(MeshDisplayMode mode) {
    s_DisplayMode = mode;
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

    PrepareAndMoveVertexToTrimLine(faces_, vertices_, affected_faces, cut,
                                   has_trim_polygon ? &trim_polygon : nullptr,
                                   keep_inside);

    std::deque<size_t> split_queue(affected_faces.begin(), affected_faces.end());
    int split_guard = 0;
    const int max_split_steps = std::max(1000, static_cast<int>(cut.size() * 8 + affected_faces.size() * 8));
    while (!split_queue.empty() && split_guard++ < max_split_steps) {
        const size_t face_index = split_queue.front();
        split_queue.pop_front();
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

        std::vector<int> touched_positions = find_cut_touched_positions(face, vertices_, cut);
        if (touched_positions.size() < 2)
            continue;

        const int pos_a = touched_positions.front();
        const int pos_b = touched_positions.back();

        bool split_by_point = false;
        for (int point_index : info.PntInFace) {
            if (point_index < 0 || point_index >= static_cast<int>(cut.size()))
                continue;

            const cVec2 point = cut[static_cast<size_t>(point_index)];
            bool already_vertex = false;
            for (const MeshCorner& corner : face.corners) {
                if (corner.v < vertices_.size() && EqualPoint2(mesh_vertex_2d(vertices_, corner.v), point, EPS2D)) {
                    already_vertex = true;
                    break;
                }
            }
            if (already_vertex)
                continue;
            
            const size_t old_face_count = faces_.size();
            if (SplitFaceByPoint(static_cast<int>(face_index), pos_a, pos_b, point)) {
                split_queue.push_back(face_index);
                for (size_t new_face_index = old_face_count; new_face_index < faces_.size(); ++new_face_index)
                    split_queue.push_back(new_face_index);
                split_by_point = true;
                break;
            }
        }

        if (split_by_point)
            continue;

        SplitFaceByLine(faces_, face_index, pos_a, pos_b);
    }

    if (cut_is_closed(cut)) {
        bool changed = false;
        for (Face& face : faces_) {
            if (face.deleted || face.corners.size() < 3)
                continue;

            const cVec2 center = face_center_2d(face, vertices_);
            const bool face_inside = ClassifyPointInFace2(trim_polygon, center, EPS2D) != PFP_OUTSIDE;
            if (face_inside != keep_inside) {
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
