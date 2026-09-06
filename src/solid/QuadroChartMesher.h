#pragma once
#include "QuadroFaceBoundary.h"
#include <array>

namespace quadro {
struct ChartFillResult {
    Id face=invalidId;
    std::string stage,error;
    bool donorReady=false;
    std::size_t patchCount=0;
    std::string strategy;
    std::vector<std::string> attempts;
    std::vector<gp_Pnt2d> sourceUV;
    std::vector<Id> sourceNodes;
    std::vector<std::array<Id,3>> triangles;
    std::vector<gp_Pnt2d> uv;
    std::vector<gp_Pnt> xyz;
    std::vector<Id> masterNodes;
    std::vector<std::array<Id,4>> quads;
    std::vector<gp_Pnt2d> candidateUV;
    std::vector<std::vector<Id>> candidateCells;
    std::vector<std::vector<gp_Pnt2d>> patchContours;
    Id failedPatch=invalidId;
    std::vector<Id> logicalCorners;
    double maximumSurfaceDeviation=0;
    bool ready() const {return error.empty()&&!quads.empty();}
};
// Isolated consumer: retains the actual donor and rejects output that changes
// boundary identity, fills holes or folds cells. Never mutates a CSolid.
ChartFillResult FillCadChart(const FaceBoundaryInput& chart,const std::vector<Id>& cornerOccurrences={});
}
