#include "QuadroBoundary.h"
#include "QuadroFaceBoundary.h"
#include "QuadroSegmentCounts.h"
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom_Surface.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <set>
#include <map>

namespace quadro {
struct BoundarySnapshot::Data {
    std::uint64_t bodyRevision = 0, samplingRevision = 0;
    bool topologyValid = false, ready = false;
    bool toleranceReconciliation = false;
    double meshTolerance = 0;
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Wire> wires;
    std::vector<Occurrence> occurrences;
    std::vector<Issue> issues;
    std::vector<MasterNode> nodes;
    std::vector<EdgeMaster> masters;
    std::vector<OccurrenceView> views;
    std::vector<UVTransition> transitions;
    std::vector<ChartRefinement> chartRefinements;
    // Geometry is deep-copied once, never exposed through mutable OCCT handles.
    std::vector<TopoDS_Edge> cadEdges;
    std::vector<Handle(Geom2d_Curve)> pcurves;
    std::vector<Handle(Geom_Surface)> surfaces;
    std::vector<TopLoc_Location> locations;
};
BoundarySnapshot::BoundarySnapshot(std::shared_ptr<const Data> d) : data_(std::move(d)) {}
std::uint64_t BoundarySnapshot::bodyRevision() const { return data_->bodyRevision; }
std::uint64_t BoundarySnapshot::samplingRevision() const { return data_->samplingRevision; }
bool BoundarySnapshot::topologyValid() const { return data_->topologyValid; }
bool BoundarySnapshot::discretizationReady() const { return data_->ready; }
bool BoundarySnapshot::cadToleranceReconciliationEnabled() const { return data_->toleranceReconciliation; }
double BoundarySnapshot::meshTolerance() const { return data_->meshTolerance; }
#define QUADRO_GETTER(type, name) const std::vector<type>& BoundarySnapshot::name() const { return data_->name; }
QUADRO_GETTER(Vertex, vertices)
QUADRO_GETTER(Edge, edges)
QUADRO_GETTER(Face, faces)
QUADRO_GETTER(Wire, wires)
QUADRO_GETTER(Occurrence, occurrences)
QUADRO_GETTER(Issue, issues)
QUADRO_GETTER(MasterNode, nodes)
QUADRO_GETTER(EdgeMaster, masters)
QUADRO_GETTER(OccurrenceView, views)
QUADRO_GETTER(UVTransition, transitions)
QUADRO_GETTER(ChartRefinement, chartRefinements)
#undef QUADRO_GETTER
gp_Pnt2d BoundarySnapshot::evaluatePCurve(Id o, double t) const {
    const auto& curve = data_->pcurves.at(o);
    if (curve.IsNull()) throw std::runtime_error("MissingPCurve");
    return curve->Value(t);
}
gp_Pnt BoundarySnapshot::evaluateSurface(Id face, const gp_Pnt2d& uv) const {
    return data_->surfaces.at(face)->Value(uv.X(),uv.Y()).Transformed(data_->locations.at(face).Transformation());
}
double BoundarySnapshot::projectedSurfaceDistance(Id face, const gp_Pnt& xyz) const {
    try {
        const auto local=xyz.Transformed(data_->locations.at(face).Transformation().Inverted());
        GeomAPI_ProjectPointOnSurf projection(local,data_->surfaces.at(face),1.e-10);
        return projection.NbPoints()>0 ? projection.NearestPoint().Transformed(data_->locations.at(face).Transformation()).Distance(xyz) : -1;
    } catch(const Standard_Failure&) {return -1;}
}
double BoundarySnapshot::projectedEdgeParameter(Id edge,const gp_Pnt& xyz,double* distance) const {
    if(distance)*distance=-1;
    try {
        double first,last;const auto curve=BRep_Tool::Curve(data_->cadEdges.at(edge),first,last);
        if(!curve.IsNull()) {
            GeomAPI_ProjectPointOnCurve projection(xyz,curve,first,last);
            if(projection.NbPoints()>0){if(distance)*distance=projection.LowerDistance();return projection.LowerDistanceParameter();}
        }
    }catch(const Standard_Failure&){}
    return std::numeric_limits<double>::quiet_NaN();
}

std::shared_ptr<const BoundarySnapshot> BoundarySnapshot::Capture(
    const std::vector<TopoDS_Face>& input, std::uint64_t revision) {
    auto d = std::make_shared<Data>(); d->bodyRevision = revision;
    auto issue = [&](std::string code, std::string message, Id f = invalidId,
                     Id w = invalidId, Id o = invalidId, Id e = invalidId) {
        d->issues.push_back({std::move(code), std::move(message), f, w, o, e});
    };
    try {
        // Copy all faces in one operation to preserve sharing. Never copy faces
        // separately: doing so would erase shared edge/vertex identity.
        BRep_Builder builder; TopoDS_Compound body; builder.MakeCompound(body);
        for (const auto& face : input) {
            if (face.IsNull()) { issue("NullFace", "Null CAD face"); continue; }
            builder.Add(body, face);
        }
        BRepBuilderAPI_Copy copy(body, true, false);
        TopTools_IndexedMapOfShape vm, em;
        auto vertex = [&](const TopoDS_Vertex& v) -> Id {
            if (v.IsNull()) return invalidId;
            const Id id = static_cast<Id>(vm.Add(v) - 1);
            if (id == d->vertices.size()) d->vertices.push_back({id, BRep_Tool::Pnt(v), BRep_Tool::Tolerance(v)});
            return id;
        };
        for (Id fi = 0; fi < input.size(); ++fi) {
            if (input[fi].IsNull()) continue;
            auto face = TopoDS::Face(copy.ModifiedShape(input[fi]));
            // ModifiedShape identifies copied geometry, but its orientation is
            // not necessarily that of this occurrence in the source body.
            face.Orientation(input[fi].Orientation());
            Face f; f.id = fi; f.reversed = face.Orientation() == TopAbs_REVERSED;
            // Wire direction is in the intrinsic forward face chart. Face
            // orientation is retained separately for final cell normals.
            face.Orientation(TopAbs_FORWARD);
            f.tolerance = BRep_Tool::Tolerance(face);
            TopLoc_Location loc; auto surf = BRep_Tool::Surface(face, loc);
            f.uPeriod = surf->IsUPeriodic() ? surf->UPeriod() : 0;
            f.vPeriod = surf->IsVPeriodic() ? surf->VPeriod() : 0;
            d->surfaces.push_back(surf); d->locations.push_back(loc);
            const auto outer = BRepTools::OuterWire(face);
            for (TopExp_Explorer wx(face, TopAbs_WIRE); wx.More(); wx.Next()) {
                const auto wire = TopoDS::Wire(wx.Current());
                Wire w; w.id = d->wires.size(); w.face = fi; w.outer = wire.IsSame(outer);
                std::size_t expected = 0;
                for (TopoDS_Iterator it(wire); it.More(); it.Next()) if (it.Value().ShapeType() == TopAbs_EDGE) ++expected;
                for (BRepTools_WireExplorer it(wire, face); it.More(); it.Next()) {
                    const auto occurrenceEdge = it.Current();
                    if (occurrenceEdge.Orientation() != TopAbs_FORWARD && occurrenceEdge.Orientation() != TopAbs_REVERSED)
                        issue("UnsupportedOrientation", "Edge is neither forward nor reversed", fi, w.id);
                    auto edge = occurrenceEdge; edge.Orientation(TopAbs_FORWARD);
                    const Id ei = static_cast<Id>(em.Add(edge) - 1);
                    if (ei == d->edges.size()) {
                        Edge e; e.id = ei;
                        e.firstVertex = vertex(TopExp::FirstVertex(edge, true));
                        e.lastVertex = vertex(TopExp::LastVertex(edge, true));
                        e.tolerance = BRep_Tool::Tolerance(edge);
                        e.degenerate = BRep_Tool::Degenerated(edge);
                        e.closed = e.firstVertex == e.lastVertex;
                        e.sameParameter = BRep_Tool::SameParameter(edge);
                        e.sameRange = BRep_Tool::SameRange(edge);
                        if (e.degenerate && e.firstVertex != e.lastVertex)
                            issue("UnsupportedDegenerateBoundary", "Degenerate edge has distinct vertex identities", fi, w.id, invalidId, ei);
                        if (!e.degenerate) {
                            BRepAdaptor_Curve c(edge); e.firstParameter = c.FirstParameter(); e.lastParameter = c.LastParameter();
                        }
                        if (e.firstVertex == invalidId || e.lastVertex == invalidId)
                            issue("MissingVertex", "Bounded edge must have endpoints", fi, w.id, invalidId, ei);
                        d->edges.push_back(e); d->cadEdges.push_back(edge);
                    }
                    Occurrence o; o.id = d->occurrences.size(); o.face = fi; o.wire = w.id; o.edge = ei;
                    o.reversed = occurrenceEdge.Orientation() == TopAbs_REVERSED;
                    o.firstVertex = vertex(TopExp::FirstVertex(occurrenceEdge, true));
                    o.lastVertex = vertex(TopExp::LastVertex(occurrenceEdge, true));
                    o.seam = BRep_Tool::IsClosed(occurrenceEdge, face);
                    // OCCT selects the second seam pcurve for a reversed edge
                    // on a forward face. Preserve the occurrence orientation.
                    auto pc = BRep_Tool::CurveOnSurface(occurrenceEdge, face, o.firstParameter, o.lastParameter);
                    o.hasPCurve = !pc.IsNull();
                    if (!o.hasPCurve) issue("MissingPCurve", "Occurrence has no pcurve", fi, w.id, o.id, ei);
                    else if (!std::isfinite(o.firstParameter) || !std::isfinite(o.lastParameter) || o.lastParameter <= o.firstParameter)
                        issue("InvalidPCurveRange", "Invalid pcurve interval", fi, w.id, o.id, ei);
                    d->pcurves.push_back(pc); d->edges[ei].occurrences.push_back(o.id);
                    w.occurrences.push_back(o.id); d->occurrences.push_back(o);
                }
                if (expected != w.occurrences.size() || expected == 0)
                    issue("IncompleteWireTraversal", "Wire explorer did not preserve every edge occurrence", fi, w.id);
                for (Id i = 0; i < w.occurrences.size(); ++i) {
                    const auto& a = d->occurrences[w.occurrences[i]];
                    const auto& b = d->occurrences[w.occurrences[(i + 1) % w.occurrences.size()]];
                    if (a.lastVertex == invalidId || a.lastVertex != b.firstVertex)
                        issue("TopologyOpenWire", "Consecutive CAD vertex identities do not match", fi, w.id, a.id, a.edge);
                }
                f.wires.push_back(w.id); d->wires.push_back(w);
            }
            if (f.wires.empty()) issue("MissingWire", "Face has no bounded wires", fi);
            d->faces.push_back(f);
        }
        // Ensure the two sides of a seam survived as different UV curves.
        for (const auto& o : d->occurrences) if (o.seam && o.hasPCurve) {
            bool otherSide = false;
            for (Id id : d->edges[o.edge].occurrences) {
                const auto& b = d->occurrences[id];
                if (b.id == o.id || b.face != o.face || !b.hasPCurve || b.reversed == o.reversed) continue;
                const auto aUV = d->pcurves[o.id]->Value((o.firstParameter + o.lastParameter) / 2);
                const auto bUV = d->pcurves[b.id]->Value((b.firstParameter + b.lastParameter) / 2);
                if (aUV.Distance(bUV) > 1.e-12) otherSide = true;
            }
            if (!otherSide) issue("AmbiguousSeam", "Second oriented seam pcurve is absent or collapsed", o.face, o.wire, o.id, o.edge);
        }
    } catch (const Standard_Failure& e) { issue("CadException", e.GetMessageString() ? e.GetMessageString() : "OCCT failure"); }
    if (input.empty()) issue("EmptyBody", "No faces supplied");
    d->topologyValid = d->issues.empty();
    return std::shared_ptr<const BoundarySnapshot>(new BoundarySnapshot(d));
}

namespace {
double segmentDistance(const gp_Pnt& p, const gp_Pnt& a, const gp_Pnt& b) {
    gp_Vec ab(a,b), ap(a,p); const double sq = ab.SquareMagnitude();
    const double t = sq > 0 ? std::clamp(ap.Dot(ab)/sq, 0., 1.) : 0.;
    return p.Distance(a.Translated(ab * t));
}

// Bounded one-dimensional fit to the selected pcurve branch. Each sample has
// its own disjoint parameter cell, so the fit cannot reverse order or jump
// to the other side of a seam. This is not global CAD healing.
template<class Evaluate>
double fitParameter(double lo, double hi, double nominal, const gp_Pnt& target, Evaluate evaluate) {
    const auto error=[&](double t){return evaluate(t).SquareDistance(target);};
    double best=nominal, value=error(best);
    int winner=0;
    for (int k=0;k<=16;++k) {
        const double t=lo+(hi-lo)*k/16.; const double v=error(t);
        if (std::isfinite(v) && v<value) {best=t;value=v;winner=k;}
    }
    // Always refine the best cell, including when nominal was best on probes.
    if (best==nominal) winner=std::clamp(int((nominal-lo)/(hi-lo)*16),0,16);
    double a=lo+(hi-lo)*std::max(0,winner-1)/16.;
    double b=lo+(hi-lo)*std::min(16,winner+1)/16.;
    constexpr double ratio=0.6180339887498948482;
    double x=b-ratio*(b-a), y=a+ratio*(b-a), fx=error(x), fy=error(y);
    for(int k=0;k<48;++k) {
        if(fx<fy){b=y;y=x;fy=fx;x=b-ratio*(b-a);fx=error(x);}
        else {a=x;x=y;fx=fy;y=a+ratio*(b-a);fy=error(y);}
    }
    for(double t:{x,y}) {const double v=error(t);if(std::isfinite(v)&&v<value){best=t;value=v;}}
    return best;
}
}

std::shared_ptr<const BoundarySnapshot> BoundarySnapshot::Prepare(
    const BoundarySnapshot& topology, const SamplingOptions& options) {
    if(options.maximumChartRefinementPasses>0&&options.maximumChartRefinementPasses<=16) {
        auto next=options;next.maximumChartRefinementPasses=0;
        std::vector<ChartRefinement> refinements;
        for(unsigned pass=0;;++pass) {
            const auto prepared=Prepare(topology,next);
            auto data=std::make_shared<Data>(*prepared->data_);data->chartRefinements=refinements;
            const auto finish=[&](){return std::shared_ptr<const BoundarySnapshot>(new BoundarySnapshot(data));};
            if(!prepared->discretizationReady())return finish();
            std::set<Id> edges;
            for(const auto& face:prepared->faces()) {
                const auto chart=BuildFaceBoundaryInput(prepared,face.id);
                for(Id occurrence:chart.refinementOccurrences)edges.insert(prepared->occurrences()[occurrence].edge);
            }
            if(edges.empty())return finish();
            if(pass==options.maximumChartRefinementPasses) {
                data->issues.push_back({"ChartRefinementBudgetExceeded","UV intersections remain after shared edge refinement"});
                data->ready=false;return finish();
            }
            next.minimumSegmentsByEdge.resize(prepared->edges().size());
            for(const auto& master:prepared->masters())next.minimumSegmentsByEdge[master.edge]=master.nodes.size()-1;
            for(Id edge:edges) {
                const auto count=next.minimumSegmentsByEdge[edge];
                if(count>options.maximumSegments/2) {
                    Issue issue{"ChartRefinementBudgetExceeded","UV intersection requires more shared edge segments than allowed"};issue.edge=edge;
                    data->issues.push_back(issue);data->ready=false;return finish();
                }
                refinements.push_back({pass+1,edge,count,count*2});next.minimumSegmentsByEdge[edge]=count*2;
            }
        }
    }
    auto d = std::make_shared<Data>(*topology.data_);
    d->ready = false; d->samplingRevision = options.samplingRevision;
    d->toleranceReconciliation = options.allowCadToleranceReconciliation;
    d->meshTolerance = options.chordTolerance;
    d->nodes.clear(); d->masters.clear(); d->views.clear(); d->transitions.clear();
    d->chartRefinements.clear();
    auto result = [&]() { return std::shared_ptr<const BoundarySnapshot>(new BoundarySnapshot(d)); };
    if (!d->topologyValid) return result();
    // Retrying a prepared snapshot must not inherit old sampling failures.
    d->issues.clear();
    if (!(std::isfinite(options.chordTolerance) && options.chordTolerance > 0 &&
          std::isfinite(options.maxSegmentLength) && options.maxSegmentLength > 0 &&
          std::isfinite(options.numericalTolerance) && options.numericalTolerance >= 0) ||
        options.minimumSegments < 1 || options.maximumSegments < options.minimumSegments ||
        options.maximumSegments > 1000000 || options.maximumChartRefinementPasses>16) {
        d->issues.push_back({"InvalidSamplingOptions", "Invalid tolerances or segment budget"}); return result();
    }
    for (const auto& v : d->vertices) d->nodes.push_back({v.id, v.xyz, v.xyz});
    std::vector<Id> parent(d->edges.size()); std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](Id e) { while (parent[e] != e) e = parent[e]; return e; };
    for (const auto& group : options.equalSegmentGroups) {
        for (Id e : group) if (e >= parent.size() || d->edges[e].degenerate) {
            d->issues.push_back({"CountConstraintConflict", "Invalid or degenerate edge in count equality"}); return result();
        }
        for (Id e : group) parent[root(e)] = root(group.front());
    }
    std::vector<std::size_t> counts(d->edges.size(), options.minimumSegments);
    const std::set<Id> arcLengthEdges(options.arcLengthEdges.begin(),options.arcLengthEdges.end());
    for(Id e:arcLengthEdges)if(e>=d->edges.size()||d->edges[e].degenerate){d->issues.push_back({"InvalidSamplingOptions","Invalid arc-length edge"});return result();}
    std::vector<std::vector<double>> sampleParameters(d->edges.size());
    for(const auto& station:options.stations) {
        if(station.edge>=d->edges.size()||d->edges[station.edge].degenerate||!std::isfinite(station.parameter)||station.beforeEdges.empty()){d->issues.push_back({"InvalidSamplingOptions","Invalid edge station"});return result();}
        for(Id e:station.beforeEdges)if(e>=d->edges.size()){d->issues.push_back({"InvalidSamplingOptions","Invalid station count prefix"});return result();}
    }
    for(const auto& sums:options.equalSegmentSums)for(const auto& side:{sums.first,sums.second}) {
        if(side.empty()){d->issues.push_back({"CountConstraintConflict","Empty logical side"});return result();}
        for(Id e:side)if(e>=d->edges.size()||d->edges[e].degenerate){d->issues.push_back({"CountConstraintConflict","Invalid edge in logical side"});return result();}
    }
    for (Id i = 0; i < options.minimumSegmentsByEdge.size(); ++i) {
        if (i >= counts.size()) { d->issues.push_back({"CountConstraintConflict", "Count requirement outside edge registry"}); return result(); }
        counts[i] = std::max(counts[i], options.minimumSegmentsByEdge[i]);
    }
    auto fail = [&](const char* code, const char* message, const Edge& e, double r = 0, double b = 0, Id oi = invalidId) {
        Issue issue{code,message}; issue.edge = e.id; issue.residual = r; issue.budget = b;
        if (oi != invalidId) { const auto& o = d->occurrences[oi]; issue.face=o.face; issue.wire=o.wire; issue.occurrence=oi; }
        d->issues.push_back(issue);
    };
    try {
        // All refinement/count propagation occurs before publishing any mesh
        // boundary. Counts grow monotonically and have a hard finite budget.
        bool changed = true;
        unsigned countPass=0;
        while (changed) {
            if(++countPass>128){d->issues.push_back({"CountConstraintBudget","Logical side count propagation did not converge"});return result();}
            changed = false;
            for (Id i=0; i<counts.size(); ++i) counts[root(i)] = std::max(counts[root(i)], counts[i]);
            for (Id i=0; i<counts.size(); ++i) counts[i] = counts[root(i)];
            if(!options.equalSegmentSums.empty()) {
                const auto previous=counts;
                if(!detail::BalanceSegmentCounts(counts,options)){d->issues.push_back({"CountConstraintBudget","No bounded integer solution for logical side counts"});return result();}
                changed=counts!=previous;
            }
            for (const auto& e : d->edges) {
                if (e.degenerate) continue;
                if (counts[e.id] > options.maximumSegments) {
                    fail("RefinementBudgetExceeded", "Shared segment requirements exceed budget", e); return result();
                }
                if ((!e.sameParameter || !e.sameRange) && !options.allowLocalParameterCorrection) {
                    fail("ParameterMapMismatch", "Non-SameParameter/SameRange requires an explicit parameter mapper", e); return result();
                }
                BRepAdaptor_Curve curve(d->cadEdges[e.id]);
                auto& parameters=sampleParameters[e.id];parameters.resize(counts[e.id]+1);
                std::map<Id,double> anchors{{0,e.firstParameter},{counts[e.id],e.lastParameter}};
                for(const auto& station:options.stations)if(station.edge==e.id) {
                    Id index=0;for(Id source:station.beforeEdges)index+=counts[source];
                    if(index==0||index>=counts[e.id]){fail("StationCountConflict","Station count prefix is outside edge",e,0,0);return result();}
                    if(station.reversed)index=counts[e.id]-index;
                    auto [it,inserted]=anchors.emplace(index,station.parameter);
                    if(!inserted&&std::abs(it->second-station.parameter)>1.e-12*std::max(1.0,std::abs(e.lastParameter-e.firstParameter))){fail("StationCountConflict","Two stations require different parameters at one index",e,0,0);return result();}
                }
                for(auto it=anchors.begin(),next=std::next(it);next!=anchors.end();++it,++next) {
                    const Id start=it->first,segments=next->first-start;const double first=it->second,last=next->second;
                    if(!(last>first)){fail("StationOrderConflict","CAD stations are not monotone",e,0,0);return result();}
                    if(arcLengthEdges.count(e.id)) {
                        GCPnts_UniformAbscissa uniform(curve,int(segments+1),first,last);
                        if(!uniform.IsDone()||uniform.NbPoints()!=int(segments+1)){fail("ArcLengthSamplingFailed","CAD arc-length discretization failed",e,0,0);return result();}
                        for(Id i=0;i<=segments;++i)parameters[start+i]=i==0?first:i==segments?last:uniform.Parameter(int(i+1));
                    }else for(Id i=0;i<=segments;++i)parameters[start+i]=first+(last-first)*double(i)/segments;
                }
                const auto point = [&](double t, std::size_t i) {
                    if (i==0) return d->vertices[e.firstVertex].xyz;
                    if (i==counts[e.id]) return d->vertices[e.lastVertex].xyz;
                    return curve.Value(t);
                };
                bool refine = false;
                for (std::size_t i=0; i<counts[e.id] && !refine; ++i) {
                    const double t0=parameters[i],t1=parameters[i+1];
                    const auto a=point(t0,i), b=point(t1,i+1);
                    refine = a.Distance(b)>options.maxSegmentLength;
                    // Chord error is relative to the original CAD curve. The
                    // endpoint-anchor discrepancy is allowed by CAD tolerance,
                    // not "repaired" by endlessly increasing segment count.
                    const double anchorBudget = (i==0 ? d->vertices[e.firstVertex].tolerance : 0) +
                        (i+1==counts[e.id] ? d->vertices[e.lastVertex].tolerance : 0);
                    for (double q : {0.25,0.5,0.75})
                        refine = refine || segmentDistance(curve.Value(t0+(t1-t0)*q),a,b) > options.chordTolerance+anchorBudget;
                }
                if (refine) { counts[e.id] *= 2; changed=true; }
            }
        }
        for (const auto& e : d->edges) {
            EdgeMaster master; master.edge=e.id;
            const std::size_t n=e.degenerate ? 1 : counts[e.id];
            std::unique_ptr<BRepAdaptor_Curve> curve;
            if (!e.degenerate) curve=std::make_unique<BRepAdaptor_Curve>(d->cadEdges[e.id]);
            for (std::size_t i=0; i<=n; ++i) {
                const double t=e.degenerate?e.firstParameter+(e.lastParameter-e.firstParameter)*double(i)/n:sampleParameters[e.id][i];
                Id node = invalidId;
                if (i==0 || e.degenerate) node=e.firstVertex;
                else if (i==n) node=e.lastVertex;
                else { node=d->nodes.size();const auto xyz=curve->Value(t);d->nodes.push_back({node,xyz,xyz}); }
                master.parameters.push_back(t); master.nodes.push_back(node);
                if (curve && (i==0 || i==n)) {
                    const double r=curve->Value(t).Distance(d->nodes[node].xyz);
                    const double budget=std::max(e.tolerance,d->vertices[node].tolerance)+options.numericalTolerance;
                    if (r>budget) fail("CadEndpointMismatch", "Curve endpoint exceeds CAD vertex/edge budget",e,r,budget);
                }
            }
            if (curve) for (std::size_t i=0; i<n; ++i) for (double q : {0.25,0.5,0.75})
                master.measuredChordError=std::max(master.measuredChordError,segmentDistance(
                    curve->Value(master.parameters[i]+(master.parameters[i+1]-master.parameters[i])*q),
                    d->nodes[master.nodes[i]].xyz,d->nodes[master.nodes[i+1]].xyz));
            d->masters.push_back(std::move(master));
        }
        for (const auto& o : d->occurrences) {
            const auto& e=d->edges[o.edge]; const auto& m=d->masters[o.edge];
            OccurrenceView view; view.occurrence=o.id;
            // Preserve a UV path even when its spatial edge collapses to a pole.
            const std::size_t n=e.degenerate ? options.minimumSegments : m.nodes.size()-1;
            for (std::size_t i=0;i<=n;++i) {
                const std::size_t j=o.reversed ? n-i : i;
                const bool matchingRange=e.degenerate || (std::abs(e.firstParameter-o.firstParameter)<=1.e-10 && std::abs(e.lastParameter-o.lastParameter)<=1.e-10);
                double t=e.degenerate || !matchingRange ? o.firstParameter+(o.lastParameter-o.firstParameter)*double(j)/n : m.parameters[j];
                const auto mappedParameter=[&](Id index){return o.firstParameter+(o.lastParameter-o.firstParameter)*(m.parameters[index]-e.firstParameter)/(e.lastParameter-e.firstParameter);};
                if(!e.degenerate&&!matchingRange&&arcLengthEdges.count(e.id))t=mappedParameter(j);
                if (!matchingRange && !options.allowLocalParameterCorrection) {
                    fail("ParameterMapMismatch", "pcurve range differs from master",e,0,0,o.id); break;
                }
                const Id node=e.degenerate ? e.firstVertex : m.nodes[j];
                const auto evaluate=[&](double parameter) {
                    const auto uv=d->pcurves[o.id]->Value(parameter);
                    return d->surfaces[o.face]->Value(uv.X(),uv.Y()).Transformed(d->locations[o.face].Transformation());
                };
                const double nominal=t, nominalResidual=evaluate(t).Distance(d->nodes[node].xyz);
                double budget=std::max(e.tolerance,d->faces[o.face].tolerance);
                if (node<d->vertices.size()) budget=std::max(budget,d->vertices[node].tolerance);
                budget+=options.numericalTolerance;
                if (options.allowLocalParameterCorrection && !e.degenerate && j>0 && j<n &&
                    (nominalResidual>budget||(arcLengthEdges.count(e.id)&&nominalResidual>options.numericalTolerance))) {
                    const double lo=arcLengthEdges.count(e.id)?t+.49*(mappedParameter(j-1)-t):o.firstParameter+(o.lastParameter-o.firstParameter)*(double(j)-0.49)/n;
                    const double hi=arcLengthEdges.count(e.id)?t+.49*(mappedParameter(j+1)-t):o.firstParameter+(o.lastParameter-o.firstParameter)*(double(j)+0.49)/n;
                    t=fitParameter(lo,hi,nominal,d->nodes[node].xyz,evaluate);
                }
                const auto uv=d->pcurves[o.id]->Value(t);
                const double r=evaluate(t).Distance(d->nodes[node].xyz);
                view.samples.push_back({node,t,uv,r,budget,nominal,nominalResidual,t!=nominal});
            }
            d->views.push_back(std::move(view));
        }
        if(options.allowCadToleranceReconciliation) for(const auto& e:d->edges) {
            if(e.degenerate)continue;
            const auto& master=d->masters[e.id];
            for(Id j=1;j+1<master.nodes.size();++j) {
                auto& node=d->nodes[master.nodes[j]];
                struct Ball {gp_Pnt center;double radius;};
                std::vector<Ball> balls{{node.cadXYZ,e.tolerance+options.numericalTolerance}};
                bool needed=false,complete=true;
                for(Id oi:e.occurrences) {
                    const auto& o=d->occurrences[oi];const auto& view=d->views[oi];
                    if(view.samples.size()!=master.nodes.size()){complete=false;break;}
                    const auto& sample=view.samples[o.reversed?master.nodes.size()-1-j:j];
                    const auto p=d->surfaces[o.face]->Value(sample.uv.X(),sample.uv.Y()).Transformed(d->locations[o.face].Transformation());
                    balls.push_back({p,sample.budget});needed=needed||sample.residual>sample.budget;
                }
                if(!complete||!needed)continue;
                if(!std::all_of(balls.begin(),balls.end(),[](const Ball& ball){
                    return std::isfinite(ball.center.X())&&std::isfinite(ball.center.Y())&&std::isfinite(ball.center.Z())&&
                        std::isfinite(ball.radius)&&ball.radius>=0;
                }))continue;
                node.reconciliationAttempted=true;
                // Deterministic ordering independent of face traversal. Cyclic
                // projections seek feasibility, not a guessed new CAD edge.
                std::sort(balls.begin(),balls.end(),[](const Ball& a,const Ball& b){
                    for(int k=1;k<=3;++k)if(a.center.Coord(k)!=b.center.Coord(k))return a.center.Coord(k)<b.center.Coord(k);
                    return a.radius<b.radius;});
                auto candidate=node.cadXYZ;bool accepted=false;
                for(unsigned iteration=1;iteration<=128;++iteration) {
                    node.reconciliationIterations=iteration;
                    for(const auto& ball:balls) {
                        const double distance=ball.center.Distance(candidate);
                        if(distance>ball.radius) {
                            // Project slightly INSIDE the unchanged budget;
                            // a radius-relative epsilon alone is too small at
                            // large world coordinates to survive XYZ rounding.
                            const double scale=std::max({1.,std::abs(ball.center.X()),std::abs(ball.center.Y()),std::abs(ball.center.Z())});
                            const double guard=std::min(ball.radius*.001,64*std::numeric_limits<double>::epsilon()*scale);
                            candidate=ball.center.Translated(gp_Vec(ball.center,candidate)*((ball.radius-guard)/distance));
                        }
                    }
                    accepted=std::all_of(balls.begin(),balls.end(),[&](const Ball& ball){return candidate.Distance(ball.center)<=ball.radius;});
                    if(accepted)break;
                }
                if(accepted)node.xyz=candidate; // publish only a feasible common point
            }
        }
        // Residuals refer to the final shared representative; original curve
        // positions and nominal residuals remain available for auditing.
        for(auto& view:d->views) {
            const auto& o=d->occurrences[view.occurrence];const auto& e=d->edges[o.edge];
            for(auto& sample:view.samples) {
                const auto p=d->surfaces[o.face]->Value(sample.uv.X(),sample.uv.Y()).Transformed(d->locations[o.face].Transformation());
                sample.residual=p.Distance(d->nodes[sample.node].xyz);
                if(!std::isfinite(sample.residual)||!std::isfinite(sample.budget)||sample.budget<0||sample.residual>sample.budget)
                    fail("CadRepresentationMismatch","Master differs from occurrence surface beyond CAD budget",e,sample.residual,sample.budget,o.id);
            }
        }
        if(options.allowCadToleranceReconciliation) for(auto& master:d->masters) {
            const auto& e=d->edges[master.edge];if(e.degenerate)continue;
            BRepAdaptor_Curve curve(d->cadEdges[e.id]);master.measuredChordError=0;
            for(Id i=0;i+1<master.nodes.size();++i) {
                const double length=d->nodes[master.nodes[i]].xyz.Distance(d->nodes[master.nodes[i+1]].xyz);
                if(length>options.maxSegmentLength)fail("ReconciledLengthBudgetExceeded","Common representative exceeds segment length",e,length,options.maxSegmentLength);
            }
            for(Id i=0;i+1<master.nodes.size();++i)for(double q:{0.25,0.5,0.75}) {
                const double error=segmentDistance(curve.Value(master.parameters[i]+q*(master.parameters[i+1]-master.parameters[i])),
                    d->nodes[master.nodes[i]].xyz,d->nodes[master.nodes[i+1]].xyz);
                master.measuredChordError=std::max(master.measuredChordError,error);
                const double anchorBudget=(i==0?d->vertices[e.firstVertex].tolerance:0)+
                    (i+2==master.nodes.size()?d->vertices[e.lastVertex].tolerance:0);
                if(error>options.chordTolerance+anchorBudget)
                    fail("ReconciledChordBudgetExceeded","Common representative exceeds requested chord error",e,error,options.chordTolerance+anchorBudget);
            }
        }
        for (const auto& w:d->wires) for (Id i=0;i<w.occurrences.size();++i) {
            const Id a=w.occurrences[i], b=w.occurrences[(i+1)%w.occurrences.size()];
            if (d->views[a].samples.empty() || d->views[b].samples.empty()) continue;
            if (d->views[a].samples.back().node != d->views[b].samples.front().node) {
                fail("BoundaryContractViolation", "Prepared wire does not close by node identity",
                    d->edges[d->occurrences[a].edge],0,0,a);
            }
            const auto p=d->views[a].samples.back().uv, q=d->views[b].samples.front().uv;
            const auto& f=d->faces[w.face]; UVTransition tr; tr.fromOccurrence=a; tr.toOccurrence=b;
            const double du=p.X()-q.X(),dv=p.Y()-q.Y();
            const double up=f.uPeriod ? du/f.uPeriod : 0, vp=f.vPeriod ? dv/f.vPeriod : 0;
            if (!std::isfinite(du) || !std::isfinite(dv) || std::abs(up)>1.e12 || std::abs(vp)>1.e12) {
                fail("InvalidUVTransition", "Non-finite or unrepresentable periodic transition",
                    d->edges[d->occurrences[a].edge],0,0,a);
                continue;
            }
            tr.uPeriods=f.uPeriod ? std::llround(du/f.uPeriod) : 0;
            tr.vPeriods=f.vPeriod ? std::llround(dv/f.vPeriod) : 0;
            tr.uRemainder=du-tr.uPeriods*f.uPeriod; tr.vRemainder=dv-tr.vPeriods*f.vPeriod;
            d->transitions.push_back(tr);
        }
    } catch (const Standard_Failure& e) { d->issues.push_back({"CadException",e.GetMessageString() ? e.GetMessageString() : "OCCT failure"}); }
    d->ready=d->issues.empty();
    return result();
}

bool BoundarySnapshot::matchesBoundary(Id id, const std::vector<Id>& ids,
    const std::vector<gp_Pnt>& xyz, double tolerance) const {
    if (!data_->ready || id>=data_->views.size() || !std::isfinite(tolerance) || tolerance<0) return false;
    const auto& samples=data_->views[id].samples;
    if (ids.size()!=samples.size() || xyz.size()!=samples.size()) return false;
    for (Id i=0;i<samples.size();++i) {
        const double distance=xyz[i].Distance(data_->nodes[samples[i].node].xyz);
        if (ids[i]!=samples[i].node || !std::isfinite(distance) || distance>tolerance) return false;
    }
    return true;
}

std::shared_ptr<const BoundarySnapshot> BoundaryCache::topology(const TopoDS_Shape& shape) {
    if (topology_ && source_.IsEqual(shape)) return topology_;
    std::vector<TopoDS_Face> faces;
    if (!shape.IsNull()) for (TopExp_Explorer it(shape,TopAbs_FACE);it.More();it.Next())
        faces.push_back(TopoDS::Face(it.Current()));
    auto snapshot=BoundarySnapshot::Capture(faces,++revision_);
    source_=shape; topology_=std::move(snapshot); prepared_.reset(); ++captureCount_;
    return topology_;
}
std::shared_ptr<const BoundarySnapshot> BoundaryCache::prepare(const TopoDS_Shape& shape, const SamplingOptions& o) {
    const auto snapshot=topology(shape);
    const bool equal=o.chordTolerance==options_.chordTolerance && o.maxSegmentLength==options_.maxSegmentLength &&
        o.numericalTolerance==options_.numericalTolerance && o.minimumSegments==options_.minimumSegments &&
        o.maximumSegments==options_.maximumSegments && o.samplingRevision==options_.samplingRevision &&
        o.allowLocalParameterCorrection==options_.allowLocalParameterCorrection &&
        o.allowCadToleranceReconciliation==options_.allowCadToleranceReconciliation &&
        o.maximumChartRefinementPasses==options_.maximumChartRefinementPasses &&
        o.minimumSegmentsByEdge==options_.minimumSegmentsByEdge && o.equalSegmentGroups==options_.equalSegmentGroups &&
        o.equalSegmentSums==options_.equalSegmentSums && o.arcLengthEdges==options_.arcLengthEdges && o.stations==options_.stations;
    if (prepared_ && equal) return prepared_;
    auto prepared=BoundarySnapshot::Prepare(*snapshot,o);
    prepared_=std::move(prepared); options_=o; ++prepareCount_;
    return prepared_;
}
void BoundaryCache::invalidate() { topology_.reset(); prepared_.reset(); source_.Nullify(); }
}
