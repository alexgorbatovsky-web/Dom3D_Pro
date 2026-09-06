#include "solid/QuadroBoundary.h"
#include "solid/QuadroFaceBoundary.h"
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom2d_BezierCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_Plane.hxx>
#include <TopoDS_Wire.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace quadro;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static std::vector<TopoDS_Face> faces(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Face> result;
    for (TopExp_Explorer it(shape,TopAbs_FACE);it.More();it.Next()) result.push_back(TopoDS::Face(it.Current()));
    return result;
}
static void valid(const BoundarySnapshot& s, bool prepared) {
    for (const auto& issue:s.issues()) std::cerr << issue.code << ": " << issue.message << " r=" << issue.residual << " b=" << issue.budget << '\n';
    require(s.topologyValid(),"Topology invalid");
    if (prepared) require(s.discretizationReady(),"Sampling invalid");
    for (const auto& w:s.wires()) for (Id i=0;i<w.occurrences.size();++i) {
        const auto& a=s.occurrences()[w.occurrences[i]];
        const auto& b=s.occurrences()[w.occurrences[(i+1)%w.occurrences.size()]];
        require(a.lastVertex==b.firstVertex,"Wire identity is not closed");
        if (prepared) require(s.views()[a.id].samples.back().node==s.views()[b.id].samples.front().node,"Wire node identity is not closed");
    }
}
int main() {
    try {
        auto box=BRepPrimAPI_MakeBox(10,20,30).Shape(); auto fs=faces(box);
        for (const auto& source : fs) for (bool reverse : {false,true}) {
            auto occurrence=source;
            if(reverse)occurrence.Reverse();
            const auto captured=BoundarySnapshot::Capture({occurrence},1);
            require(captured->faces()[0].reversed==(occurrence.Orientation()==TopAbs_REVERSED),
                "CAD copy changed source face occurrence orientation");
        }
        auto topology=BoundarySnapshot::Capture(fs,42); valid(*topology,false);
        require(topology->vertices().size()==8 && topology->edges().size()==12 && topology->occurrences().size()==24,"Box sharing lost");
        require(!topology->discretizationReady() && topology->masters().empty(),"Capture invoked sampling");
        SamplingOptions opts; opts.equalSegmentGroups={{0,1,2}}; opts.minimumSegmentsByEdge={17};
        auto prepared=BoundarySnapshot::Prepare(*topology,opts); valid(*prepared,true);
        for(const auto& face:prepared->faces())require(BuildFaceBoundaryInput(prepared,face.id).ready,"Box chart rejected");
        require(prepared->bodyRevision()==42,"Revision lost");
        require(prepared->masters()[0].nodes.size()==prepared->masters()[1].nodes.size(),"Count equality lost");
        require(topology->masters().empty(),"Preparation mutated topology snapshot");
        for (const auto& o:prepared->occurrences()) {
            std::vector<Id> ids; std::vector<gp_Pnt> xyz;
            for (const auto& s:prepared->views()[o.id].samples) { ids.push_back(s.node);xyz.push_back(prepared->nodes()[s.node].xyz); }
            require(prepared->matchesBoundary(o.id,ids,xyz,0),"Exact master boundary rejected");
            xyz[0].SetX(xyz[0].X()+0.3);
            require(!prepared->matchesBoundary(o.id,ids,xyz,1.e-9),"Donor endpoint replacement accepted");
        }
        opts.maximumSegments=8;
        require(!BoundarySnapshot::Prepare(*topology,opts)->discretizationReady(),"Count overflow accepted");
        auto shuffled=fs; std::reverse(shuffled.begin(),shuffled.end());
        auto reverse=BoundarySnapshot::Prepare(*BoundarySnapshot::Capture(shuffled,42),SamplingOptions{}); valid(*reverse,true);
        auto plain=BoundarySnapshot::Prepare(*topology,SamplingOptions{});
        // IDs are revision-local traversal IDs; compare physical master curves,
        // not numeric IDs, after permuting the faces.
        auto signature=[](const BoundarySnapshot& s) {
            std::vector<std::vector<double>> values;
            for (const auto& m:s.masters()) {
                std::vector<double> ps;
                for (Id n:m.nodes) {const auto& p=s.nodes()[n].xyz;ps.insert(ps.end(),{p.X(),p.Y(),p.Z()});}
                values.push_back(ps);
            }
            std::sort(values.begin(),values.end()); return values;
        };
        require(signature(*plain)==signature(*reverse),"Face order changes master discretization");
        for (const auto& shape: {BRepPrimAPI_MakeCylinder(10,20).Shape(),BRepPrimAPI_MakeTorus(20,5).Shape(),BRepPrimAPI_MakeSphere(10).Shape()}) {
            auto t=BoundarySnapshot::Capture(faces(shape),1); valid(*t,false);
            auto s=BoundarySnapshot::Prepare(*t,SamplingOptions{}); valid(*s,true);
            bool seam=false;
            for (const auto& a:s->occurrences()) if (a.seam) {
                for (Id id:s->edges()[a.edge].occurrences) {
                    const auto& b=s->occurrences()[id]; if (a.face!=b.face || a.reversed==b.reversed) continue;
                    const auto& av=s->views()[a.id].samples; const auto& bv=s->views()[b.id].samples;
                    require(av.size()==bv.size(),"Seam sampling mismatch");
                    for (Id i=0;i<av.size();++i) require(av[i].node==bv[av.size()-1-i].node,"Seam node identity lost");
                    require(av[av.size()/2].uv.Distance(bv[bv.size()/2].uv)>1,"Seam pcurves collapsed");seam=true;
                }
            }
            require(seam,"Seam occurrences absent");
            for(const auto& face:s->faces()) {
                bool pole=false;
                for(Id wi:face.wires)for(Id oi:s->wires()[wi].occurrences)
                    pole=pole||s->edges()[s->occurrences()[oi].edge].degenerate;
                const auto chart=BuildFaceBoundaryInput(s,face.id);
                if(!pole) require(chart.ready,"Periodic cylinder/torus chart rejected");
                else require(!chart.ready && std::any_of(chart.issues.begin(),chart.issues.end(),[](const Issue& i){return i.code=="PoleNeedsChartCut";}),"Pole accepted without explicit chart cut");
            }
        }
        // Capture owns a deep copy: later edits to the source CAD must not
        // change either stored vertex data or evaluable occurrence geometry.
        const auto before=topology->evaluatePCurve(0,topology->occurrences()[0].firstParameter);
        TopExp_Explorer vx(box,TopAbs_VERTEX); BRep_Builder b;
        b.UpdateVertex(TopoDS::Vertex(vx.Current()),gp_Pnt(999,999,999),1.e-7);
        auto after=BoundarySnapshot::Prepare(*topology,SamplingOptions{});valid(*after,true);
        require(before.Distance(topology->evaluatePCurve(0,topology->occurrences()[0].firstParameter))==0,"Snapshot geometry changed");
        // Coincident but distinct CAD bodies are not welded by position.
        auto a=faces(BRepPrimAPI_MakeBox(1,1,1).Shape());auto c=faces(BRepPrimAPI_MakeBox(1,1,1).Shape());
        a.insert(a.end(),c.begin(),c.end());auto separate=BoundarySnapshot::Capture(a,1);
        require(separate->vertices().size()==16 && separate->edges().size()==24,"Geometric coincidence merged topology");
        BoundaryCache cache;auto cacheShape=BRepPrimAPI_MakeCylinder(3,7).Shape();
        auto cached=cache.topology(cacheShape);require(cached==cache.topology(cacheShape),"Topology cache miss");
        auto sampled=cache.prepare(cacheShape,SamplingOptions{});
        require(sampled==cache.prepare(cacheShape,SamplingOptions{}),"Sampling cache miss");
        SamplingOptions finer;finer.maxSegmentLength=0.25;
        require(sampled!=cache.prepare(cacheShape,finer),"Sampling options ignored");
        BRepTools::Clean(cacheShape);
        require(cached==cache.topology(cacheShape)&&cache.captureCount()==1,"Triangulation/density recreated topology");
        gp_Trsf cacheMove;cacheMove.SetTranslation(gp_Vec(10,0,0));
        auto moved=cacheShape.Moved(TopLoc_Location(cacheMove));
        require(cached!=cache.topology(moved)&&cache.captureCount()==2,"Placement invalidation failed");
        cache.invalidate();cache.topology(moved);require(cache.captureCount()==3,"Explicit invalidation failed");

        // SameParameter is metadata, not proof: construct a pcurve on the
        // same straight edge with a slightly different speed and fixed ends.
        TColgp_Array1OfPnt poles(1,3);poles(1)=gp_Pnt(0,0,0);poles(2)=gp_Pnt(5,0,0);poles(3)=gp_Pnt(10,0,0);
        auto warpedEdge=BRepBuilderAPI_MakeEdge(new Geom_BezierCurve(poles)).Edge();
        auto wire=BRepBuilderAPI_MakeWire(warpedEdge,
            BRepBuilderAPI_MakeEdge(gp_Pnt(10,0,0),gp_Pnt(10,10,0)).Edge(),
            BRepBuilderAPI_MakeEdge(gp_Pnt(10,10,0),gp_Pnt(0,10,0)).Edge(),
            BRepBuilderAPI_MakeEdge(gp_Pnt(0,10,0),gp_Pnt(0,0,0)).Edge()).Wire();
        auto warpedFace=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),wire).Face();
        TColgp_Array1OfPnt2d uvPoles(1,3);uvPoles(1)=gp_Pnt2d(0,0);uvPoles(2)=gp_Pnt2d(5.15,0);uvPoles(3)=gp_Pnt2d(10,0);
        b.UpdateEdge(warpedEdge,new Geom2d_BezierCurve(uvPoles),warpedFace,1.e-7);b.SameParameter(warpedEdge,true);
        auto warpedTopology=BoundarySnapshot::Capture({warpedFace},1);valid(*warpedTopology,false);
        SamplingOptions strict;strict.allowLocalParameterCorrection=false;
        auto nominal=BoundarySnapshot::Prepare(*warpedTopology,strict);require(!nominal->discretizationReady(),"Warped parameter test did not fail");
        auto mapped=BoundarySnapshot::Prepare(*warpedTopology,SamplingOptions{});valid(*mapped,true);
        require(signature(*mapped)==signature(*nominal),"Parameter fitting moved master XYZ");
        bool corrected=false;
        for(const auto& v:mapped->views())for(Id i=0;i<v.samples.size();++i) {
            corrected=corrected||v.samples[i].parameterCorrected;
            if(i) require((v.samples[i].parameter>v.samples[i-1].parameter)!=mapped->occurrences()[v.occurrence].reversed,"Parameter order changed");
        }
        require(corrected,"Parameter correction not exercised");
        require(BuildFaceBoundaryInput(mapped,0).ready,"Mapped rectangular chart rejected");
        // A curve and its surface representation need a common point inside
        // BOTH original tolerance envelopes; increasing tolerance is forbidden.
        uvPoles(2)=gp_Pnt2d(5,0.03);
        b.UpdateEdge(warpedEdge,new Geom2d_BezierCurve(uvPoles),warpedFace,0.01);
        auto offsetTopology=BoundarySnapshot::Capture({warpedFace},1);
        auto offsetStrict=BoundarySnapshot::Prepare(*offsetTopology,SamplingOptions{});
        require(!offsetStrict->discretizationReady(),"Offset pcurve did not exercise tolerance reconciliation");
        SamplingOptions consensus;consensus.allowCadToleranceReconciliation=true;
        auto reconciled=BoundarySnapshot::Prepare(*offsetTopology,consensus);valid(*reconciled,true);
        bool movedMaster=false;
        for(const auto& m:reconciled->masters())for(Id id:m.nodes) {
            const auto& node=reconciled->nodes()[id];const double movement=node.xyz.Distance(node.cadXYZ);
            movedMaster=movedMaster||movement>0;
            require(movement<=reconciled->edges()[m.edge].tolerance+consensus.numericalTolerance,"Master left CAD edge envelope");
            if(id<reconciled->vertices().size())require(movement==0,"CAD vertex moved");
        }
        require(movedMaster,"No common master was fitted");
        require(signature(*offsetStrict)==signature(*BoundarySnapshot::Prepare(*offsetTopology,SamplingOptions{})),"Opt-in reconciliation mutated source snapshot");
        for(Id i=0;i<offsetStrict->views().size();++i)for(Id j=0;j<offsetStrict->views()[i].samples.size();++j)
            require(offsetStrict->views()[i].samples[j].uv.Distance(reconciled->views()[i].samples[j].uv)==0,"Reconciliation moved pcurve sample");
        uvPoles(2)=gp_Pnt2d(5,0.1);
        b.UpdateEdge(warpedEdge,new Geom2d_BezierCurve(uvPoles),warpedFace,0.01);
        auto infeasible=BoundarySnapshot::Prepare(*BoundarySnapshot::Capture({warpedFace},1),consensus);
        require(!infeasible->discretizationReady(),"Disjoint tolerance envelopes silently accepted");
        bool boundedFailure=false;
        for(const auto& node:infeasible->nodes())if(node.reconciliationIterations==128) {
            require(node.xyz.Distance(node.cadXYZ)==0,"Failed fit published partial movement");boundedFailure=true;
        }
        require(boundedFailure,"Infeasible solve did not terminate within its budget");
        auto cachedConsensus=cache.prepare(moved,consensus);
        require(cachedConsensus==cache.prepare(moved,consensus),"Reconciliation cache miss");
        require(cachedConsensus!=cache.prepare(moved,SamplingOptions{}),"Cache ignored reconciliation policy");
        BoundaryCache sumsCache;const auto sumsBox=BRepPrimAPI_MakeBox(10,10,10).Shape();
        SamplingOptions sums;sums.equalSegmentSums={{{0,1},{2}},{{3,4},{5}}};sums.equalSegmentGroups={{2,3}};
        auto balanced=sumsCache.prepare(sumsBox,sums);valid(*balanced,true);
        const auto count=[&](Id e){return balanced->masters()[e].nodes.size()-1;};
        require(count(0)+count(1)==count(2)&&count(2)==count(3)&&count(3)+count(4)==count(5),"Compound side constraints did not propagate through shared edges");
        require(balanced==sumsCache.prepare(sumsBox,sums)&&sumsCache.captureCount()==1,"Logical side plan missed topology cache");
        auto noSums=sums;noSums.equalSegmentSums.clear();
        require(balanced!=sumsCache.prepare(sumsBox,noSums)&&sumsCache.captureCount()==1,"Cache ignored compound side requirements");
        auto impossible=sums;impossible.maximumSegments=64;impossible.equalSegmentSums={{{0},{0,1}}};
        require(!sumsCache.prepare(sumsBox,impossible)->discretizationReady(),"Contradictory positive side counts were accepted");
        SamplingOptions stations;stations.equalSegmentSums={{{0},{1,2}}};stations.arcLengthEdges={0};
        const auto stationTopology=sumsCache.topology(sumsBox);const auto& stationEdge=stationTopology->edges()[0];
        const auto aStation=stationTopology->vertices()[stationEdge.firstVertex].xyz,bStation=stationTopology->vertices()[stationEdge.lastVertex].xyz;
        const gp_Pnt stationPoint(aStation.X()*.7+bStation.X()*.3,aStation.Y()*.7+bStation.Y()*.3,aStation.Z()*.7+bStation.Z()*.3);
        const double parameter=stationTopology->projectedEdgeParameter(0,stationPoint);
        stations.stations.push_back({0,parameter,{1},false});
        const auto stationSnapshot=sumsCache.prepare(sumsBox,stations);valid(*stationSnapshot,true);
        const Id stationIndex=stationSnapshot->masters()[1].nodes.size()-1;
        require(stationSnapshot->nodes()[stationSnapshot->masters()[0].nodes[stationIndex]].xyz.Distance(stationPoint)<1.e-9,"Shared station was not placed at its prescribed CAD parameter");
        require(stationSnapshot==sumsCache.prepare(sumsBox,stations)&&sumsCache.captureCount()==1,"Station plan did not reuse topology and sampling cache");
        auto conflictingStations=stations;conflictingStations.stations.push_back({0,(stationEdge.firstParameter+stationEdge.lastParameter)*.5,{1},false});
        require(!sumsCache.prepare(sumsBox,conflictingStations)->discretizationReady(),"Conflicting station parameters silently merged");
        auto rectangle=[](double lo,double hi){BRepBuilderAPI_MakePolygon p;p.Add(gp_Pnt(lo,lo,0));p.Add(gp_Pnt(hi,lo,0));p.Add(gp_Pnt(hi,hi,0));p.Add(gp_Pnt(lo,hi,0));p.Close();return p.Wire();};
        auto inner=rectangle(3,7);inner.Reverse();
        BRepBuilderAPI_MakeFace holed(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),rectangle(0,10));holed.Add(inner);
        auto holeSnapshot=BoundarySnapshot::Prepare(*BoundarySnapshot::Capture({holed.Face()},1),SamplingOptions{});valid(*holeSnapshot,true);
        const auto holeChart=BuildFaceBoundaryInput(holeSnapshot,0);
        require(holeChart.ready && holeChart.loops.size()==2,"Hole chart not preserved");
        require(!ContainsUV(holeChart,gp_Pnt2d(5,5)) && ContainsUV(holeChart,gp_Pnt2d(1,1)),"Hole promoted to filled domain");
        auto located=BRepPrimAPI_MakeBox(1,1,1).Shape(); auto originals=faces(located);
        gp_Trsf move; move.SetTranslation(gp_Vec(10,0,0));
        auto placed=faces(located.Moved(TopLoc_Location(move)));
        originals.insert(originals.end(),placed.begin(),placed.end());
        auto instances=BoundarySnapshot::Capture(originals,1); valid(*instances,false);
        require(instances->vertices().size()==16 && instances->edges().size()==24,"Different placements of one TShape merged");
        // A valid tolerance envelope may contain distinct curve endpoints.
        // Every incident edge still uses the one CAD vertex anchor.
        auto tolerant=BRepPrimAPI_MakeBox(10,10,10).Shape();
        TopExp_Explorer tv(tolerant,TopAbs_VERTEX);const auto vv=TopoDS::Vertex(tv.Current());
        const auto vp=BRep_Tool::Pnt(vv);b.UpdateVertex(vv,vp.Translated(gp_Vec(0.003,0,0)),0.01);
        auto ts=BoundarySnapshot::Prepare(*BoundarySnapshot::Capture(faces(tolerant),1),SamplingOptions{});valid(*ts,true);
        require(std::any_of(ts->views().begin(),ts->views().end(),[](const auto& v){
            return std::any_of(v.samples.begin(),v.samples.end(),[](const auto& p){return p.residual>0.002 && p.residual<=p.budget;});
        }),"Tolerance test did not exercise distinct endpoints");
        auto corrupt=BRepPrimAPI_MakeBox(10,10,10).Shape();TopExp_Explorer cv(corrupt,TopAbs_VERTEX);
        const auto bad=TopoDS::Vertex(cv.Current());b.UpdateVertex(bad,BRep_Tool::Pnt(bad).Translated(gp_Vec(0.003,0,0)),1.e-7);
        auto rejected=BoundarySnapshot::Prepare(*BoundarySnapshot::Capture(faces(corrupt),1),SamplingOptions{});
        require(!rejected->discretizationReady(),"Outside-tolerance endpoint accepted");
        require(std::any_of(rejected->issues().begin(),rejected->issues().end(),[](const Issue& i){return i.code=="CadEndpointMismatch";}),"Endpoint violation not diagnosed");
        require(!BoundarySnapshot::Capture({},1)->topologyValid(),"Empty body accepted");
        BRepBuilderAPI_MakePolygon bowTie;
        bowTie.Add(gp_Pnt(0,0,0));bowTie.Add(gp_Pnt(2,2,0));bowTie.Add(gp_Pnt(0,2,0));bowTie.Add(gp_Pnt(2,0,0));bowTie.Close();
        const auto crossedFace=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),bowTie.Wire()).Face();
        auto bounded=consensus;bounded.maximumChartRefinementPasses=1;
        auto crossed=BoundarySnapshot::Prepare(*BoundarySnapshot::Capture({crossedFace},1),bounded);
        require(!crossed->discretizationReady(),"True self-intersection accepted by chart refinement");
        require(std::any_of(crossed->issues().begin(),crossed->issues().end(),[](const Issue& issue){return issue.code=="ChartRefinementBudgetExceeded";}),"Chart refinement did not stop with an explicit budget error");
        require(!BoundarySnapshot::Capture({TopoDS_Face()},1)->topologyValid(),"Null face accepted");
        TopoDS_Wire openWire;b.MakeWire(openWire);
        const auto e1=BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0),gp_Pnt(1,0,0)).Edge();
        b.Add(openWire,e1);
        TopoDS_Face openFace;b.MakeFace(openFace,new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1)),1.e-7);b.Add(openFace,openWire);
        auto open=BoundarySnapshot::Capture({openFace},1);
        require(!open->topologyValid(),"Open CAD wire accepted");
        require(std::any_of(open->issues().begin(),open->issues().end(),[](const Issue& i){return i.code=="TopologyOpenWire";}),"Open wire not diagnosed by identity");
        std::cout << "Quadro boundary snapshot: topology, seams, masters, isolation, budgets and contracts passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
