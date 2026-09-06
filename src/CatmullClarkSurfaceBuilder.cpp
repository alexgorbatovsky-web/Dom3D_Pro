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

bool patch_grid(const SubdivisionLevel& first,
                const SubdivisionLevel& second,
                size_t original_face,
                std::array<std::array<Vec3, 5>, 5>& grid) {
    std::array<std::array<bool, 5>, 5> assigned{};
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
    const GridQuad root{{{0, 0}, {4, 0}, {4, 4}, {0, 4}}};
    for (size_t first_quadrant = 0; first_quadrant < 4; ++first_quadrant) {
        const size_t first_face = first.children[original_face][first_quadrant];
        const GridQuad first_coordinates =
            child_coordinates(root, first_quadrant);
        for (size_t second_quadrant = 0; second_quadrant < 4; ++second_quadrant) {
            const size_t second_face = second.children[first_face][second_quadrant];
            const GridQuad coordinates =
                child_coordinates(first_coordinates, second_quadrant);
            const Quad& quad = second.faces[second_face];
            for (size_t corner = 0; corner < 4; ++corner) {
                const int gx = coordinates[corner].first;
                const int gy = coordinates[corner].second;
                grid[gx][gy] = second.vertices[quad[corner]];
                assigned[gx][gy] = true;
            }
        }
    }
    for (const auto& column : assigned)
        for (bool value : column)
            if (!value) return false;
    return true;
}
}

CatmullClarkSurfaceBuildResult BuildCatmullClarkSurfaceBody(
        const CMesh3D& control_mesh,
        double sewing_tolerance) {
    CatmullClarkSurfaceBuildResult result;
    try {
        SubdivisionLevel control;
        if (!make_control_level(control_mesh, control)) return result;
        const SubdivisionLevel first = subdivide(control);
        const SubdivisionLevel second = subdivide(first);

        BRepBuilderAPI_Sewing sewing(
            std::max(1.0e-7, sewing_tolerance), Standard_True,
            Standard_True, Standard_True, Standard_False);
        for (size_t face = 0; face < control.faces.size(); ++face) {
            std::array<std::array<Vec3, 5>, 5> grid{};
            if (!patch_grid(first, second, face, grid)) continue;
            TColgp_Array2OfPnt points(1, 5, 1, 5);
            for (int u = 0; u < 5; ++u)
                for (int v = 0; v < 5; ++v) {
                    const Vec3 point = grid[u][v];
                    points.SetValue(u + 1, v + 1,
                        gp_Pnt(point.x, point.y, point.z));
                }
            // Uniform parameters are essential here. They reproduce the old
            // five-by-five spline interpolation while making a shared row of
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
            ++result.patch_count;
        }
        if (result.patch_count != control.faces.size()) return {};
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
