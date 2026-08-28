#include "../src/solid/PolyhedronShapeBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

double Volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return std::fabs(properties.Mass());
}

int FaceCount(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE);
         explorer.More();
         explorer.Next()) {
        ++count;
    }
    return count;
}
}

int main() {
    const std::vector<Vec3> closed_profile = {
        {5.0f, 0.0f, -10.0f},
        {8.0f, 0.0f, -10.0f},
        {8.0f, 0.0f, 10.0f},
        {5.0f, 0.0f, 10.0f}};
    TopoDS_Shape closed_result;
    Require(
        BuildPolyhedronShape(
            closed_profile, true, {}, {0.0f, 0.0f, 1.0f}, 8, closed_result),
        "Closed polyhedron profile failed.");
    Require(BRepCheck_Analyzer(closed_result).IsValid(),
            "Closed polyhedron is invalid.");
    const double expected_volume =
        4.0 * std::sin(3.14159265358979323846 / 4.0)
        * (64.0 - 25.0) * 20.0;
    Require(std::fabs(Volume(closed_result) - expected_volume) < 1.0e-3,
            "Closed polyhedron volume is incorrect.");
    Require(FaceCount(closed_result) == 18,
            "Coplanar polyhedron end faces were not unified.");

    const std::vector<Vec3> open_profile = {
        {5.0f, 0.0f, -10.0f},
        {8.0f, 0.0f, 0.0f},
        {5.0f, 0.0f, 10.0f}};
    TopoDS_Shape open_result;
    Require(
        BuildPolyhedronShape(
            open_profile, false, {}, {0.0f, 0.0f, 1.0f}, 6, open_result),
        "Open polyhedron profile failed.");
    Require(BRepCheck_Analyzer(open_result).IsValid(),
            "Open polyhedron is invalid.");
    Require(Volume(open_result) > 1.0, "Open polyhedron has no volume.");

    std::vector<Vec3> rounded_profile = {
        {5.0f, 0.0f, -10.0f},
        {8.0f, 0.0f, -10.0f},
        {8.0f, 0.0f, 8.0f}};
    constexpr int arc_segments = 9;
    for (int segment = 1; segment <= arc_segments; ++segment) {
        const double angle =
            static_cast<double>(segment) / arc_segments
            * 3.14159265358979323846 / 2.0;
        rounded_profile.push_back({
            static_cast<float>(6.0 + 2.0 * std::cos(angle)),
            0.0f,
            static_cast<float>(8.0 + 2.0 * std::sin(angle))});
    }
    rounded_profile.push_back({5.0f, 0.0f, 10.0f});
    TopoDS_Shape rounded_result;
    Require(
        BuildPolyhedronShape(
            rounded_profile,
            true,
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            rounded_result),
        "Rounded polyhedron profile failed.");
    Require(BRepCheck_Analyzer(rounded_result).IsValid(),
            "Rounded polyhedron is invalid.");
    Require(Volume(rounded_result) > 1.0,
            "Rounded polyhedron has no volume.");

    rounded_profile.front().x = 0.0f;
    rounded_profile.back().x = 0.0f;
    TopoDS_Shape rounded_axis_result;
    Require(
        BuildPolyhedronShape(
            rounded_profile,
            true,
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            rounded_axis_result),
        "Rounded profile touching the axis failed.");
    Require(BRepCheck_Analyzer(rounded_axis_result).IsValid(),
            "Rounded profile touching the axis is invalid.");

    std::vector<Vec3> slightly_non_planar_profile = rounded_profile;
    for (std::size_t index = 3;
         index + 1 < slightly_non_planar_profile.size();
         ++index) {
        slightly_non_planar_profile[index].y =
            index % 2 == 0 ? 2.0e-5f : -2.0e-5f;
    }
    TopoDS_Shape non_planar_result;
    Require(
        BuildPolyhedronShape(
            slightly_non_planar_profile,
            true,
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            non_planar_result),
        "Slightly non-planar rounded profile failed.");
    Require(BRepCheck_Analyzer(non_planar_result).IsValid(),
            "Slightly non-planar rounded profile is invalid.");

    BRepBuilderAPI_MakeWire exact_wire;
    const gp_Pnt exact_inner_bottom(5.0, 0.0, -10.0);
    const gp_Pnt exact_outer_bottom(8.0, 0.0, -10.0);
    const gp_Pnt exact_arc_start(8.0, 0.0, 8.0);
    const gp_Pnt exact_arc_middle(
        6.0 + std::sqrt(2.0), 0.0, 8.0 + std::sqrt(2.0));
    const gp_Pnt exact_arc_end(6.0, 0.0, 10.0);
    const gp_Pnt exact_inner_top(5.0, 0.0, 10.0);
    exact_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_inner_bottom, exact_outer_bottom).Edge());
    exact_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_outer_bottom, exact_arc_start).Edge());
    const GC_MakeArcOfCircle exact_arc(
        exact_arc_start, exact_arc_middle, exact_arc_end);
    Require(exact_arc.IsDone(), "Exact test fillet arc failed.");
    exact_wire.Add(BRepBuilderAPI_MakeEdge(exact_arc.Value()).Edge());
    exact_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_arc_end, exact_inner_top).Edge());
    exact_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_inner_top, exact_inner_bottom).Edge());
    Require(exact_wire.IsDone(), "Exact rounded profile wire failed.");
    const BRepBuilderAPI_MakeFace exact_face(exact_wire.Wire(), true);
    Require(exact_face.IsDone(), "Exact rounded profile face failed.");
    TopoDS_Shape exact_rounded_result;
    Require(
        BuildPolyhedronShapeFromProfileFace(
            exact_face.Face(),
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            exact_rounded_result),
        "Exact rounded polyhedron profile failed.");
    Require(BRepCheck_Analyzer(exact_rounded_result).IsValid(),
            "Exact rounded polyhedron is invalid.");
    Require(FaceCount(exact_rounded_result) == 26,
            "Exact fillet was split into unnecessary surface strips.");

    BRepBuilderAPI_MakeWire exact_open_wire;
    exact_open_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_inner_bottom, exact_outer_bottom).Edge());
    exact_open_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_outer_bottom, exact_arc_start).Edge());
    exact_open_wire.Add(BRepBuilderAPI_MakeEdge(exact_arc.Value()).Edge());
    exact_open_wire.Add(BRepBuilderAPI_MakeEdge(
        exact_arc_end, exact_inner_top).Edge());
    Require(exact_open_wire.IsDone(),
            "Exact open rounded profile wire failed.");
    TopoDS_Shape exact_open_result;
    Require(
        BuildPolyhedronShapeFromOpenProfileWire(
            exact_open_wire.Wire(),
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            exact_open_result),
        "Exact open rounded polyhedron profile failed.");
    Require(BRepCheck_Analyzer(exact_open_result).IsValid(),
            "Exact open rounded polyhedron is invalid.");

    const std::vector<gp_Pnt> zigzag_points = {
        {5.0, 0.0, -12.0},
        {5.0, 0.0, -2.0},
        {3.5, 0.0, 0.0},
        {3.5, 0.0, 10.0},
        {8.0, 0.0, 12.0},
        {8.0, 0.0, 20.0},
        {6.0, 0.0, 23.0}};
    BRepBuilderAPI_MakeWire zigzag_wire;
    for (std::size_t index = 1; index < zigzag_points.size(); ++index) {
        zigzag_wire.Add(BRepBuilderAPI_MakeEdge(
            zigzag_points[index - 1], zigzag_points[index]).Edge());
    }
    Require(zigzag_wire.IsDone(), "Open zigzag profile wire failed.");
    std::vector<Vec3> zigzag_profile_points;
    for (const gp_Pnt& point : zigzag_points) {
        zigzag_profile_points.push_back({
            static_cast<float>(point.X()),
            static_cast<float>(point.Y()),
            static_cast<float>(point.Z())});
    }
    TopoDS_Shape zigzag_point_result;
    Require(
        BuildPolyhedronShape(
            zigzag_profile_points,
            false,
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            zigzag_point_result),
        "Open zigzag point profile failed.");
    TopoDS_Shape zigzag_result;
    Require(
        BuildPolyhedronShapeFromOpenProfileWire(
            zigzag_wire.Wire(),
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            zigzag_result),
        "Open zigzag polyhedron profile failed.");
    Require(BRepCheck_Analyzer(zigzag_result).IsValid(),
            "Open zigzag polyhedron is invalid.");

    const gp_Pnt multi_start(8.0, 0.0, -12.0);
    const gp_Pnt multi_arc1_start(8.0, 0.0, -2.0);
    const gp_Pnt multi_arc1_mid(7.5, 0.0, -0.7);
    const gp_Pnt multi_arc1_end(6.0, 0.0, 0.0);
    const gp_Pnt multi_arc2_start(6.0, 0.0, 10.0);
    const gp_Pnt multi_arc2_mid(6.5, 0.0, 11.3);
    const gp_Pnt multi_arc2_end(8.0, 0.0, 12.0);
    const gp_Pnt multi_end(8.0, 0.0, 20.0);
    const GC_MakeArcOfCircle multi_arc1(
        multi_arc1_start, multi_arc1_mid, multi_arc1_end);
    const GC_MakeArcOfCircle multi_arc2(
        multi_arc2_start, multi_arc2_mid, multi_arc2_end);
    Require(multi_arc1.IsDone() && multi_arc2.IsDone(),
            "Multiple open profile arcs failed.");
    BRepBuilderAPI_MakeWire multi_arc_wire;
    multi_arc_wire.Add(BRepBuilderAPI_MakeEdge(
        multi_start, multi_arc1_start).Edge());
    multi_arc_wire.Add(BRepBuilderAPI_MakeEdge(multi_arc1.Value()).Edge());
    multi_arc_wire.Add(BRepBuilderAPI_MakeEdge(
        multi_arc1_end, multi_arc2_start).Edge());
    multi_arc_wire.Add(BRepBuilderAPI_MakeEdge(multi_arc2.Value()).Edge());
    multi_arc_wire.Add(BRepBuilderAPI_MakeEdge(
        multi_arc2_end, multi_end).Edge());
    Require(multi_arc_wire.IsDone(),
            "Multiple-radius open profile wire failed.");
    TopoDS_Shape multi_arc_result;
    Require(
        BuildPolyhedronShapeFromOpenProfileWire(
            multi_arc_wire.Wire(),
            {},
            {0.0f, 0.0f, 1.0f},
            8,
            multi_arc_result),
        "Multiple-radius open polyhedron failed.");
    Require(BRepCheck_Analyzer(multi_arc_result).IsValid(),
            "Multiple-radius open polyhedron is invalid.");
    Require(FaceCount(multi_arc_result) == 42,
            "Multiple-radius open profile was split into strips.");
    for (int tested_turns : {4, 6, 10, 12, 16}) {
        TopoDS_Shape varied_turns_result;
        Require(
            BuildPolyhedronShapeFromOpenProfileWire(
                multi_arc_wire.Wire(),
                {},
                {0.0f, 0.0f, 1.0f},
                tested_turns,
                varied_turns_result),
            "Multiple-radius open polyhedron failed for varied turns.");
        Require(
            FaceCount(varied_turns_result) == tested_turns * 5 + 2,
            "Varied-turns radius was split into surface strips.");
    }

    TopoDS_Shape invalid_result;
    Require(
        !BuildPolyhedronShape(
            {{1.0f, 0.0f, 0.0f}}, false, {}, {0.0f, 0.0f, 1.0f}, 3, invalid_result),
        "One-point profile must be rejected.");
    return 0;
}
