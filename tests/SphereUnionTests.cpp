#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "CMesh3D.h"
#include "SurfaceUVMapping.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

int TestSphereUnion(const char* path)
{
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room,error;
    ProjectViewState view;
    if(!serializer.Load(QString::fromLocal8Bit(path),document,room,view,error)) {
        std::cerr<<error.toStdString();return 1;
    }
    CSolid* solid=nullptr;
    for(const auto& object:document.GetObjects())
        if(auto* candidate=dynamic_cast<CSolid*>(object.get());candidate&&candidate->GetName()=="Boolean Union")solid=candidate;
    if(!solid)return 1;
    solid->MeshQuadro=true;solid->MeshQuadroHoleSLX=false;
    bool valid=true;
    std::map<float,std::string> repeats;
    using Point=std::tuple<float,float,float>;
    using Edge=std::pair<size_t,size_t>;
    for(float density:{.25f,.35f,.45f,.5f,.65f,.7f,1.f,.5f,.25f}) {
        bool built=solid->ReBuldMesh(1.f/density);
        valid=valid&&built;
        if(!built)std::cout<<"error="<<solid->GetSurfaceFace(0)->GetLastIslandFillError()<<std::endl;
        std::map<Point,size_t> nodes;
        std::map<Edge,std::pair<int,int>> edges;
        std::vector<double> steps;
        std::ostringstream snapshot;
        snapshot<<std::hexfloat;
        size_t quads=0,cells=0;
        for(int i=0;i<solid->GetNumSurfaces();++i) {
            auto* s=solid->GetSurfaceFace(i);
            if(!s||!s->pMesh3D||!s->IsInitMesh){valid=false;continue;}
            BRepAdaptor_Surface adaptor(TopoDS::Face(s->m_Face));
            const bool spherical=adaptor.GetType()==GeomAbs_Sphere;
            if(!spherical && adaptor.GetType()!=GeomAbs_BSplineSurface){valid=false;continue;}
            SurfaceUVMapping mapping(s);
            const auto& vertices=s->pMesh3D->GetVertices();
            std::vector<bool> checked(vertices.size(),false);
            std::map<Edge,int> local_edges;
            double area=0;
            for(const auto& f:s->pMesh3D->GetFaces())if(!f.deleted) {
                ++cells;quads+=f.corners.size()==4;
                valid=valid&&(f.corners.size()==3||f.corners.size()==4);
                std::vector<size_t> ids;
                for(auto c:f.corners) {
                    auto p=vertices[c.v];
                    if(!checked[c.v]) {
                        checked[c.v]=true;
                        SurfaceUVPoint uv;
                        if(!mapping.Project(p,uv))valid=false;
                        else {
                            double residual=gp_Pnt(p.x,p.y,p.z).Distance(adaptor.Value(uv.u,uv.v));
                            valid=valid&&std::isfinite(residual)&&residual<2.e-4;
                        }
                    }
                    auto inserted=nodes.emplace(Point{p.x,p.y,p.z},nodes.size());ids.push_back(inserted.first->second);
                    snapshot<<p.x<<','<<p.y<<','<<p.z<<';';
                }
                snapshot<<'\n';
                for(size_t k=0;k<ids.size();++k) {
                    size_t a=ids[k],b=ids[(k+1)%ids.size()];auto& edge=edges[std::minmax(a,b)];
                    ++edge.first;edge.second+=a<b?1:-1;
                    ++local_edges[std::minmax(f.corners[k].v,f.corners[(k+1)%ids.size()].v)];
                }
                auto a=vertices[f.corners[0].v];
                for(size_t k=1;k+1<f.corners.size();++k) {
                    auto n=cross(vertices[f.corners[k].v]-a,vertices[f.corners[k+1].v]-a);
                    double triangle_area=.5*std::sqrt(double(dot(n,n)));
                    valid=valid&&triangle_area>1.e-10;area+=triangle_area;
                }
                if(f.corners.size()==4) {
                    auto b=vertices[f.corners[1].v],c=vertices[f.corners[2].v],d=vertices[f.corners[3].v];
                    valid=valid&&dot(cross(b-a,c-a),cross(c-a,d-a))>1.e-10;
                }
            }
            GProp_GProps props;BRepGProp::SurfaceProperties(s->m_Face,props);
            double ratio=area/props.Mass();valid=valid&&ratio>.94&&ratio<1.06;
            std::vector<double> lengths;
            for(const auto& edge:local_edges)if(edge.second==2) {
                auto d=vertices[edge.first.first]-vertices[edge.first.second];lengths.push_back(std::sqrt(double(dot(d,d))));
            }
            if(lengths.empty())valid=false;
            else if(spherical){std::sort(lengths.begin(),lengths.end());steps.push_back(lengths[lengths.size()/2]);}
            std::cout<<" face="<<i<<" areaRatio="<<ratio<<'\n';
        }
        size_t bad=0;std::map<size_t,std::vector<size_t>> adjacency;
        for(const auto& e:edges) {
            if(e.second.first!=2||e.second.second!=0)++bad;
            adjacency[e.first.first].push_back(e.first.second);adjacency[e.first.second].push_back(e.first.first);
        }
        std::set<size_t> reached;
        if(!adjacency.empty()) {
            std::vector<size_t> todo{adjacency.begin()->first};reached.insert(todo[0]);
            for(size_t k=0;k<todo.size();++k)for(auto next:adjacency[todo[k]])if(reached.insert(next).second)todo.push_back(next);
        }
        double step_ratio=steps.empty()?0:*std::max_element(steps.begin(),steps.end()) / *std::min_element(steps.begin(),steps.end());
        valid=valid&&bad==0&&cells>0&&quads>.65*cells&&reached.size()==nodes.size()&&step_ratio>0&&step_ratio<1.8;
        auto prior=repeats.find(density);if(prior!=repeats.end())valid=valid&&prior->second==snapshot.str();else repeats[density]=snapshot.str();
        std::cout<<"density="<<density<<" built="<<built<<" exactBadEdges="<<bad<<" stepRatio="<<step_ratio<<" quads="<<quads<<'/'<<cells<<" valid="<<valid<<std::endl;
    }
    return valid?0:1;
}
