#include "ObjIO.h"
#include <BRepFilletAPI_MakeFillet.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include "solid/LowPolySharpEdges.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include "Tools_Mesh3D.h"
#include "CPolyline.h"
#include "ContourQuadrangulator3DCoat.h"
#include "solid/SurfaceFace.h"
#include "solid/Solid.h"
#include "solid/ConvexMeshMerge.h"
#include "solid/MeshBoundaryEdges.h"
#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepLib.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <GeomConvert.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include "SurfaceUVMapping.h"
#include <TColgp_Array2OfPnt.hxx>

#include <QTemporaryDir>
#include <cstdlib>
#include <sstream>
#include "UndoRedo.h"
#include "ui/OpenGLViewport.h"
#include <QApplication>
#include <QDir>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <cmath>
#include <chrono>

namespace {
void Check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string Read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    Check(bool(stream), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void CheckClosedQuadPatch(const Tools_Mesh3D::FillContourResult& result)
{
    const auto& vertices = result.mesh->GetVertices();
    const auto& faces = result.mesh->GetFaces();
    std::map<std::pair<size_t, size_t>, int> edges;
    std::map<std::pair<size_t, size_t>, int> directions;
    std::map<size_t, std::set<size_t>> neighbours;
    double mesh_area = 0.0;
    for (const auto& face : faces) {
        if (face.deleted) continue;
        Check(face.corners.size() == 4, "UV patch must contain only quads.");
        double area = 0.0;
        for (size_t i = 0; i < face.corners.size(); ++i) {
            const size_t a = face.corners[i].v;
            const size_t b = face.corners[(i + 1) % face.corners.size()].v;
            Check(a < vertices.size() && b < vertices.size() && a != b,
                "Invalid quad vertex index.");
            const auto key = std::minmax(a, b);
            ++edges[key];
            directions[key] += a < b ? 1 : -1;
            neighbours[a].insert(b);
            neighbours[b].insert(a);
            area += double(vertices[a].x) * vertices[b].y
                - double(vertices[b].x) * vertices[a].y;
        }
        Check(area > 0.0, "Quad has zero or reversed UV area.");
        mesh_area += area;
    }
    std::vector<int> input_index(vertices.size(), -1);
    for (size_t i = 0; i < vertices.size(); ++i)
        for (size_t j = 0; j < result.boundary.size(); ++j)
            if (vertices[i].x == result.boundary[j].x
                && vertices[i].y == result.boundary[j].y
                && vertices[i].z == result.boundary[j].z)
                input_index[i] = static_cast<int>(j);
    size_t boundary_count = 0;
    for (const auto& entry : edges) {
        Check(entry.second <= 2, "Non-manifold quad edge.");
        if (entry.second == 2) {
            Check(directions[entry.first] == 0, "Interior quad edge orientation mismatch.");
            continue;
        }
        ++boundary_count;
        const int a = input_index[entry.first.first];
        const int b = input_index[entry.first.second];
        const int count = static_cast<int>(result.boundary.size());
        Check(a >= 0 && b >= 0 && ((a + 1) % count == b || (b + 1) % count == a),
            "Quad patch has an open interior front (the UV slit).");
    }
    Check(boundary_count == result.boundary.size(), "Quad patch lost boundary edges.");
    std::set<size_t> visited;
    std::vector<size_t> queue{neighbours.begin()->first};
    for (size_t i = 0; i < queue.size(); ++i)
        if (visited.insert(queue[i]).second)
            for (size_t next : neighbours[queue[i]]) queue.push_back(next);
    Check(visited.size() == neighbours.size(), "Quad patch is disconnected.");
    double boundary_area = 0.0;
    for (size_t i = 0; i < result.boundary.size(); ++i) {
        const auto& a = result.boundary[i];
        const auto& b = result.boundary[(i + 1) % result.boundary.size()];
        boundary_area += double(a.x) * b.y - double(b.x) * a.y;
    }
    Check(std::abs(mesh_area - boundary_area) < std::abs(boundary_area) * 1.0e-6,
        "Quad patch does not cover the input UV area.");
}
} // namespace

void TestSlxHoleScope(const char* path)
{
    const auto signature = [](const CMesh3D& mesh) {
        std::ostringstream out; out.precision(9);
        out << mesh.GetVertices().size() << '|';
        for (const auto& f:mesh.GetFaces()) if (!f.deleted) {
            for (const auto& c:f.corners) out << c.v << ',';
            out << ';';
        }
        return out.str();
    };
    const auto verify = [&](CSolid& solid, float density, bool expect_holes) {
        solid.MeshQuadro=true; solid.MeshQuadroHoleDivideFace=false; solid.MeshQuadroTrimByPline=false;
        std::vector<std::string> baseline;
        std::vector<std::vector<Vec3>> positions;
        std::vector<bool> holes;
        for (bool slx:{false,true}) {
            solid.MeshQuadroHoleSLX=slx;
            Check(solid.ReBuldMesh(1.f/density),"SLX scope rebuild failed");
            size_t checked=0,cut=0;
            for (int i=0;i<solid.GetNumSurfaces();++i) {
                const auto* surface=solid.GetSurfaceFace(i);
                Check(surface && surface->IsInitMesh && surface->pMesh3D,"SLX scope missing face");
                int wires=0;
                for (TopExp_Explorer w(surface->m_Face,TopAbs_WIRE);w.More();w.Next()) ++wires;
                if (!slx) {baseline.push_back(signature(*surface->pMesh3D));positions.push_back(surface->pMesh3D->GetVertices());holes.push_back(wires>1);}
                if (!holes[i]) {
                    ++checked;
                    Check(!surface->UsedSlxHoleCut(),"SLX was used on an unperforated face");
                    Check(signature(*surface->pMesh3D)==baseline[i],"SLX changed an unperforated face: surface="+std::to_string(i)+" density="+std::to_string(density));
                    // Final seam averaging may differ by a float rounding unit
                    // when a perforated neighbour has different cell ordering.
                    const auto& current=surface->pMesh3D->GetVertices();
                    for (size_t v=0;v<current.size();++v) {
                        const auto delta=current[v]-positions[i][v];
                        Check(dot(delta,delta)<1.e-10f,"SLX moved an unperforated face vertex");
                    }
                }
                cut+=surface->UsedSlxHoleCut();
            }
            Check(checked>0,"SLX scope did not compare any ordinary faces");
            if (slx && expect_holes) Check(cut>0,"SLX no longer cuts the perforated face");
        }
    };
    for (auto shape:{BRepPrimAPI_MakeBox(20,30,40).Shape(),
        BRepPrimAPI_MakeCylinder(10,25).Shape(),BRepPrimAPI_MakeSphere(10).Shape()}) {
        CSolid solid(shape);
        Check(solid.InitSurfaces() && solid.InitEdges(),"SLX primitive initialization failed");
        verify(solid,.25f,false);
    }
    CAlfaDoc document; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    Check(serializer.Load(QString::fromLocal8Bit(path),document,room,view,error),error.toStdString());
    size_t count=0;
    for (const auto& object:document.GetObjects()) if (auto* solid=dynamic_cast<CSolid*>(object.get())) {
        ++count;
        for (float density:{.1f,.25f,.5f}) verify(*solid,density,true);
    }
    Check(count==1,"Unexpected SLX scope fixture");
    std::cout << "SLX scope: ordinary face geometry/topology identical; perforated face uses SLX\n";
}

void TestExtrudeCornerQuads(const char* path)
{
    CAlfaDoc document; Dom3DProjectSerializer serializer;
    QString room, error; ProjectViewState view;
    Check(serializer.Load(QString::fromLocal8Bit(path),document,room,view,error),error.toStdString());
    CSolid* body = nullptr;
    for (const auto& object : document.GetObjects())
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            Check(!body,"Ambiguous extrude fixture"); body = solid;
        }
    Check(body && body->GetNumSurfaces() == 27,"Unexpected extrude fixture");
    body->MeshQuadro = true; body->MeshQuadroTrimByPline = false;
    body->MeshQuadroHoleDivideFace = false;
    for (bool slx : {false,true}) for (float density : {.25f,.5f,1.f}) {
        body->MeshQuadroHoleSLX = slx;
        Check(body->ReBuldMesh(1.f/density),"Extrude rebuild failed");
        std::vector<const CMesh3D*> parts;
        size_t repaired = 0;
        for (int index = 0; index < body->GetNumSurfaces(); ++index) {
            const auto* surface = body->GetSurfaceFace(index);
            Check(surface->IsInitMesh && surface->pMesh3D,"Missing extrude surface");
            const auto& mesh = *surface->pMesh3D;
            parts.push_back(&mesh);
            BRepAdaptor_Surface geometry(TopoDS::Face(surface->m_Face));
            if (geometry.GetType() != GeomAbs_Cylinder || surface->m_TypeMesh == REGULAR_MESH) continue;
            ++repaired;
            std::map<std::pair<size_t,size_t>,int> patch_edges;
            for (const auto& face : mesh.GetFaces()) if (!face.deleted) {
                Check(face.corners.size() == 4,"Corner fillet contains a diagonal/triangle");
                const auto& v = mesh.GetVertices();
                const auto a=v[face.corners[0].v], b=v[face.corners[1].v],
                    c=v[face.corners[2].v], d=v[face.corners[3].v];
                Check(dot(cross(b-a,c-a),cross(c-a,d-a))>1.e-12,"Folded corner quad");
                Check(dot(cross(b-a,c-a),mesh.GetNormals()[face.corners[0].n])>0,"Reversed corner quad");
                for (size_t k=0;k<4;++k)
                    ++patch_edges[std::minmax(face.corners[k].v,face.corners[(k+1)%4].v)];
            }
            std::set<size_t> boundary;
            for (const auto& [edge,count] : patch_edges) if (count==1) {
                boundary.insert(edge.first); boundary.insert(edge.second);
            }
            // Every original sample must still occur on the mesh boundary;
            // neighbouring CAD faces use those same samples to stay watertight.
            for (int side=0;side<surface->GetPreparedPolylineCount();++side) {
                std::vector<CPoint3d> samples;
                Check(surface->GetPreparedPolylinePoints(side,samples),"Missing prepared side");
                for (const auto& p:samples) {
                    double nearest=1.e30;
                    for (size_t i:boundary) {
                        const auto q=mesh.GetVertices()[i];
                        nearest=std::min(nearest,(p.x-q.x)*(p.x-q.x)+(p.y-q.y)*(p.y-q.y)+(p.z-q.z)*(p.z-q.z));
                    }
                    Check(nearest<1.e-9,"Corner repair moved a shared boundary node");
                }
            }
            SurfaceUVMapping mapping(surface);
            for (const auto& p:mesh.GetVertices()) {
                SurfaceUVPoint uv;
                Check(mapping.Project(p,uv),"Corner UV projection failed");
                const auto exact=geometry.Value(uv.u,uv.v);
                Check(exact.Distance(gp_Pnt(p.x,p.y,p.z))<1.e-4,"Corner node off CAD cylinder");
            }
        }
        Check(repaired==4,"Expected four trimmed cylindrical fillets");
        auto welded=CMesh3D::CreateWelded(parts);
        Check(bool(welded),"Extrude weld failed");
        std::map<std::pair<size_t,size_t>,int> edges;
        for (const auto& face:welded->GetFaces()) if (!face.deleted)
            for (size_t k=0;k<face.corners.size();++k)
                ++edges[std::minmax(face.corners[k].v,face.corners[(k+1)%face.corners.size()].v)];
        for (const auto& [edge,count]:edges) Check(count==2,"Extrude mesh has an open/non-manifold seam");
        std::cout << "Extrude corner quads: density=" << density << " slx=" << slx << " passed\n";
    }
}

void TestDemosMesh(const char* path)
{
    CAlfaDoc document; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    Check(serializer.Load(QString::fromLocal8Bit(path),document,room,view,error),error.toStdString());
    CSolid* body=nullptr;
    for(const auto& o:document.GetObjects()) if(auto* s=dynamic_cast<CSolid*>(o.get()); s && (s->GetNumSurfaces()>100 || s->GetNumSurfaces()==5 || s->GetNumSurfaces()==11)) {
        Check(!body,"Ambiguous Demos body"); body=s;
    }
    Check(body,"Missing Demos CAD body");
    if(body->GetNumSurfaces()==11) {
        body->MeshQuadro=true; body->MeshQuadroTrimByPline=true;
        for(bool divide:{false,true}) for(float density:{.25f,.5f,1.f}) {
            body->MeshQuadroHoleSLX=!divide;body->MeshQuadroHoleDivideFace=divide;
            Check(body->ReBuldMesh(1.f/density),"Rectangular pocket rebuild failed");
            Vec3 low{1.e30f,1.e30f,1.e30f},high{-1.e30f,-1.e30f,-1.e30f};
            for(int i=0;i<body->GetNumSurfaces();++i)for(const auto& f:body->GetSurfaceFace(i)->pMesh3D->GetFaces())if(!f.deleted)for(auto c:f.corners) {
                const auto p=body->GetSurfaceFace(i)->pMesh3D->GetVertices()[c.v];
                low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
                high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);
            }
            std::vector<const CMesh3D*> parts;
            size_t outside_count=0;
            for(int i=0;i<body->GetNumSurfaces();++i) {
                auto* surface=body->GetSurfaceFace(i);
                Check(surface->IsInitMesh && surface->pMesh3D,"Missing pocket face");
                const auto& mesh=*surface->pMesh3D;parts.push_back(&mesh);
                // Internal pocket walls may be split to conform to the cut.
                // This regression targets the six outside planes of the box.
                bool outside=false;
                for(int axis=0;axis<3;++axis)for(bool upper:{false,true}) {
                    const auto coord=[axis](Vec3 p){return axis==0?p.x:axis==1?p.y:p.z;};
                    const float boundary=coord(upper?high:low);
                    bool on_plane=true;
                    for(const auto& f:mesh.GetFaces())if(!f.deleted)for(auto c:f.corners)
                        on_plane &= std::abs(coord(mesh.GetVertices()[c.v])-boundary)<1.e-4;
                    outside|=on_plane;
                }
                if(!outside)continue;
                ++outside_count;
                SurfaceUVMapping mapping(surface);
                std::vector<SurfaceUVPoint> points;
                double u0=1.e30,u1=-1.e30,v0=1.e30,v1=-1.e30;
                for(auto p:mesh.GetVertices()) {
                    SurfaceUVPoint uv{};Check(mapping.Project(p,uv),"Pocket UV projection");points.push_back(uv);
                }
                for(const auto& f:mesh.GetFaces())if(!f.deleted)for(auto c:f.corners) {
                    auto uv=points[c.v];u0=std::min(u0,uv.u);u1=std::max(u1,uv.u);v0=std::min(v0,uv.v);v1=std::max(v1,uv.v);
                }
                const double eps=std::max(u1-u0,v1-v0)*1.e-5;
                for(const auto& face:mesh.GetFaces()) if(!face.deleted) {
                    bool boundary=false;
                    for(auto c:face.corners) {auto p=points[c.v];boundary|=std::min({std::abs(p.u-u0),std::abs(p.u-u1),std::abs(p.v-v0),std::abs(p.v-v1)})<eps;}
                    if(!boundary)continue;
                    Check(face.corners.size()==4,"Pocket outer boundary contains triangles: surface="+std::to_string(i));
                    for(size_t k=0;k<4;++k) {
                        auto a=points[face.corners[(k+3)%4].v],b=points[face.corners[k].v],c=points[face.corners[(k+1)%4].v];
                        const double x=a.u-b.u,y=a.v-b.v,dx=c.u-b.u,dy=c.v-b.v;
                        Check(std::abs(x*dy-y*dx)>.5*std::hypot(x,y)*std::hypot(dx,dy),"Distorted pocket boundary cell");
                    }
                }
            }
            Check(outside_count==6,"Pocket test must check all six outside planes");
            auto welded=CMesh3D::CreateWelded(parts);Check(bool(welded),"Pocket weld failed");
            std::map<std::pair<size_t,size_t>,int> edges;
            for(const auto& f:welded->GetFaces()) if(!f.deleted) for(size_t k=0;k<f.corners.size();++k)
                ++edges[std::minmax(f.corners[k].v,f.corners[(k+1)%f.corners.size()].v)];
            for(auto e:edges)Check(e.second==2,"Pocket mesh contains an open seam");
        }
        return;
    }
    if(body->GetNumSurfaces()==5) {
        body->MeshQuadro=true; body->MeshQuadroTrimByPline=false;
        for(int mode:{0,1,2}) for(float density:{.3f,.5f,1.f}) {
            body->MeshQuadroHoleSLX=mode==1; body->MeshQuadroHoleDivideFace=mode==2;
            Check(body->ReBuldMesh(1.f/density),"Hemisphere rebuild failed");
            std::vector<const CMesh3D*> parts; size_t spheres=0;
            for(int i=0;i<body->GetNumSurfaces();++i) {
                auto* s=body->GetSurfaceFace(i);
                Check(s->IsInitMesh && s->pMesh3D,"Hemisphere missing surface");
                const auto& mesh=*s->pMesh3D; parts.push_back(&mesh);
                BRepAdaptor_Surface geometry(TopoDS::Face(s->m_Face));
                const bool spherical=geometry.GetType()==GeomAbs_Sphere; spheres+=spherical;
                size_t count=0;
                for(const auto& f:mesh.GetFaces()) if(!f.deleted) {
                    ++count; Check(f.corners.size()==4,"Hemisphere must be all-quad: mode="+std::to_string(mode)+" density="+std::to_string(density)+" surface="+std::to_string(i));
                    const auto& v=mesh.GetVertices(); double smallest=1.e30,largest=0;
                    for(size_t k=0;k<4;++k) {
                        const auto a=v[f.corners[k].v],b=v[f.corners[(k+1)%4].v];
                        const double length=std::sqrt(dot(b-a,b-a));
                        Check(length>1.e-6,"Collapsed hemisphere edge");
                        smallest=std::min(smallest,length);largest=std::max(largest,length);
                        if(spherical) {
                            const auto sphere=geometry.Sphere();const auto center=sphere.Location();
                            const gp_Pnt point(a.x,a.y,a.z);
                            Check(std::abs(point.Distance(center)-sphere.Radius())<sphere.Radius()*1.e-5,"Hemisphere vertex off sphere");
                        }
                    }
                    if(spherical) Check(largest/smallest<3,"Hemisphere has stretched pole strips");
                    const auto a=v[f.corners[0].v],b=v[f.corners[1].v],c=v[f.corners[2].v],d=v[f.corners[3].v];
                    Check(dot(cross(b-a,c-a),cross(c-a,d-a))>0,"Folded hemisphere quad");
                }
                Check(count>0,"Empty hemisphere surface");
            }
            Check(spheres==4,"Hemisphere fixture must retain its four spherical patches");
            auto welded=CMesh3D::CreateWelded(parts);Check(bool(welded),"Hemisphere weld failed");
            std::map<std::pair<size_t,size_t>,std::pair<int,int>> edges;
            for(const auto& f:welded->GetFaces()) if(!f.deleted) for(size_t k=0;k<f.corners.size();++k) {
                const auto a=f.corners[k].v,b=f.corners[(k+1)%f.corners.size()].v;
                auto& e=edges[std::minmax(a,b)];++e.first;e.second+=a<b?1:-1;
            }
            for(auto e:edges) Check(e.second.first==2 && e.second.second==0,"Hemisphere seam open or inconsistently oriented");
        }
        return;
    }
    const bool gear=body->GetNumSurfaces()==109;
    Check(gear || body->GetNumSurfaces()==635,"Unexpected Demos fixture");
    body->MeshQuadro=true; body->MeshQuadroTrimByPline=false;
    // Keep the large 635-face assembly bounded; test its tip at finer steps
    // separately instead of refining every unrelated surface in the assembly.
    if(!gear) {
        auto shape=body->GetSurfaceFace(0)->m_Face; CSolid tip(shape);
        Check(tip.InitSurfaces() && tip.InitEdges(),"Isolated cone initialization");
        tip.MeshQuadro=true;
        for(bool divide:{false,true}) for(float density:{1.f,2.f}) {
            tip.MeshQuadroHoleSLX=!divide; tip.MeshQuadroHoleDivideFace=divide;
            Check(tip.ReBuldMesh(1.f/density),"Fine conical sector rebuild failed");
            Check(tip.GetSurfaceFace(0)->pMesh3D && !tip.GetSurfaceFace(0)->pMesh3D->GetFaces().empty(),"Fine cone missing cells");
        }
    }
    for(bool divide:{false,true}) for(float density:(gear ? std::vector<float>{.3f,.5f,1.f}:std::vector<float>{.3f,.5f})) {
        body->MeshQuadroHoleSLX=!divide; body->MeshQuadroHoleDivideFace=divide;
        Check(body->ReBuldMesh(1.f/density),"Demos rebuild failed");
        std::vector<const CMesh3D*> parts; size_t cone_cells=0,active=0;
        for(int i=0;i<body->GetNumSurfaces();++i) {
            const auto* s=body->GetSurfaceFace(i);
            Check(s->IsInitMesh && s->pMesh3D,"Demos missing surface");
            const auto& mesh=*s->pMesh3D; parts.push_back(&mesh);
            const bool cone=BRepAdaptor_Surface(TopoDS::Face(s->m_Face)).GetType()==GeomAbs_Cone;
            size_t cells=0;
            for(const auto& f:mesh.GetFaces()) if(!f.deleted) {
                ++cells; ++active; cone_cells+=cone;
                const auto& v=mesh.GetVertices();
                for(size_t k=0;k<f.corners.size();++k) {
                    const auto d=v[f.corners[k].v]-v[f.corners[(k+1)%f.corners.size()].v];
                    // Nist also contains pre-existing structured singular
                    // patches; this regression targets its conical tips.
                    if(gear || cone) Check(dot(d,d)>0,"Demos zero-length repaired cell edge");
                }
                if(cone) {
                    const auto a=v[f.corners[0].v],b=v[f.corners[1].v],c=v[f.corners[2].v];
                    Check(dot(cross(b-a,c-a),cross(b-a,c-a))>1.e-12,"Degenerate conical tip cell");
                }
            }
            if(cone || (gear && (i==1 || i==3))) Check(cells>0,"Missing repaired surface cells");
        }
        Check(active>0 && (gear || cone_cells>=148),"Demos repair coverage missing");
        auto welded=CMesh3D::CreateWelded(parts); Check(bool(welded),"Demos welding failed");
        std::map<std::pair<size_t,size_t>,int> edges;
        for(const auto& f:welded->GetFaces()) if(!f.deleted) for(size_t k=0;k<f.corners.size();++k)
            ++edges[std::minmax(f.corners[k].v,f.corners[(k+1)%f.corners.size()].v)];
        size_t unmatched=0; for(auto e:edges) unmatched+=e.second!=2;
        std::cout<<"Demos gear="<<gear<<" divide="<<divide<<" density="<<density<<" unmatched="<<unmatched<<std::endl;
        if(gear) Check(unmatched==0,"Gear mesh must be closed and manifold");
    }
}

void TestDivideFace(const char* output_directory)
{
    // A four-edge trimmed BSpline is not necessarily its UV bounding box.
    // The former half-span tolerance filled the missing trapezoid corners.
    {
        TColgp_Array2OfPnt poles(1,2,1,2);
        for (int u=1;u<=2;++u) for (int v=1;v<=2;++v)
            poles.SetValue(u,v,gp_Pnt((u-1)*10.,(v-1)*10.,0));
        Handle(Geom_BSplineSurface) geometry=GeomConvert::SurfaceToBSplineSurface(new Geom_BezierSurface(poles));
        const std::array<gp_Pnt2d,4> uv{{{0,0},{1,0},{.8,1},{.2,1}}};
        BRepBuilderAPI_MakeWire wire;
        for (int i=0;i<4;++i)
            wire.Add(BRepBuilderAPI_MakeEdge(GCE2d_MakeSegment(uv[i],uv[(i+1)%4]).Value(),geometry).Edge());
        auto shape=BRepBuilderAPI_MakeFace(geometry,wire.Wire(),true).Shape();
        BRepLib::BuildCurves3d(shape);
        CSolid body(shape);
        Check(body.InitSurfaces() && body.InitEdges(),"Trimmed BSpline initialization");
        Check(!body.GetSurfaceFace(0)->HasRectangularUVBoundary(),"Oblique trim accepted as natural UV rectangle");
        body.MeshQuadro=true;
        for (bool divide : {false,true}) {
            body.MeshQuadroHoleDivideFace=divide;
            Check(body.ReBuldMesh(2.f),"Trimmed BSpline meshing");
            auto* surface=body.GetSurfaceFace(0);
            Check(surface->pMesh3D,"Trimmed BSpline mesh missing");
            const auto& mesh=*surface->pMesh3D;
            SurfaceUVMapping mapping(surface);
            double area=0;
            for (const auto& face:mesh.GetFaces()) if (!face.deleted) {
                for (size_t i=0;i<face.corners.size();++i) {
                    const auto a=mesh.GetVertices()[face.corners[i].v];
                    const auto b=mesh.GetVertices()[face.corners[(i+1)%face.corners.size()].v];
                    SurfaceUVPoint point;
                    Check(mapping.Project(a,point),"Trimmed BSpline vertex projection");
                    BRepClass_FaceClassifier classifier(TopoDS::Face(shape),gp_Pnt2d(point.u,point.v),1.e-5);
                    Check(classifier.State()!=TopAbs_OUT,"BSpline mesh protrudes outside trim");
                    area+=double(a.x)*b.y-double(b.x)*a.y;
                }
            }
            Check(std::abs(std::abs(area)*.5-80.)<1.e-3,"BSpline mesh does not cover exact trim area");
        }
    }
    for (bool concave : {false,true}) {
        CMesh3D pair;
        std::vector<Vec3> points{{0,0,0},{2,0,0},concave?Vec3{.5f,.5f,0}:Vec3{2,2,0},{0,2,0}};
        Check(pair.SetGeometry(points,{{0,1,2},{0,2,3}}),"Merge test initialization");
        quadro::MergeConvexTrianglePairsXY(pair);
        size_t active=0;
        for (const auto& f:pair.GetFaces()) if (!f.deleted) ++active;
        Check(active==(concave?2:1),"Triangle merge must reject concave unions and merge convex pairs");
        const auto boundary = quadro::MeshBoundaryEdges(pair);
        Check(boundary.size()==4,"Merged quad must keep exactly four render boundary edges");
        Check(std::find(boundary.begin(),boundary.end(),std::make_pair(size_t(0),size_t(2)))==boundary.end(),
            "Deleted triangle diagonal must not become a rendered crease");
    }
    // The user's pocket model showed a changing stray crease at .45 and .50.
    // Every rendered boundary of its planar faces must lie on a CAD edge.
    {
        CAlfaDoc document; Dom3DProjectSerializer serializer;
        QString room,error; ProjectViewState view;
        const auto fixture=std::filesystem::path(__FILE__).parent_path()/"data/mesh-regression/Box_Min_Box_EdgeRegression.dom3d";
        Check(serializer.Load(QString::fromStdString(fixture.string()),document,room,view,error),error.toStdString());
        size_t bodies=0;
        for (const auto& object:document.GetObjects()) if (auto* body=dynamic_cast<CSolid*>(object.get())) {
            ++bodies; body->MeshQuadro=true; body->MeshQuadroHoleSLX=true; body->MeshQuadroHoleDivideFace=false;
            for (float density:{.45f,.5f,.45f}) {
                Check(body->ReBuldMesh(1.f/density),"Pocket SLX rebuild failed");
                for (int i=0;i<body->GetNumSurfaces();++i) {
                    const auto* surface=body->GetSurfaceFace(i);
                    const auto& mesh=*surface->pMesh3D;
                    std::vector<std::vector<CPoint3d>> contours;
                    for (int e=0;e<surface->GetPreparedPolylineCount();++e) {
                        std::vector<CPoint3d> points;
                        Check(surface->GetPreparedPolylinePoints(e,points),"Pocket CAD edge missing");
                        contours.push_back(std::move(points));
                    }
                    for (const auto& edge:quadro::MeshBoundaryEdges(mesh)) {
                        const Vec3 a=mesh.GetVertices()[edge.first],b=mesh.GetVertices()[edge.second];
                        const Vec3 mid=(a+b)*.5f; bool on_cad=false;
                        for (const auto& line:contours) for (size_t j=1;j<line.size();++j) {
                            const Vec3 p{float(line[j-1].x),float(line[j-1].y),float(line[j-1].z)};
                            const Vec3 q{float(line[j].x),float(line[j].y),float(line[j].z)};
                            const Vec3 d=q-p; const float length=dot(d,d);
                            if (length<=0) continue;
                            const Vec3 delta=mid-(p+d*std::clamp(dot(mid-p,d)/length,0.f,1.f));
                            if (dot(delta,delta)<1.e-8f) on_cad=true;
                        }
                        Check(on_cad,"SLX render boundary contains an internal mesh diagonal");
                    }
                }
            }
        }
        Check(bodies==1,"Pocket fixture must contain one body");
    }
    const auto grid = [](int n) {
        CMesh3D mesh;
        std::vector<Vec3> vertices;
        std::vector<CMesh3D::Face> faces;
        for (int y=0;y<=n;++y) for (int x=0;x<=n;++x)
            vertices.push_back({10.0f*x/n,10.0f*y/n,0});
        for (int y=0;y<n;++y) for (int x=0;x<n;++x) {
            const size_t a=y*(n+1)+x;
            faces.push_back({a,a+1,a+n+2,a+n+1});
        }
        Check(mesh.SetGeometry(std::move(vertices),std::move(faces)),"DivideFace test grid");
        return mesh;
    };
    const auto verify = [](const CMesh3D& mesh, double expected, int loops) {
        const auto& vertices=mesh.GetVertices();
        std::map<std::pair<size_t,size_t>,int> edges;
        std::map<size_t,std::set<size_t>> boundary;
        double area=0;
        for (const auto& f:mesh.GetFaces()) if (!f.deleted) {
            Check(f.corners.size()>=3 && f.corners.size()<=4,"DivideFace must return triangles/quads");
            double a=0;
            for (size_t e=0;e<f.corners.size();++e) {
                const size_t u=f.corners[e].v,v=f.corners[(e+1)%f.corners.size()].v;
                Check(u<vertices.size() && v<vertices.size() && u!=v,"DivideFace invalid edge");
                ++edges[std::minmax(u,v)];
                a+=double(vertices[u].x)*vertices[v].y-double(vertices[v].x)*vertices[u].y;
            }
            Check(a>1e-9,"DivideFace degenerate/reversed cell"); area+=a/2;
            if (f.corners.size()==4) for (size_t e=0;e<4;++e) {
                const auto p=vertices[f.corners[e].v],q=vertices[f.corners[(e+1)%4].v],r=vertices[f.corners[(e+2)%4].v];
                Check((double(q.x)-p.x)*(r.y-q.y)-(double(q.y)-p.y)*(r.x-q.x)>1e-9,
                    "DivideFace output contains a concave or flat quad corner");
            }
        }
        Check(std::abs(area-expected)<1e-4,"DivideFace area mismatch: "+std::to_string(area)+" expected "+std::to_string(expected));
        for (const auto& [edge,count]:edges) {
            Check(count<=2,"DivideFace nonmanifold edge");
            if (count==1) { boundary[edge.first].insert(edge.second); boundary[edge.second].insert(edge.first); }
        }
        for (const auto& [v,neighbors]:boundary) Check(neighbors.size()==2,"DivideFace broken boundary/T-junction");
        int count=0;
        std::set<size_t> visited;
        for (const auto& [v,neighbors]:boundary) if (!visited.count(v)) {
            ++count; std::vector<size_t> queue{v}; visited.insert(v);
            for (size_t i=0;i<queue.size();++i) for (size_t next:boundary[queue[i]])
                if (visited.insert(next).second) queue.push_back(next);
        }
        Check(count==loops,"DivideFace boundary loop count");
    };
    {
        auto single=grid(1); CPolyline line;
        line.AddPoint({5,5,0}); line.AddPoint({0,0,0});
        Check(single.TrimByDivideFace(&line,{1,8,0}),"DivideFace one-node fan failed");
        verify(single,100,1);
        Check(single.GetVertices().size()==5 && single.GetFaces().size()==4,
            "One internal node must make exactly four triangles without S/T nodes");
        for (const auto& f:single.GetFaces()) Check(f.corners.size()==3,"One-node fan must be triangular");
    }
    for (bool vertical : {false,true}) {
        auto pair=grid(1); CPolyline line;
        line.AddPoint(vertical ? CPoint3d(5,3,0) : CPoint3d(3,5,0));
        line.AddPoint(vertical ? CPoint3d(5,7,0) : CPoint3d(7,5,0));
        Check(pair.TrimByDivideFace(&line,{1,1,0}),"DivideFace two-node insertion failed");
        verify(pair,100,1);
        size_t triangles=0,quads=0;
        for (const auto& f:pair.GetFaces()) { triangles+=f.corners.size()==3; quads+=f.corners.size()==4; }
        Check(pair.GetVertices().size()==6 && triangles==2 && quads==2,
            "Two nodes must share an edge between two end triangles and two side quads");
        if (output_directory) {
            std::filesystem::create_directories(output_directory);
            pair.ExportToObj((std::filesystem::path(output_directory)/(vertical?"pair-v.obj":"pair.obj")).string());
        }
    }
    for (bool reverse : {false,true}) {
        auto enclosed=grid(1); CPolyline hole;
        std::vector<CPoint3d> points{{3,3,0},{7,3,0},{7,7,0},{3,7,0}};
        if (reverse) std::reverse(points.begin(),points.end());
        for (auto p:points) hole.AddPoint(p);
        hole.SetClosed(true);
        Check(enclosed.TrimByDivideFace(&hole,{0,0,0}),"DivideFace enclosed rectangle failed");
        verify(enclosed,84,2);
        Check(enclosed.GetVertices().size()==8 && enclosed.GetFaces().size()==4,
            "An enclosed rectangle must produce exactly four surrounding cells");
        for (const auto& f:enclosed.GetFaces()) Check(f.corners.size()==4,"Enclosed rectangle must have four quads");
        if (output_directory) enclosed.ExportToObj((std::filesystem::path(output_directory)/"enclosed.obj").string());
    }
    for (int n : {1,2,5}) {
        auto mesh=grid(n);
        CPolyline hole;
        for (CPoint3d p : {CPoint3d(3,3,0),CPoint3d(7,3,0),CPoint3d(7,7,0),CPoint3d(3,7,0)}) hole.AddPoint(p);
        hole.SetClosed(true);
        Check(mesh.TrimByDivideFace(&hole,{0,0,0}),"DivideFace interior rectangle failed");
        verify(mesh,84,2);
        CPolyline hole2;
        for (CPoint3d p : {CPoint3d(1,1,0),CPoint3d(2,1,0),CPoint3d(2,2,0),CPoint3d(1,2,0)}) hole2.AddPoint(p);
        hole2.SetClosed(true);
        Check(mesh.TrimByDivideFace(&hole2,{0,0,0}),"DivideFace second hole failed");
        verify(mesh,83,3);
        auto inside=grid(n);
        Check(inside.TrimByDivideFace(&hole,{5,5,0}),"DivideFace keep inside failed");
        verify(inside,16,1);
    }
    for (const auto& points : std::vector<std::vector<CPoint3d>>{
        {{-1,-1,0},{11,11,0}}, {{-1,5,0},{11,5,0}}, {{0,0,0},{5,10,0}},
        {{-1,3,0},{4,3,0},{4,7,0},{11,7,0}}}) {
        auto mesh=grid(2); CPolyline line;
        for (auto p:points) line.AddPoint(p);
        Check(mesh.TrimByDivideFace(&line,{1,9,0}),"DivideFace open cut failed");
        const double expected = points.size()==4 ? 46 : points[1].x==5 ? 25 : 50;
        verify(mesh,expected,1);
    }
    {
        auto unchanged=grid(2); CPolyline outside;
        outside.AddPoint({-2,5,0}); outside.AddPoint({-1,5,0});
        Check(unchanged.TrimByDivideFace(&outside,{1,1,0}),"DivideFace exterior segment failed");
        verify(unchanged,100,1);
        Check(unchanged.GetFaces().size()==4,"DivideFace cut the infinite line instead of its segment");
    }
    auto mesh=grid(2); CPolyline circle;
    double circle_area=0;
    for (int i=0;i<32;++i) {
        const double t=2*3.141592653589793*i/32;
        circle.AddPoint({5+2*std::cos(t),5+2*std::sin(t),0});
    }
    for (size_t i=0;i<circle.GetPoints().size();++i) {
        const auto a=circle.GetPoints()[i],b=circle.GetPoints()[(i+1)%32];
        circle_area+=(a.x*b.y-b.x*a.y)/2;
    }
    circle.SetClosed(true);
    Check(mesh.TrimByDivideFace(&circle,{0,0,0}),"DivideFace circular hole failed");
    verify(mesh,100-circle_area,2);
    auto enclosed_circle=grid(1);
    Check(enclosed_circle.TrimByDivideFace(&circle,{0,0,0}),"DivideFace enclosed circle failed");
    verify(enclosed_circle,100-circle_area,2);
    Check(enclosed_circle.GetFaces().size()==32 && enclosed_circle.GetVertices().size()==36,
        "Enclosed circle should connect its ring directly to the four cell corners");
    auto multiple=grid(3);
    int holes=0;
    for (const auto centre : std::vector<cVec2>{{2,2},{5,2},{8,2},{2,5},{5,5},{8,5},{5,8}}) {
        CPolyline hole;
        for (int i=0;i<24;++i) {
            const double t=i*2*3.141592653589793/24;
            hole.AddPoint({centre.x+.7*std::cos(t),centre.y+.7*std::sin(t),0});
        }
        hole.SetClosed(true);
        Check(multiple.TrimByDivideFace(&hole,{0,0,0}),"DivideFace multiple circular cuts failed");
        ++holes;
        verify(multiple,100-holes*12*.49*std::sin(2*3.141592653589793/24),holes+1);
    }
    BRepBuilderAPI_MakePolygon outer;
    for (gp_Pnt p : {gp_Pnt(0,0,0),gp_Pnt(10,0,0),gp_Pnt(10,10,0),gp_Pnt(0,10,0)}) outer.Add(p);
    outer.Close();
    BRepBuilderAPI_MakePolygon inner;
    for (gp_Pnt p : {gp_Pnt(3,3,0),gp_Pnt(3,7,0),gp_Pnt(7,7,0),gp_Pnt(7,3,0)}) inner.Add(p);
    inner.Close();
    BRepBuilderAPI_MakeFace face(outer.Wire()); face.Add(inner.Wire());
    auto plane_shape=face.Shape();
    CSolid solid(plane_shape);
    Check(solid.InitSurfaces() && solid.InitEdges(),"DivideFace CAD initialization");
    solid.MeshQuadro=true; solid.MeshQuadroHoleDivideFace=true;
    Check(solid.ReBuldMesh(2.0f),"DivideFace CAD meshing failed");
    Check(solid.GetNumSurfaces()==1 && solid.GetSurfaceFace(0)->pMesh3D,"DivideFace missing CAD mesh");
    verify(*solid.GetSurfaceFace(0)->pMesh3D,84,2);
    TColgp_Array2OfPnt poles(1,4,1,4);
    for (int u=1;u<=4;++u) for (int v=1;v<=4;++v)
        poles.SetValue(u,v,gp_Pnt((u-1)*10.0/3,(v-1)*10.0/3,(u==2 || u==3) ? 3.0 : 0.0));
    Handle(Geom_BezierSurface) geometry=new Geom_BezierSurface(poles);
    BRepBuilderAPI_MakeWire window;
    const std::array<gp_Pnt2d,4> uv{{{.3,.3},{.3,.7},{.7,.7},{.7,.3}}};
    for (int i=0;i<4;++i)
        window.Add(BRepBuilderAPI_MakeEdge(GCE2d_MakeSegment(uv[i],uv[(i+1)%4]).Value(),geometry).Edge());
    BRepBuilderAPI_MakeFace curved(geometry,1e-7); curved.Add(window.Wire());
    auto curved_shape=curved.Shape(); BRepLib::BuildCurves3d(curved_shape);
    CSolid curved_solid(curved_shape);
    Check(curved_solid.InitSurfaces() && curved_solid.InitEdges(),"DivideFace curved CAD initialization");
    curved_solid.MeshQuadro=true; curved_solid.MeshQuadroHoleDivideFace=true;
    Check(curved_solid.ReBuldMesh(2.0f),"DivideFace curved CAD meshing failed");
    Check(curved_solid.GetSurfaceFace(0)->pMesh3D,"DivideFace missing curved mesh");
    verify(*curved_solid.GetSurfaceFace(0)->pMesh3D,84,2);
    BRepBuilderAPI_MakePolygon circle_wire;
    for (auto i=circle.GetPoints().rbegin();i!=circle.GetPoints().rend();++i) circle_wire.Add(gp_Pnt(i->x,i->y,0));
    circle_wire.Close();
    BRepBuilderAPI_MakeFace collared_face(outer.Wire()); collared_face.Add(circle_wire.Wire());
    auto collared_shape=collared_face.Shape(); CSolid collared(collared_shape);
    Check(collared.InitSurfaces() && collared.InitEdges(),"Collar CAD initialization");
    collared.MeshQuadro=true;
    for (bool divide : {false,true}) for (float deflection : {2.f,1.f,3.f}) {
    collared.MeshQuadroHoleSLX=!divide;
    collared.MeshQuadroHoleDivideFace=divide;
    Check(collared.ReBuldMesh(deflection),"Shared collar build failed");
    verify(*collared.GetSurfaceFace(0)->pMesh3D,100-circle_area,2);
    Face2D exact_hole;
    for (auto p:circle.GetPoints()) exact_hole.verts.push_back({p.x,p.y});
    size_t collar_edges=0;
    const auto& collar_mesh=*collared.GetSurfaceFace(0)->pMesh3D;
    for (const auto& f:collar_mesh.GetFaces()) if (!f.deleted)
        for (size_t e=0;e<f.corners.size();++e) {
            const auto a=collar_mesh.GetVertices()[f.corners[e].v],b=collar_mesh.GetVertices()[f.corners[(e+1)%f.corners.size()].v];
            if (ClassifyPointInFace2(exact_hole,{(a.x+b.x)*.5,(a.y+b.y)*.5},1e-5)!=PFP_BOUNDARY) continue;
            ++collar_edges;
            Check(f.corners.size()==4,"Exact hole boundary must be surrounded by collar quads");
        }
    Check(collar_edges>=32,"Collar lost exact hole edges");
    }
    // Almost tangent to the outer edge: there is no room for a usable collar.
    // Both modes must still cover the exact trimmed domain without overlap.
    BRepBuilderAPI_MakePolygon close_wire;
    for (auto i=circle.GetPoints().rbegin();i!=circle.GetPoints().rend();++i)
        close_wire.Add(gp_Pnt(i->x-2.98,i->y,0));
    close_wire.Close();
    BRepBuilderAPI_MakeFace close_face(outer.Wire()); close_face.Add(close_wire.Wire());
    auto close_shape=close_face.Shape(); CSolid close_hole(close_shape);
    Check(close_hole.InitSurfaces() && close_hole.InitEdges(),"Close hole initialization");
    close_hole.MeshQuadro=true;
    for (bool divide:{false,true}) {
        close_hole.MeshQuadroHoleSLX=!divide; close_hole.MeshQuadroHoleDivideFace=divide;
        Check(close_hole.ReBuldMesh(2.f),"Close hole build failed");
        Check(close_hole.GetSurfaceFace(0)->pMesh3D,"Close hole mesh missing");
        verify(*close_hole.GetSurfaceFace(0)->pMesh3D,100-circle_area,2);
    }
    {
        CAlfaDoc document; Dom3DProjectSerializer serializer;
        QString room,error; ProjectViewState view;
        const auto fixture=std::filesystem::path(__FILE__).parent_path()/"data/mesh-regression/Box_And_Hole.dom3d";
        Check(serializer.Load(QString::fromStdString(fixture.string()),document,room,view,error),error.toStdString());
        size_t bodies=0;
        for (const auto& object:document.GetObjects()) if (auto* body=dynamic_cast<CSolid*>(object.get())) {
            ++bodies; body->MeshQuadro=true; body->MeshQuadroTrimByPline=false;
            for (bool divide:{false,true}) for (float density:{.3f,.5f,1.f}) {
                body->MeshQuadroHoleSLX=!divide; body->MeshQuadroHoleDivideFace=divide;
                Check(body->ReBuldMesh(1.f/density),"Box_And_Hole rebuild failed");
                std::vector<const CMesh3D*> parts;
                size_t collars=0;
                for (int i=0;i<body->GetNumSurfaces();++i) {
                    const auto* surface=body->GetSurfaceFace(i);
                    Check(surface->pMesh3D && surface->IsInitMesh,"Box_And_Hole missing surface");
                    const auto& mesh=*surface->pMesh3D; parts.push_back(&mesh);
                    if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()!=GeomAbs_Plane) continue;
                    for (int e=0;e<surface->GetPreparedPolylineCount();++e) {
                        std::vector<CPoint3d> hole;
                        Check(surface->GetPreparedPolylinePoints(e,hole),"Missing prepared edge");
                        if (hole.size()<4 || hole.front().DistTo(&hole.back())>1.e-5) continue;
                        ++collars; size_t owned=0;
                        for (const auto& f:mesh.GetFaces()) if (!f.deleted)
                            for (size_t k=0;k<f.corners.size();++k) {
                                const auto a=mesh.GetVertices()[f.corners[k].v],b=mesh.GetVertices()[f.corners[(k+1)%f.corners.size()].v];
                                const auto mid=(a+b)*.5f;
                                bool boundary=false;
                                for (size_t j=1;j<hole.size();++j) {
                                    const Vec3 p{float(hole[j-1].x),float(hole[j-1].y),float(hole[j-1].z)};
                                    const Vec3 q{float(hole[j].x),float(hole[j].y),float(hole[j].z)};
                                    const auto d=q-p;
                                    if (dot(d,d)<=0) continue;
                                    const auto delta=mid-(p+d*std::clamp(dot(mid-p,d)/dot(d,d),0.f,1.f));
                                    if (dot(delta,delta)<1.e-8f) boundary=true;
                                }
                                if (!boundary) continue;
                                ++owned;
                                Check(f.corners.size()==4,"Box_And_Hole must have a complete inner collar row in both modes");
                            }
                        Check(owned>=hole.size()-1,"Box_And_Hole lost a hole boundary");
                    }
                }
                Check(collars==2,"Box_And_Hole must have two planar collars");
                auto welded=CMesh3D::CreateWelded(parts);
                Check(bool(welded),"Cannot weld Box_And_Hole");
                std::map<std::pair<size_t,size_t>,std::pair<int,int>> edges;
                for (const auto& f:welded->GetFaces()) if (!f.deleted)
                    for (size_t k=0;k<f.corners.size();++k) {
                        const auto a=f.corners[k].v,b=f.corners[(k+1)%f.corners.size()].v;
                        auto& edge=edges[std::minmax(a,b)]; ++edge.first; edge.second+=a<b?1:-1;
                    }
                for (const auto& entry:edges) Check(entry.second.first==2 && entry.second.second==0,
                    "Box_And_Hole must remain closed and consistently oriented");
            }
        }
        Check(bodies==1,"Box_And_Hole fixture must contain one body");
    }
    // Equal holes at different phases of the background grid must both keep
    // exact boundaries while their collar widths are selected independently.
    BRepBuilderAPI_MakeFace two_holes_face(outer.Wire());
    double two_holes_area=100.0;
    for (const auto center:std::vector<cVec2>{{2.8,3.1},{6.9,6.6}}) {
        BRepBuilderAPI_MakePolygon wire;
        for (int k=23;k>=0;--k) {
            const double angle=k*2*3.141592653589793/24;
            wire.Add(gp_Pnt(center.x+1.2*std::cos(angle),center.y+1.2*std::sin(angle),0));
        }
        wire.Close(); two_holes_face.Add(wire.Wire());
        two_holes_area-=12*1.2*1.2*std::sin(2*3.141592653589793/24);
    }
    auto two_holes_shape=two_holes_face.Shape(); CSolid two_holes(two_holes_shape);
    Check(two_holes.InitSurfaces() && two_holes.InitEdges(),"Two holes initialization");
    two_holes.MeshQuadro=true;
    for (bool divide:{false,true}) {
        two_holes.MeshQuadroHoleSLX=!divide; two_holes.MeshQuadroHoleDivideFace=divide;
        Check(two_holes.ReBuldMesh(2.f),"Two holes collar search failed");
        Check(two_holes.GetSurfaceFace(0)->pMesh3D,"Two holes mesh missing");
        verify(*two_holes.GetSurfaceFace(0)->pMesh3D,two_holes_area,3);
    }
    BRepBuilderAPI_MakeFace cluster_face(outer.Wire());
    for (double x : {2.,4.,6.}) {
        BRepBuilderAPI_MakePolygon hole;
        for (gp_Pnt p:{gp_Pnt(x,3,0),gp_Pnt(x,5,0),gp_Pnt(x+1,5,0),gp_Pnt(x+1,3,0)}) hole.Add(p);
        hole.Close(); cluster_face.Add(hole.Wire());
    }
    auto cluster_shape=cluster_face.Shape(); CSolid cluster(cluster_shape);
    Check(cluster.InitSurfaces() && cluster.InitEdges(),"Cluster CAD initialization");
    cluster.MeshQuadro=true; cluster.MeshQuadroHoleDivideFace=true;
    Check(cluster.ReBuldMesh(2.f),"DivideFace shared zone build failed");
    verify(*cluster.GetSurfaceFace(0)->pMesh3D,94,4);
    Check(cluster.GetSurfaceFace(0)->m_LastIslandBoundariesUV.size()==1,
        "Nearby rectangular holes must use one shared zone");
    if (output_directory) {
        const std::filesystem::path directory(output_directory);
        std::filesystem::create_directories(directory);
        Check(mesh.ExportToObj((directory/"circle.obj").string()),"DivideFace circle export");
        Check(multiple.ExportToObj((directory/"multiple.obj").string()),"DivideFace multiple holes export");
        Check(solid.GetSurfaceFace(0)->pMesh3D->ExportToObj((directory/"plane.obj").string()),"DivideFace plane export");
        Check(curved_solid.GetSurfaceFace(0)->pMesh3D->ExportToObj((directory/"curved.obj").string()),"DivideFace curved export");
        Check(collared.GetSurfaceFace(0)->pMesh3D->ExportToObj((directory/"collar.obj").string()),"Collar export");
        Check(cluster.GetSurfaceFace(0)->pMesh3D->ExportToObj((directory/"cluster.obj").string()),"Cluster export");
        auto benchmark=grid(64);
        const auto started=std::chrono::steady_clock::now();
        Check(benchmark.TrimByDivideFace(&circle,{0,0,0}),"DivideFace benchmark cut failed");
        const double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        verify(benchmark,100-circle_area,2);
        Check(benchmark.ExportToObj((directory/"benchmark.obj").string()),"DivideFace benchmark export");
        std::cout << "DivideFace 4096 cells / 32 contour nodes: " << milliseconds << " ms\n";
    }
    std::cout << "DivideFace: enclosed holes, repeated cuts, vertex/edge hits and open cuts passed.\n";
}

void TestToolsMesh3D(const char* boundary_path, const char* diagnostic_directory)
{
    TestDivideFace(nullptr);
    std::ifstream input(boundary_path);
    std::vector<std::unique_ptr<CPolyline>> lines;
    std::string error;
    Check(CPolyline::LoadTextPolylines(input, lines, error), error);
    Check(lines.size() == 1 && lines.front()->GetPointCount() == 60,
        "Expected the single 60-node Extrude_SL UV boundary.");
    auto& line = *lines.front();
    line.SetClosed(true);
    const auto original_points = line.GetPoints();

    QTemporaryDir temporary;
    Check(temporary.isValid(), "Cannot create test directory.");
    const std::filesystem::path directory(temporary.path().toStdString());
    Tools_Mesh3D::FillContourOptions options;
    options.diagnostic_directory = diagnostic_directory
        ? std::filesystem::path(diagnostic_directory) : directory / "dumps";
    for (bool raw : {false, true}) {
        options.raw_quadrangulator = raw;
        auto result = Tools_Mesh3D::FillContour(line, options);
        Check(bool(result), result.error);
        CheckClosedQuadPatch(result);
        Check(result.boundary.size() == 60, "The tool changed boundary node count.");
        for (size_t i = 0; i < original_points.size(); ++i) {
            const auto& point = original_points[i];
            const auto& actual = result.boundary[i];
            Check(actual.x == static_cast<float>(point.x)
                && actual.y == static_cast<float>(point.y)
                && actual.z == static_cast<float>(point.z),
                "The tool scaled, reordered or resampled the UV boundary.");
            const auto& unchanged = line.GetPoints()[i];
            Check(point.x == unchanged.x && point.y == unchanged.y
                && point.z == unchanged.z, "The tool mutated the source polyline.");
        }

        // Reproduce the pre-extraction UI code independently and compare the
        // complete OBJ bytes, including normals, UVs and face connectivity.
        Vec3 normal{};
        for (size_t i = 0; i < result.boundary.size(); ++i) {
            const Vec3& a = result.boundary[i];
            const Vec3& b = result.boundary[(i + 1) % result.boundary.size()];
            normal.x += (a.y - b.y) * (a.z + b.z);
            normal.y += (a.z - b.z) * (a.x + b.x);
            normal.z += (a.x - b.x) * (a.y + b.y);
        }
        normal = normalize(normal);
        CSurfaceFace filler;
        CMesh3D triangle, reference(line.GetName() + " Fill Mesh");
        std::string rejection;
        Check(filler.MakeFilledContour(result.boundary, normal, &reference,
            false, &error, &triangle, &rejection), error);
        if (raw)
            Check(Build3DCoatQuadrangulation(triangle.GetVertices(), triangle.GetFaces(),
                &reference, true), "Reference raw quadrangulation failed.");
        Check(result.quadrangulator_rejection == rejection,
            "Production diagnostic changed during extraction.");
        Check(triangle.ExportToObj((directory / "reference-triangle.obj").string()),
            "Reference triangle export failed.");
        Check(reference.ExportToObj((directory / "reference-quad.obj").string()),
            "Reference quad export failed.");
        const auto& dumps = options.diagnostic_directory;
        Check(Read(directory / "reference-triangle.obj")
            == Read(dumps / "Dom3D_ContourToFill_Trisngle.obj"),
            "Triangle output changed during extraction.");
        Check(Read(dumps / "Dom3D_ContourToFill.obj")
            == Read(dumps / "Dom3D_ContourToFill_Trisngle.obj"),
            "Legacy triangle dump differs.");
        Check(Read(directory / "reference-quad.obj")
            == Read(dumps / "Dom3D_ContourToFillQuade.obj"),
            "Quad output changed during extraction.");
        std::ifstream saved(dumps / "Dom3D_BoundaryLine.txt");
        std::vector<std::unique_ptr<CPolyline>> saved_lines;
        Check(CPolyline::LoadTextPolylines(saved, saved_lines, error), error);
        Check(saved_lines.size() == 1 && saved_lines.front()->GetPointCount() == 60,
            "Boundary dumps must overwrite, not append contours.");
        for (size_t i = 0; i < result.boundary.size(); ++i) {
            const auto& point = saved_lines.front()->GetPoints()[i];
            const auto& actual = result.boundary[i];
            Check(static_cast<float>(point.x) == actual.x
                && static_cast<float>(point.y) == actual.y
                && static_cast<float>(point.z) == actual.z,
                "Boundary dump loses input precision.");
        }
    }
    for (double scale : {0.5, 2.0, 10.0}) {
        for (bool scale_u_only : {false, true}) {
            CPolyline scaled;
            for (const auto& point : original_points)
                scaled.AddPoint(CPoint3d(point.x * scale,
                    point.y * (scale_u_only ? 1.0 : scale), point.z));
            scaled.SetClosed(true);
            auto result = Tools_Mesh3D::FillContour(scaled);
            Check(bool(result), result.error);
            CheckClosedQuadPatch(result);
        }
    }
    // Preserve repeated closing-point handling, also with diagnostics disabled.
    line.AddPoint(original_points.front());
    auto closed = Tools_Mesh3D::FillContour(line);
    Check(bool(closed) && closed.boundary.size() == 60,
        "Repeated closure point handling changed.");
    CPolyline empty;
    auto invalid = Tools_Mesh3D::FillContour(empty);
    Check(!invalid && !invalid.error.empty(), "Invalid input needs an error.");
    options.diagnostic_directory = directory / "reference-triangle.obj";
    auto blocked = Tools_Mesh3D::FillContour(line, options);
    Check(!blocked && !blocked.error.empty(), "Failed diagnostic writes need an error.");
    std::cout << "Tools_Mesh3D: unchanged UV input, triangle and quad outputs in both modes; diagnostics passed.\n";
}


void TestMeshSubdivision() {
    auto cube = std::make_unique<CMesh3D>("Subdivision cube");
    Check(cube->SetGeometry({{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                            {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}},
        {{3,2,1,0},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}}), "Cube setup");
    Check(cube->SetSubdivisionLevel(1), "Cube subdivision rejected");
    const CMesh3D& first = cube->GetEvaluatedMesh();
    Check(first.GetFaces().size() == 24 && first.GetVertices().size() == 26, "Cube topology at level 1");
    Check(std::abs(first.GetVertices()[0].x + 5.f/9.f) < 1e-6f, "Catmull-Clark vertex stencil");
    Check(cube->GetFaces().size() == 6 && cube->GetVertices().size() == 8, "Source geometry changed");
    Check(cube->SetSubdivisionLevel(2), "Cube level 2 rejected");
    Check(cube->GetEvaluatedMesh().GetFaces().size() == 96, "Cube topology at level 2");
    Check(cube->SetSubdivisionLevel(0), "Cannot disable subdivision");
    cube->SetShadingMode(1);
    const CMesh3D& smooth = cube->GetEvaluatedMesh();
    Check(smooth.GetFaces().size() == 6 && smooth.GetVertices().size() == 8, "Shade Smooth adds geometry");
    Check(dot(smooth.GetNormals()[0], normalize(Vec3{-1,-1,-1})) > .9999f, "Smooth normals not averaged");
    cube->SetShadingMode(2);
    const CMesh3D& flat = cube->GetEvaluatedMesh();
    Check(flat.GetFaces()[0].corners[0].n != flat.GetFaces()[2].corners[0].n, "Flat normals not split");
    cube->SetShadingMode(1);
    Check(cube->SetSubdivisionLevel(2), "Restore level");
    auto cloned = cube->Clone();
    auto* copy = dynamic_cast<CMesh3D*>(cloned.get());
    Check(copy && copy->GetSubdivisionLevel() == 2 && copy->GetShadingMode() == 1, "Clone settings lost");
    copy->SetSubdivisionLevel(1);
    copy->AddSharpEdge(0,1); copy->AddSharpEdge(0,3); copy->AddSharpEdge(0,4);
    Check(std::abs(copy->GetEvaluatedMesh().GetVertices()[0].x + 1.f) < 1e-6f, "Crease corner must stay fixed");
    const auto& evaluated_uv = copy->GetEvaluatedMesh();
    for (const auto& f : evaluated_uv.GetFaces()) for (const auto& c : f.corners)
        Check(c.uv < evaluated_uv.GetUVs().size(), "Subdivision UV index out of bounds");
    std::stringstream stream;
    Check(cube->Save(stream), "Legacy save");
    CMesh3D legacy;
    Check(legacy.Load(stream) && legacy.GetSubdivisionLevel() == 2 && legacy.GetShadingMode() == 1, "Legacy roundtrip settings");
    CMesh3D patch;
    Check(patch.SetGeometry({{0,0,0},{2,0,0},{2,2,0},{0,2,0}}, {{0,1,2,3}}), "Patch setup");
    Check(patch.SetSubdivisionLevel(1), "Open mesh subdivision rejected");
    Check(std::abs(patch.GetEvaluatedMesh().GetVertices()[0].x-.25f) < 1e-6f, "Boundary stencil");
    patch.AddSharpEdge(0, 1); patch.AddSharpEdge(0, 3); patch.AddSharpEdge(0, 2);
    // Two boundary neighbors retain the boundary rule; a non-topological mark is ignored.
    Check(std::abs(patch.GetEvaluatedMesh().GetVertices()[0].x-.25f) < 1e-6f, "Sharp-edge cache rebuild");
    CMesh3D triangle;
    Check(triangle.SetGeometry({{0,0,0},{2,0,0},{0,2,0}}, {{0,1,2}}) && triangle.SetSubdivisionLevel(1), "Triangle subdivision");
    Check(triangle.GetEvaluatedMesh().GetFaces().size() == 3, "Triangle must create three quads");
    CMesh3D invalid;
    Check(invalid.SetGeometry({{0,0,0},{1,0,0},{0,1,0},{0,0,1},{0,-1,0}}, {{0,1,2},{1,0,3},{0,1,4}}), "Nonmanifold setup");
    Check(!invalid.SetSubdivisionLevel(1) && invalid.GetSubdivisionLevel() == 0, "Nonmanifold rejection must be atomic");
    Check(!cube->SetSubdivisionLevel(7) && cube->GetSubdivisionLevel() == 2, "Invalid level must preserve state");
    cube->Translate({3,0,0});
    Check(cube->GetEvaluatedMesh().GetVertices()[0].x > 2, "Stale cache after transform");
    CAlfaDoc document;
    document.AddObject(std::move(cube));
    CUndoRedo undo(document);
    auto* selected = dynamic_cast<CMesh3D*>(document.GetObjects().back().get());
    selected->SetSubdivisionLevel(3); selected->SetShadingMode(2);
    Check(undo.RecordChange("Subdivision"), "Record undo");
    Check(undo.Undo(), "Undo subdivision");
    selected = dynamic_cast<CMesh3D*>(document.GetObjects().back().get());
    Check(selected->GetSubdivisionLevel() == 2 && selected->GetShadingMode() == 1, "Undo settings lost");
    Check(undo.Redo(), "Redo subdivision");
    QTemporaryDir temp;
    QString error, room;
    ProjectViewState view;
    Dom3DProjectSerializer serializer;
    Check(serializer.Save(temp.filePath("subdivision.dom3d"), document, {}, view, {}, error), error.toStdString());
    CAlfaDoc restored;
    Check(serializer.Load(temp.filePath("subdivision.dom3d"), restored, room, view, error), error.toStdString());
    auto* loaded = dynamic_cast<CMesh3D*>(restored.GetObjects().back().get());
    Check(loaded && loaded->GetSubdivisionLevel() == 3 && loaded->GetShadingMode() == 2, "Project settings lost");
    Check(loaded->GetFaces().size() == 6 && loaded->GetEvaluatedMesh().GetFaces().size() == 384, "Project geometry lost");
    std::cout << "Mesh subdivision, shading, boundaries, normals, clone, undo/redo and persistence passed.\n";
}


void RenderMeshSubdivisionPreview(const char* directory) {
    QDir().mkpath(QString::fromLocal8Bit(directory));
    CAlfaDoc document;
    auto mesh = std::make_unique<CMesh3D>("Catmull-Clark preview");
    std::vector<Vec3> vertices;
    for (float x : {-1.f, 1.f})
        for (Vec3 p : {Vec3{0,0,0},Vec3{0,2,0},Vec3{0,2,1},Vec3{0,1,1},Vec3{0,1,2},Vec3{0,0,2}})
            vertices.push_back({x,p.y,p.z});
    std::vector<CMesh3D::Face> faces{{5,4,3,2,1,0},{6,7,8,9,10,11}};
    for (size_t i=0;i<6;++i) faces.push_back({i,(i+1)%6,(i+1)%6+6,i+6});
    Check(mesh->SetGeometry(vertices, faces) && mesh->SetSubdivisionLevel(3), "Preview mesh setup");
    Material material = mesh->GetMaterial(); material.diffuse = {.55f,.64f,.68f}; material.alpha = 1;
    mesh->SetMaterial(material);
    document.AddObject(std::move(mesh));
    const auto id = document.GetObjects().back()->m_id;
    OpenGLViewport viewport;
    viewport.resize(800,800); viewport.move(-20000,-20000);
    viewport.SetDocument(&document); viewport.SetOrthographicProjection(true);
    viewport.SetFloorGridVisible(false); viewport.SetCoordinateAxesVisible(false);
    viewport.FitToDocument(); viewport.show(); QApplication::processEvents();
    CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceGray);
    CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
    for (bool selected : {false,true}) {
        document.ClearSelection();
        if (selected) document.SelectObjectById(id);
        const auto frame = viewport.CaptureSceneImage(QSize(800,800));
        Check(!frame.isNull() && frame.save(QDir(QString::fromLocal8Bit(directory)).filePath(
            selected ? "selected.png" : "unselected.png")), "Preview capture");
    }
}


void TestLowPolySharpEdges() {
    const auto copy_mesh = [](const CSolid& solid) {
        auto result = std::make_unique<CMesh3D>();
        std::vector<Vec3> vertices; std::vector<CMesh3D::Face> faces;
        for (int i=0;i<solid.GetNumSurfaces();++i) {
            const auto* source=solid.GetSurfaceFace(i)->pMesh3D;
            Check(source != nullptr, "Missing source surface mesh");
            const size_t offset=vertices.size();
            vertices.insert(vertices.end(),source->GetVertices().begin(),source->GetVertices().end());
            for (auto f:source->GetFaces()) {
                f.sourceFaceId=i;
                for (auto& c:f.corners) c.v=c.uv=c.n=c.v+offset;
                faces.push_back(std::move(f));
            }
        }
        Check(result->SetGeometry(vertices,faces),"Cannot copy low poly mesh");
        return result;
    };
    TopoDS_Shape box_shape = BRepPrimAPI_MakeBox(20,20,20).Shape();
    CSolid box(box_shape);
    Check(box.ReBuldMesh(2.f),"Box meshing");
    auto mesh=copy_mesh(box);
    const size_t count=mesh->GetFaces().size();
    Check(lowpoly::AddSharpSurfaceEdges(box,*mesh,30)>0,"Box has no automatic sharp edges");
    const auto& vertices=static_cast<const CMesh3D&>(*mesh).GetVertices();
    for (auto e:mesh->GetSharpEdges()) {
        Vec3 a=vertices[e.v1],b=vertices[e.v2];
        int planes=0;
        for (auto values:{std::pair<float,float>{a.x,b.x},{a.y,b.y},{a.z,b.z}})
            if (std::abs(values.first-values.second)<1e-5
                && (std::abs(values.first)<1e-5 || std::abs(values.first-20)<1e-5)) ++planes;
        Check(planes==2,"Interior mesh diagonal marked sharp");
    }
    Check(mesh->GetFaces().size()==count,"Sharp assignment changed mesh topology");
    auto welded=CMesh3D::CreateWelded({mesh.get()});
    Check(welded && !welded->GetSharpEdges().empty(),"Welding discarded crease flags");
    Check(welded->SetSubdivisionLevel(1),"Creased welded cube cannot subdivide");
    Vec3 lo{},hi{}; welded->GetEvaluatedMesh().GetBounds(lo,hi);
    Check(std::abs(lo.x)<1e-5 && std::abs(hi.x-20)<1e-5,"Subdivision rounded sharp cube corners");
    mesh->ClearSharpEdges();
    Check(lowpoly::AddSharpSurfaceEdges(box,*mesh,100)==0,"Box angle threshold ignored");
    TopoDS_Shape cylinder_shape = BRepPrimAPI_MakeCylinder(10,20).Shape();
    CSolid cylinder(cylinder_shape);
    Check(cylinder.ReBuldMesh(2.f),"Cylinder meshing");
    auto cylindrical=copy_mesh(cylinder);
    Check(lowpoly::AddSharpSurfaceEdges(cylinder,*cylindrical,30)>0,"Cylinder rims unmarked");
    const auto& cv=static_cast<const CMesh3D&>(*cylindrical).GetVertices();
    for (auto e:cylindrical->GetSharpEdges())
        Check(std::abs(cv[e.v1].z-cv[e.v2].z)<1e-4
            && (std::abs(cv[e.v1].z)<1e-4 || std::abs(cv[e.v1].z-20)<1e-4),"Smooth periodic seam marked sharp");
    TopoDS_Shape sphere_shape = BRepPrimAPI_MakeSphere(10).Shape();
    CSolid sphere(sphere_shape);
    Check(sphere.ReBuldMesh(2.f),"Sphere meshing");
    auto spherical=copy_mesh(sphere);
    Check(lowpoly::AddSharpSurfaceEdges(sphere,*spherical,0)==0,"Smooth sphere seam marked sharp");
    BRepFilletAPI_MakeFillet fillet(box_shape);
    for (TopExp_Explorer edge(box_shape,TopAbs_EDGE);edge.More();edge.Next())
        fillet.Add(2.0,TopoDS::Edge(edge.Current()));
    fillet.Build(); Check(fillet.IsDone(),"Rounded box construction");
    TopoDS_Shape rounded_shape=fillet.Shape(); CSolid rounded(rounded_shape);
    Check(rounded.ReBuldMesh(2.f),"Rounded box meshing");
    auto rounded_mesh=copy_mesh(rounded);
    Check(lowpoly::AddSharpSurfaceEdges(rounded,*rounded_mesh,30)==0,"Tangent fillet joins marked sharp");
    std::cout << "Automatic CAD sharp edges, angle threshold, periodic seams, welding and subdivision passed.\n";
}


void TestObjSharpEdges() {
    CAlfaDoc doc;
    for(int i=0;i<2;++i) {
        auto mesh=std::make_unique<CMesh3D>("Sharp strip "+std::to_string(i));
        Check(mesh->SetGeometry({{0,0,0},{10,0,0},{20,0,10},{30,0,10},
            {0,10,0},{10,10,0},{20,10,10},{30,10,10}},
            {{0,1,5,4},{1,2,6,5},{2,3,7,6}}),"Strip setup");
        Check(mesh->AddSharpEdge(1,5),"Crease setup");
        mesh->Translate({float(i*50),0,0});
        doc.AddObject(std::move(mesh));
    }
    QTemporaryDir directory;
    ObjIO io;std::string error;
    const auto path=directory.filePath("sharp.obj").toStdString();
    Check(io.Export(path,doc,error,ObjLengthUnit::Millimeters),error);
    std::ifstream file(path);std::string line;size_t tags=0;std::set<int> groups;
    while(std::getline(file,line)) {
        if(line.rfind("# Dom3D sharp_edge ",0)==0) ++tags;
        if(line.rfind("s ",0)==0) groups.insert(std::stoi(line.substr(2)));
    }
    Check(tags==2 && groups.size()==4,"OBJ crease records or smoothing groups missing");
    const auto verify=[](const CMesh3D& mesh) {
        Check(mesh.GetSharpEdges().size()==1,"OBJ roundtrip lost exact Sharp flag");
        Check(mesh.GetFaces().size()==3 && mesh.GetVertices().size()==8,"OBJ shading split position topology");
        const auto& f=mesh.GetFaces();const auto& n=mesh.GetNormals();
        Check(!n.empty(),"OBJ normals not imported");
        Check(dot(n[f[0].corners[1].n],n[f[1].corners[0].n])<.99f,"Sharp edge normals were smoothed");
        Check(dot(n[f[1].corners[1].n],n[f[2].corners[0].n])>.9999f,"Unmarked edge lost smooth shading");
    };
    std::vector<std::unique_ptr<CMesh3D>> loaded;
    Check(io.Import(path,loaded,error),error);Check(loaded.size()==2,"Multiple OBJ objects lost");
    for(const auto& mesh:loaded) verify(*mesh);
    const auto* source=dynamic_cast<const CMesh3D*>(doc.GetObjects().back().get());
    Check(source!=nullptr,"Test source mesh missing");
    const auto direct=directory.filePath("direct.obj").toStdString();
    Check(source->ExportToObj(direct),"Direct mesh export failed");
    Check(io.Import(direct,loaded,error),error);Check(loaded.size()==1,"Direct OBJ import failed");verify(*loaded.front());
    Check(source->GetNormals().empty() && source->GetSharpEdges().size()==1,"Export mutated source mesh");
    std::cout<<"OBJ Sharp edges: s/vn, smooth neighbors, offsets, direct export and exact flag roundtrip passed.\n";
}

#include "SlxAudit.inc"
