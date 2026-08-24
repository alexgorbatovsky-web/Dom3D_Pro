#include "ContourQuadrangulator3DCoat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1.0e-9;

double lengthSquared(Vec3 value) { return static_cast<double>(dot(value, value)); }
double distance(Vec3 first, Vec3 second) { return std::sqrt(lengthSquared(first - second)); }

Vec3 safeNormal(Vec3 value, Vec3 fallback = {0.0f, 0.0f, 1.0f})
{
    return lengthSquared(value) > kEpsilon ? normalize(value) : fallback;
}

struct QuadPoint {
    Vec3 position{};
    Vec3 tangent1{};
    Vec3 tangent2{};
    Vec3 front1{};
    Vec3 front2{};
    Vec3 normal{0.0f, 0.0f, 1.0f};
    double derived_edge_length = 0.0;
    double angle = 0.0;
    int average_distance = 0;
    int parent_vertex = -1;
    int edge_type = 0;
    int previous = -1;
    int next = -1;
    bool initial = false;
};

using QuadFace = std::vector<int>;
using BoundaryEdgeKey = std::pair<size_t, size_t>;

BoundaryEdgeKey orderedEdge(size_t first, size_t second)
{
    return std::minmax(first, second);
}

double triangleArea(Vec3 first, Vec3 second, Vec3 third)
{
    return 0.5 * std::sqrt(lengthSquared(cross(second - first, third - first)));
}

double quadrilateralQuality(Vec3 v1, Vec3 v2, Vec3 v3, Vec3 v4)
{
    const auto angle = [](Vec3 first, Vec3 center, Vec3 third) {
        Vec3 d1 = safeNormal(first - center);
        Vec3 d2 = safeNormal(third - center);
        return std::acos(std::clamp(static_cast<double>(dot(d1, d2)), -1.0, 1.0))
            * 180.0 / kPi;
    };
    return std::min({angle(v1, v2, v3), angle(v2, v3, v4),
                     angle(v3, v4, v1), angle(v4, v1, v2)});
}

class TriangleSnapper {
public:
    TriangleSnapper(const std::vector<Vec3>& vertices,
                    const std::vector<CMesh3D::Face>& faces)
        : vertices_(vertices), faces_(faces) {}

    void Snap(Vec3& position, Vec3 normal) const
    {
        normal = safeNormal(normal);
        double best_distance = std::numeric_limits<double>::max();
        Vec3 best = position;
        for (const CMesh3D::Face& face : faces_) {
            if (face.corners.size() != 3)
                continue;
            const Vec3 a = vertices_[face.corners[0].v];
            const Vec3 b = vertices_[face.corners[1].v];
            const Vec3 c = vertices_[face.corners[2].v];
            double ray_distance = 0.0;
            Vec3 hit{};
            if (RayTriangle(position, normal, a, b, c, ray_distance, hit)) {
                const double absolute_distance = std::fabs(ray_distance);
                if (absolute_distance < best_distance) {
                    best_distance = absolute_distance;
                    best = hit;
                }
            }
        }
        if (best_distance < std::numeric_limits<double>::max())
            position = best;
    }

private:
    static bool RayTriangle(Vec3 origin, Vec3 direction,
                            Vec3 a, Vec3 b, Vec3 c,
                            double& ray_distance, Vec3& hit)
    {
        const Vec3 edge1 = b - a;
        const Vec3 edge2 = c - a;
        const Vec3 p = cross(direction, edge2);
        const double determinant = dot(edge1, p);
        if (std::fabs(determinant) <= kEpsilon)
            return false;
        const double inverse = 1.0 / determinant;
        const Vec3 t = origin - a;
        const double u = dot(t, p) * inverse;
        if (u < -1.0e-6 || u > 1.0 + 1.0e-6)
            return false;
        const Vec3 q = cross(t, edge1);
        const double v = dot(direction, q) * inverse;
        if (v < -1.0e-6 || u + v > 1.0 + 1.0e-6)
            return false;
        ray_distance = dot(edge2, q) * inverse;
        hit = origin + direction * static_cast<float>(ray_distance);
        return true;
    }

    const std::vector<Vec3>& vertices_;
    const std::vector<CMesh3D::Face>& faces_;
};

void updateDirections(std::vector<QuadPoint>& contour,
                      bool derived = false,
                      double crease_angle = 50.0)
{
    for (QuadPoint& point : contour) {
        point.angle = 0.0;
        if (point.previous < 0 || point.next < 0)
            continue;
        point.tangent2 = safeNormal(contour[point.next].position - point.position);
        point.tangent1 = safeNormal(contour[point.previous].position - point.position);
        point.front1 = cross(point.tangent1, point.normal);
        point.front2 = cross(point.normal, point.tangent2);
        double angle = std::atan2(dot(point.tangent2, point.front1),
                                  dot(point.tangent1, point.tangent2))
            * 180.0 / kPi;
        while (angle < 0.0)
            angle += 360.0;
        point.edge_type = 0;
        if (angle < 180.0 - crease_angle)
            point.edge_type = 1;
        if (angle > 230.0)
            point.edge_type = -1;
        point.angle = angle;
        if (derived) {
            point.derived_edge_length =
                (distance(point.position, contour[point.previous].position)
                 + distance(point.position, contour[point.next].position)) * 0.5;
        }
    }
}

int contourSize(const std::vector<QuadPoint>& contour, int start)
{
    int count = 1;
    int current = start;
    for (size_t guard = 0; guard < contour.size(); ++guard) {
        const int next = contour[current].next;
        if (next < 0 || next == start)
            break;
        ++count;
        current = next;
    }
    return count;
}

bool validLinks(const std::vector<QuadPoint>& contour)
{
    for (size_t i = 0; i < contour.size(); ++i) {
        const QuadPoint& point = contour[i];
        if ((point.next < 0) != (point.previous < 0))
            return false;
        if (point.next >= static_cast<int>(contour.size())
            || point.previous >= static_cast<int>(contour.size()))
            return false;
        if (point.next >= 0 && contour[point.next].previous != static_cast<int>(i))
            return false;
        if (point.previous >= 0 && contour[point.previous].next != static_cast<int>(i))
            return false;
    }
    return true;
}

bool checkFlips(const std::vector<QuadPoint>& contour,
                const std::vector<QuadFace>& faces)
{
    for (const QuadFace& face : faces) {
        int flipped = 0;
        for (size_t i = 0; i < face.size(); ++i) {
            const int previous = face[(i + face.size() - 1) % face.size()];
            const int current = face[i];
            const int next = face[(i + 1) % face.size()];
            const Vec3 normal = safeNormal(cross(
                contour[next].position - contour[current].position,
                contour[previous].position - contour[current].position));
            if (dot(normal, contour[current].normal) < 0.0f)
                ++flipped;
        }
        if (flipped > 1)
            return false;
    }
    return true;
}

double segmentDistanceSquared(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2)
{
    const Vec3 d1 = q1 - p1;
    const Vec3 d2 = q2 - p2;
    const Vec3 r = p1 - p2;
    const double a = dot(d1, d1);
    const double e = dot(d2, d2);
    const double f = dot(d2, r);
    double s = 0.0;
    double t = 0.0;
    if (a <= kEpsilon && e <= kEpsilon)
        return lengthSquared(p1 - p2);
    if (a <= kEpsilon) {
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = dot(d1, r);
        if (e <= kEpsilon) {
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = dot(d1, d2);
            const double denominator = a * e - b * b;
            if (std::fabs(denominator) > kEpsilon)
                s = std::clamp((b * f - c * e) / denominator, 0.0, 1.0);
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }
    const Vec3 c1 = p1 + d1 * static_cast<float>(s);
    const Vec3 c2 = p2 + d2 * static_cast<float>(t);
    return lengthSquared(c1 - c2);
}

bool checkSelfIntersections(const std::vector<QuadPoint>& contour)
{
    for (size_t i = 0; i < contour.size(); ++i) {
        const int i_next = contour[i].next;
        if (i_next < 0)
            continue;
        const double first_length = distance(contour[i].position, contour[i_next].position);
        for (size_t j = i + 1; j < contour.size(); ++j) {
            const int j_next = contour[j].next;
            if (j_next < 0 || static_cast<int>(i) == j_next
                || i_next == static_cast<int>(j) || i_next == j_next)
                continue;
            const double second_length = distance(contour[j].position, contour[j_next].position);
            const double tolerance = std::min(first_length, second_length) * 0.1;
            if (segmentDistanceSquared(
                    contour[i].position, contour[i_next].position,
                    contour[j].position, contour[j_next].position)
                < tolerance * tolerance)
                return true;
        }
    }
    return false;
}

struct Expansion {
    double cost = std::numeric_limits<double>::max();
    std::vector<QuadPoint> contour;
    std::vector<QuadFace> faces;
};

Expansion tryExpand(const std::vector<QuadPoint>& source,
                    int start_point,
                    const TriangleSnapper& snapper)
{
    Expansion result;
    result.contour = source;
    std::vector<QuadPoint>& contour = result.contour;
    QuadPoint* start = &contour[start_point];
    if (start->angle < 15.0) {
        const int p = start->previous;
        const int pp = contour[p].previous;
        const int n = start->next;
        const int nn = contour[n].next;
        if (p < 0 || pp < 0 || n < 0 || nn < 0)
            return result;
        if (distance(contour[p].position, contour[nn].position)
            < distance(contour[pp].position, contour[n].position)) {
            result.faces.push_back({start_point, n, nn, p});
            contour[start_point].previous = contour[start_point].next = -1;
            contour[n].previous = contour[n].next = -1;
            contour[nn].previous = p;
            contour[p].next = nn;
        } else {
            result.faces.push_back({start_point, n, pp, p});
            contour[start_point].previous = contour[start_point].next = -1;
            contour[p].previous = contour[p].next = -1;
            contour[n].previous = pp;
            contour[pp].next = n;
        }
        result.cost = validLinks(contour) ? 0.0 : std::numeric_limits<double>::max();
        return result;
    }

    int end_point = start_point;
    int chunk_count = 0;
    double derived_length = 0.0;
    double edge_distance = 0.0;
    QuadPoint* point = &contour[end_point];
    do {
        ++chunk_count;
        end_point = point->next;
        if (end_point < 0)
            return result;
        QuadPoint* next = &contour[end_point];
        derived_length += (point->derived_edge_length + next->derived_edge_length) * 0.5;
        edge_distance += point->average_distance;
        if (end_point == start_point || next->edge_type)
            break;
        point = next;
    } while (chunk_count <= static_cast<int>(contour.size()));
    if (chunk_count <= 0)
        return result;
    derived_length /= chunk_count;
    edge_distance /= chunk_count;

    std::vector<Vec3> old_points;
    std::vector<int> old_ids;
    double minimum_angle = 90.0;
    double proportion = 1.0;

    if (start_point != end_point) {
        const int pre_start = contour[start_point].previous;
        const int post_end = contour[end_point].next;
        if (pre_start < 0 || post_end < 0)
            return result;
        const int start_type = contour[start_point].edge_type;
        const int end_type = contour[end_point].edge_type;
        const Vec3 post_position = end_type != -1
            ? contour[post_end].position
            : contour[end_point].position * 2.0f - contour[post_end].position;
        const Vec3 pre_position = start_type != -1
            ? contour[pre_start].position
            : contour[start_point].position * 2.0f - contour[pre_start].position;
        const double start_front = dot(contour[start_point].front2,
            pre_position - contour[start_point].position);
        const double end_front = dot(contour[end_point].front1,
            post_position - contour[end_point].position);
        const double start_tangent = dot(contour[start_point].tangent2,
            pre_position - contour[start_point].position);
        const double end_tangent = dot(contour[end_point].tangent1,
            contour[end_point].position - post_position);

        std::vector<Vec3> chunks{pre_position};
        std::vector<Vec3> chunk_normals{contour[start_point].normal};
        std::vector<int> chunk_ids{start_type == -1 ? -1 : pre_start};
        old_points.push_back(contour[start_point].position);
        old_ids.push_back(start_point);
        int current = start_point;
        int part = 1;
        do {
            current = contour[current].next;
            if (current < 0)
                return result;
            QuadPoint& current_point = contour[current];
            old_points.push_back(current_point.position);
            old_ids.push_back(current);
            const double alpha = static_cast<double>(part++) / chunk_count;
            const double front = start_front * (1.0 - alpha) + end_front * alpha;
            const double tangent = start_tangent * (1.0 - alpha) + end_tangent * alpha;
            Vec3 new_position = current == end_point
                ? post_position
                : current_point.position
                    + current_point.front2 * static_cast<float>(front)
                    + current_point.tangent2 * static_cast<float>(tangent);
            chunks.push_back(new_position);
            chunk_normals.push_back(current_point.normal);
            chunk_ids.push_back(-1);
        } while (current != end_point);
        if (end_type != -1)
            chunk_ids.back() = post_end;
        for (size_t i = 0; i < chunks.size(); ++i)
            snapper.Snap(chunks[i], chunk_normals[i]);
        for (int pass = 0; pass < 5; ++pass) {
            for (size_t i = 1; i + 1 < chunks.size(); ++i) {
                const Vec3 tangent = safeNormal(chunks[i - 1] - chunks[i + 1]);
                const Vec3 delta = (chunks[i - 1] + chunks[i + 1]) * 0.5f - chunks[i];
                chunks[i] = chunks[i] + tangent * dot(tangent, delta) * 0.35f + delta * 0.15f;
            }
            for (size_t i = chunks.size() - 1; i-- > 1;) {
                const Vec3 tangent = safeNormal(chunks[i - 1] - chunks[i + 1]);
                const Vec3 delta = (chunks[i - 1] + chunks[i + 1]) * 0.5f - chunks[i];
                chunks[i] = chunks[i] + tangent * dot(tangent, delta) * 0.35f + delta * 0.15f;
            }
        }
        double old_length = 0.0;
        double new_length = 0.0;
        for (size_t i = 0; i + 1 < chunks.size(); ++i) {
            old_length += distance(old_points[i], old_points[i + 1]);
            new_length += distance(chunks[i], chunks[i + 1]);
            minimum_angle = std::min(minimum_angle, quadrilateralQuality(
                chunks[i], chunks[i + 1], old_points[i + 1], old_points[i]));
        }
        proportion = new_length / std::max(derived_length * chunk_count, kEpsilon);

        int erase_start = start_type == 1 ? start_point : contour[start_point].next;
        const int erase_last = end_type == 1 ? end_point : contour[end_point].previous;
        if (erase_start < 0 || erase_last < 0)
            return result;
        for (size_t i = 0; i < chunk_ids.size(); ++i) {
            if (chunk_ids[i] < 0) {
                QuadPoint new_point;
                new_point.position = chunks[i];
                new_point.normal = chunk_normals[i];
                chunk_ids[i] = static_cast<int>(contour.size());
                contour.push_back(new_point);
            }
        }
        for (size_t i = 0; i < chunk_ids.size(); ++i) {
            QuadPoint& chunk = contour[chunk_ids[i]];
            if (i + 1 < chunk_ids.size())
                chunk.next = chunk_ids[i + 1];
            if (i > 0)
                chunk.previous = chunk_ids[i - 1];
            chunk.derived_edge_length = derived_length;
            chunk.parent_vertex = old_ids[i];
            chunk.average_distance = 0;
        }
        if (start_type == -1) {
            contour[chunk_ids.front()].previous = start_point;
            contour[start_point].next = chunk_ids.front();
        }
        if (end_type == -1) {
            contour[chunk_ids.back()].next = end_point;
            contour[end_point].previous = chunk_ids.back();
        }
        for (size_t guard = 0; guard < contour.size(); ++guard) {
            const int next = contour[erase_start].next;
            contour[erase_start].next = contour[erase_start].previous = -1;
            if (erase_start == erase_last)
                break;
            erase_start = next;
            if (erase_start < 0)
                return result;
        }
        for (size_t i = 0; i + 1 < chunks.size(); ++i)
            result.faces.push_back({old_ids[i], old_ids[i + 1], chunk_ids[i + 1], chunk_ids[i]});
    } else {
        int current = start_point;
        std::vector<Vec3> old_ring;
        std::vector<Vec3> new_ring;
        std::vector<Vec3> ring_normals;
        std::vector<int> old_ring_ids;
        do {
            QuadPoint& current_point = contour[current];
            if (current_point.previous < 0 || current_point.next < 0)
                return result;
            const double local_length =
                (distance(current_point.position, contour[current_point.next].position)
                 + distance(current_point.position, contour[current_point.previous].position)) * 0.5;
            old_ring.push_back(current_point.position);
            new_ring.push_back(current_point.position
                + safeNormal(current_point.front1 + current_point.front2)
                    * static_cast<float>(local_length));
            ring_normals.push_back(current_point.normal);
            old_ring_ids.push_back(current);
            const int next = current_point.next;
            current_point.previous = current_point.next = -1;
            current = next;
        } while (current != start_point && old_ring.size() <= contour.size());
        if (new_ring.size() < 3)
            return result;
        for (int pass = 0; pass < 5; ++pass) {
            const std::vector<Vec3> temporary = new_ring;
            for (size_t i = 0; i < new_ring.size(); ++i) {
                const size_t previous = (i + new_ring.size() - 1) % new_ring.size();
                const size_t next = (i + 1) % new_ring.size();
                const Vec3 tangent = safeNormal(temporary[previous] - temporary[next]);
                const Vec3 delta = (temporary[previous] + temporary[next]) * 0.5f - temporary[i];
                new_ring[i] = new_ring[i] + tangent * dot(tangent, delta) * 0.15f + delta * 0.05f;
            }
        }
        for (size_t i = 0; i < new_ring.size(); ++i)
            snapper.Snap(new_ring[i], ring_normals[i]);
        std::vector<int> new_ring_ids(new_ring.size(), -1);
        for (size_t i = 0; i < new_ring.size(); ++i) {
            QuadPoint new_point;
            new_point.position = new_ring[i];
            new_point.normal = ring_normals[i];
            new_point.parent_vertex = old_ring_ids[i];
            new_point.derived_edge_length = contour[old_ring_ids[i]].derived_edge_length;
            new_ring_ids[i] = static_cast<int>(contour.size());
            contour.push_back(new_point);
        }
        double old_length = 0.0;
        double new_length = 0.0;
        for (size_t i = 0; i < new_ring.size(); ++i) {
            const size_t next = (i + 1) % new_ring.size();
            contour[new_ring_ids[i]].next = new_ring_ids[next];
            contour[new_ring_ids[next]].previous = new_ring_ids[i];
            old_length += distance(old_ring[i], old_ring[next]);
            new_length += distance(new_ring[i], new_ring[next]);
            result.faces.push_back({old_ring_ids[i], old_ring_ids[next],
                                    new_ring_ids[next], new_ring_ids[i]});
        }
        proportion = old_length / std::max(new_length, kEpsilon);
    }

    if (!validLinks(contour))
        return result;
    const double distortion = proportion > 1.0 ? proportion - 1.0 : 1.0 / std::max(proportion, kEpsilon) - 1.0;
    result.cost = distortion + old_points.size() / 3.0
        + (95.0 / (minimum_angle + 5.0) - 1.0) * 0.25 + edge_distance / 5.0;
    return result;
}

void addAdjacency(const std::vector<QuadFace>& faces,
                  std::vector<std::set<int>>& adjacency)
{
    for (const QuadFace& face : faces) {
        for (size_t i = 0; i < face.size(); ++i) {
            const int first = face[i];
            const int second = face[(i + 1) % face.size()];
            if (first >= static_cast<int>(adjacency.size())
                || second >= static_cast<int>(adjacency.size()))
                adjacency.resize(static_cast<size_t>(std::max(first, second) + 1));
            adjacency[first].insert(second);
            adjacency[second].insert(first);
        }
    }
}

} // namespace

bool Build3DCoatQuadrangulation(const std::vector<Vec3>& vertices,
                                const std::vector<CMesh3D::Face>& triangles,
                                CMesh3D* result)
{
    if (!result || vertices.empty() || triangles.empty())
        return false;

    std::map<BoundaryEdgeKey, int> edge_counts;
    std::map<BoundaryEdgeKey, std::pair<size_t, size_t>> edge_directions;
    std::vector<Vec3> vertex_normals(vertices.size(), {});
    double initial_area = 0.0;
    for (const CMesh3D::Face& face : triangles) {
        if (face.corners.size() != 3)
            return false;
        const size_t a = face.corners[0].v;
        const size_t b = face.corners[1].v;
        const size_t c = face.corners[2].v;
        const Vec3 face_normal = cross(vertices[b] - vertices[a], vertices[c] - vertices[a]);
        vertex_normals[a] = vertex_normals[a] + face_normal;
        vertex_normals[b] = vertex_normals[b] + face_normal;
        vertex_normals[c] = vertex_normals[c] + face_normal;
        initial_area += triangleArea(vertices[a], vertices[b], vertices[c]);
        const std::array<std::pair<size_t, size_t>, 3> edges{{{a, b}, {b, c}, {c, a}}};
        for (const auto& edge : edges) {
            ++edge_counts[orderedEdge(edge.first, edge.second)];
            edge_directions[orderedEdge(edge.first, edge.second)] = edge;
        }
    }
    for (Vec3& normal : vertex_normals)
        normal = safeNormal(normal);

    std::map<size_t, int> encoding;
    std::vector<QuadPoint> contour;
    for (const auto& entry : edge_counts) {
        if (entry.second != 1)
            continue;
        const auto direction = edge_directions[entry.first];
        for (size_t vertex : {direction.first, direction.second}) {
            if (!encoding.count(vertex)) {
                QuadPoint point;
                point.position = vertices[vertex];
                point.normal = vertex_normals[vertex];
                point.initial = true;
                encoding[vertex] = static_cast<int>(contour.size());
                contour.push_back(point);
            }
        }
        contour[encoding[direction.first]].next = encoding[direction.second];
        contour[encoding[direction.second]].previous = encoding[direction.first];
    }
    if (contour.size() < 4 || !validLinks(contour))
        return false;

    updateDirections(contour, true);
    TriangleSnapper snapper(vertices, triangles);
    std::vector<QuadFace> output_faces;
    std::vector<std::set<int>> adjacency(contour.size());
    double crease = 50.0;
    initial_area *= 1.1;

    for (int pass = 0; pass < 5000; ++pass) {
        bool contour_found = false;
        std::vector<int> starts;
        Expansion best;
        for (size_t i = 0; i < contour.size(); ++i) {
            if (contour[i].next < 0)
                continue;
            contour_found = true;
            if (!contour[i].edge_type)
                continue;
            const int size = contourSize(contour, static_cast<int>(i));
            if (size <= 4) {
                QuadFace closing;
                int current = static_cast<int>(i);
                for (int count = 0; count < size; ++count) {
                    closing.push_back(current);
                    const int next = contour[current].next;
                    contour[current].previous = contour[current].next = -1;
                    current = next;
                }
                output_faces.push_back(std::move(closing));
                starts.clear();
                break;
            }
            starts.push_back(static_cast<int>(i));
        }
        if (!contour_found)
            break;

        for (int start : starts) {
            Expansion candidate = tryExpand(contour, start, snapper);
            if (candidate.cost >= best.cost)
                continue;
            if (!checkFlips(candidate.contour, candidate.faces)
                || checkSelfIntersections(candidate.contour))
                continue;
            best = std::move(candidate);
        }
        if (best.cost < 10000000.0) {
            contour = std::move(best.contour);
            output_faces.insert(output_faces.end(), best.faces.begin(), best.faces.end());
            updateDirections(contour);
            adjacency.resize(contour.size());
            addAdjacency(best.faces, adjacency);
            crease = 50.0;
        } else {
            crease *= 0.9;
            updateDirections(contour, false, crease);
            if (crease < 1.0)
                return false;
        }

        double current_area = 0.0;
        for (const QuadFace& face : output_faces) {
            if (face.size() == 3) {
                current_area += triangleArea(contour[face[0]].position,
                                             contour[face[1]].position,
                                             contour[face[2]].position);
            } else if (face.size() == 4) {
                current_area += triangleArea(contour[face[0]].position,
                                             contour[face[1]].position,
                                             contour[face[2]].position);
                current_area += triangleArea(contour[face[0]].position,
                                             contour[face[2]].position,
                                             contour[face[3]].position);
            }
        }
        if (current_area > initial_area)
            return false;
    }

    if (output_faces.empty())
        return false;
    adjacency.resize(contour.size());
    addAdjacency(output_faces, adjacency);
    for (int pass = 0; pass < 20; ++pass) {
        std::vector<Vec3> smoothed(contour.size());
        for (size_t i = 0; i < contour.size(); ++i) {
            if (contour[i].initial || adjacency[i].empty()) {
                smoothed[i] = contour[i].position;
                continue;
            }
            Vec3 sum = contour[i].position;
            double weight = 1.0;
            for (int neighbour : adjacency[i]) {
                sum = sum + contour[neighbour].position;
                weight += 1.0;
            }
            smoothed[i] = sum * static_cast<float>(1.0 / weight);
        }
        for (size_t i = 0; i < contour.size(); ++i) {
            if (contour[i].initial)
                continue;
            contour[i].position = smoothed[i];
            snapper.Snap(contour[i].position, contour[i].normal);
        }
    }

    std::vector<Vec3> result_vertices;
    result_vertices.reserve(contour.size());
    for (const QuadPoint& point : contour)
        result_vertices.push_back(point.position);
    std::vector<CMesh3D::Face> result_faces;
    result_faces.reserve(output_faces.size());
    for (const QuadFace& face : output_faces) {
        if (face.size() < 3 || face.size() > 4)
            return false;
        CMesh3D::Face result_face;
        for (int vertex : face)
            result_face.corners.push_back({static_cast<size_t>(vertex),
                                           static_cast<size_t>(vertex),
                                           static_cast<size_t>(vertex)});
        result_faces.push_back(std::move(result_face));
    }
    result->Clear();
    return result->SetGeometry(std::move(result_vertices), std::move(result_faces));
}
