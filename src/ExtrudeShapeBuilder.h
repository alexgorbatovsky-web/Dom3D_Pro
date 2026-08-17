#pragma once

#include "Common.h"

class TopoDS_Face;
class TopoDS_Shape;

// Builds a prism and, when requested, applies a taper measured from the
// extrusion direction. Supports profiles containing free-form curves.
TopoDS_Shape BuildExtrudeShape(const TopoDS_Face& profile_face,
                               Vec3 normal,
                               double distance,
                               double taper_angle_degrees);
