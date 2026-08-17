#pragma once

#include "../Common.h"

#include <TopoDS_Shape.hxx>

#include <vector>

class TopoDS_Face;
class TopoDS_Wire;

bool BuildPolyhedronShape(const std::vector<Vec3>& profile_points,
                          bool profile_closed,
                          Vec3 axis_origin,
                          Vec3 axis_direction,
                          int turns,
                          TopoDS_Shape& result);

bool BuildPolyhedronShapeFromProfileFace(const TopoDS_Face& profile_face,
                                         Vec3 axis_origin,
                                         Vec3 axis_direction,
                                         int turns,
                                         TopoDS_Shape& result);

bool BuildPolyhedronShapeFromOpenProfileWire(
    const TopoDS_Wire& profile_wire,
    Vec3 axis_origin,
    Vec3 axis_direction,
    int turns,
    TopoDS_Shape& result);
