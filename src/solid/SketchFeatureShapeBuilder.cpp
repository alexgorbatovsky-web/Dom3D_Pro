#include "SketchFeatureShapeBuilder.h"

#include "../ExtrudeShapeBuilder.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>

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
    const TopoDS_Shape tool = BuildExtrudeShape(
        profile_face, outward_normal, signed_depth, taper_angle_degrees);
    if (tool.IsNull()) {
        return false;
    }

    try {
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
