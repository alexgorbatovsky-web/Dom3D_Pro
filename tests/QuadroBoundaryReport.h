#pragma once
#include "solid/QuadroBoundary.h"
#include "solid/QuadroFaceBoundary.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>

inline void WriteQuadroBoundaryReport(std::shared_ptr<const quadro::BoundarySnapshot> snapshot, const QString& path) {
    const auto& s=*snapshot;
    auto id=[](quadro::Id i)->QJsonValue {return i==quadro::invalidId ? QJsonValue() : QJsonValue(double(i));};
    auto point=[](const gp_Pnt& p){return QJsonArray{p.X(),p.Y(),p.Z()};};
    auto ids=[&](const auto& list){QJsonArray a;for(auto i:list)a.append(id(i));return a;};
    QJsonObject root{{"bodyRevision",QString::number(s.bodyRevision())},{"samplingRevision",QString::number(s.samplingRevision())},
        {"topologyValid",s.topologyValid()},{"discretizationReady",s.discretizationReady()},
        {"meshingConnected",false},{"chartsValidated",false},{"cadToleranceReconciliation",s.cadToleranceReconciliationEnabled()}};
    QJsonArray vertices,edges,faces,wires,occurrences,issues,nodes,masters,views,transitions;
    for(const auto& v:s.vertices())vertices.append(QJsonObject{{"id",id(v.id)},{"xyz",point(v.xyz)},{"tolerance",v.tolerance}});
    for(const auto& e:s.edges())edges.append(QJsonObject{{"id",id(e.id)},{"firstVertex",id(e.firstVertex)},{"lastVertex",id(e.lastVertex)},
        {"tolerance",e.tolerance},{"firstParameter",e.firstParameter},{"lastParameter",e.lastParameter},
        {"closed",e.closed},{"degenerate",e.degenerate},{"sameParameter",e.sameParameter},{"sameRange",e.sameRange},{"occurrences",ids(e.occurrences)}});
    for(const auto& f:s.faces())faces.append(QJsonObject{{"id",id(f.id)},{"reversed",f.reversed},{"tolerance",f.tolerance},{"uPeriod",f.uPeriod},{"vPeriod",f.vPeriod},{"wires",ids(f.wires)}});
    for(const auto& w:s.wires())wires.append(QJsonObject{{"id",id(w.id)},{"face",id(w.face)},{"outer",w.outer},{"occurrences",ids(w.occurrences)}});
    for(const auto& o:s.occurrences()) {
        QJsonArray pc;
        if(o.hasPCurve) for(int i=0;i<=16;++i) {double q=o.reversed ? 1.-i/16. : i/16.;auto p=s.evaluatePCurve(o.id,o.firstParameter+(o.lastParameter-o.firstParameter)*q);pc.append(QJsonArray{p.X(),p.Y()});}
        occurrences.append(QJsonObject{{"id",id(o.id)},{"face",id(o.face)},{"wire",id(o.wire)},{"edge",id(o.edge)},
            {"firstVertex",id(o.firstVertex)},{"lastVertex",id(o.lastVertex)},{"reversed",o.reversed},{"seam",o.seam},
            {"firstParameter",o.firstParameter},{"lastParameter",o.lastParameter},{"pcurve",pc}});
    }
    for(const auto& i:s.issues())issues.append(QJsonObject{{"code",QString::fromStdString(i.code)},{"message",QString::fromStdString(i.message)},
        {"face",id(i.face)},{"wire",id(i.wire)},{"occurrence",id(i.occurrence)},{"edge",id(i.edge)},{"residual",i.residual},{"budget",i.budget}});
    for(const auto& n:s.nodes())nodes.append(QJsonObject{{"id",id(n.id)},{"xyz",point(n.xyz)},
        {"cadXYZ",point(n.cadXYZ)},{"displacement",n.xyz.Distance(n.cadXYZ)},
        {"reconciliationAttempted",n.reconciliationAttempted},{"reconciliationIterations",int(n.reconciliationIterations)}});
    for(const auto& m:s.masters()) {QJsonArray ps;for(double p:m.parameters)ps.append(p);masters.append(QJsonObject{{"edge",id(m.edge)},{"nodes",ids(m.nodes)},{"parameters",ps},{"measuredChordError",m.measuredChordError}});}
    for(const auto& v:s.views()) {
        QJsonArray ps;
        for(const auto& p:v.samples) {
            QJsonObject sample{{"node",id(p.node)},{"parameter",p.parameter},{"uv",QJsonArray{p.uv.X(),p.uv.Y()}},
                {"residual",p.residual},{"budget",p.budget},{"nominalParameter",p.nominalParameter},
                {"nominalResidual",p.nominalResidual},{"parameterCorrected",p.parameterCorrected}};
            if(p.residual>p.budget) sample["surfaceProjectionDistance"]=s.projectedSurfaceDistance(s.occurrences()[v.occurrence].face,s.nodes()[p.node].xyz);
            ps.append(sample);
        }
        views.append(QJsonObject{{"occurrence",id(v.occurrence)},{"samples",ps}});
    }
    for(const auto& t:s.transitions())transitions.append(QJsonObject{{"from",id(t.fromOccurrence)},{"to",id(t.toOccurrence)},{"uPeriods",double(t.uPeriods)},{"vPeriods",double(t.vPeriods)},{"uRemainder",t.uRemainder},{"vRemainder",t.vRemainder}});
    root["vertices"]=vertices;root["edges"]=edges;root["faces"]=faces;root["wires"]=wires;root["occurrences"]=occurrences;
    root["issues"]=issues;root["nodes"]=nodes;root["masters"]=masters;root["views"]=views;root["transitions"]=transitions;
    QJsonArray charts; bool allChartsReady=!s.faces().empty();
    for(const auto& face:s.faces()) {
        auto chart=quadro::BuildFaceBoundaryInput(snapshot,face.id);allChartsReady=allChartsReady&&chart.ready;
        QJsonArray loops,errors;
        for(const auto& loop:chart.loops) {
            QJsonArray chartVertices;
            for(const auto& v:loop.vertices)chartVertices.append(QJsonObject{{"node",id(v.node)},{"occurrence",id(v.occurrence)},
                {"sample",id(v.sample)},{"uv",QJsonArray{v.uv.X(),v.uv.Y()}},{"cornerReconciled",v.cornerReconciled}});
            loops.append(QJsonObject{{"wire",id(loop.wire)},{"outer",loop.outer},{"signedArea",loop.signedArea},{"vertices",chartVertices}});
        }
        for(const auto& e:chart.issues)errors.append(QJsonObject{{"code",QString::fromStdString(e.code)},{"message",QString::fromStdString(e.message)},{"wire",id(e.wire)}});
        charts.append(QJsonObject{{"face",id(face.id)},{"ready",chart.ready},{"reversed",chart.reversed},{"loops",loops},{"issues",errors}});
    }
    root["charts"]=charts;root["chartsValidated"]=allChartsReady;
    QJsonArray refinements;
    for(const auto& r:s.chartRefinements())refinements.append(QJsonObject{{"pass",int(r.pass)},{"edge",id(r.edge)},
        {"previousSegments",double(r.previousSegments)},{"nextSegments",double(r.nextSegments)}});
    root["chartRefinements"]=refinements;
    QSaveFile file(path);const auto bytes=QJsonDocument(root).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()) throw std::runtime_error("Cannot save boundary snapshot report");
}
