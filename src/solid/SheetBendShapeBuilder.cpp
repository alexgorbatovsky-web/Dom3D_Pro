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
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
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

bool ResolveSheetBendFrame(const TopoDS_Shape& source,
                         const SheetBendParameters& parameters,
                         SheetBendFrame& frame,
                         std::string& error_message) try {
    error_message.clear();
    if (source.IsNull()) {
        error_message = "The selected solid has no shape.";
        return false;
    }
    if (!std::isfinite(parameters.inner_radius) || parameters.inner_radius <= 0.0) {
        error_message = "The bend radius must be greater than zero.";
        return false;
    }
    if (!std::isfinite(parameters.angle_degrees) || std::abs(parameters.angle_degrees) < 0.01
        || std::abs(parameters.angle_degrees) >= 179.0) {
        error_message = "The bend angle must be between 0 and 179 degrees.";
        return false;
    }

    const V3 line_start{parameters.line_start_x, parameters.line_start_y,
                        parameters.line_start_z};
    const V3 line_end{parameters.line_end_x, parameters.line_end_y,
                      parameters.line_end_z};
    const double line_length = length(sub(line_end, line_start));
    if (!std::isfinite(line_length) || line_length <= 1.0e-7) {
        error_message = "The bend line is too short.";
        return false;
    }
    const V3 axis = normalized(sub(line_end, line_start));

    Bnd_Box bounds;
    BRepBndLib::Add(source, bounds);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const double diagonal = std::sqrt((xmax - xmin) * (xmax - xmin)
                                    + (ymax - ymin) * (ymax - ymin)
                                    + (zmax - zmin) * (zmax - zmin));

    // A nearby infinite plane can belong to a distant bent flange. Only
    // consider faces whose trimmed interior contains the projected bend line
    // and whose inward material thickness can be measured there.
    bool found_plane = false;
    V3 plane_origin;
    V3 normal;
    double best_distance = std::numeric_limits<double>::max();
    double best_area = 0.0;
    double thickness = 0.0;
    bool varying_thickness = false;
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
        if (std::abs(dot(axis, candidate_normal)) > 1.e-4) continue;
        const double distance = std::abs(dot(sub(line_start, candidate_origin),
                                             candidate_normal))
            + std::abs(dot(sub(line_end, candidate_origin), candidate_normal));
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        const double area = properties.Mass();
        if (found_plane && (distance > best_distance + 1.e-7
            || (std::abs(distance - best_distance) <= 1.e-7 && area <= best_area))) continue;

        double candidate_thickness = 0.0;
        bool varies = false;
        for (double fraction : {0.5, 0.25, 0.75, 0.125, 0.875}) {
            V3 sample = add(line_start, mul(axis, line_length * fraction));
            sample = sub(sample, mul(candidate_normal,
                dot(sub(sample, candidate_origin), candidate_normal)));
            BRepClass_FaceClassifier on_face(face, point(sample), 1.e-6);
            if (on_face.State() != TopAbs_IN) continue;
            const double reach = diagonal + 1.0;
            const auto intervals = material_intervals_on_line(source,
                sub(sample, mul(candidate_normal, reach)), candidate_normal, 2.0 * reach);
            for (const auto& interval : intervals) {
                if (std::abs(interval.second - reach) > 1.e-5) continue;
                const double measured = interval.second - interval.first;
                if (measured <= 1.e-7) continue;
                if (candidate_thickness > 0.0
                    && std::abs(candidate_thickness - measured) > std::max(1.e-5, candidate_thickness * 1.e-4)) {
                    varies = true;
                    break;
                }
                candidate_thickness = measured;
            }
            if (varies) break;
        }
        varying_thickness = varying_thickness || varies;
        if (varies || candidate_thickness <= 1.e-7 || candidate_thickness > diagonal * 0.25) continue;
        {
            thickness = candidate_thickness;
            found_plane = true;
            plane_origin = candidate_origin;
            normal = normalized(candidate_normal);
            best_distance = distance;
            best_area = area;
        }
    }
    if (!found_plane) {
        error_message = varying_thickness
            ? "The thickness varies along the bend line. Choose a flat part of the sheet."
            : "The projected bend line does not cross the interior of a planar sheet face.";
        return false;
    }
    if (std::abs(dot(axis, normal)) > 1.0e-4) {
        error_message = "The bend line must lie in the sheet plane.";
        return false;
    }

    const double minimum_offset = -thickness;
    const double maximum_offset = 0.0;
    // A directed construction line may be drawn on the current work/view
    // plane above the sheet. Project it onto the nearest parallel sheet face;
    // the trimmed-face and local material checks above verify the projection.

    if (thickness > diagonal * 0.25) {
        error_message = "The selected body does not look like a thin sheet.";
        return false;
    }

    const double neutral_offset = (minimum_offset + maximum_offset) * 0.5;
    const V3 neutral_start = add(line_start, mul(normal,
        neutral_offset - dot(sub(line_start, plane_origin), normal)));
    frame.origin = gp_Pnt(neutral_start.x, neutral_start.y, neutral_start.z);
    frame.axis = gp_Dir(axis.x, axis.y, axis.z);
    frame.normal = gp_Dir(normal.x, normal.y, normal.z);
    frame.thickness = thickness;
    frame.line_length = line_length;
    frame.diagonal = diagonal;
    return true;
} catch (const Standard_Failure&) {
    error_message = "Could not detect the flat sheet at the bend line.";
    return false;
}

bool BuildSheetBendShape(const TopoDS_Shape& source,
                         const SheetBendParameters& parameters,
                         TopoDS_Shape& result,
                         std::string& error_message) try {
    result.Nullify();
    SheetBendFrame frame;
    if (!ResolveSheetBendFrame(source, parameters, frame, error_message)) return false;
    const V3 axis{frame.axis.X(), frame.axis.Y(), frame.axis.Z()};
    const V3 normal{frame.normal.X(), frame.normal.Y(), frame.normal.Z()};
    const V3 neutral_start{frame.origin.X(), frame.origin.Y(), frame.origin.Z()};
    const double thickness = frame.thickness, diagonal = frame.diagonal, line_length = frame.line_length;
    const double side_sign = parameters.reverse_side ? -1.0 : 1.0;
    const V3 moving_direction = mul(normalized(cross(axis, normal)), side_sign);
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
    const V3 radial_start = mul(normal, rotation_sign * side_sign);
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
    int solid_count = 0;
    for (TopExp_Explorer solid(result, TopAbs_SOLID); solid.More(); solid.Next()) ++solid_count;
    if (solid_count != 1) {
        error_message = "The bend would leave disconnected parts. Check the moving side and bend allowance.";
        result.Nullify();
        return false;
    }
    GProp_GProps source_properties, result_properties;
    BRepGProp::VolumeProperties(source, source_properties);
    BRepGProp::VolumeProperties(result, result_properties);
    const double source_volume = std::abs(source_properties.Mass());
    if (std::abs(std::abs(result_properties.Mass()) - source_volume)
        > std::max(1.e-6, source_volume * 1.e-5)) {
        error_message = "The bend allowance does not fit this flange without adding or removing material. Reduce the radius or move the line.";
        result.Nullify();
        return false;
    }
    return true;
} catch (const Standard_Failure& failure) {
    result.Nullify();
    const char* message = failure.GetMessageString();
    error_message = std::string("The sheet bend could not be constructed: ")
        + (message ? message : "CAD geometry error");
    return false;
}
