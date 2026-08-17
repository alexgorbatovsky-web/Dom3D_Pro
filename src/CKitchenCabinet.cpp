#include "CKitchenCabinet.h"
#include "SmartLine.h"

#ifdef Coord
#undef Coord
#endif
#ifdef String
#undef String
#endif
#ifdef LPCTSTR
#undef LPCTSTR
#endif
#ifdef Pixel
#undef Pixel
#endif

#include "solid/Solid.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <Geom_BezierCurve.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <Geom_BoundedCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <GeomConvert_CompCurveToBSplineCurve.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <cmath>
#include <ostream>
#include <utility>

namespace {
bool valid_body_type(KitchenCabinetBodyType type) {
    return type == KitchenCabinetBodyType::Straight
        || type == KitchenCabinetBodyType::Corner
        || type == KitchenCabinetBodyType::Radius
        || type == KitchenCabinetBodyType::Corner2
        || type == KitchenCabinetBodyType::Radius2
        || type == KitchenCabinetBodyType::Radius3
        || type == KitchenCabinetBodyType::Radius4;
}

bool valid_facade_type(KitchenCabinetFacadeType type) {
    return type == KitchenCabinetFacadeType::Open
        || type == KitchenCabinetFacadeType::SingleDoor
        || type == KitchenCabinetFacadeType::DoubleDoor;
}

bool valid_facade_style(KitchenCabinetFacadeStyle style) {
    return CFacadeFurniture::IsValidStyle(style);
}

TopoDS_Shape box_shape(double x, double y, double z,
                       double width, double depth, double height) {
    BRepPrimAPI_MakeBox builder(gp_Pnt(x, y, z), width, depth, height);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

// Kept for the corner-facade compatibility builders below.  New planar
// facade geometry lives in CFacadeFurniture.
TopoDS_Shape fillet_edges_on_coordinate_plane(const TopoDS_Shape& shape,
                                               bool x_plane,
                                               double coordinate,
                                               double radius) {
    if (shape.IsNull() || radius <= 0.0) {
        return shape;
    }
    std::vector<TopoDS_Shape> rounded_solids;
    for (TopExp_Explorer solid_explorer(shape, TopAbs_SOLID);
         solid_explorer.More(); solid_explorer.Next()) {
        const TopoDS_Solid solid = TopoDS::Solid(solid_explorer.Current());
        BRepFilletAPI_MakeFillet fillet(solid);
        int edge_count = 0;
        for (TopExp_Explorer edge_explorer(solid, TopAbs_EDGE);
             edge_explorer.More(); edge_explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
            TopoDS_Vertex first_vertex;
            TopoDS_Vertex last_vertex;
            TopExp::Vertices(edge, first_vertex, last_vertex);
            if (first_vertex.IsNull() || last_vertex.IsNull()) {
                continue;
            }
            const gp_Pnt first = BRep_Tool::Pnt(first_vertex);
            const gp_Pnt last = BRep_Tool::Pnt(last_vertex);
            const double first_coordinate = x_plane ? first.X() : first.Y();
            const double last_coordinate = x_plane ? last.X() : last.Y();
            if (std::abs(first_coordinate - coordinate) <= 1.0e-7
                && std::abs(last_coordinate - coordinate) <= 1.0e-7) {
                fillet.Add(radius, edge);
                ++edge_count;
            }
        }
        if (edge_count > 0) {
            fillet.Build();
        }
        rounded_solids.push_back(
            edge_count > 0 && fillet.IsDone() ? fillet.Shape() : solid);
    }
    if (rounded_solids.empty()) {
        return shape;
    }
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& solid : rounded_solids) {
        builder.Add(compound, solid);
    }
    return compound;
}

Handle(Geom_BoundedCurve) bounded_curve(const TopoDS_Edge& edge) {
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (curve.IsNull() || last <= first) {
        return {};
    }
    return new Geom_TrimmedCurve(curve, first, last);
}

TopoDS_Edge joined_bspline_edge(const TopoDS_Edge& first_edge,
                                const TopoDS_Edge& second_edge) {
    const Handle(Geom_BoundedCurve) first_curve = bounded_curve(first_edge);
    const Handle(Geom_BoundedCurve) second_curve = bounded_curve(second_edge);
    if (first_curve.IsNull() || second_curve.IsNull()) {
        return {};
    }
    GeomConvert_CompCurveToBSplineCurve joined(first_curve);
    if (!joined.Add(second_curve, 1.0e-7, Standard_True, Standard_True, 1)) {
        return {};
    }
    BRepBuilderAPI_MakeEdge edge(joined.BSplineCurve());
    return edge.IsDone() ? edge.Edge() : TopoDS_Edge();
}

TopoDS_Edge ellipse_arc(double radius_x,
                        double radius_y,
                        double z,
                        double start_angle,
                        double end_angle) {
    constexpr double half_pi = 1.57079632679489661923;
    const bool major_axis_is_x = radius_x >= radius_y;
    const gp_Ax2 axes(
        gp_Pnt(0.0, 0.0, z),
        gp_Dir(0.0, 0.0, -1.0),
        major_axis_is_x ? gp_Dir(1.0, 0.0, 0.0) : gp_Dir(0.0, 1.0, 0.0));
    const gp_Elips ellipse(
        axes,
        major_axis_is_x ? radius_x : radius_y,
        major_axis_is_x ? radius_y : radius_x);
    const double parameter_offset = major_axis_is_x ? 0.0 : half_pi;
    BRepBuilderAPI_MakeEdge edge(
        ellipse,
        start_angle + parameter_offset,
        end_angle + parameter_offset);
    return edge.IsDone() ? edge.Edge() : TopoDS_Edge();
}

struct CircularSegmentGeometry {
    double half_width = 0.1;
    double sagitta = 0.1;
    double front_y = 0.0;
    double radius = 0.1;
    double center_y = 0.0;
    double start_angle = 0.0;
    double end_angle = 0.0;
};

CircularSegmentGeometry circular_segment_geometry(double half_width,
                                                   double sagitta,
                                                   double front_y) {
    CircularSegmentGeometry result;
    result.half_width = std::max(0.1, half_width);
    result.sagitta = std::clamp(sagitta, 0.1, result.half_width * 0.999);
    result.front_y = front_y;
    result.radius = (result.half_width * result.half_width
                     + result.sagitta * result.sagitta)
        / (2.0 * result.sagitta);
    result.center_y = result.front_y + result.radius;
    result.start_angle = std::acos(std::clamp(
        result.half_width / result.radius, -1.0, 1.0));
    result.end_angle = 3.14159265358979323846 - result.start_angle;
    return result;
}

gp_Pnt circular_segment_point(const CircularSegmentGeometry& segment,
                              double fraction,
                              double z) {
    const double angle = segment.start_angle
        + (segment.end_angle - segment.start_angle)
            * std::clamp(fraction, 0.0, 1.0);
    return {
        segment.radius * std::cos(angle),
        segment.center_y - segment.radius * std::sin(angle),
        z};
}

TopoDS_Edge circular_segment_arc(const CircularSegmentGeometry& segment,
                                 double z,
                                 double start_fraction = 0.0,
                                 double end_fraction = 1.0) {
    const gp_Ax2 axes(
        gp_Pnt(0.0, segment.center_y, z),
        gp_Dir(0.0, 0.0, -1.0),
        gp_Dir(1.0, 0.0, 0.0));
    const gp_Circ circle(axes, segment.radius);
    const double first = segment.start_angle
        + (segment.end_angle - segment.start_angle) * start_fraction;
    const double last = segment.start_angle
        + (segment.end_angle - segment.start_angle) * end_fraction;
    BRepBuilderAPI_MakeEdge edge(circle, first, last);
    return edge.IsDone() ? edge.Edge() : TopoDS_Edge();
}

struct SideRadiusGeometry {
    double half_width = 0.1;
    double back_y = 0.0;
    double front_y = 0.0;
    double straight_length = 0.0;
    double radius_x = 0.1;
    double radius_y = 0.1;
    double center_x = 0.0;
    double center_y = 0.0;
};

SideRadiusGeometry side_radius_geometry(
    const KitchenCabinetDefinition& definition,
    double straight_length,
    double inset) {
    SideRadiusGeometry result;
    result.half_width = std::max(0.1, definition.width * 0.5 - inset);
    result.back_y = definition.depth * 0.5 - inset;
    result.front_y = -definition.depth * 0.5 + inset;
    const double available_depth = std::max(0.1, result.back_y - result.front_y);
    result.straight_length = std::clamp(
        straight_length - inset, 0.0, std::max(0.0, available_depth - 0.1));
    result.radius_x = result.half_width * 2.0;
    result.radius_y = std::max(0.1, available_depth - result.straight_length);
    result.center_x = result.half_width;
    result.center_y = result.back_y - result.straight_length;
    return result;
}

gp_Pnt side_radius_point(const SideRadiusGeometry& geometry,
                         double fraction,
                         double z) {
    constexpr double half_pi = 1.57079632679489661923;
    const double angle = half_pi
        + half_pi * std::clamp(fraction, 0.0, 1.0);
    return {
        geometry.center_x + geometry.radius_x * std::cos(angle),
        geometry.center_y - geometry.radius_y * std::sin(angle),
        z};
}

TopoDS_Edge side_radius_arc(const SideRadiusGeometry& geometry,
                            double z,
                            double start_fraction = 0.0,
                            double end_fraction = 1.0) {
    constexpr double half_pi = 1.57079632679489661923;
    const bool major_axis_is_x = geometry.radius_x >= geometry.radius_y;
    const gp_Ax2 axes(
        gp_Pnt(geometry.center_x, geometry.center_y, z),
        gp_Dir(0.0, 0.0, -1.0),
        major_axis_is_x ? gp_Dir(1.0, 0.0, 0.0) : gp_Dir(0.0, 1.0, 0.0));
    const gp_Elips ellipse(
        axes,
        major_axis_is_x ? geometry.radius_x : geometry.radius_y,
        major_axis_is_x ? geometry.radius_y : geometry.radius_x);
    const double parameter_offset = major_axis_is_x ? 0.0 : half_pi;
    const double first = half_pi + half_pi * start_fraction + parameter_offset;
    const double last = half_pi + half_pi * end_fraction + parameter_offset;
    BRepBuilderAPI_MakeEdge edge(ellipse, first, last);
    return edge.IsDone() ? edge.Edge() : TopoDS_Edge();
}

TopoDS_Shape footprint_shape(const KitchenCabinetDefinition& definition,
                             double z,
                             double height,
                             double radius_inset = 0.0,
                             double corner_front_inset = 0.0) {
    const double half_width = std::max(0.1, definition.width * 0.5 - radius_inset);
    const double half_depth = std::max(0.1, definition.depth * 0.5 - radius_inset);
    const double facade_bulge = std::max(0.1, definition.facade_bulge - radius_inset);
    std::vector<std::pair<double, double>> footprint;
    if (definition.body_type == KitchenCabinetBodyType::Corner) {
        footprint = {
            {-half_width, -half_depth},
            {half_width, -half_depth},
            {half_width, 0.0},
            {0.0, 0.0},
            {0.0, half_depth},
            {-half_width, half_depth}};
    } else if (definition.body_type == KitchenCabinetBodyType::Corner2) {
        const double normal_scale = std::sqrt(
            1.0 / (half_width * half_width)
            + 1.0 / (half_depth * half_depth));
        const double x_edge_offset = std::min(
            half_width * 0.95,
            corner_front_inset * half_width * normal_scale);
        const double y_edge_offset = std::min(
            half_depth * 0.95,
            corner_front_inset * half_depth * normal_scale);
        footprint = {
            {-half_width, -half_depth},
            {half_width, -half_depth},
            {half_width, -y_edge_offset},
            {-x_edge_offset, half_depth},
            {-half_width, half_depth}};
    } else if (definition.body_type == KitchenCabinetBodyType::Radius3
               || definition.body_type == KitchenCabinetBodyType::Radius4) {
        const double straight_length = definition.body_type == KitchenCabinetBodyType::Radius4
            ? definition.radius_side_straight : 0.0;
        const SideRadiusGeometry geometry = side_radius_geometry(
            definition, straight_length, radius_inset);
        const gp_Pnt front_right = side_radius_point(geometry, 0.0, z);
        const gp_Pnt curve_left = side_radius_point(geometry, 1.0, z);
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(-geometry.half_width, geometry.back_y, z),
            gp_Pnt(geometry.half_width, geometry.back_y, z)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(geometry.half_width, geometry.back_y, z), front_right).Edge());
        const TopoDS_Edge arc = side_radius_arc(geometry, z);
        if (arc.IsNull()) {
            return {};
        }
        wire.Add(arc);
        if (geometry.straight_length > 1.0e-9) {
            BRepBuilderAPI_MakeEdge left_edge(
                curve_left,
                gp_Pnt(-geometry.half_width, geometry.back_y, z));
            if (!left_edge.IsDone()) {
                return {};
            }
            wire.Add(left_edge.Edge());
        }
        if (!wire.IsDone()) {
            return {};
        }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        if (!face.IsDone()) {
            return {};
        }
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
        prism.Build();
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
    } else if (definition.body_type == KitchenCabinetBodyType::Radius2) {
        const double front_y = -definition.depth * 0.5 + radius_inset;
        const double back_y = definition.depth * 0.5 - radius_inset;
        const double sagitta = std::max(0.1, definition.radius2_bulge - radius_inset);
        const CircularSegmentGeometry segment = circular_segment_geometry(
            half_width, sagitta, front_y);
        const gp_Pnt front_right = circular_segment_point(segment, 0.0, z);
        const gp_Pnt front_left = circular_segment_point(segment, 1.0, z);
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(-half_width, back_y, z),
            gp_Pnt(half_width, back_y, z)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(half_width, back_y, z), front_right).Edge());
        const TopoDS_Edge arc = circular_segment_arc(segment, z);
        if (arc.IsNull()) {
            return {};
        }
        wire.Add(arc);
        wire.Add(BRepBuilderAPI_MakeEdge(
            front_left, gp_Pnt(-half_width, back_y, z)).Edge());
        if (!wire.IsDone()) {
            return {};
        }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        if (!face.IsDone()) {
            return {};
        }
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
        prism.Build();
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
    } else if (definition.body_type == KitchenCabinetBodyType::Radius) {
        constexpr double pi = 3.14159265358979323846;
        BRepBuilderAPI_MakeWire wire;
        const gp_Pnt back_left(-half_width, half_depth, z);
        const gp_Pnt back_right(half_width, half_depth, z);
        const gp_Pnt front_right(half_width, 0.0, z);
        const gp_Pnt front_left(-half_width, 0.0, z);
        wire.Add(BRepBuilderAPI_MakeEdge(back_left, back_right).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(back_right, front_right).Edge());
        const TopoDS_Edge arc = ellipse_arc(
            half_width, facade_bulge, z, 0.0, pi);
        if (arc.IsNull()) {
            return {};
        }
        wire.Add(arc);
        wire.Add(BRepBuilderAPI_MakeEdge(front_left, back_left).Edge());
        if (!wire.IsDone()) {
            return {};
        }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        if (!face.IsDone()) {
            return {};
        }
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
        prism.Build();
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
    } else {
        footprint = {
            {-half_width, -half_depth},
            {half_width, -half_depth},
            {half_width, half_depth},
            {-half_width, half_depth}};
    }
    BRepBuilderAPI_MakePolygon polygon;
    for (const auto& point : footprint) {
        polygon.Add(gp_Pnt(point.first, point.second, z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeFace face(polygon.Wire());
    if (!face.IsDone()) {
        return {};
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
}

TopoDS_Shape radius_facade_shape(const KitchenCabinetDefinition& definition,
                                 double start_angle,
                                 double end_angle,
                                 double z,
                                 double height,
                                 double radial_thickness,
                                 double edge_gap) {
    const double inner_radius_x = std::max(
        0.1, definition.width * 0.5 - radial_thickness + edge_gap);
    const double inner_radius_y = std::max(
        0.1, definition.facade_bulge - radial_thickness + edge_gap);
    const double outer_radius_x = inner_radius_x + radial_thickness;
    const double outer_radius_y = inner_radius_y + radial_thickness;
    const TopoDS_Edge outer_arc = ellipse_arc(
        outer_radius_x, outer_radius_y, z, start_angle, end_angle);
    TopoDS_Edge inner_arc = ellipse_arc(
        inner_radius_x, inner_radius_y, z, start_angle, end_angle);
    if (outer_arc.IsNull() || inner_arc.IsNull()) {
        return {};
    }
    inner_arc.Reverse();
    const gp_Pnt outer_end(
        outer_radius_x * std::cos(end_angle),
        -outer_radius_y * std::sin(end_angle), z);
    const gp_Pnt inner_end(
        inner_radius_x * std::cos(end_angle),
        -inner_radius_y * std::sin(end_angle), z);
    const gp_Pnt inner_start(
        inner_radius_x * std::cos(start_angle),
        -inner_radius_y * std::sin(start_angle), z);
    const gp_Pnt outer_start(
        outer_radius_x * std::cos(start_angle),
        -outer_radius_y * std::sin(start_angle), z);
    BRepBuilderAPI_MakeWire wire;
    wire.Add(outer_arc);
    wire.Add(BRepBuilderAPI_MakeEdge(outer_end, inner_end).Edge());
    wire.Add(inner_arc);
    wire.Add(BRepBuilderAPI_MakeEdge(inner_start, outer_start).Edge());
    if (!wire.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeFace face(wire.Wire());
    if (!face.IsDone()) {
        return {};
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
}

TopoDS_Shape radius2_facade_shape(
    const KitchenCabinetDefinition& definition,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap) {
    const double outer_half_width = std::max(
        0.1, definition.width * 0.5 + edge_gap);
    const double outer_front_y = -definition.depth * 0.5 - edge_gap;
    const double outer_sagitta = std::max(
        0.1, definition.radius2_bulge + edge_gap);
    const CircularSegmentGeometry outer = circular_segment_geometry(
        outer_half_width, outer_sagitta, outer_front_y);
    CircularSegmentGeometry inner = outer;
    inner.radius = std::max(0.1, outer.radius - radial_thickness);
    const TopoDS_Edge outer_arc = circular_segment_arc(
        outer, z, start_fraction, end_fraction);
    TopoDS_Edge inner_arc = circular_segment_arc(
        inner, z, start_fraction, end_fraction);
    if (outer_arc.IsNull() || inner_arc.IsNull()) {
        return {};
    }
    inner_arc.Reverse();
    const gp_Pnt outer_end = circular_segment_point(outer, end_fraction, z);
    const gp_Pnt inner_end = circular_segment_point(inner, end_fraction, z);
    const gp_Pnt inner_start = circular_segment_point(inner, start_fraction, z);
    const gp_Pnt outer_start = circular_segment_point(outer, start_fraction, z);
    BRepBuilderAPI_MakeWire wire;
    wire.Add(outer_arc);
    wire.Add(BRepBuilderAPI_MakeEdge(outer_end, inner_end).Edge());
    wire.Add(inner_arc);
    wire.Add(BRepBuilderAPI_MakeEdge(inner_start, outer_start).Edge());
    if (!wire.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeFace face(wire.Wire());
    if (!face.IsDone()) {
        return {};
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
}

TopoDS_Shape side_radius_facade_shape(
    const KitchenCabinetDefinition& definition,
    double straight_length,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap) {
    const SideRadiusGeometry outer = side_radius_geometry(
        definition, straight_length, -edge_gap);
    SideRadiusGeometry inner = outer;
    inner.radius_x = std::max(0.1, outer.radius_x - radial_thickness);
    inner.radius_y = std::max(0.1, outer.radius_y - radial_thickness);
    const TopoDS_Edge outer_arc = side_radius_arc(
        outer, z, start_fraction, end_fraction);
    TopoDS_Edge inner_arc = side_radius_arc(
        inner, z, start_fraction, end_fraction);
    if (outer_arc.IsNull() || inner_arc.IsNull()) {
        return {};
    }
    const gp_Pnt outer_end = side_radius_point(outer, end_fraction, z);
    const gp_Pnt inner_end = side_radius_point(inner, end_fraction, z);
    const gp_Pnt inner_start = side_radius_point(inner, start_fraction, z);
    const gp_Pnt outer_start = side_radius_point(outer, start_fraction, z);
    const bool extend_to_straight_side = straight_length > 0.0
        && end_fraction >= 1.0 - 1.0e-9;
    BRepBuilderAPI_MakeWire wire;
    if (extend_to_straight_side) {
        const gp_Pnt outer_tip(
            outer_end.X(), outer_end.Y() + radial_thickness, z);
        const gp_Pnt inner_tip(
            inner_end.X(), inner_end.Y() + radial_thickness, z);
        const TopoDS_Edge outer_extension =
            BRepBuilderAPI_MakeEdge(outer_end, outer_tip).Edge();
        const TopoDS_Edge inner_extension =
            BRepBuilderAPI_MakeEdge(inner_end, inner_tip).Edge();
        const TopoDS_Edge outer_boundary =
            joined_bspline_edge(outer_arc, outer_extension);
        TopoDS_Edge inner_boundary =
            joined_bspline_edge(inner_arc, inner_extension);
        if (outer_boundary.IsNull() || inner_boundary.IsNull()) {
            return {};
        }
        inner_boundary.Reverse();
        wire.Add(outer_boundary);
        wire.Add(BRepBuilderAPI_MakeEdge(outer_tip, inner_tip).Edge());
        wire.Add(inner_boundary);
    } else {
        inner_arc.Reverse();
        wire.Add(outer_arc);
        wire.Add(BRepBuilderAPI_MakeEdge(outer_end, inner_end).Edge());
        wire.Add(inner_arc);
    }
    wire.Add(BRepBuilderAPI_MakeEdge(inner_start, outer_start).Edge());
    if (!wire.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeFace face(wire.Wire());
    if (!face.IsDone()) {
        return {};
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
}

TopoDS_Shape polygon_prism(const std::vector<std::pair<double, double>>& points,
                           double z,
                           double height) {
    if (points.size() < 3) {
        return {};
    }
    BRepBuilderAPI_MakePolygon polygon;
    for (const auto& point : points) {
        polygon.Add(gp_Pnt(point.first, point.second, z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeFace face(polygon.Wire());
    if (!face.IsDone()) {
        return {};
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
}

TopoDS_Shape diagonal_facade_shape(double first_x,
                                   double first_y,
                                   double second_x,
                                   double second_y,
                                   double z,
                                   double height,
                                   double thickness) {
    const double dx = second_x - first_x;
    const double dy = second_y - first_y;
    const double length = std::hypot(dx, dy);
    if (length <= 1.0e-9) {
        return {};
    }
    const double normal_x = dy / length;
    const double normal_y = -dx / length;
    return polygon_prism(
        {
            {first_x, first_y},
            {second_x, second_y},
            {second_x + normal_x * thickness,
             second_y + normal_y * thickness},
            {first_x + normal_x * thickness,
             first_y + normal_y * thickness},
        },
        z,
        height);
}

TopoDS_Shape rotate_about_z(const TopoDS_Shape& shape,
                            double hinge_x,
                            double hinge_y,
                            double angle_degrees) {
    if (shape.IsNull() || std::abs(angle_degrees) <= 1.0e-9) {
        return shape;
    }
    constexpr double pi = 3.14159265358979323846;
    gp_Trsf transform;
    transform.SetRotation(
        gp_Ax1(gp_Pnt(hinge_x, hinge_y, 0.0), gp_Dir(0.0, 0.0, 1.0)),
        angle_degrees * pi / 180.0);
    BRepBuilderAPI_Transform builder(shape, transform, true);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape planar_door_handle_shape(double facade_left,
                                      double facade_front,
                                      double facade_bottom,
                                      double facade_width,
                                      double facade_height,
                                      bool vertical,
                                      bool free_edge_on_right) {
    constexpr double handle_depth = 12.0;
    constexpr double handle_gap = 2.0;
    constexpr double handle_thickness = 10.0;
    const double handle_length = std::clamp(
        vertical ? facade_height * 0.22 : facade_width * 0.35,
        70.0,
        140.0);
    const double edge_margin = std::clamp(
        facade_width * 0.12, 28.0, 55.0);
    const double horizontal_center_inset = std::max(
        edge_margin, handle_length * 0.5 + 20.0);
    constexpr double horizontal_top_margin = 30.0;
    const double top_margin = vertical
        ? handle_length * 0.5 + horizontal_top_margin
        : horizontal_top_margin;
    const double x = vertical
        ? (free_edge_on_right
            ? facade_left + facade_width - edge_margin - handle_thickness * 0.5
            : facade_left + edge_margin - handle_thickness * 0.5)
        : (free_edge_on_right
            ? facade_left + facade_width - horizontal_center_inset
                - handle_length * 0.5
            : facade_left + horizontal_center_inset
                - handle_length * 0.5);
    const double z = vertical
        ? facade_bottom + facade_height - top_margin - handle_length * 0.5
        : facade_bottom + facade_height - top_margin - handle_thickness * 0.5;
    return box_shape(
        x,
        facade_front - handle_gap - handle_depth,
        z,
        vertical ? handle_thickness : handle_length,
        handle_depth,
        vertical ? handle_length : handle_thickness);
}

struct RoundDoorHandleShapes {
    TopoDS_Shape base;
    TopoDS_Shape face;
};

RoundDoorHandleShapes round_door_handle_shapes(const gp_Pnt& base,
                                                const gp_Dir& outward) {
    // Original Furniture3 profile. The first coordinate is the radius from
    // the revolution axis, the second is the distance out from the facade.
    const gp_Vec axis(outward);
    const gp_Vec radial(0.0, 0.0, 1.0);
    const auto profile_point = [&base, &axis, &radial](double radius,
                                                       double distance) {
        return base.Translated(axis.Multiplied(distance)
            + radial.Multiplied(radius));
    };
    const auto bezier_edge = [](std::initializer_list<gp_Pnt> poles) {
        TColgp_Array1OfPnt points(1, static_cast<Standard_Integer>(poles.size()));
        Standard_Integer index = 1;
        for (const gp_Pnt& point : poles) {
            points.SetValue(index++, point);
        }
        Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(points);
        BRepBuilderAPI_MakeEdge edge(curve);
        return edge.IsDone() ? edge.Edge() : TopoDS_Edge();
    };

    const gp_Pnt p1 = profile_point(7.0000, 0.0000);
    const gp_Pnt p2 = profile_point(7.0000, 2.2220);
    const gp_Pnt p3 = profile_point(4.8254, 2.8271);
    const gp_Pnt p4 = profile_point(2.5715, 4.0085);
    const gp_Pnt p5 = profile_point(2.2982, 12.6224);
    const gp_Pnt p6 = profile_point(9.0503, 14.5744);
    const gp_Pnt p7 = profile_point(14.8327, 15.2707);
    const gp_Pnt p8 = profile_point(15.0000, 19.5561);
    const gp_Pnt p9 = profile_point(13.6823, 19.5561);
    const gp_Pnt p10 = profile_point(13.4555, 20.7793);
    const gp_Pnt p11 = profile_point(11.2219, 21.6065);
    const gp_Pnt p12 = profile_point(0.0243, 21.6371);

    const TopoDS_Edge curve1 = bezier_edge({p2, p3, p4, p5});
    const TopoDS_Edge curve2 = bezier_edge({p5, p6, p7, p8});
    const TopoDS_Edge curve3 = bezier_edge({p9, p10, p11, p12});
    if (curve1.IsNull() || curve2.IsNull() || curve3.IsNull()) {
        return {};
    }

    BRepBuilderAPI_MakeWire base_profile;
    base_profile.Add(BRepBuilderAPI_MakeEdge(p1, p2).Edge());
    base_profile.Add(curve1);
    base_profile.Add(curve2);
    base_profile.Add(BRepBuilderAPI_MakeEdge(p8, p9).Edge());
    BRepBuilderAPI_MakeWire face_profile;
    face_profile.Add(curve3);
    if (!base_profile.IsDone() || !face_profile.IsDone()) {
        return {};
    }
    constexpr double two_pi = 6.28318530717958647692;
    const gp_Ax1 revolution_axis(base, outward);
    BRepPrimAPI_MakeRevol base_revolve(
        base_profile.Wire(), revolution_axis, two_pi, true);
    base_revolve.Build();
    BRepPrimAPI_MakeRevol face_revolve(
        face_profile.Wire(), revolution_axis, two_pi, true);
    face_revolve.Build();
    return {
        base_revolve.IsDone() ? base_revolve.Shape() : TopoDS_Shape(),
        face_revolve.IsDone() ? face_revolve.Shape() : TopoDS_Shape()};
}

void rotate_round_door_handle(RoundDoorHandleShapes& handle,
                              double hinge_x,
                              double hinge_y,
                              double angle_degrees) {
    handle.base = rotate_about_z(
        std::move(handle.base), hinge_x, hinge_y, angle_degrees);
    handle.face = rotate_about_z(
        std::move(handle.face), hinge_x, hinge_y, angle_degrees);
}

TopoDS_Shape place_along_panel(TopoDS_Shape shape,
                               double first_x,
                               double first_y,
                               double second_x,
                               double second_y) {
    const double angle = std::atan2(
        second_y - first_y, second_x - first_x)
        * 180.0 / 3.14159265358979323846;
    shape = rotate_about_z(shape, 0.0, 0.0, angle);
    if (shape.IsNull()) {
        return {};
    }
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(first_x, first_y, 0.0));
    BRepBuilderAPI_Transform builder(shape, translation, true);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape diagonal_door_handle_shape(double first_x,
                                        double first_y,
                                        double second_x,
                                        double second_y,
                                        double facade_bottom,
                                        double facade_height,
                                        bool vertical,
                                        bool free_edge_at_second) {
    const double panel_length = std::hypot(
        second_x - first_x, second_y - first_y);
    if (panel_length <= 1.0e-9) {
        return {};
    }
    return place_along_panel(
        planar_door_handle_shape(
            0.0, 0.0, facade_bottom,
            panel_length, facade_height,
            vertical, free_edge_at_second),
        first_x, first_y, second_x, second_y);
}

TopoDS_Shape orient_corner_front(TopoDS_Shape shape,
                                 bool horizontal,
                                 double thickness) {
    if (shape.IsNull()) {
        return {};
    }
    gp_Trsf front_orientation;
    front_orientation.SetMirror(gp_Ax2(
        horizontal
            ? gp_Pnt(0.0, thickness * 0.5, 0.0)
            : gp_Pnt(thickness * 0.5, 0.0, 0.0),
        horizontal
            ? gp_Dir(0.0, 1.0, 0.0)
            : gp_Dir(1.0, 0.0, 0.0)));
    BRepBuilderAPI_Transform builder(shape, front_orientation, true);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape cut_intersecting_compound_parts(
    const TopoDS_Shape& shape,
    const TopoDS_Shape& cutter,
    const Bnd_Box& cutter_bounds) {
    if (shape.IsNull()) return {};

    if (shape.ShapeType() == TopAbs_COMPOUND
        || shape.ShapeType() == TopAbs_COMPSOLID) {
        BRep_Builder builder;
        TopoDS_Compound result;
        builder.MakeCompound(result);
        bool has_part = false;
        for (TopoDS_Iterator iterator(shape); iterator.More(); iterator.Next()) {
            TopoDS_Shape part = cut_intersecting_compound_parts(
                iterator.Value(), cutter, cutter_bounds);
            if (!part.IsNull()) {
                builder.Add(result, part);
                has_part = true;
            }
        }
        return has_part ? TopoDS_Shape(result) : TopoDS_Shape{};
    }

    Bnd_Box shape_bounds;
    BRepBndLib::Add(shape, shape_bounds);
    if (shape_bounds.IsVoid() || shape_bounds.IsOut(cutter_bounds))
        return shape;

    BRepAlgoAPI_Cut cut(shape, cutter);
    cut.Build();
    return cut.IsDone() ? cut.Shape() : shape;
}

TopoDS_Shape styled_diagonal_facade_shape(
    KitchenCabinetFacadeStyle style,
    double first_x,
    double first_y,
    double second_x,
    double second_y,
    double z,
    double height,
    double thickness,
    bool round_screen_front = true) {
    const double dx = second_x - first_x;
    const double dy = second_y - first_y;
    const double length = std::hypot(dx, dy);
    if (length <= 1.0e-9) {
        return {};
    }
    TopoDS_Shape facade = CFacadeFurniture::BuildPlanarShape(
        style, 0.0, 0.0, z, length, thickness, height,
        round_screen_front);
    return place_along_panel(
        std::move(facade), first_x, first_y, second_x, second_y);
}

TopoDS_Shape corner_facade_panel_shape(KitchenCabinetFacadeStyle style,
                                       bool horizontal,
                                       double length,
                                       double z,
                                       double height,
                                       double thickness) {
    TopoDS_Shape facade = horizontal
        ? CFacadeFurniture::BuildPlanarShape(
              style, 0.0, 0.0, z, length, thickness, height, false)
        : styled_diagonal_facade_shape(
              style, 0.0, length, 0.0, 0.0,
              z, height, thickness, false);
    if (facade.IsNull()) {
        return {};
    }

    facade = orient_corner_front(std::move(facade), horizontal, thickness);
    if (facade.IsNull()) {
        return {};
    }

    const TopoDS_Shape miter_cutter = polygon_prism(
        horizontal
            ? std::vector<std::pair<double, double>>{
                  {0.0, 0.0}, {0.0, thickness}, {thickness, thickness}}
            : std::vector<std::pair<double, double>>{
                  {0.0, 0.0}, {thickness, 0.0}, {thickness, thickness}},
        z - 0.1,
        height + 0.2);
    TopoDS_Shape result;
    if (style == KitchenCabinetFacadeStyle::Frame
        || style == KitchenCabinetFacadeStyle::Milano) {
        // Profiled corner leaves meet on their two perpendicular front
        // planes. Their thickness overlaps only inside the closed joint and
        // is not visible. Boolean-cutting that tiny hidden wedge from a
        // compound of four rails took 3-4 seconds per leaf. Keep the exact
        // profile and its square technological end instead.
        result = facade;
    } else {
        Bnd_Box cutter_bounds;
        BRepBndLib::Add(miter_cutter, cutter_bounds);
        result = cut_intersecting_compound_parts(
            facade, miter_cutter, cutter_bounds);
    }
    if (result.IsNull()) result = facade;
    if (style == KitchenCabinetFacadeStyle::Screen) {
        result = fillet_edges_on_coordinate_plane(
            result, !horizontal, thickness, 2.0);
    }
    return result;
}

std::unique_ptr<CSolid> make_solid(const std::string& name,
                                   TopoDS_Shape shape,
                                   Color color) {
    if (shape.IsNull()) {
        return nullptr;
    }
    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName(name);
    solid->SetColor(color);
    // Cabinet parts are often transformed again by a parent assembly (the
    // two wings and both corner cabinets are placed after BuildParts()).
    // Eager ReBuldMesh() computed a complete mesh that was immediately thrown
    // away by that placement.  Keep only face metadata here; EnsureRenderMesh
    // builds CNet once, from the final BRep, on the first Draw.
    return solid->InitSurfaces() ? std::move(solid) : nullptr;
}
}

CKitchenCabinet::CKitchenCabinet(std::string name)
    : CAssembled(std::move(name)) {
}

CKitchenCabinet::CKitchenCabinet(std::string name,
                                 std::vector<unsigned long> element_ids,
                                 KitchenCabinetDefinition definition)
    : CAssembled(std::move(name), std::move(element_ids)) {
    if (IsValid(definition)) {
        definition_ = definition;
    }
}

const KitchenCabinetDefinition& CKitchenCabinet::GetDefinition() const {
    return definition_;
}

bool CKitchenCabinet::SetDefinition(const KitchenCabinetDefinition& definition) {
    if (!IsValid(definition)) {
        return false;
    }
    definition_ = definition;
    return true;
}

KitchenCabinetBodyType CKitchenCabinet::GetBodyType() const {
    return definition_.body_type;
}

KitchenCabinetFacadeType CKitchenCabinet::GetFacadeType() const {
    return definition_.facade_type;
}

KitchenCabinetFacadeStyle CKitchenCabinet::GetFacadeStyle() const {
    return definition_.facade_style;
}

int CKitchenCabinet::GetShelfCount() const {
    return definition_.shelf_count;
}

double CKitchenCabinet::GetWidth() const {
    return definition_.width;
}

double CKitchenCabinet::GetDepth() const {
    return definition_.depth;
}

double CKitchenCabinet::GetHeight() const {
    return definition_.height;
}

double CKitchenCabinet::GetPanelThickness() const {
    return definition_.panel_thickness;
}

double CKitchenCabinet::GetFacadeBulge() const {
    return definition_.facade_bulge;
}

double CKitchenCabinet::GetRadius2Bulge() const {
    return definition_.radius2_bulge;
}

double CKitchenCabinet::GetRadiusSideStraight() const {
    return definition_.radius_side_straight;
}

bool CKitchenCabinet::GetDoorAnimation(
    const std::string& part_name,
    KitchenCabinetDoorAnimation* animation) const {
    if (!animation
        || definition_.facade_type == KitchenCabinetFacadeType::Open
        || definition_.body_type == KitchenCabinetBodyType::Corner) {
        return false;
    }

    const bool double_door =
        definition_.facade_type == KitchenCabinetFacadeType::DoubleDoor;
    const bool left_door = part_name.find("Left Facade") != std::string::npos;
    const bool right_door = part_name.find("Right Facade") != std::string::npos;
    if (double_door && !left_door && !right_door) {
        return false;
    }

    const double width = definition_.width;
    const double depth = definition_.depth;
    const double thickness = definition_.panel_thickness;
    const double gap = std::min(2.0, thickness * 0.25);
    double hinge_x = 0.0;
    double hinge_y = 0.0;
    double angle_sign = 0.0;

    if (definition_.body_type == KitchenCabinetBodyType::Straight) {
        const double left = -width * 0.5;
        hinge_y = -depth * 0.5;
        if (double_door) {
            hinge_x = left_door ? left + gap : left + width - gap;
            angle_sign = left_door ? -1.0 : 1.0;
        } else {
            const bool hinge_on_right = definition_.door_hinge_side == 1;
            hinge_x = hinge_on_right ? left + width - gap : left + gap;
            angle_sign = hinge_on_right ? 1.0 : -1.0;
        }
    } else if (definition_.body_type == KitchenCabinetBodyType::Corner2) {
        const double first_x = width * 0.5 - gap;
        const double first_y = 0.0;
        const double second_x = 0.0;
        const double second_y = depth * 0.5 - gap;
        const bool first_hinge = double_door
            ? right_door : definition_.door_hinge_side == 1;
        // Corner-2 parts receive a final 180-degree orientation in add().
        hinge_x = -(first_hinge ? first_x : second_x);
        hinge_y = -(first_hinge ? first_y : second_y);
        angle_sign = first_hinge ? -1.0 : 1.0;
    } else if (definition_.body_type == KitchenCabinetBodyType::Radius) {
        const double hinge_radius_x = width * 0.5 + gap;
        const bool hinge_on_right = double_door
            ? right_door : definition_.door_hinge_side == 1;
        hinge_x = hinge_on_right ? hinge_radius_x : -hinge_radius_x;
        angle_sign = hinge_on_right ? 1.0 : -1.0;
    } else if (definition_.body_type == KitchenCabinetBodyType::Radius2) {
        const CircularSegmentGeometry segment = circular_segment_geometry(
            std::max(0.1, width * 0.5 + gap),
            std::max(0.1, definition_.radius2_bulge + gap),
            -depth * 0.5 - gap);
        const bool hinge_on_right = double_door
            ? right_door : definition_.door_hinge_side == 1;
        const gp_Pnt hinge = circular_segment_point(
            segment, hinge_on_right ? 0.0 : 1.0, 0.0);
        hinge_x = hinge.X();
        hinge_y = hinge.Y();
        angle_sign = hinge_on_right ? 1.0 : -1.0;
    } else if (definition_.body_type == KitchenCabinetBodyType::Radius3
               || definition_.body_type == KitchenCabinetBodyType::Radius4) {
        const double straight_length =
            definition_.body_type == KitchenCabinetBodyType::Radius4
                ? definition_.radius_side_straight : 0.0;
        const SideRadiusGeometry geometry = side_radius_geometry(
            definition_, straight_length, -gap);
        const bool hinge_on_right = double_door
            ? right_door : definition_.door_hinge_side == 1;
        gp_Pnt hinge = side_radius_point(
            geometry, hinge_on_right ? 0.0 : 1.0, 0.0);
        if (!hinge_on_right && straight_length > 0.0) {
            hinge.SetY(hinge.Y() + thickness);
        }
        hinge_x = hinge.X();
        hinge_y = hinge.Y();
        angle_sign = hinge_on_right ? 1.0 : -1.0;
    } else {
        return false;
    }

    animation->hinge = {
        static_cast<float>(hinge_x),
        static_cast<float>(hinge_y),
        0.0f};
    animation->angle_sign = static_cast<float>(angle_sign);
    return std::fabs(angle_sign) > 0.5;
}

std::vector<std::unique_ptr<CAlfaObject>> CKitchenCabinet::BuildParts(
    const KitchenCabinetDefinition& definition,
    const CSmartLine* frame_profile,
    const CSmartLine* panel_profile,
    const CSmartLine* milling_profile,
    const std::vector<const CSmartLine*>& milling_guides) {
    if (!IsValid(definition)) {
        return {};
    }
    const Color carcass_color{0.72f, 0.62f, 0.46f};
    const Color shelf_color{0.76f, 0.67f, 0.52f};
    const Color facade_color{0.48f, 0.30f, 0.16f};
    const Color handle_color{0.28f, 0.24f, 0.14f};
    const double width = definition.width;
    const double depth = definition.depth;
    const double height = definition.height;
    const double thickness = definition.panel_thickness;
    const double left = -width * 0.5;
    const double front = -depth * 0.5;
    const double left_open_angle = definition.left_door_open_angle;
    const double right_open_angle = definition.right_door_open_angle;
    const bool reverse_corner_front =
        definition.body_type == KitchenCabinetBodyType::Corner
        || definition.body_type == KitchenCabinetBodyType::Corner2;
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    std::vector<CSmartLine> milling_debug_profiles;
    std::vector<TopoDS_Shape> milling_debug_cutters;
    TopoDS_Shape cached_custom_facade;
    double cached_custom_x = 0.0;
    double cached_custom_y = 0.0;
    double cached_custom_z = 0.0;
    double cached_custom_width = 0.0;
    double cached_custom_thickness = 0.0;
    double cached_custom_height = 0.0;
    const auto build_planar_facade = [&](double facade_x,
                                         double facade_y,
                                         double facade_z,
                                         double facade_width,
                                         double facade_thickness,
                                         double facade_height) {
        constexpr double tolerance = 1.0e-9;
        const bool matching_cached_facade =
            !cached_custom_facade.IsNull()
            && std::abs(cached_custom_y - facade_y) <= tolerance
            && std::abs(cached_custom_z - facade_z) <= tolerance
            && std::abs(cached_custom_width - facade_width) <= tolerance
            && std::abs(cached_custom_thickness - facade_thickness) <= tolerance
            && std::abs(cached_custom_height - facade_height) <= tolerance;
        if (definition.facade_style == KitchenCabinetFacadeStyle::Frame
            && frame_profile && panel_profile) {
            if (matching_cached_facade) {
                gp_Trsf placement;
                placement.SetTranslation(
                    gp_Vec(facade_x - cached_custom_x, 0.0, 0.0));
                BRepBuilderAPI_Transform placed(
                    cached_custom_facade, placement, true);
                placed.Build();
                if (placed.IsDone()) {
                    return placed.Shape();
                }
            }
            const TopoDS_Shape custom =
                CFacadeFurniture::BuildPlanarFrameFromProfiles(
                    *frame_profile, *panel_profile,
                    facade_x, facade_y, facade_z,
                    facade_width, facade_thickness, facade_height);
            if (!custom.IsNull()) {
                cached_custom_facade = custom;
                cached_custom_x = facade_x;
                cached_custom_y = facade_y;
                cached_custom_z = facade_z;
                cached_custom_width = facade_width;
                cached_custom_thickness = facade_thickness;
                cached_custom_height = facade_height;
                return custom;
            }
        }
        if (definition.facade_style == KitchenCabinetFacadeStyle::Milled
            && milling_profile && !milling_guides.empty()) {
            if (matching_cached_facade) {
                const double cached_center =
                    cached_custom_x + cached_custom_width * 0.5;
                const double requested_center = facade_x + facade_width * 0.5;
                gp_Trsf placement;
                if (std::abs(cached_center + requested_center) <= tolerance) {
                    // Double-door fronts are symmetric about the cabinet YZ
                    // plane.  Mirror the already milled first door instead of
                    // repeating every sweep and boolean operation.
                    placement.SetMirror(
                        gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)));
                } else {
                    placement.SetTranslation(
                        gp_Vec(facade_x - cached_custom_x, 0.0, 0.0));
                }
                BRepBuilderAPI_Transform placed(
                    cached_custom_facade, placement, true);
                placed.Build();
                if (placed.IsDone()) {
                    return placed.Shape();
                }
            }
            CSmartLine placed_debug(
                "Catalog Milled Cutter XY Source (temporary)");
            TopoDS_Shape cutter_debug;
            const TopoDS_Shape custom =
                CFacadeFurniture::BuildPlanarMilledFromProfiles(
                    *milling_profile, milling_guides,
                    facade_x, facade_y, facade_z,
                    facade_width, facade_thickness, facade_height,
                    &placed_debug, true, &cutter_debug,
                    definition.milling_depth);
            if (!custom.IsNull()) {
                if (placed_debug.GetNumLines() > 0) {
                    milling_debug_profiles.push_back(
                        std::move(placed_debug));
                }
                if (!cutter_debug.IsNull()) {
                    milling_debug_cutters.push_back(
                        std::move(cutter_debug));
                }
                cached_custom_facade = custom;
                cached_custom_x = facade_x;
                cached_custom_y = facade_y;
                cached_custom_z = facade_z;
                cached_custom_width = facade_width;
                cached_custom_thickness = facade_thickness;
                cached_custom_height = facade_height;
                return custom;
            }
        }
        CSmartLine built_in_debug(
            "Built-in Milled Cutter XY Source (temporary)");
        CSmartLine* debug_output =
            definition.facade_style == KitchenCabinetFacadeStyle::Milled
                ? &built_in_debug : nullptr;
        TopoDS_Shape standard = CFacadeFurniture::BuildPlanarShape(
            definition.facade_style,
            facade_x, facade_y, facade_z,
            facade_width, facade_thickness, facade_height,
            true, definition.showcase_fill, debug_output);
        if (debug_output && built_in_debug.GetNumLines() > 0) {
            milling_debug_profiles.push_back(
                std::move(built_in_debug));
        }
        return standard;
    };
    auto add = [&parts, reverse_corner_front, &definition](
                   const std::string& name, TopoDS_Shape shape, Color color) {
        if (definition.facade_style == KitchenCabinetFacadeStyle::Screen
            && name.find("Handle") != std::string::npos
            && !shape.IsNull()) {
            gp_Trsf raise_handle;
            raise_handle.SetTranslation(gp_Vec(0.0, 0.0, 10.0));
            BRepBuilderAPI_Transform raised(shape, raise_handle, true);
            raised.Build();
            shape = raised.IsDone() ? raised.Shape() : TopoDS_Shape();
        }
        // Corner cabinet geometry is conveniently constructed around the
        // positive X/Y quadrant.  Furniture fronts, however, use -Y as the
        // common forward direction.  Rotate the complete corner assembly so
        // its fronts and hinges face the same way as straight cabinets.
        if (reverse_corner_front) {
            shape = rotate_about_z(shape, 0.0, 0.0, 180.0);
        }
        parts.push_back(make_solid(name, std::move(shape), color));
    };
    auto add_facade = [&add, &definition, facade_color](
                          const std::string& name,
                          TopoDS_Shape shape) {
        if (definition.facade_style == KitchenCabinetFacadeStyle::Screen
            && definition.showcase_fill != KitchenCabinetShowcaseFill::None
            && shape.ShapeType() == TopAbs_COMPOUND) {
            int part_index = 0;
            for (TopoDS_Iterator iterator(shape);
                 iterator.More(); iterator.Next(), ++part_index) {
                const bool glass_panel = part_index == 0
                    && definition.showcase_fill
                        != KitchenCabinetShowcaseFill::Lattice;
                const bool stained_wire = part_index == 1
                    && definition.showcase_fill
                        == KitchenCabinetShowcaseFill::StainedGlass;
                add(glass_panel
                        ? name + " Showcase Glass"
                        : stained_wire
                            ? name + " Stained Glass Wire"
                            : name + " Showcase Frame "
                                + std::to_string(part_index + 1),
                    iterator.Value(), facade_color);
            }
            return;
        }
        if (definition.facade_style != KitchenCabinetFacadeStyle::Frame
            && definition.facade_style != KitchenCabinetFacadeStyle::Milano) {
            add(name, std::move(shape), facade_color);
            return;
        }

        // Frame facades are two technological parts.  Keep the frame and the
        // centre panel as separate document objects, although all door
        // transforms have already been applied to their common compound.
        std::vector<TopoDS_Shape> technological_parts;
        if (shape.ShapeType() == TopAbs_COMPOUND) {
            for (TopoDS_Iterator iterator(shape);
                 iterator.More(); iterator.Next()) {
                technological_parts.push_back(iterator.Value());
            }
        }
        if (technological_parts.size() != 2) {
            add(name, std::move(shape), facade_color);
            return;
        }
        add(name + " Frame", std::move(technological_parts[0]), facade_color);
        add(name + " Center Panel", std::move(technological_parts[1]), facade_color);
    };
    auto add_round_handle = [&add, handle_color, facade_color](
                                const std::string& facade_name,
                                RoundDoorHandleShapes handle) {
        add(facade_name + " Handle Base",
            std::move(handle.base), handle_color);
        add(facade_name + " Handle Face",
            std::move(handle.face), facade_color);
    };

    if (definition.body_type == KitchenCabinetBodyType::Straight) {
        const double inner_width = std::max(1.0, width - 2.0 * thickness);
        const double inner_height = std::max(1.0, height - 2.0 * thickness);
        const double shelf_depth = std::max(1.0, depth - thickness);
        add("Cabinet Left Side",
            box_shape(left, front, 0.0, thickness, depth, height), carcass_color);
        add("Cabinet Right Side",
            box_shape(left + width - thickness, front, 0.0,
                                thickness, depth, height), carcass_color);
        add("Cabinet Bottom",
            box_shape(left + thickness, front, 0.0,
                                inner_width, depth, thickness), carcass_color);
        add("Cabinet Top",
            box_shape(left + thickness, front, height - thickness,
                                inner_width, depth, thickness), carcass_color);
        add("Cabinet Back",
            box_shape(left + thickness, front + depth - thickness, thickness,
                                inner_width, thickness, inner_height), carcass_color);

        for (int shelf = 0; shelf < definition.shelf_count; ++shelf) {
            const double center_z = thickness
                + inner_height * static_cast<double>(shelf + 1)
                    / static_cast<double>(definition.shelf_count + 1);
            const double shelf_z = std::clamp(
                center_z - thickness * 0.5,
                thickness,
                height - 2.0 * thickness);
            add("Cabinet Shelf " + std::to_string(shelf + 1),
                box_shape(left + thickness, front, shelf_z,
                                    inner_width, shelf_depth, thickness), shelf_color);
        }
    } else if (definition.body_type == KitchenCabinetBodyType::Radius3
               || definition.body_type == KitchenCabinetBodyType::Radius4) {
        const double inner_height = std::max(1.0, height - 2.0 * thickness);
        const double half_depth = depth * 0.5;
        const double straight_length = definition.body_type == KitchenCabinetBodyType::Radius4
            ? definition.radius_side_straight : 0.0;
        const SideRadiusGeometry geometry = side_radius_geometry(
            definition, straight_length, 0.0);
        constexpr double radius_panel_gap = 2.0;
        const double outer_panel_inset = thickness + radius_panel_gap;
        const double side_panel_clearance = thickness + radius_panel_gap;
        const std::string prefix = definition.body_type == KitchenCabinetBodyType::Radius4
            ? "Radius-4 Cabinet " : "Radius-3 Cabinet ";
        add(prefix + "Bottom",
            footprint_shape(
                definition, 0.0, thickness, outer_panel_inset), carcass_color);
        add(prefix + "Top",
            footprint_shape(
                definition, height - thickness, thickness, outer_panel_inset), carcass_color);
        add(prefix + "Back",
            box_shape(left + thickness, half_depth - thickness, 0.0,
                      std::max(1.0, width - 2.0 * thickness), thickness, height),
            carcass_color);
        add(prefix + "Right Side",
            box_shape(left + width - thickness, front + side_panel_clearance, 0.0,
                      thickness, std::max(1.0, depth - side_panel_clearance), height),
            carcass_color);
        const double left_side_depth = geometry.straight_length - side_panel_clearance;
        if (left_side_depth > 0.1) {
            add(prefix + "Left Side",
                box_shape(left, geometry.center_y + side_panel_clearance, 0.0,
                          thickness, left_side_depth, height),
                carcass_color);
        }
        for (int shelf = 0; shelf < definition.shelf_count; ++shelf) {
            const double center_z = thickness
                + inner_height * static_cast<double>(shelf + 1)
                    / static_cast<double>(definition.shelf_count + 1);
            const double shelf_z = std::clamp(
                center_z - thickness * 0.5,
                thickness,
                height - 2.0 * thickness);
            add("Cabinet Shelf " + std::to_string(shelf + 1),
                footprint_shape(
                    definition, shelf_z, thickness, thickness), shelf_color);
        }
    } else if (definition.body_type == KitchenCabinetBodyType::Radius2) {
        const double inner_height = std::max(1.0, height - 2.0 * thickness);
        const double half_depth = depth * 0.5;
        const double half_width = width * 0.5;
        const CircularSegmentGeometry segment = circular_segment_geometry(
            half_width, definition.radius2_bulge, front);
        const double chord_y = circular_segment_point(segment, 0.0, 0.0).Y();
        const double side_depth = std::max(1.0, half_depth - chord_y);
        constexpr double radius_panel_gap = 2.0;
        const double outer_panel_inset = thickness + radius_panel_gap;
        const double side_panel_clearance = thickness + radius_panel_gap;
        const double trimmed_side_depth = side_depth - side_panel_clearance;
        add("Radius-2 Cabinet Bottom",
            footprint_shape(
                definition, 0.0, thickness, outer_panel_inset), carcass_color);
        add("Radius-2 Cabinet Top",
            footprint_shape(
                definition, height - thickness, thickness, outer_panel_inset), carcass_color);
        add("Radius-2 Cabinet Back",
            box_shape(left + thickness, half_depth - thickness, 0.0,
                      std::max(1.0, width - 2.0 * thickness), thickness, height),
            carcass_color);
        if (trimmed_side_depth > 0.1) {
            add("Radius-2 Cabinet Left Side",
                box_shape(left, chord_y + side_panel_clearance, 0.0,
                          thickness, trimmed_side_depth, height),
                carcass_color);
            add("Radius-2 Cabinet Right Side",
                box_shape(left + width - thickness, chord_y + side_panel_clearance, 0.0,
                          thickness, trimmed_side_depth, height), carcass_color);
        }
        for (int shelf = 0; shelf < definition.shelf_count; ++shelf) {
            const double center_z = thickness
                + inner_height * static_cast<double>(shelf + 1)
                    / static_cast<double>(definition.shelf_count + 1);
            const double shelf_z = std::clamp(
                center_z - thickness * 0.5,
                thickness,
                height - 2.0 * thickness);
            add("Cabinet Shelf " + std::to_string(shelf + 1),
                footprint_shape(
                    definition, shelf_z, thickness, thickness), shelf_color);
        }
    } else if (definition.body_type == KitchenCabinetBodyType::Radius) {
        const double inner_height = std::max(1.0, height - 2.0 * thickness);
        const double half_depth = depth * 0.5;
        constexpr double radius_panel_gap = 2.0;
        const double outer_panel_inset = thickness + radius_panel_gap;
        add("Radius Cabinet Bottom",
            footprint_shape(
                definition, 0.0, thickness, outer_panel_inset), carcass_color);
        add("Radius Cabinet Top",
            footprint_shape(
                definition, height - thickness, thickness, outer_panel_inset), carcass_color);
        add("Radius Cabinet Back",
            box_shape(left + thickness, half_depth - thickness, 0.0,
                                std::max(1.0, width - 2.0 * thickness), thickness,
                                height), carcass_color);
        if (half_depth > 0.1) {
            add("Radius Cabinet Left Side",
                box_shape(left, 0.0, 0.0,
                          thickness, half_depth, height), carcass_color);
            add("Radius Cabinet Right Side",
                box_shape(left + width - thickness, 0.0, 0.0,
                          thickness, half_depth, height), carcass_color);
        }
        for (int shelf = 0; shelf < definition.shelf_count; ++shelf) {
            const double center_z = thickness
                + inner_height * static_cast<double>(shelf + 1)
                    / static_cast<double>(definition.shelf_count + 1);
            const double shelf_z = std::clamp(
                center_z - thickness * 0.5,
                thickness,
                height - 2.0 * thickness);
            add("Cabinet Shelf " + std::to_string(shelf + 1),
                footprint_shape(
                    definition, shelf_z, thickness, thickness), shelf_color);
        }
    } else {
        const double half_width = width * 0.5;
        const double half_depth = depth * 0.5;
        add("Corner Cabinet Bottom",
            footprint_shape(
                definition, 0.0, thickness, thickness), carcass_color);
        add("Corner Cabinet Top",
            footprint_shape(
                definition, height - thickness, thickness, thickness), carcass_color);
        add("Corner Cabinet Back X",
            box_shape(
                left, front, 0.0, width, thickness, height), carcass_color);
        add("Corner Cabinet Back Y",
            box_shape(
                left, front + thickness, 0.0,
                thickness, std::max(1.0, depth - thickness), height), carcass_color);
        add("Corner Cabinet Right Side",
            box_shape(
                left + width - thickness, front + thickness, 0.0,
                thickness, std::max(1.0, half_depth - thickness), height), carcass_color);
        add("Corner Cabinet Left Side",
            box_shape(
                left + thickness, half_depth - thickness, 0.0,
                std::max(1.0, half_width - thickness), thickness, height), carcass_color);
        for (int shelf = 0; shelf < definition.shelf_count; ++shelf) {
            const double center_z = thickness
                + std::max(1.0, height - 2.0 * thickness)
                    * static_cast<double>(shelf + 1)
                    / static_cast<double>(definition.shelf_count + 1);
            const double shelf_z = std::clamp(
                center_z - thickness * 0.5,
                thickness,
                height - 2.0 * thickness);
            add("Cabinet Shelf " + std::to_string(shelf + 1),
                footprint_shape(
                    definition, shelf_z, thickness, thickness,
                    definition.body_type == KitchenCabinetBodyType::Corner2
                        ? 12.0 : 0.0),
                shelf_color);
        }
    }

    const double gap = std::min(2.0, thickness * 0.25);
    const double facade_height = std::max(1.0, height - 2.0 * gap);
    if (definition.body_type == KitchenCabinetBodyType::Corner2
        && definition.facade_type != KitchenCabinetFacadeType::Open) {
        const double half_width = width * 0.5;
        const double half_depth = depth * 0.5;
        const double first_x = half_width - gap;
        const double first_y = 0.0;
        const double second_x = 0.0;
        const double second_y = half_depth - gap;
        const double open_angle = definition.door_open_angle;
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = styled_diagonal_facade_shape(
                definition.facade_style,
                first_x, first_y, second_x, second_y,
                gap, facade_height, thickness);
            const bool hinge_on_right = definition.door_hinge_side == 1;
            TopoDS_Shape handle = diagonal_door_handle_shape(
                first_x, first_y, second_x, second_y,
                gap, facade_height,
                definition.handle_orientation == 1,
                hinge_on_right);
            facade = rotate_about_z(
                facade,
                hinge_on_right ? first_x : second_x,
                hinge_on_right ? first_y : second_y,
                hinge_on_right ? -open_angle : open_angle);
            handle = rotate_about_z(
                handle,
                hinge_on_right ? first_x : second_x,
                hinge_on_right ? first_y : second_y,
                hinge_on_right ? -open_angle : open_angle);
            add_facade("Corner-2 Cabinet Facade", std::move(facade));
            add("Corner-2 Cabinet Facade Handle",
                std::move(handle), handle_color);
        } else {
            const double dx = second_x - first_x;
            const double dy = second_y - first_y;
            const double diagonal_length = std::hypot(dx, dy);
            const double center_x = (first_x + second_x) * 0.5;
            const double center_y = (first_y + second_y) * 0.5;
            const double center_gap = std::min(gap, diagonal_length * 0.1);
            const double gap_x = diagonal_length > 1.0e-9
                ? dx / diagonal_length * center_gap * 0.5 : 0.0;
            const double gap_y = diagonal_length > 1.0e-9
                ? dy / diagonal_length * center_gap * 0.5 : 0.0;
            TopoDS_Shape right_facade = styled_diagonal_facade_shape(
                definition.facade_style,
                first_x,
                first_y,
                center_x - gap_x,
                center_y - gap_y,
                gap,
                facade_height,
                thickness);
            TopoDS_Shape left_facade = styled_diagonal_facade_shape(
                definition.facade_style,
                center_x + gap_x,
                center_y + gap_y,
                second_x,
                second_y,
                gap,
                facade_height,
                thickness);
            TopoDS_Shape right_handle = diagonal_door_handle_shape(
                first_x, first_y,
                center_x - gap_x, center_y - gap_y,
                gap, facade_height,
                definition.handle_orientation == 1, true);
            TopoDS_Shape left_handle = diagonal_door_handle_shape(
                center_x + gap_x, center_y + gap_y,
                second_x, second_y,
                gap, facade_height,
                definition.handle_orientation == 1, false);
            right_facade = rotate_about_z(
                right_facade, first_x, first_y, -right_open_angle);
            left_facade = rotate_about_z(
                left_facade, second_x, second_y, left_open_angle);
            right_handle = rotate_about_z(
                right_handle, first_x, first_y, -right_open_angle);
            left_handle = rotate_about_z(
                left_handle, second_x, second_y, left_open_angle);
            add_facade("Corner-2 Cabinet Right Facade",
                       std::move(right_facade));
            add_facade("Corner-2 Cabinet Left Facade",
                       std::move(left_facade));
            add("Corner-2 Cabinet Right Facade Handle",
                std::move(right_handle), handle_color);
            add("Corner-2 Cabinet Left Facade Handle",
                std::move(left_handle), handle_color);
        }
    } else if (definition.body_type == KitchenCabinetBodyType::Corner
        && definition.facade_type != KitchenCabinetFacadeType::Open) {
        const double half_width = width * 0.5;
        const double half_depth = depth * 0.5;
        const double open_angle = definition.door_open_angle;
        TopoDS_Shape horizontal_facade = corner_facade_panel_shape(
            definition.facade_style,
            true,
            half_width - gap,
            gap,
            facade_height,
            thickness);
        TopoDS_Shape vertical_facade = corner_facade_panel_shape(
            definition.facade_style,
            false,
            half_depth - gap,
            gap,
            facade_height,
            thickness);

        // A real corner front is a linked bi-fold door. One front is
        // attached to the cabinet with a wide-angle hinge; the other is
        // attached to that front with a 60-degree corner hinge.
        const double fold_angle = std::min(
            60.0, open_angle * (60.0 / 150.0));
        const bool master_on_right = definition.door_hinge_side == 1;
        // The linked corner door has one operating handle. Keep it on the
        // broad left leaf (the horizontal panel before the final cabinet
        // orientation), irrespective of which outer hinge is selected.
        TopoDS_Shape handle = diagonal_door_handle_shape(
            0.0, 0.0, half_width - gap, 0.0,
            gap, facade_height,
            definition.handle_orientation == 1, true);
        if (!handle.IsNull()) {
            gp_Trsf shift_left_on_finished_cabinet;
            // Corner cabinet parts receive a final 180-degree orientation in
            // add(), therefore +X here is 10 mm left in the finished cabinet.
            shift_left_on_finished_cabinet.SetTranslation(
                gp_Vec(10.0, 0.0, 0.0));
            BRepBuilderAPI_Transform shifted_handle(
                handle, shift_left_on_finished_cabinet, true);
            shifted_handle.Build();
            handle = shifted_handle.IsDone()
                ? shifted_handle.Shape() : TopoDS_Shape();
        }
        handle = orient_corner_front(std::move(handle), true, thickness);
        if (master_on_right) {
            vertical_facade = rotate_about_z(
                vertical_facade, thickness, thickness, -fold_angle);
            horizontal_facade = rotate_about_z(
                horizontal_facade,
                half_width - gap,
                0.0,
                -open_angle);
            vertical_facade = rotate_about_z(
                vertical_facade,
                half_width - gap,
                0.0,
                -open_angle);
            handle = rotate_about_z(
                handle,
                half_width - gap,
                0.0,
                -open_angle);
        } else {
            horizontal_facade = rotate_about_z(
                horizontal_facade, thickness, thickness, fold_angle);
            handle = rotate_about_z(
                handle, thickness, thickness, fold_angle);
            vertical_facade = rotate_about_z(
                vertical_facade,
                0.0,
                half_depth - gap,
                open_angle);
            horizontal_facade = rotate_about_z(
                horizontal_facade,
                0.0,
                half_depth - gap,
                open_angle);
            handle = rotate_about_z(
                handle,
                0.0,
                half_depth - gap,
                open_angle);
        }
        add_facade("Corner Cabinet Right Facade",
                   std::move(horizontal_facade));
        add_facade("Corner Cabinet Left Facade",
                   std::move(vertical_facade));
        add("Corner Cabinet Right Facade Handle",
            std::move(handle), handle_color);
    } else if ((definition.body_type == KitchenCabinetBodyType::Radius3
                || definition.body_type == KitchenCabinetBodyType::Radius4)
               && definition.facade_type != KitchenCabinetFacadeType::Open) {
        const double straight_length = definition.body_type == KitchenCabinetBodyType::Radius4
            ? definition.radius_side_straight : 0.0;
        const SideRadiusGeometry hinge_geometry = side_radius_geometry(
            definition, straight_length, -gap);
        constexpr double half_pi = 1.57079632679489661923;
        const double middle_angle = 0.75 * 3.14159265358979323846;
        const double middle_speed = half_pi * std::sqrt(
            std::pow(hinge_geometry.radius_x * std::sin(middle_angle), 2.0)
            + std::pow(hinge_geometry.radius_y * std::cos(middle_angle), 2.0));
        const double fraction_gap = std::min(
            0.04, gap * 0.5 / std::max(1.0, middle_speed));
        const gp_Pnt right_hinge = side_radius_point(hinge_geometry, 0.0, 0.0);
        gp_Pnt left_hinge = side_radius_point(hinge_geometry, 1.0, 0.0);
        if (straight_length > 0.0) {
            left_hinge.SetY(left_hinge.Y() + thickness);
        }
        const double open_angle = definition.door_open_angle;
        const std::string prefix = definition.body_type == KitchenCabinetBodyType::Radius4
            ? "Radius-4 Cabinet " : "Radius-3 Cabinet ";
        const double handle_z = gap + facade_height - 30.0;
        const auto make_handle = [&](double fraction) {
            const gp_Pnt base = side_radius_point(
                hinge_geometry, fraction, handle_z);
            const double nx = (base.X() - hinge_geometry.center_x)
                / (hinge_geometry.radius_x * hinge_geometry.radius_x);
            const double ny = (base.Y() - hinge_geometry.center_y)
                / (hinge_geometry.radius_y * hinge_geometry.radius_y);
            return round_door_handle_shapes(base, gp_Dir(nx, ny, 0.0));
        };
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = side_radius_facade_shape(
                definition, straight_length, 0.0, 1.0,
                gap, facade_height, thickness, gap);
            const bool hinge_on_right = definition.door_hinge_side == 1;
            RoundDoorHandleShapes handle = make_handle(
                hinge_on_right ? 0.88 : 0.12);
            facade = rotate_about_z(
                facade,
                hinge_on_right ? right_hinge.X() : left_hinge.X(),
                hinge_on_right ? right_hinge.Y() : left_hinge.Y(),
                hinge_on_right ? open_angle : -open_angle);
            rotate_round_door_handle(
                handle,
                hinge_on_right ? right_hinge.X() : left_hinge.X(),
                hinge_on_right ? right_hinge.Y() : left_hinge.Y(),
                hinge_on_right ? open_angle : -open_angle);
            add(prefix + "Facade", std::move(facade), facade_color);
            add_round_handle(prefix + "Facade", std::move(handle));
        } else {
            TopoDS_Shape right_facade = side_radius_facade_shape(
                definition, straight_length, 0.0, 0.5 - fraction_gap,
                gap, facade_height, thickness, gap);
            RoundDoorHandleShapes right_handle = make_handle(0.44);
            right_facade = rotate_about_z(
                right_facade, right_hinge.X(), right_hinge.Y(), right_open_angle);
            rotate_round_door_handle(
                right_handle, right_hinge.X(), right_hinge.Y(), right_open_angle);
            TopoDS_Shape left_facade = side_radius_facade_shape(
                definition, straight_length, 0.5 + fraction_gap, 1.0,
                gap, facade_height, thickness, gap);
            RoundDoorHandleShapes left_handle = make_handle(0.56);
            left_facade = rotate_about_z(
                left_facade, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            rotate_round_door_handle(
                left_handle, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            add(prefix + "Right Facade", std::move(right_facade), facade_color);
            add(prefix + "Left Facade", std::move(left_facade), facade_color);
            add_round_handle(prefix + "Right Facade", std::move(right_handle));
            add_round_handle(prefix + "Left Facade", std::move(left_handle));
        }
    } else if (definition.body_type == KitchenCabinetBodyType::Radius2
        && definition.facade_type != KitchenCabinetFacadeType::Open) {
        const CircularSegmentGeometry hinge_segment = circular_segment_geometry(
            std::max(0.1, width * 0.5 + gap),
            std::max(0.1, definition.radius2_bulge + gap),
            -depth * 0.5 - gap);
        const double arc_speed = hinge_segment.radius
            * (hinge_segment.end_angle - hinge_segment.start_angle);
        const double fraction_gap = std::min(
            0.04, gap * 0.5 / std::max(1.0, arc_speed));
        const gp_Pnt right_hinge = circular_segment_point(
            hinge_segment, 0.0, 0.0);
        const gp_Pnt left_hinge = circular_segment_point(
            hinge_segment, 1.0, 0.0);
        const double open_angle = definition.door_open_angle;
        const double handle_z = gap + facade_height - 30.0;
        const auto make_handle = [&](double fraction) {
            const gp_Pnt base = circular_segment_point(
                hinge_segment, fraction, handle_z);
            return round_door_handle_shapes(
                base,
                gp_Dir(base.X(), base.Y() - hinge_segment.center_y, 0.0));
        };
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = radius2_facade_shape(
                definition, 0.0, 1.0,
                gap, facade_height, thickness, gap);
            const bool hinge_on_right = definition.door_hinge_side == 1;
            RoundDoorHandleShapes handle = make_handle(
                hinge_on_right ? 0.88 : 0.12);
            facade = rotate_about_z(
                facade,
                hinge_on_right ? right_hinge.X() : left_hinge.X(),
                hinge_on_right ? right_hinge.Y() : left_hinge.Y(),
                hinge_on_right ? open_angle : -open_angle);
            rotate_round_door_handle(
                handle,
                hinge_on_right ? right_hinge.X() : left_hinge.X(),
                hinge_on_right ? right_hinge.Y() : left_hinge.Y(),
                hinge_on_right ? open_angle : -open_angle);
            add("Radius-2 Cabinet Facade", std::move(facade), facade_color);
            add_round_handle("Radius-2 Cabinet Facade", std::move(handle));
        } else {
            TopoDS_Shape right_facade = radius2_facade_shape(
                definition, 0.0, 0.5 - fraction_gap,
                gap, facade_height, thickness, gap);
            RoundDoorHandleShapes right_handle = make_handle(0.44);
            right_facade = rotate_about_z(
                right_facade, right_hinge.X(), right_hinge.Y(), right_open_angle);
            rotate_round_door_handle(
                right_handle, right_hinge.X(), right_hinge.Y(), right_open_angle);
            TopoDS_Shape left_facade = radius2_facade_shape(
                definition, 0.5 + fraction_gap, 1.0,
                gap, facade_height, thickness, gap);
            RoundDoorHandleShapes left_handle = make_handle(0.56);
            left_facade = rotate_about_z(
                left_facade, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            rotate_round_door_handle(
                left_handle, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            add("Radius-2 Cabinet Right Facade",
                std::move(right_facade), facade_color);
            add("Radius-2 Cabinet Left Facade",
                std::move(left_facade), facade_color);
            add_round_handle(
                "Radius-2 Cabinet Right Facade", std::move(right_handle));
            add_round_handle(
                "Radius-2 Cabinet Left Facade", std::move(left_handle));
        }
    } else if (definition.body_type == KitchenCabinetBodyType::Radius
        && definition.facade_type != KitchenCabinetFacadeType::Open) {
        constexpr double pi = 3.14159265358979323846;
        const double angular_gap = std::min(
            0.08, gap * 0.5 / std::max(1.0, width * 0.5));
        const double hinge_radius_x = width * 0.5 + gap;
        const double open_angle = definition.door_open_angle;
        const double outer_radius_x = width * 0.5 + gap;
        const double outer_radius_y = definition.facade_bulge + gap;
        const double handle_z = gap + facade_height - 30.0;
        const auto make_handle = [&](double fraction) {
            const double angle = angular_gap
                + (3.14159265358979323846 - 2.0 * angular_gap) * fraction;
            const gp_Pnt base(
                outer_radius_x * std::cos(angle),
                -outer_radius_y * std::sin(angle),
                handle_z);
            return round_door_handle_shapes(
                base,
                gp_Dir(
                    base.X() / (outer_radius_x * outer_radius_x),
                    base.Y() / (outer_radius_y * outer_radius_y),
                    0.0));
        };
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = radius_facade_shape(
                definition, angular_gap, pi - angular_gap,
                gap, facade_height, thickness, gap);
            const bool hinge_on_right = definition.door_hinge_side == 1;
            RoundDoorHandleShapes handle = make_handle(
                hinge_on_right ? 0.88 : 0.12);
            facade = rotate_about_z(
                facade,
                hinge_on_right ? hinge_radius_x : -hinge_radius_x,
                0.0,
                hinge_on_right ? open_angle : -open_angle);
            rotate_round_door_handle(
                handle,
                hinge_on_right ? hinge_radius_x : -hinge_radius_x,
                0.0,
                hinge_on_right ? open_angle : -open_angle);
            add("Cabinet Radius Facade",
                std::move(facade), facade_color);
            add_round_handle("Cabinet Radius Facade", std::move(handle));
        } else {
            TopoDS_Shape right_facade = radius_facade_shape(
                definition, angular_gap, pi * 0.5 - angular_gap,
                gap, facade_height, thickness, gap);
            RoundDoorHandleShapes right_handle = make_handle(0.44);
            right_facade = rotate_about_z(
                right_facade, hinge_radius_x, 0.0, right_open_angle);
            rotate_round_door_handle(
                right_handle, hinge_radius_x, 0.0, right_open_angle);
            TopoDS_Shape left_facade = radius_facade_shape(
                definition, pi * 0.5 + angular_gap, pi - angular_gap,
                gap, facade_height, thickness, gap);
            RoundDoorHandleShapes left_handle = make_handle(0.56);
            left_facade = rotate_about_z(
                left_facade, -hinge_radius_x, 0.0, -left_open_angle);
            rotate_round_door_handle(
                left_handle, -hinge_radius_x, 0.0, -left_open_angle);
            add("Cabinet Radius Right Facade",
                std::move(right_facade), facade_color);
            add("Cabinet Radius Left Facade",
                std::move(left_facade), facade_color);
            add_round_handle(
                "Cabinet Radius Right Facade", std::move(right_handle));
            add_round_handle(
                "Cabinet Radius Left Facade", std::move(left_handle));
        }
    } else if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
        const bool hinge_on_right = definition.door_hinge_side == 1;
        const double facade_left = left + gap;
        const double facade_width = std::max(1.0, width - 2.0 * gap);
        TopoDS_Shape facade = build_planar_facade(
            facade_left, front - thickness, gap,
            facade_width, thickness, facade_height);
        TopoDS_Shape handle = planar_door_handle_shape(
            facade_left, front - thickness, gap,
            facade_width, facade_height,
            definition.handle_orientation == 1,
            !hinge_on_right);
        facade = rotate_about_z(
            facade,
            hinge_on_right ? left + width - gap : left + gap,
            front,
            hinge_on_right
                ? definition.door_open_angle
                : -definition.door_open_angle);
        handle = rotate_about_z(
            handle,
            hinge_on_right ? left + width - gap : left + gap,
            front,
            hinge_on_right
                ? definition.door_open_angle
                : -definition.door_open_angle);
        add_facade("Cabinet Facade", std::move(facade));
        add("Cabinet Facade Handle", std::move(handle), handle_color);
    } else if (definition.facade_type == KitchenCabinetFacadeType::DoubleDoor) {
        const double door_width = std::max(1.0, (width - 3.0 * gap) * 0.5);
        const double left_facade_x = left + gap;
        const double right_facade_x = left + 2.0 * gap + door_width;
        TopoDS_Shape left_facade = build_planar_facade(
            left_facade_x, front - thickness, gap,
            door_width, thickness, facade_height);
        TopoDS_Shape left_handle = planar_door_handle_shape(
            left_facade_x, front - thickness, gap,
            door_width, facade_height,
            definition.handle_orientation == 1, true);
        left_facade = rotate_about_z(
            left_facade,
            left + gap,
            front,
            -left_open_angle);
        left_handle = rotate_about_z(
            left_handle,
            left + gap,
            front,
            -left_open_angle);
        TopoDS_Shape right_facade = build_planar_facade(
            right_facade_x, front - thickness, gap,
            door_width, thickness, facade_height);
        TopoDS_Shape right_handle = planar_door_handle_shape(
            right_facade_x, front - thickness, gap,
            door_width, facade_height,
            definition.handle_orientation == 1, false);
        right_facade = rotate_about_z(
            right_facade,
            left + width - gap,
            front,
            right_open_angle);
        right_handle = rotate_about_z(
            right_handle,
            left + width - gap,
            front,
            right_open_angle);
        add_facade("Cabinet Left Facade", std::move(left_facade));
        add_facade("Cabinet Right Facade", std::move(right_facade));
        // Append hardware after the existing facade sequence so rebuilding an
        // older cabinet preserves the identities and materials of its doors.
        add("Cabinet Left Facade Handle", std::move(left_handle), handle_color);
        add("Cabinet Right Facade Handle", std::move(right_handle), handle_color);
    }

    for (CSmartLine& debug_profile : milling_debug_profiles) {
        parts.push_back(std::make_unique<CSmartLine>(
            std::move(debug_profile)));
    }
    for (TopoDS_Shape& cutter : milling_debug_cutters) {
        gp_Trsf move_debug;
        move_debug.SetTranslation(gp_Vec(
            definition.width + 150.0, 0.0, 0.0));
        BRepBuilderAPI_Transform moved_debug(cutter, move_debug, true);
        moved_debug.Build();
        TopoDS_Shape visible_cutter = moved_debug.IsDone()
            ? moved_debug.Shape() : cutter;
        auto debug_solid = make_solid(
            "Milling Cutter Envelope (temporary)",
            std::move(visible_cutter),
            Color{0.95f, 0.25f, 0.65f});
        if (debug_solid) {
            parts.push_back(std::move(debug_solid));
        }
    }

    if (std::any_of(parts.begin(), parts.end(),
                    [](const std::unique_ptr<CAlfaObject>& part) { return !part; })) {
        return {};
    }
    return parts;
}

bool CKitchenCabinet::Save(std::ostream& stream) const {
    stream << "KitchenCabinet " << m_id << ' ' << GetElementIds().size();
    for (unsigned long id : GetElementIds()) {
        stream << ' ' << id;
    }
    stream << ' ' << static_cast<unsigned int>(GetDrawParam())
           << ' ' << GetIdDim()
           << ' ' << static_cast<unsigned int>(definition_.body_type)
           << ' ' << static_cast<unsigned int>(definition_.facade_type)
           << ' ' << definition_.shelf_count
           << ' ' << definition_.width
           << ' ' << definition_.depth
           << ' ' << definition_.height
           << ' ' << definition_.panel_thickness
           << ' ' << definition_.milling_depth
           << ' ' << definition_.facade_bulge
           << ' ' << static_cast<unsigned int>(definition_.facade_style)
           << ' ' << definition_.radius2_bulge
           << ' ' << definition_.radius_side_straight
           << ' ' << definition_.handle_orientation
           << ' ' << static_cast<unsigned int>(definition_.showcase_fill) << '\n';
    return static_cast<bool>(stream);
}

std::unique_ptr<CAlfaObject> CKitchenCabinet::Clone() const {
    auto copy = std::make_unique<CKitchenCabinet>(
        GetName() + " Copy", GetElementIds(), definition_);
    copy->SetGroupName(GetGroupName());
    copy->CAlfaObject::SetVisible(IsVisible());
    copy->CAlfaObject::SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->m_LayerID = m_LayerID;
    copy->SetDrawParam(GetDrawParam());
    copy->SetIdDim(GetIdDim());
    copy->SetAssemblyTransform(GetAssemblyTransform());
    copy->SetParametricDefinition(GetParametricToolId(), GetParametricParameters());
    for (const CDimens3D* dimension : GetDimensions()) {
        if (dimension) {
            copy->AddDimension(*dimension);
        }
    }
    return copy;
}

bool CKitchenCabinet::IsValid(const KitchenCabinetDefinition& definition) {
    const double smallest_dimension = std::min({
        definition.width, definition.depth, definition.height});
    return valid_body_type(definition.body_type)
        && valid_facade_type(definition.facade_type)
        && valid_facade_style(definition.facade_style)
        && static_cast<unsigned int>(definition.showcase_fill) <= 4U
        && definition.shelf_count >= 0
        && definition.shelf_count <= 100
        && std::isfinite(definition.width) && definition.width > 0.0
        && std::isfinite(definition.depth) && definition.depth > 0.0
        && std::isfinite(definition.height) && definition.height > 0.0
        && std::isfinite(definition.panel_thickness)
        && definition.panel_thickness > 0.0
        && definition.panel_thickness < smallest_dimension * 0.5
        && std::isfinite(definition.milling_depth)
        && definition.milling_depth > 0.0
        && definition.milling_depth
            <= std::max(0.1, definition.panel_thickness - 0.5)
        && std::isfinite(definition.facade_bulge)
        && definition.facade_bulge > 0.0
        && std::isfinite(definition.radius2_bulge)
        && definition.radius2_bulge > 0.0
        && std::isfinite(definition.radius_side_straight)
        && definition.radius_side_straight >= 0.0
        && (definition.body_type != KitchenCabinetBodyType::Radius4
            || definition.radius_side_straight < definition.depth)
        && (definition.body_type != KitchenCabinetBodyType::Radius2
            || (definition.radius2_bulge < definition.width * 0.5
                && definition.radius2_bulge < definition.depth))
        && std::isfinite(definition.door_open_angle)
        && definition.door_open_angle >= 0.0
        && definition.door_open_angle <= 180.0
        && std::isfinite(definition.left_door_open_angle)
        && definition.left_door_open_angle >= 0.0
        && definition.left_door_open_angle <= 180.0
        && std::isfinite(definition.right_door_open_angle)
        && definition.right_door_open_angle >= 0.0
        && definition.right_door_open_angle <= 180.0
        && definition.door_hinge_side >= 0
        && definition.door_hinge_side <= 1
        && definition.handle_orientation >= 0
        && definition.handle_orientation <= 1;
}
