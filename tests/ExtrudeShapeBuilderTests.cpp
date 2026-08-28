#include "ExtrudeShapeBuilder.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <Geom_BezierCurve.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

TopoDS_Face make_bezier_profile()
{
    const gp_Pnt bottom_left(0.0, 0.0, 0.0);
    const gp_Pnt bottom_right(10.0, 0.0, 0.0);
    const gp_Pnt top_right(10.0, 6.0, 0.0);
    const gp_Pnt top_left(0.0, 6.0, 0.0);

    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, top_right);
    poles.SetValue(2, gp_Pnt(7.0, 9.0, 0.0));
    poles.SetValue(3, gp_Pnt(3.0, 9.0, 0.0));
    poles.SetValue(4, top_left);

    BRepBuilderAPI_MakeWire wire;
    wire.Add(BRepBuilderAPI_MakeEdge(bottom_left, bottom_right).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(bottom_right, top_right).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(new Geom_BezierCurve(poles)).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(top_left, bottom_left).Edge());
    require(wire.IsDone(), "Bezier profile wire was not built.");

    BRepBuilderAPI_MakeFace face(wire.Wire(), true);
    require(face.IsDone(), "Bezier profile face was not built.");
    return face.Face();
}

TopoDS_Face make_rounded_bezier_profile()
{
    const gp_Pnt bottom_left(0.0, 0.0, 0.0);
    const gp_Pnt bottom_right(10.0, 0.0, 0.0);
    const gp_Pnt right_arc_start(10.0, 4.0, 0.0);
    const gp_Pnt right_arc_middle(9.4142135623731, 5.4142135623731, 0.0);
    const gp_Pnt right_arc_end(8.0, 6.0, 0.0);
    const gp_Pnt left_arc_start(2.0, 6.0, 0.0);
    const gp_Pnt left_arc_middle(0.585786437626905, 5.4142135623731, 0.0);
    const gp_Pnt left_arc_end(0.0, 4.0, 0.0);

    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, bottom_left);
    poles.SetValue(2, gp_Pnt(3.0, -1.0, 0.0));
    poles.SetValue(3, gp_Pnt(7.0, -1.0, 0.0));
    poles.SetValue(4, bottom_right);

    GC_MakeArcOfCircle right_arc(right_arc_start, right_arc_middle, right_arc_end);
    GC_MakeArcOfCircle left_arc(left_arc_start, left_arc_middle, left_arc_end);
    require(right_arc.IsDone() && left_arc.IsDone(), "Rounded profile arcs were not built.");

    BRepBuilderAPI_MakeWire wire;
    wire.Add(BRepBuilderAPI_MakeEdge(new Geom_BezierCurve(poles)).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(bottom_right, right_arc_start).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(right_arc.Value()).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(right_arc_end, left_arc_start).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(left_arc.Value()).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(left_arc_end, bottom_left).Edge());
    require(wire.IsDone(), "Rounded Bezier profile wire was not built.");

    BRepBuilderAPI_MakeFace face(wire.Wire(), true);
    require(face.IsDone(), "Rounded Bezier profile face was not built.");
    return face.Face();
}

TopoDS_Face make_trimmed_bezier_profile()
{
    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, gp_Pnt(10.0, 6.0, 0.0));
    poles.SetValue(2, gp_Pnt(7.0, 9.0, 0.0));
    poles.SetValue(3, gp_Pnt(3.0, 9.0, 0.0));
    poles.SetValue(4, gp_Pnt(0.0, 6.0, 0.0));
    const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
    constexpr double first_parameter = 0.15;
    constexpr double last_parameter = 0.85;
    const gp_Pnt curve_start = curve->Value(first_parameter);
    const gp_Pnt curve_end = curve->Value(last_parameter);
    const gp_Pnt bottom_left(curve_end.X(), 0.0, 0.0);
    const gp_Pnt bottom_right(curve_start.X(), 0.0, 0.0);

    BRepBuilderAPI_MakeWire wire;
    wire.Add(BRepBuilderAPI_MakeEdge(bottom_left, bottom_right).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(bottom_right, curve_start).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(curve, first_parameter, last_parameter).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(curve_end, bottom_left).Edge());
    require(wire.IsDone(), "Trimmed Bezier profile wire was not built.");

    BRepBuilderAPI_MakeFace face(wire.Wire(), true);
    require(face.IsDone(), "Trimmed Bezier profile face was not built.");
    return face.Face();
}

void check_extrude(const TopoDS_Face& face,
                   double distance,
                   double taper,
                   int expected_face_count)
{
    const TopoDS_Shape result = BuildExtrudeShape(face, {0.0f, 0.0f, 1.0f}, distance, taper);
    require(!result.IsNull(), "Bezier profile tapered extrusion returned a null shape.");
    require(BRepCheck_Analyzer(result).IsValid(), "Bezier profile tapered extrusion is invalid.");
    int face_count = 0;
    for (TopExp_Explorer explorer(result, TopAbs_FACE); explorer.More(); explorer.Next()) {
        ++face_count;
    }
    require(face_count == expected_face_count,
            "Bezier profile tapered extrusion contains transition face fragments.");
}
}

int main()
{
    const TopoDS_Face face = make_bezier_profile();
    check_extrude(face, 3.0, 5.0, 6);
    check_extrude(face, 3.0, -5.0, 6);
    check_extrude(face, -3.0, 5.0, 6);
    check_extrude(face, -3.0, -5.0, 6);

    const TopoDS_Face rounded_face = make_rounded_bezier_profile();
    check_extrude(rounded_face, 4.4, 15.0, 8);
    check_extrude(rounded_face, 4.4, -15.0, 8);
    check_extrude(rounded_face, -4.4, 15.0, 8);
    check_extrude(rounded_face, -4.4, -15.0, 8);

    const TopoDS_Face trimmed_face = make_trimmed_bezier_profile();
    check_extrude(trimmed_face, 3.0, 5.0, 6);
    check_extrude(trimmed_face, 3.0, -5.0, 6);
    return EXIT_SUCCESS;
}
