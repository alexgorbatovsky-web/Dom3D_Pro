#include "BeamShapeBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr double kTolerance = 1.0e-6;

TopoDS_Wire MakePolygon(std::vector<gp_Pnt> points, bool reverse = false) {
    if (reverse) {
        std::reverse(points.begin(), points.end());
    }

    BRepBuilderAPI_MakePolygon polygon;
    for (const gp_Pnt& point : points) {
        polygon.Add(point);
    }
    polygon.Close();
    return polygon.IsDone() ? polygon.Wire() : TopoDS_Wire();
}

TopoDS_Wire MakeRectangle(double half_width,
                          double half_height,
                          bool reverse = false) {
    return MakePolygon({
        gp_Pnt(-half_width, -half_height, 0.0),
        gp_Pnt(half_width, -half_height, 0.0),
        gp_Pnt(half_width, half_height, 0.0),
        gp_Pnt(-half_width, half_height, 0.0)
    }, reverse);
}

bool MakePrism(const TopoDS_Face& face, double length, TopoDS_Shape& result) {
    if (face.IsNull()) {
        return false;
    }

    BRepPrimAPI_MakePrism prism(face, gp_Vec(0.0, 0.0, length), true);
    prism.Build();
    if (!prism.IsDone() || prism.Shape().IsNull()) {
        return false;
    }

    result = prism.Shape();
    return BRepCheck_Analyzer(result).IsValid();
}

bool MakePolygonPrism(const std::vector<gp_Pnt>& points,
                      double length,
                      TopoDS_Shape& result) {
    const TopoDS_Wire wire = MakePolygon(points);
    if (wire.IsNull()) {
        return false;
    }

    BRepBuilderAPI_MakeFace face(wire);
    return face.IsDone() && MakePrism(face.Face(), length, result);
}
}

bool BuildBeamShape(BeamSectionType type,
                    double width,
                    double height,
                    double length,
                    double thickness,
                    TopoDS_Shape& result) {
    result.Nullify();
    if (width <= kTolerance
        || height <= kTolerance
        || length <= kTolerance
        || thickness <= kTolerance) {
        return false;
    }

    const double half_width = width * 0.5;
    const double half_height = height * 0.5;
    const double half_thickness = thickness * 0.5;

    switch (type) {
    case BeamSectionType::LBeam:
        if (thickness >= std::min(width, height)) {
            return false;
        }
        return MakePolygonPrism({
            gp_Pnt(-half_width, -half_height, 0.0),
            gp_Pnt(half_width, -half_height, 0.0),
            gp_Pnt(half_width, -half_height + thickness, 0.0),
            gp_Pnt(-half_width + thickness, -half_height + thickness, 0.0),
            gp_Pnt(-half_width + thickness, half_height, 0.0),
            gp_Pnt(-half_width, half_height, 0.0)
        }, length, result);

    case BeamSectionType::Square: {
        if (2.0 * thickness >= std::min(width, height)) {
            return false;
        }
        const TopoDS_Wire outer = MakeRectangle(half_width, half_height);
        const TopoDS_Wire inner = MakeRectangle(
            half_width - thickness, half_height - thickness, true);
        if (outer.IsNull() || inner.IsNull()) {
            return false;
        }
        BRepBuilderAPI_MakeFace face(outer);
        face.Add(inner);
        face.Build();
        return face.IsDone() && MakePrism(face.Face(), length, result);
    }

    case BeamSectionType::Channel:
        if (2.0 * thickness >= height || thickness >= width) {
            return false;
        }
        return MakePolygonPrism({
            gp_Pnt(-half_width, -half_height, 0.0),
            gp_Pnt(half_width, -half_height, 0.0),
            gp_Pnt(half_width, -half_height + thickness, 0.0),
            gp_Pnt(-half_width + thickness, -half_height + thickness, 0.0),
            gp_Pnt(-half_width + thickness, half_height - thickness, 0.0),
            gp_Pnt(half_width, half_height - thickness, 0.0),
            gp_Pnt(half_width, half_height, 0.0),
            gp_Pnt(-half_width, half_height, 0.0)
        }, length, result);

    case BeamSectionType::TBeam:
        if (thickness >= std::min(width, height)) {
            return false;
        }
        return MakePolygonPrism({
            gp_Pnt(-half_width, half_height - thickness, 0.0),
            gp_Pnt(-half_thickness, half_height - thickness, 0.0),
            gp_Pnt(-half_thickness, -half_height, 0.0),
            gp_Pnt(half_thickness, -half_height, 0.0),
            gp_Pnt(half_thickness, half_height - thickness, 0.0),
            gp_Pnt(half_width, half_height - thickness, 0.0),
            gp_Pnt(half_width, half_height, 0.0),
            gp_Pnt(-half_width, half_height, 0.0)
        }, length, result);

    case BeamSectionType::IBeam:
        if (2.0 * thickness >= height || thickness >= width) {
            return false;
        }
        return MakePolygonPrism({
            gp_Pnt(-half_width, -half_height, 0.0),
            gp_Pnt(half_width, -half_height, 0.0),
            gp_Pnt(half_width, -half_height + thickness, 0.0),
            gp_Pnt(half_thickness, -half_height + thickness, 0.0),
            gp_Pnt(half_thickness, half_height - thickness, 0.0),
            gp_Pnt(half_width, half_height - thickness, 0.0),
            gp_Pnt(half_width, half_height, 0.0),
            gp_Pnt(-half_width, half_height, 0.0),
            gp_Pnt(-half_width, half_height - thickness, 0.0),
            gp_Pnt(-half_thickness, half_height - thickness, 0.0),
            gp_Pnt(-half_thickness, -half_height + thickness, 0.0),
            gp_Pnt(-half_width, -half_height + thickness, 0.0)
        }, length, result);

    case BeamSectionType::Tube: {
        const double outer_radius = std::min(width, height) * 0.5;
        if (thickness >= outer_radius) {
            return false;
        }
        const gp_Ax2 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
        const TopoDS_Wire outer = BRepBuilderAPI_MakeWire(
            BRepBuilderAPI_MakeEdge(gp_Circ(axis, outer_radius)).Edge()).Wire();
        TopoDS_Wire inner = BRepBuilderAPI_MakeWire(
            BRepBuilderAPI_MakeEdge(gp_Circ(axis, outer_radius - thickness)).Edge()).Wire();
        inner.Reverse();
        BRepBuilderAPI_MakeFace face(outer);
        face.Add(inner);
        face.Build();
        return face.IsDone() && MakePrism(face.Face(), length, result);
    }
    }

    return false;
}
