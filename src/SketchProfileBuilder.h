#pragma once

#include "Common.h"

class CSmartLine;
class TopoDS_Face;
class TopoDS_Wire;

bool BuildSketchProfileFace(const CSmartLine& sketch, TopoDS_Face& face, Vec3& normal);
bool BuildSketchRevolveProfileFace(const CSmartLine& sketch,
                                   Vec3 axis_origin,
                                   Vec3 axis_direction,
                                   TopoDS_Face& face,
                                   Vec3& normal);
bool BuildOpenSketchProfileWire(const CSmartLine& sketch,
                                TopoDS_Wire& wire);
// Builds the sketch edges in their authored order. Unlike the face boundary,
// this preserves line 0 as the start of a closed sweep path.
bool BuildSketchPathWire(const CSmartLine& sketch,
                         TopoDS_Wire& wire);
