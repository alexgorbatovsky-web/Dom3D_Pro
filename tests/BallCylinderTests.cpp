#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "CMesh3D.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <iostream>
#include <map>
#include <cmath>

int TestBallCylinder(const char* path) {
    CAlfaDoc document;Dom3DProjectSerializer serializer;QString room,error;ProjectViewState view;
    if(!serializer.Load(QString::fromLocal8Bit(path),document,room,view,error)){std::cerr<<error.toStdString();return 1;}
    bool valid=true;int bodies=0;
    for(const auto& object:document.GetObjects()) {
        auto* solid=dynamic_cast<CSolid*>(object.get());if(!solid||solid->GetName()!="Boolean Union")continue;++bodies;
        solid->MeshQuadro=true;solid->MeshQuadroHoleSLX=false;
        for(float density:{.45f,.65f,.25f,.35f,.5f,.7f,1.f,.45f,.65f}) {
            bool built=solid->ReBuldMesh(1.f/density);std::cout<<"density="<<density<<" built="<<built<<'\n';valid=built&&valid;
            std::vector<const CMesh3D*> meshes;
            for(int i=0;i<solid->GetNumSurfaces();++i){auto* s=solid->GetSurfaceFace(i);if(!s||!s->pMesh3D){valid=false;continue;}meshes.push_back(s->pMesh3D);
                auto type=BRepAdaptor_Surface(TopoDS::Face(s->m_Face)).GetType();size_t active=0;double area=0;
                const auto& vs=s->pMesh3D->GetVertices();
                for(const auto& f:s->pMesh3D->GetFaces()){if(f.deleted)continue;++active;
                    // Clipping the sphere and filling its cap can retain triangles.
                    // The rebuilt cylindrical strip must consist of full quads.
                    if(f.corners.size()<3 || (type==GeomAbs_Cylinder && f.corners.size()!=4))valid=false;
                    if(type==GeomAbs_Cylinder && f.corners.size()==4){
                        auto a=vs[f.corners[0].v],b=vs[f.corners[1].v],c=vs[f.corners[2].v],d=vs[f.corners[3].v];
                        if(dot(cross(b-a,c-a),cross(c-a,d-a))<=1.e-10)valid=false;
                    }
                    auto a=vs[f.corners[0].v];for(size_t j=1;j+1<f.corners.size();++j){auto b=vs[f.corners[j].v],c=vs[f.corners[j+1].v];double ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;area+=.5*std::sqrt(std::pow(uy*vz-uz*vy,2)+std::pow(uz*vx-ux*vz,2)+std::pow(ux*vy-uy*vx,2));}}
                GProp_GProps props;BRepGProp::SurfaceProperties(s->m_Face,props);double ratio=area/props.Mass();
                std::cout<<" face="<<i<<" type="<<int(type)<<" initialized="<<s->IsInitMesh<<" cells="<<active<<" areaRatio="<<ratio<<" error="<<s->GetLastIslandFillError()<<'\n';
                valid=valid&&s->IsInitMesh&&active>0&&ratio>.94&&ratio<1.06;
            }
            auto welded=CMesh3D::CreateWelded(meshes);size_t open=0,bad=0;
            if(!welded)valid=false;else {std::map<std::pair<size_t,size_t>,int> edges;
                for(const auto& f:welded->GetFaces())if(!f.deleted)for(size_t k=0;k<f.corners.size();++k){auto a=f.corners[k].v,b=f.corners[(k+1)%f.corners.size()].v;++edges[{std::min(a,b),std::max(a,b)}];}
                for(auto e:edges){if(e.second==1)++open;else if(e.second!=2)++bad;}}
            std::cout<<" open="<<open<<" nonManifold="<<bad<<std::endl;valid=valid&&open==0&&bad==0;
        }
    }
    if(!valid||bodies!=1){std::cerr<<"Ball/cylinder mesh regression failed\n";return 1;}return 0;
}
