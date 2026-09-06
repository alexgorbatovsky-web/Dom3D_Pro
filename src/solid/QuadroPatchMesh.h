#pragma once
#include "QuadroFaceBoundary.h"
#include <array>

namespace quadro {
struct StructuredBodyPlan {
    bool eligible = false;
    SamplingOptions sampling;
    // Four CAD corner occurrences for a 4-edge face; four short-edge
    // occurrences (split at their master midpoint) for an 8-edge face.
    std::vector<std::vector<Id>> cornerOccurrences;
    std::vector<bool> splitCornerEdges;
};
struct StructuredPatch {
    Id face = invalidId;
    std::size_t columns = 0, rows = 0;
    std::vector<gp_Pnt2d> uv;
    std::vector<gp_Pnt> xyz;
    std::vector<Id> masterNodes;
    std::vector<std::array<Id,4>> quads;
    std::string error;
    double maximumSurfaceDeviation=0;
};
struct LogicalSidePlan {
    bool ready=false;
    std::array<Id,4> corners{};
    std::array<std::vector<Id>,4> edges;
};
LogicalSidePlan PlanLogicalCadSides(const FaceBoundaryInput& input);
struct LogicalBoundaryPlan {
    SamplingOptions sampling;
    std::vector<std::vector<Id>> cornerOccurrences;
};
LogicalBoundaryPlan PlanLogicalBoundary(const std::shared_ptr<const BoundarySnapshot>& boundary,const SamplingOptions& options);
StructuredBodyPlan PlanStructuredBody(const BoundarySnapshot& topology, double density);
StructuredPatch BuildStructuredPatch(const FaceBoundaryInput& input,
    const std::vector<Id>& cornerOccurrences, bool splitCornerEdges);
// Logical corners are indices in the unchanged chart loop, not new CAD vertices.
StructuredPatch BuildStructuredPatchAtCorners(const FaceBoundaryInput& input,
    const std::array<Id,4>& corners,bool harmonic=false);
StructuredPatch ValidateStructuredPatch(const FaceBoundaryInput& input,StructuredPatch patch,bool extendBoundaryDisplacement=false);
bool ValidateClosedPatchBody(const BoundarySnapshot& boundary,
    const std::vector<StructuredPatch>& patches, std::string& error);
}
