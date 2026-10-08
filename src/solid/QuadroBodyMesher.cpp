#include "QuadroBodyMesher.h"
#include "QuadroPatchMesh.h"
#include "Solid.h"
#include "../CMesh3D.h"
#include <cmath>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gce_MakeCirc.hxx>
#include <array>

namespace quadro {
namespace {
// Invert cumulative chord length, keeping the exact surface parameters at
// both ends. This avoids concentrating rings at B-spline knot intervals.
template<class PointAt> std::vector<double> tubeParameters(PointAt pointAt,int count) {
    constexpr int samples=1024;
    std::array<double,samples+1> lengths{};
    auto previous=pointAt(0.0);
    for(int i=1;i<=samples;++i) {
        const auto point=pointAt(double(i)/samples);
        lengths[i]=lengths[i-1]+previous.Distance(point);previous=point;
    }
    std::vector<double> result(count+1);
    for(int i=1;i<count;++i) {
        const double length=lengths.back()*i/count;
        const auto it=std::lower_bound(lengths.begin()+1,lengths.end(),length);
        const int j=int(it-lengths.begin());
        const double span=lengths[j]-lengths[j-1];
        result[i]=(j-1+(span>0?(length-lengths[j-1])/span:0))/samples;
    }
    result.back()=1;return result;
}
}

BodyMeshAttempt BuildTubeCadBody(CSolid& solid,float deflection) {
    // Only complete, uncut tubes: two rectangular closed-U walls and two
    // annular planar ends. Edited/trimmed bodies retain the general mesher.
    if(solid.GetNumSurfaces()!=4 || !std::isfinite(deflection) || deflection<=0)
        return BodyMeshAttempt::NotApplicable;
    try {
        std::vector<int> walls,caps;
        for(int i=0;i<4;++i) {
            auto* surface=solid.GetSurfaceFace(i);
            if(!surface||!surface->pMesh3D)return BodyMeshAttempt::NotApplicable;
            const auto face=TopoDS::Face(surface->m_Face);
            BRepAdaptor_Surface a(face);
            int wires=0,edges=0,seams=0;
            for(TopExp_Explorer e(face,TopAbs_WIRE);e.More();e.Next())++wires;
            for(TopExp_Explorer e(face,TopAbs_EDGE);e.More();e.Next()) {
                ++edges;if(BRep_Tool::IsClosed(TopoDS::Edge(e.Current()),face))++seams;
            }
            if(a.GetType()==GeomAbs_Plane&&wires==2&&edges==2)caps.push_back(i);
            else if(a.IsUClosed()&&!a.IsVClosed()&&wires==1&&edges==4&&seams==2)walls.push_back(i);
            else return BodyMeshAttempt::NotApplicable;
        }
        if(walls.size()!=2||caps.size()!=2)return BodyMeshAttempt::NotApplicable;
        std::array<std::array<double,4>,2> bounds;
        for(int s=0;s<2;++s)BRepTools::UVBounds(TopoDS::Face(solid.GetSurfaceFace(walls[s])->m_Face),
            bounds[s][0],bounds[s][1],bounds[s][2],bounds[s][3]);
        const auto point=[&](int s,double u,double v) {
            const auto& b=bounds[s];
            return BRepAdaptor_Surface(TopoDS::Face(solid.GetSurfaceFace(walls[s])->m_Face))
                .Value(b[0]+u*(b[1]-b[0]),b[2]+v*(b[3]-b[2]));
        };
        const auto circle=[&](int s,double v) {
            return gce_MakeCirc(point(s,0,v),point(s,1.0/3,v),point(s,2.0/3,v)).Value();
        };
        const double radius=std::max(circle(0,0).Radius(),circle(1,0).Radius());
        const double thickness=std::abs(circle(0,0).Radius()-circle(1,0).Radius());
        const double tolerance=std::max(1e-6,radius*1e-5);
        if(thickness<=tolerance)return BodyMeshAttempt::NotApplicable;
        // Verify the two natural charts describe concentric circular rings,
        // with matching angular and longitudinal directions, before sharing
        // their sampling. Never apply this shortcut to an arbitrary quilt.
        for(int j=0;j<=8;++j) {
            const double v=double(j)/8;const auto c0=circle(0,v),c1=circle(1,v);
            if(c0.Location().Distance(c1.Location())>tolerance)return BodyMeshAttempt::NotApplicable;
            for(int i=0;i<=16;++i) {
                const double u=double(i)/16;
                if(std::abs(point(0,u,v).Distance(point(1,u,v))-thickness)>tolerance
                    ||std::abs(point(0,u,v).Distance(c0.Location())-c0.Radius())>tolerance
                    ||std::abs(point(1,u,v).Distance(c1.Location())-c1.Radius())>tolerance)
                    return BodyMeshAttempt::NotApplicable;
            }
        }
        // Identify each cap through shared CAD edges, not face ordering.
        std::array<int,2> ends{{-1,-1}};
        for(int cap:caps) {
            const auto face=TopoDS::Face(solid.GetSurfaceFace(cap)->m_Face);
            BRepAdaptor_Surface a(face);
            int shared=0;
            for(int wall:walls)for(TopExp_Explorer e(face,TopAbs_EDGE);e.More();e.Next())
                for(TopExp_Explorer w(solid.GetSurfaceFace(wall)->m_Face,TopAbs_EDGE);w.More();w.Next())
                    if(e.Current().IsSame(w.Current()))++shared;
            if(shared!=2)return BodyMeshAttempt::NotApplicable;
            for(int end=0;end<2;++end) {
                bool onPlane=true;
                for(int s=0;s<2;++s)for(int i=0;i<8;++i)
                    onPlane=onPlane&&a.Plane().Distance(point(s,double(i)/8,end))<=tolerance;
                if(onPlane)ends[end]=cap;
            }
        }
        if(ends[0]<0||ends[1]<0||ends[0]==ends[1])return BodyMeshAttempt::NotApplicable;
        const double step=std::sqrt(2*radius*double(deflection));
        const int columns=int(std::clamp(std::ceil(2*std::acos(-1.0)*radius/step),12.0,256.0));
        double length=0;auto previous=circle(0,0).Location();
        for(int j=1;j<=256;++j){auto p=circle(0,double(j)/256).Location();length+=previous.Distance(p);previous=p;}
        const int rows=int(std::clamp(std::ceil(length/step),2.0,2048.0));
        const auto us=tubeParameters([&](double u){return point(0,u,0);},columns);
        const auto vs=tubeParameters([&](double v){return circle(0,v).Location();},rows);
        struct MeshData {std::vector<Vec3> xyz;std::vector<UV> uv;std::vector<Vec3> normals;std::vector<CMesh3D::Face> faces;};
        std::array<MeshData,4> data;
        const auto addQuad=[&](MeshData& mesh,unsigned a,unsigned b,unsigned c,unsigned d) {
            const auto p=mesh.xyz[a],q=mesh.xyz[b],r=mesh.xyz[c],s=mesh.xyz[d];
            const gp_Vec ab(q.x-p.x,q.y-p.y,q.z-p.z),ac(r.x-p.x,r.y-p.y,r.z-p.z),ad(s.x-p.x,s.y-p.y,s.z-p.z);
            if(ab.Crossed(ac).Dot(ac.Crossed(ad))<=0)return false;
            const auto n=mesh.normals[a];
            if(ab.Crossed(ac).Dot(gp_Vec(n.x,n.y,n.z))<0)std::swap(b,d);
            CMesh3D::Face f;for(auto id:{a,b,c,d})f.corners.push_back({id,id,id});mesh.faces.push_back(std::move(f));return true;
        };
        for(int s=0;s<2;++s) {
            auto& mesh=data[walls[s]];const auto face=TopoDS::Face(solid.GetSurfaceFace(walls[s])->m_Face);
            BRepAdaptor_Surface a(face);const auto& b=bounds[s];
            for(int j=0;j<=rows;++j)for(int i=0;i<columns;++i) {
                const double u=b[0]+us[i]*(b[1]-b[0]),v=b[2]+vs[j]*(b[3]-b[2]);
                gp_Pnt p;gp_Vec du,dv;a.D1(u,v,p,du,dv);auto n=du.Crossed(dv);
                if(n.Magnitude()<1e-12)return BodyMeshAttempt::NotApplicable;
                n.Normalize();if(face.Orientation()==TopAbs_REVERSED)n.Reverse();
                mesh.xyz.push_back({float(p.X()),float(p.Y()),float(p.Z())});
                mesh.uv.push_back({float(u),float(v)});mesh.normals.push_back({float(n.X()),float(n.Y()),float(n.Z())});
            }
            // One geometric vertex at the seam, but two texture coordinates.
            // Otherwise the final strip would interpolate across the whole UV chart.
            const unsigned seamUV=unsigned(mesh.uv.size());
            for(int j=0;j<=rows;++j)mesh.uv.push_back({float(b[1]),float(b[2]+vs[j]*(b[3]-b[2]))});
            for(int j=0;j<rows;++j)for(int i=0;i<columns;++i) {
                const int next=(i+1)%columns;
                if(!addQuad(mesh,j*columns+i,j*columns+next,(j+1)*columns+next,(j+1)*columns+i))return BodyMeshAttempt::NotApplicable;
                if(next==0)for(auto& corner:mesh.faces.back().corners)
                    if(corner.v%columns==0)corner.uv=seamUV+unsigned(corner.v/columns);
            }
        }
        for(int end=0;end<2;++end) {
            auto& mesh=data[ends[end]];const auto face=TopoDS::Face(solid.GetSurfaceFace(ends[end])->m_Face);
            const auto plane=BRepAdaptor_Surface(face).Plane();auto n=plane.Axis().Direction();
            if(face.Orientation()==TopAbs_REVERSED)n.Reverse();
            for(int s=0;s<2;++s)for(int i=0;i<columns;++i) {
                // Copy wall coordinates exactly: no independently sampled cap
                // boundaries and no tolerance-dependent stitching afterwards.
                const auto p=data[walls[s]].xyz[end*rows*columns+i];mesh.xyz.push_back(p);
                const gp_Vec offset(plane.Location(),gp_Pnt(p.x,p.y,p.z));
                mesh.uv.push_back({float(offset.Dot(gp_Vec(plane.XAxis().Direction()))),float(offset.Dot(gp_Vec(plane.YAxis().Direction())))});
                mesh.normals.push_back({float(n.X()),float(n.Y()),float(n.Z())});
            }
            for(int i=0;i<columns;++i) {
                const int next=(i+1)%columns;
                if(!addQuad(mesh,i,next,columns+next,columns+i))return BodyMeshAttempt::NotApplicable;
            }
        }
        std::array<CMesh3D,4> meshes;
        for(int i=0;i<4;++i)if(!meshes[i].SetGeometry(std::move(data[i].xyz),std::move(data[i].faces),
            std::move(data[i].uv),std::move(data[i].normals)))return BodyMeshAttempt::NotApplicable;
        for(int i=0;i<4;++i) {
            auto* surface=solid.GetSurfaceFace(i);
            surface->pMesh3D->SetGeometry(meshes[i].GetVertices(),meshes[i].GetFaces(),meshes[i].GetUVs(),meshes[i].GetNormals());
            surface->IsInitMesh=true;surface->IsTrimmed=true;surface->m_TypeMesh=REGULAR_MESH;
            surface->m_QtyU=columns;surface->m_QtyV=(i==walls[0]||i==walls[1])?rows+1:2;
            surface->m_LastLowPolyDensity=1/deflection;surface->m_LastIslandFillError.clear();
            surface->m_LastQuadrangulationDiagnostic.clear();surface->m_LastIslandBoundariesUV.clear();
        }
        return BodyMeshAttempt::Built;
    } catch(const Standard_Failure&) {return BodyMeshAttempt::NotApplicable;}
}

BodyMeshAttempt BuildStructuredCadBody(CSolid& solid,float deflection) {
    const auto tube=BuildTubeCadBody(solid,deflection);
    if(tube!=BodyMeshAttempt::NotApplicable)return tube;
    // Eligibility already requires one wire per structured face. A hole-only
    // option must not disable this path for a body made of ordinary patches.
    if(solid.MeshQuadroHoleDivideFace || solid.MeshQuadroTrimByPline)return BodyMeshAttempt::NotApplicable;
    const double density=1.0/double(deflection);
    const auto topology=solid.GetQuadroTopologySnapshot();
    const auto* baseOperation=solid.GetOperation(0);
    const bool sixFaceShell=baseOperation && baseOperation->ToolId=="SolidShell";
    const auto plan=PlanStructuredBody(*topology,density,sixFaceShell);
    if(!plan.eligible)return BodyMeshAttempt::NotApplicable;
    const auto fail=[&](Id face,const std::string& message){
        if(auto* surface=solid.GetSurfaceFace(int(face)))surface->m_LastIslandFillError="CAD boundary patch: "+message;
        return BodyMeshAttempt::Failed;
    };
    auto prepared=solid.PrepareQuadroBoundary(plan.sampling);
    // Some imported quilts have valid faces but mutually inconsistent pcurves.
    // The strict shared-patch consumer cannot use them; let the existing
    // per-face mesher handle the import without relaxing the CAD budget.
    if(!prepared->discretizationReady()&&!prepared->issues().empty()&&
        std::all_of(prepared->issues().begin(),prepared->issues().end(),[](const Issue& issue){return issue.code=="CadRepresentationMismatch";}))
        return BodyMeshAttempt::NotApplicable;
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
