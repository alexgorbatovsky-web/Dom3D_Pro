#pragma once

#include <cstddef>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <vector>

class CMesh3D;

struct CatmullClarkSurfaceBuildResult {
    TopoDS_Shape shape;
    bool solid = false;
    size_t patch_count = 0;
    size_t free_edge_count = 0;
    std::vector<TopoDS_Face> control_faces;
};

// Catmull-Clark divisions followed by one B-spline patch per control face.
// Two steps preserve the legacy Cushion Hybrid workflow. Three/four steps
// sample limit positions on a 9x9/17x17 grid before B-spline interpolation.
CatmullClarkSurfaceBuildResult BuildCatmullClarkSurfaceBody(
    const CMesh3D& control_mesh,
    double sewing_tolerance = 1.0e-4,
    int subdivision_steps = 2);
