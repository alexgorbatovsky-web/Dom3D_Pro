#pragma once

#include <cstddef>

#include <TopoDS_Shape.hxx>

class CMesh3D;

struct CatmullClarkSurfaceBuildResult {
    TopoDS_Shape shape;
    bool solid = false;
    size_t patch_count = 0;
    size_t free_edge_count = 0;
};

// Reproduces the old CMesh3D_XL::MakeTwoSmoothAndSurface workflow:
// two Catmull-Clark divisions followed by one B-spline patch per control face.
CatmullClarkSurfaceBuildResult BuildCatmullClarkSurfaceBody(
    const CMesh3D& control_mesh,
    double sewing_tolerance = 1.0e-4);
