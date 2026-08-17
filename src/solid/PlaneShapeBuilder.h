#pragma once

#include "../Common.h"

#include <TopoDS_Face.hxx>

bool BuildFinitePlaneFace(Vec3 origin,
                          Vec3 normal,
                          double size,
                          TopoDS_Face& result);
