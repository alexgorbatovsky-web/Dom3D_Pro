#include "solid/SketchFeatureShapeBuilder.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

TopoDS_Face make_profile() {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(3.0, 3.0, 10.0));
    polygon.Add(gp_Pnt(7.0, 3.0, 10.0));
    polygon.Add(gp_Pnt(7.0, 7.0, 10.0));
    polygon.Add(gp_Pnt(3.0, 7.0, 10.0));
    polygon.Close();
    require(polygon.IsDone(), "Profile wire was not built.");
    BRepBuilderAPI_MakeFace face(polygon.Wire(), true);
    require(face.IsDone(), "Profile face was not built.");
    return face.Face();
}

double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
}

int main() {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const TopoDS_Face profile = make_profile();
    const double box_volume = volume(box);

    TopoDS_Shape protrusion;
    require(BuildSketchFeatureShape(box, profile, {0.0f, 0.0f, 1.0f},
                                    2.0, 0.0,
                                    SketchFeatureOperation::Protrusion,
                                    protrusion),
            "Protrusion was not built.");
    require(volume(protrusion) > box_volume + 31.0,
            "Protrusion did not increase body volume.");

    TopoDS_Shape cut;
    require(BuildSketchFeatureShape(box, profile, {0.0f, 0.0f, 1.0f},
                                    2.0, 0.0,
                                    SketchFeatureOperation::Cut,
                                    cut),
            "Cut was not built.");
    require(volume(cut) < box_volume - 31.0,
            "Cut did not decrease body volume.");

    TopoDS_Shape tapered_cut;
    require(BuildSketchFeatureShape(box, profile, {0.0f, 0.0f, 1.0f},
                                    2.0, 5.0,
                                    SketchFeatureOperation::Cut,
                                    tapered_cut),
            "Tapered cut was not built.");
    require(std::fabs(volume(tapered_cut) - box_volume) > 1.0,
            "Tapered cut did not modify body volume.");
    return EXIT_SUCCESS;
}
