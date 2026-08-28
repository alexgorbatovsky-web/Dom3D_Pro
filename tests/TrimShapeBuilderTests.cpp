#include "../src/solid/PlaneShapeBuilder.h"
#include "../src/solid/TrimShapeBuilder.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

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
    return properties.Mass();
}

double CenterZ(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.CentreOfMass().Z();
}
}

int main() {
    const TopoDS_Shape box =
        BRepPrimAPI_MakeBox(gp_Pnt(-10.0, -10.0, -10.0), 20.0, 20.0, 20.0)
            .Shape();

    TopoDS_Face plane;
    Require(
        BuildFinitePlaneFace(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 100.0, plane),
        "Plane face could not be built.");

    TopoDS_Shape positive;
    TopoDS_Shape negative;
    Require(
        TrimSolidByFace(box, plane, true, positive),
        "Positive plane trim failed.");
    Require(
        TrimSolidByFace(box, plane, false, negative),
        "Negative plane trim failed.");
    Require(BRepCheck_Analyzer(positive).IsValid(), "Positive trim is invalid.");
    Require(BRepCheck_Analyzer(negative).IsValid(), "Negative trim is invalid.");
    Require(std::fabs(Volume(positive) - 4000.0) < 1.0e-6,
            "Positive plane trim volume is incorrect.");
    Require(std::fabs(Volume(negative) - 4000.0) < 1.0e-6,
            "Negative plane trim volume is incorrect.");
    Require(CenterZ(positive) * CenterZ(negative) < 0.0,
            "Plane directions kept the same side.");

    // The reference Plane is finite only for display. Trimming must use its
    // infinite mathematical plane even when that rectangle is wholly inside
    // a recess and does not touch the solid.
    const TopoDS_Shape outer =
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 100.0, 20.0, 100.0)
            .Shape();
    const TopoDS_Shape recess =
        BRepPrimAPI_MakeBox(gp_Pnt(30.0, -1.0, 50.0), 40.0, 22.0, 51.0)
            .Shape();
    BRepAlgoAPI_Cut recess_cut(outer, recess);
    recess_cut.Build();
    Require(recess_cut.IsDone(), "Recessed test solid could not be built.");
    TopoDS_Face small_plane_in_recess;
    Require(
        BuildFinitePlaneFace(
            {50.0f, 10.0f, 75.0f}, {0.0f, 0.0f, 1.0f}, 8.0,
            small_plane_in_recess),
        "Small plane in recess could not be built.");
    TopoDS_Shape recessed_positive;
    TopoDS_Shape recessed_negative;
    Require(
        TrimSolidByFace(
            recess_cut.Shape(), small_plane_in_recess, true,
            recessed_positive),
        "Plane trim failed when visible Plane was inside a recess.");
    Require(
        TrimSolidByFace(
            recess_cut.Shape(), small_plane_in_recess, false,
            recessed_negative),
        "Reverse plane trim failed when visible Plane was inside a recess.");
    Require(std::fabs(
                Volume(recessed_positive) + Volume(recessed_negative)
                - Volume(recess_cut.Shape()))
                < 1.0e-4,
            "Plane inside recess did not partition the complete body.");

    // Furniture moulding frames are represented by a compound of solids,
    // rather than by one primitive solid. They must be trimmed as one object.
    TopoDS_Compound frame;
    BRep_Builder frame_builder;
    frame_builder.MakeCompound(frame);
    frame_builder.Add(frame,
        BRepPrimAPI_MakeBox(gp_Pnt(-50.0, -50.0, 0.0), 100.0, 10.0, 18.0).Shape());
    frame_builder.Add(frame,
        BRepPrimAPI_MakeBox(gp_Pnt(-50.0, 40.0, 0.0), 100.0, 10.0, 18.0).Shape());
    frame_builder.Add(frame,
        BRepPrimAPI_MakeBox(gp_Pnt(-50.0, -40.0, 0.0), 10.0, 80.0, 18.0).Shape());
    frame_builder.Add(frame,
        BRepPrimAPI_MakeBox(gp_Pnt(40.0, -40.0, 0.0), 10.0, 80.0, 18.0).Shape());
    TopoDS_Face frame_plane;
    Require(
        BuildFinitePlaneFace(
            {0.0f, 0.0f, 9.0f}, {1.0f, 0.0f, 0.0f}, 8.0, frame_plane),
        "Frame cutting plane could not be built.");
    TopoDS_Shape half_frame;
    Require(
        TrimSolidByFace(frame, frame_plane, true, half_frame),
        "Compound furniture frame could not be trimmed.");
    Require(Volume(half_frame) > 1.0 && Volume(half_frame) < Volume(frame),
        "Compound furniture frame trim kept an invalid volume.");

    const gp_Ax2 circle_axis(
        gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Wire circle = BRepBuilderAPI_MakeWire(
        BRepBuilderAPI_MakeEdge(gp_Circ(circle_axis, 5.0)).Edge()).Wire();
    const TopoDS_Face circle_face = BRepBuilderAPI_MakeFace(circle).Face();
    TopoDS_Shape profile_inside;
    TopoDS_Shape profile_outside;
    Require(
        TrimSolidByClosedProfile(
            box, circle_face, {0.0f, 0.0f, 1.0f}, true, profile_inside),
        "Closed sketch inside trim failed.");
    Require(
        TrimSolidByClosedProfile(
            box, circle_face, {0.0f, 0.0f, 1.0f}, false, profile_outside),
        "Closed sketch outside trim failed.");
    Require(std::fabs(
                Volume(profile_inside) + Volume(profile_outside)
                - Volume(box))
                < 1.0e-5,
            "Closed sketch trim did not partition the body.");

    BRepBuilderAPI_MakeWire open_wire;
    open_wire.Add(BRepBuilderAPI_MakeEdge(
        gp_Pnt(-20.0, 0.0, 0.0), gp_Pnt(20.0, 0.0, 0.0)).Edge());
    TopoDS_Shape open_positive;
    TopoDS_Shape open_negative;
    Require(
        TrimSolidByOpenProfile(
            box,
            open_wire.Wire(),
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            true,
            open_positive),
        "Open sketch positive trim failed.");
    Require(
        TrimSolidByOpenProfile(
            box,
            open_wire.Wire(),
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            false,
            open_negative),
        "Open sketch negative trim failed.");
    Require(std::fabs(
                Volume(open_positive) + Volume(open_negative) - Volume(box))
                < 1.0e-5,
            "Open sketch trim did not partition the body.");

    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(0.0, 0.0, -20.0), gp_Dir(0.0, 0.0, 1.0)),
            5.0,
            40.0)
            .Shape();
    TopoDS_Face cylinder_side;
    for (TopExp_Explorer explorer(cylinder, TopAbs_FACE);
         explorer.More();
         explorer.Next()) {
        const TopoDS_Face candidate = TopoDS::Face(explorer.Current());
        if (BRepAdaptor_Surface(candidate).GetType() == GeomAbs_Cylinder) {
            cylinder_side = candidate;
            break;
        }
    }
    Require(!cylinder_side.IsNull(), "Cylinder side face was not found.");
    TopoDS_Shape curved_positive;
    TopoDS_Shape curved_negative;
    Require(
        TrimSolidByFace(box, cylinder_side, true, curved_positive),
        "Curved surface positive trim failed.");
    Require(
        TrimSolidByFace(box, cylinder_side, false, curved_negative),
        "Curved surface negative trim failed.");
    Require(Volume(curved_positive) > 1.0 && Volume(curved_negative) > 1.0,
            "Curved surface trim produced an empty side.");
    Require(std::fabs(
                Volume(curved_positive) + Volume(curved_negative) - Volume(box))
                < 1.0e-4,
            "Curved surface trim did not partition the body.");

    return 0;
}
