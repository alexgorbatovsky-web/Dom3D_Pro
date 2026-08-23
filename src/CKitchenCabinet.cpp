#include "CKitchenCabinet.h"
#include "CBSpline.h"
#include "SmartLine.h"
#include "Conic.h"
#include "iges/SplineCurve.h"

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
#include "solid/TrimShapeBuilder.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <Geom_BezierCurve.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Geom_OffsetCurve.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pln.hxx>
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
#include <Standard_Failure.hxx>

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

bool edge_joins_points(const TopoDS_Edge& edge,
                       const gp_Pnt& first,
                       const gp_Pnt& second,
                       double tolerance = 1.0e-5) {
    TopoDS_Vertex first_vertex;
    TopoDS_Vertex last_vertex;
    TopExp::Vertices(edge, first_vertex, last_vertex);
    if (first_vertex.IsNull() || last_vertex.IsNull()) return false;
    const gp_Pnt edge_first = BRep_Tool::Pnt(first_vertex);
    const gp_Pnt edge_last = BRep_Tool::Pnt(last_vertex);
    return (edge_first.Distance(first) <= tolerance
            && edge_last.Distance(second) <= tolerance)
        || (edge_first.Distance(second) <= tolerance
            && edge_last.Distance(first) <= tolerance);
}

bool point_on_shape(const gp_Pnt& point,
                    const TopoDS_Shape& reference,
                    double tolerance = 1.0e-4) {
    BRepBuilderAPI_MakeVertex vertex_builder(point);
    if (!vertex_builder.IsDone()) return false;
    BRepExtrema_DistShapeShape distance(vertex_builder.Vertex(), reference);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= tolerance;
}

bool edge_on_shape(const TopoDS_Edge& edge,
                   const TopoDS_Shape& reference) {
    try {
        BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last)) return false;
        return point_on_shape(curve.Value(first), reference)
            && point_on_shape(curve.Value((first + last) * 0.5), reference)
            && point_on_shape(curve.Value(last), reference);
    } catch (const Standard_Failure&) {
        return false;
    }
}

TopoDS_Shape round_radial_plain_facade(
    const TopoDS_Shape& panel,
    const TopoDS_Edge& front_boundary,
    const gp_Pnt& front_start,
    const gp_Pnt& front_end,
    const gp_Pnt& rear_start,
    const gp_Pnt& rear_end,
    double height,
    double radius) {
    if (panel.IsNull() || front_boundary.IsNull() || radius <= 0.0
        || height <= 2.0 * radius) {
        return panel;
    }

    // First pass: the four short edges through the panel thickness.  Apply
    // them one by one: OCC can reject a single four-edge operation on a
    // curved strip even though every individual R3 fillet is valid.
    TopoDS_Shape rounded_corners = panel;
    try {
        const gp_Vec up(0.0, 0.0, height);
        const gp_Pnt top_front_start = front_start.Translated(up);
        const gp_Pnt top_front_end = front_end.Translated(up);
        const gp_Pnt top_rear_start = rear_start.Translated(up);
        const gp_Pnt top_rear_end = rear_end.Translated(up);

        const std::pair<gp_Pnt, gp_Pnt> corner_edges[] = {
            {front_start, rear_start},
            {front_end, rear_end},
            {top_front_start, top_rear_start},
            {top_front_end, top_rear_end}
        };
        for (const auto& points : corner_edges) {
            TopoDS_Edge corner_edge;
            for (TopExp_Explorer explorer(rounded_corners, TopAbs_EDGE);
                 explorer.More(); explorer.Next()) {
                const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
                if (edge_joins_points(edge, points.first, points.second)) {
                    corner_edge = edge;
                    break;
                }
            }
            if (corner_edge.IsNull()) continue;

            try {
                BRepFilletAPI_MakeFillet fillet(rounded_corners);
                fillet.Add(radius, corner_edge);
                fillet.Build();
                if (fillet.IsDone()) {
                    rounded_corners = fillet.Shape();
                }
            } catch (const Standard_Failure&) {
                // A failed corner must not prevent the remaining corners.
            }
        }
    } catch (const Standard_Failure&) {
        // Preserve all successfully completed corners.
    }

    // Second pass: every remaining edge of the original outward face.
    // Midpoint testing excludes the new corner blend edges.
    try {
        BRepPrimAPI_MakePrism front_prism(
            front_boundary, gp_Vec(0.0, 0.0, height));
        front_prism.Build();
        if (!front_prism.IsDone()) return rounded_corners;
        const TopoDS_Shape front_surface = front_prism.Shape();
        BRepFilletAPI_MakeFillet fillet(rounded_corners);
        int edge_count = 0;
        for (TopExp_Explorer explorer(rounded_corners, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            if (edge_on_shape(edge, front_surface)) {
                fillet.Add(radius, edge);
                ++edge_count;
            }
        }
        if (edge_count > 0) {
            fillet.Build();
            if (fillet.IsDone()) return fillet.Shape();
        }
    } catch (const Standard_Failure&) {
    }
    return rounded_corners;
}

TopoDS_Shape radius_facade_shape(const KitchenCabinetDefinition& definition,
                                 double start_angle,
                                 double end_angle,
                                 double z,
                                 double height,
                                 double radial_thickness,
                                 double edge_gap,
                                 double plain_round_radius = 0.0) {
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
    if (!prism.IsDone()) return {};
    return round_radial_plain_facade(
        prism.Shape(), outer_arc, outer_start, outer_end,
        inner_start, inner_end, height, plain_round_radius);
}

TopoDS_Shape radius2_facade_shape(
    const KitchenCabinetDefinition& definition,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap,
    double plain_round_radius = 0.0) {
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
    if (!prism.IsDone()) return {};
    return round_radial_plain_facade(
        prism.Shape(), outer_arc, outer_start, outer_end,
        inner_start, inner_end, height, plain_round_radius);
}

TopoDS_Shape side_radius_facade_shape(
    const KitchenCabinetDefinition& definition,
    double straight_length,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap,
    double plain_round_radius = 0.0) {
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
    TopoDS_Edge front_boundary = outer_arc;
    gp_Pnt front_end = outer_end;
    gp_Pnt rear_end = inner_end;
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
        front_boundary = outer_boundary;
        front_end = outer_tip;
        rear_end = inner_tip;
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
    if (!prism.IsDone()) return {};
    return round_radial_plain_facade(
        prism.Shape(), front_boundary, outer_start, front_end,
        inner_start, rear_end, height, plain_round_radius);
}

TopoDS_Shape shape_compound(const std::vector<TopoDS_Shape>& shapes) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    bool has_shape = false;
    for (const TopoDS_Shape& shape : shapes) {
        if (shape.IsNull()) continue;
        builder.Add(compound, shape);
        has_shape = true;
    }
    return has_shape ? TopoDS_Shape(compound) : TopoDS_Shape{};
}

template <typename FacadeBuilder>
TopoDS_Shape curved_milano_facade_shape(
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap,
    double full_arc_length,
    FacadeBuilder&& build_facade) {
    const double span = end_fraction - start_fraction;
    const double approximate_arc_length = full_arc_length * span;
    if (span <= 1.0e-6 || height <= 1.0
        || approximate_arc_length <= 1.0) {
        return {};
    }

    const double frame_width = std::clamp(
        std::min(approximate_arc_length, height) * 0.13, 32.0, 60.0);
    if (height <= 2.0 * frame_width + 2.0
        || approximate_arc_length <= 2.0 * frame_width + 2.0) {
        return build_facade(
            start_fraction, end_fraction, z, height,
            radial_thickness, edge_gap);
    }

    const double frame_fraction = std::min(
        span * 0.42,
        frame_width / std::max(1.0, approximate_arc_length) * span);
    const double molding_width = std::clamp(frame_width * 0.18, 6.0, 11.0);
    const double molding_fraction = std::min(
        frame_fraction * 0.55,
        molding_width / std::max(1.0, approximate_arc_length) * span);
    const double panel_depth = std::clamp(
        radial_thickness * 0.42, 4.0, radial_thickness);
    const double panel_gap = std::max(0.1, edge_gap - 2.0);
    const double profile_gap =
        edge_gap + std::min(2.5, radial_thickness * 0.14);

    std::vector<TopoDS_Shape> parts;
    parts.reserve(9);
    // Match the Radius-3 Milano construction: recessed curved panel, four
    // broad rails and four raised inner molding strips.
    parts.push_back(build_facade(
        start_fraction + frame_fraction,
        end_fraction - frame_fraction,
        z + frame_width, height - 2.0 * frame_width,
        panel_depth, panel_gap));
    parts.push_back(build_facade(
        start_fraction, end_fraction,
        z, frame_width, radial_thickness, edge_gap));
    parts.push_back(build_facade(
        start_fraction, end_fraction,
        z + height - frame_width, frame_width,
        radial_thickness, edge_gap));
    parts.push_back(build_facade(
        start_fraction, start_fraction + frame_fraction,
        z, height, radial_thickness, edge_gap));
    parts.push_back(build_facade(
        end_fraction - frame_fraction, end_fraction,
        z, height, radial_thickness, edge_gap));
    parts.push_back(build_facade(
        start_fraction, end_fraction,
        z + frame_width - molding_width, molding_width,
        radial_thickness, profile_gap));
    parts.push_back(build_facade(
        start_fraction, end_fraction,
        z + height - frame_width, molding_width,
        radial_thickness, profile_gap));
    parts.push_back(build_facade(
        start_fraction + frame_fraction - molding_fraction,
        start_fraction + frame_fraction,
        z + frame_width, height - 2.0 * frame_width,
        radial_thickness, profile_gap));
    parts.push_back(build_facade(
        end_fraction - frame_fraction,
        end_fraction - frame_fraction + molding_fraction,
        z + frame_width, height - 2.0 * frame_width,
        radial_thickness, profile_gap));
    return shape_compound(parts);
}

template <typename ArcBuilder, typename PointBuilder,
          typename OutwardBuilder, typename TangentBuilder>
TopoDS_Shape swept_curved_milano_frame(
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double frame_width,
    double radial_thickness,
    double edge_gap,
    double end_stile_angle_degrees,
    ArcBuilder&& build_arc,
    PointBuilder&& point_at,
    OutwardBuilder&& outward_at,
    TangentBuilder&& tangent_at) {
    (void)radial_thickness;
    // Build a true constant-distance guide from the nominal facade curve.
    // Enlarging the ellipse radii also moved its centre and displaced the
    // right endpoint sideways, so the two mitres could not both close.
    const auto offset_arc = [edge_gap](const TopoDS_Edge& basis_edge) {
        if (basis_edge.IsNull() || std::abs(edge_gap) <= 1.0e-12) {
            return basis_edge;
        }
        Standard_Real first = 0.0;
        Standard_Real last = 0.0;
        const Handle(Geom_Curve) basis =
            BRep_Tool::Curve(basis_edge, first, last);
        if (basis.IsNull()) return TopoDS_Edge{};
        try {
            const Handle(Geom_TrimmedCurve) trimmed =
                new Geom_TrimmedCurve(basis, first, last);
            const Handle(Geom_OffsetCurve) parallel =
                new Geom_OffsetCurve(
                    trimmed, edge_gap, gp_Dir(0.0, 0.0, -1.0));
            BRepBuilderAPI_MakeEdge edge(parallel);
            return edge.IsDone() ? edge.Edge() : TopoDS_Edge{};
        } catch (const Standard_Failure&) {
            return TopoDS_Edge{};
        }
    };
    TopoDS_Edge bottom = offset_arc(
        build_arc(z, start_fraction, end_fraction));
    TopoDS_Edge top = offset_arc(
        build_arc(z + height, start_fraction, end_fraction));
    if (bottom.IsNull() || top.IsNull()) return {};

    const double scale = frame_width / 60.0;
    constexpr double milano_profile_depth = 13.2226;
    const double profile_depth_world = milano_profile_depth * scale;
    const auto offset_point = [&](double fraction, double point_z) {
        const gp_Pnt base = point_at(fraction, point_z);
        const gp_Dir outward = outward_at(fraction);
        return gp_Pnt(
            base.X() + outward.X() * edge_gap,
            base.Y() + outward.Y() * edge_gap,
            base.Z() + outward.Z() * edge_gap);
    };
    const gp_Pnt bottom_start = offset_point(start_fraction, z);
    const gp_Pnt bottom_end = offset_point(end_fraction, z);
    const gp_Pnt top_start = offset_point(start_fraction, z + height);
    const gp_Pnt top_end = offset_point(end_fraction, z + height);
    const auto make_section = [&](const gp_Pnt& origin,
                                  const gp_Dir& inward,
                                  const gp_Dir& outward) {
        // CreateFilenka5's Milano section reaches 13.2226 mm in depth.
        // The guide describes the visible/front envelope of the facade, so
        // seat the section behind that envelope instead of building all of
        // its depth outwards (which left a visible gap to the cabinet body).
        const gp_Pnt seated_origin(
            origin.X() - outward.X() * profile_depth_world,
            origin.Y() - outward.Y() * profile_depth_world,
            origin.Z() - outward.Z() * profile_depth_world);
        const auto point = [&](double u, double v) {
            return gp_Pnt(
                seated_origin.X() + inward.X() * u * scale
                    + outward.X() * v * scale,
                seated_origin.Y() + inward.Y() * u * scale
                    + outward.Y() * v * scale,
                seated_origin.Z() + inward.Z() * u * scale
                    + outward.Z() * v * scale);
        };
        BRepBuilderAPI_MakeWire section;
        const auto add_line = [&](double u1, double v1,
                                  double u2, double v2) {
            BRepBuilderAPI_MakeEdge edge(
                point(u1, v1), point(u2, v2));
            if (edge.IsDone()) section.Add(edge.Edge());
        };
        const auto add_bezier = [&](
            const std::array<std::pair<double, double>, 4>& poles) {
            TColgp_Array1OfPnt points(1, 4);
            for (int index = 0; index < 4; ++index) {
                points.SetValue(index + 1,
                    point(poles[index].first, poles[index].second));
            }
            const Handle(Geom_BezierCurve) curve =
                new Geom_BezierCurve(points);
            BRepBuilderAPI_MakeEdge edge(curve);
            if (edge.IsDone()) section.Add(edge.Edge());
        };
        add_line(0.0, 0.0, 0.0, 5.0);
        add_line(0.0, 5.0, 2.0, 5.0);
        add_line(2.0, 5.0, 4.0, 8.0);
        add_bezier({{{4.0, 8.0}, {13.9, 13.2226},
                     {41.881, 13.2226}, {51.3, 8.0}}});
        add_bezier({{{51.3, 8.0}, {53.0, 10.0},
                     {55.0, 10.0}, {57.0, 8.0}}});
        add_line(57.0, 8.0, 60.0, 8.0);
        add_line(60.0, 8.0, 60.0, 0.0);
        add_line(60.0, 0.0, 0.0, 0.0);
        return section.IsDone() ? section.Wire() : TopoDS_Wire{};
    };
    const auto sweep_rail = [](const TopoDS_Edge& edge,
                               const TopoDS_Wire& section,
                               const gp_Dir& plane_normal) {
        if (edge.IsNull() || section.IsNull()) return TopoDS_Shape{};
        BRepBuilderAPI_MakeWire guide;
        guide.Add(edge);
        if (!guide.IsDone()) return TopoDS_Shape{};
        try {
            BRepOffsetAPI_MakePipeShell sweep(guide.Wire());
            // The facade guide is planar.  A fixed binormal prevents the
            // corrected-Frenet frame from twisting the Milano section at the
            // left ellipse endpoint, where it must match the straight stile.
            sweep.SetMode(plane_normal);
            sweep.Add(section, false, false);
            if (!sweep.IsReady()) return TopoDS_Shape{};
            sweep.Build();
            if (!sweep.IsDone() || !sweep.MakeSolid()) {
                return TopoDS_Shape{};
            }
            return sweep.Shape();
        } catch (const Standard_Failure&) {
            return TopoDS_Shape{};
        }
    };
    const auto extrude_stile = [](const TopoDS_Wire& section,
                                  const gp_Vec& direction) {
        if (section.IsNull() || direction.SquareMagnitude() <= 1.0e-12) {
            return TopoDS_Shape{};
        }
        BRepBuilderAPI_MakeFace face(section);
        if (!face.IsDone()) return TopoDS_Shape{};
        BRepPrimAPI_MakePrism prism(face.Face(), direction);
        prism.Build();
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape{};
    };
    const auto keep_plane_side = [](const TopoDS_Shape& shape,
                                    const gp_Pnt& plane_origin,
                                    const gp_Vec& positive_normal,
                                    bool keep_positive) {
        if (shape.IsNull() || positive_normal.SquareMagnitude() <= 1.0e-12) {
            return TopoDS_Shape{};
        }
        const gp_Dir normal(positive_normal);
        BRepBuilderAPI_MakeFace plane_face(gp_Pln(plane_origin, normal));
        if (!plane_face.IsDone()) return TopoDS_Shape{};
        const double sign = keep_positive ? 1.0 : -1.0;
        const gp_Pnt reference(
            plane_origin.X() + sign * normal.X() * 100.0,
            plane_origin.Y() + sign * normal.Y() * 100.0,
            plane_origin.Z() + sign * normal.Z() * 100.0);
        BRepPrimAPI_MakeHalfSpace half_space(plane_face.Face(), reference);
        half_space.Build();
        if (!half_space.IsDone()) return TopoDS_Shape{};
        BRepAlgoAPI_Common common(shape, half_space.Solid());
        common.Build();
        if (!common.IsDone() || common.Shape().IsNull()
            || !BRepCheck_Analyzer(common.Shape()).IsValid()) {
            return TopoDS_Shape{};
        }
        return common.Shape();
    };

    const gp_Dir up(0.0, 0.0, 1.0);
    const gp_Dir down(0.0, 0.0, -1.0);
    const gp_Dir start_tangent = tangent_at(start_fraction);
    const gp_Dir end_tangent = tangent_at(end_fraction);
    top.Reverse();
    TopoDS_Shape bottom_rail = sweep_rail(
        bottom, make_section(bottom_start, up,
                             outward_at(start_fraction)), up);
    TopoDS_Shape top_rail = sweep_rail(
        top, make_section(top_end, down,
                          outward_at(end_fraction)), down);
    // A pipe shell is appropriate for the curved rails, but it is not stable
    // for a straight vertical guide: its automatically selected trihedron can
    // rotate/offset the Milano section at the arc endpoints.  Extruding the
    // already oriented planar section keeps both stiles exactly on those
    // endpoints and makes their 60 mm legs coincide with the curved rails.
    TopoDS_Shape start_rail = extrude_stile(
        make_section(bottom_start, start_tangent,
                     outward_at(start_fraction)),
        gp_Vec(0.0, 0.0, height));
    // Keep the original Milano section orientation.  The left stile needs a
    // small plan rotation (as in the legacy radius facade), not a 180-degree
    // reversal of its cross-section.
    const gp_Dir end_outward = outward_at(end_fraction);
    TopoDS_Shape end_rail = extrude_stile(
        make_section(bottom_end, gp_Dir(
            -end_tangent.X(), -end_tangent.Y(), 0.0),
            end_outward),
        gp_Vec(0.0, 0.0, height));
    if (!end_rail.IsNull()
        && std::abs(end_stile_angle_degrees) > 1.0e-9) {
        constexpr double pi = 3.14159265358979323846;
        gp_Trsf turn;
        turn.SetRotation(
            gp_Ax1(gp_Pnt(bottom_end.X(), bottom_end.Y(), 0.0),
                   gp_Dir(0.0, 0.0, 1.0)),
            end_stile_angle_degrees * pi / 180.0);
        BRepBuilderAPI_Transform rotated(end_rail, turn, true);
        rotated.Build();
        end_rail = rotated.IsDone() ? rotated.Shape() : TopoDS_Shape{};
    }
    if (bottom_rail.IsNull() || top_rail.IsNull()
        || start_rail.IsNull() || end_rail.IsNull()) {
        return {};
    }

    // Four true mitre joints.  In the local corner coordinates S is the
    // distance from an arc endpoint towards the centre of the rail.  The
    // lower joint is split by S=Z and the upper one by S=height-Z.
    const gp_Vec start_in(start_tangent);
    const gp_Vec end_in(-end_tangent.X(), -end_tangent.Y(), 0.0);
    const gp_Vec vertical(0.0, 0.0, 1.0);
    const auto trim_pair = [&](TopoDS_Shape& horizontal,
                               TopoDS_Shape& stile,
                               const gp_Pnt& corner,
                               const gp_Vec& inward,
                               bool upper) {
        const gp_Vec normal = upper ? inward + vertical
                                    : inward - vertical;
        TopoDS_Shape trimmed_horizontal = keep_plane_side(
            horizontal, corner, normal, true);
        TopoDS_Shape trimmed_stile = keep_plane_side(
            stile, corner, normal, false);
        if (trimmed_horizontal.IsNull() || trimmed_stile.IsNull()) {
            return false;
        }
        horizontal = std::move(trimmed_horizontal);
        stile = std::move(trimmed_stile);
        return true;
    };
    if (!trim_pair(bottom_rail, start_rail, bottom_start, start_in, false)
        || !trim_pair(bottom_rail, end_rail, bottom_end, end_in, false)
        || !trim_pair(top_rail, start_rail, top_start, start_in, true)
        || !trim_pair(top_rail, end_rail, top_end, end_in, true)) {
        return {};
    }
    return shape_compound(
        {bottom_rail, top_rail, start_rail, end_rail});
}

TopoDS_Shape swept_side_radius_milano_frame(
    const KitchenCabinetDefinition& definition,
    double straight_length,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double frame_width,
    double radial_thickness,
    double edge_gap) {
    const SideRadiusGeometry geometry = side_radius_geometry(
        definition, straight_length, 0.0);
    constexpr double half_pi = 1.57079632679489661923;
    return swept_curved_milano_frame(
        start_fraction, end_fraction, z, height,
        frame_width, radial_thickness, edge_gap,
        straight_length > 1.0e-9 ? 16.0 : 3.0,
        [&geometry](double part_z, double first, double last) {
            return side_radius_arc(geometry, part_z, first, last);
        },
        [&geometry](double fraction, double part_z) {
            return side_radius_point(geometry, fraction, part_z);
        },
        [&geometry, half_pi](double fraction) {
            const double angle = half_pi + half_pi * fraction;
            return gp_Dir(
                std::cos(angle) / geometry.radius_x,
                -std::sin(angle) / geometry.radius_y,
                0.0);
        },
        [&geometry, half_pi](double fraction) {
            const double angle = half_pi + half_pi * fraction;
            return gp_Dir(
                -geometry.radius_x * std::sin(angle),
                -geometry.radius_y * std::cos(angle),
                0.0);
        });
}

TopoDS_Shape side_radius_milano_facade_shape(
    const KitchenCabinetDefinition& definition,
    double straight_length,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap) {
    const double span = end_fraction - start_fraction;
    if (span <= 1.0e-6 || height <= 1.0) return {};

    const SideRadiusGeometry geometry = side_radius_geometry(
        definition, straight_length, -edge_gap);
    constexpr double half_pi = 1.57079632679489661923;
    const double approximate_arc_length = half_pi * std::sqrt(
        (geometry.radius_x * geometry.radius_x
         + geometry.radius_y * geometry.radius_y) * 0.5) * span;
    const double frame_width = std::clamp(
        std::min(approximate_arc_length, height) * 0.13, 32.0, 60.0);
    if (height <= 2.0 * frame_width + 2.0
        || approximate_arc_length <= 2.0 * frame_width + 2.0) {
        return side_radius_facade_shape(
            definition, straight_length, start_fraction, end_fraction,
            z, height, radial_thickness, edge_gap);
    }

    const double frame_fraction = std::min(
        span * 0.42,
        frame_width / std::max(1.0, approximate_arc_length) * span);
    const double panel_depth = std::clamp(
        radial_thickness * 0.42, 4.0, radial_thickness);

    const TopoDS_Shape swept_frame = swept_side_radius_milano_frame(
        definition, straight_length, start_fraction, end_fraction,
        z, height, frame_width, radial_thickness, edge_gap);
    if (!swept_frame.IsNull()) {
        // The recessed panel must continue underneath the two side stiles.
        // Ending it exactly at the theoretical 60 mm opening exposed a gap
        // of about 8 mm at an ellipse endpoint because fraction-to-length is
        // not uniform there.  Keep this overlap horizontal only: extending
        // the panel vertically would intrude into the four mitre joints.
        const double left_panel_overlap = std::min(
            10.0, frame_width * 0.25);
        // Final endpoint corrections measured on the true offset guide:
        // extend the panel 6 mm to the right and 5 mm to the left.
        const double radius4_right_extension =
            straight_length > 1.0e-9 ? 10.0 : 0.0;
        const double right_panel_overlap = std::max(
            0.0, left_panel_overlap - 5.0 + 6.0
                + radius4_right_extension);
        // The earlier left correction was 15.3 mm; restore 5 mm after the
        // stile angle and the guide were corrected.
        constexpr double left_panel_reduction = 10.3;
        const double left_signed_adjustment =
            left_panel_overlap - left_panel_reduction;
        const double left_adjustment_fraction = std::clamp(
            left_signed_adjustment
                / std::max(1.0, approximate_arc_length) * span,
            -frame_fraction * 0.5, frame_fraction * 0.5);
        const double right_overlap_fraction = std::min(
            frame_fraction * 0.5,
            right_panel_overlap
                / std::max(1.0, approximate_arc_length) * span);
        constexpr double centre_panel_recess = 6.0;
        const double centre_panel_edge =
            std::max(0.1, edge_gap - 5.0) - centre_panel_recess;
        const double untrimmed_left_fraction =
            end_fraction - frame_fraction + left_adjustment_fraction;
        // Radius-3 and Radius-4 use the same panel boundary algorithm.  The
        // different Radius-4 stile angle must not introduce a separate trim.
        const double centre_panel_left_fraction = untrimmed_left_fraction;
        const TopoDS_Shape centre_panel = side_radius_facade_shape(
            definition, straight_length,
            start_fraction + frame_fraction - right_overlap_fraction,
            centre_panel_left_fraction,
            z + frame_width, height - 2.0 * frame_width,
            panel_depth, centre_panel_edge);
        return shape_compound({swept_frame, centre_panel});
    }

    return curved_milano_facade_shape(
        start_fraction, end_fraction, z, height,
        radial_thickness, edge_gap,
        approximate_arc_length / span,
        [&definition, straight_length](
            double first, double last, double part_z, double part_height,
            double part_thickness, double part_gap) {
            return side_radius_facade_shape(
                definition, straight_length, first, last,
                part_z, part_height, part_thickness, part_gap);
        });
}

TopoDS_Shape radius_milano_facade_shape(
    const KitchenCabinetDefinition& definition,
    double start_angle,
    double end_angle,
    double z,
    double height,
    double radial_thickness,
    double edge_gap) {
    const double radius_x = std::max(
        0.1, definition.width * 0.5 + edge_gap);
    const double radius_y = std::max(
        0.1, definition.facade_bulge + edge_gap);
    const double angle_span = end_angle - start_angle;
    const double approximate_arc_length = std::abs(angle_span) * std::sqrt(
        (radius_x * radius_x + radius_y * radius_y) * 0.5);
    const double frame_width = std::clamp(
        std::min(approximate_arc_length, height) * 0.13, 32.0, 60.0);
    const auto angle_at = [start_angle, angle_span](double fraction) {
        return start_angle + angle_span * fraction;
    };
    const TopoDS_Shape swept_frame = swept_curved_milano_frame(
        0.0, 1.0, z, height,
        frame_width, radial_thickness, edge_gap, 3.0,
        [radius_x, radius_y, &angle_at](
            double part_z, double first, double last) {
            return ellipse_arc(
                radius_x, radius_y, part_z,
                angle_at(first), angle_at(last));
        },
        [radius_x, radius_y, &angle_at](double fraction, double part_z) {
            const double angle = angle_at(fraction);
            return gp_Pnt(
                radius_x * std::cos(angle),
                -radius_y * std::sin(angle), part_z);
        },
        [radius_x, radius_y, &angle_at](double fraction) {
            const double angle = angle_at(fraction);
            return gp_Dir(
                std::cos(angle) / radius_x,
                -std::sin(angle) / radius_y, 0.0);
        },
        [radius_x, radius_y, &angle_at](double fraction) {
            const double angle = angle_at(fraction);
            return gp_Dir(
                -radius_x * std::sin(angle),
                -radius_y * std::cos(angle), 0.0);
        });
    if (!swept_frame.IsNull()) {
        const double panel_overlap = std::min(10.0, frame_width * 0.25);
        const double panel_fraction = std::min(
            0.42, (frame_width - panel_overlap)
                / std::max(1.0, approximate_arc_length));
        const double panel_depth = std::clamp(
            radial_thickness * 0.42, 4.0, radial_thickness);
        const TopoDS_Shape centre_panel = radius_facade_shape(
            definition,
            angle_at(panel_fraction), angle_at(1.0 - panel_fraction),
            z + frame_width, height - 2.0 * frame_width,
            panel_depth, std::max(0.1, edge_gap - 2.0));
        return shape_compound({swept_frame, centre_panel});
    }
    return curved_milano_facade_shape(
        0.0, 1.0, z, height, radial_thickness, edge_gap,
        approximate_arc_length,
        [&definition, start_angle, angle_span](
            double first, double last, double part_z, double part_height,
            double part_thickness, double part_gap) {
            return radius_facade_shape(
                definition,
                start_angle + angle_span * first,
                start_angle + angle_span * last,
                part_z, part_height, part_thickness, part_gap);
        });
}

TopoDS_Shape radius2_milano_facade_shape(
    const KitchenCabinetDefinition& definition,
    double start_fraction,
    double end_fraction,
    double z,
    double height,
    double radial_thickness,
    double edge_gap) {
    const CircularSegmentGeometry outer = circular_segment_geometry(
        std::max(0.1, definition.width * 0.5 + edge_gap),
        std::max(0.1, definition.radius2_bulge + edge_gap),
        -definition.depth * 0.5 - edge_gap);
    const double full_arc_length = outer.radius
        * (outer.end_angle - outer.start_angle);
    const double span = end_fraction - start_fraction;
    const double approximate_arc_length = full_arc_length * span;
    const double frame_width = std::clamp(
        std::min(approximate_arc_length, height) * 0.13, 32.0, 60.0);
    const auto angle_at = [&outer](double fraction) {
        return outer.start_angle
            + (outer.end_angle - outer.start_angle) * fraction;
    };
    const TopoDS_Shape swept_frame = swept_curved_milano_frame(
        start_fraction, end_fraction, z, height,
        frame_width, radial_thickness, edge_gap, 3.0,
        [&outer](double part_z, double first, double last) {
            return circular_segment_arc(outer, part_z, first, last);
        },
        [&outer](double fraction, double part_z) {
            return circular_segment_point(outer, fraction, part_z);
        },
        [&outer, &angle_at](double fraction) {
            const double angle = angle_at(fraction);
            return gp_Dir(std::cos(angle), -std::sin(angle), 0.0);
        },
        [&outer, &angle_at](double fraction) {
            const double angle = angle_at(fraction);
            return gp_Dir(
                -outer.radius * std::sin(angle),
                -outer.radius * std::cos(angle), 0.0);
        });
    if (!swept_frame.IsNull()) {
        const double panel_overlap = std::min(10.0, frame_width * 0.25);
        const double panel_fraction = std::min(
            span * 0.42,
            (frame_width - panel_overlap)
                / std::max(1.0, approximate_arc_length) * span);
        const double panel_depth = std::clamp(
            radial_thickness * 0.42, 4.0, radial_thickness);
        const TopoDS_Shape centre_panel = radius2_facade_shape(
            definition,
            start_fraction + panel_fraction,
            end_fraction - panel_fraction,
            z + frame_width, height - 2.0 * frame_width,
            panel_depth, std::max(0.1, edge_gap - 2.0));
        return shape_compound({swept_frame, centre_panel});
    }
    return curved_milano_facade_shape(
        start_fraction, end_fraction, z, height,
        radial_thickness, edge_gap, full_arc_length,
        [&definition](
            double first, double last, double part_z, double part_height,
            double part_thickness, double part_gap) {
            return radius2_facade_shape(
                definition, first, last,
                part_z, part_height, part_thickness, part_gap);
        });
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

using NamedCabinetShape = std::pair<std::string, TopoDS_Shape>;

// Рабочая заготовка нового общего генератора радиусного шкафа.
// Для Radius-3 этот генератор уже используется вместо старого построения;
// недостающие детали добавляются сюда по мере разработки.
std::vector<NamedCabinetShape> build_radius3_cabinet_parts(
    const KitchenCabinetDefinition& definition) {
    std::vector<NamedCabinetShape> parts;
    if (definition.body_type != KitchenCabinetBodyType::Radius3) {
        return parts;
    }

    // Основные параметры шкафа. Все построения выполняются в локальной
    // системе CKitchenCabinet: центр по X, фасад направлен в сторону -Y,
    // низ шкафа находится на Z = 0.
    const double width = definition.width;
    const double depth = definition.depth;
    const double height = definition.height;
    const double thickness = definition.panel_thickness;
    const double left = -width * 0.5;
    const double right = width * 0.5;
    const double front = -depth * 0.5;
    const double back = depth * 0.5;
    constexpr double facade_gap = 2.0;
    const double facade_z = facade_gap;
    const double facade_height = std::max(1.0, height - 2.0 * facade_gap);

    // Для Radius-3 straight_length равен нулю. В дальнейшем эту
    // направляющую можно заменить общей структурой для Radius/2/3/4.
    const SideRadiusGeometry guide = side_radius_geometry(
        definition, 0.0, 0.0);

    const auto add_part = [&parts](std::string name, TopoDS_Shape shape) {
        if (!shape.IsNull()) {
            parts.emplace_back(std::move(name), std::move(shape));
        }
    };

    // ------------------------------------------------------------------
    // 1. Корпус
    // ------------------------------------------------------------------
    TopoDS_Shape bottom;
    TopoDS_Shape top;
    TopoDS_Shape back_panel;
    TopoDS_Shape right_side;
    std::vector<TopoDS_Shape> shelves;

    // Radius-3 is closed by the curved front, the back panel and the right
    // side. The curve reaches the left end of the back panel directly, so
    // this body type has no separate straight left side.
    constexpr double carcass_clearance = 2.0;

    double iho = 0.4142;
    CPoint3d pA2(-width / 2.0, depth/2.0, 0);
    CPoint3d pC2(width/2.0, -depth / 2.0, 0);
    CPoint3d pB(-width / 2.0, -depth / 2.0, 0);

    CConic* conic2 = new CConic(&pA2, &pB, &pC2, iho);
	int Qty = 8;
    CSplineCurve* Guideshelves = conic2->MakeSpline(Qty);

    CSplineCurve* GuideBox = conic2->MakeSpline(Qty);
    if (GuideBox) {
        GuideBox->printToFile("GuideBox.sketch");
        GuideBox->Offset(carcass_clearance);
    }
    if (Guideshelves) {
        CPlane pl1(0, -1, 0, depth / 2.0 - thickness);
        CPoint3d kp1(width / 2.0, -depth / 2.0, 0);
        Guideshelves->TrimByPlane(&pl1, &kp1);
        CPlane pl2(-1, 0, 0, width / 2.0 - thickness);
        CPoint3d kp2(-width / 2.0, depth / 2.0, 0);
        Guideshelves->TrimByPlane(&pl2, &kp2);
        Guideshelves->printToFile("Guideshelves.sketch");
        Guideshelves->Offset(carcass_clearance);
    }

     CSplineCurve* Guide2 = conic2->MakeSpline(Qty);
    if (Guide2)
       Guide2->printToFile("Guide2.sketch");


//	I want to make an sketh  to create shelves.

    // conic2 and Guide2 are legacy factory results. Own them here so repeated
    // cabinet rebuilds do not leak the temporary construction geometry.
    std::unique_ptr<CConic> conic_owner(conic2);
    std::unique_ptr<CSplineCurve> box_spline_owner(GuideBox);
    std::unique_ptr<CSplineCurve> shelf_spline_owner(Guideshelves);
    std::unique_ptr<CSplineCurve> spline_owner(Guide2);

    // GuideBox and Guideshelves are open splines. Complete either one with
    // the right and back straight edges before making a horizontal panel.
    constexpr int plate_curve_samples = 81;
    const auto guide_endpoints = [](
        CSplineCurve* curve, CPoint3d& start, CPoint3d& end) {
        return curve && curve->np() >= 2
            && curve->GetPoint(0.0, &start)
            && curve->GetPoint(
                static_cast<double>(curve->np() - 1), &end);
    };
    const auto make_closed_guide_plate = [=](
        CSplineCurve* curve,
        const CPoint3d& curve_start_point,
        const CPoint3d& curve_end_point,
        double plate_z) {
            if (!curve || curve->np() < 2) {
                return TopoDS_Shape{};
            }
            TColgp_Array1OfPnt curve_points(1, plate_curve_samples);
            for (int index = 0; index < plate_curve_samples; ++index) {
                const double parameter =
                    static_cast<double>(curve->np() - 1)
                    * static_cast<double>(index)
                    / static_cast<double>(plate_curve_samples - 1);
                CPoint3d point;
                if (!curve->GetPoint(parameter, &point)) {
                    return TopoDS_Shape{};
                }
                curve_points.SetValue(
                    index + 1, gp_Pnt(point.x, point.y, plate_z));
            }

            GeomAPI_PointsToBSpline curve_builder(curve_points);
            const Handle(Geom_BSplineCurve) plate_curve =
                curve_builder.Curve();
            if (plate_curve.IsNull()) {
                return TopoDS_Shape{};
            }

            const gp_Pnt curve_start(
                curve_start_point.x, curve_start_point.y, plate_z);
            const gp_Pnt curve_end(
                curve_end_point.x, curve_end_point.y, plate_z);
            const gp_Pnt right_back(
                curve_end_point.x, curve_start_point.y, plate_z);
            BRepBuilderAPI_MakeWire plate_wire;
            plate_wire.Add(BRepBuilderAPI_MakeEdge(plate_curve).Edge());
            plate_wire.Add(
                BRepBuilderAPI_MakeEdge(curve_end, right_back).Edge());
            plate_wire.Add(
                BRepBuilderAPI_MakeEdge(right_back, curve_start).Edge());
            if (!plate_wire.IsDone()) {
                return TopoDS_Shape{};
            }

            BRepBuilderAPI_MakeFace plate_face(plate_wire.Wire());
            if (!plate_face.IsDone()) {
                return TopoDS_Shape{};
            }
            BRepPrimAPI_MakePrism plate_prism(
                plate_face.Face(), gp_Vec(0.0, 0.0, thickness));
            plate_prism.Build();
            return plate_prism.IsDone()
                ? plate_prism.Shape() : TopoDS_Shape{};
    };

    CPoint3d box_guide_start;
    CPoint3d box_guide_end;
    if (guide_endpoints(GuideBox, box_guide_start, box_guide_end)) {
        // Complete the open GuideBox spline and use that closed sketch for
        // both horizontal carcass panels.
        bottom = make_closed_guide_plate(
            GuideBox, box_guide_start, box_guide_end, 0.0);
        top = make_closed_guide_plate(
            GuideBox, box_guide_start, box_guide_end,
            height - thickness);

        // These panels coincide with the two straight edges added to close
        // GuideBox. Vertically they fit between the bottom and top panels.
        const double vertical_z = thickness;
        const double vertical_height = std::max(
            1.0, height - 2.0 * thickness);
        const double back_width =
            box_guide_end.x - box_guide_start.x;
        const double right_depth =
            box_guide_start.y - box_guide_end.y - thickness;
        if (back_width > 0.1) {
            back_panel = box_shape(
                box_guide_start.x,
                box_guide_start.y - thickness,
                vertical_z,
                back_width, thickness, vertical_height);
        }
        if (right_depth > 0.1) {
            right_side = box_shape(
                box_guide_end.x - thickness,
                box_guide_end.y,
                vertical_z,
                thickness, right_depth, vertical_height);
        }
    }

    CPoint3d shelf_guide_start;
    CPoint3d shelf_guide_end;
    const bool has_shelf_endpoints = guide_endpoints(
        Guideshelves, shelf_guide_start, shelf_guide_end);
    if (has_shelf_endpoints) {
        const double inner_height = std::max(
            1.0, height - 2.0 * thickness);
        for (int shelf = 0; shelf < definition.shelf_count; ++shelf) {
            const double center_z = thickness
                + inner_height * static_cast<double>(shelf + 1)
                    / static_cast<double>(definition.shelf_count + 1);
            const double shelf_z = std::clamp(
                center_z - thickness * 0.5,
                thickness,
                std::max(thickness, height - 2.0 * thickness));

            TopoDS_Shape shelf_shape = make_closed_guide_plate(
                Guideshelves, shelf_guide_start, shelf_guide_end, shelf_z);
            if (!shelf_shape.IsNull()) {
                shelves.push_back(std::move(shelf_shape));
            }
        }
    }


    add_part("Radius-3 Cabinet Bottom", std::move(bottom));
    add_part("Radius-3 Cabinet Top", std::move(top));
    add_part("Radius-3 Cabinet Back", std::move(back_panel));
    add_part("Radius-3 Cabinet Right Side", std::move(right_side));
    for (std::size_t index = 0; index < shelves.size(); ++index) {
        add_part(
            "Radius-3 Cabinet Shelf " + std::to_string(index + 1),
            std::move(shelves[index]));
    }

    if (definition.facade_type == KitchenCabinetFacadeType::Open) {
        return parts;
    }

    // Plain Radius-3 must follow the very same spline as GuideBox.  The
    // analytic ellipse used by the generic radius cabinets does not exactly
    // coincide with the spline produced by CConic after GuideBox is offset.
    // Only Milano continues into the five-detail custom builder below.
    if (definition.facade_style != KitchenCabinetFacadeStyle::Milano) {
        // Guide2 is the rear face of the door.  The carcass guide is its
        // inward +2 mm offset, so the door thickness must grow in the
        // opposite (outward) direction.
        CSplineCurve front_facade_guide;
        if (!Guide2 || !front_facade_guide.Copy(Guide2)
            || !front_facade_guide.Offset(-thickness)) {
            return parts;
        }
        const auto guide_frame_at = [](CSplineCurve* curve,
                                       double fraction,
                                       CPoint7d& frame) {
            return curve && curve->np() >= 2
                && curve->GetPoint7d(
                    std::clamp(fraction, 0.0, 1.0)
                        * static_cast<double>(curve->np() - 1),
                    &frame);
        };
        CPoint7d left_hinge_frame{};
        CPoint7d right_hinge_frame{};
        if (!guide_frame_at(Guide2, 0.0, left_hinge_frame)
            || !guide_frame_at(Guide2, 1.0, right_hinge_frame)) {
            return parts;
        }
        const gp_Pnt left_hinge(
            left_hinge_frame.x, left_hinge_frame.y, 0.0);
        const gp_Pnt right_hinge(
            right_hinge_frame.x, right_hinge_frame.y, 0.0);
        const double handle_z = facade_z + facade_height - 30.0;
        const auto make_handle = [&](double fraction) {
            CPoint7d frame{};
            if (!guide_frame_at(&front_facade_guide, fraction, frame)) {
                return RoundDoorHandleShapes{};
            }
            gp_Vec outward(frame.m, -frame.l, 0.0);
            if (outward.SquareMagnitude() <= 1.0e-12) {
                return RoundDoorHandleShapes{};
            }
            outward.Normalize();
            return round_door_handle_shapes(
                gp_Pnt(frame.x, frame.y, handle_z), gp_Dir(outward));
        };
        const auto make_plain_facade = [&](double first, double last) {
            if (!Guide2 || Guide2->np() < 2
                || last - first <= 1.0e-6) {
                return TopoDS_Shape{};
            }

            constexpr int facade_curve_samples = 81;
            TColgp_Array1OfPnt front_points(1, facade_curve_samples);
            TColgp_Array1OfPnt rear_points(1, facade_curve_samples);
            const double parameter_scale =
                static_cast<double>(Guide2->np() - 1);
            for (int index = 0; index < facade_curve_samples; ++index) {
                const double fraction = first
                    + (last - first) * static_cast<double>(index)
                        / static_cast<double>(facade_curve_samples - 1);
                const double parameter = parameter_scale * fraction;
                CPoint3d front_point;
                CPoint3d rear_point;
                if (!front_facade_guide.GetPoint(parameter, &front_point)
                    || !Guide2->GetPoint(parameter, &rear_point)) {
                    return TopoDS_Shape{};
                }
                front_points.SetValue(
                    index + 1,
                    gp_Pnt(front_point.x, front_point.y, facade_z));
                rear_points.SetValue(
                    index + 1,
                    gp_Pnt(rear_point.x, rear_point.y, facade_z));
            }

            GeomAPI_PointsToBSpline front_builder(front_points);
            GeomAPI_PointsToBSpline rear_builder(rear_points);
            const Handle(Geom_BSplineCurve) front_curve =
                front_builder.Curve();
            const Handle(Geom_BSplineCurve) rear_curve = rear_builder.Curve();
            if (front_curve.IsNull() || rear_curve.IsNull()) {
                return TopoDS_Shape{};
            }

            const TopoDS_Edge front_edge =
                BRepBuilderAPI_MakeEdge(front_curve).Edge();
            TopoDS_Edge rear_edge =
                BRepBuilderAPI_MakeEdge(rear_curve).Edge();
            const gp_Pnt front_start = front_points.Value(1);
            const gp_Pnt front_end =
                front_points.Value(facade_curve_samples);
            const gp_Pnt rear_start = rear_points.Value(1);
            const gp_Pnt rear_end = rear_points.Value(facade_curve_samples);
            rear_edge.Reverse();

            BRepBuilderAPI_MakeWire wire;
            wire.Add(front_edge);
            wire.Add(BRepBuilderAPI_MakeEdge(front_end, rear_end).Edge());
            wire.Add(rear_edge);
            wire.Add(BRepBuilderAPI_MakeEdge(rear_start, front_start).Edge());
            if (!wire.IsDone()) return TopoDS_Shape{};
            BRepBuilderAPI_MakeFace face(wire.Wire());
            if (!face.IsDone()) return TopoDS_Shape{};
            BRepPrimAPI_MakePrism prism(
                face.Face(), gp_Vec(0.0, 0.0, facade_height));
            prism.Build();
            if (!prism.IsDone()) return TopoDS_Shape{};
            return round_radial_plain_facade(
                prism.Shape(), front_edge,
                front_start, front_end, rear_start, rear_end,
                facade_height, 3.0);
        };

        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            const bool plain_hinge_on_right = definition.door_hinge_side == 1;
            const double hinge_x = plain_hinge_on_right
                ? right_hinge.X() : left_hinge.X();
            const double hinge_y = plain_hinge_on_right
                ? right_hinge.Y() : left_hinge.Y();
            const double angle = plain_hinge_on_right
                ? definition.door_open_angle : -definition.door_open_angle;
            TopoDS_Shape facade = rotate_about_z(
                make_plain_facade(0.0, 1.0), hinge_x, hinge_y, angle);
            RoundDoorHandleShapes handle = make_handle(
                plain_hinge_on_right ? 0.12 : 0.88);
            rotate_round_door_handle(handle, hinge_x, hinge_y, angle);
            add_part("Radius-3 Cabinet Facade", std::move(facade));
            add_part("Radius-3 Cabinet Facade Handle Base",
                     std::move(handle.base));
            add_part("Radius-3 Cabinet Facade Handle Face",
                     std::move(handle.face));
        } else {
            const double fraction_gap = std::min(
                0.04,
                facade_gap * 0.5
                    / std::max(1.0, Guide2->GetLength()));

            TopoDS_Shape right_facade = rotate_about_z(
                make_plain_facade(0.5 + fraction_gap, 1.0),
                right_hinge.X(), right_hinge.Y(),
                definition.right_door_open_angle);
            RoundDoorHandleShapes right_handle = make_handle(0.56);
            rotate_round_door_handle(
                right_handle, right_hinge.X(), right_hinge.Y(),
                definition.right_door_open_angle);

            TopoDS_Shape left_facade = rotate_about_z(
                make_plain_facade(0.0, 0.5 - fraction_gap),
                left_hinge.X(), left_hinge.Y(),
                -definition.left_door_open_angle);
            RoundDoorHandleShapes left_handle = make_handle(0.44);
            rotate_round_door_handle(
                left_handle, left_hinge.X(), left_hinge.Y(),
                -definition.left_door_open_angle);

            add_part("Radius-3 Cabinet Right Facade",
                     std::move(right_facade));
            add_part("Radius-3 Cabinet Left Facade",
                     std::move(left_facade));
            add_part("Radius-3 Cabinet Right Facade Handle Base",
                     std::move(right_handle.base));
            add_part("Radius-3 Cabinet Right Facade Handle Face",
                     std::move(right_handle.face));
            add_part("Radius-3 Cabinet Left Facade Handle Base",
                     std::move(left_handle.base));
            add_part("Radius-3 Cabinet Left Facade Handle Face",
                     std::move(left_handle.face));
        }
        return parts;
    }

    // ------------------------------------------------------------------
    // 2. Радиусная дверь Milano: сборка строго из пяти деталей
    // ------------------------------------------------------------------
    TopoDS_Shape facade_bottom_profile;
    TopoDS_Shape facade_top_profile;
    TopoDS_Shape facade_left_profile;
    TopoDS_Shape facade_right_profile;
    TopoDS_Shape facade_center_panel;

    // TODO(Alex):
    // - создать направляющие профилей;
    // - построить каждый профиль с помощью Swept;
    // - получить точки пересечения вертикальных и горизонтальных профилей;
    // - обрезать направляющую филёнки по фактическим точкам стыковки;
    // - построить филёнку по обрезанной направляющей.

    constexpr double half_pi = 1.57079632679489661923;
    const double facade_arc_length = half_pi * std::sqrt(
        (guide.radius_x * guide.radius_x
         + guide.radius_y * guide.radius_y) * 0.5);
    const double frame_width = std::clamp(
        std::min(facade_arc_length, facade_height) * 0.13, 32.0, 60.0);
    std::unique_ptr<CSmartLine> frame_profile_owner =
        CFacadeFurniture::CreateMilanoProfile(frame_width);
    CSmartLine* frame_profile = frame_profile_owner.get();

    std::string error;
    facade_bottom_profile = CFacadeFurniture::BuildSweptProfile(
		frame_profile, Guide2, 90.0, 0.0, 0.0, &error);
    if (!facade_bottom_profile.IsNull()) {
        gp_Trsf bottom_profile_translation;
        bottom_profile_translation.SetTranslation(
            gp_Vec(0.0, 0.0, facade_z));

        BRepBuilderAPI_Transform moved_bottom_profile(
            facade_bottom_profile, bottom_profile_translation, true);
        moved_bottom_profile.Build();
        if (moved_bottom_profile.IsDone()
            && !moved_bottom_profile.Shape().IsNull()) {
            facade_bottom_profile = moved_bottom_profile.Shape();
        } else {
            facade_bottom_profile.Nullify();
        }
    }

//========= Make Up Profile  ===========

    if (!facade_bottom_profile.IsNull()) {
        // The upper belt is the vertical mirror of the finished lower belt.
        // Reflect around the horizontal plane through the facade centre, so
        // the lower profile grows upward and its copy grows downward.
        const double facade_middle_z =
            facade_z + facade_height * 0.5;
        gp_Trsf top_profile_mirror;
        top_profile_mirror.SetMirror(gp_Ax2(
            gp_Pnt(0.0, 0.0, facade_middle_z),
            gp_Dir(0.0, 0.0, 1.0)));

        BRepBuilderAPI_Transform mirrored_top_profile(
            facade_bottom_profile, top_profile_mirror, true);
        mirrored_top_profile.Build();
        if (mirrored_top_profile.IsDone()
            && !mirrored_top_profile.Shape().IsNull()) {
            facade_top_profile = mirrored_top_profile.Shape();
        } else {
            facade_top_profile.Nullify();
        }
    }


    const Vec3 profile_rotation_center{};
    const Vec3 profile_rotation_axis{0.0f, 0.0f, 1.0f};
    frame_profile->Rotate(
        profile_rotation_center, profile_rotation_axis,
        static_cast<float>(PI));

    facade_right_profile = CFacadeFurniture::BuildExtrude(
		frame_profile, CVector{ 0.0, 0.0, 0.1 }, facade_height, &error);
    if (!facade_right_profile.IsNull()) {
        facade_left_profile = CSolid::CopyShape(facade_right_profile);

        CVector cx(1, 0.0, 0.0);
        double dist = width / 2.0;
        CSolid::MoveShape(facade_right_profile, cx, dist);
        CVector cy(0.0, 1.0, 0.0);
        CSolid::MoveShape(facade_right_profile, cy, -depth / 2.0);

        CPlane plane(1, 0, 0, 0);
        CSolid::MirrorShape(facade_left_profile, plane);

        CPoint3d left_rotation_center;
        CVector cz(0, 0.0, 1.0);
        CSolid::RotateShape(
            facade_left_profile, left_rotation_center, cz, -PI / 2.0);
        CSolid::MoveShape(facade_left_profile, cx, -dist);
        CSolid::MoveShape(facade_left_profile, cy, depth / 2.0);

        // A small plan correction seats both Milano stiles against the
        // curved belts. Rotate around the actual Guide2 endpoints so their
        // positions stay fixed; the mitre planes below cut the corrected
        // solids afterwards.
        constexpr double left_stile_turn_radians =
            6.0 * 3.14159265358979323846 / 180.0;
        constexpr double right_stile_turn_radians =
            2.5 * 3.14159265358979323846 / 180.0;
        const CVector vertical_axis(0.0, 0.0, 1.0);
        CSolid::RotateShape(
            facade_left_profile,
            CPoint3d(pA2.x, pA2.y, 0.0),
            vertical_axis, left_stile_turn_radians);
        CSolid::RotateShape(
            facade_right_profile,
            CPoint3d(pC2.x, pC2.y, 0.0),
            vertical_axis, -right_stile_turn_radians);


    }

    // Cut all four frame joints by mitre planes. At each Guide2 endpoint the
    // horizontal belt and vertical stile keep opposite sides of the same
    // 45-degree plane. Work on temporary shapes and commit only if every cut
    // succeeds, so an OCCT boolean failure cannot remove a cabinet part.
    if (!facade_bottom_profile.IsNull() && !facade_top_profile.IsNull()
        && !facade_left_profile.IsNull() && !facade_right_profile.IsNull()
        && Guide2 && Guide2->np() >= 2) {
        CPoint7d guide_start{};
        CPoint7d guide_end{};
        const bool has_end_frames =
            Guide2->GetPoint7d(0.0, &guide_start)
            && Guide2->GetPoint7d(
                static_cast<double>(Guide2->np() - 1), &guide_end);

        gp_Vec start_inward(guide_start.l, guide_start.m, 0.0);
        gp_Vec end_inward(-guide_end.l, -guide_end.m, 0.0);
        if (has_end_frames
            && start_inward.SquareMagnitude() > 1.0e-12
            && end_inward.SquareMagnitude() > 1.0e-12) {
            start_inward.Normalize();
            end_inward.Normalize();

            TopoDS_Shape trimmed_bottom = facade_bottom_profile;
            TopoDS_Shape trimmed_top = facade_top_profile;
            TopoDS_Shape trimmed_left = facade_left_profile;
            TopoDS_Shape trimmed_right = facade_right_profile;

            const auto keep_plane_side = [](
                const TopoDS_Shape& source,
                const gp_Pnt& origin,
                const gp_Vec& normal,
                bool keep_positive,
                TopoDS_Shape& result) {
                if (source.IsNull()
                    || normal.SquareMagnitude() <= 1.0e-12) {
                    return false;
                }
                BRepBuilderAPI_MakeFace plane_face(
                    gp_Pln(origin, gp_Dir(normal)));
                return plane_face.IsDone()
                    && TrimSolidByFace(
                        source, plane_face.Face(), keep_positive, result)
                    && !result.IsNull()
                    && BRepCheck_Analyzer(result).IsValid();
            };
            const gp_Vec vertical(0.0, 0.0, 1.0);
            const auto trim_pair = [&](TopoDS_Shape& horizontal,
                                       TopoDS_Shape& stile,
                                       const gp_Pnt& corner,
                                       const gp_Vec& inward,
                                       bool upper) {
                const gp_Vec plane_normal = upper
                    ? inward + vertical
                    : inward - vertical;
                TopoDS_Shape horizontal_result;
                TopoDS_Shape stile_result;
                if (!keep_plane_side(
                        horizontal, corner, plane_normal,
                        true, horizontal_result)
                    || !keep_plane_side(
                        stile, corner, plane_normal,
                        false, stile_result)) {
                    return false;
                }
                horizontal = std::move(horizontal_result);
                stile = std::move(stile_result);
                return true;
            };

            const gp_Pnt bottom_start(
                guide_start.x, guide_start.y, facade_z);
            const gp_Pnt bottom_end(
                guide_end.x, guide_end.y, facade_z);
            const gp_Pnt top_start(
                guide_start.x, guide_start.y,
                facade_z + facade_height);
            const gp_Pnt top_end(
                guide_end.x, guide_end.y,
                facade_z + facade_height);
            const bool all_joints_trimmed =
                trim_pair(trimmed_bottom, trimmed_left,
                          bottom_start, start_inward, false)
                && trim_pair(trimmed_bottom, trimmed_right,
                             bottom_end, end_inward, false)
                && trim_pair(trimmed_top, trimmed_left,
                             top_start, start_inward, true)
                && trim_pair(trimmed_top, trimmed_right,
                             top_end, end_inward, true);
            if (all_joints_trimmed) {
                facade_bottom_profile = std::move(trimmed_bottom);
                facade_top_profile = std::move(trimmed_top);
                facade_left_profile = std::move(trimmed_left);
                facade_right_profile = std::move(trimmed_right);
            }
        }
    }


    // Place the Radius-3 handle on the untrimmed facade guide.  The outward
    // normal is the right-hand normal of Guide2 (the cabinet interior lies
    // on the opposite side of the curve).
    RoundDoorHandleShapes facade_handle;
    const bool hinge_on_right = definition.door_hinge_side == 1;
    // Guide2 runs from the left hinge candidate pA2 to the right candidate
    // pC2, opposite to side_radius_point's fraction direction. Keep the
    // handle near the free edge, never beside the selected hinge.
    const double handle_fraction = hinge_on_right ? 0.12 : 0.88;
    if (Guide2 && Guide2->np() >= 2) {
        CPoint7d handle_frame{};
        const double handle_parameter =
            static_cast<double>(Guide2->np() - 1) * handle_fraction;
        if (Guide2->GetPoint7d(handle_parameter, &handle_frame)) {
            gp_Vec outward(handle_frame.m, -handle_frame.l, 0.0);
            if (outward.SquareMagnitude() > 1.0e-12) {
                outward.Normalize();
                const double handle_z =
                    facade_z + facade_height - 30.0;
                const gp_Pnt handle_base(
                    handle_frame.x + outward.X() * facade_gap,
                    handle_frame.y + outward.Y() * facade_gap,
                    handle_z);
                facade_handle = round_door_handle_shapes(
                    handle_base, gp_Dir(outward));
            }
        }
    }

	//========= Make Center Panel  facade_center_panel===========

	double sketh_width = 60.0;
    double dd = 2.0;
    CPlane pltr1(0, -1, 0, depth/2.0- sketh_width + dd);
    CPoint3d pc1(width/2.0, -depth / 2.0, 0);
    Guide2->TrimByPlane(&pltr1, &pc1);
    CPlane pltr2(-1, 0, 0, width/2.0- sketh_width+ dd);
    CPoint3d pc2(-width / 2.0, depth/2.0, 0);
    Guide2->TrimByPlane(&pltr2, &pc2);
	double offset = 6.0;
    Guide2->Offset(-offset);
    CVector cz(0, 0.0, 1.0);
    Guide2->Move(&cz, sketh_width-2);

    CSurfaceFace face;
    CVector dir(0.0, 0.0, 1.0);
	double distance =   height - sketh_width*2.0 +2;
    if (!face.CreateRuled(Guide2, dir, distance))
        return {};
	double thickFilenka = 6.0;
    facade_center_panel = CSolid::Shell(&face, thickFilenka, &error);

    // Radius-3 is assembled from five independent facade solids. Rotate all
    // of them and the handle around exactly the same hinge axis.
    const double hinge_x = hinge_on_right ? pC2.x : pA2.x;
    const double hinge_y = hinge_on_right ? pC2.y : pA2.y;
    const double open_angle = hinge_on_right
        ? definition.door_open_angle : -definition.door_open_angle;
    facade_bottom_profile = rotate_about_z(
        facade_bottom_profile, hinge_x, hinge_y, open_angle);
    facade_top_profile = rotate_about_z(
        facade_top_profile, hinge_x, hinge_y, open_angle);
    facade_left_profile = rotate_about_z(
        facade_left_profile, hinge_x, hinge_y, open_angle);
    facade_right_profile = rotate_about_z(
        facade_right_profile, hinge_x, hinge_y, open_angle);
    facade_center_panel = rotate_about_z(
        facade_center_panel, hinge_x, hinge_y, open_angle);
    rotate_round_door_handle(
        facade_handle, hinge_x, hinge_y, open_angle);

    add_part("Radius-3 Cabinet Facade Bottom Profile",
             std::move(facade_bottom_profile));
    add_part("Radius-3 Cabinet Facade Top Profile",
             std::move(facade_top_profile));
    add_part("Radius-3 Cabinet Facade Left Profile",
             std::move(facade_left_profile));
    add_part("Radius-3 Cabinet Facade Right Profile",
             std::move(facade_right_profile));
    add_part("Radius-3 Cabinet Facade Center Panel",
             std::move(facade_center_panel));
    add_part("Radius-3 Cabinet Facade Handle Base",
             std::move(facade_handle.base));
    add_part("Radius-3 Cabinet Facade Handle Face",
             std::move(facade_handle.face));

    // Эти значения уже подготовлены для первых построений. Удалите строки
    // (void), когда соответствующий параметр начнёт использоваться.
    (void)thickness;
    (void)left;
    (void)right;
    (void)front;
    (void)back;
    (void)facade_height;
    (void)guide;
    (void)frame_profile;

    return parts;
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
        if ((definition.facade_style == KitchenCabinetFacadeStyle::Screen
             || definition.facade_style == KitchenCabinetFacadeStyle::Frame
             || definition.facade_style == KitchenCabinetFacadeStyle::Milled
             || definition.facade_style == KitchenCabinetFacadeStyle::Milano)
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
        const bool curved_milano =
            definition.facade_style == KitchenCabinetFacadeStyle::Milano
            && name.find("Radius") != std::string::npos;
        if (curved_milano
            && technological_parts[0].ShapeType() == TopAbs_COMPOUND) {
            std::vector<TopoDS_Shape> profiles;
            for (TopoDS_Iterator iterator(technological_parts[0]);
                 iterator.More(); iterator.Next()) {
                profiles.push_back(iterator.Value());
            }
            if (profiles.size() == 4) {
                add(name + " Bottom Profile", std::move(profiles[0]), facade_color);
                add(name + " Top Profile", std::move(profiles[1]), facade_color);
                add(name + " Right Profile", std::move(profiles[2]), facade_color);
                add(name + " Left Profile", std::move(profiles[3]), facade_color);
                add(name + " Center Panel",
                    std::move(technological_parts[1]), facade_color);
                return;
            }
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
    } else if (definition.body_type == KitchenCabinetBodyType::Radius3) {
        // Radius-3 is now constructed by build_radius3_cabinet_parts() below.
        // Keep the old carcass builder disabled while the replacement is
        // being filled with the new individual parts.
    } else if (definition.body_type == KitchenCabinetBodyType::Radius4) {
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
        const bool radius4 =
            definition.body_type == KitchenCabinetBodyType::Radius4;
        const double left_side_front_clearance =
            radius4 ? 0.0 : side_panel_clearance;
        const double left_side_depth =
            geometry.straight_length - left_side_front_clearance;
        if (left_side_depth > 0.1) {
            add(prefix + "Left Side",
                box_shape(left,
                          geometry.center_y + left_side_front_clearance, 0.0,
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
    } else if (definition.body_type == KitchenCabinetBodyType::Radius3) {
        // The new Radius-3 facade parts are appended by
        // build_radius3_cabinet_parts() below. Do not call the legacy facade
        // construction in parallel.
    } else if (definition.body_type == KitchenCabinetBodyType::Radius4
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
        const auto make_facade = [&](double start_fraction,
                                     double end_fraction) {
            if (definition.facade_style
                == KitchenCabinetFacadeStyle::Milano) {
                return side_radius_milano_facade_shape(
                    definition, straight_length,
                    start_fraction, end_fraction,
                    gap, facade_height, thickness, gap);
            }
            return side_radius_facade_shape(
                definition, straight_length,
                start_fraction, end_fraction,
                gap, facade_height, thickness, gap, 3.0);
        };
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = make_facade(0.0, 1.0);
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
            add_facade(prefix + "Facade", std::move(facade));
            add_round_handle(prefix + "Facade", std::move(handle));
        } else {
            TopoDS_Shape right_facade = make_facade(
                0.0, 0.5 - fraction_gap);
            RoundDoorHandleShapes right_handle = make_handle(0.44);
            right_facade = rotate_about_z(
                right_facade, right_hinge.X(), right_hinge.Y(), right_open_angle);
            rotate_round_door_handle(
                right_handle, right_hinge.X(), right_hinge.Y(), right_open_angle);
            TopoDS_Shape left_facade = make_facade(
                0.5 + fraction_gap, 1.0);
            RoundDoorHandleShapes left_handle = make_handle(0.56);
            left_facade = rotate_about_z(
                left_facade, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            rotate_round_door_handle(
                left_handle, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            add_facade(prefix + "Right Facade", std::move(right_facade));
            add_facade(prefix + "Left Facade", std::move(left_facade));
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
        const auto make_facade = [&](double start_fraction,
                                     double end_fraction) {
            if (definition.facade_style
                == KitchenCabinetFacadeStyle::Milano) {
                return radius2_milano_facade_shape(
                    definition, start_fraction, end_fraction,
                    gap, facade_height, thickness, gap);
            }
            return radius2_facade_shape(
                definition, start_fraction, end_fraction,
                gap, facade_height, thickness, gap, 3.0);
        };
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = make_facade(0.0, 1.0);
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
            add_facade("Radius-2 Cabinet Facade", std::move(facade));
            add_round_handle("Radius-2 Cabinet Facade", std::move(handle));
        } else {
            TopoDS_Shape right_facade = make_facade(
                0.0, 0.5 - fraction_gap);
            RoundDoorHandleShapes right_handle = make_handle(0.44);
            right_facade = rotate_about_z(
                right_facade, right_hinge.X(), right_hinge.Y(), right_open_angle);
            rotate_round_door_handle(
                right_handle, right_hinge.X(), right_hinge.Y(), right_open_angle);
            TopoDS_Shape left_facade = make_facade(
                0.5 + fraction_gap, 1.0);
            RoundDoorHandleShapes left_handle = make_handle(0.56);
            left_facade = rotate_about_z(
                left_facade, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            rotate_round_door_handle(
                left_handle, left_hinge.X(), left_hinge.Y(), -left_open_angle);
            add_facade("Radius-2 Cabinet Right Facade",
                       std::move(right_facade));
            add_facade("Radius-2 Cabinet Left Facade",
                       std::move(left_facade));
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
        const auto make_facade = [&](double start_angle,
                                     double end_angle) {
            if (definition.facade_style
                == KitchenCabinetFacadeStyle::Milano) {
                return radius_milano_facade_shape(
                    definition, start_angle, end_angle,
                    gap, facade_height, thickness, gap);
            }
            return radius_facade_shape(
                definition, start_angle, end_angle,
                gap, facade_height, thickness, gap, 3.0);
        };
        if (definition.facade_type == KitchenCabinetFacadeType::SingleDoor) {
            TopoDS_Shape facade = make_facade(
                angular_gap, pi - angular_gap);
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
            add_facade("Cabinet Radius Facade", std::move(facade));
            add_round_handle("Cabinet Radius Facade", std::move(handle));
        } else {
            TopoDS_Shape right_facade = make_facade(
                angular_gap, pi * 0.5 - angular_gap);
            RoundDoorHandleShapes right_handle = make_handle(0.44);
            right_facade = rotate_about_z(
                right_facade, hinge_radius_x, 0.0, right_open_angle);
            rotate_round_door_handle(
                right_handle, hinge_radius_x, 0.0, right_open_angle);
            TopoDS_Shape left_facade = make_facade(
                pi * 0.5 + angular_gap, pi - angular_gap);
            RoundDoorHandleShapes left_handle = make_handle(0.56);
            left_facade = rotate_about_z(
                left_facade, -hinge_radius_x, 0.0, -left_open_angle);
            rotate_round_door_handle(
                left_handle, -hinge_radius_x, 0.0, -left_open_angle);
            add_facade("Cabinet Radius Right Facade",
                       std::move(right_facade));
            add_facade("Cabinet Radius Left Facade",
                       std::move(left_facade));
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

    // Radius-3 is built here by the new individual-part generator. The legacy
    // carcass and facade branches above intentionally do nothing for Radius-3.
    if (definition.body_type == KitchenCabinetBodyType::Radius3) {
        std::vector<NamedCabinetShape> prototype_parts =
            build_radius3_cabinet_parts(definition);
        for (NamedCabinetShape& prototype_part : prototype_parts) {
            const bool handle_base_part =
                prototype_part.first.find("Handle Base") != std::string::npos;
            const bool facade_part =
                prototype_part.first.find("Facade") != std::string::npos;
            const bool shelf_part =
                prototype_part.first.find("Shelf") != std::string::npos;
            add(prototype_part.first,
                std::move(prototype_part.second),
                handle_base_part ? handle_color
                    : facade_part ? facade_color
                    : shelf_part ? shelf_color : carcass_color);
        }
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
