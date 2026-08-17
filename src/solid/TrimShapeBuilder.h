#pragma once

#include "../Common.h"

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

bool TrimSolidByFace(const TopoDS_Shape& body,
                     const TopoDS_Face& cutting_face,
                     bool keep_positive_side,
                     TopoDS_Shape& result);

bool TrimSolidByClosedProfile(const TopoDS_Shape& body,
                              const TopoDS_Face& profile,
                              Vec3 extrusion_normal,
                              bool keep_inside,
                              TopoDS_Shape& result);

bool TrimSolidByOpenProfile(const TopoDS_Shape& body,
                            const TopoDS_Wire& profile,
                            Vec3 extrusion_normal,
                            Vec3 side_normal,
                            Vec3 side_origin,
                            bool keep_positive_side,
                            TopoDS_Shape& result);
