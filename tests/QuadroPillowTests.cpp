#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "solid/QuadroBoundary.h"
#include <BRepGProp.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include "SurfaceUVMapping.h"
#include <QDir>
#include <map>
#include <set>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value){std::cerr<<message<<std::endl;throw std::runtime_error(message);}}
std::vector<double> signature(const CMesh3D& mesh) {
    std::vector<double> values;
    for(const auto& p:mesh.GetVertices()){values.push_back(p.x);values.push_back(p.y);values.push_back(p.z);}
    for(const auto& f:mesh.GetFaces())for(const auto& c:f.corners)values.push_back(double(c.v));
    return values;
}
}

static void TestCadPatchBody(const char* path,const char* outputPrefix,bool frame) {
    CAlfaDoc document;Dom3DProjectSerializer serializer;QString room,error;ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),document,room,view,error),"Cannot load pillow fixture");
    CSolid* solid=nullptr;
    for(const auto& o:document.GetObjects())if(auto* s=dynamic_cast<CSolid*>(o.get());s&&s->GetName()==(frame?"Frame":"Extrude Solid"))solid=s;
    check(solid&&solid->InitSurfaces()&&solid->GetNumSurfaces()==(frame?32:18),"CAD patch body fixture body changed");
    const auto topology=solid->GetQuadroTopologySnapshot();
    for(int i=0;i<solid->GetNumSurfaces();++i)
        check(topology->faces()[i].reversed==(solid->GetSurfaceFace(i)->m_Face.Orientation()==TopAbs_REVERSED),"Snapshot changed CAD face orientation");
    solid->MeshQuadro=true;solid->MeshQuadroHoleSLX=false;
    GProp_GProps properties;BRepGProp::VolumeProperties(solid->m_Shape,properties);
    const double cadVolume=properties.Mass();
    std::vector<double> repeat;std::size_t lowCount=0,highCount=0;
    int iteration=0;
    for(float density:{0.25f,frame?0.45f:0.5f,1.f,frame?0.45f:0.5f}) {
        check(solid->ReBuldMesh(1.f/density),"CAD patch body CAD quad meshing failed");
        check(solid->GetQuadroTopologySnapshot()==topology&&solid->GetQuadroBoundaryCaptureCount()==1,"Density recaptured CAD topology");
        std::vector<const CMesh3D*> meshes;
        for(int i=0;i<solid->GetNumSurfaces();++i) {
            const auto* s=solid->GetSurfaceFace(i);
            check(s->IsInitMesh&&s->GetLastIslandFillError().empty()&&!s->pMesh3D->GetFaces().empty(),"CAD patch body face empty or failed");
            SurfaceUVMapping mapping(s);
            for(const auto& f:s->pMesh3D->GetFaces()) {
                check(!f.deleted&&f.corners.size()==4,"CAD patch body contains a non-quad cell");
                std::set<std::size_t> unique;Vec3 center{};
                for(const auto& c:f.corners){unique.insert(c.v);const auto p=s->pMesh3D->GetVertices()[c.v];center.x+=p.x/4;center.y+=p.y/4;center.z+=p.z/4;}
                check(unique.size()==4,"CAD patch body contains collapsed cell indices");
                SurfaceUVPoint uv;
                check(mapping.Project(center,uv),"Cannot project pillow cell centre");
                BRepClass_FaceClassifier classifier(TopoDS::Face(s->m_Face),gp_Pnt2d(uv.u,uv.v),1.e-5);
                check(classifier.State()!=TopAbs_OUT,"CAD patch body cell centre outside CAD face");
            }
            meshes.push_back(s->pMesh3D);
        }
        auto mesh=CMesh3D::CreateWelded(meshes);check(bool(mesh),"Cannot assemble pillow mesh");
        std::map<std::pair<std::size_t,std::size_t>,std::pair<int,int>> edges;
        double volume=0;
        for(const auto& f:mesh->GetFaces()) {
            check(f.corners.size()==4,"Welding changed quad arity");
            for(std::size_t k=0;k<4;++k){auto a=f.corners[k].v,b=f.corners[(k+1)%4].v;auto& e=edges[std::minmax(a,b)];++e.first;e.second+=a<b?1:-1;}
            const auto a=mesh->GetVertices()[f.corners[0].v];
            for(std::size_t k=1;k<3;++k){const auto b=mesh->GetVertices()[f.corners[k].v],c=mesh->GetVertices()[f.corners[k+1].v];
                volume+=(double(a.x)*(double(b.y)*c.z-double(b.z)*c.y)+double(a.y)*(double(b.z)*c.x-double(b.x)*c.z)+double(a.z)*(double(b.x)*c.y-double(b.y)*c.x))/6;}
        }
        for(const auto& e:edges)check(e.second.first==2&&e.second.second==0,"CAD patch body has open/non-manifold/inconsistently oriented edges");
        check(static_cast<long long>(mesh->GetVertices().size())-static_cast<long long>(edges.size())+static_cast<long long>(mesh->GetFaces().size())==(frame?0:2),"Unexpected CAD patch body Euler characteristic");
        std::cout<<"CAD volume="<<cadVolume<<" mesh volume="<<volume<<std::endl;
        check(std::abs(volume-cadVolume)<std::abs(cadVolume)*0.03,"CAD patch body volume differs from CAD by more than 3 percent");
        if(density==0.25f)lowCount=mesh->GetFaces().size();
        if(density==1.f)highCount=mesh->GetFaces().size();
        if(iteration==1)repeat=signature(*mesh);
        if(iteration==3)check(repeat==signature(*mesh),"Repeated density changed pillow result");
        std::cout<<"CAD patch body density="<<density<<" quads="<<mesh->GetFaces().size()<<" vertices="<<mesh->GetVertices().size()
            <<" openEdges=0 volumeError="<<std::abs(volume-cadVolume)/std::abs(cadVolume)<<" captures="<<solid->GetQuadroBoundaryCaptureCount()<<'\n';
        if(iteration==3&&outputPrefix) {
            const QString prefix=QString::fromLocal8Bit(outputPrefix);
            check(mesh->ExportToObj((prefix+".obj").toStdString()),"Cannot save pillow OBJ");
            mesh->SetName("CAD patch body-2 Quadro");CAlfaDoc result;result.Clear();result.AddObject(std::move(mesh));
            check(serializer.Save(prefix+".dom3d",result,"Surfaces",view,{},error),"Cannot save pillow result project");
            CAlfaDoc loaded;QString loadedRoom;ProjectViewState loadedView;
            check(serializer.Load(prefix+".dom3d",loaded,loadedRoom,loadedView,error),"Cannot reopen saved pillow result");
            const CMesh3D* reloaded=nullptr;std::size_t meshCount=0;
            for(const auto& object:loaded.GetObjects())if(const auto* candidate=dynamic_cast<const CMesh3D*>(object.get())){reloaded=candidate;++meshCount;}
            check(meshCount==1,"Saved pillow result has wrong mesh count");
            check(reloaded&&signature(*reloaded)==repeat,"Saved pillow mesh did not round-trip exactly");
        }
        ++iteration;
    }
    check(highCount>lowCount,"Density does not refine pillow mesh");
}

void TestPillowCadQuadro(const char* path,const char* outputPrefix) { TestCadPatchBody(path,outputPrefix,false); }
void TestFrameCadQuadro(const char* path) { TestCadPatchBody(path,nullptr,true); }
