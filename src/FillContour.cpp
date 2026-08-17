#include "FillContour.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace {
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

float length_sq(Vec3 value)
{
    return dot(value, value);
}

Vec3 contour_normal(const std::vector<ContourPoint>& points)
{
    Vec3 normal{};
    if (points.size() < 3)
        return {0.0f, 1.0f, 0.0f};

    for (size_t i = 0; i < points.size(); ++i) {
        const Vec3& current = points[i].Pos;
        const Vec3& next = points[(i + 1) % points.size()].Pos;
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    normal = normalize(normal);
    if (length_sq(normal) <= 0.000001f) {
        for (const ContourPoint& point : points) {
            normal = normal + point.Normal;
        }
        normal = normalize(normal);
    }
    return length_sq(normal) > 0.000001f ? normal : Vec3{0.0f, 1.0f, 0.0f};
}

void plane_basis(Vec3 normal, Vec3& u, Vec3& v)
{
    normal = normalize(normal);
    Vec3 reference = std::fabs(normal.y) < 0.85f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    u = normalize(cross(reference, normal));
    if (length_sq(u) <= 0.000001f) {
        reference = {0.0f, 0.0f, 1.0f};
        u = normalize(cross(reference, normal));
    }
    v = normalize(cross(normal, u));
}

std::vector<Vec2> project_points(const std::vector<Vec3>& vertices, Vec3 normal)
{
    Vec3 u{};
    Vec3 v{};
    plane_basis(normal, u, v);
    std::vector<Vec2> projected;
    projected.reserve(vertices.size());
    for (Vec3 vertex : vertices) {
        projected.push_back({dot(vertex, u), dot(vertex, v)});
    }
    return projected;
}

float signed_area(const std::vector<Vec2>& points)
{
    float area = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        const Vec2& a = points[i];
        const Vec2& b = points[(i + 1) % points.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5f;
}

float cross2(Vec2 a, Vec2 b, Vec2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_in_triangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c)
{
    const float ab = cross2(a, b, p);
    const float bc = cross2(b, c, p);
    const float ca = cross2(c, a, p);
    constexpr float eps = -0.000001f;
    return ab >= eps && bc >= eps && ca >= eps;
}

CMesh3D::Face make_face(std::initializer_list<size_t> indices)
{
    return CMesh3D::Face(indices);
}

std::vector<CMesh3D::Face> triangulate_polygon(const std::vector<Vec3>& vertices, Vec3 normal)
{
    std::vector<CMesh3D::Face> faces;
    if (vertices.size() < 3)
        return faces;
    if (vertices.size() == 3) {
        faces.push_back(make_face({0, 1, 2}));
        return faces;
    }

    std::vector<Vec2> projected = project_points(vertices, normal);
    std::vector<size_t> polygon(vertices.size());
    std::iota(polygon.begin(), polygon.end(), size_t{0});
    if (signed_area(projected) < 0.0f) {
        std::reverse(polygon.begin(), polygon.end());
    }

    size_t guard = 0;
    while (polygon.size() > 3 && guard++ < vertices.size() * vertices.size()) {
        bool clipped = false;
        for (size_t i = 0; i < polygon.size(); ++i) {
            const size_t previous = polygon[(i + polygon.size() - 1) % polygon.size()];
            const size_t current = polygon[i];
            const size_t next = polygon[(i + 1) % polygon.size()];
            if (cross2(projected[previous], projected[current], projected[next]) <= 0.000001f)
                continue;

            bool contains_point = false;
            for (size_t candidate : polygon) {
                if (candidate == previous || candidate == current || candidate == next)
                    continue;
                if (point_in_triangle(projected[candidate], projected[previous], projected[current], projected[next])) {
                    contains_point = true;
                    break;
                }
            }
            if (contains_point)
                continue;

            faces.push_back(make_face({previous, current, next}));
            polygon.erase(polygon.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped)
            break;
    }

    if (polygon.size() == 3) {
        faces.push_back(make_face({polygon[0], polygon[1], polygon[2]}));
    }
    if (faces.empty()) {
        for (size_t i = 1; i + 1 < vertices.size(); ++i) {
            faces.push_back(make_face({0, i, i + 1}));
        }
    }
    return faces;
}

using EdgeKey = std::pair<size_t, size_t>;

EdgeKey edge_key(size_t a, size_t b)
{
    return {std::min(a, b), std::max(a, b)};
}

bool face_has_vertex(const CMesh3D::Face& face, size_t vertex)
{
    return std::any_of(face.corners.begin(), face.corners.end(), [vertex](const MeshCorner& corner) {
        return corner.v == vertex;
    });
}

bool convex_quad(const std::vector<Vec2>& projected, const std::array<size_t, 4>& quad)
{
    float sign = 0.0f;
    for (size_t i = 0; i < quad.size(); ++i) {
        const float c = cross2(projected[quad[i]], projected[quad[(i + 1) % quad.size()]], projected[quad[(i + 2) % quad.size()]]);
        if (std::fabs(c) <= 0.000001f)
            continue;
        if (sign == 0.0f) {
            sign = c;
        } else if ((sign > 0.0f) != (c > 0.0f)) {
            return false;
        }
    }
    return sign > 0.0f;
}

bool merged_quad(const CMesh3D::Face& first,
                 const CMesh3D::Face& second,
                 const std::vector<Vec2>& projected,
                 CMesh3D::Face& quad)
{
    if (first.corners.size() != 3 || second.corners.size() != 3)
        return false;

    std::vector<size_t> shared;
    std::vector<size_t> unique;
    for (const MeshCorner& corner : first.corners) {
        if (face_has_vertex(second, corner.v))
            shared.push_back(corner.v);
        else
            unique.push_back(corner.v);
    }
    for (const MeshCorner& corner : second.corners) {
        if (!face_has_vertex(first, corner.v))
            unique.push_back(corner.v);
    }
    if (shared.size() != 2 || unique.size() != 2)
        return false;

    std::array<size_t, 4> candidate{unique[0], shared[0], unique[1], shared[1]};
    Vec2 center{};
    for (size_t index : candidate) {
        center.x += projected[index].x;
        center.y += projected[index].y;
    }
    center.x *= 0.25f;
    center.y *= 0.25f;
    std::sort(candidate.begin(), candidate.end(), [&](size_t a, size_t b) {
        return std::atan2(projected[a].y - center.y, projected[a].x - center.x)
            < std::atan2(projected[b].y - center.y, projected[b].x - center.x);
    });

    if (!convex_quad(projected, candidate))
        return false;

    quad = make_face({candidate[0], candidate[1], candidate[2], candidate[3]});
    return true;
}

std::vector<CMesh3D::Face> merge_triangles_to_quads(const std::vector<Vec3>& vertices,
                                                     const std::vector<CMesh3D::Face>& faces)
{
    if (faces.empty())
        return {};

    Vec3 normal{};
    for (const CMesh3D::Face& face : faces) {
        if (face.corners.size() < 3)
            continue;
        normal = normal + cross(vertices[face.corners[1].v] - vertices[face.corners[0].v],
                                vertices[face.corners[2].v] - vertices[face.corners[0].v]);
    }
    normal = normalize(normal);
    if (length_sq(normal) <= 0.000001f)
        normal = {0.0f, 1.0f, 0.0f};
    const std::vector<Vec2> projected = project_points(vertices, normal);

    std::map<EdgeKey, std::vector<size_t>> edge_faces;
    for (size_t face_index = 0; face_index < faces.size(); ++face_index) {
        const CMesh3D::Face& face = faces[face_index];
        if (face.corners.size() != 3)
            continue;
        for (size_t i = 0; i < 3; ++i) {
            edge_faces[edge_key(face.corners[i].v, face.corners[(i + 1) % 3].v)].push_back(face_index);
        }
    }

    std::vector<CMesh3D::Face> result;
    std::vector<bool> used(faces.size(), false);
    for (const auto& entry : edge_faces) {
        const std::vector<size_t>& adjacent = entry.second;
        if (adjacent.size() != 2 || used[adjacent[0]] || used[adjacent[1]])
            continue;
        CMesh3D::Face quad;
        if (merged_quad(faces[adjacent[0]], faces[adjacent[1]], projected, quad)) {
            result.push_back(std::move(quad));
            used[adjacent[0]] = true;
            used[adjacent[1]] = true;
        }
    }

    for (size_t i = 0; i < faces.size(); ++i) {
        if (!used[i])
            result.push_back(faces[i]);
    }
    return result;
}
}

bool ContourToFill::PlacePoint(Vec3& pt, Vec3& n)
{
    n = normalize(n);
    return length_sq(pt) >= 0.0f;
}

void ContourToFill::FillByQuads(CMesh3D& mesh)
{
    CMesh3D triangle_mesh;
    FillByTriangles(triangle_mesh, 0);
    ContourQuadrangulator quadrangulator;
    quadrangulator.CreateFromMesh(&triangle_mesh);
    quadrangulator.Quadrangulate(&mesh);
}

void ContourToFill::FillByTriangles(CMesh3D& mesh, int nSubd)
{
    (void)nSubd;
    if (points_.size() < 3) {
        mesh.Clear();
        return;
    }

    std::vector<Vec3> vertices;
    vertices.reserve(points_.size());
    std::vector<Vec3> normals;
    normals.reserve(points_.size());
    for (ContourPoint point : points_) {
        PlacePoint(point.Pos, point.Normal);
        vertices.push_back(point.Pos);
        normals.push_back(length_sq(point.Normal) > 0.000001f ? normalize(point.Normal) : contour_normal(points_));
    }
    if (InvertOrder) {
        std::reverse(vertices.begin(), vertices.end());
        std::reverse(normals.begin(), normals.end());
    }

    std::vector<CMesh3D::Face> faces = triangulate_polygon(vertices, contour_normal(points_));
    if (faces.empty()) {
        mesh.Clear();
        return;
    }
    mesh.SetGeometry(std::move(vertices), std::move(faces), {}, std::move(normals));
}

void ContourToFill::Clear()
{
    points_.clear();
    WasFlipped = false;
}

void ContourToFill::AddPoint(const Vec3& pos, const Vec3& normal)
{
    if (!points_.empty() && length_sq(pos - points_.back().Pos) <= 0.00000001f)
        return;
    points_.push_back({pos, normalize(normal)});
}

void ContourQuadrangulator::CreateFromMesh(const CMesh3D* mesh)
{
    Clear();
    if (!mesh)
        return;
    vertices_ = mesh->GetVertices();
    faces_ = mesh->GetFaces();
}

void ContourQuadrangulator::Clear()
{
    vertices_.clear();
    faces_.clear();
}

void ContourQuadrangulator::Quadrangulate(CMesh3D* res)
{
    if (!res)
        return;
    if (vertices_.empty() || faces_.empty()) {
        res->Clear();
        return;
    }

    std::vector<CMesh3D::Face> quad_faces = merge_triangles_to_quads(vertices_, faces_);
    if (quad_faces.empty()) {
        res->Clear();
        return;
    }
    res->SetGeometry(vertices_, std::move(quad_faces));
}

bool FillContorByTriangles(CMesh3D* mesh, const std::vector<Vec3>& contour, Vec3 normal)
{
    if (!mesh)
        return false;
    ContourToFill filler;
    for (Vec3 point : contour) {
        filler.AddPoint(point, normal);
    }
    filler.FillByTriangles(*mesh, 0);
    return !mesh->GetFaces().empty();
}

bool FillContourByTriangles(CMesh3D* mesh, const std::vector<Vec3>& contour, Vec3 normal)
{
    return FillContorByTriangles(mesh, contour, normal);
}
