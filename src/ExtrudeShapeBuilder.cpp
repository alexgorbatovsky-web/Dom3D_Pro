#include "ExtrudeShapeBuilder.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_BezierCurve.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Standard_Failure.hxx>
#include <ShapeFix_Wire.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <TColgp_Array1OfPnt.hxx>

#include <cmath>
#include <vector>

namespace {
constexpr double kLinearTolerance = 1.0e-4;
constexpr double kAngleToleranceDegrees = 1.0e-4;
constexpr double kPiValue = 3.14159265358979323846;

bool is_cap_face(const TopoDS_Face& face, Vec3 extrusion_normal)
{
    BRepAdaptor_Surface surface(face, true);
    if (surface.GetType() != GeomAbs_Plane) {
        return false;
    }

    const gp_Dir face_normal = surface.Plane().Axis().Direction();
    const double alignment = std::fabs(face_normal.X() * extrusion_normal.x
        + face_normal.Y() * extrusion_normal.y
        + face_normal.Z() * extrusion_normal.z);
    return alignment > 0.98;
}

bool draft_angle_supports_all_lateral_faces(const TopoDS_Shape& prism, Vec3 extrusion_normal)
{
    for (TopExp_Explorer explorer(prism, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        if (is_cap_face(face, extrusion_normal)) {
            continue;
        }

        BRepAdaptor_Surface surface(face, true);
        const GeomAbs_SurfaceType type = surface.GetType();
        if (type != GeomAbs_Plane && type != GeomAbs_Cylinder && type != GeomAbs_Cone) {
            return false;
        }
    }
    return true;
}

TopoDS_Shape build_supported_draft(const TopoDS_Shape& prism,
                                   const TopoDS_Face& profile_face,
                                   Vec3 normal,
                                   double distance,
                                   double angle_radians)
{
    BRepAdaptor_Surface base_surface(profile_face, true);
    if (base_surface.GetType() != GeomAbs_Plane) {
        return {};
    }

    const double direction_sign = distance >= 0.0 ? 1.0 : -1.0;
    const gp_Dir draft_direction(normal.x * direction_sign,
                                 normal.y * direction_sign,
                                 normal.z * direction_sign);
    const gp_Pln neutral_plane = base_surface.Plane();

    BRepOffsetAPI_DraftAngle draft(prism);
    for (TopExp_Explorer explorer(prism, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        if (is_cap_face(face, normal)) {
            continue;
        }

        draft.Add(face, draft_direction, angle_radians, neutral_plane);
        if (!draft.AddDone()) {
            draft.Remove(face);
            return {};
        }
    }

    draft.Build();
    if (!draft.IsDone() || draft.Shape().IsNull()) {
        return {};
    }
    return draft.Shape();
}

TopoDS_Wire first_wire(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return {};
    }
    Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges->Append(explorer.Current());
    }
    Handle(TopTools_HSequenceOfShape) wires = new TopTools_HSequenceOfShape;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, 0.001, Standard_False, wires);
    if (wires->Length() == 1 && wires->Value(1).ShapeType() == TopAbs_WIRE) {
        return TopoDS::Wire(wires->Value(1));
    }
    return {};
}

struct TaperEdgeData {
    TopoDS_Edge edge;
    GeomAbs_CurveType type = GeomAbs_OtherCurve;
    gp_Pnt start;
    gp_Pnt end;
    gp_Vec start_tangent;
    gp_Vec end_tangent;
    bool reversed = false;
};

double oriented_cross(const gp_Vec& first, const gp_Vec& second, const gp_Vec& normal)
{
    return first.Crossed(second).Dot(normal);
}

gp_Pnt shifted_corner(const gp_Pnt& corner,
                      const gp_Vec& incoming,
                      const gp_Vec& outgoing,
                      const gp_Vec& plane_normal,
                      double offset)
{
    gp_Vec incoming_unit = incoming;
    gp_Vec outgoing_unit = outgoing;
    incoming_unit.Normalize();
    outgoing_unit.Normalize();

    gp_Vec incoming_normal = plane_normal.Crossed(incoming_unit);
    gp_Vec outgoing_normal = plane_normal.Crossed(outgoing_unit);
    incoming_normal.Normalize();
    outgoing_normal.Normalize();

    const gp_Pnt on_incoming = corner.Translated(incoming_normal * offset);
    const gp_Pnt on_outgoing = corner.Translated(outgoing_normal * offset);
    const double denominator = oriented_cross(incoming_unit, outgoing_unit, plane_normal);
    if (std::fabs(denominator) > 1.0e-9) {
        const gp_Vec between(on_incoming, on_outgoing);
        const double parameter =
            oriented_cross(between, outgoing_unit, plane_normal) / denominator;
        return on_incoming.Translated(incoming_unit * parameter);
    }

    gp_Vec average = incoming_normal + outgoing_normal;
    if (average.SquareMagnitude() <= 1.0e-18) {
        average = outgoing_normal;
    }
    average.Normalize();
    const double projection = average.Dot(outgoing_normal);
    const double length = std::fabs(projection) > 1.0e-9 ? offset / projection : offset;
    return corner.Translated(average * length);
}

TopoDS_Wire build_same_topology_offset_wire(const TopoDS_Wire& base_wire,
                                            Vec3 normal,
                                            double offset)
{
    std::vector<TaperEdgeData> edges;
    for (BRepTools_WireExplorer explorer(base_wire); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = explorer.Current();
        BRepAdaptor_Curve curve(edge);
        const GeomAbs_CurveType type = curve.GetType();
        if (type != GeomAbs_Line
            && type != GeomAbs_BezierCurve
            && type != GeomAbs_Circle) {
            return {};
        }

        TaperEdgeData data;
        data.edge = edge;
        data.type = type;
        data.reversed = edge.Orientation() == TopAbs_REVERSED;

        gp_Pnt first_point;
        gp_Pnt last_point;
        gp_Vec first_tangent;
        gp_Vec last_tangent;
        curve.D1(curve.FirstParameter(), first_point, first_tangent);
        curve.D1(curve.LastParameter(), last_point, last_tangent);
        if (data.reversed) {
            data.start = last_point;
            data.end = first_point;
            data.start_tangent = -last_tangent;
            data.end_tangent = -first_tangent;
        } else {
            data.start = first_point;
            data.end = last_point;
            data.start_tangent = first_tangent;
            data.end_tangent = last_tangent;
        }
        if (data.start_tangent.SquareMagnitude() <= 1.0e-18
            || data.end_tangent.SquareMagnitude() <= 1.0e-18) {
            return {};
        }
        edges.push_back(data);
    }
    if (edges.size() < 2) {
        return {};
    }

    gp_Vec plane_normal(normal.x, normal.y, normal.z);
    if (plane_normal.SquareMagnitude() <= 1.0e-18) {
        return {};
    }
    plane_normal.Normalize();

    std::vector<gp_Pnt> corners;
    corners.reserve(edges.size());
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const TaperEdgeData& previous = edges[(index + edges.size() - 1) % edges.size()];
        const TaperEdgeData& current = edges[index];
        corners.push_back(shifted_corner(
            current.start,
            previous.end_tangent,
            current.start_tangent,
            plane_normal,
            offset));
    }

    BRepBuilderAPI_MakeWire wire_builder;
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const std::size_t next = (index + 1) % edges.size();
        BRepBuilderAPI_MakeEdge edge_builder;
        if (edges[index].type == GeomAbs_Line) {
            edge_builder = BRepBuilderAPI_MakeEdge(corners[index], corners[next]);
        } else if (edges[index].type == GeomAbs_BezierCurve) {
            BRepAdaptor_Curve adaptor(edges[index].edge);
            const Handle(Geom_BezierCurve) adapted_curve = adaptor.Bezier();
            if (adapted_curve.IsNull() || adapted_curve->IsRational()) {
                return {};
            }
            Handle(Geom_BezierCurve) source =
                Handle(Geom_BezierCurve)::DownCast(adapted_curve->Copy());
            source->Segment(adaptor.FirstParameter(), adaptor.LastParameter());

            const int pole_count = source->NbPoles();
            TColgp_Array1OfPnt poles(1, pole_count);
            const gp_Pnt source_start =
                edges[index].reversed ? source->Pole(pole_count) : source->Pole(1);
            const gp_Pnt source_end =
                edges[index].reversed ? source->Pole(1) : source->Pole(pole_count);
            const gp_Vec start_shift(source_start, corners[index]);
            const gp_Vec end_shift(source_end, corners[next]);
            for (int pole_index = 1; pole_index <= pole_count; ++pole_index) {
                const int source_index =
                    edges[index].reversed ? pole_count - pole_index + 1 : pole_index;
                const double parameter = pole_count > 1
                    ? static_cast<double>(pole_index - 1) / static_cast<double>(pole_count - 1)
                    : 0.0;
                gp_Vec shift = start_shift * (1.0 - parameter) + end_shift * parameter;
                poles.SetValue(pole_index, source->Pole(source_index).Translated(shift));
            }
            const Handle(Geom_BezierCurve) tapered_curve = new Geom_BezierCurve(poles);
            edge_builder = BRepBuilderAPI_MakeEdge(tapered_curve);
        } else {
            BRepAdaptor_Curve adaptor(edges[index].edge);
            const gp_Pnt center = adaptor.Circle().Location();
            const double start_radius = center.Distance(corners[index]);
            const double end_radius = center.Distance(corners[next]);
            const double radius = (start_radius + end_radius) * 0.5;
            if (radius <= kLinearTolerance
                || std::fabs(start_radius - end_radius) > 0.001) {
                return {};
            }

            const double middle_parameter =
                (adaptor.FirstParameter() + adaptor.LastParameter()) * 0.5;
            const gp_Pnt source_middle = adaptor.Value(middle_parameter);
            gp_Vec middle_radius(center, source_middle);
            if (middle_radius.SquareMagnitude() <= 1.0e-18) {
                return {};
            }
            middle_radius.Normalize();
            const gp_Pnt middle = center.Translated(middle_radius * radius);
            GC_MakeArcOfCircle arc(corners[index], middle, corners[next]);
            if (!arc.IsDone()) {
                return {};
            }
            edge_builder = BRepBuilderAPI_MakeEdge(arc.Value());
        }

        if (!edge_builder.IsDone()) {
            return {};
        }
        wire_builder.Add(edge_builder.Edge());
    }
    if (!wire_builder.IsDone()) {
        return {};
    }
    TopoDS_Wire result = wire_builder.Wire();
    result.Closed(Standard_True);
    return result;
}

TopoDS_Shape build_freeform_draft(const TopoDS_Face& profile_face,
                                  Vec3 normal,
                                  double distance,
                                  double angle_radians)
{
    TopoDS_Wire base_wire = BRepTools::OuterWire(profile_face);
    if (base_wire.IsNull()) {
        return {};
    }
    base_wire = first_wire(base_wire);
    if (base_wire.IsNull()) {
        return {};
    }
    ShapeFix_Wire base_wire_fixer(base_wire, profile_face, 0.001);
    base_wire_fixer.Perform();
    base_wire_fixer.FixClosed(0.001);
    base_wire = base_wire_fixer.WireAPIMake();
    base_wire.Closed(Standard_True);

    const double offset_distance = std::fabs(distance) * std::tan(angle_radians);
    TopoDS_Wire end_wire =
        build_same_topology_offset_wire(base_wire, normal, offset_distance);
    if (end_wire.IsNull()) {
        // General fallback for curve types other than line and Bezier. Rounded
        // joins guarantee a closed wire when OCCT cannot build sharp joins.
        BRepOffsetAPI_MakeOffset offset(profile_face, GeomAbs_Arc, Standard_False);
        offset.Perform(offset_distance);
        if (!offset.IsDone()) {
            return {};
        }
        end_wire = first_wire(offset.Shape());
        if (end_wire.IsNull()) {
            return {};
        }
    }
    end_wire.Closed(Standard_True);

    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(normal.x * distance,
                                      normal.y * distance,
                                      normal.z * distance));
    BRepBuilderAPI_Transform move(end_wire, translation, true);
    move.Build();
    if (!move.IsDone()) {
        return {};
    }
    end_wire = first_wire(move.Shape());
    if (end_wire.IsNull()) {
        return {};
    }
    end_wire.Closed(Standard_True);

    const gp_Pnt end_origin(normal.x * distance, normal.y * distance, normal.z * distance);
    const gp_Pln end_plane(end_origin, gp_Dir(normal.x, normal.y, normal.z));
    BRepBuilderAPI_MakeFace provisional_end_face(end_plane, end_wire, true);
    if (!provisional_end_face.IsDone()) {
        return {};
    }
    ShapeFix_Wire wire_fixer(end_wire, provisional_end_face.Face(), 0.001);
    wire_fixer.Perform();
    wire_fixer.FixClosed(0.001);
    end_wire = wire_fixer.WireAPIMake();
    end_wire.Closed(Standard_True);
    BRepOffsetAPI_ThruSections loft(Standard_True, Standard_True, 0.001);
    loft.CheckCompatibility(Standard_True);
    loft.AddWire(base_wire);
    loft.AddWire(end_wire);
    loft.Build();
    if (!loft.IsDone() || loft.Shape().IsNull()) {
        return {};
    }

    const TopoDS_Shape result = loft.Shape();
    if (!BRepCheck_Analyzer(result).IsValid()) {
        return {};
    }
    return result;
}
}

TopoDS_Shape BuildExtrudeShape(const TopoDS_Face& profile_face,
                               Vec3 normal,
                               double distance,
                               double taper_angle_degrees)
{
    if (profile_face.IsNull() || std::fabs(distance) <= kLinearTolerance) {
        return {};
    }

    try {
        const gp_Vec vector(normal.x * distance, normal.y * distance, normal.z * distance);
        BRepPrimAPI_MakePrism prism_builder(profile_face, vector, false, true);
        prism_builder.Build();
        if (!prism_builder.IsDone() || prism_builder.Shape().IsNull()) {
            return {};
        }

        const TopoDS_Shape prism = prism_builder.Shape();
        if (std::fabs(taper_angle_degrees) <= kAngleToleranceDegrees) {
            return prism;
        }

        const double angle_radians = taper_angle_degrees * kPiValue / 180.0;
        if (!std::isfinite(angle_radians) || std::fabs(angle_radians) >= kPiValue * 0.5) {
            return {};
        }

        // DraftAngle explicitly supports only planar, cylindrical and conical
        // faces. A Bezier edge produces a free-form surface of extrusion, so
        // sending only its neighbouring planar faces to DraftAngle can leave
        // the algorithm in an inconsistent state during Build().
        if (draft_angle_supports_all_lateral_faces(prism, normal)) {
            const TopoDS_Shape drafted =
                build_supported_draft(prism, profile_face, normal, distance, angle_radians);
            if (!drafted.IsNull()) {
                return drafted;
            }
        }

        return build_freeform_draft(profile_face, normal, distance, angle_radians);
    } catch (const Standard_Failure&) {
        return {};
    }
}
