#pragma once

#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

// CAD boundary preparation, independent of CNet and ContourQuadrangulator.
namespace quadro {
using Id = std::size_t;
constexpr Id invalidId = static_cast<Id>(-1);

struct Issue {
    std::string code, message;
    Id face = invalidId, wire = invalidId, occurrence = invalidId, edge = invalidId;
    double residual = 0, budget = 0;
};
struct Vertex { Id id; gp_Pnt xyz; double tolerance; };
struct Edge {
    Id id, firstVertex, lastVertex;
    double firstParameter = 0, lastParameter = 0, tolerance = 0;
    bool degenerate = false, closed = false, sameParameter = false, sameRange = false;
    std::vector<Id> occurrences;
};
struct Occurrence {
    Id id, face, wire, edge, firstVertex, lastVertex;
    bool reversed = false, seam = false, hasPCurve = false;
    double firstParameter = 0, lastParameter = 0;
};
struct Wire { Id id, face; bool outer = false; std::vector<Id> occurrences; };
struct Face {
    Id id; double tolerance = 0, uPeriod = 0, vPeriod = 0;
    bool reversed = false;
    std::vector<Id> wires;
};
struct MasterNode {
    Id id; gp_Pnt xyz, cadXYZ;
    bool reconciliationAttempted = false;
    unsigned reconciliationIterations = 0;
};
struct EdgeMaster {
    Id edge;
    std::vector<double> parameters;
    std::vector<Id> nodes;
    double measuredChordError = 0;
};
struct ChartRefinement {unsigned pass;Id edge;std::size_t previousSegments,nextSegments;};
struct OccurrenceSample {
    Id node; double parameter; gp_Pnt2d uv;
    double residual, budget;
    double nominalParameter = 0, nominalResidual = 0;
    bool parameterCorrected = false;
};
struct OccurrenceView { Id occurrence; std::vector<OccurrenceSample> samples; };
struct UVTransition {
    Id fromOccurrence, toOccurrence;
    // Raw pcurves are retained. This records the chart transition; it does
    // not merge the two UV instances of a seam or silently cut the domain.
    long long uPeriods = 0, vPeriods = 0;
    double uRemainder = 0, vRemainder = 0;
};
struct EdgeSamplingStation {
    Id edge=invalidId;
    double parameter=0;
    std::vector<Id> beforeEdges;
    bool reversed=false;
    bool operator==(const EdgeSamplingStation& other) const {return edge==other.edge&&parameter==other.parameter&&beforeEdges==other.beforeEdges&&reversed==other.reversed;}
};
struct SamplingOptions {
    double chordTolerance = 0.01;
    double maxSegmentLength = 5.0;
    double numericalTolerance = 1.e-9;
    std::size_t minimumSegments = 8, maximumSegments = 32768;
    std::uint64_t samplingRevision = 1;
    bool allowLocalParameterCorrection = true;
    // Explicit opt-in: select one common representative inside the original
    // CAD tolerance envelopes. CAD vertices and source geometry stay fixed.
    bool allowCadToleranceReconciliation = false;
    unsigned maximumChartRefinementPasses = 0;
    // Requirements refer to IDs in this topology snapshot. A common edge
    // has one variable; simple opposite-side equalities share one count.
    std::vector<std::size_t> minimumSegmentsByEdge;
    std::vector<std::vector<Id>> equalSegmentGroups;
    // Opposite logical sides may span different numbers of CAD edges.
    std::vector<std::pair<std::vector<Id>,std::vector<Id>>> equalSegmentSums;
    std::vector<Id> arcLengthEdges;
    std::vector<EdgeSamplingStation> stations;
};

class BoundarySnapshot {
public:
    static std::shared_ptr<const BoundarySnapshot> Capture(
        const std::vector<TopoDS_Face>& faces, std::uint64_t bodyRevision);
    static std::shared_ptr<const BoundarySnapshot> Prepare(
        const BoundarySnapshot& topology, const SamplingOptions& options);

    std::uint64_t bodyRevision() const;
    std::uint64_t samplingRevision() const;
    bool topologyValid() const;
    bool discretizationReady() const;
    bool cadToleranceReconciliationEnabled() const;
    double meshTolerance() const;
    const std::vector<Vertex>& vertices() const;
    const std::vector<Edge>& edges() const;
    const std::vector<Face>& faces() const;
    const std::vector<Wire>& wires() const;
    const std::vector<Occurrence>& occurrences() const;
    const std::vector<Issue>& issues() const;
    const std::vector<MasterNode>& nodes() const;
    const std::vector<EdgeMaster>& masters() const;
    const std::vector<OccurrenceView>& views() const;
    const std::vector<UVTransition>& transitions() const;
    const std::vector<ChartRefinement>& chartRefinements() const;
    gp_Pnt2d evaluatePCurve(Id occurrence, double parameter) const;
    gp_Pnt evaluateSurface(Id face, const gp_Pnt2d& uv) const;
    // Diagnostic projection onto the underlying surface (not the trimmed
    // domain). Returns -1 if OCCT could not find a projection.
    double projectedSurfaceDistance(Id face, const gp_Pnt& xyz) const;
    double projectedEdgeParameter(Id edge,const gp_Pnt& xyz,double* distance=nullptr) const;

    // Checks an output boundary against the immutable master. Node identity
    // and order are mandatory, including closed-edge phase. No repair occurs.
    bool matchesBoundary(Id occurrence, const std::vector<Id>& nodeIds,
        const std::vector<gp_Pnt>& xyz, double numericalTolerance) const;
private:
    struct Data;
    explicit BoundarySnapshot(std::shared_ptr<const Data> data);
    std::shared_ptr<const Data> data_;
};

// One cache per body, accessed on its owning thread. Topology does not depend
// on sampling density. CAD replacement/location/orientation changes are caught
// by IsEqual; in-place geometry edits must call invalidate() before the edit.
class BoundaryCache {
public:
    std::shared_ptr<const BoundarySnapshot> topology(const TopoDS_Shape& shape);
    std::shared_ptr<const BoundarySnapshot> prepare(const TopoDS_Shape& shape, const SamplingOptions& options);
    void invalidate();
    std::uint64_t captureCount() const { return captureCount_; }
    std::uint64_t prepareCount() const { return prepareCount_; }
private:
    TopoDS_Shape source_;
    std::shared_ptr<const BoundarySnapshot> topology_, prepared_;
    SamplingOptions options_;
    std::uint64_t captureCount_ = 0, prepareCount_ = 0, revision_ = 0;
};
}
