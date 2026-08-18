#include "SheetBendShapeBuilder.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {
constexpr double kPi = 3.1415926535897932384626433832795;

struct V3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 mul(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
double length(V3 a) { return std::sqrt(dot(a, a)); }
V3 normalized(V3 a) {
    const double value = length(a);
    return value > 1.0e-15 ? mul(a, 1.0 / value) : V3{};
}
gp_Pnt point(V3 value) { return {value.x, value.y, value.z}; }
gp_Vec vector(V3 value) { return {value.x, value.y, value.z}; }

V3 rotate_vector(V3 value, V3 axis, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return add(add(mul(value, c), mul(cross(axis, value), s)),
               mul(axis, dot(axis, value) * (1.0 - c)));
}

TopoDS_Shape make_prism_box(V3 origin,
                            V3 axis_x,
                            V3 axis_y,
                            double x_length,
                            double y_length,
                            V3 extrusion) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(point(origin));
    polygon.Add(point(add(origin, mul(axis_x, x_length))));
    polygon.Add(point(add(add(origin, mul(axis_x, x_length)),
                          mul(axis_y, y_length))));
    polygon.Add(point(add(origin, mul(axis_y, y_length))));
    polygon.Close();
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(polygon.Wire());
    return BRepPrimAPI_MakePrism(face, vector(extrusion), true).Shape();
}

bool common_shape(const TopoDS_Shape& left,
                  const TopoDS_Shape& right,
                  TopoDS_Shape& result) {
    BRepAlgoAPI_Common operation(left, right);
    operation.SetFuzzyValue(1.0e-7);
    operation.Build();
    if (!operation.IsDone() || operation.Shape().IsNull()) return false;
    result = operation.Shape();
    return true;
}

bool fuse_shape(const TopoDS_Shape& left,
                const TopoDS_Shape& right,
                TopoDS_Shape& result) {
    BRepAlgoAPI_Fuse operation(left, right);
    operation.SetFuzzyValue(1.0e-7);
    operation.Build();
    if (!operation.IsDone() || operation.Shape().IsNull()) return false;
    result = operation.Shape();
    return true;
}

std::vector<std::pair<double, double>> material_intervals_on_line(
        const TopoDS_Shape& source,
        V3 line_start,
        V3 axis,
        double line_length) {
    constexpr double tolerance = 1.0e-6;
    std::vector<double> boundaries{0.0, line_length};
    IntCurvesFace_ShapeIntersector intersector;
    intersector.Load(source, tolerance);
    intersector.Perform(
        gp_Lin(point(line_start), gp_Dir(axis.x, axis.y, axis.z)),
        -tolerance, line_length + tolerance);
    if (intersector.IsDone()) {
        for (int index = 1; index <= intersector.NbPnt(); ++index) {
            const double parameter = intersector.WParameter(index);
            if (parameter >= -tolerance
                && parameter <= line_length + tolerance) {
                boundaries.push_back(std::clamp(parameter, 0.0, line_length));
            }
        }
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end(),
        [](double left, double right) {
            return std::abs(left - right) <= tolerance;
        }), boundaries.end());

    BRepClass3d_SolidClassifier classifier(source);
    std::vector<std::pair<double, double>> intervals;
    for (size_t index = 1; index < boundaries.size(); ++index) {
        const double first = boundaries[index - 1];
        const double last = boundaries[index];
        if (last - first <= tolerance) continue;
        const double middle = (first + last) * 0.5;
        classifier.Perform(point(add(line_start, mul(axis, middle))), tolerance);
        if (classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON) {
            if (!intervals.empty()
                && std::abs(intervals.back().second - first) <= tolerance) {
                intervals.back().second = last;
            } else {
                intervals.emplace_back(first, last);
            }
        }
    }
    return intervals;
}
}  // namespace

bool BuildSheetBendShape(const TopoDS_Shape& source,
                         const SheetBendParameters& parameters,
                         TopoDS_Shape& result,
                         std::string& error_message) {
    result.Nullify();
    error_message.clear();
    if (source.IsNull()) {
        error_message = "The selected solid has no shape.";
        return false;
    }
    if (parameters.inner_radius <= 0.0) {
        error_message = "The bend radius must be greater than zero.";
        return false;
    }
    if (std::abs(parameters.angle_degrees) < 0.01
        || std::abs(parameters.angle_degrees) >= 179.0) {
        error_message = "The bend angle must be between 0 and 179 degrees.";
        return false;
    }

    const V3 line_start{parameters.line_start_x, parameters.line_start_y,
                        parameters.line_start_z};
    const V3 line_end{parameters.line_end_x, parameters.line_end_y,
                      parameters.line_end_z};
    const double line_length = length(sub(line_end, line_start));
    if (line_length <= 1.0e-7) {
        error_message = "The bend line is too short.";
        return false;
    }
    const V3 axis = normalized(sub(line_end, line_start));

    // Use the planar sheet face nearest to the supplied line. For equal
    // planes, prefer the largest face to avoid selecting a small cutout face.
    bool found_plane = false;
    V3 plane_origin;
    V3 normal;
    double best_distance = std::numeric_limits<double>::max();
    double best_area = 0.0;
    for (TopExp_Explorer explorer(source, TopAbs_FACE); explorer.More();
         explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face, true);
        if (surface.GetType() != GeomAbs_Plane) continue;
        const gp_Pln plane = surface.Plane();
        V3 candidate_origin{plane.Location().X(), plane.Location().Y(),
                            plane.Location().Z()};
        V3 candidate_normal{plane.Axis().Direction().X(),
                            plane.Axis().Direction().Y(),
                            plane.Axis().Direction().Z()};
        if (face.Orientation() == TopAbs_REVERSED) {
            candidate_normal = mul(candidate_normal, -1.0);
        }
        const double distance = std::abs(dot(sub(line_start, candidate_origin),
                                             candidate_normal))
            + std::abs(dot(sub(line_end, candidate_origin), candidate_normal));
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        const double area = properties.Mass();
        if (!found_plane || distance < best_distance - 1.0e-7
            || (std::abs(distance - best_distance) <= 1.0e-7
                && area > best_area)) {
            found_plane = true;
            plane_origin = candidate_origin;
            normal = normalized(candidate_normal);
            best_distance = distance;
            best_area = area;
        }
    }
    if (!found_plane) {
        error_message = "A planar sheet face could not be detected.";
        return false;
    }
    if (std::abs(dot(axis, normal)) > 1.0e-4) {
        error_message = "The bend line must lie in the sheet plane.";
        return false;
    }

    double minimum_offset = std::numeric_limits<double>::max();
    double maximum_offset = -std::numeric_limits<double>::max();
    for (TopExp_Explorer explorer(source, TopAbs_VERTEX); explorer.More();
         explorer.Next()) {
        const gp_Pnt vertex = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
        const V3 value{vertex.X(), vertex.Y(), vertex.Z()};
        const double offset = dot(sub(value, plane_origin), normal);
        minimum_offset = std::min(minimum_offset, offset);
        maximum_offset = std::max(maximum_offset, offset);
    }
    const double thickness = maximum_offset - minimum_offset;
    if (!std::isfinite(thickness) || thickness <= 1.0e-7) {
        error_message = "The sheet thickness could not be detected.";
        return false;
    }
    const double line_plane_distance = std::max(
        std::abs(dot(sub(line_start, plane_origin), normal)),
        std::abs(dot(sub(line_end, plane_origin), normal)));
    if (line_plane_distance > std::max(0.02, thickness * 0.1)) {
        error_message = "The bend line must be placed on a planar sheet face.";
        return false;
    }

    Bnd_Box bounds;
    BRepBndLib::Add(source, bounds);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const double diagonal = std::sqrt((xmax - xmin) * (xmax - xmin)
                                    + (ymax - ymin) * (ymax - ymin)
                                    + (zmax - zmin) * (zmax - zmin));
    if (thickness > diagonal * 0.25) {
        error_message = "The selected body does not look like a thin sheet.";
        return false;
    }

    const V3 moving_direction = normalized(cross(axis, normal));
    const double neutral_offset = (minimum_offset + maximum_offset) * 0.5;
    const V3 neutral_start = add(line_start, mul(normal,
        neutral_offset - dot(sub(line_start, plane_origin), normal)));
    const double neutral_radius = parameters.inner_radius + thickness * 0.5;
    const double signed_angle = (parameters.clockwise ? -1.0 : 1.0)
        * std::abs(parameters.angle_degrees) * kPi / 180.0;
    const double rotation_sign = signed_angle < 0.0 ? -1.0 : 1.0;
    const double allowance = std::abs(signed_angle) * neutral_radius;
    const double extent = std::max(diagonal * 4.0,
        line_length + allowance + neutral_radius * 4.0 + 10.0);

    // Retain the fixed half and the moving half after removing the neutral
    // bend allowance from the original flat blank.
    const TopoDS_Shape fixed_clip = make_prism_box(
        sub(sub(sub(neutral_start, mul(axis, extent)),
                mul(moving_direction, extent)), mul(normal, extent)),
        axis, moving_direction, extent * 2.0, extent,
        mul(normal, extent * 2.0));
    const TopoDS_Shape moving_clip = make_prism_box(
        sub(add(sub(neutral_start, mul(axis, extent)),
                mul(moving_direction, allowance)), mul(normal, extent)),
        axis, moving_direction, extent * 2.0, extent,
        mul(normal, extent * 2.0));
    TopoDS_Shape fixed_part;
    TopoDS_Shape moving_part;
    if (!common_shape(source, fixed_clip, fixed_part)
        || !common_shape(source, moving_clip, moving_part)) {
        error_message = "The sheet could not be split by the bend line.";
        return false;
    }

    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(point(neutral_start),
                                gp_Dir(axis.x, axis.y, axis.z)), signed_angle);
    TopoDS_Shape transformed_moving =
        BRepBuilderAPI_Transform(moving_part, rotation, true).Shape();
    const V3 rotated_moving = rotate_vector(moving_direction, axis, signed_angle);
    const V3 radial_start = mul(normal, rotation_sign);
    const V3 radial_end_at_neutral =
        rotate_vector(radial_start, axis, signed_angle);
    const V3 arc_end = mul(
        sub(radial_end_at_neutral, radial_start), neutral_radius);
    const V3 translation_value = sub(arc_end, mul(rotated_moving, allowance));
    gp_Trsf translation;
    translation.SetTranslation(vector(translation_value));
    transformed_moving =
        BRepBuilderAPI_Transform(transformed_moving, translation, true).Shape();

    // Build one exact annular-sector sketch from two analytic circular arcs
    // and two radial lines. Extruding this face creates true cylindrical
    // surfaces instead of a faceted approximation.
    const V3 center = sub(
        neutral_start, mul(radial_start, neutral_radius));
    const V3 radial_middle =
        rotate_vector(radial_start, axis, signed_angle * 0.5);
    const V3 radial_end =
        rotate_vector(radial_start, axis, signed_angle);
    const double outer_radius = parameters.inner_radius + thickness;
    const gp_Pnt inner_start = point(add(center,
        mul(radial_start, parameters.inner_radius)));
    const gp_Pnt inner_middle = point(add(center,
        mul(radial_middle, parameters.inner_radius)));
    const gp_Pnt inner_end = point(add(center,
        mul(radial_end, parameters.inner_radius)));
    const gp_Pnt outer_start = point(add(center,
        mul(radial_start, outer_radius)));
    const gp_Pnt outer_middle = point(add(center,
        mul(radial_middle, outer_radius)));
    const gp_Pnt outer_end = point(add(center,
        mul(radial_end, outer_radius)));

    GC_MakeArcOfCircle make_inner_arc(inner_start, inner_middle, inner_end);
    GC_MakeArcOfCircle make_outer_arc(outer_end, outer_middle, outer_start);
    if (!make_inner_arc.IsDone() || !make_outer_arc.IsDone()) {
        error_message = "The analytic bend arcs could not be constructed.";
        return false;
    }
    BRepBuilderAPI_MakeWire sector_wire;
    sector_wire.Add(BRepBuilderAPI_MakeEdge(make_inner_arc.Value()).Edge());
    sector_wire.Add(BRepBuilderAPI_MakeEdge(inner_end, outer_end).Edge());
    sector_wire.Add(BRepBuilderAPI_MakeEdge(make_outer_arc.Value()).Edge());
    sector_wire.Add(BRepBuilderAPI_MakeEdge(outer_start, inner_start).Edge());
    if (!sector_wire.IsDone()) {
        error_message = "The analytic bend sketch could not be closed.";
        return false;
    }
    const TopoDS_Face sector_face =
        BRepBuilderAPI_MakeFace(sector_wire.Wire()).Face();
    const auto material_intervals = material_intervals_on_line(
        source, neutral_start, axis, line_length);
    if (material_intervals.empty()) {
        error_message = "The bend line does not cross the sheet material.";
        return false;
    }

    TopoDS_Shape bend_part;
    for (const auto& interval : material_intervals) {
        gp_Trsf interval_translation;
        interval_translation.SetTranslation(
            vector(mul(axis, interval.first)));
        const TopoDS_Face interval_face = TopoDS::Face(
            BRepBuilderAPI_Transform(
                sector_face, interval_translation, true).Shape());
        const TopoDS_Shape interval_part = BRepPrimAPI_MakePrism(
            interval_face,
            vector(mul(axis, interval.second - interval.first)),
            true).Shape();
        if (bend_part.IsNull()) {
            bend_part = interval_part;
        } else {
            TopoDS_Shape joined_intervals;
            if (!fuse_shape(bend_part, interval_part, joined_intervals)) {
                error_message = "The local bend sections could not be joined.";
                return false;
            }
            bend_part = joined_intervals;
        }
    }

    TopoDS_Shape combined;
    if (!fuse_shape(fixed_part, bend_part, combined)) {
        error_message = "The bent sheet parts could not be joined.";
        return false;
    }
    if (!fuse_shape(combined, transformed_moving, result)) {
        error_message = "The bent sheet parts could not be joined.";
        return false;
    }
    if (!BRepCheck_Analyzer(result).IsValid()) {
        error_message = "The bend produced an invalid solid. Check the line and radius.";
        result.Nullify();
        return false;
    }
    return true;
}
