#include "solid/OffsetFaceShapeBuilder.h"
#include "solid/TrimShapeBuilder.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

TopoDS_Face face_of_type(const TopoDS_Shape& shape,
                         GeomAbs_SurfaceType type,
                         bool last = false) {
    TopoDS_Face found;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        if (BRepAdaptor_Surface(face, true).GetType() == type) {
            found = face;
            if (!last) {
                break;
            }
        }
    }
    return found;
}
}

int main() {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const TopoDS_Face box_face = face_of_type(box, GeomAbs_Plane, true);
    require(!box_face.IsNull(), "Box face was not found.");
    const double box_volume = volume(box);

    TopoDS_Shape box_out;
    require(BuildOffsetFaceShape(box, box_face, 2.0, box_out),
            "Positive box face offset failed.");
    require(volume(box_out) > box_volume, "Positive offset did not grow box.");

    TopoDS_Shape box_in;
    require(BuildOffsetFaceShape(box, box_face, -2.0, box_in),
            "Negative box face offset failed.");
    require(volume(box_in) < box_volume, "Negative offset did not shrink box.");

    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(5.0, 10.0).Shape();
    const TopoDS_Face cylinder_face =
        face_of_type(cylinder, GeomAbs_Cylinder);
    require(!cylinder_face.IsNull(), "Cylinder face was not found.");
    const double cylinder_volume = volume(cylinder);
    TopoDS_Shape cylinder_out;
    require(BuildOffsetFaceShape(
                cylinder, cylinder_face, 1.0, cylinder_out),
            "Cylindrical face offset failed.");
    require(volume(cylinder_out) > cylinder_volume,
            "Cylindrical offset did not grow cylinder.");

    const TopoDS_Shape trim_box =
        BRepPrimAPI_MakeBox(gp_Pnt(-10.0, -10.0, -10.0),
                            20.0, 20.0, 20.0).Shape();
    const TopoDS_Shape trim_cylinder =
        BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(0.0, 0.0, -20.0), gp_Dir(0.0, 0.0, 1.0)),
            12.0,
            40.0).Shape();
    const TopoDS_Face trim_cutter =
        face_of_type(trim_cylinder, GeomAbs_Cylinder);
    TopoDS_Shape trimmed;
    require(TrimSolidByFace(trim_box, trim_cutter, false, trimmed),
            "Curved trim for offset test failed.");
    const TopoDS_Face trimmed_curved_face =
        face_of_type(trimmed, GeomAbs_Cylinder);
    require(!trimmed_curved_face.IsNull(),
            "Trimmed cylindrical face was not found.");
    TopoDS_Shape trimmed_offset;
    require(BuildOffsetFaceShape(
                trimmed, trimmed_curved_face, 2.0, trimmed_offset),
            "Offset after curved trim failed.");
    require(std::fabs(volume(trimmed_offset) - volume(trimmed)) > 1.0,
            "Offset after curved trim did not change the body.");
    return EXIT_SUCCESS;
}
