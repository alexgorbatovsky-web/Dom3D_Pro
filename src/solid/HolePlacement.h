#pragma once

#include <array>
#include <string>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

class TopoDS_Face;

struct HoleFacePlacement {
    gp_Pnt center;
    gp_Dir normal;
    std::array<gp_Pnt, 2> starts, ends;
    std::array<double, 2> distances{}, sides{};
    double clearance = 0.0;
};

// Read-only placement on a planar CAD face, not on its display triangulation.
bool BuildHoleFacePlacement(const TopoDS_Face& face,
                           const gp_Pnt& clicked_point,
                           HoleFacePlacement& placement, std::string& error);
