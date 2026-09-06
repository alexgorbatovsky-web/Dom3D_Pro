#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "solid/QuadroBoundary.h"
#include "solid/QuadroFaceBoundary.h"
#include "QuadroBoundaryReport.h"
#include <iostream>
#include <stdexcept>

void TestHairdryerCadBoundary(const char* path) {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    CAlfaDoc document;Dom3DProjectSerializer serializer;QString room,error;ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),document,room,view,error),"Cannot load hairdryer fixture");
    CSolid* solid=nullptr;
    for(const auto& object:document.GetObjects())if(auto* s=dynamic_cast<CSolid*>(object.get());s&&s->GetName()=="Imported STEP 4")solid=s;
    check(solid,"Hairdryer body missing");
    const auto topology=solid->GetQuadroTopologySnapshot();
    check(topology->topologyValid()&&topology->faces().size()==85,"Hairdryer topology changed");
    for(double chord:{0.01,0.005}) {
        quadro::SamplingOptions options;options.chordTolerance=chord;
        options.maxSegmentLength=chord==0.01?5:2.5;
        auto strict=solid->PrepareQuadroBoundary(options);
        check(!strict->discretizationReady(),"Strict diagnostic baseline unexpectedly changed");
        options.allowCadToleranceReconciliation=true;
        options.maximumChartRefinementPasses=8;
        const auto prepared=solid->PrepareQuadroBoundary(options);
        check(prepared==solid->PrepareQuadroBoundary(options),"Prepared boundary not cached");
        auto control=options;control.allowCadToleranceReconciliation=false;control.maximumChartRefinementPasses=0;
        control.minimumSegmentsByEdge.resize(prepared->edges().size());
        for(const auto& master:prepared->masters())control.minimumSegmentsByEdge[master.edge]=master.nodes.size()-1;
        strict=quadro::BoundarySnapshot::Prepare(*topology,control);
        const auto reportPrefix=qEnvironmentVariable("DOM3D_BOUNDARY_TEST_REPORT");
        if(!reportPrefix.isEmpty()) {
            WriteQuadroBoundaryReport(strict,reportPrefix+"-strict-"+QString::number(chord)+".json");
            WriteQuadroBoundaryReport(prepared,reportPrefix+"-reconciled-"+QString::number(chord)+".json");
        }
        check(prepared->discretizationReady(),"Hairdryer common CAD boundary is not ready");
        check(strict->nodes().size()==prepared->nodes().size(),"Reconciliation changed master sampling");
        std::size_t moved=0;double maxMovement=0;
        for(const auto& edge:prepared->edges()) {
            const auto& master=prepared->masters()[edge.id];
            check(master.nodes==strict->masters()[edge.id].nodes,"Reconciliation changed node identity or order");
            check(edge.tolerance==strict->edges()[edge.id].tolerance,"CAD edge tolerance changed");
            for(auto id:master.nodes) {
                const auto& n=prepared->nodes()[id];
                check(n.cadXYZ.Distance(strict->nodes()[id].xyz)==0,"Lost original CAD curve point");
                const double distance=n.xyz.Distance(n.cadXYZ);
                check(distance<=edge.tolerance+options.numericalTolerance,"Node escaped CAD edge envelope");
                if(id<prepared->vertices().size())check(distance==0,"CAD vertex moved");
            }
        }
        for(const auto& n:prepared->nodes())if(n.xyz.Distance(n.cadXYZ)>0){++moved;maxMovement=std::max(maxMovement,n.xyz.Distance(n.cadXYZ));}
        check(moved>0,"Hairdryer did not exercise reconciliation");
        for(const auto& occurrence:prepared->occurrences()) {
            const auto& samples=prepared->views()[occurrence.id].samples;
            const auto& original=strict->views()[occurrence.id].samples;
            check(samples.size()==original.size(),"Occurrence lost samples");
            for(std::size_t i=0;i<samples.size();++i) {
                check(samples[i].uv.Distance(original[i].uv)==0&&samples[i].parameter==original[i].parameter,"Stored pcurve sample changed");
                check(samples[i].budget==original[i].budget,"Occurrence tolerance budget increased");
                check(prepared->evaluateSurface(occurrence.face,samples[i].uv).Distance(prepared->nodes()[samples[i].node].xyz)<=samples[i].budget,"Occurrence escaped original tolerance");
            }
        }
        for(const auto& wire:prepared->wires())for(std::size_t i=0;i<wire.occurrences.size();++i)
            check(prepared->views()[wire.occurrences[i]].samples.back().node==prepared->views()[wire.occurrences[(i+1)%wire.occurrences.size()]].samples.front().node,"CAD wire opened");
        for(const auto& face:prepared->faces()) {
            const auto chart=quadro::BuildFaceBoundaryInput(prepared,face.id);
            if(!chart.ready){std::cerr<<"Rejected face "<<face.id<<'\n';for(const auto& issue:chart.issues)std::cerr<<issue.code<<'\n';}
            check(chart.ready,"Hairdryer chart rejected");
            check(chart.loops.size()==face.wires.size(),"CAD wire disappeared");
            for(const auto& loop:chart.loops)for(const auto& vertex:loop.vertices) {
                const auto& sample=prepared->views()[vertex.occurrence].samples[vertex.sample];
                check(vertex.node==sample.node,"Chart lost CAD node identity");
                check(prepared->evaluateSurface(face.id,vertex.uv).Distance(prepared->nodes()[vertex.node].xyz)<=sample.budget,"Chart corner escaped CAD tolerance");
            }
        }
        const auto hole=quadro::BuildFaceBoundaryInput(prepared,15);
        check(quadro::ContainsUV(hole,gp_Pnt2d(4,15))&&!quadro::ContainsUV(hole,gp_Pnt2d(7.854,15.45)),"Hairdryer hole became filled area");
        check(solid->GetQuadroTopologySnapshot()==topology&&solid->GetQuadroBoundaryCaptureCount()==1,"Reconciliation recaptured CAD body");
        std::cout<<"Hairdryer chord="<<chord<<" charts=85/85 moved="<<moved<<" maxMovement="<<maxMovement<<" refinedEdges="<<prepared->chartRefinements().size()<<" captures=1\n";
    }
    CSolid* second=nullptr;
    for(const auto& object:document.GetObjects())if(auto* s=dynamic_cast<CSolid*>(object.get());s&&s->GetName()=="Imported STEP 13")second=s;
    check(second,"Second hairdryer shell missing");
    quadro::SamplingOptions secondOptions;secondOptions.allowCadToleranceReconciliation=true;secondOptions.maximumChartRefinementPasses=8;
    const auto secondBoundary=second->PrepareQuadroBoundary(secondOptions);
    check(secondBoundary->faces().size()==283&&secondBoundary->discretizationReady(),"Second shell boundary rejected");
    for(const auto& face:secondBoundary->faces())check(quadro::BuildFaceBoundaryInput(secondBoundary,face.id).ready,"Second shell chart rejected");
    std::cout<<"Hairdryer second shell charts=283/283\n";
}
