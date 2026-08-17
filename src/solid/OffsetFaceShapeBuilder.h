#pragma once

class TopoDS_Face;
class TopoDS_Shape;

// Moves one face of a solid by a signed offset. Adjacent faces are extended
// and intersected by OCCT so the result remains a closed solid.
bool BuildOffsetFaceShape(const TopoDS_Shape& body,
                          const TopoDS_Face& face,
                          double distance,
                          TopoDS_Shape& result);
