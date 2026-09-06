#include "QuadroBodyMesher.h"
#include "QuadroPatchMesh.h"
#include "Solid.h"
#include "../CMesh3D.h"
#include <cmath>

namespace quadro {
BodyMeshAttempt BuildStructuredCadBody(CSolid& solid,float deflection) {
    if(solid.MeshQuadroHoleSLX)return BodyMeshAttempt::NotApplicable;
    const double density=1.0/double(deflection);
    const auto topology=solid.GetQuadroTopologySnapshot();
    const auto plan=PlanStructuredBody(*topology,density);
    if(!plan.eligible)return BodyMeshAttempt::NotApplicable;
    const auto fail=[&](Id face,const std::string& message){
        if(auto* surface=solid.GetSurfaceFace(int(face)))surface->m_LastIslandFillError="CAD boundary patch: "+message;
        return BodyMeshAttempt::Failed;
    };
    auto prepared=solid.PrepareQuadroBoundary(plan.sampling);
    if(!prepared->discretizationReady())return fail(0,prepared->issues().empty()?"Boundary preparation failed":prepared->issues().front().message);
    std::vector<StructuredPatch> patches;
    std::vector<FaceBoundaryInput> charts;
    for(const auto& f:prepared->faces()) {
        auto chart=BuildFaceBoundaryInput(prepared,f.id);
        if(!chart.ready)return fail(f.id,chart.issues.empty()?"Invalid chart":chart.issues.front().message);
        auto patch=BuildStructuredPatch(chart,plan.cornerOccurrences[f.id],plan.splitCornerEdges[f.id]);
        if(!patch.error.empty())return fail(f.id,patch.error);
        patches.push_back(std::move(patch));charts.push_back(std::move(chart));
    }
    std::string error;
    if(!ValidateClosedPatchBody(*prepared,patches,error))return fail(0,error);
    if(solid.GetNumSurfaces()!=int(patches.size()))return fail(0,"CAD face count changed");
    for(Id i=0;i<patches.size();++i)
        if(!solid.GetSurfaceFace(int(i))||!solid.GetSurfaceFace(int(i))->pMesh3D)
            return fail(i,"Missing destination face mesh");
    // Build all render meshes before publishing any of them. Mesh setters
    // compute normals and validate indices, but never alter master coordinates.
    std::vector<std::unique_ptr<CMesh3D>> meshes;
    for(const auto& patch:patches) {
        std::vector<Vec3> xyz;std::vector<UV> uv;std::vector<CMesh3D::Face> faces;
        for(const auto& p:patch.xyz)xyz.push_back({float(p.X()),float(p.Y()),float(p.Z())});
        for(const auto& p:patch.uv)uv.push_back({float(p.X()),float(p.Y())});
        // Float storage must not collapse otherwise valid CAD cells.
        for(const auto& q:patch.quads) {
            gp_Pnt points[4];
            for(Id k=0;k<4;++k){const auto v=xyz[q[k]];
                if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return fail(patch.face,"Float coordinate overflow");
                points[k]=gp_Pnt(v.x,v.y,v.z);}
            const gp_Vec a(points[0],points[1]),b(points[0],points[2]),c(points[0],points[3]);
            if(a.Crossed(b).Dot(b.Crossed(c))<=0)return fail(patch.face,"Float storage collapses or folds a cell");
        }
        for(const auto& q:patch.quads){CMesh3D::Face f;for(Id id:q)f.corners.push_back({id,id,0});faces.push_back(std::move(f));}
        auto mesh=std::make_unique<CMesh3D>();
        if(!mesh->SetGeometry(std::move(xyz),std::move(faces),std::move(uv)))return fail(patch.face,"Render mesh rejected patch");
        meshes.push_back(std::move(mesh));
    }
    for(Id i=0;i<patches.size();++i) {
        auto* surface=solid.GetSurfaceFace(int(i));
        // Preserve the face's mesh material/visual state by updating its
        // existing mesh object only after the transaction is fully validated.
        surface->pMesh3D->SetGeometry(meshes[i]->GetVertices(),meshes[i]->GetFaces(),meshes[i]->GetUVs(),meshes[i]->GetNormals());
        surface->IsInitMesh=true;surface->IsTrimmed=true;surface->m_TypeMesh=REGULAR_MESH;
        surface->m_QtyU=int(patches[i].columns);surface->m_QtyV=int(patches[i].rows);
        surface->m_LastLowPolyDensity=float(density);surface->m_LastIslandFillError.clear();
        surface->m_LastQuadrangulationDiagnostic.clear();surface->m_LastIslandBoundariesUV.clear();
        std::vector<CPoint3d> loop;for(const auto& v:charts[i].loops.front().vertices)loop.emplace_back(v.uv.X(),v.uv.Y(),0);
        surface->m_LastIslandBoundariesUV.push_back(std::move(loop));
    }
    return BodyMeshAttempt::Built;
}
}
