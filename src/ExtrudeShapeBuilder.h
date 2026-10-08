#pragma once

#include "Common.h"
#include <vector>
#include <string>

class TopoDS_Face;
class TopoDS_Shape;
TopoDS_Shape BuildExtrudedFaceSolid(const TopoDS_Shape& body,const TopoDS_Face& face,
                                  Vec3 direction,double distance,double taper,std::string& error);

// Builds a prism and, when requested, applies a taper measured from the
// extrusion direction. Supports profiles containing free-form curves.
TopoDS_Shape BuildExtrudeShape(const TopoDS_Face& profile_face,
                               Vec3 normal,
                               double distance,
                               double taper_angle_degrees);
// Disconnected profile regions remain separate solids in one compound.
TopoDS_Shape BuildExtrudeShape(const std::vector<TopoDS_Face>& profile_faces,
                              Vec3 normal, double distance, double taper_angle_degrees);

// Curved taper uses a similar curved cap; positive angles narrow in either direction.
bool CurvedExtrudeScale(const TopoDS_Face& face, double distance, double angle,
                        Vec3& center, double& scale);
