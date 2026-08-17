#pragma once

#include "../Common.h"

class TopoDS_Face;
class TopoDS_Shape;

enum class SketchFeatureOperation {
    Protrusion = 0,
    Cut = 1
};

// Builds a temporary tapered extrusion from a sketch face and immediately
// applies it to the master body. The temporary tool shape is not retained.
bool BuildSketchFeatureShape(const TopoDS_Shape& body,
                             const TopoDS_Face& profile_face,
                             Vec3 outward_normal,
                             double depth,
                             double taper_angle_degrees,
                             SketchFeatureOperation operation,
                             TopoDS_Shape& result);
