#include "FillContour.h"
#include "ContourQuadrangulator3DCoat.h"

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
    // The original 3DCoat ContourToFill::Prepare() accepts a closed contour
    // whose final point repeats the first one and removes that duplicate
    // internally. BoundaryLine is passed here verbatim, so preserve the same
    // input contract instead of modifying the line before this call.
    if (points_.size() > 1
        && length_sq(points_.front().Pos - points_.back().Pos) <= 0.00000001f) {
        points_.pop_back();
    }
    if (points_.size() < 3) {
        mesh.Clear();
        return;
    }

    // Port of the moving-front core from 3DCoat's
    // ContourToFill::FillByTriangles().  Unlike planar ear clipping, obtuse
    // front corners generate a new interior point and two triangles; the
    // active contour then advances inward until it can be closed.
    std::vector<ContourPoint> prepared = points_;
    Vec3 fallback_normal = contour_normal(prepared);
    for (ContourPoint& point : prepared) {
        if (length_sq(point.Normal) <= 0.000001f)
            point.Normal = fallback_normal;
        else
            point.Normal = normalize(point.Normal);
    }

    constexpr float pi = 3.14159265358979323846f;
    const auto front_angle = [](Vec3 center, Vec3 previous, Vec3 next, Vec3 normal) {
        const Vec3 a = previous - center;
        const Vec3 b = next - center;
        const Vec3 turn = cross(b, a);
        const float sine = std::sqrt(std::max(0.0f, length_sq(turn)));
        const float cosine = dot(a, b);
        float angle = std::atan2(sine, cosine);
        if (dot(turn, normal) < 0.0f)
            angle = 2.0f * pi - angle;
        return angle;
    };

    float average_angle = 0.0f;
    for (size_t i = 0; i < prepared.size(); ++i) {
        const size_t previous = (i + prepared.size() - 1) % prepared.size();
        const size_t next = (i + 1) % prepared.size();
        average_angle += front_angle(
            prepared[i].Pos, prepared[previous].Pos, prepared[next].Pos,
            prepared[i].Normal);
    }
    average_angle /= static_cast<float>(prepared.size());
    if ((average_angle > pi) != InvertOrder)
        std::reverse(prepared.begin(), prepared.end());

    struct FrontPoint {
        size_t vertex = 0;
        int previous = -1;
        int next = -1;
        float angle = 0.0f;
        bool active = true;
    };

    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<FrontPoint> front(prepared.size());
    std::vector<int> live;
    vertices.reserve(prepared.size() * 3);
    normals.reserve(prepared.size() * 3);
    live.reserve(prepared.size());
    float average_edge_length = 0.0f;
    for (size_t i = 0; i < prepared.size(); ++i) {
        vertices.push_back(prepared[i].Pos);
        normals.push_back(prepared[i].Normal);
        front[i].vertex = i;
        front[i].previous = static_cast<int>((i + prepared.size() - 1) % prepared.size());
        front[i].next = static_cast<int>((i + 1) % prepared.size());
        live.push_back(static_cast<int>(i));
        average_edge_length += std::sqrt(length_sq(
            prepared[i].Pos - prepared[(i + prepared.size() - 1) % prepared.size()].Pos));
    }
    average_edge_length /= static_cast<float>(prepared.size());
    if (!std::isfinite(average_edge_length) || average_edge_length <= 0.000001f) {
        mesh.Clear();
        return;
    }

    std::vector<CMesh3D::Face> faces;
    faces.reserve(prepared.size() * 3);
    const auto add_triangle = [&faces](size_t first, size_t second, size_t third) {
        if (first != second && second != third && third != first)
            faces.emplace_back(std::initializer_list<size_t>{first, second, third});
    };
    const auto safe_normal = [fallback_normal](Vec3 value) {
        return length_sq(value) > 0.000001f ? normalize(value) : fallback_normal;
    };

    const size_t maximum_iterations = prepared.size() * prepared.size() * 4;
    size_t iteration = 0;
    while (live.size() > 2 && iteration++ < maximum_iterations) {
        size_t best_live_position = live.size();
        float minimum_angle = std::numeric_limits<float>::max();
        for (size_t position = 0; position < live.size(); ++position) {
            FrontPoint& current = front[static_cast<size_t>(live[position])];
            const FrontPoint& previous = front[static_cast<size_t>(current.previous)];
            const FrontPoint& next = front[static_cast<size_t>(current.next)];
            const Vec3 normal = safe_normal(
                normals[previous.vertex] + normals[current.vertex] + normals[next.vertex]);
            if (std::fabs(current.angle) <= 0.0001f) {
                current.angle = front_angle(
                    vertices[current.vertex], vertices[previous.vertex],
                    vertices[next.vertex], normal);
            }
            float selection_angle = current.angle;
            if (selection_angle > pi * 1.999f)
                selection_angle -= 2.0f * pi;
            if (selection_angle <= minimum_angle) {
                minimum_angle = selection_angle;
                best_live_position = position;
            }
        }
        if (best_live_position == live.size())
            break;

        const int current_index = live[best_live_position];
        FrontPoint& current = front[static_cast<size_t>(current_index)];
        FrontPoint& previous = front[static_cast<size_t>(current.previous)];
        FrontPoint& next = front[static_cast<size_t>(current.next)];
        const size_t vertex1 = previous.vertex;
        const size_t vertex2 = current.vertex;
        const size_t vertex3 = next.vertex;
        const float angle = current.angle;
        const bool finish_fast = iteration > prepared.size() * prepared.size() * 2;

        if (angle <= pi * 0.5f || angle >= pi || live.size() == 3 || finish_fast) {
            add_triangle(vertex1, vertex2, vertex3);
            current.active = false;
            previous.next = current.next;
            next.previous = current.previous;
            previous.angle = 0.0f;
            next.angle = 0.0f;
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(best_live_position));
            continue;
        }

        const Vec3 vertex_position1 = vertices[vertex1];
        const Vec3 vertex_position2 = vertices[vertex2];
        const Vec3 vertex_position3 = vertices[vertex3];
        Vec3 direction = safe_normal(
            safe_normal(vertex_position1 - vertex_position2)
            + safe_normal(vertex_position3 - vertex_position2));
        Vec3 new_position = vertex_position2 + direction * average_edge_length;
        Vec3 new_normal = safe_normal(
            normals[vertex1] + normals[vertex2] + normals[vertex3]);
        PlacePoint(new_position, new_normal);
        const size_t generated = vertices.size();
        vertices.push_back(new_position);
        normals.push_back(new_normal);

        // Standalone equivalent of FillContour::SnapSomewhere(): when the
        // advancing point reaches another non-adjacent part of the live
        // front, reuse that vertex and split the linked contour there.
        int snap_index = -1;
        float snap_distance_squared = std::numeric_limits<float>::max();
        const Vec3 advance = new_position - vertex_position2;
        const float advance_length_squared = length_sq(advance);
        if (advance_length_squared > 0.000001f) {
            for (int candidate_index : live) {
                if (candidate_index == current_index
                    || candidate_index == current.previous
                    || candidate_index == current.next) {
                    continue;
                }
                const FrontPoint& candidate = front[static_cast<size_t>(candidate_index)];
                const Vec3 candidate_position = vertices[candidate.vertex];
                const float projection = std::clamp(
                    dot(candidate_position - vertex_position2, advance)
                        / advance_length_squared,
                    0.0f, 1.0f);
                const Vec3 projected = vertex_position2 + advance * projection;
                const float distance_squared = length_sq(candidate_position - projected);
                const float allowed = average_edge_length
                    * 0.95f * std::max(projection, 0.1f);
                if (distance_squared < allowed * allowed
                    && distance_squared < snap_distance_squared) {
                    snap_distance_squared = distance_squared;
                    snap_index = candidate_index;
                }
            }
        }

        size_t front_vertex = generated;
        Vec3 front_position = new_position;
        if (snap_index >= 0) {
            FrontPoint& snap = front[static_cast<size_t>(snap_index)];
            const int old_previous = current.previous;
            const int old_snap_previous = snap.previous;
            current.vertex = snap.vertex;
            current.previous = old_snap_previous;
            snap.previous = old_previous;
            front[static_cast<size_t>(current.previous)].next = current_index;
            front[static_cast<size_t>(current.next)].previous = current_index;
            front[static_cast<size_t>(snap.previous)].next = snap_index;
            front[static_cast<size_t>(snap.next)].previous = snap_index;
            front_vertex = snap.vertex;
            front_position = vertices[front_vertex];
            front[static_cast<size_t>(current.previous)].angle = 0.0f;
            front[static_cast<size_t>(snap.previous)].angle = 0.0f;
            snap.angle = 0.0f;
        }

        const Vec3 triangle_normal1 = safe_normal(cross(
            vertex_position1 - front_position,
            vertex_position2 - front_position));
        const Vec3 triangle_normal2 = safe_normal(cross(
            vertex_position2 - front_position,
            vertex_position3 - front_position));
        if (dot(triangle_normal1, triangle_normal2) > 0.0f) {
            add_triangle(vertex1, vertex2, front_vertex);
            add_triangle(front_vertex, vertex2, vertex3);
        } else {
            add_triangle(vertex1, vertex2, vertex3);
            add_triangle(front_vertex, vertex1, vertex3);
        }
        if (snap_index < 0)
            current.vertex = generated;
        current.angle = 0.0f;
        previous.angle = 0.0f;
        next.angle = 0.0f;
    }

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

    if (!Build3DCoatQuadrangulation(vertices_, faces_, res)) {
        res->Clear();
    }
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
