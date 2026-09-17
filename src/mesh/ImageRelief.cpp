#include "ImageRelief.h"
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <sstream>

namespace image_relief {
namespace {
using V = std::array<double, 3>;
V sub(V a,V b) { return {a[0]-b[0],a[1]-b[1],a[2]-b[2]}; }
double dot(V a,V b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
V cross(V a,V b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
double length(V a) { return std::sqrt(dot(a,a)); }
struct Tri { size_t a,b,c; int surface; };
using Key=std::pair<size_t,size_t>;
Key edge(size_t a,size_t b) { return {std::min(a,b),std::max(a,b)}; }
struct Hash { size_t operator()(Key k) const { return k.first*73856093u ^ k.second*19349663u; } };
struct EdgeInfo { int count=0, balance=0, surface=-1; size_t midpoint=SIZE_MAX; };
using Edges=std::unordered_map<Key,EdgeInfo,Hash>;
Edges edges(const std::vector<Tri>& ts) {
    Edges result; result.reserve(ts.size()*2);
    for(const auto& t:ts) for(auto ab:{Key{t.a,t.b},Key{t.b,t.c},Key{t.c,t.a}}) {
        auto& e=result[edge(ab.first,ab.second)];
        if(e.count==0) e.surface=t.surface;
        else if(e.surface!=t.surface) e.surface=-2; // preserve CAD face boundaries
        ++e.count; e.balance+=ab.first<ab.second?1:-1;
    }
    return result;
}
double smooth(double x) { x=std::clamp(x,0.0,1.0);return x*x*(3-2*x); }
double sample(const QImage& image,double u,double v,bool invert) {
    // Periodic bilinear filtering blends both sides of the wrap seam.
    double x=(u-std::floor(u))*image.width()-.5;
    double y=(1-(v-std::floor(v)))*image.height()-.5;
    int ix=static_cast<int>(std::floor(x)),iy=static_cast<int>(std::floor(y));
    auto p=[&](int a,int b) {
        a=(a%image.width()+image.width())%image.width();
        b=(b%image.height()+image.height())%image.height();
        QRgb c=image.pixel(a,b); double gray=qGray(c)/255.0;
        return (invert?1-gray:gray)*qAlpha(c)/255.0;
    };
    double fx=x-std::floor(x),fy=y-std::floor(y);
    return (p(ix,iy)*(1-fx)+p(ix+1,iy)*fx)*(1-fy)
        +(p(ix,iy+1)*(1-fx)+p(ix+1,iy+1)*fx)*fy;
}
}

bool UsesSurfaceMapping(const TopoDS_Shape& shape,int face_index) {
    int index=0;
    for(TopExp_Explorer ex(shape,TopAbs_FACE);ex.More();ex.Next(),++index)if(index==face_index) {
        auto type=BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType();
        return type!=GeomAbs_Cylinder&&type!=GeomAbs_Cone&&type!=GeomAbs_Sphere
            &&type!=GeomAbs_Torus&&type!=GeomAbs_SurfaceOfRevolution;
    }
    return false;
}

Result Build(const TopoDS_Shape& input,const QImage& source,const Options& o,
             const std::function<bool(const char*)>& progress) {
    Result result;
    auto update=[&](const char* text) { if(progress&&!progress(text)) throw std::runtime_error("Cancelled"); };
    try {
        if(input.IsNull()||source.isNull()) throw std::runtime_error("Select a Solid and a readable ornament image.");
        if(!std::isfinite(o.spacing)||o.spacing<.05||!std::isfinite(o.height)||o.height<0||o.height>100
            ||o.axis<0||o.axis>2||o.repeat_u<1||o.repeat_v<1||o.repeat_u>1000||o.repeat_v>1000
            ||!std::isfinite(o.rotation_degrees)||!std::isfinite(o.lower_percent)||!std::isfinite(o.upper_percent)
            ||o.lower_percent<0||o.upper_percent>100||o.lower_percent>=o.upper_percent
            ||!std::isfinite(o.fade_mm)||o.fade_mm<=0||o.max_triangles<4)
            throw std::runtime_error("Invalid relief or mesh parameters.");
        update("Meshing a copy of the Solid...");
        if(!BRepCheck_Analyzer(input).IsValid()) throw std::runtime_error("The input Solid is not valid.");
        TopoDS_Shape shape=BRepBuilderAPI_Copy(input,true,false).Shape();
        const bool surface_mapping=UsesSurfaceMapping(shape,o.face_index);
        double u0=0,u1=0,v0=0,v1=0,uscale=1,vscale=1;
        BRepTools::Clean(shape);
        BRepMesh_IncrementalMesh mesher(shape,std::max(.001,o.spacing*.025),false,.15,true);
        if(!mesher.IsDone()) throw std::runtime_error("The Solid could not be triangulated.");
        std::vector<V> vertices;
        std::vector<Tri> triangles;
        std::vector<Handle(Geom_Surface)> surfaces;
        std::vector<std::unique_ptr<GeomAPI_ProjectPointOnSurf>> projectors;
        // Quantised neighbour search joins both periodic and inter-face seams.
        using Cell=std::array<long long,3>;
        std::map<Cell,std::vector<size_t>> cells;
        constexpr double tolerance=1e-5;
        auto weld=[&](V p) {
            Cell k;for(int a=0;a<3;++a) k[a]=static_cast<long long>(std::floor(p[a]/tolerance));
            for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z) {
                auto found=cells.find({k[0]+x,k[1]+y,k[2]+z});
                if(found!=cells.end())for(size_t i:found->second) if(length(sub(p,vertices[i]))<=tolerance) return i;
            }
            size_t id=vertices.size();vertices.push_back(p);cells[k].push_back(id);return id;
        };
        for(TopExp_Explorer ex(shape,TopAbs_FACE);ex.More();ex.Next()) {
            update("Joining surface seams...");
            auto face=TopoDS::Face(ex.Current());TopLoc_Location loc;
            auto tri=BRep_Tool::Triangulation(face,loc);
            if(tri.IsNull()) throw std::runtime_error("A Solid face has no triangulation.");
            int sid=static_cast<int>(surfaces.size());surfaces.push_back(BRep_Tool::Surface(face));
            auto projector=std::make_unique<GeomAPI_ProjectPointOnSurf>();
            if(!surfaces.back().IsNull()&&(!o.preview||surface_mapping)&&(o.face_index<0||sid==o.face_index)) {
                double ua,ub,va,vb;BRepTools::UVBounds(face,ua,ub,va,vb);
                projector->Init(surfaces.back(),ua,ub,va,vb,1e-6);
                if(surface_mapping&&sid==o.face_index) {
                    u0=ua;u1=ub;v0=va;v1=vb;
                    gp_Pnt p;gp_Vec du,dv;surfaces.back()->D1((ua+ub)/2,(va+vb)/2,p,du,dv);
                    uscale=du.Magnitude();vscale=dv.Magnitude();
                    if(ub-ua<1e-10||vb-va<1e-10||uscale<1e-10||vscale<1e-10)
                        throw std::runtime_error("The selected surface has degenerate image coordinates.");
                }
            }
            projectors.push_back(std::move(projector));
            std::vector<size_t> ids(tri->NbNodes()+1);
            for(int n=1;n<=tri->NbNodes();++n) {auto p=tri->Node(n).Transformed(loc.Transformation());ids[n]=weld({p.X(),p.Y(),p.Z()});}
            for(int n=1;n<=tri->NbTriangles();++n) {
                int a,b,c;tri->Triangle(n).Get(a,b,c);if(face.Orientation()==TopAbs_REVERSED) std::swap(b,c);
                if(ids[a]!=ids[b]&&ids[b]!=ids[c]&&ids[a]!=ids[c])triangles.push_back({ids[a],ids[b],ids[c],sid});
            }
            if(triangles.size()>o.max_triangles) throw std::runtime_error("Mesh limit exceeded. Increase the mesh spacing.");
        }
        cells.clear();
        if(triangles.empty()) throw std::runtime_error("The Solid has no triangles.");
        if(o.face_index < -1 || o.face_index>=static_cast<int>(surfaces.size()))
            throw std::runtime_error("Select a valid face for the relief.");
        result.closed=true;
        for(const auto& e:edges(triangles)) {
            if(e.second.count==1&&o.allow_open){result.closed=false;continue;}
            if(e.second.count!=2||e.second.balance!=0)
                throw std::runtime_error("The Solid mesh is not closed or has inconsistent face orientation.");
        }
        // Split shared edges once. Every adjacent triangle uses the same midpoint.
        bool complete=false;
        for(int pass=0;pass<24;++pass) {
            auto report=[&](size_t done,size_t total) {
                std::ostringstream s;s<<"Refine "<<(pass+1)<<": "<<triangles.size()<<" triangles, "
                    <<(total?100*done/total:0)<<"%";update(s.str().c_str());
            };
            report(0,1);
            auto es=edges(triangles);size_t split=0,work=0;
            std::unordered_map<Key,bool,Hash> target;
            if(o.face_index>=0)for(const auto& t:triangles)if(t.surface==o.face_index)
                for(Key k:{edge(t.a,t.b),edge(t.b,t.c),edge(t.c,t.a)})target.emplace(k,true);
            size_t previous_vertices=vertices.size();bool preview_limit=false;
            for(auto& item:es) {
                if((++work%1024)==0)report(work,es.size());
                if(o.face_index>=0&&target.find(item.first)==target.end())continue;
                V a=vertices[item.first.first],b=vertices[item.first.second];
                if(length(sub(a,b))<=o.spacing*(1+1e-8))continue;
                if(triangles.size()+2*(split+1)>o.max_triangles) {
                    if(o.preview){preview_limit=true;break;}
                    throw std::runtime_error("Mesh limit exceeded. Increase the mesh spacing.");
                }
                V p={(a[0]+b[0])/2,(a[1]+b[1])/2,(a[2]+b[2])/2};
                int sid=item.second.surface;
                if(sid>=0&&!surfaces[sid].IsNull()&&!o.preview) {
                    auto& projection=*projectors[sid];projection.Perform(gp_Pnt(p[0],p[1],p[2]));
                    if(projection.NbPoints()>0) {auto q=projection.NearestPoint();p={q.X(),q.Y(),q.Z()};}
                }
                item.second.midpoint=vertices.size();vertices.push_back(p);++split;
            }
            if(preview_limit){vertices.resize(previous_vertices);complete=true;break;}
            if(!split) {complete=true;break;}
            std::vector<Tri> refined;refined.reserve(triangles.size()+2*split);
            for(auto t:triangles) {
                size_t a=t.a,b=t.b,c=t.c;
                size_t ab=es.at(edge(a,b)).midpoint,bc=es.at(edge(b,c)).midpoint,ca=es.at(edge(c,a)).midpoint;
                int mask=(ab!=SIZE_MAX?1:0)|(bc!=SIZE_MAX?2:0)|(ca!=SIZE_MAX?4:0);
                auto add=[&](size_t i,size_t j,size_t k) {refined.push_back({i,j,k,t.surface});};
                switch(mask) {
                case 0:add(a,b,c);break;
                case 1:add(a,ab,c);add(ab,b,c);break;
                case 2:add(b,bc,a);add(bc,c,a);break;
                case 4:add(c,ca,b);add(ca,a,b);break;
                case 3:add(b,bc,ab);add(a,ab,c);add(ab,bc,c);break;
                case 6:add(c,ca,bc);add(b,bc,a);add(bc,ca,a);break;
                case 5:add(a,ab,ca);add(c,ca,b);add(ca,ab,b);break;
                case 7:add(a,ab,ca);add(ab,b,bc);add(ca,bc,c);add(ab,bc,ca);break;
                }
            }
            triangles.swap(refined);
        }
        if(!complete)throw std::runtime_error("Could not reach the requested mesh spacing.");
        std::vector<V> normals(vertices.size(),V{0,0,0});
        std::vector<bool> target_vertex(vertices.size(),o.face_index<0),protected_vertex(vertices.size(),false);
        V lo=vertices.front(),hi=lo;
        for(V p:vertices)for(int a=0;a<3;++a){lo[a]=std::min(lo[a],p[a]);hi[a]=std::max(hi[a],p[a]);}
        for(const auto& t:triangles) {
            V n=cross(sub(vertices[t.b],vertices[t.a]),sub(vertices[t.c],vertices[t.a]));
            for(size_t i:{t.a,t.b,t.c})for(int a=0;a<3;++a)normals[i][a]+=n[a];
            if(o.face_index>=0)for(size_t i:{t.a,t.b,t.c}) {
                if(t.surface==o.face_index)target_vertex[i]=true;else protected_vertex[i]=true;
            }
            if(o.face_index<0||t.surface==o.face_index)
                result.max_base_edge=std::max({result.max_base_edge,length(sub(vertices[t.a],vertices[t.b])),length(sub(vertices[t.b],vertices[t.c])),length(sub(vertices[t.c],vertices[t.a]))});
        }
        // Pin open and shared CAD boundaries so adjacent faces cannot acquire relief.
        if(o.face_index>=0)for(const auto& e:edges(triangles))if(e.second.count==1)
            protected_vertex[e.first.first]=protected_vertex[e.first.second]=true;
        const int z=o.axis,x=(z+1)%3,y=(z+2)%3;
        double zlo=lo[z],zhi=hi[z];
        if(o.face_index>=0){zlo=hi[z];zhi=lo[z];for(size_t i=0;i<vertices.size();++i)if(target_vertex[i]){zlo=std::min(zlo,vertices[i][z]);zhi=std::max(zhi,vertices[i][z]);}}
        if(surface_mapping){zlo=v0*vscale;zhi=v1*vscale;}
        double bottom=zlo+(zhi-zlo)*o.lower_percent/100;
        double top=zlo+(zhi-zlo)*o.upper_percent/100;
        if(top-bottom<1e-6)throw std::runtime_error("The relief height range is empty.");
        QImage image=source.convertToFormat(QImage::Format_ARGB32);
        std::vector<Vec3> output;output.reserve(vertices.size());
        for(size_t i=0;i<vertices.size();++i) {
            if(i%4096==0)update("Applying image relief...");
            V p=vertices[i],n=normals[i];double len=length(n);if(len>1e-15)for(double& q:n)q/=len;
            double rx=p[x]-(lo[x]+hi[x])/2,ry=p[y]-(lo[y]+hi[y])/2,r=std::hypot(rx,ry);
            double outward=r>1e-10?(n[x]*rx+n[y]*ry)/r:0;
            double mask=(o.face_index>=0?1:smooth((outward-.05)/.3))*smooth((p[z]-bottom)/o.fade_mm)*smooth((top-p[z])/o.fade_mm);
            if(!target_vertex[i]||protected_vertex[i])mask=0;
            double u=(std::atan2(ry,rx)/(2*3.141592653589793)+o.rotation_degrees/360)*o.repeat_u;
            double v=(p[z]-bottom)/(top-bottom)*o.repeat_v;
            if(surface_mapping&&target_vertex[i]&&!protected_vertex[i]) {
                auto& projection=*projectors[o.face_index];projection.Perform(gp_Pnt(p[0],p[1],p[2]));
                if(projection.NbPoints()==0)throw std::runtime_error("Could not map image coordinates on the selected face.");
                double su,sv;projection.LowerDistanceParameters(su,sv);
                mask=smooth((su-u0)*uscale/o.fade_mm)*smooth((u1-su)*uscale/o.fade_mm)
                    *smooth((sv*vscale-bottom)/o.fade_mm)*smooth((top-sv*vscale)/o.fade_mm);
                double a=(su-u0)/(u1-u0)-.5,b=(sv*vscale-bottom)/(top-bottom)-.5;
                double angle=o.rotation_degrees*3.141592653589793/180;
                u=(a*std::cos(angle)-b*std::sin(angle)+.5)*o.repeat_u;
                v=(a*std::sin(angle)+b*std::cos(angle)+.5)*o.repeat_v;
            }
            double d=o.height*sample(image,u,v,o.invert)*mask;
            for(int a=0;a<3;++a)p[a]+=n[a]*d;
            output.push_back({static_cast<float>(p[0]),static_cast<float>(p[1]),static_cast<float>(p[2])});
        }
        update("Checking the relief mesh...");
        std::vector<CMesh3D::Face> faces;faces.reserve(triangles.size());
        for(const auto& t:triangles) {
            auto get=[&](size_t i)->V {return {output[i].x,output[i].y,output[i].z};};
            V before=cross(sub(vertices[t.b],vertices[t.a]),sub(vertices[t.c],vertices[t.a]));
            V after=cross(sub(get(t.b),get(t.a)),sub(get(t.c),get(t.a)));
            if(length(after)<1e-12||dot(before,after)<=0)throw std::runtime_error("Relief folds a triangle. Reduce the relief height or use a smoother image.");
            CMesh3D::Face f{t.a,t.b,t.c};f.sourceFaceId=t.surface;faces.push_back(std::move(f));
        }
        result.mesh=std::make_unique<CMesh3D>("Image Relief");
        if(!result.mesh->SetGeometry(std::move(output),std::move(faces)))throw std::runtime_error("Could not create the relief mesh.");
        result.triangles=triangles.size();
    } catch(const Standard_Failure& e) {result.error=e.GetMessageString()?e.GetMessageString():"CAD meshing failed.";result.mesh.reset();}
      catch(const std::exception& e) {result.error=e.what();result.mesh.reset();}
    return result;
}
} // namespace image_relief
