#pragma once

// Opt-in, test-runner-only observer. Replays joining on copies; never edits CAD
// or feeds the replay results back into production meshing.
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <BRepTools_WireExplorer.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <stdexcept>
#include <TopExp.hxx>
#include <Geom2d_Curve.hxx>

inline void ExportQuadroPipelineDiagnostics(CSolid& solid, const char* phase, float density)
{
    const auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
    const QString directory = QString::fromLocal8Bit(qgetenv("DOM3D_PIPELINE_DUMP"));
    if (directory.isEmpty()) return;
    require(QDir().mkpath(directory), "Cannot create pipeline diagnostic directory.");
    const auto point = [](const auto& p) { return QJsonArray{p.x, p.y, p.z}; };
    const auto points = [&](const auto& ps) {
        QJsonArray a; for (const auto& p : ps) a.append(point(p)); return a;
    };
    QJsonArray surfaces;
    for (int id = 0; id < solid.GetNumSurfaces(); ++id) {
        auto* s = solid.GetSurfaceFace(id);
        if (!s) continue;
        QJsonObject o{{"id", id}, {"error", QString::fromStdString(s->GetLastIslandFillError())}};
        const auto face = TopoDS::Face(s->m_Face);
        BRepAdaptor_Surface adaptor(face);
        o["geometry"] = int(adaptor.GetType());
        o["cadValid"] = BRepCheck_Analyzer(face).IsValid();
        o["uPeriodic"] = adaptor.IsUPeriodic();
        o["vPeriodic"] = adaptor.IsVPeriodic();
        o["uvBounds"] = QJsonArray{adaptor.FirstUParameter(), adaptor.LastUParameter(),
            adaptor.FirstVParameter(), adaptor.LastVParameter()};
        GProp_GProps cadProperties;
        BRepGProp::SurfaceProperties(face, cadProperties);
        o["cadArea"] = cadProperties.Mass();
        QJsonArray topology;
        QJsonArray wires;
        for (TopExp_Explorer wi(face, TopAbs_WIRE); wi.More(); wi.Next()) {
            QJsonArray edges;
            QJsonArray topoEdges;
            for (BRepTools_WireExplorer ei(TopoDS::Wire(wi.Current()), face); ei.More(); ei.Next()) {
                BRepAdaptor_Curve curve(ei.Current());
                const auto edge = ei.Current();
                const auto start = TopExp::FirstVertex(edge, true);
                const auto end = TopExp::LastVertex(edge, true);
                const auto a = BRep_Tool::Pnt(start), b = BRep_Tool::Pnt(end);
                QJsonObject te{{"start", QJsonArray{a.X(), a.Y(), a.Z()}},
                    {"end", QJsonArray{b.X(), b.Y(), b.Z()}},
                    {"startTolerance", BRep_Tool::Tolerance(start)},
                    {"endTolerance", BRep_Tool::Tolerance(end)},
                    {"edgeTolerance", BRep_Tool::Tolerance(edge)},
                    {"seam", BRep_Tool::IsClosed(edge, face)},
                    {"reversed", edge.Orientation() == TopAbs_REVERSED}};
                QJsonArray preparedIndices;
                for (int index = 0; index < s->GetPreparedPolylineCount(); ++index) {
                    TopoDS_Edge candidate;
                    if (s->GetPreparedTopoEdge(index, candidate) && candidate.IsSame(edge))
                        preparedIndices.append(index);
                }
                te["preparedIndices"] = preparedIndices;
                Standard_Real first, last;
                const auto pc = BRep_Tool::CurveOnSurface(edge, face, first, last);
                QJsonArray pcurve;
                if (!pc.IsNull()) for (int k = 0; k <= 24; ++k) {
                    const double t = edge.Orientation() == TopAbs_REVERSED ? 1. - k / 24. : k / 24.;
                    const auto q = pc->Value(first + (last-first)*t);
                    pcurve.append(QJsonArray{q.X(), q.Y()});
                }
                te["pcurve"] = pcurve;
                topoEdges.append(te);
                QJsonArray samples;
                for (int k = 0; k <= 24; ++k) {
                    const auto p = curve.Value(curve.FirstParameter() +
                        (curve.LastParameter() - curve.FirstParameter()) * k / 24.0);
                    samples.append(QJsonArray{p.X(), p.Y(), p.Z()});
                }
                edges.append(samples);
            }
            wires.append(edges);
            topology.append(topoEdges);
        }
        o["cadWires"] = wires;
        o["topology"] = topology;
        std::unique_ptr<CSurfaceFace> reference;
        const CMesh3D* mesh = s->pMesh3D;
        if (std::string(phase) == "before" && (!mesh || mesh->GetVertices().empty())) {
            // Deserialized faces may have no display mesh. Tessellate a deep
            // CAD copy for visualization, without changing the document caches.
            BRepBuilderAPI_Copy copy(s->m_Face, true, false);
            reference = std::make_unique<CSurfaceFace>(copy.Shape());
            require(reference->BuldMeshTriangle(.2f, .3f), "Reference tessellation failed.");
            mesh = reference->pMesh3D;
            o["meshOrigin"] = "reference tessellation of CAD copy (deflection .2, angle .3)";
        }
        if (mesh) {
            o["vertices"] = points(mesh->GetVertices());
            QJsonArray cells;
            for (const auto& f : mesh->GetFaces()) {
                if (f.deleted) continue;
                QJsonArray c; for (const auto& v : f.corners) c.append(int(v.v));
                cells.append(c);
            }
            o["cells"] = cells;
            QJsonArray outside;
            SurfaceUVMapping mapping(s);
            int index = 0;
            for (const auto& f : mesh->GetFaces()) {
                if (f.deleted) continue;
                Vec3 center{};
                for (const auto& corner : f.corners) center = center + mesh->GetVertices()[corner.v];
                center = center * (1.f / std::max(size_t(1), f.corners.size()));
                SurfaceUVPoint uv;
                if (mapping.Project(center, uv)) {
                    BRepClass_FaceClassifier classifier(face, gp_Pnt2d(uv.u, uv.v), 1.e-5);
                    if (classifier.State() == TopAbs_OUT) outside.append(index);
                }
                ++index;
            }
            o["outsideCells"] = outside;
        }
        if (std::string(phase) == "after") {
            CSurfaceFace isolated(s->m_Face);
            isolated.InitEdges();
            isolated.InitEdges3DCoat();
            isolated.lenEdgeMax = s->lenEdgeMax;
            isolated.PrepareEdges(1.f / density, true);
            QJsonArray initial, initialUV, boundTests;
            SurfaceUVMapping initialMapping(&isolated);
            for (int e = 0; e < isolated.GetPreparedPolylineCount(); ++e) {
                std::vector<CPoint3d> ps;
                isolated.GetPreparedPolylinePoints(e, ps);
                initial.append(points(ps));
                CPolyline line;
                QJsonArray projected;
                for (const auto& p : ps) {
                    line.AddPoint(p);
                    SurfaceUVPoint uv;
                    if (initialMapping.Project({float(p.x), float(p.y), float(p.z)}, uv))
                        projected.append(QJsonArray{uv.u, uv.v});
                    else projected.append(QJsonValue());
                }
                initialUV.append(projected);
                boundTests.append(QJsonObject{{"accepted", isolated.IsBoundLine(&line)},
                    {"tolerance", line.GetLength() * .01}});
            }
            o["isolatedPreparedReplay"] = initial;
            o["isolatedPreparedUV"] = initialUV;
            o["boundLineTests"] = boundTests;
            isolated.UpdateMeshTypeFromBoundary();
            o["isolatedMeshType"] = int(isolated.m_TypeMesh);
            if (s->GetLastIslandFillError() == "No closed UV contours were produced.") {
                const bool filled = isolated.BuildFilledMeshWhithHoles(1.f / density, false);
                o["isolatedFillSucceeded"] = filled;
                o["isolatedFillError"] = QString::fromStdString(isolated.GetLastIslandFillError());
                o["isolatedFillCellCount"] = int(isolated.pMesh3D->GetFaces().size());
                if (!filled) {
                    // Controlled experiment on the isolated copy only: honor
                    // shared CAD vertices instead of each curve's own endpoint.
                    double maximumMove = 0.;
                    for (int e = 0; e < isolated.GetPreparedPolylineCount(); ++e) {
                        TopoDS_Edge edge;
                        std::vector<CPoint3d> ps;
                        if (!isolated.GetPreparedTopoEdge(e, edge)
                            || !isolated.GetPreparedPolylinePoints(e, ps) || ps.size() < 2) continue;
                        const auto a = BRep_Tool::Pnt(TopExp::FirstVertex(edge));
                        const auto b = BRep_Tool::Pnt(TopExp::LastVertex(edge));
                        auto distance = [](const CPoint3d& p, const gp_Pnt& q) {
                            return gp_Pnt(p.x,p.y,p.z).Distance(q);
                        };
                        const bool reverse = distance(ps.front(), b) + distance(ps.back(), a)
                            < distance(ps.front(), a) + distance(ps.back(), b);
                        const auto first = reverse ? b : a, last = reverse ? a : b;
                        maximumMove = std::max({maximumMove, distance(ps.front(), first), distance(ps.back(), last)});
                        ps.front() = CPoint3d(first.X(), first.Y(), first.Z());
                        ps.back() = CPoint3d(last.X(), last.Y(), last.Z());
                        isolated.SetPreparedPolylinePoints(e, ps);
                    }
                    o["canonicalVertexControl"] = QJsonObject{
                        {"maximumMove", maximumMove},
                        {"filled", isolated.BuildFilledMeshWhithHoles(1.f / density, false)},
                        {"error", QString::fromStdString(isolated.GetLastIslandFillError())},
                        {"cells", int(isolated.pMesh3D->GetFaces().size())}};
                }
            }
            const double tolerance = std::max(1.e-7, double(std::max(s->lenEdgeMax, 1.f)) * 1.e-5);
            o["joinTolerance"] = tolerance;
            QJsonArray prepared;
            std::vector<std::unique_ptr<CPolyline>> storage;
            std::vector<CPolyline*> loose, joined;
            for (int e = 0; e < s->GetPreparedPolylineCount(); ++e) {
                std::vector<CPoint3d> ps;
                s->GetPreparedPolylinePoints(e, ps);
                prepared.append(points(ps));
                auto line = std::make_unique<CPolyline>();
                for (const auto& p : ps) line->AddPoint(p);
                if (ps.size() >= 2) loose.push_back(line.get());
                storage.push_back(std::move(line));
            }
            o["prepared"] = prepared;
            QJsonArray shared;
            for (int e = 0; e < s->GetPreparedPolylineCount(); ++e) {
                TopoDS_Edge edge;
                if (!s->GetPreparedTopoEdge(e, edge)) continue;
                for (int other = 0; other < solid.GetNumSurfaces(); ++other) {
                    if (other == id) continue;
                    auto* donor = solid.GetSurfaceFace(other);
                    for (int de = 0; de < donor->GetPreparedPolylineCount(); ++de) {
                        TopoDS_Edge candidate;
                        if (!donor->GetPreparedTopoEdge(de, candidate) || !candidate.IsSame(edge)) continue;
                        std::vector<CPoint3d> row;
                        donor->GetRegularMeshBoundaryPoints(de, row);
                        shared.append(QJsonObject{{"edge", e}, {"neighbour", other}, {"neighbourEdge", de},
                            {"regularBoundary", points(row)}});
                    }
                }
            }
            o["sharedEdges"] = shared;
            o["joinReturned"] = CPolyline::JoinMultuLines(&loose, &joined, tolerance);
            QJsonArray loops;
            SurfaceUVMapping mapping(s);
            for (auto* line : joined) {
                const double gap = line->P(0)->DistTo(line->PLast());
                if (gap <= tolerance) line->SetClosed(true);
                QJsonObject l{{"xyz", points(line->GetPoints())}, {"gap", gap}, {"closed", line->IsClosed()}};
                QJsonArray uv, failed, residual, directions;
                for (int n = 0; n < int(line->GetPointCount()); ++n) {
                    const auto p = line->GetPoints()[n];
                    SurfaceUVPoint q;
                    if (!mapping.Project({float(p.x), float(p.y), float(p.z)}, q)) {
                        failed.append(n); uv.append(QJsonValue()); residual.append(QJsonValue()); continue;
                    }
                    uv.append(QJsonArray{q.u, q.v});
                    residual.append(adaptor.Value(q.u, q.v).Distance(gp_Pnt(p.x, p.y, p.z)));
                    if (n % 8 == 0) {
                        gp_Pnt origin; gp_Vec du, dv;
                        adaptor.D1(q.u, q.v, origin, du, dv);
                        directions.append(QJsonObject{{"origin", QJsonArray{origin.X(), origin.Y(), origin.Z()}},
                            {"du", QJsonArray{du.X(), du.Y(), du.Z()}},
                            {"dv", QJsonArray{dv.X(), dv.Y(), dv.Z()}},
                            {"jacobian", du.Crossed(dv).Magnitude()}});
                    }
                }
                l["uv"] = uv; l["projectionFailedNodes"] = failed; l["projectionResidual"] = residual;
                l["directions"] = directions;
                l["putOnSurface"] = line->IsClosed() && line->PutOnSurface(s);
                loops.append(l);
            }
            o["joinedReplay"] = loops;
            QJsonArray patches;
            std::vector<std::unique_ptr<CPolyline>> boundaries;
            s->CreateLastQuadrangulationBoundaryPolylines(boundaries);
            for (const auto& line : boundaries) patches.append(points(line->GetPoints()));
            o["quadrangulatorInput"] = patches;
            o["diagnosticPath"] = QString::fromStdString(s->GetLastQuadrangulationDiagnostic());
            // Copy only the diagnostic named by this run, never stale directory contents.
            if (!s->GetLastQuadrangulationDiagnostic().empty()) {
                const QString source = QString::fromStdString(s->GetLastQuadrangulationDiagnostic());
                const QString target = directory + '/' + QFileInfo(source).fileName();
                QFile input(source);
                if (input.open(QIODevice::ReadOnly)) {
                    QFile output(target);
                    require(output.open(QIODevice::WriteOnly), "Cannot copy named diagnostic.");
                    output.write(input.readAll());
                }
            }
            // This observer cannot invent front iteration history for successful faces.
            o["frontIterations"] = QJsonValue();
            o["iterationStatus"] = s->GetLastIslandFillError() == "No closed UV contours were produced."
                ? "not reached: UV contour admission failed" : "not instrumented";
        }
        surfaces.append(o);
    }
    QString name = QString::fromStdString(solid.GetName());
    name.replace('/', '_').replace('\\', '_');
    QFile file(directory + '/' + name + '-' + phase + ".json");
    require(file.open(QIODevice::WriteOnly), "Cannot open pipeline diagnostic JSON.");
    const auto data = QJsonDocument(QJsonObject{{"solid", name}, {"phase", phase}, {"surfaces", surfaces}}).toJson();
    require(file.write(data) == data.size(), "Cannot write pipeline diagnostic JSON.");
}

