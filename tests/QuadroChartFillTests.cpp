#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "solid/QuadroChartMesher.h"
#include "solid/QuadroPatchMesh.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>
#include <QTextStream>
#include <iostream>
#include <stdexcept>
#include <map>
#include <set>

void DiagnoseHairdryerChartFill(const char* path,const char* output) {
    CAlfaDoc doc;Dom3DProjectSerializer serializer;QString room,error;ProjectViewState view;
    if(!serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error))throw std::runtime_error("Cannot load hairdryer");
    CSolid* solid=nullptr;
    for(const auto& o:doc.GetObjects())if(auto* s=dynamic_cast<CSolid*>(o.get());s&&s->GetName()=="Imported STEP 4")solid=s;
    if(!solid)throw std::runtime_error("Missing hairdryer body");
    quadro::SamplingOptions options;options.allowCadToleranceReconciliation=true;options.maximumChartRefinementPasses=8;
    auto snapshot=solid->PrepareQuadroBoundary(options);
    const auto bodyPlan=quadro::PlanLogicalBoundary(snapshot,options);
    const QString dir=QString::fromLocal8Bit(output);QDir().mkpath(dir);
    QJsonArray constraints,minimums;
    for(const auto& c:bodyPlan.sampling.equalSegmentSums){QJsonArray left,right;for(auto e:c.first)left.append(double(e));for(auto e:c.second)right.append(double(e));constraints.append(QJsonArray{left,right});}
    for(auto c:bodyPlan.sampling.minimumSegmentsByEdge)minimums.append(double(c));
    QSaveFile planFile(dir+"/sampling-plan.json");if(!planFile.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot save sampling plan");
    planFile.write(QJsonDocument(QJsonObject{{"constraints",constraints},{"minimums",minimums}}).toJson());if(!planFile.commit())throw std::runtime_error("Cannot commit sampling plan");
    snapshot=solid->PrepareQuadroBoundary(bodyPlan.sampling);
    if(!snapshot->discretizationReady())throw std::runtime_error(snapshot->issues().empty()?"Boundary not ready":snapshot->issues()[0].code+": "+snapshot->issues()[0].message);
    QJsonArray boundaryFits;
    for(auto face:{47u,58u}) {
        double residual=0,normalResidual=0;
        const auto input=quadro::BuildFaceBoundaryInput(snapshot,face);
        for(const auto& loop:input.loops)for(const auto& v:loop.vertices){const auto& p=snapshot->nodes()[v.node].xyz;residual=std::max(residual,p.Distance(snapshot->evaluateSurface(face,v.uv)));normalResidual=std::max(normalResidual,snapshot->projectedSurfaceDistance(face,p));}
        boundaryFits.append(QJsonObject{{"face",int(face)},{"maximumResidual",residual},{"maximumProjectionDistance",normalResidual},{"faceTolerance",snapshot->faces()[face].tolerance}});
    }
    QSaveFile fitFile(dir+"/boundary-fit.json");if(!fitFile.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot write fit diagnostics");fitFile.write(QJsonDocument(boundaryFits).toJson());if(!fitFile.commit())throw std::runtime_error("Cannot commit fit diagnostics");
    QJsonArray reports;unsigned donors=0,accepted=0;
    std::vector<quadro::StructuredPatch> patches;
    for(const auto& face:snapshot->faces()) {
        std::cout<<"F"<<face.id<<" filling"<<std::endl;
        const auto chart=quadro::BuildFaceBoundaryInput(snapshot,face.id);
        const auto plan=quadro::PlanLogicalCadSides(chart);
        QJsonArray plannedCorners,plannedCounts,plannedEdges;
        if(plan.ready)for(unsigned k=0;k<4;++k){plannedCorners.append(double(plan.corners[k]));QJsonArray edges;std::size_t count=0;for(auto e:plan.edges[k]){edges.append(double(e));count+=snapshot->masters()[e].nodes.size()-1;}plannedEdges.append(edges);plannedCounts.append(double(count));}
        const auto result=quadro::FillCadChart(chart,bodyPlan.cornerOccurrences[face.id]);
        if(result.ready()) {
            quadro::StructuredPatch patch;patch.face=face.id;patch.uv=result.uv;patch.xyz=result.xyz;patch.masterNodes=result.masterNodes;patch.quads=result.quads;patches.push_back(std::move(patch));
            using E=std::pair<quadro::Id,quadro::Id>;
            std::map<E,unsigned> expected,actual;
            std::map<E,std::pair<unsigned,E>> edges;
            for(const auto& loop:chart.loops)for(std::size_t i=0;i<loop.vertices.size();++i) {
                auto a=loop.vertices[i].node,b=loop.vertices[(i+1)%loop.vertices.size()].node;
                if(chart.reversed)std::swap(a,b);++expected[{a,b}];
            }
            for(const auto& q:result.quads)for(std::size_t i=0;i<4;++i) {
                auto a=q[i],b=q[(i+1)%4];auto& edge=edges[std::minmax(a,b)];++edge.first;edge.second={a,b};
            }
            for(const auto& [key,use]:edges)if(use.first==1) {
                const auto a=result.masterNodes[use.second.first],b=result.masterNodes[use.second.second];
                if(a==quadro::invalidId||b==quadro::invalidId)throw std::runtime_error("Open auxiliary cut or unmapped boundary");
                if(result.xyz[use.second.first].Distance(snapshot->nodes()[a].xyz)!=0||result.xyz[use.second.second].Distance(snapshot->nodes()[b].xyz)!=0)
                    throw std::runtime_error("Output changed master XYZ");
                ++actual[{a,b}];
            }
            if(actual!=expected)throw std::runtime_error("Output changed oriented CAD boundary identity");
        }
        const bool donor=result.donorReady;
        donors+=donor;accepted+=result.ready();
        QJsonArray uv,xyz,triangles,quads,sourceUV,sourceXYZ,sourceNodes,attempts,candidateUV,candidateCells,patchContours,logicalCorners;
        for(auto i:result.logicalCorners)logicalCorners.append(double(i));
        for(const auto& attempt:result.attempts)attempts.append(QString::fromStdString(attempt));
        for(const auto& p:result.sourceUV)sourceUV.append(QJsonArray{p.X(),p.Y()});
        for(std::size_t i=0;i<result.sourceNodes.size();++i) {
            const auto n=result.sourceNodes[i];sourceNodes.append(n==quadro::invalidId?QJsonValue():QJsonValue(double(n)));
            const auto p=n==quadro::invalidId?snapshot->evaluateSurface(face.id,result.sourceUV[i]):snapshot->nodes()[n].xyz;
            sourceXYZ.append(QJsonArray{p.X(),p.Y(),p.Z()});
        }
        for(const auto& p:result.candidateUV)candidateUV.append(QJsonArray{p.X(),p.Y()});
        for(const auto& cell:result.candidateCells){QJsonArray ids;for(auto v:cell)ids.append(double(v));candidateCells.append(ids);}
        for(const auto& patch:result.patchContours){QJsonArray contour;for(const auto& p:patch)contour.append(QJsonArray{p.X(),p.Y()});patchContours.append(contour);}
        for(const auto& p:result.uv)uv.append(QJsonArray{p.X(),p.Y()});
        for(const auto& p:result.xyz)xyz.append(QJsonArray{p.X(),p.Y(),p.Z()});
        for(const auto& t:result.triangles)triangles.append(QJsonArray{double(t[0]),double(t[1]),double(t[2])});
        if(result.ready())for(const auto& q:result.quads)quads.append(QJsonArray{double(q[0]),double(q[1]),double(q[2]),double(q[3])});
        reports.append(QJsonObject{{"face",int(face.id)},{"loops",int(chart.loops.size())},{"stage",QString::fromStdString(result.stage)},
            {"error",QString::fromStdString(result.error)},{"sourceUV",sourceUV},{"sourceNodes",sourceNodes},
            {"donorReady",donor},{"triangles",triangles},{"uv",uv},{"xyz",xyz},{"quads",quads},{"ready",result.ready()},
            {"strategy",QString::fromStdString(result.strategy)},{"attempts",attempts},{"patchCount",int(result.patchCount)},
            {"sourceXYZ",sourceXYZ},{"candidateUV",candidateUV},{"candidateCells",candidateCells},{"patchContours",patchContours},{"logicalCorners",logicalCorners},
            {"failedPatch",result.failedPatch==quadro::invalidId?QJsonValue():QJsonValue(double(result.failedPatch))},
            {"plannedCorners",plannedCorners},{"plannedCounts",plannedCounts},{"plannedEdges",plannedEdges},{"maximumSurfaceDeviation",result.maximumSurfaceDeviation}});
        std::cout<<"F"<<face.id<<" stage="<<result.stage<<" donor="<<result.triangles.size()<<" quads="<<(result.ready()?result.quads.size():0)<<" error="<<result.error<<std::endl;
        // Checkpoint diagnostics after every face; a failed consumer cannot
        // erase successfully captured input for earlier faces.
        QSaveFile file(dir+"/report.json");
        if(!file.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open chart report");
        const auto bytes=QJsonDocument(QJsonObject{{"faces",reports},{"donors",int(donors)},{"accepted",int(accepted)},{"meshingConnected",false}}).toJson(QJsonDocument::Compact);
        if(file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error("Cannot save chart report");
    }
    std::cout<<"Donors "<<donors<<"/85, admitted quad faces "<<accepted<<"/85\n";
    if(donors!=85)throw std::runtime_error("CAD donor coverage regression");
    if(accepted!=85)throw std::runtime_error("Complete hairdryer quad mesh not ready");
    if(solid->GetQuadroBoundaryCaptureCount()!=1)throw std::runtime_error("Consumer recaptured CAD topology");
    std::string bodyError;
    if(!quadro::ValidateClosedPatchBody(*snapshot,patches,bodyError))throw std::runtime_error(bodyError);
    std::map<quadro::Id,quadro::Id> shared;
    std::vector<gp_Pnt> vertices;std::vector<std::array<quadro::Id,4>> cells;
    for(const auto& patch:patches) {
        std::vector<quadro::Id> indices(patch.xyz.size());
        for(std::size_t i=0;i<indices.size();++i) {
            const auto node=patch.masterNodes[i];
            if(node!=quadro::invalidId){auto it=shared.find(node);if(it!=shared.end()){indices[i]=it->second;continue;}shared[node]=vertices.size();}
            indices[i]=vertices.size();vertices.push_back(patch.xyz[i]);
        }
        for(const auto& q:patch.quads) {
            std::array<quadro::Id,4> cell;gp_Pnt points[4];
            for(unsigned k=0;k<4;++k){cell[k]=indices[q[k]];const auto& p=vertices[cell[k]];points[k]=gp_Pnt(float(p.X()),float(p.Y()),float(p.Z()));}
            const gp_Vec a(points[0],points[1]),b(points[0],points[2]),c(points[0],points[3]);
            if(!(a.Crossed(b).Dot(b.Crossed(c))>0))throw std::runtime_error("Float storage folds a hairdryer quad");
            cells.push_back(cell);
        }
    }
    QSaveFile meshFile(dir+"/Hairdryer_Quadro.obj");if(!meshFile.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open complete mesh output");
    QTextStream mesh(&meshFile);mesh.setRealNumberPrecision(17);mesh<<"# Validated complete CAD-boundary quad mesh\no Hairdryer_Quadro\n";
    for(const auto& p:vertices)mesh<<"v "<<p.X()<<' '<<p.Y()<<' '<<p.Z()<<'\n';
    for(const auto& q:cells)mesh<<"f "<<qulonglong(q[0]+1)<<' '<<qulonglong(q[1]+1)<<' '<<qulonglong(q[2]+1)<<' '<<qulonglong(q[3]+1)<<'\n';
    mesh.flush();if(mesh.status()!=QTextStream::Ok||!meshFile.commit())throw std::runtime_error("Cannot save complete quad mesh");
    std::cout<<"Closed body: "<<vertices.size()<<" vertices, "<<cells.size()<<" quads, float storage validated\n";
    QSaveFile finalReport(dir+"/report.json");
    if(!finalReport.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open final report");
    const auto finalBytes=QJsonDocument(QJsonObject{{"faces",reports},{"donors",int(donors)},{"accepted",int(accepted)},
        {"meshingConnected",false},{"bodyClosed",true},{"floatStorageValidated",true},
        {"vertices",double(vertices.size())},{"quads",double(cells.size())},{"topologyCaptures",int(solid->GetQuadroBoundaryCaptureCount())}}).toJson(QJsonDocument::Compact);
    if(finalReport.write(finalBytes)!=finalBytes.size()||!finalReport.commit())throw std::runtime_error("Cannot commit final report");
}
