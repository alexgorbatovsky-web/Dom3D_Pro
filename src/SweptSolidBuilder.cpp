#include "SweptSolidBuilder.h"

#include "CAlfaDoc.h"
#include "CBSpline.h"
#include "CPolyline.h"
#include "SketchProfileBuilder.h"
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

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_TransitionMode.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakeEvolved.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <BRepTools.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_GTrsf.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {
constexpr double kGeometryTolerance = 1.0e-9;
constexpr double kPi = 3.14159265358979323846;

CPoint3d subtract(const CPoint3d& left, const CPoint3d& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

CPoint3d add(const CPoint3d& left, const CPoint3d& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

CPoint3d multiply(const CPoint3d& point, double factor) {
    return {point.x * factor, point.y * factor, point.z * factor};
}

double dot3(const CPoint3d& left, const CPoint3d& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

CPoint3d cross(const CPoint3d& left, const CPoint3d& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

bool normalize(CPoint3d& point) {
    const double length = std::sqrt(dot3(point, point));
    if (length <= kGeometryTolerance) {
        return false;
    }
    point = multiply(point, 1.0 / length);
    return true;
}

bool initial_guide_frame(const CAlfaObject& guide,
                         CPoint3d& origin,
                         CPoint3d& tangent,
                         CPoint3d& guide_normal) {
    if (const auto* sketch = dynamic_cast<const CSmartLine*>(&guide)) {
        const std::vector<CPoint3d> points = sketch->GetProfilePointsWorld();
        if (points.size() < 2) {
            return false;
        }
        origin = points.front();
        for (std::size_t index = 1; index < points.size(); ++index) {
            tangent = subtract(points[index], origin);
            if (normalize(tangent)) {
                break;
            }
        }
        if (dot3(tangent, tangent) <= kGeometryTolerance) {
            return false;
        }
        guide_normal = sketch->GetCoordinateSystem().normal;
        return normalize(guide_normal);
    }

    const auto* spline = dynamic_cast<const CBSpline*>(&guide);
    if (!spline || spline->GetPointCount() < 2) {
        return false;
    }
    origin = spline->Evaluate(0.0f);
    for (int step = 1; step <= 100; ++step) {
        tangent = subtract(spline->Evaluate(static_cast<float>(step) / 1000.0f), origin);
        if (normalize(tangent)) {
            break;
        }
    }
    if (dot3(tangent, tangent) <= kGeometryTolerance) {
        return false;
    }

    // A free 3D B-Spline has no sketch plane. Use its first non-collinear
    // control direction as the plane normal, with a stable world-axis fallback.
    for (const CPoint3d& point : spline->GetPoints()) {
        guide_normal = cross(tangent, subtract(point, origin));
        if (normalize(guide_normal)) {
            return true;
        }
    }
    CPoint3d reference = std::abs(tangent.z) < 0.9
        ? CPoint3d(0.0, 0.0, 1.0)
        : CPoint3d(0.0, 1.0, 0.0);
    guide_normal = cross(tangent, reference);
    return normalize(guide_normal);
}

TopoDS_Wire place_section_at_guide_start(const TopoDS_Wire& section_wire,
                                         const CSmartLine& section,
                                         const CAlfaObject& guide,
                                         double delta_x,
                                         double delta_y,
                                         double angle_degrees) {
    CPoint3d target_origin;
    CPoint3d target_z;
    CPoint3d guide_normal;
    if (!initial_guide_frame(guide, target_origin, target_z, guide_normal)) {
        return {};
    }

    CPoint3d base_x = cross(guide_normal, target_z);
    if (!normalize(base_x)) {
        return {};
    }
    CPoint3d base_y = cross(target_z, base_x);
    if (!normalize(base_y)) {
        return {};
    }

    target_origin = add(
        target_origin,
        add(multiply(base_x, delta_x), multiply(base_y, delta_y)));

    const double angle = angle_degrees
        * 3.14159265358979323846 / 180.0;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const CPoint3d target_x = add(multiply(base_x, cosine), multiply(base_y, sine));
    const CPoint3d target_y = add(multiply(base_y, cosine), multiply(base_x, -sine));

    const SketchCoordinateSystem& source = section.GetCoordinateSystem();
    double rotation[3][3]{};
    const double target_matrix[3][3] = {
        {target_x.x, target_y.x, target_z.x},
        {target_x.y, target_y.y, target_z.y},
        {target_x.z, target_y.z, target_z.z}
    };
    const double source_matrix[3][3] = {
        {source.x_axis.x, source.y_axis.x, source.normal.x},
        {source.x_axis.y, source.y_axis.y, source.normal.y},
        {source.x_axis.z, source.y_axis.z, source.normal.z}
    };
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            for (int axis = 0; axis < 3; ++axis) {
                rotation[row][column] +=
                    target_matrix[row][axis] * source_matrix[column][axis];
            }
        }
    }

    const double translation[3] = {
        target_origin.x - rotation[0][0] * source.origin.x
            - rotation[0][1] * source.origin.y - rotation[0][2] * source.origin.z,
        target_origin.y - rotation[1][0] * source.origin.x
            - rotation[1][1] * source.origin.y - rotation[1][2] * source.origin.z,
        target_origin.z - rotation[2][0] * source.origin.x
            - rotation[2][1] * source.origin.y - rotation[2][2] * source.origin.z
    };

    gp_Trsf transform;
    transform.SetValues(
        rotation[0][0], rotation[0][1], rotation[0][2], translation[0],
        rotation[1][0], rotation[1][1], rotation[1][2], translation[1],
        rotation[2][0], rotation[2][1], rotation[2][2], translation[2]);
    BRepBuilderAPI_Transform builder(section_wire, transform, true);
    return builder.IsDone() ? TopoDS::Wire(builder.Shape()) : TopoDS_Wire{};
}

double evaluate_scale_graph(const std::vector<double>& values, double parameter) {
    if (values.empty()) return 1.0;
    if (values.size() == 1) return std::max(0.01, values.front());
    const double position = std::clamp(parameter, 0.0, 1.0) * (values.size() - 1);
    const int segment = std::min(
        static_cast<int>(values.size()) - 2,
        std::max(0, static_cast<int>(std::floor(position))));
    const double u = position - segment;
    const auto value = [&values](int index) {
        return values[static_cast<size_t>(std::clamp(
            index, 0, static_cast<int>(values.size()) - 1))];
    };
    const double p0 = value(segment - 1);
    const double p1 = value(segment);
    const double p2 = value(segment + 1);
    const double p3 = value(segment + 2);
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double result = 0.5 * ((2.0 * p1)
        + (-p0 + p2) * u
        + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * u2
        + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * u3);
    return std::max(0.01, result);
}

bool scale_graph_is_neutral(const std::vector<double>& values) {
    return values.empty() || std::all_of(values.begin(), values.end(), [](double value) {
        return std::abs(value - 1.0) <= 1.0e-9;
    });
}

bool guide_frame_at(const CAlfaObject& guide,
                    double parameter,
                    CPoint3d& origin,
                    CPoint3d& tangent,
                    CPoint3d& guide_normal) {
    parameter = std::clamp(parameter, 0.0, 1.0);
    if (const auto* sketch = dynamic_cast<const CSmartLine*>(&guide)) {
        const std::vector<CPoint3d> points = sketch->GetProfilePointsWorld();
        if (points.size() < 2) return false;
        std::vector<double> lengths(points.size(), 0.0);
        for (size_t index = 1; index < points.size(); ++index) {
            const CPoint3d edge = subtract(points[index], points[index - 1]);
            lengths[index] = lengths[index - 1] + std::sqrt(dot3(edge, edge));
        }
        if (lengths.back() <= kGeometryTolerance) return false;
        const double distance = parameter * lengths.back();
        size_t segment = 0;
        while (segment + 1 < lengths.size() - 1
               && lengths[segment + 1] < distance) {
            ++segment;
        }
        const double segment_length = lengths[segment + 1] - lengths[segment];
        const double local = segment_length <= kGeometryTolerance
            ? 0.0 : (distance - lengths[segment]) / segment_length;
        origin = add(points[segment], multiply(
            subtract(points[segment + 1], points[segment]), local));
        tangent = subtract(points[segment + 1], points[segment]);
        guide_normal = sketch->GetCoordinateSystem().normal;
        return normalize(tangent) && normalize(guide_normal);
    }

    const auto* spline = dynamic_cast<const CBSpline*>(&guide);
    if (!spline || spline->GetPointCount() < 2) return false;
    origin = spline->Evaluate(static_cast<float>(parameter));
    const double before = std::max(0.0, parameter - 0.0005);
    const double after = std::min(1.0, parameter + 0.0005);
    tangent = subtract(spline->Evaluate(static_cast<float>(after)),
                       spline->Evaluate(static_cast<float>(before)));
    CPoint3d start_origin;
    CPoint3d start_tangent;
    return normalize(tangent)
        && initial_guide_frame(guide, start_origin, start_tangent, guide_normal);
}

TopoDS_Wire place_scaled_section(const TopoDS_Wire& source_wire,
                                 const CSmartLine& section,
                                 const CAlfaObject& guide,
                                 double parameter,
                                 double width_scale,
                                 double height_scale,
                                 double delta_x,
                                 double delta_y,
                                 double angle_degrees) {
    CPoint3d target_origin;
    CPoint3d target_z;
    CPoint3d guide_normal;
    if (!guide_frame_at(guide, parameter, target_origin, target_z, guide_normal)) {
        return {};
    }
    CPoint3d base_x = cross(guide_normal, target_z);
    if (!normalize(base_x)) return {};
    CPoint3d base_y = cross(target_z, base_x);
    if (!normalize(base_y)) return {};
    target_origin = add(target_origin,
                        add(multiply(base_x, delta_x), multiply(base_y, delta_y)));

    const double angle = angle_degrees * kPi / 180.0;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const CPoint3d target_x = add(multiply(base_x, cosine), multiply(base_y, sine));
    const CPoint3d target_y = add(multiply(base_y, cosine), multiply(base_x, -sine));
    const SketchCoordinateSystem& source = section.GetCoordinateSystem();
    const CPoint3d source_axes[3] = {source.x_axis, source.y_axis, source.normal};
    const CPoint3d target_axes[3] = {
        multiply(target_x, width_scale),
        multiply(target_y, height_scale),
        target_z
    };
    double matrix[3][3]{};
    for (int row = 0; row < 3; ++row) {
        const double target_components[3] = {
            row == 0 ? target_axes[0].x : (row == 1 ? target_axes[0].y : target_axes[0].z),
            row == 0 ? target_axes[1].x : (row == 1 ? target_axes[1].y : target_axes[1].z),
            row == 0 ? target_axes[2].x : (row == 1 ? target_axes[2].y : target_axes[2].z)
        };
        for (int column = 0; column < 3; ++column) {
            const double source_components[3] = {
                column == 0 ? source_axes[0].x : (column == 1 ? source_axes[0].y : source_axes[0].z),
                column == 0 ? source_axes[1].x : (column == 1 ? source_axes[1].y : source_axes[1].z),
                column == 0 ? source_axes[2].x : (column == 1 ? source_axes[2].y : source_axes[2].z)
            };
            for (int axis = 0; axis < 3; ++axis) {
                matrix[row][column] += target_components[axis] * source_components[axis];
            }
        }
    }
    const double source_origin[3] = {source.origin.x, source.origin.y, source.origin.z};
    const double target[3] = {target_origin.x, target_origin.y, target_origin.z};
    gp_GTrsf transform;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            transform.SetValue(row + 1, column + 1, matrix[row][column]);
        }
        double translation = target[row];
        for (int column = 0; column < 3; ++column) {
            translation -= matrix[row][column] * source_origin[column];
        }
        transform.SetValue(row + 1, 4, translation);
    }
    BRepBuilderAPI_GTransform builder(source_wire, transform, true);
    return builder.IsDone() ? TopoDS::Wire(builder.Shape()) : TopoDS_Wire{};
}

TopoDS_Wire build_polyline_wire(const std::vector<CPoint3d>& points, bool close) {
    if (points.size() < 2) {
        return {};
    }
    BRepBuilderAPI_MakePolygon polygon;
    for (const CPoint3d& point : points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    if (close) {
        polygon.Close();
    }
    return polygon.IsDone() ? polygon.Wire() : TopoDS_Wire{};
}

TopoDS_Wire build_bspline_wire(const CBSpline& spline) {
    if (spline.IsClosed() || spline.GetPointCount() < 2) {
        return {};
    }

    const int sample_count = std::max(24, static_cast<int>(spline.GetPointCount()) * 16);
    TColgp_Array1OfPnt samples(1, sample_count);
    for (int index = 0; index < sample_count; ++index) {
        const float parameter = static_cast<float>(index) / static_cast<float>(sample_count - 1);
        const CPoint3d point = spline.Evaluate(parameter);
        samples.SetValue(index + 1, gp_Pnt(point.x, point.y, point.z));
    }

    GeomAPI_PointsToBSpline curve_builder(samples);
    const Handle(Geom_BSplineCurve) curve = curve_builder.Curve();
    if (curve.IsNull()) {
        return {};
    }
    BRepBuilderAPI_MakeEdge edge_builder(curve);
    if (!edge_builder.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeWire wire_builder(edge_builder.Edge());
    return wire_builder.IsDone() ? wire_builder.Wire() : TopoDS_Wire{};
}

BRepBuilderAPI_TransitionMode transition_mode_from_index(int mode) {
    if (mode == 0) {
        return BRepBuilderAPI_Transformed;
    }
    if (mode == 2) {
        return BRepBuilderAPI_RoundCorner;
    }
    return BRepBuilderAPI_RightCorner;
}

TopoDS_Wire build_rounded_polyline_wire(const CPolyline& path) {
    if (path.GetPointCount() < 2) return {};
    if (path.IsClosed()) {
        return build_polyline_wire(path.GetRoundedPathPoints(0.08726646259971647), false);
    }
    const std::vector<CPoint3d>& points = path.GetPoints();
    BRepBuilderAPI_MakeWire wire;
    for (size_t edge_index = 0; edge_index + 1 < points.size(); ++edge_index) {
        CPolylineFilletGeometry start_fillet;
        CPolylineFilletGeometry end_fillet;
        const bool has_start_fillet = path.GetFilletGeometry(edge_index, start_fillet);
        const bool has_end_fillet = path.GetFilletGeometry(edge_index + 1, end_fillet);
        const CPoint3d& line_start = has_start_fillet ? start_fillet.tangent_next : points[edge_index];
        const CPoint3d& line_end = has_end_fillet ? end_fillet.tangent_previous : points[edge_index + 1];
        const CPoint3d line_delta = subtract(line_end, line_start);
        if (dot3(line_delta, line_delta) > kGeometryTolerance * kGeometryTolerance) {
            BRepBuilderAPI_MakeEdge line(gp_Pnt(line_start.x, line_start.y, line_start.z),
                                         gp_Pnt(line_end.x, line_end.y, line_end.z));
            if (!line.IsDone()) return {};
            wire.Add(line.Edge());
        }
        if (has_end_fillet) {
            const Vec3 center = {static_cast<float>(end_fillet.center.x), static_cast<float>(end_fillet.center.y), static_cast<float>(end_fillet.center.z)};
            const Vec3 start = {static_cast<float>(end_fillet.tangent_previous.x), static_cast<float>(end_fillet.tangent_previous.y), static_cast<float>(end_fillet.tangent_previous.z)};
            const Vec3 mid = center + rotate_around_axis(start - center, end_fillet.normal,
                                                         static_cast<float>(end_fillet.signed_angle * 0.5));
            GC_MakeArcOfCircle arc(gp_Pnt(end_fillet.tangent_previous.x, end_fillet.tangent_previous.y, end_fillet.tangent_previous.z),
                                   gp_Pnt(mid.x, mid.y, mid.z),
                                   gp_Pnt(end_fillet.tangent_next.x, end_fillet.tangent_next.y, end_fillet.tangent_next.z));
            if (!arc.IsDone()) return {};
            BRepBuilderAPI_MakeEdge arc_edge(arc.Value());
            if (!arc_edge.IsDone()) return {};
            wire.Add(arc_edge.Edge());
        }
    }
    return wire.IsDone() ? wire.Wire() : TopoDS_Wire{};
}

TopoDS_Shape build_physical_milling_envelope(
        const CSmartLine& section,
        const TopoDS_Wire& source_section_wire,
        const CSmartLine& guide,
        double delta_x,
        double delta_y,
        double angle_degrees) {
    if (!guide.IsClosed() || guide.GetNumLines() < 2) return {};

    std::vector<TopoDS_Shape> swept_pieces;
    std::vector<TopoDS_Shape> corner_pieces;
    swept_pieces.reserve(guide.GetNumLines());
    corner_pieces.reserve(guide.GetNumLines());
    const SketchCoordinateSystem& system = guide.GetCoordinateSystem();

    // Sweep every authored guide element. These pieces contain the exact
    // straight/arc motion of the router between vertices.
    for (size_t index = 0; index < guide.GetNumLines(); ++index) {
        const CLinkLine* line = guide.GetLine(index);
        if (!line) return {};
        CSmartLine segment("Milling path element");
        if (!segment.SetCoordinateSystem(
                system.origin, system.x_axis, system.normal)
            || !segment.AddLine(line->Clone(), false)) {
            return {};
        }
        TopoDS_Shape swept = BuildSweptSolidShape(
            section, segment, 0, delta_x, delta_y, angle_degrees);
        if (!swept.IsNull()) {
            swept_pieces.push_back(std::move(swept));
        }
    }

    CPoint3d axis_direction = system.normal;
    if (!normalize(axis_direction)) return {};
    for (size_t index = 0; index < guide.GetNumLines(); ++index) {
        const CLinkLine* previous = guide.GetLine(index);
        const CLinkLine* next = guide.GetLine(
            (index + 1) % guide.GetNumLines());
        if (!previous || !next) return {};
        CPoint3d incoming_local = previous->GetTangent(1.0);
        CPoint3d outgoing_local = next->GetTangent(0.0);
        if (!normalize(incoming_local) || !normalize(outgoing_local)) return {};
        const CPoint3d world_origin = guide.LocalToWorld({0.0, 0.0, 0.0});
        CPoint3d incoming = subtract(
            guide.LocalToWorld(incoming_local), world_origin);
        CPoint3d outgoing = subtract(
            guide.LocalToWorld(outgoing_local), world_origin);
        if (!normalize(incoming) || !normalize(outgoing)) return {};
        const double sine = dot3(axis_direction, cross(incoming, outgoing));
        const double cosine = std::clamp(dot3(incoming, outgoing), -1.0, 1.0);
        const double turn = std::atan2(sine, cosine);
        if (std::abs(turn) <= 1.0e-6) continue;

        // Position the cutter at the shared vertex with its tangent equal to
        // the incoming path tangent. Revolving that complete closed section
        // through the signed turn angle creates the real router corner
        // envelope, including its visible tool-radius surfaces.
        const CPoint3d local_vertex = previous->GetEnd();
        CSmartLine corner_frame("Milling corner frame");
        if (!corner_frame.SetCoordinateSystem(
                system.origin, system.x_axis, system.normal)
            || !corner_frame.Add(new CLinkLine(
                local_vertex, add(local_vertex, incoming_local)))) {
            return {};
        }
        const TopoDS_Wire placed_wire = place_section_at_guide_start(
            source_section_wire, section, corner_frame,
            delta_x, delta_y, angle_degrees);
        if (placed_wire.IsNull()) continue;
        BRepBuilderAPI_MakeFace face_builder(placed_wire, true);
        if (!face_builder.IsDone()) continue;
        const CPoint3d world_vertex = guide.LocalToWorld(local_vertex);
        try {
            // Swept consumes the complete closed cutter section. A 360-degree
            // Revolve, however, needs a meridian: only one half of that
            // section measured from the cutter axis. Split the placed face by
            // the plane through the cutter axis and incoming path tangent.
            CPoint3d radial = cross(axis_direction, incoming);
            if (!normalize(radial)) continue;
            BRepBuilderAPI_MakeFace splitter_face(
                gp_Pln(
                    gp_Pnt(world_vertex.x, world_vertex.y, world_vertex.z),
                    gp_Dir(radial.x, radial.y, radial.z)),
                -1.0e6, 1.0e6, -1.0e6, 1.0e6);
            if (!splitter_face.IsDone()) continue;
            BRepPrimAPI_MakeHalfSpace half_space(
                splitter_face.Face(),
                gp_Pnt(world_vertex.x + radial.x,
                       world_vertex.y + radial.y,
                       world_vertex.z + radial.z));
            half_space.Build();
            if (!half_space.IsDone()) continue;
            BRepAlgoAPI_Common half_profile_builder(
                face_builder.Face(), half_space.Solid());
            half_profile_builder.Build();
            if (!half_profile_builder.IsDone()) continue;
            // A section touching the splitting plane can occasionally yield
            // more than one face. Explorer order depends on orientation, so
            // choose the actual meridian by area instead of taking a random
            // first face (which produced a tiny corner projection).
            TopoDS_Face revolve_profile;
            double largest_area = 0.0;
            for (TopExp_Explorer half_face(
                     half_profile_builder.Shape(), TopAbs_FACE);
                 half_face.More(); half_face.Next()) {
                const TopoDS_Face candidate =
                    TopoDS::Face(half_face.Current());
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(candidate, properties);
                if (properties.Mass() > largest_area) {
                    largest_area = properties.Mass();
                    revolve_profile = candidate;
                }
            }
            if (revolve_profile.IsNull()) continue;

            // This is the rotating cutter at the corner, not a transition
            // patch between the two path elements. Revolve the cutter profile
            // through a complete turn around its own axis. The adjacent swept
            // elements enter this round body and their common envelope forms
            // the physical router radius visible on the facade.
            BRepPrimAPI_MakeRevol revolve(
                revolve_profile,
                gp_Ax1(gp_Pnt(world_vertex.x, world_vertex.y, world_vertex.z),
                       gp_Dir(axis_direction.x, axis_direction.y, axis_direction.z)),
                2.0 * kPi,
                Standard_True);
            revolve.Build();
            if (revolve.IsDone() && !revolve.Shape().IsNull()) {
                corner_pieces.push_back(revolve.Shape());
            }
        } catch (const Standard_Failure&) {
            // Keep the successfully swept path elements. A failed corner can
            // then be inspected independently instead of hiding the complete
            // milling operation behind the unchanged facade.
        }
    }

    if (corner_pieces.empty() && swept_pieces.empty()) return {};

    BRep_Builder builder;
    TopoDS_Compound result;
    builder.MakeCompound(result);
    // Boolean subtraction follows the Compound order. Remove the round
    // cutter positions first, while the panel is topologically simple; the
    // segment sweeps then enter those already-open round regions. Doing this
    // in the reverse order leaves coincident sharp edges at some vertices and
    // may cause Open Cascade to skip a corner cut.
    for (const TopoDS_Shape& piece : corner_pieces) builder.Add(result, piece);
    for (const TopoDS_Shape& piece : swept_pieces) builder.Add(result, piece);
    return result;
}
}

MillingCutterPlacement ResolveMillingCutterPlacement(
    const CSmartLine& cutter) {
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    for (const CPoint3d& world : cutter.GetProfilePointsWorld()) {
        const CPoint3d local = cutter.WorldToLocal(world);
        min_y = std::min(min_y, local.y);
        max_y = std::max(max_y, local.y);
    }
    if (min_y == std::numeric_limits<double>::max()) {
        return {};
    }

    MillingCutterPlacement placement;
    constexpr double orientation_tolerance = 1.0e-7;
    if (min_y >= -orientation_tolerance
        && max_y > orientation_tolerance) {
        // A physical cutter is authored with its rounded cutting tip at Y=0
        // and its shank toward +Y. The facade placement already maps +Y to
        // the outside of the panel, so no additional 180-degree rotation is
        // needed here. Advancing the tip is handled by the milling-depth
        // caller; rotating this profile would bury the shank in the facade.
        placement.tip_at_cutting_depth = true;
        return placement;
    }
    if (max_y <= orientation_tolerance
        && min_y < -orientation_tolerance) {
        // The same physical convention mirrored in catalog XY: the tip is
        // still at Y=0, but the shank/profile extends toward -Y. Rotate only
        // the working copy so the shank stays outside, then advance the tip
        // to Milling Depth exactly as for a +Y cutter.
        placement.angle_degrees = 180.0;
        placement.rotated_legacy_profile = true;
        placement.tip_at_cutting_depth = true;
        return placement;
    }

    // Profiles crossing both sides of their origin have no unambiguous tip
    // datum. Keep the compatibility heuristic for those older sketches.
    placement.rotated_legacy_profile = max_y > -min_y;
    placement.angle_degrees = placement.rotated_legacy_profile ? 180.0 : 0.0;
    // Put the narrow reference edge on the facade surface. For a natural
    // profile that is its highest Y; after rotating a legacy profile it is
    // the negated original minimum Y.
    const double oriented_surface_y = placement.rotated_legacy_profile
        ? -min_y : max_y;
    placement.delta_y = -oriented_surface_y;
    return placement;
}

bool ValidateMillingCutterCatalogProfile(
    const CSmartLine& cutter,
    std::string& error) {
    error.clear();
    if (!cutter.IsClosed()) {
        error = "Cutter catalog rule: the native Sketch must be closed.";
        return false;
    }

    TopoDS_Face profile_face;
    Vec3 profile_normal{};
    if (!BuildSketchProfileFace(cutter, profile_face, profile_normal)
        || profile_face.IsNull()) {
        error = "Cutter catalog rule: the Sketch must form one valid closed profile.";
        return false;
    }

    std::vector<CPoint3d> points;
    for (const CPoint3d& world : cutter.GetProfilePointsWorld()) {
        points.push_back(cutter.WorldToLocal(world));
    }
    if (points.size() < 4) {
        error = "Cutter catalog rule: the Sketch profile is empty or degenerate.";
        return false;
    }

    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    for (const CPoint3d& point : points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }
    const double size = std::max(max_x - min_x, max_y - min_y);
    if (size <= 1.0e-9) {
        error = "Cutter catalog rule: the Sketch profile is degenerate.";
        return false;
    }
    const double tolerance = std::max(1.0e-5, size * 1.0e-6);
    // Curve sampling is not bit-identical on every segment, but catalog
    // symmetry is still a machining datum, not a visual approximation.
    const double sampling_tolerance = std::max(1.0e-4, size * 1.0e-5);

    if (min_y < -tolerance || std::abs(min_y) > tolerance) {
        error = "Cutter catalog rule: put the cutting tip on Y=0 and keep the complete profile in Y>=0.";
        return false;
    }
    if (std::abs(min_x + max_x) > tolerance) {
        error = "Cutter catalog rule: center the cutter axis on X=0.";
        return false;
    }

    bool tip_reaches_axis = false;
    double tip_min_x = std::numeric_limits<double>::max();
    double tip_max_x = std::numeric_limits<double>::lowest();
    for (const CPoint3d& point : points) {
        if (point.y <= min_y + sampling_tolerance) {
            tip_min_x = std::min(tip_min_x, point.x);
            tip_max_x = std::max(tip_max_x, point.x);
        }
    }
    tip_reaches_axis = tip_min_x <= tolerance && tip_max_x >= -tolerance;
    if (!tip_reaches_axis) {
        error = "Cutter catalog rule: the cutting tip or edge at Y=0 must cross the X=0 axis.";
        return false;
    }

    for (const CPoint3d& point : points) {
        const bool mirrored = std::any_of(
            points.begin(), points.end(), [&point, sampling_tolerance](
                const CPoint3d& candidate) {
                return std::abs(candidate.x + point.x) <= sampling_tolerance
                    && std::abs(candidate.y - point.y) <= sampling_tolerance;
            });
        if (!mirrored) {
            error = "Cutter catalog rule: the profile must be symmetric about X=0.";
            return false;
        }
    }
    return true;
}

const CSmartLine* ResolveMillingCutterCatalogProfile(
    const CAlfaDoc& catalog_document,
    std::string& error) {
    error.clear();
    const CSmartLine* cutter_sketch = nullptr;
    size_t profile_object_count = 0;
    for (const auto& object : catalog_document.GetObjects()) {
        if (const auto* sketch =
                dynamic_cast<const CSmartLine*>(object.get())) {
            if (sketch->GetNumLines() == 0) {
                continue;
            }
            ++profile_object_count;
            cutter_sketch = sketch;
        } else if (const auto* polyline =
                       dynamic_cast<const CPolyline*>(object.get())) {
            if (!polyline->IsEmpty()) {
                ++profile_object_count;
            }
        }
    }
    if (profile_object_count != 1 || !cutter_sketch) {
        error =
            "Cutter catalog rule: keep exactly one non-empty native Sketch "
            "profile. Extra non-empty Sketch or Polyline objects are not "
            "allowed; empty placeholders are ignored.";
        return nullptr;
    }
    if (!ValidateMillingCutterCatalogProfile(*cutter_sketch, error)) {
        return nullptr;
    }
    return cutter_sketch;
}

TopoDS_Shape LimitMillingCutterToFacadeDepth(
    const TopoDS_Shape& cutter,
    const TopoDS_Shape& facade,
    double cutting_depth) {
    if (cutter.IsNull() || facade.IsNull() || cutting_depth <= 0.0) {
        return {};
    }
    Bnd_Box bounds;
    BRepBndLib::Add(facade, bounds);
    if (bounds.IsVoid()) return {};
    Standard_Real min_x = 0.0;
    Standard_Real min_y = 0.0;
    Standard_Real min_z = 0.0;
    Standard_Real max_x = 0.0;
    Standard_Real max_y = 0.0;
    Standard_Real max_z = 0.0;
    bounds.Get(min_x, min_y, min_z, max_x, max_y, max_z);
    const double facade_depth = std::max(0.0, max_y - min_y);
    // Decorative facade milling must leave a back skin. Apart from preventing
    // accidental through-cuts this avoids coincident rear faces in the BOP.
    const double safe_maximum_depth = std::max(0.1, facade_depth - 0.5);
    const double limited_depth = std::min(
        cutting_depth, safe_maximum_depth);
    if (limited_depth <= 0.0) return {};

    constexpr double front_overlap = 0.25;
    const double margin = std::max(max_x - min_x, max_z - min_z) + 1.0;
    BRepPrimAPI_MakeBox limiter(
        gp_Pnt(min_x - margin, min_y - front_overlap, min_z - margin),
        (max_x - min_x) + 2.0 * margin,
        limited_depth + front_overlap,
        (max_z - min_z) + 2.0 * margin);
    limiter.Build();
    if (!limiter.IsDone()) return {};
    BRep_Builder compound_builder;
    TopoDS_Compound limited_pieces;
    compound_builder.MakeCompound(limited_pieces);
    bool has_limited_piece = false;
    // The physical milling envelope is a compound of overlapping segment
    // sweeps and corner revolutions. Intersecting that self-overlapping
    // compound in one BOP can yield an empty result. Clip every solid first;
    // cut_milled_panel will subtract the resulting positions sequentially.
    for (TopExp_Explorer solid(cutter, TopAbs_SOLID);
         solid.More(); solid.Next()) {
        try {
            BRepAlgoAPI_Common common(solid.Current(), limiter.Shape());
            common.SetRunParallel(false);
            common.Build();
            if (!common.IsDone() || common.Shape().IsNull()) continue;
            for (TopExp_Explorer clipped(common.Shape(), TopAbs_SOLID);
                 clipped.More(); clipped.Next()) {
                compound_builder.Add(limited_pieces, clipped.Current());
                has_limited_piece = true;
            }
        } catch (const Standard_Failure&) {
            // One bad corner must not discard valid straight passes.
        }
    }
    return has_limited_piece ? limited_pieces : TopoDS_Shape{};
}

TopoDS_Shape BuildMillingSweepShape(const TopoDS_Wire& section_wire,
                                    const TopoDS_Wire& guide_wire,
                                    const Vec3& guide_normal,
                                    int transition_mode,
                                    bool require_evolved) {
    if (section_wire.IsNull() || guide_wire.IsNull()) {
        return {};
    }
    const auto valid_solid = [](const TopoDS_Shape& shape) {
        return !shape.IsNull()
            && TopExp_Explorer(shape, TopAbs_SOLID).More()
            && BRepCheck_Analyzer(shape).IsValid();
    };

    // Evolved is the closest OCCT equivalent of a planar router operation.
    // At a salient guide vertex GeomAbs_Arc revolves the complete cutter
    // section around an axis through that vertex and normal to the guide
    // plane. This preserves the physical cutter radius at the corner instead
    // of merely extending/intersecting adjacent swept pieces.
    struct EvolvedMode {
        Standard_Boolean axes_from_global;
        Standard_Boolean profile_on_spine;
        Standard_Boolean make_volume;
    };
    // Catalog profiles commonly touch the guide at their local origin, but
    // older profiles may merely cross it. Try both interpretations. Volume
    // mode is last because it is slower, but it removes self-intersections
    // created by a wide cutter around concave corners.
    if (require_evolved) {
        for (const EvolvedMode mode : {
                 EvolvedMode{Standard_False, Standard_True,  Standard_False},
                 EvolvedMode{Standard_False, Standard_False, Standard_False},
                 EvolvedMode{Standard_True,  Standard_False, Standard_False},
                 EvolvedMode{Standard_False, Standard_False, Standard_True}}) {
            try {
                BRepOffsetAPI_MakeEvolved evolved(
                    guide_wire,
                    section_wire,
                    GeomAbs_Arc,
                    mode.axes_from_global,
                    Standard_True,
                    mode.profile_on_spine,
                    1.0e-7,
                    mode.make_volume,
                    Standard_True);
                evolved.Build();
                if (evolved.IsDone() && valid_solid(evolved.Shape())) {
                    return evolved.Shape();
                }
            } catch (const Standard_Failure&) {
            }
        }
        return {};
    }

    const auto attempt = [&](BRepBuilderAPI_TransitionMode mode,
                             bool correct_section) -> TopoDS_Shape {
        try {
            BRepOffsetAPI_MakePipeShell sweep(guide_wire);
            sweep.SetMode(gp_Dir(
                guide_normal.x, guide_normal.y, guide_normal.z));
            sweep.SetTransitionMode(mode);
            sweep.Add(section_wire, false, correct_section);
            if (!sweep.IsReady()) return {};
            sweep.Build();
            if (!sweep.IsDone() || !sweep.MakeSolid()) return {};
            const TopoDS_Shape result = sweep.Shape();
            if (!valid_solid(result)) {
                return {};
            }
            return result;
        } catch (const Standard_Failure&) {
            return {};
        }
    };

    // Prefer the physically correct round transition. Some authored catalog
    // profiles are already placed exactly normal to the guide, and OCCT's
    // correction pass cannot close those at the seam. Retry without changing
    // their authored orientation before falling back to other corner rules.
    const BRepBuilderAPI_TransitionMode requested =
        transition_mode_from_index(transition_mode);
    for (const bool correction : {true, false}) {
        TopoDS_Shape result = attempt(requested, correction);
        if (!result.IsNull()) return result;
    }
    for (const BRepBuilderAPI_TransitionMode fallback : {
             BRepBuilderAPI_RightCorner,
             BRepBuilderAPI_Transformed}) {
        if (fallback == requested) continue;
        for (const bool correction : {true, false}) {
            TopoDS_Shape result = attempt(fallback, correction);
            if (!result.IsNull()) return result;
        }
    }
    return {};
}

bool BuildPlacedSweptSectionSketch(const CSmartLine& section,
                                   const CAlfaObject& guide,
                                   CSmartLine& placed_section,
                                   double delta_x,
                                   double delta_y,
                                   double angle_degrees) {
    CPoint3d target_origin;
    CPoint3d target_z;
    CPoint3d guide_normal;
    if (!initial_guide_frame(
            guide, target_origin, target_z, guide_normal)) {
        return false;
    }
    CPoint3d base_x = cross(guide_normal, target_z);
    if (!normalize(base_x)) {
        return false;
    }
    CPoint3d base_y = cross(target_z, base_x);
    if (!normalize(base_y)) {
        return false;
    }
    target_origin = add(target_origin,
        add(multiply(base_x, delta_x), multiply(base_y, delta_y)));
    const double angle = angle_degrees
        * 3.14159265358979323846 / 180.0;
    const CPoint3d target_x = add(
        multiply(base_x, std::cos(angle)),
        multiply(base_y, std::sin(angle)));
    placed_section = section.MakeCopy();
    return placed_section.SetCoordinateSystem(
        target_origin, target_x, target_z);
}

TopoDS_Shape BuildSweptSolidShape(const CSmartLine& section,
                                  const CAlfaObject& guide,
                                  int transition_mode,
                                  double delta_x,
                                  double delta_y,
                                  double angle_degrees,
                                  const std::vector<double>& width_scales,
                                  const std::vector<double>& height_scales,
                                  bool require_evolved) {
    if (!section.IsClosed()) {
        return {};
    }

    TopoDS_Face section_face;
    Vec3 section_normal{};
    if (!BuildSketchProfileFace(section, section_face, section_normal)) {
        return {};
    }
    const TopoDS_Wire source_section_wire = BRepTools::OuterWire(section_face);
    if (source_section_wire.IsNull()) {
        return {};
    }
    const TopoDS_Wire section_wire = place_section_at_guide_start(
        source_section_wire, section, guide, delta_x, delta_y, angle_degrees);
    if (section_wire.IsNull()) {
        return {};
    }

    TopoDS_Wire guide_wire;
    const CSmartLine* guide_sketch = dynamic_cast<const CSmartLine*>(&guide);
    if (const auto* spline = dynamic_cast<const CBSpline*>(&guide)) {
        guide_wire = build_bspline_wire(*spline);
    } else if (guide_sketch) {
        // A face's OuterWire may begin at a different vertex than sketch line
        // zero. The section is placed at line zero, so a closed catalog guide
        // must preserve the authored edge order as well.
        BuildSketchPathWire(*guide_sketch, guide_wire);
    }
    if (guide_wire.IsNull()) {
        return {};
    }

    if (require_evolved && guide_sketch
        && guide_sketch->IsClosed()
        && guide_sketch->GetNumLines() >= 2) {
        return build_physical_milling_envelope(
            section, source_section_wire, *guide_sketch,
            delta_x, delta_y, angle_degrees);
    }

    try {
        const bool variable_scale = !scale_graph_is_neutral(width_scales)
            || !scale_graph_is_neutral(height_scales);
        if (variable_scale) {
            // MakePipeShell is allowed to determine section locations on the
            // spine automatically.  With many nearby scaled sections that
            // matching can become non-monotonic and produces crossed faces.
            // ThruSections preserves insertion order, so graph stations always
            // run from the beginning of the guide to its end.
            BRepOffsetAPI_ThruSections loft(true, false, 1.0e-6);
            loft.CheckCompatibility(true);
            constexpr int section_count = 25;
            for (int index = 0; index < section_count; ++index) {
                const double parameter = static_cast<double>(index) / (section_count - 1);
                const TopoDS_Wire scaled_section = place_scaled_section(
                    source_section_wire, section, guide, parameter,
                    evaluate_scale_graph(width_scales, parameter),
                    evaluate_scale_graph(height_scales, parameter),
                    delta_x, delta_y, angle_degrees);
                if (scaled_section.IsNull()) return {};
                loft.AddWire(scaled_section);
            }
            loft.Build();
            return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
        }

        Vec3 guide_normal{};
        if (guide_sketch) {
            const SketchCoordinateSystem& system = guide_sketch->GetCoordinateSystem();
            guide_normal = Vec3{
                static_cast<float>(system.normal.x),
                static_cast<float>(system.normal.y),
                static_cast<float>(system.normal.z)};
        } else {
            // Keep a stable frame on a free 3D B-Spline.  Corrected Frenet can
            // flip at an inflection (zero curvature), producing a visible
            // crease/twist in wide profiles.  Use the same binormal that was
            // used to place the section at the guide start.
            CPoint3d guide_origin;
            CPoint3d guide_tangent;
            CPoint3d guide_binormal;
            if (!initial_guide_frame(guide, guide_origin, guide_tangent, guide_binormal)) {
                return {};
            }
            guide_normal = Vec3{
                static_cast<float>(guide_binormal.x),
                static_cast<float>(guide_binormal.y),
                static_cast<float>(guide_binormal.z)};
        }
        return BuildMillingSweepShape(
            section_wire, guide_wire, guide_normal, transition_mode,
            require_evolved && !guide_sketch);
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Shape BuildFrameSolidShape(const CSmartLine& profile,
                                  double width,
                                  double height) {
    if (!profile.IsClosed()
        || width <= kGeometryTolerance
        || height <= kGeometryTolerance) {
        return {};
    }

    TopoDS_Face profile_face;
    Vec3 profile_normal{};
    if (!BuildSketchProfileFace(profile, profile_face, profile_normal)) {
        return {};
    }
    const TopoDS_Wire profile_wire = BRepTools::OuterWire(profile_face);
    if (profile_wire.IsNull()) {
        return {};
    }

    const SketchCoordinateSystem& system = profile.GetCoordinateSystem();
    CPoint3d width_axis = system.normal;
    if (!normalize(width_axis)) {
        return {};
    }

    // Frame height follows the world Y direction. Project it onto the plane
    // perpendicular to the profile normal so an YZ profile produces an XY frame.
    const CPoint3d world_y(0.0, 1.0, 0.0);
    CPoint3d height_axis = subtract(
        world_y, multiply(width_axis, dot3(world_y, width_axis)));
    if (!normalize(height_axis)) {
        const CPoint3d world_z(0.0, 0.0, 1.0);
        height_axis = subtract(
            world_z, multiply(width_axis, dot3(world_z, width_axis)));
        if (!normalize(height_axis)) {
            return {};
        }
    }

    const CPoint3d p0 = system.origin;
    const CPoint3d p1 = add(p0, multiply(width_axis, width));
    const CPoint3d p2 = add(p1, multiply(height_axis, height));
    const CPoint3d p3 = add(p0, multiply(height_axis, height));
    const TopoDS_Wire frame_wire = build_polyline_wire({p0, p1, p2, p3}, true);
    if (frame_wire.IsNull()) {
        return {};
    }

    CPoint3d frame_normal = cross(width_axis, height_axis);
    if (!normalize(frame_normal)) {
        return {};
    }

    try {
        BRepOffsetAPI_MakePipeShell sweep(frame_wire);
        sweep.SetMode(gp_Dir(frame_normal.x, frame_normal.y, frame_normal.z));
        sweep.SetTransitionMode(BRepBuilderAPI_RightCorner);
        sweep.Add(profile_wire, false, false);
        if (!sweep.IsReady()) {
            return {};
        }
        sweep.Build();
        if (!sweep.IsDone() || !sweep.MakeSolid()) {
            return {};
        }
        return sweep.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Shape BuildWireSolidShape(const CPolyline& path, double radius) {
    if (radius <= kGeometryTolerance || path.GetPointCount() < 2) return {};
    const TopoDS_Wire spine = build_rounded_polyline_wire(path);
    if (spine.IsNull()) return {};
    const std::vector<CPoint3d> rounded = path.GetRoundedPathPoints();
    if (rounded.size() < 2) return {};
    CPoint3d tangent = subtract(rounded[1], rounded[0]);
    if (!normalize(tangent)) return {};
    try {
        const gp_Ax2 frame(gp_Pnt(rounded[0].x, rounded[0].y, rounded[0].z),
                           gp_Dir(tangent.x, tangent.y, tangent.z));
        BRepBuilderAPI_MakeEdge circle_edge(gp_Circ(frame, radius));
        if (!circle_edge.IsDone()) return {};
        BRepBuilderAPI_MakeWire circle_wire(circle_edge.Edge());
        if (!circle_wire.IsDone()) return {};
        BRepOffsetAPI_MakePipeShell sweep(spine);
        sweep.SetMode(false);
        sweep.SetTransitionMode(BRepBuilderAPI_RoundCorner);
        sweep.Add(circle_wire.Wire(), false, false);
        if (!sweep.IsReady()) return {};
        sweep.Build();
        if (!sweep.IsDone() || !sweep.MakeSolid()) return {};
        return sweep.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Shape BuildWireSolidShape(const CAlfaObject& path, double radius) {
    if (const auto* polyline = dynamic_cast<const CPolyline*>(&path)) {
        return BuildWireSolidShape(*polyline, radius);
    }
    if (radius <= kGeometryTolerance) return {};

    TopoDS_Wire spine;
    std::vector<CPoint3d> path_points;
    if (const auto* sketch = dynamic_cast<const CSmartLine*>(&path)) {
        if (sketch->IsClosed() || !BuildOpenSketchProfileWire(*sketch, spine)) return {};
        path_points = sketch->GetProfilePointsWorld();
    } else if (const auto* spline = dynamic_cast<const CBSpline*>(&path)) {
        if (spline->IsClosed() || spline->GetPointCount() < 2) return {};
        spine = build_bspline_wire(*spline);
        constexpr int tangent_samples = 100;
        path_points.reserve(tangent_samples + 1);
        for (int i = 0; i <= tangent_samples; ++i) {
            path_points.push_back(spline->Evaluate(static_cast<float>(i) / tangent_samples));
        }
    } else {
        return {};
    }
    if (spine.IsNull() || path_points.size() < 2) return {};

    size_t next_index = 1;
    CPoint3d tangent;
    while (next_index < path_points.size()) {
        tangent = subtract(path_points[next_index], path_points.front());
        if (normalize(tangent)) break;
        ++next_index;
    }
    if (next_index >= path_points.size()) return {};

    try {
        const CPoint3d& origin = path_points.front();
        const gp_Ax2 frame(gp_Pnt(origin.x, origin.y, origin.z),
                           gp_Dir(tangent.x, tangent.y, tangent.z));
        BRepBuilderAPI_MakeEdge circle_edge(gp_Circ(frame, radius));
        if (!circle_edge.IsDone()) return {};
        BRepBuilderAPI_MakeWire circle_wire(circle_edge.Edge());
        if (!circle_wire.IsDone()) return {};
        BRepOffsetAPI_MakePipeShell sweep(spine);
        sweep.SetMode(false);
        sweep.SetTransitionMode(BRepBuilderAPI_RoundCorner);
        sweep.Add(circle_wire.Wire(), false, false);
        if (!sweep.IsReady()) return {};
        sweep.Build();
        if (!sweep.IsDone() || !sweep.MakeSolid()) return {};
        return sweep.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}
