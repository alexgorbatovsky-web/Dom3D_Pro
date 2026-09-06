#pragma once
#include "QuadroBoundary.h"

namespace quadro {
struct ChartVertex {
    Id node, occurrence, sample;
    gp_Pnt2d uv;
    bool cornerReconciled = false;
};
struct ChartLoop {
    Id wire;
    bool outer = false;
    double signedArea = 0;
    // No duplicate closing point. Distinct seam sides keep distinct instances
    // even when they share a spatial node ID.
    std::vector<ChartVertex> vertices;
};
struct FaceBoundaryInput {
    std::shared_ptr<const BoundarySnapshot> owner;
    Id face = invalidId;
    bool reversed = false, ready = false;
    std::vector<ChartLoop> loops;
    std::vector<Issue> issues;
    // Original occurrence IDs of the first intersecting polygon segments.
    // Refinement must go through their shared CAD masters, never face-local.
    std::vector<Id> refinementOccurrences;
};

// Adapter for a bounded single UV chart. It never guesses outer/hole by area.
// Noncontractible wires and poles require explicit cuts, reported as such.
FaceBoundaryInput BuildFaceBoundaryInput(std::shared_ptr<const BoundarySnapshot> boundary, Id face);
bool ContainsUV(const FaceBoundaryInput& input, const gp_Pnt2d& uv);
}
