#include "CatmullClarkSurfaceBuilder.h"

#include "CMesh3D.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <GeomAPI_PointsToBSplineSurface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <ShapeFix_Solid.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {
using Quad = std::array<size_t, 4>;
using EdgeKey = std::pair<size_t, size_t>;

EdgeKey edge_key(size_t first, size_t second) {
    return std::minmax(first, second);
}

Vec3 average(const std::vector<Vec3>& points) {
    Vec3 result{};
    for (Vec3 point : points) result = result + point;
    return points.empty() ? result : result * (1.0f / points.size());
}

struct SubdivisionLevel {
    std::vector<Vec3> vertices;
    std::vector<Quad> faces;
    std::set<EdgeKey> sharp_edges;
    std::vector<std::array<size_t, 4>> children;
};

bool make_control_level(const CMesh3D& mesh, SubdivisionLevel& level) {
    level.vertices = mesh.GetVertices();
    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        if (face.deleted) continue;
        if (face.corners.size() != 4) return false;
        Quad quad{};
        for (size_t corner = 0; corner < 4; ++corner) {
            quad[corner] = face.corners[corner].v;
            if (quad[corner] >= level.vertices.size()) return false;
        }
        level.faces.push_back(quad);
    }
    if (level.faces.empty()) return false;
    for (const Edge& edge : mesh.GetSharpEdges()) {
        if (edge.v1 >= 0 && edge.v2 >= 0
            && static_cast<size_t>(edge.v1) < level.vertices.size()
            && static_cast<size_t>(edge.v2) < level.vertices.size()
            && edge.v1 != edge.v2) {
            level.sharp_edges.insert(edge_key(
                static_cast<size_t>(edge.v1), static_cast<size_t>(edge.v2)));
        }
    }
    return true;
}

SubdivisionLevel subdivide(const SubdivisionLevel& source) {
    SubdivisionLevel result;
    const size_t vertex_count = source.vertices.size();
    const size_t face_count = source.faces.size();
    std::vector<Vec3> face_points(face_count);
    std::vector<std::vector<size_t>> vertex_faces(vertex_count);
    std::vector<std::set<size_t>> vertex_neighbors(vertex_count);
    std::map<EdgeKey, std::vector<size_t>> edge_faces;
    for (size_t face_index = 0; face_index < face_count; ++face_index) {
        const Quad& face = source.faces[face_index];
        std::vector<Vec3> points;
        points.reserve(4);
        for (size_t corner = 0; corner < 4; ++corner) {
            const size_t first = face[corner];
            const size_t second = face[(corner + 1) % 4];
            points.push_back(source.vertices[first]);
            vertex_faces[first].push_back(face_index);
            vertex_neighbors[first].insert(second);
            vertex_neighbors[second].insert(first);
            edge_faces[edge_key(first, second)].push_back(face_index);
        }
        face_points[face_index] = average(points);
    }

    result.vertices.resize(vertex_count);
    for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
        const Vec3 original = source.vertices[vertex];
        std::vector<size_t> crease_neighbors;
        for (size_t neighbor : vertex_neighbors[vertex]) {
            const EdgeKey edge = edge_key(vertex, neighbor);
            const auto owners = edge_faces.find(edge);
            if (source.sharp_edges.count(edge)
                || owners == edge_faces.end() || owners->second.size() != 2) {
                crease_neighbors.push_back(neighbor);
            }
        }
        if (crease_neighbors.size() >= 3) {
            result.vertices[vertex] = original;
        } else if (crease_neighbors.size() == 2) {
            result.vertices[vertex] = original * 0.75f
                + (source.vertices[crease_neighbors[0]]
                    + source.vertices[crease_neighbors[1]]) * 0.125f;
        } else {
            const size_t n = vertex_faces[vertex].size();
            if (n == 0 || vertex_neighbors[vertex].empty()) {
                result.vertices[vertex] = original;
                continue;
            }
            std::vector<Vec3> around_faces;
            for (size_t face : vertex_faces[vertex])
                around_faces.push_back(face_points[face]);
            std::vector<Vec3> edge_midpoints;
            for (size_t neighbor : vertex_neighbors[vertex])
                edge_midpoints.push_back((original + source.vertices[neighbor]) * 0.5f);
            const Vec3 f = average(around_faces);
            const Vec3 r = average(edge_midpoints);
            result.vertices[vertex] = (f + r * 2.0f
                + original * static_cast<float>(n - 3))
                * (1.0f / static_cast<float>(n));
        }
    }

    std::map<EdgeKey, size_t> edge_points;
    for (const auto& item : edge_faces) {
        const EdgeKey edge = item.first;
        const Vec3 midpoint = (source.vertices[edge.first]
            + source.vertices[edge.second]) * 0.5f;
        Vec3 point = midpoint;
        if (!source.sharp_edges.count(edge) && item.second.size() == 2) {
            point = (source.vertices[edge.first] + source.vertices[edge.second]
                + face_points[item.second[0]] + face_points[item.second[1]]) * 0.25f;
        }
        edge_points[edge] = result.vertices.size();
        result.vertices.push_back(point);
    }
    std::vector<size_t> face_point_indices(face_count);
    for (size_t face = 0; face < face_count; ++face) {
        face_point_indices[face] = result.vertices.size();
        result.vertices.push_back(face_points[face]);
    }

    result.children.resize(face_count);
    result.faces.reserve(face_count * 4);
    for (size_t face_index = 0; face_index < face_count; ++face_index) {
        const Quad& face = source.faces[face_index];
        for (size_t corner = 0; corner < 4; ++corner) {
            const size_t previous = (corner + 3) % 4;
            const Quad child{
                face[corner],
                edge_points.at(edge_key(face[corner], face[(corner + 1) % 4])),
                face_point_indices[face_index],
                edge_points.at(edge_key(face[previous], face[corner]))};
            result.children[face_index][corner] = result.faces.size();
            result.faces.push_back(child);
        }
    }
    for (const EdgeKey& edge : source.sharp_edges) {
        const auto midpoint = edge_points.find(edge);
        if (midpoint == edge_points.end()) continue;
        result.sharp_edges.insert(edge_key(edge.first, midpoint->second));
        result.sharp_edges.insert(edge_key(midpoint->second, edge.second));
    }
    return result;
}

void project_to_limit(SubdivisionLevel& level) {
    // Catmull-Clark limit mask: (n*n*P + 4*sum(edge neighbours)
    // + sum(opposite face vertices)) / (n*(n+5)).
    // https://patents.google.com/patent/USRE41677E1/en (limit-position stencil).
    std::vector<std::set<size_t>> neighbours(level.vertices.size());
    std::vector<Vec3> opposites(level.vertices.size());
    std::map<EdgeKey, int> owners;
    for (const auto& face : level.faces) {
        for (size_t i = 0; i < 4; ++i) {
            const auto a = face[i], b = face[(i + 1) % 4];
            neighbours[a].insert(b); neighbours[b].insert(a);
            opposites[a] = opposites[a] + level.vertices[face[(i + 2) % 4]];
            ++owners[edge_key(a, b)];
        }
    }
    auto projected = level.vertices;
    for (size_t i = 0; i < projected.size(); ++i) {
        std::vector<size_t> creases;
        Vec3 sum{};
        for (auto j : neighbours[i]) {
            sum = sum + level.vertices[j];
            const auto edge = edge_key(i, j);
            if (level.sharp_edges.count(edge) || owners[edge] != 2) creases.push_back(j);
        }
        const float n = static_cast<float>(neighbours[i].size());
        if (creases.size() >= 3 || n == 0) continue;
        if (creases.size() == 2) {
            projected[i] = (level.vertices[i] * 4.f + level.vertices[creases[0]]
                + level.vertices[creases[1]]) * (1.f / 6.f);
        } else {
            projected[i] = (level.vertices[i] * (n * n) + sum * 4.f + opposites[i])
                * (1.f / (n * (n + 5.f)));
        }
    }
    level.vertices = std::move(projected);
}

bool patch_grid(const std::vector<SubdivisionLevel>& levels,
                size_t original_face,
                std::vector<std::vector<Vec3>>& grid) {
    const int extent = 1 << (levels.size() - 1);
    grid.assign(extent + 1, std::vector<Vec3>(extent + 1));
    std::vector<std::vector<bool>> assigned(extent + 1, std::vector<bool>(extent + 1));
    using GridPoint = std::pair<int, int>;
    using GridQuad = std::array<GridPoint, 4>;
    const auto midpoint = [](GridPoint first, GridPoint second) {
        return GridPoint{(first.first + second.first) / 2,
                         (first.second + second.second) / 2};
    };
    const auto child_coordinates = [&](const GridQuad& parent,
                                       size_t corner) {
        const size_t next = (corner + 1) % 4;
        const size_t previous = (corner + 3) % 4;
        const GridPoint center{
            (parent[0].first + parent[1].first
                + parent[2].first + parent[3].first) / 4,
            (parent[0].second + parent[1].second
                + parent[2].second + parent[3].second) / 4};
        return GridQuad{parent[corner],
                        midpoint(parent[corner], parent[next]),
                        center,
                        midpoint(parent[previous], parent[corner])};
    };
    const GridQuad root{{{0, 0}, {extent, 0}, {extent, extent}, {0, extent}}};
    const auto visit = [&](const auto& self, size_t level, size_t face,
                           const GridQuad& coordinates) -> void {
        if (level + 1 == levels.size()) {
            const Quad& quad = levels[level].faces[face];
            for (size_t corner = 0; corner < 4; ++corner) {
                const int gx = coordinates[corner].first;
                const int gy = coordinates[corner].second;
                grid[gx][gy] = levels[level].vertices[quad[corner]];
                assigned[gx][gy] = true;
            }
            return;
        }
        for (size_t corner = 0; corner < 4; ++corner)
            self(self, level + 1, levels[level + 1].children[face][corner],
                 child_coordinates(coordinates, corner));
    };
    visit(visit, 0, original_face, root);
    for (const auto& column : assigned)
        for (bool value : column)
            if (!value) return false;
    return true;
}
}

CatmullClarkSurfaceBuildResult BuildCatmullClarkSurfaceBody(
        const CMesh3D& control_mesh,
        double sewing_tolerance,
        int subdivision_steps) {
    CatmullClarkSurfaceBuildResult result;
    try {
        if (subdivision_steps < 2 || subdivision_steps > 4) return result;
        SubdivisionLevel control;
        if (!make_control_level(control_mesh, control)) return result;
        std::vector<SubdivisionLevel> levels;
        levels.push_back(std::move(control));
        for (int step = 0; step < subdivision_steps; ++step)
            levels.push_back(subdivide(levels.back()));
        if (subdivision_steps >= 3) project_to_limit(levels.back());
        const int grid_size = (1 << subdivision_steps) + 1;

        BRepBuilderAPI_Sewing sewing(
            std::max(1.0e-7, sewing_tolerance), Standard_True,
            Standard_True, Standard_True, Standard_False);
        for (size_t face = 0; face < levels.front().faces.size(); ++face) {
            std::vector<std::vector<Vec3>> grid;
            if (!patch_grid(levels, face, grid)) continue;
            TColgp_Array2OfPnt points(1, grid_size, 1, grid_size);
            for (int u = 0; u < grid_size; ++u)
                for (int v = 0; v < grid_size; ++v) {
                    const Vec3 point = grid[u][v];
                    points.SetValue(u + 1, v + 1,
                        gp_Pnt(point.x, point.y, point.z));
                }
            // Uniform parameters make a shared row of
            // points generate the exact same boundary curve on both patches.
            GeomAPI_PointsToBSplineSurface interpolation;
            interpolation.Interpolate(
                points, Approx_IsoParametric, Standard_False);
            if (!interpolation.IsDone() || interpolation.Surface().IsNull())
                continue;
            BRepBuilderAPI_MakeFace make_face(
                interpolation.Surface(), 1.0e-7);
            if (!make_face.IsDone() || make_face.Face().IsNull()) continue;
            sewing.Add(make_face.Face());
            result.control_faces.push_back(make_face.Face());
            ++result.patch_count;
        }
        if (result.patch_count != levels.front().faces.size()) return {};
        sewing.Perform();
        result.free_edge_count = static_cast<size_t>(sewing.NbFreeEdges());
        result.shape = sewing.SewedShape();
        if (result.shape.IsNull()) return result;

        if (sewing.NbFreeEdges() == 0 && sewing.NbMultipleEdges() == 0) {
            TopoDS_Shell shell;
            int shell_count = 0;
            if (result.shape.ShapeType() == TopAbs_SHELL) {
                shell = TopoDS::Shell(result.shape);
                shell_count = 1;
            } else {
                for (TopExp_Explorer it(result.shape, TopAbs_SHELL);
                     it.More(); it.Next()) {
                    shell = TopoDS::Shell(it.Current());
                    ++shell_count;
                }
            }
            if (shell_count == 1 && !shell.IsNull()) {
                BRepBuilderAPI_MakeSolid make_solid(shell);
                if (make_solid.IsDone() && !make_solid.Solid().IsNull()) {
                    ShapeFix_Solid fixer(make_solid.Solid());
                    fixer.Perform();
                    const TopoDS_Shape solid = fixer.Solid();
                    if (!solid.IsNull() && BRepCheck_Analyzer(solid).IsValid()) {
                        result.shape = solid;
                        result.solid = true;
                    }
                }
            }
        }
    } catch (const Standard_Failure&) {
        return {};
    }
    return result;
}
