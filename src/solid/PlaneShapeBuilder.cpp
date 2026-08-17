#include "PlaneShapeBuilder.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <cmath>

bool BuildFinitePlaneFace(Vec3 origin,
                          Vec3 normal,
                          double size,
                          TopoDS_Face& result) {
    result.Nullify();
    const double normal_length = std::sqrt(
        static_cast<double>(normal.x) * normal.x
        + static_cast<double>(normal.y) * normal.y
        + static_cast<double>(normal.z) * normal.z);
    if (normal_length <= 1.0e-9 || size <= 1.0e-6) {
        return false;
    }

    try {
        const gp_Pln plane(
            gp_Pnt(origin.x, origin.y, origin.z),
            gp_Dir(normal.x, normal.y, normal.z));
        const double half_size = size * 0.5;
        BRepBuilderAPI_MakeFace face(
            plane, -half_size, half_size, -half_size, half_size);
        if (!face.IsDone() || face.Face().IsNull()) {
            return false;
        }
        result = face.Face();
        return true;
    } catch (...) {
        return false;
    }
}
