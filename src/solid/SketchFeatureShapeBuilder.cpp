#include "SketchFeatureShapeBuilder.h"

#include "../ExtrudeShapeBuilder.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>

namespace {
// Face attachments pass through float-valued UI coordinates. Restore exact
// coplanarity before the Boolean; a sub-micron gap/tilt can otherwise leave
// the pocket's starting cap inside the body as a nearly coincident membrane.
TopoDS_Face align_profile_to_host(const TopoDS_Shape& body,
                                 const TopoDS_Face& profile) {
    BRepAdaptor_Surface profile_surface(profile);
    if (profile_surface.GetType() != GeomAbs_Plane) return profile;
    const gp_Pln source = profile_surface.Plane();
    gp_Pln host;
    double closest = 1.0e-5;
    bool found = false;
    for (TopExp_Explorer it(body, TopAbs_FACE); it.More(); it.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
        if (surface.GetType() != GeomAbs_Plane) continue;
        const gp_Pln candidate = surface.Plane();
        if (std::abs(candidate.Axis().Direction().Dot(source.Axis().Direction()))
            < 1.0 - 1.0e-12) continue;
        const double distance = candidate.Distance(source.Location());
        if (distance < closest) {
            host = candidate;
            closest = distance;
            found = true;
        }
    }
    if (!found) return profile;
    gp_Dir normal = host.Axis().Direction();
    if (normal.Dot(source.Axis().Direction()) < 0.0) normal.Reverse();
    const gp_Vec n(normal);
    const gp_Pnt origin = source.Location().Translated(
        -n * gp_Vec(host.Location(), source.Location()).Dot(n));
    const gp_Vec x(source.Position().XDirection());
    const gp_Dir aligned_x(x - n * x.Dot(n));
    gp_Trsf transform;
    transform.SetDisplacement(source.Position(), gp_Ax3(origin, normal, aligned_x));
    BRepBuilderAPI_Transform move(profile, transform, true);
    return TopoDS::Face(move.Shape());
}
}

bool BuildSketchFeatureShape(const TopoDS_Shape& body,
                             const TopoDS_Face& profile_face,
                             Vec3 outward_normal,
                             double depth,
                             double taper_angle_degrees,
                             SketchFeatureOperation operation,
                             TopoDS_Shape& result)
{
    result.Nullify();
    if (body.IsNull() || profile_face.IsNull()
        || !std::isfinite(depth) || depth <= 1.0e-4
        || !std::isfinite(taper_angle_degrees)) {
        return false;
    }

    outward_normal = normalize(outward_normal);
    if (dot(outward_normal, outward_normal) <= 1.0e-12f) {
        return false;
    }

    const double signed_depth = operation == SketchFeatureOperation::Cut
        ? -depth
        : depth;
    try {
        const TopoDS_Face aligned_profile = align_profile_to_host(body, profile_face);
        const TopoDS_Shape tool = BuildExtrudeShape(
            aligned_profile, outward_normal, signed_depth, taper_angle_degrees);
        if (tool.IsNull()) return false;
        if (operation == SketchFeatureOperation::Cut) {
            BRepAlgoAPI_Cut boolean_operation(body, tool);
            boolean_operation.Build();
            if (!boolean_operation.IsDone()) {
                return false;
            }
            result = boolean_operation.Shape();
        } else {
            BRepAlgoAPI_Fuse boolean_operation(body, tool);
            boolean_operation.Build();
            if (!boolean_operation.IsDone()) {
                return false;
            }
            result = boolean_operation.Shape();
        }
    } catch (const Standard_Failure&) {
        result.Nullify();
        return false;
    }

    return !result.IsNull() && BRepCheck_Analyzer(result).IsValid();
}
