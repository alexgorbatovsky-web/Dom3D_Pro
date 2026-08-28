#pragma once

#include <string>
#include <vector>

struct SurfacePatchPoint {
    double u = 0.0;
    double v = 0.0;
    // True only for a node inherited from an original surface boundary.
    // Subdivision points created inside bridges are deliberately ineligible
    // as anchors for subsequent bridges.
    bool boundary_node = true;
};

// Converts one outer UV contour and any nested hole contours into independent
// simple contours.  The returned patches contain no holes and can therefore be
// triangulated and passed to ContourQuadrangulator independently.
bool BuildSurfacePatchesWithoutHoles(
    const std::vector<std::vector<SurfacePatchPoint>>& contours,
    std::vector<std::vector<SurfacePatchPoint>>& patches,
    std::string* error = nullptr);
