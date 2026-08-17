#include "PolyhedronShapeBuilder.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <ShapeFix_Solid.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr float kPointTolerance = 1.0e-5f;

float LengthSquared(Vec3 value) {
    return dot(value, value);
}

Vec3 Normalize(Vec3 value) {
    const float length = std::sqrt(LengthSquared(value));
    return length > 1.0e-12f ? value * (1.0f / length) : Vec3{};
}

Vec3 ProjectToAxis(Vec3 point, Vec3 origin, Vec3 axis) {
    return origin + axis * dot(point - origin, axis);
}

Vec3 RotateAroundAxis(Vec3 point,
                      Vec3 origin,
                      Vec3 axis,
                      double angle) {
    const Vec3 relative = point - origin;
    const Vec3 axial = axis * dot(axis, relative);
    const Vec3 radial = relative - axial;
    const float cosine = static_cast<float>(std::cos(angle));
    const float sine = static_cast<float>(std::sin(angle));
    return origin
        + axial
        + radial * cosine
        + cross(axis, radial) * sine;
}

bool SamePoint(Vec3 first, Vec3 second) {
    return LengthSquared(first - second)
        <= kPointTolerance * kPointTolerance;
}

bool IsOnAxis(Vec3 point, Vec3 origin, Vec3 axis) {
    return SamePoint(point, ProjectToAxis(point, origin, axis));
}

bool AddFace(BRepBuilderAPI_Sewing& sewing,
             const std::vector<Vec3>& points) {
    BRepBuilderAPI_MakePolygon polygon;
    for (Vec3 point : points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return false;
    }
    BRepBuilderAPI_MakeFace face(polygon.Wire(), true);
    if (!face.IsDone() || face.Face().IsNull()) {
        return false;
    }
    sewing.Add(face.Face());
    return true;
}

bool AddSideFace(BRepBuilderAPI_Sewing& sewing,
                 const std::vector<Vec3>& points,
                 size_t& face_count) {
    if (points.size() != 4) {
        if (!AddFace(sewing, points)) {
            return false;
        }
        ++face_count;
        return true;
    }
    if (AddFace(sewing, points)) {
        ++face_count;
        return true;
    }

    // A fillet or Bezier sample can make the four vertices very slightly
    // non-planar after conversion to world coordinates. Two triangles still
    // describe the intended ruled facet without requiring a perfect plane.
    const std::vector<Vec3> first_triangle = {
        points[0], points[1], points[2]};
    const std::vector<Vec3> second_triangle = {
        points[0], points[2], points[3]};
    if (!AddFace(sewing, first_triangle)
        || !AddFace(sewing, second_triangle)) {
        return false;
    }
    face_count += 2;
    return true;
}

bool FinishSewing(BRepBuilderAPI_Sewing& sewing,
                  TopoDS_Shape& result) {
    sewing.Perform();
    const TopoDS_Shape sewed = sewing.SewedShape();
    TopoDS_Shell shell;
    if (sewed.ShapeType() == TopAbs_SHELL) {
        shell = TopoDS::Shell(sewed);
    } else {
        TopExp_Explorer explorer(sewed, TopAbs_SHELL);
        if (!explorer.More()) {
            return false;
        }
        shell = TopoDS::Shell(explorer.Current());
    }

    BRepBuilderAPI_MakeSolid solid_builder(shell);
    if (!solid_builder.IsDone() || solid_builder.Solid().IsNull()) {
        return false;
    }
    ShapeFix_Solid fixer(solid_builder.Solid());
    fixer.Perform();
    const TopoDS_Shape fixed_shape = fixer.Solid();
    if (fixed_shape.IsNull()
        || !BRepCheck_Analyzer(fixed_shape).IsValid()) {
        return false;
    }
    ShapeUpgrade_UnifySameDomain unify(
        fixed_shape, true, true, false);
    unify.SetLinearTolerance(1.0e-5);
    unify.Build();
    result = unify.Shape();
    if (result.IsNull() || !BRepCheck_Analyzer(result).IsValid()) {
        result = fixed_shape;
    }
    GProp_GProps properties;
    BRepGProp::VolumeProperties(result, properties);
    return std::fabs(properties.Mass()) > 1.0e-8;
}
}

bool BuildPolyhedronShape(const std::vector<Vec3>& profile_points,
                          bool profile_closed,
                          Vec3 axis_origin,
                          Vec3 axis_direction,
                          int turns,
                          TopoDS_Shape& result) {
    result.Nullify();
    turns = std::max(3, turns);
    axis_direction = Normalize(axis_direction);
    if (LengthSquared(axis_direction) <= 1.0e-12f) {
        return false;
    }

    std::vector<Vec3> contour;
    contour.reserve(profile_points.size() + 2);
    for (Vec3 point : profile_points) {
        if (contour.empty() || !SamePoint(contour.back(), point)) {
            contour.push_back(point);
        }
    }
    if (contour.size() > 1 && SamePoint(contour.front(), contour.back())) {
        contour.pop_back();
    }
    if (contour.size() < 2) {
        return false;
    }

    if (contour.size() < (profile_closed ? 3u : 2u)) {
        return false;
    }

    std::vector<std::vector<Vec3>> rings(
        static_cast<size_t>(turns),
        std::vector<Vec3>(contour.size()));
    constexpr double two_pi = 6.28318530717958647692;
    for (int turn = 0; turn < turns; ++turn) {
        const double angle = two_pi * turn / turns;
        for (size_t point = 0; point < contour.size(); ++point) {
            rings[static_cast<size_t>(turn)][point] = RotateAroundAxis(
                contour[point], axis_origin, axis_direction, angle);
        }
    }

    try {
        BRepBuilderAPI_Sewing sewing(1.0e-5);
        size_t face_count = 0;
        for (int turn = 0; turn < turns; ++turn) {
            const size_t current = static_cast<size_t>(turn);
            const size_t next = static_cast<size_t>((turn + 1) % turns);
            const size_t segment_count = profile_closed
                ? contour.size()
                : contour.size() - 1;
            for (size_t point = 0; point < segment_count; ++point) {
                const size_t following = (point + 1) % contour.size();
                const bool first_on_axis = IsOnAxis(
                    contour[point], axis_origin, axis_direction);
                const bool second_on_axis = IsOnAxis(
                    contour[following], axis_origin, axis_direction);
                if (first_on_axis && second_on_axis) {
                    continue;
                }

                std::vector<Vec3> face_points;
                if (first_on_axis) {
                    face_points = {
                        rings[current][point],
                        rings[next][following],
                        rings[current][following]};
                } else if (second_on_axis) {
                    face_points = {
                        rings[current][point],
                        rings[next][point],
                        rings[current][following]};
                } else {
                    face_points = {
                        rings[current][point],
                        rings[next][point],
                        rings[next][following],
                        rings[current][following]};
                }
                if (!AddSideFace(sewing, face_points, face_count)) {
                    return false;
                }
            }
        }
        if (!profile_closed) {
            if (!IsOnAxis(
                    contour.front(), axis_origin, axis_direction)) {
                std::vector<Vec3> first_cap;
                first_cap.reserve(rings.size());
                for (auto ring = rings.rbegin(); ring != rings.rend(); ++ring) {
                    first_cap.push_back(ring->front());
                }
                if (!AddFace(sewing, first_cap)) {
                    return false;
                }
                ++face_count;
            }
            if (!IsOnAxis(
                    contour.back(), axis_origin, axis_direction)) {
                std::vector<Vec3> last_cap;
                last_cap.reserve(rings.size());
                for (const auto& ring : rings) {
                    last_cap.push_back(ring.back());
                }
                if (!AddFace(sewing, last_cap)) {
                    return false;
                }
                ++face_count;
            }
        }
        if (face_count < 4) {
            return false;
        }

        return FinishSewing(sewing, result);
    } catch (...) {
        result.Nullify();
        return false;
    }
}

bool BuildPolyhedronShapeFromProfileFace(const TopoDS_Face& profile_face,
                                         Vec3 axis_origin,
                                         Vec3 axis_direction,
                                         int turns,
                                         TopoDS_Shape& result) {
    result.Nullify();
    turns = std::max(3, turns);
    axis_direction = Normalize(axis_direction);
    if (profile_face.IsNull()
        || LengthSquared(axis_direction) <= 1.0e-12f) {
        return false;
    }

    try {
        std::vector<TopoDS_Edge> profile_edges;
        for (TopExp_Explorer explorer(profile_face, TopAbs_EDGE);
             explorer.More();
             explorer.Next()) {
            profile_edges.push_back(TopoDS::Edge(explorer.Current()));
        }
        if (profile_edges.size() < 2) {
            return false;
        }

        const gp_Ax1 rotation_axis(
            gp_Pnt(axis_origin.x, axis_origin.y, axis_origin.z),
            gp_Dir(axis_direction.x, axis_direction.y, axis_direction.z));
        constexpr double two_pi = 6.28318530717958647692;
        BRepBuilderAPI_Sewing sewing(1.0e-5);
        size_t face_count = 0;
        for (int turn = 0; turn < turns; ++turn) {
            gp_Trsf current_transform;
            current_transform.SetRotation(
                rotation_axis, two_pi * turn / turns);
            gp_Trsf next_transform;
            next_transform.SetRotation(
                rotation_axis, two_pi * (turn + 1) / turns);

            for (const TopoDS_Edge& profile_edge : profile_edges) {
                TopoDS_Vertex first_vertex;
                TopoDS_Vertex last_vertex;
                TopExp::Vertices(
                    profile_edge, first_vertex, last_vertex, true);
                if (!first_vertex.IsNull() && !last_vertex.IsNull()) {
                    const gp_Pnt first = BRep_Tool::Pnt(first_vertex);
                    const gp_Pnt last = BRep_Tool::Pnt(last_vertex);
                    if (IsOnAxis(
                            {static_cast<float>(first.X()),
                             static_cast<float>(first.Y()),
                             static_cast<float>(first.Z())},
                            axis_origin,
                            axis_direction)
                        && IsOnAxis(
                            {static_cast<float>(last.X()),
                             static_cast<float>(last.Y()),
                             static_cast<float>(last.Z())},
                            axis_origin,
                            axis_direction)) {
                        continue;
                    }
                }

                const TopoDS_Edge current_edge = TopoDS::Edge(
                    BRepBuilderAPI_Transform(
                        profile_edge, current_transform, true).Shape());
                const TopoDS_Edge next_edge = TopoDS::Edge(
                    BRepBuilderAPI_Transform(
                        profile_edge, next_transform, true).Shape());
                const TopoDS_Face side_face =
                    BRepFill::Face(current_edge, next_edge);
                if (side_face.IsNull()) {
                    return false;
                }
                sewing.Add(side_face);
                ++face_count;
            }
        }
        if (face_count < 4) {
            return false;
        }
        return FinishSewing(sewing, result);
    } catch (...) {
        result.Nullify();
        return false;
    }
}

bool BuildPolyhedronShapeFromOpenProfileWire(
    const TopoDS_Wire& profile_wire,
    Vec3 axis_origin,
    Vec3 axis_direction,
    int turns,
    TopoDS_Shape& result) {
    result.Nullify();
    turns = std::max(3, turns);
    axis_direction = Normalize(axis_direction);
    if (profile_wire.IsNull()
        || LengthSquared(axis_direction) <= 1.0e-12f) {
        return false;
    }

    try {
        std::vector<TopoDS_Edge> profile_edges;
        for (TopExp_Explorer explorer(profile_wire, TopAbs_EDGE);
             explorer.More();
             explorer.Next()) {
            profile_edges.push_back(TopoDS::Edge(explorer.Current()));
        }
        TopoDS_Vertex first_vertex;
        TopoDS_Vertex last_vertex;
        TopExp::Vertices(profile_wire, first_vertex, last_vertex);
        if (profile_edges.empty()
            || first_vertex.IsNull()
            || last_vertex.IsNull()) {
            return false;
        }

        const gp_Pnt first_point = BRep_Tool::Pnt(first_vertex);
        const gp_Pnt last_point = BRep_Tool::Pnt(last_vertex);
        const Vec3 first{
            static_cast<float>(first_point.X()),
            static_cast<float>(first_point.Y()),
            static_cast<float>(first_point.Z())};
        const Vec3 last{
            static_cast<float>(last_point.X()),
            static_cast<float>(last_point.Y()),
            static_cast<float>(last_point.Z())};

        const gp_Ax1 rotation_axis(
            gp_Pnt(axis_origin.x, axis_origin.y, axis_origin.z),
            gp_Dir(axis_direction.x, axis_direction.y, axis_direction.z));
        constexpr double two_pi = 6.28318530717958647692;
        BRepBuilderAPI_Sewing sewing(1.0e-5);
        size_t face_count = 0;
        for (int turn = 0; turn < turns; ++turn) {
            gp_Trsf current_transform;
            current_transform.SetRotation(
                rotation_axis, two_pi * turn / turns);
            gp_Trsf next_transform;
            next_transform.SetRotation(
                rotation_axis, two_pi * (turn + 1) / turns);
            for (const TopoDS_Edge& profile_edge : profile_edges) {
                const TopoDS_Edge current_edge = TopoDS::Edge(
                    BRepBuilderAPI_Transform(
                        profile_edge, current_transform, true).Shape());
                const TopoDS_Edge next_edge = TopoDS::Edge(
                    BRepBuilderAPI_Transform(
                        profile_edge, next_transform, true).Shape());
                const TopoDS_Face side_face =
                    BRepFill::Face(current_edge, next_edge);
                if (side_face.IsNull()) {
                    return false;
                }
                sewing.Add(side_face);
                ++face_count;
            }
        }

        const auto add_end_cap =
            [&](Vec3 endpoint, bool reverse) {
                if (IsOnAxis(endpoint, axis_origin, axis_direction)) {
                    return true;
                }
                std::vector<Vec3> cap;
                cap.reserve(static_cast<size_t>(turns));
                for (int turn = 0; turn < turns; ++turn) {
                    const int index = reverse ? turns - 1 - turn : turn;
                    cap.push_back(RotateAroundAxis(
                        endpoint,
                        axis_origin,
                        axis_direction,
                        two_pi * index / turns));
                }
                if (!AddFace(sewing, cap)) {
                    return false;
                }
                ++face_count;
                return true;
            };
        if (!add_end_cap(first, true)
            || !add_end_cap(last, false)
            || face_count < 4) {
            return false;
        }
        return FinishSewing(sewing, result);
    } catch (...) {
        result.Nullify();
        return false;
    }
}
