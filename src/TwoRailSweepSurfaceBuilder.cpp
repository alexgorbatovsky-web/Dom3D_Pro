#include "TwoRailSweepSurfaceBuilder.h"

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
#ifdef XtPointer
#undef XtPointer
#endif

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_FindPlane.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Mat.hxx>
#include <gp_XYZ.hxx>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPointTolerance = 1.0e-8;

double squared_distance(const CPoint3d& a, const CPoint3d& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

std::vector<CPoint3d> clean_points(const SweepCurveSamples& samples) {
    std::vector<CPoint3d> result;
    result.reserve(samples.points.size());
    for (const CPoint3d& point : samples.points) {
        if (result.empty() || squared_distance(result.back(), point) > kPointTolerance * kPointTolerance) {
            result.push_back(point);
        }
    }
    if (samples.closed && result.size() > 2) {
        double diagonal_squared = 0.0;
        for (const CPoint3d& point : result) {
            diagonal_squared = std::max(diagonal_squared, squared_distance(result.front(), point));
        }
        const double closure_tolerance = std::max(kPointTolerance, std::sqrt(diagonal_squared) * 1.0e-5);
        if (squared_distance(result.front(), result.back()) <= closure_tolerance * closure_tolerance) {
            result.pop_back();
        }
    }
    return result;
}

bool planarize_profile(const SweepCurveSamples& source,
                       SweepCurveSamples& result,
                       CPoint3d& plane_normal) {
    result = source;
    result.points = clean_points(source);
    if (result.points.size() < 3) {
        return result.points.size() >= 2;
    }

    CPoint3d center{};
    for (const CPoint3d& point : result.points) {
        center.x += point.x;
        center.y += point.y;
        center.z += point.z;
    }
    const double inverse_count = 1.0 / static_cast<double>(result.points.size());
    center.x *= inverse_count;
    center.y *= inverse_count;
    center.z *= inverse_count;

    CPoint3d normal{};
    double best_length_squared = 0.0;
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        const CPoint3d a(result.points[i].x - center.x,
                         result.points[i].y - center.y,
                         result.points[i].z - center.z);
        for (std::size_t j = i + 1; j < result.points.size(); ++j) {
            const CPoint3d b(result.points[j].x - center.x,
                             result.points[j].y - center.y,
                             result.points[j].z - center.z);
            const CPoint3d candidate(a.y * b.z - a.z * b.y,
                                     a.z * b.x - a.x * b.z,
                                     a.x * b.y - a.y * b.x);
            const double length_squared = candidate.x * candidate.x
                + candidate.y * candidate.y + candidate.z * candidate.z;
            if (length_squared > best_length_squared) {
                best_length_squared = length_squared;
                normal = candidate;
            }
        }
    }
    if (best_length_squared <= kPointTolerance * kPointTolerance) {
        return false;
    }
    const double inverse_normal_length = 1.0 / std::sqrt(best_length_squared);
    normal.x *= inverse_normal_length;
    normal.y *= inverse_normal_length;
    normal.z *= inverse_normal_length;
    plane_normal = normal;

    CPoint3d minimum = result.points.front();
    CPoint3d maximum = result.points.front();
    double maximum_deviation = 0.0;
    for (const CPoint3d& point : result.points) {
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
        const double deviation = std::abs((point.x - center.x) * normal.x
            + (point.y - center.y) * normal.y + (point.z - center.z) * normal.z);
        maximum_deviation = std::max(maximum_deviation, deviation);
    }
    const double dx = maximum.x - minimum.x;
    const double dy = maximum.y - minimum.y;
    const double dz = maximum.z - minimum.z;
    const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (maximum_deviation > std::max(1.0e-5, diagonal * 0.01)) {
        return false;
    }

    for (CPoint3d& point : result.points) {
        const double deviation = (point.x - center.x) * normal.x
            + (point.y - center.y) * normal.y + (point.z - center.z) * normal.z;
        point.x -= deviation * normal.x;
        point.y -= deviation * normal.y;
        point.z -= deviation * normal.z;
    }
    return true;
}

CPoint3d add(const CPoint3d& a, const CPoint3d& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

CPoint3d subtract(const CPoint3d& a, const CPoint3d& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

CPoint3d multiply(const CPoint3d& point, double factor) {
    return {point.x * factor, point.y * factor, point.z * factor};
}

double dot3(const CPoint3d& a, const CPoint3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

CPoint3d cross(const CPoint3d& a, const CPoint3d& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

bool normalize(CPoint3d& point) {
    const double length = std::sqrt(dot3(point, point));
    if (length <= kPointTolerance) {
        return false;
    }
    point = multiply(point, 1.0 / length);
    return true;
}

CPoint3d sample_points(const std::vector<CPoint3d>& points, double parameter) {
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1 || parameter <= 0.0) {
        return points.front();
    }
    if (parameter >= 1.0) {
        return points.back();
    }
    const double position = parameter * static_cast<double>(points.size() - 1);
    const std::size_t first = static_cast<std::size_t>(position);
    const std::size_t second = std::min(first + 1, points.size() - 1);
    const double fraction = position - static_cast<double>(first);
    return add(multiply(points[first], 1.0 - fraction), multiply(points[second], fraction));
}

CPoint3d sample_points_by_length(const std::vector<CPoint3d>& points,
                                 double parameter) {
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1 || parameter <= 0.0) {
        return points.front();
    }
    if (parameter >= 1.0) {
        return points.back();
    }

    double total_length = 0.0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        total_length += std::sqrt(squared_distance(points[index - 1], points[index]));
    }
    if (total_length <= kPointTolerance) {
        return points.front();
    }

    const double target_length = parameter * total_length;
    double accumulated_length = 0.0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        const double segment_length =
            std::sqrt(squared_distance(points[index - 1], points[index]));
        if (segment_length <= kPointTolerance) {
            continue;
        }
        if (accumulated_length + segment_length >= target_length) {
            const double fraction =
                (target_length - accumulated_length) / segment_length;
            return add(multiply(points[index - 1], 1.0 - fraction),
                       multiply(points[index], fraction));
        }
        accumulated_length += segment_length;
    }
    return points.back();
}

CPoint3d sample_closed_points(const std::vector<CPoint3d>& points, double parameter) {
    if (points.empty()) {
        return {};
    }
    const double wrapped = parameter - std::floor(parameter);
    const double position = wrapped * static_cast<double>(points.size());
    const std::size_t first = static_cast<std::size_t>(position) % points.size();
    const std::size_t second = (first + 1) % points.size();
    const double fraction = position - std::floor(position);
    return add(multiply(points[first], 1.0 - fraction), multiply(points[second], fraction));
}

SweepCurveSamples resample_profile(const SweepCurveSamples& profile,
                                   int closed_sample_count = 24) {
    constexpr int kOpenSampleCount = 25;
    const int count = profile.closed
        ? std::max(8, closed_sample_count)
        : kOpenSampleCount;
    SweepCurveSamples result;
    result.closed = profile.closed;
    result.points.reserve(static_cast<std::size_t>(count + (profile.closed ? 0 : 1)));
    const int last = profile.closed ? count - 1 : count;
    for (int i = 0; i <= last; ++i) {
        const double parameter = static_cast<double>(i) / static_cast<double>(count);
        result.points.push_back(profile.closed
            ? sample_closed_points(profile.points, parameter)
            : sample_points(profile.points, parameter));
    }
    return result;
}

TopoDS_Wire interpolate_wire(const SweepCurveSamples& samples) {
    const std::vector<CPoint3d> points = clean_points(samples);
    const std::size_t minimum = samples.closed ? 3u : 2u;
    if (points.size() < minimum) {
        return {};
    }

    try {
        Handle(TColgp_HArray1OfPnt) array = new TColgp_HArray1OfPnt(1, static_cast<int>(points.size()));
        for (std::size_t i = 0; i < points.size(); ++i) {
            array->SetValue(static_cast<int>(i + 1), gp_Pnt(points[i].x, points[i].y, points[i].z));
        }

        GeomAPI_Interpolate interpolation(array, samples.closed, 1.0e-7);
        interpolation.Perform();
        if (!interpolation.IsDone()) {
            return {};
        }
        Handle(Geom_BSplineCurve) curve = interpolation.Curve();
        if (curve.IsNull()) {
            return {};
        }

        BRepBuilderAPI_MakeEdge edge(curve);
        if (!edge.IsDone()) {
            return {};
        }
        BRepBuilderAPI_MakeWire wire(edge.Edge());
        return wire.IsDone() ? wire.Wire() : TopoDS_Wire{};
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Wire polygon_wire(const SweepCurveSamples& samples) {
    const std::vector<CPoint3d> points = clean_points(samples);
    if ((!samples.closed && points.size() < 2)
        || (samples.closed && points.size() < 3)) {
        return {};
    }
    BRepBuilderAPI_MakePolygon polygon;
    for (const CPoint3d& point : points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    if (samples.closed) {
        polygon.Close();
    }
    return polygon.IsDone() ? polygon.Wire() : TopoDS_Wire{};
}

bool has_face(const TopoDS_Shape& shape) {
    TopExp_Explorer faces(shape, TopAbs_FACE);
    return faces.More();
}

TopoDS_Shape loft_sections(const std::vector<TopoDS_Wire>& sections,
                           bool make_solid = false,
                           bool check_compatibility = true) {
    if (sections.size() < 2) {
        return {};
    }
    try {
        BRepOffsetAPI_ThruSections loft(
            make_solid ? Standard_True : Standard_False,
            Standard_False,
            1.0e-4);
        loft.CheckCompatibility(check_compatibility ? Standard_True : Standard_False);
        if (!make_solid) {
            loft.SetSmoothing(Standard_True);
            loft.SetMaxDegree(8);
        }
        for (const TopoDS_Wire& section : sections) {
            loft.AddWire(section);
        }
        loft.Build();
        if (!loft.IsDone() || !has_face(loft.Shape())) {
            return {};
        }
        return loft.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

bool principal_profile_axis(const SweepCurveSamples& profile,
                            const CPoint3d& normal,
                            CPoint3d& center,
                            CPoint3d& u_axis,
                            CPoint3d& v_axis,
                            double& u_midpoint,
                            double& u_span) {
    if (profile.points.size() < 3) {
        return false;
    }
    center = {};
    for (const CPoint3d& point : profile.points) {
        center = add(center, point);
    }
    center = multiply(center, 1.0 / static_cast<double>(profile.points.size()));

    CPoint3d basis_u{};
    for (const CPoint3d& point : profile.points) {
        basis_u = subtract(point, center);
        basis_u = subtract(basis_u, multiply(normal, dot3(basis_u, normal)));
        if (normalize(basis_u)) {
            break;
        }
    }
    if (!normalize(basis_u)) {
        return false;
    }
    CPoint3d basis_v = cross(normal, basis_u);
    if (!normalize(basis_v)) {
        return false;
    }

    double covariance_uu = 0.0;
    double covariance_uv = 0.0;
    double covariance_vv = 0.0;
    for (const CPoint3d& point : profile.points) {
        const CPoint3d relative = subtract(point, center);
        const double u = dot3(relative, basis_u);
        const double v = dot3(relative, basis_v);
        covariance_uu += u * u;
        covariance_uv += u * v;
        covariance_vv += v * v;
    }
    const double angle = 0.5 * std::atan2(
        2.0 * covariance_uv, covariance_uu - covariance_vv);
    u_axis = add(multiply(basis_u, std::cos(angle)),
                 multiply(basis_v, std::sin(angle)));
    if (!normalize(u_axis)) {
        return false;
    }
    v_axis = cross(normal, u_axis);
    if (!normalize(v_axis)) {
        return false;
    }

    double minimum_u = dot3(subtract(profile.points.front(), center), u_axis);
    double maximum_u = minimum_u;
    for (const CPoint3d& point : profile.points) {
        const double u = dot3(subtract(point, center), u_axis);
        minimum_u = std::min(minimum_u, u);
        maximum_u = std::max(maximum_u, u);
    }
    u_midpoint = (minimum_u + maximum_u) * 0.5;
    u_span = maximum_u - minimum_u;
    return u_span > kPointTolerance;
}

TopoDS_Shape solid_from_sections(const std::vector<TopoDS_Wire>& sections) {
    if (sections.size() < 2) {
        return {};
    }
    const TopoDS_Shape direct_solid = loft_sections(sections, true, false);
    if (!direct_solid.IsNull()) {
        return direct_solid;
    }
    const TopoDS_Shape side_faces = loft_sections(sections, false, false);
    if (side_faces.IsNull()) {
        return {};
    }
    BRepBuilderAPI_MakeFace first_cap(sections.front(), Standard_True);
    BRepBuilderAPI_MakeFace last_cap(sections.back(), Standard_True);
    if (!first_cap.IsDone() || !last_cap.IsDone()) {
        return {};
    }

    BRepBuilderAPI_Sewing sewing(1.0e-4, Standard_True, Standard_True,
                                 Standard_True, Standard_False);
    sewing.Add(side_faces);
    sewing.Add(first_cap.Face());
    sewing.Add(last_cap.Face());
    sewing.Perform();
    const TopoDS_Shape sewed = sewing.SewedShape();
    if (sewed.IsNull()) {
        return {};
    }

    const auto make_from_shell = [](const TopoDS_Shape& shell_shape) -> TopoDS_Shape {
        BRepBuilderAPI_MakeSolid solid(TopoDS::Shell(shell_shape));
        if (solid.IsDone() && !solid.Solid().IsNull()) {
            return solid.Solid();
        }
        return {};
    };
    if (sewed.ShapeType() == TopAbs_SHELL) {
        return make_from_shell(sewed);
    }
    for (TopExp_Explorer shells(sewed, TopAbs_SHELL); shells.More(); shells.Next()) {
        const TopoDS_Shape solid = make_from_shell(shells.Current());
        if (!solid.IsNull()) {
            return solid;
        }
    }
    return {};
}

TopoDS_Wire transform_exact_profile_wire(const TopoDS_Wire& source,
                                         const CPoint3d& source_center,
                                         const CPoint3d& source_u,
                                         const CPoint3d& source_v,
                                         const CPoint3d& source_normal,
                                         double source_u_midpoint,
                                         const CPoint3d& target_center,
                                         const CPoint3d& target_u,
                                         const CPoint3d& target_v,
                                         const CPoint3d& target_tangent,
                                         double scale_u) {
    if (source.IsNull()) {
        return {};
    }
    const auto value = [scale_u](double target_u_component,
                                 double source_u_component,
                                 double target_v_component,
                                 double source_v_component,
                                 double target_normal_component,
                                 double source_normal_component) {
        return target_u_component * scale_u * source_u_component
            + target_v_component * source_v_component
            + target_normal_component * source_normal_component;
    };
    const gp_Mat matrix(
        value(target_u.x, source_u.x, target_v.x, source_v.x, target_tangent.x, source_normal.x),
        value(target_u.x, source_u.y, target_v.x, source_v.y, target_tangent.x, source_normal.y),
        value(target_u.x, source_u.z, target_v.x, source_v.z, target_tangent.x, source_normal.z),
        value(target_u.y, source_u.x, target_v.y, source_v.x, target_tangent.y, source_normal.x),
        value(target_u.y, source_u.y, target_v.y, source_v.y, target_tangent.y, source_normal.y),
        value(target_u.y, source_u.z, target_v.y, source_v.z, target_tangent.y, source_normal.z),
        value(target_u.z, source_u.x, target_v.z, source_v.x, target_tangent.z, source_normal.x),
        value(target_u.z, source_u.y, target_v.z, source_v.y, target_tangent.z, source_normal.y),
        value(target_u.z, source_u.z, target_v.z, source_v.z, target_tangent.z, source_normal.z));
    const gp_XYZ mapped_center(
        matrix.Value(1, 1) * source_center.x
            + matrix.Value(1, 2) * source_center.y
            + matrix.Value(1, 3) * source_center.z,
        matrix.Value(2, 1) * source_center.x
            + matrix.Value(2, 2) * source_center.y
            + matrix.Value(2, 3) * source_center.z,
        matrix.Value(3, 1) * source_center.x
            + matrix.Value(3, 2) * source_center.y
            + matrix.Value(3, 3) * source_center.z);
    const gp_XYZ translation(
        target_center.x - mapped_center.X()
            - target_u.x * scale_u * source_u_midpoint,
        target_center.y - mapped_center.Y()
            - target_u.y * scale_u * source_u_midpoint,
        target_center.z - mapped_center.Z()
            - target_u.z * scale_u * source_u_midpoint);
    try {
        gp_GTrsf transform;
        transform.SetVectorialPart(matrix);
        transform.SetTranslationPart(translation);
        BRepBuilderAPI_GTransform builder(source, transform, true);
        builder.Build();
        if (!builder.IsDone() || builder.Shape().IsNull()
            || builder.Shape().ShapeType() != TopAbs_WIRE) {
            return {};
        }
        return TopoDS::Wire(builder.Shape());
    } catch (const Standard_Failure&) {
        return {};
    }
}
}

TopoDS_Shape BuildTwoRailSweepSurfaceShape(const SweepCurveSamples& profile,
                                           const SweepCurveSamples& first_rail,
                                           const SweepCurveSamples& second_rail) {
    if (profile.points.size() < 2 || first_rail.closed || second_rail.closed
        || first_rail.points.size() < 2 || second_rail.points.size() < 2) {
        return {};
    }

    SweepCurveSamples aligned_second = second_rail;
    const double same_direction = squared_distance(first_rail.points.front(), second_rail.points.front())
        + squared_distance(first_rail.points.back(), second_rail.points.back());
    const double opposite_direction = squared_distance(first_rail.points.front(), second_rail.points.back())
        + squared_distance(first_rail.points.back(), second_rail.points.front());
    if (opposite_direction < same_direction) {
        std::reverse(aligned_second.points.begin(), aligned_second.points.end());
    }

    SweepCurveSamples planar_profile;
    CPoint3d profile_normal{};
    if (!planarize_profile(profile, planar_profile, profile_normal)) {
        return {};
    }
    planar_profile = resample_profile(planar_profile);

    std::size_t first_anchor = 0;
    std::size_t second_anchor = 0;
    double diameter_squared = 0.0;
    for (std::size_t i = 0; i < planar_profile.points.size(); ++i) {
        for (std::size_t j = i + 1; j < planar_profile.points.size(); ++j) {
            const double distance = squared_distance(
                planar_profile.points[i], planar_profile.points[j]);
            if (distance > diameter_squared) {
                diameter_squared = distance;
                first_anchor = i;
                second_anchor = j;
            }
        }
    }
    if (diameter_squared <= kPointTolerance * kPointTolerance) {
        return {};
    }

    const CPoint3d template_center = multiply(add(
        planar_profile.points[first_anchor], planar_profile.points[second_anchor]), 0.5);
    CPoint3d template_u = subtract(
        planar_profile.points[second_anchor], planar_profile.points[first_anchor]);
    const double template_diameter = std::sqrt(diameter_squared);
    if (!normalize(template_u)) {
        return {};
    }
    CPoint3d template_v = cross(profile_normal, template_u);
    if (!normalize(template_v)) {
        return {};
    }

    constexpr int kSectionCount = 17;
    std::vector<TopoDS_Wire> sections;
    sections.reserve(kSectionCount);
    CPoint3d previous_v{};
    for (int section_index = 0; section_index < kSectionCount; ++section_index) {
        const double parameter = static_cast<double>(section_index)
            / static_cast<double>(kSectionCount - 1);
        const CPoint3d rail1 = sample_points(first_rail.points, parameter);
        const CPoint3d rail2 = sample_points(aligned_second.points, parameter);
        const CPoint3d center = multiply(add(rail1, rail2), 0.5);
        CPoint3d target_u = subtract(rail2, rail1);
        const double target_diameter = std::sqrt(dot3(target_u, target_u));
        if (!normalize(target_u) || target_diameter <= kPointTolerance) {
            return {};
        }

        const double delta = 1.0 / static_cast<double>(kSectionCount - 1);
        const double before_parameter = std::max(0.0, parameter - delta);
        const double after_parameter = std::min(1.0, parameter + delta);
        const CPoint3d center_before = multiply(add(
            sample_points(first_rail.points, before_parameter),
            sample_points(aligned_second.points, before_parameter)), 0.5);
        const CPoint3d center_after = multiply(add(
            sample_points(first_rail.points, after_parameter),
            sample_points(aligned_second.points, after_parameter)), 0.5);
        CPoint3d tangent = subtract(center_after, center_before);
        tangent = subtract(tangent, multiply(target_u, dot3(tangent, target_u)));
        if (!normalize(tangent)) {
            return {};
        }
        CPoint3d target_v = cross(tangent, target_u);
        if (!normalize(target_v)) {
            return {};
        }
        if (section_index > 0 && dot3(target_v, previous_v) < 0.0) {
            target_v = multiply(target_v, -1.0);
        }
        previous_v = target_v;

        const double scale = target_diameter / template_diameter;
        SweepCurveSamples transformed;
        transformed.closed = planar_profile.closed;
        transformed.points.reserve(planar_profile.points.size());
        for (const CPoint3d& point : planar_profile.points) {
            const CPoint3d relative = subtract(point, template_center);
            const double u = dot3(relative, template_u) * scale;
            const double v = dot3(relative, template_v) * scale;
            transformed.points.push_back(add(center,
                add(multiply(target_u, u), multiply(target_v, v))));
        }
        const TopoDS_Wire section = interpolate_wire(transformed);
        if (section.IsNull() || !BRepBuilderAPI_FindPlane(section).Found()) {
            return {};
        }
        sections.push_back(section);
    }
    return loft_sections(sections);
}

TopoDS_Shape BuildTwoRailSweepSolidShape(const SweepCurveSamples& profile,
                                         const SweepCurveSamples& first_rail,
                                         const SweepCurveSamples& second_rail) {
    return BuildTwoRailSweepSolidShape(
        profile, first_rail, second_rail, TopoDS_Wire{});
}

TopoDS_Shape BuildTwoRailSweepSolidShape(const SweepCurveSamples& profile,
                                         const SweepCurveSamples& first_rail,
                                         const SweepCurveSamples& second_rail,
                                         const TopoDS_Wire& exact_profile_wire) {
    if (!profile.closed || profile.points.size() < 3
        || first_rail.closed || second_rail.closed
        || first_rail.points.size() < 2 || second_rail.points.size() < 2) {
        return {};
    }

    SweepCurveSamples aligned_second = second_rail;
    const double same_direction = squared_distance(
        first_rail.points.front(), second_rail.points.front())
        + squared_distance(first_rail.points.back(), second_rail.points.back());
    const double opposite_direction = squared_distance(
        first_rail.points.front(), second_rail.points.back())
        + squared_distance(first_rail.points.back(), second_rail.points.front());
    if (opposite_direction < same_direction) {
        std::reverse(aligned_second.points.begin(), aligned_second.points.end());
    }

    SweepCurveSamples planar_profile;
    CPoint3d profile_normal{};
    if (!planarize_profile(profile, planar_profile, profile_normal)) {
        return {};
    }
    planar_profile = resample_profile(planar_profile, 64);

    CPoint3d template_center{};
    CPoint3d template_u{};
    CPoint3d template_v{};
    double template_u_midpoint = 0.0;
    double template_u_span = 0.0;
    if (!principal_profile_axis(planar_profile,
                                profile_normal,
                                template_center,
                                template_u,
                                template_v,
                                template_u_midpoint,
                                template_u_span)) {
        return {};
    }

    // A two-rail sweep with coplanar guides has a natural fixed direction:
    // the normal of the guide plane.  Keeping the unscaled profile axis in
    // this direction preserves the original section orientation (XY guides
    // therefore keep that axis parallel to Z).
    SweepCurveSamples combined_rails;
    combined_rails.points = first_rail.points;
    combined_rails.points.insert(combined_rails.points.end(),
                                 aligned_second.points.begin(),
                                 aligned_second.points.end());
    SweepCurveSamples planar_rails;
    CPoint3d guide_plane_normal{};
    const bool coplanar_guides = planarize_profile(
        combined_rails, planar_rails, guide_plane_normal);
    if (coplanar_guides && dot3(guide_plane_normal, template_v) < 0.0) {
        guide_plane_normal = multiply(guide_plane_normal, -1.0);
    }

    constexpr int kSectionCount = 49;
    const double frame_parameter_delta = 1.0 / std::max(
        256.0,
        4.0 * static_cast<double>(std::max(
            first_rail.points.size(), aligned_second.points.size())));
    std::vector<TopoDS_Wire> sections;
    sections.reserve(kSectionCount);
    CPoint3d previous_v{};
    for (int section_index = 0; section_index < kSectionCount; ++section_index) {
        const double parameter = static_cast<double>(section_index)
            / static_cast<double>(kSectionCount - 1);
        const CPoint3d rail1 =
            sample_points_by_length(first_rail.points, parameter);
        const CPoint3d rail2 =
            sample_points_by_length(aligned_second.points, parameter);
        const CPoint3d center = multiply(add(rail1, rail2), 0.5);

        const double before_parameter =
            std::max(0.0, parameter - frame_parameter_delta);
        const double after_parameter =
            std::min(1.0, parameter + frame_parameter_delta);
        const CPoint3d center_before = multiply(add(
            sample_points_by_length(first_rail.points, before_parameter),
            sample_points_by_length(aligned_second.points, before_parameter)), 0.5);
        const CPoint3d center_after = multiply(add(
            sample_points_by_length(first_rail.points, after_parameter),
            sample_points_by_length(aligned_second.points, after_parameter)), 0.5);
        CPoint3d tangent = subtract(center_after, center_before);
        if (!normalize(tangent)) {
            return {};
        }

        CPoint3d target_u = subtract(rail2, rail1);
        CPoint3d target_v{};
        double target_span = 0.0;
        if (coplanar_guides) {
            // The scalable axis joins the two rails exactly.  In particular,
            // the first wire now passes through both guide start points and
            // cannot acquire a tangent-induced tilt.
            target_u = subtract(target_u, multiply(
                guide_plane_normal, dot3(target_u, guide_plane_normal)));
            target_span = std::sqrt(dot3(target_u, target_u));
            if (!normalize(target_u) || target_span <= kPointTolerance) {
                return {};
            }
            target_v = guide_plane_normal;
            tangent = cross(target_u, target_v);
            if (!normalize(tangent)) {
                return {};
            }
        } else {
            target_u = subtract(target_u, multiply(tangent, dot3(target_u, tangent)));
            target_span = std::sqrt(dot3(target_u, target_u));
            if (!normalize(target_u) || target_span <= kPointTolerance) {
                return {};
            }
            target_v = cross(tangent, target_u);
            if (!normalize(target_v)) {
                return {};
            }
        }
        if (section_index > 0 && dot3(target_v, previous_v) < 0.0) {
            target_v = multiply(target_v, -1.0);
        }
        previous_v = target_v;

        const double scale_u = target_span / template_u_span;
        SweepCurveSamples transformed;
        transformed.closed = true;
        transformed.points.reserve(planar_profile.points.size());
        for (const CPoint3d& point : planar_profile.points) {
            const CPoint3d relative = subtract(point, template_center);
            const double u = (dot3(relative, template_u) - template_u_midpoint)
                * scale_u;
            const double v = dot3(relative, template_v);
            transformed.points.push_back(add(center,
                add(multiply(target_u, u), multiply(target_v, v))));
        }
        const TopoDS_Wire section = exact_profile_wire.IsNull()
            ? polygon_wire(transformed)
            : transform_exact_profile_wire(
                exact_profile_wire,
                template_center,
                template_u,
                template_v,
                profile_normal,
                template_u_midpoint,
                center,
                target_u,
                target_v,
                tangent,
                scale_u);
        if (section.IsNull() || !BRepBuilderAPI_FindPlane(section).Found()) {
            return {};
        }
        sections.push_back(section);
    }
    return solid_from_sections(sections);
}
