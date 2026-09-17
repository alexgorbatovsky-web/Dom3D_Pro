#include "StepIO.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"
#include "CMesh3D.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <iostream>

int TestPlasticityImport(const char* path) {
    StepIO io;
    std::vector<std::unique_ptr<CSolid>> solids;
    std::string error;
    if (!io.Import(path, solids, error) || solids.size() != 1) {
        std::cerr << "STEP import failed: " << error << '\n'; return 1;
    }
    auto& solid = *solids.front();
    if (solid.GetNumSurfaces() != 14 || !BRepCheck_Analyzer(solid.m_Shape).IsValid()) return 1;
    solid.MeshQuadro = true;
    solid.MeshQuadroHoleSLX = false;
    for (float density : {.1f, .5f, 1.f, .5f}) {
        if (!solid.ReBuldMesh(1.f / density)) {
            std::cerr << "Rebuild failed at density " << density << '\n'; return 1;
        }
        size_t quads = 0, triangles = 0;
        for (int i = 0; i < solid.GetNumSurfaces(); ++i) {
            const auto* face = solid.GetSurfaceFace(i);
            if (!face || !face->IsInitMesh || !face->pMesh3D || face->pMesh3D->GetFaces().empty()) return 1;
            size_t faceQuads = 0;
            for (const auto& cell : face->pMesh3D->GetFaces()) {
                if (cell.deleted) continue;
                if (cell.corners.size() == 3) { ++triangles; continue; }
                if (cell.corners.size() != 4) return 1;
                ++quads; ++faceQuads;
            }
            // Coarse odd-sided contours can end with one triangle. The bug
            // rejected the entire surface, requiring a full triangle fallback.
            if (!faceQuads) { std::cerr << "No quad mesh on face " << i << '\n'; return 1; }
            // Prepared donor endpoints must still agree with CAD vertices.
            for (int e = 0; e < face->GetPreparedPolylineCount(); ++e) {
                TopoDS_Edge edge;
                std::vector<CPoint3d> points;
                if (!face->GetPreparedTopoEdge(e, edge) || !face->GetPreparedPolylinePoints(e, points) || points.empty()) return 1;
                const auto first = BRep_Tool::Pnt(TopExp::FirstVertex(edge));
                const auto last = BRep_Tool::Pnt(TopExp::LastVertex(edge));
                for (const auto& p : {points.front(), points.back()}) {
                    const gp_Pnt point(p.x,p.y,p.z);
                    if (std::min(point.Distance(first),point.Distance(last)) > .002) {
                        std::cerr << "Displaced CAD endpoint on face " << i << '\n'; return 1;
                    }
                }
            }
        }
        std::cout << "density=" << density << ": 14 meshed surfaces, " << quads << " quads, " << triangles << " triangles\n";
    }
    return 0;
}
