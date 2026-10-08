#include "solid/SurfaceSet.h"
#include "Net.h"
#include "CMesh3D.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <Geom_BezierSurface.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineSurface.hxx>
#include <TColgp_Array2OfPnt.hxx>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

size_t check_net(CSolid& solid, float tolerance)
{
    check(solid.ReBuldMesh(tolerance * 10.0f), "Display mesh build failed");
    auto* surface = solid.GetSurfaceFace(0);
    check(surface && surface->pMesh3D && !surface->IsTrimmed,
          "Natural patch was classified as trimmed");
    const auto& faces = surface->pMesh3D->GetFaces();
    check(!faces.empty(), "Empty display mesh");
    for (const auto& face : faces)
        check(face.deleted || face.corners.size() == 4,
              "Natural spline patch was triangulated");
    CNet reference;
    check(reference.Build(surface, tolerance) == 0, "Reference CNet failed");
    const auto& vertices = surface->pMesh3D->GetVertices();
    check(vertices.size() == reference.GetPoints().size(),
          "Display mesh does not use the supplied CNet tolerance");
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& point = reference.GetPoints()[i];
        check(std::abs(vertices[i].x - point.x) < 1.e-4
                  && std::abs(vertices[i].y - point.y) < 1.e-4
                  && std::abs(vertices[i].z - point.z) < 1.e-4,
              "Display vertices differ from CNet");
    }
    return faces.size();
}
}

void TestSteppedCylinderSpacing();

int TestSurfaceDisplayNet()
{
    TestSteppedCylinderSpacing();
    TColgp_Array2OfPnt poles(1, 4, 1, 4);
    for (int u = 1; u <= 4; ++u) {
        for (int v = 1; v <= 4; ++v) {
            const double z = (v == 2 || v == 3 ? 60.0 : 0.0)
                + (u == 2 ? 40.0 : u == 3 ? -40.0 : 0.0);
            poles.SetValue(u, v, gp_Pnt((u - 1) * 100.0, (v - 1) * 40.0, z));
        }
    }
    Handle(Geom_BezierSurface) bezier = new Geom_BezierSurface(poles);
    const Handle(Geom_Surface) surfaces[] = {
        bezier, GeomConvert::SurfaceToBSplineSurface(bezier)
    };
    for (const auto& geometry : surfaces) {
        TopoDS_Shape shape = BRepBuilderAPI_MakeFace(geometry, 1.e-7).Shape();
        CSolid solid(shape);
        check(solid.InitSurfaces() && solid.InitEdges(), "Solid initialization failed");
        const size_t coarse = check_net(solid, 2.0f);
        const size_t fine = check_net(solid, 0.125f);
        check(fine > coarse, "Smaller tolerance did not refine the net");
        check(check_net(solid, 2.0f) == coarse, "Rebuild retained the finer net");
        CSurfaceSet standalone(shape);
        check(!standalone.MeshQuadro, "Standalone display defaults to Low Poly");
        check(standalone.InitSurfaces() && standalone.InitEdges(),
              "Surface initialization failed");
        check_net(standalone, 0.125f);
        std::cout << "Natural spline display: " << coarse << " -> " << fine << " quads\n";
    }
    std::cout << "Surface display CNet tests passed\n";
    return EXIT_SUCCESS;
}


#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "ui/ToolRegistry.h"
#include "CBSpline.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QTemporaryDir>
#include <sstream>
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>
#include <gp_Sphere.hxx>
#include <gp_Cylinder.hxx>
#include <map>
#include <BRepAdaptor_Curve.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <ElSLib.hxx>
#include <gp_Torus.hxx>
#include "StepIO.h"
#include "solid/LowPolyCompletion.h"
#include <BRep_Tool.hxx>
#include <Geom2d_Curve.hxx>
#include <filesystem>
#include <fstream>
#include "solid/QuadroBodyMesher.h"
#include "solid/PlanarTrimRecovery.h"
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Circle.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <ShapeFix_Edge.hxx>

void TestSteppedCylinderSpacing()
{
    const double pi=std::acos(-1.);
    for(double scale : {1.,100.}) for(double notch : {.125,.0125}) for(int segments : {8,16}) {
        const double radius=22.225*scale, height=38.1*scale, shoulder=12.7*scale;
        Handle(Geom_CylindricalSurface) geometry=new Geom_CylindricalSurface(gp_Ax3(),radius);
        std::vector<gp_Pnt2d> contour{{0,0},{pi,0},{pi,shoulder},{pi-notch,shoulder},
            {pi-notch,height},{notch,height},{notch,shoulder},{0,shoulder}};
        for(auto& point:contour)point.SetX(point.X()+0.4);
        BRepBuilderAPI_MakeWire wire;
        for(size_t i=0;i<contour.size();++i) {
            const auto a=contour[i],b=contour[(i+1)%contour.size()];
            TopoDS_Edge edge;
            if(a.Y()==b.Y()) {
                Handle(Geom_Circle) circle=new Geom_Circle(gp_Ax2(gp_Pnt(0,0,a.Y()),gp_Dir(0,0,1)),radius);
                edge=BRepBuilderAPI_MakeEdge(circle,std::min(a.X(),b.X()),std::max(a.X(),b.X()));
                if(b.X()<a.X())edge.Reverse();
            } else edge=BRepBuilderAPI_MakeEdge(geometry->Value(a.X(),a.Y()),geometry->Value(b.X(),b.Y()));
            wire.Add(edge);
        }
        auto shape=BRepBuilderAPI_MakeFace(geometry,wire.Wire(),true).Shape();
        ShapeFix_Edge edge_fixer;
        for(TopExp_Explorer it(shape,TopAbs_EDGE);it.More();it.Next())
            edge_fixer.FixAddPCurve(TopoDS::Edge(it.Current()),TopoDS::Face(shape),false,1.e-7);
        shape=BRepBuilderAPI_MakeFace(geometry,wire.Wire(),true).Shape();
        CSolid body(shape);body.MeshQuadro=true;
        check(body.InitSurfaces() && body.InitEdges(),"Stepped cylinder initialization");
        auto* surface=body.GetSurfaceFace(0);
        check(surface->InitEdges3DCoat(),"Stepped cylinder UV preparation");
        surface->PrepareEdges(1.f);
        for(int i=0;i<surface->GetPreparedPolylineCount();++i) {
            TopoDS_Edge edge;check(surface->GetPreparedTopoEdge(i,edge),"Missing cylinder CAD edge");
            const BRepAdaptor_Curve curve(edge);
            const bool long_arc=curve.GetType()==GeomAbs_Circle && curve.LastParameter()-curve.FirstParameter()>1.;
            surface->SetPreparedPolylinePointCount(i,long_arc?segments+1:3);
        }
        quadro::BuildSteppedCylinderFaces(body);
        check(surface->IsInitMesh,"Stepped cylinder mesher declined the domain");
        const auto& mesh=*surface->pMesh3D;
        double area=0;
        size_t cells=0;
        for(const auto& cell:mesh.GetFaces()) if(!cell.deleted) {
            ++cells;check(cell.corners.size()==4,"Stepped cylinder lost its quads");
            for(size_t i=0;i<cell.corners.size();++i) {
                const auto a=mesh.GetUVs()[cell.corners[i].uv],b=mesh.GetUVs()[cell.corners[(i+1)%cell.corners.size()].uv];
                area+=double(a.u)*b.v-double(b.u)*a.v;
                BRepClass_FaceClassifier classifier(TopoDS::Face(shape),gp_Pnt2d(a.u,a.v),1.e-5*scale);
                check(classifier.State()!=TopAbs_OUT,"Cylinder grid crossed a shoulder");
            }
        }
        const double expected=pi*height-2*notch*(height-shoulder);
        check(std::abs(std::abs(area)*.5-expected)<expected*1.e-5,"Cylinder grid lost domain area");
        check(cells<=size_t(segments*6+16),"Tiny shoulder propagated its spacing over the entire cylinder");
    }
}

void TestCylinderStep(const char* path, const char* output)
{
    CAlfaDoc document; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    std::vector<std::unique_ptr<CSolid>> imported;
    std::vector<CSolid*> bodies;
    if (std::filesystem::path(path).extension()==".dom3d") {
        check(serializer.Load(QString::fromLocal8Bit(path),document,room,view,error),"Cannot load cylinder fixture");
        for (const auto& object:document.GetObjects())
            if (auto* body=dynamic_cast<CSolid*>(object.get())) bodies.push_back(body);
    } else {
        StepIO io; std::string message;
        check(io.Import(path,imported,message),message.c_str());
        for (auto& body:imported) bodies.push_back(body.get());
    }
    check(!bodies.empty(),"Cylinder fixture has no solids");
    std::filesystem::create_directories(output);
    std::ofstream report(std::filesystem::path(output)/"surfaces.txt");
    int body_index=0,stepped_cylinders=0;
    const float density=qEnvironmentVariableIsSet("DOM3D_TEST_PANEL_DENSITY")
        ? qEnvironmentVariable("DOM3D_TEST_PANEL_DENSITY").toFloat() : .6f;
    for (auto* body:bodies) {
        if (body->GetNumSurfaces()==0) continue; // STEP PMI curves are not meshable bodies.
        body->MeshQuadro=true; body->MeshQuadroHoleSLX=true; body->MeshQuadroHoleDivideFace=false;
        if(qEnvironmentVariableIsSet("DOM3D_TEST_REGULAR_PANEL")) {
            body->MeshQuadroHoleSLX=false;body->MeshQuadroHoleDivideFace=true;
            body->MeshQuadroTrimByPline=true;
        }
        if(qEnvironmentVariableIsSet("DOM3D_TEST_TRIM_OUTER_SLX")) {
            body->MeshQuadroHoleSLX=true;body->MeshQuadroHoleDivideFace=false;body->MeshQuadroTrimByPline=true;
        }
        if(qEnvironmentVariableIsSet("DOM3D_TEST_DIVIDE_ONLY")) {
            body->MeshQuadroHoleSLX=false;body->MeshQuadroHoleDivideFace=true;
        }
        body->ReBuldMesh(1.f/density);
        check(body->GetNumSurfaces()>127 && body->GetSurfaceFace(127)->IsInitMesh,
            "NIST lettering wall fell back to CAD triangulation");
        check(body->GetSurfaceFace(127)->m_UsedRegularOuterTrim==body->MeshQuadroTrimByPline,
            "NIST panel ignored the outer TrimByPline switch");
        const auto completion=lowpoly::CompleteWithTriangles(*body,1.f/density);
        check(completion.complete(),"Cylinder fixture rebuild failed");
        std::cout<<"Retained="<<completion.retained<<" fallback="<<completion.triangulated.size()<<'\n';
        for (int i=0;i<body->GetNumSurfaces();++i) {
            auto* surface=body->GetSurfaceFace(i);
            check(surface && surface->pMesh3D,"Cylinder fixture missing mesh");
            BRepAdaptor_Surface geometry(TopoDS::Face(surface->m_Face));
            double u0,u1,v0,v1; BRepTools::UVBounds(TopoDS::Face(surface->m_Face),u0,u1,v0,v1);
            const auto name=std::to_string(body_index)+"_"+std::to_string(i);
            surface->pMesh3D->ExportToObj((std::filesystem::path(output)/(name+".obj")).string());
            if(i==127) {
                const auto& mesh=*surface->pMesh3D;
                std::vector<std::vector<SurfacePatchPoint>> loops;
                size_t outer=0;double largest=0;
                for(const auto& boundary:surface->m_LastIslandBoundariesUV) {
                    std::vector<SurfacePatchPoint> loop;double area=0;
                    for(size_t k=0;k<boundary.size();++k) {
                        const auto a=boundary[k],b=boundary[(k+1)%boundary.size()];
                        loop.push_back({a.x,a.y});area+=a.x*b.y-a.y*b.x;
                    }
                    if(std::abs(area)>largest){largest=std::abs(area);outer=loops.size();}
                    loops.push_back(std::move(loop));
                }
                std::vector<Vec3> points;
                for(const auto uv:mesh.GetUVs())points.push_back({uv.u,uv.v,0});
                auto cells=mesh.GetFaces();size_t quads=0,triangles=0;
                for(auto& cell:cells)if(!cell.deleted) {
                    if(cell.corners.size()==4)++quads;else ++triangles;
                    for(auto& corner:cell.corners)corner.v=corner.uv;
                }
                CMesh3D flat;
                check(flat.SetGeometry(std::move(points),std::move(cells)),"NIST lettering UV mesh invalid");
                if(body->MeshQuadroTrimByPline) {
                    std::set<size_t> used;
                    for(const auto& cell:flat.GetFaces())if(!cell.deleted)for(auto corner:cell.corners)used.insert(corner.v);
                    // Grid intersections may subdivide a CAD segment, but may
                    // neither move it nor introduce an unmatched interior edge.
                    for(auto& loop:loops) {
                        std::vector<SurfacePatchPoint> split;
                        for(size_t k=0;k<loop.size();++k) {
                            const auto a=loop[k],b=loop[(k+1)%loop.size()];
                            const double dx=b.u-a.u,dy=b.v-a.v,len2=dx*dx+dy*dy;
                            std::vector<std::pair<double,SurfacePatchPoint>> nodes{{0,a}};
                            for(const auto index:used) {
                                const auto p=flat.GetVertices()[index];
                                const double t=((p.x-a.u)*dx+(p.y-a.v)*dy)/len2;
                                if(t>1.e-5 && t<1-1.e-5
                                    && std::hypot(p.x-a.u-t*dx,p.y-a.v-t*dy)<1.e-4)
                                    nodes.push_back({t,{p.x,p.y}});
                            }
                            std::sort(nodes.begin(),nodes.end(),[](const auto& x,const auto& y){return x.first<y.first;});
                            double previous=-1;
                            for(const auto& node:nodes)if(node.first-previous>1.e-5){split.push_back(node.second);previous=node.first;}
                        }
                        double scale=1;for(auto p:split)scale=std::max({scale,std::abs(p.u),std::abs(p.v)});
                        const auto key=[&](SurfacePatchPoint p){return std::make_pair(std::llround(double(float(p.u))/(scale*2.e-6)),std::llround(double(float(p.v))/(scale*2.e-6)));};
                        loop.clear();for(auto p:split)if(loop.empty() || key(loop.back())!=key(p))loop.push_back(p);
                        if(loop.size()>1 && key(loop.front())==key(loop.back()))loop.pop_back();
                    }
                }
                flat.ExportToObj((std::filesystem::path(output)/"panel-uv.obj").string());
                std::ofstream contour_report(std::filesystem::path(output)/"panel-loops.txt");
                contour_report.precision(17);contour_report<<outer<<'\n';
                for(const auto& loop:loops){contour_report<<loop.size()<<'\n';for(auto p:loop)contour_report<<p.u<<' '<<p.v<<'\n';}
                contour_report.close();
                check(loops.size()>=3 && quadro::ValidatePlanarTrim(loops,outer,flat),
                    "NIST lettering lost a boundary, area, convexity or manifold topology");
                check(quads>triangles,"NIST lettering recovery did not retain a quad majority");
                std::cout<<"NIST lettering: "<<quads<<" quads, "<<triangles<<" triangles, validated contours\n";
            }
            if (geometry.GetType()==GeomAbs_Cylinder && surface->GetPreparedPolylineCount()==8
                && std::abs(geometry.Cylinder().Radius()-22.225)<1.e-4 && std::abs(v1-v0-38.1)<1.e-4) {
                ++stepped_cylinders;
                const auto& mesh=*surface->pMesh3D;
                size_t cells=0;double area=0;
                for(const auto& cell:mesh.GetFaces())if(!cell.deleted) {
                    ++cells;if(!body->MeshQuadroTrimByPline)
                        check(cell.corners.size()==4,"NIST stepped cylinder contains non-quads");
                    for(size_t j=0;j<cell.corners.size();++j) {
                        const auto a=mesh.GetUVs()[cell.corners[j].uv],b=mesh.GetUVs()[cell.corners[(j+1)%cell.corners.size()].uv];
                        area+=double(a.u)*b.v-double(b.u)*a.v;
                        BRepClass_FaceClassifier classifier(TopoDS::Face(surface->m_Face),gp_Pnt2d(a.u,a.v),1.e-5);
                        check(classifier.State()!=TopAbs_OUT,"NIST cylinder cells escaped the CAD boundary");
                    }
                }
                GProp_GProps properties;BRepGProp::SurfaceProperties(surface->m_Face,properties);
                check(std::abs(std::abs(area)*.5*geometry.Cylinder().Radius()-properties.Mass())<properties.Mass()*1.e-5,
                    "NIST cylinder mesh lost CAD area");
                check(cells<=size_t(body->MeshQuadroTrimByPline?300:64),"NIST cylinder still has excessive narrow strips");
            }
            report<<name<<" type="<<int(geometry.GetType())<<" qty="<<surface->m_QtyU<<","<<surface->m_QtyV
                <<" uv="<<u0<<","<<u1<<","<<v0<<","<<v1;
            if (geometry.GetType()==GeomAbs_Cylinder) report<<" R="<<geometry.Cylinder().Radius();
            report<<" edges:";
            for (int e=0;e<surface->GetPreparedPolylineCount();++e) {
                TopoDS_Edge edge; if (!surface->GetPreparedTopoEdge(e,edge)) continue;
                double first,last; auto curve=BRep_Tool::CurveOnSurface(edge,TopoDS::Face(surface->m_Face),first,last);
                if (curve.IsNull()) continue;
                const auto a=curve->Value(first),b=curve->Value(last);
                report<<" ["<<surface->GetPreparedPolylinePointCount(e)<<":"<<a.X()<<","<<a.Y()<<"->"<<b.X()<<","<<b.Y()<<"]";
            }
            report<<'\n';
        }
        if(body->MeshQuadroTrimByPline) {
            using Key=std::array<long long,3>;using Edge=std::pair<Key,Key>;
            const auto key=[](Vec3 p){return Key{std::llround(p.x*1000.),std::llround(p.y*1000.),std::llround(p.z*1000.)};};
            std::map<Edge,int> all_edges,panel_edges;
            for(int i=0;i<body->GetNumSurfaces();++i) {
                const auto& mesh=*body->GetSurfaceFace(i)->pMesh3D;
                for(const auto& cell:mesh.GetFaces())if(!cell.deleted)
                    for(size_t k=0;k<cell.corners.size();++k) {
                        auto a=key(mesh.GetVertices()[cell.corners[k].v]),b=key(mesh.GetVertices()[cell.corners[(k+1)%cell.corners.size()].v]);
                        const Edge edge=std::minmax(a,b);++all_edges[edge];if(i==127)++panel_edges[edge];
                    }
            }
            for(const auto& [edge,count]:panel_edges)if(count==1)
                check(all_edges[edge]==2,"Regular panel has an unmatched neighbour boundary");
        }
        ++body_index;
    }
    check(stepped_cylinders==4,"NIST regression must exercise all four stepped cylinders");
    std::cout<<"Four NIST stepped cylinders: bounded spacing and complete CAD coverage\n";
}

void TestRibQuadro(const char* path, const char* output)
{
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load Rib fixture");
    CSolid* solid=nullptr;
    for(const auto& object:doc.GetObjects())if(auto* body=dynamic_cast<CSolid*>(object.get())) {
        check(!solid,"Expected one Rib solid");solid=body;
    }
    check(solid && solid->GetNumSurfaces()==15,"Rib fixture topology changed");
    solid->MeshQuadro=true;solid->MeshQuadroHoleSLX=false;
    for(float density:{.2f,1.f,.5f}) {
        check(solid->ReBuldMesh(1/density),"Rib still needs triangle fallback");
        using Key=std::array<long long,3>;
        std::map<std::pair<Key,Key>,int> edges;
        const auto key=[](const Vec3& p){return Key{std::llround(p.x*1000.),std::llround(p.y*1000.),std::llround(p.z*1000.)};};
        for(int i=0;i<solid->GetNumSurfaces();++i) {
            const auto* s=solid->GetSurfaceFace(i);
            check(s->IsInitMesh && s->pMesh3D && !s->pMesh3D->GetFaces().empty(),"Missing Rib face mesh");
            const auto& vertices=s->pMesh3D->GetVertices();
            for(const auto& f:s->pMesh3D->GetFaces())if(!f.deleted) {
                if(i==12)check(f.corners.size()==4,"Rib front face is not quad meshed");
                for(size_t k=0;k<f.corners.size();++k) {
                    auto a=key(vertices[f.corners[k].v]),b=key(vertices[f.corners[(k+1)%f.corners.size()].v]);
                    check(a!=b,"Rib has a collapsed mesh edge");if(b<a)std::swap(a,b);++edges[{a,b}];
                }
            }
        }
        for(const auto& e:edges)check(e.second==2,"Rib contains an open seam or nonmanifold edge");
        std::cout<<"Rib density="<<density<<": 15 meshed faces, closed seams, no triangle fallback\n";
    }
    std::vector<const CMesh3D*> parts;
    for(int i=0;i<solid->GetNumSurfaces();++i)parts.push_back(solid->GetSurfaceFace(i)->pMesh3D);
    auto mesh=CMesh3D::CreateWelded(parts);check(bool(mesh),"Cannot create Rib Low Poly snapshot");
    mesh->SetName("Rib - Quadro 0.50");solid->SetVisible(false);
    const auto* saved=mesh.get();doc.AddMesh(std::move(mesh));
    QTemporaryDir temp;
    const QString destination=output?QString::fromLocal8Bit(output):temp.filePath("Rib.dom3d");
    check(serializer.Save(destination,doc,room,view,{},error),"Cannot save Rib mesh");
    CAlfaDoc loaded;check(serializer.Load(destination,loaded,room,view,error),"Cannot reopen Rib mesh");
    const auto* reopened=dynamic_cast<CMesh3D*>(loaded.GetObjects().back().get());
    check(reopened && saved->GetVertices().size()==reopened->GetVertices().size()
        && saved->GetFaces().size()==reopened->GetFaces().size(),"Saved Rib mesh topology changed");
    for(size_t i=0;i<saved->GetVertices().size();++i) {
        const auto delta=saved->GetVertices()[i]-reopened->GetVertices()[i];check(dot(delta,delta)<1.e-10,"Saved Rib coordinates changed");
    }
}

void TestPeriodicBandMesh(const char* path, const char* output)
{
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load periodic band fixture");
    int tested=0;
    std::unique_ptr<CMesh3D> snapshot;
    for(const auto& object:doc.GetObjects())if(auto* solid=dynamic_cast<CSolid*>(object.get())) {
        ++tested;
        const auto verify=[&]() {
            using Key=std::array<long long,3>;
            std::map<std::pair<Key,Key>,int> edgeUse;
            int bandCount=0;
            for(int i=0;i<solid->GetNumSurfaces();++i) {
                const auto* s=solid->GetSurfaceFace(i);check(s && s->pMesh3D,"Missing surface mesh");
                const auto face=TopoDS::Face(s->m_Face);BRepAdaptor_Surface a(face);
                double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
                const bool band=(a.GetType()==GeomAbs_Torus || a.GetType()==GeomAbs_Cylinder) && std::abs(u1-u0-2*std::acos(-1.))<1.e-6;
                bandCount+=band;
                const auto& vertices=s->pMesh3D->GetVertices();
                const auto key=[](const Vec3& p){return Key{std::llround(p.x*100.),std::llround(p.y*100.),std::llround(p.z*100.)};};
                for(const auto& f:s->pMesh3D->GetFaces())if(!f.deleted) {
                    check(f.corners.size()>=3,"Invalid mesh face");
                    for(size_t k=0;k<f.corners.size();++k) {
                        auto p=key(vertices[f.corners[k].v]),q=key(vertices[f.corners[(k+1)%f.corners.size()].v]);
                        check(p!=q,"Collapsed mesh edge");if(q<p)std::swap(p,q);++edgeUse[{p,q}];
                    }
                    if(!band)continue;
                    check(f.corners.size()==4,"Periodic band contains non-quads");
                    const auto p=vertices[f.corners[0].v],q=vertices[f.corners[1].v],r=vertices[f.corners[2].v],t=vertices[f.corners[3].v];
                    const auto n=cross(q-p,r-p);
                    check(dot(n,cross(r-p,t-p))>0,"Folded periodic quad");
                    const auto center=(p+q+r+t)*.25f;
                    double u,v;
                    if(a.GetType()==GeomAbs_Torus)ElSLib::Parameters(a.Torus(),gp_Pnt(center.x,center.y,center.z),u,v);
                    else ElSLib::Parameters(a.Cylinder(),gp_Pnt(center.x,center.y,center.z),u,v);
                    u+=std::round(((u0+u1)*.5-u)/(2*std::acos(-1.)))*2*std::acos(-1.);
                    if(a.GetType()==GeomAbs_Torus)v+=std::round(((v0+v1)*.5-v)/(2*std::acos(-1.)))*2*std::acos(-1.);
                    BRepClass_FaceClassifier classifier(face,gp_Pnt2d(u,v),1.e-6);
                    check(classifier.State()==TopAbs_IN || classifier.State()==TopAbs_ON,"Band quad outside CAD face");
                    gp_Pnt point;gp_Vec du,dv;a.D1(u,v,point,du,dv);auto normal=du.Crossed(dv);
                    if(face.Orientation()==TopAbs_REVERSED)normal.Reverse();
                    check(normal.Dot(gp_Vec(n.x,n.y,n.z))>0,"Reversed band winding");
                    for(const auto& c:f.corners) {
                        const auto xyz=vertices[c.v];const auto uv=s->pMesh3D->GetUVs()[c.uv];
                        check(a.Value(uv.u,uv.v).Distance(gp_Pnt(xyz.x,xyz.y,xyz.z))<.002,"Band vertex left CAD surface");
                    }
                }
            }
            check(bandCount==3,"Fixture lost its cylinder and two torus bands");
            for(const auto& e:edgeUse)check(e.second==2,"Open edge, T-junction, or nonmanifold periodic boss");
        };
        solid->MeshQuadro=true;solid->MeshQuadroHoleSLX=false;
        for(float density:{.2f,1.f,.5f}) {
            check(solid->ReBuldMesh(1/density),"Periodic boss rebuild failed");verify();
            std::cout<<"Periodic boss density="<<density<<": quad bands, closed, CAD-conforming, consistent winding\n";
        }
        std::vector<const CMesh3D*> parts;
        for(int i=0;i<solid->GetNumSurfaces();++i)parts.push_back(solid->GetSurfaceFace(i)->pMesh3D);
        snapshot=CMesh3D::CreateWelded(parts);
        check(bool(snapshot),"Cannot create Low Poly snapshot");
        snapshot->SetName("Box And Cylinder - Quadro 0.50");
        solid->SetVisible(false);
    }
    check(tested==1,"Expected one imported solid");
    const auto* savedMesh=snapshot.get();
    doc.AddMesh(std::move(snapshot));
    QTemporaryDir temp;
    const QString destination=output?QString::fromLocal8Bit(output):temp.filePath("periodic-bands.dom3d");
    check(serializer.Save(destination,doc,room,view,{},error),"Cannot save repaired periodic bands");
    CAlfaDoc loaded;
    check(serializer.Load(destination,loaded,room,view,error),"Cannot reopen repaired periodic bands");
    const auto* reopened=dynamic_cast<CMesh3D*>(loaded.GetObjects().back().get());
    check(reopened && savedMesh->GetVertices().size()==reopened->GetVertices().size()
        && savedMesh->GetFaces().size()==reopened->GetFaces().size(),"Saved mesh topology changed");
    for(size_t k=0;k<savedMesh->GetVertices().size();++k) {
        const auto d=savedMesh->GetVertices()[k]-reopened->GetVertices()[k];check(dot(d,d)<1.e-8,"Saved mesh coordinates changed");
    }
}

void TestPeriodicBSpline(const char* path)
{
    CBSpline curve;
    for(const auto& p:std::vector<CPoint3d>{{0,0,0},{12,0,0},{12,12,0},{0,12,0}})curve.AddPoint(p);
    const auto points_close=[](const CPoint3d& a,const CPoint3d& b,double eps=1e-5) {
        return std::abs(a.x-b.x)<eps && std::abs(a.y-b.y)<eps && std::abs(a.z-b.z)<eps;
    };
    check(points_close(curve.Evaluate(0),{0,0,0}) && points_close(curve.Evaluate(1),{0,12,0}),"Open endpoints changed");
    curve.Close();
    check(points_close(curve.Evaluate(0),{10,2,0}),"Closed cubic does not use periodic B-spline basis");
    check(points_close(curve.Evaluate(0),curve.Evaluate(1)),"Periodic seam is open");
    const auto a=curve.Evaluate(0),b=curve.Evaluate(0.0001f),c=curve.Evaluate(0.9999f);
    check(points_close(b-a,a-c,1e-4),"Periodic seam tangent is discontinuous");
    for(int i=0;i<=1000;++i) {
        const auto p=curve.Evaluate(i/1000.f);
        check(p.x>=0 && p.x<=12 && p.y>=0 && p.y<=12,"Periodic curve leaves convex hull");
        for(const auto& pole:curve.GetPoints())check(!points_close(p,pole,0.1),"Cubic interpolates control polygon corners");
    }
    curve.SetDegree(1);
    check(points_close(curve.Evaluate(0),{0,0,0}),"Degree one periodic spline must interpolate poles");
    curve.SetDegree(2);
    check(points_close(curve.Evaluate(0),{6,0,0}),"Quadratic periodic basis is incorrect");
    curve.SetDegree(3);
    curve.SetWeights({1,20,3,1});
    check(points_close(curve.Evaluate(0),{10,2,0}),"Non-rational B-spline must ignore weights");
    std::stringstream stream; check(curve.Save(stream),"Cannot save periodic stream");
    CBSpline restored; check(restored.Load(stream),"Cannot load periodic stream");
    check(points_close(restored.Evaluate(0),{10,2,0}),"Periodic stream lost mode");
    std::stringstream old_stream("BSpline \"old\" 1 1 1 1 0 0 1 0 3 4\n0 0 0 1\n12 0 0 1\n12 12 0 1\n0 12 0 1\n");
    CBSpline old_curve;
    check(old_curve.Load(old_stream) && old_curve.UsesLegacyClosedInterpolation()
        && points_close(old_curve.Evaluate(0),{0,0,0}),"Legacy text stream changed shape");
    CAlfaDoc doc; Dom3DProjectSerializer serializer; QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load legacy spline file");
    auto* legacy=dynamic_cast<CBSpline*>(doc.FindObjectById(7));
    check(legacy && legacy->UsesLegacyClosedInterpolation(),"Legacy curve mode not preserved");
    check(points_close(legacy->Evaluate(0),legacy->GetPoints()[0]),"Legacy geometry changed on load");
    auto copy=legacy->Clone();
    auto* copied=dynamic_cast<CBSpline*>(copy.get());
    check(copied && copied->UsesLegacyClosedInterpolation(),"Clone lost legacy mode");
    std::vector<CPoint3d> old_points;
    for(int i=0;i<=40;++i)old_points.push_back(legacy->Evaluate(i/40.f));
    doc.AddObject(curve.Clone());
    const auto periodic_id=doc.GetObjects().back()->m_id;
    QTemporaryDir temp;
    check(serializer.Save(temp.filePath("curves.dom3d"),doc,room,view,QImage(),error),"Cannot save curve project");
    CAlfaDoc loaded;
    check(serializer.Load(temp.filePath("curves.dom3d"),loaded,room,view,error),"Cannot reload curve project");
    legacy=dynamic_cast<CBSpline*>(loaded.FindObjectById(7));
    check(legacy != nullptr,"Legacy curve missing after roundtrip");
    for(int i=0;i<=40;++i)check(points_close(legacy->Evaluate(i/40.f),old_points[i]),"Legacy shape changed after roundtrip");
    auto* periodic=dynamic_cast<CBSpline*>(loaded.FindObjectById(periodic_id));
    check(periodic && !periodic->UsesLegacyClosedInterpolation() && points_close(periodic->Evaluate(0),{10,2,0}),"Project lost periodic mode");
    legacy->Open(); legacy->Close();
    check(!legacy->UsesLegacyClosedInterpolation(),"Explicit reclosure must use the new mode");
    std::cout << "Periodic B-spline geometry and legacy persistence passed\n";
}

void TestTwoRailSurfaceDocument(const char* path)
{
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error), "Cannot load Swept-2");
    ToolRegistry registry;
    size_t index = 0;
    while (index < doc.GetObjects().size() && doc.GetObjects()[index]->m_id != 8) ++index;
    check(index < doc.GetObjects().size(), "Missing sweep surface");
    auto active = registry.ActiveObjectFromDocument(index, *doc.GetObjects()[index], 0, &doc);
    const auto set = [&](const char* id, double value) {
        for (auto& p : active.parameters) if (p.id == id) { p.value=value; return; }
        check(false, "Missing sweep parameter");
    };
    // The supplied legacy file lost all three references. These are its red
    // closed profile and two blue rails; keep the fixture unmodified.
    set("profile.id",7); set("guide1.id",6); set("guide2.id",4);
    set("dx",0); set("dy",0); set("angle",0);
    doc.FindObjectById(7)->SetParametricDefinition("",{});
    registry.Rebuild(active,doc);
    const auto signature = [&]() {
        auto* body=dynamic_cast<CSolid*>(doc.GetObjects()[index].get());
        check(body && !body->m_Shape.IsNull(),"Missing rebuilt shape");
        GProp_GProps props; BRepGProp::SurfaceProperties(body->m_Shape,props);
        const auto c=props.CentreOfMass();
        return std::array<double,4>{props.Mass(),c.X(),c.Y(),c.Z()};
    };
    const auto changed = [](const auto& a,const auto& b) {
        for(size_t i=0;i<a.size();++i) if(std::abs(a[i]-b[i])>1e-4) return true;
        return false;
    };
    const auto base=signature();
    for (const char* id : {"dx","dy","angle"}) {
        set(id, id==std::string("angle") ? -90 : 20);
        registry.Rebuild(active,doc);
        check(changed(base,signature()), "Document parameter did not change surface");
        set(id,0); registry.Rebuild(active,doc);
    }
    // Restore links in a reviewable copy of the user's file, before test edits.
    const QString repaired_path=qEnvironmentVariable("DOM3D_REPAIRED_SWEEP");
    if(!repaired_path.isEmpty())
        check(serializer.Save(repaired_path,doc,room,view,QImage(),error), "Cannot save repaired file");
    for(unsigned long id : {7UL,6UL,4UL}) {
        auto* curve=dynamic_cast<CBSpline*>(doc.FindObjectById(id));
        check(curve,"Missing source spline");
        const auto before=signature();
        const auto old=curve->GetPoints()[1];
        auto point=old; point.y+=20;
        check(curve->SetPoint(1,point),"Cannot edit spline");
        check(registry.ReplayProfileDependents(id,doc),"Dependency did not replay");
        check(changed(before,signature()),"Spline edit did not change sweep");
        curve->SetPoint(1,old); registry.ReplayProfileDependents(id,doc);
    }
    QTemporaryDir temp;
    check(temp.isValid(),"Missing test temporary directory");
    set("dx",12); registry.Rebuild(active,doc);
    check(serializer.Save(temp.filePath("sweep.dom3d"),doc,room,view,QImage(),error),"Cannot save sweep");
    CAlfaDoc loaded;
    check(serializer.Load(temp.filePath("sweep.dom3d"),loaded,room,view,error),"Cannot reload sweep");
    auto saved=registry.ActiveObjectFromDocument(index,*loaded.GetObjects()[index],0,&loaded);
    for(const auto& p:saved.parameters) {
        if(p.id=="dx")check(p.value==12,"Offset lost during save");
        if(p.id=="profile.id"||p.id=="guide1.id"||p.id=="guide2.id")check(p.value>0,"Reference lost during save");
    }
    check(registry.ReplayProfileDependents(6,loaded),"Reloaded sweep lost dependency");
    // Fresh unsaved splines have no persistent IDs until explicitly assigned.
    CAlfaDoc fresh; fresh.Clear(); fresh.GetObjects().clear();
    for(unsigned long id:{7UL,6UL,4UL}) {
        auto curve=doc.FindObjectById(id)->Clone();
        curve->SetParametricDefinition("",{});
        fresh.AddObject(std::move(curve));
    }
    fresh.ClearSelection();
    for(auto& curve:fresh.GetObjects()) {
        fresh.EnsureObjectId(*curve);
        check(fresh.SelectObjectById(curve->m_id,SelectionAction::Add),"Cannot select input");
    }
    for(auto& curve:fresh.GetObjects())curve->m_id=0;
    const auto created=registry.Activate("SurfaceSweepTwoRails",fresh);
    check(created.tool_id=="SurfaceSweepTwoRails", "Cannot activate unsaved surface");
    const auto* result=fresh.GetObjects().back().get();
    for(const auto& p:result->GetParametricParameters())
        if(p.id=="profile.id"||p.id=="guide1.id"||p.id=="guide2.id")
            check(p.value>0 && fresh.FindObjectById(static_cast<unsigned long>(p.value)),"Creation stored a zero reference");
    fresh.ClearSelection();
    fresh.SelectObjectById(fresh.GetObjects().front()->m_id,SelectionAction::Add);
    const auto count=fresh.GetObjects().size();
    check(registry.Activate("SurfaceSweepTwoRails",fresh).tool_id.empty(),"Incomplete selection must not activate a sweep");
    check(fresh.GetObjects().size()==count && fresh.GetObjects().front()->GetParametricToolId().empty(),
          "Failed activation overwrote source spline history");
    std::cout << "Two-rail surface document edits, spline dependencies and save/reload passed\n";
}

void TestCylinderQuadroNormals(const char* path)
{
    // Exercise separate position/normal/UV indexing during a quad-strip split.
    CMesh3D strip; CMesh3D::Face quad{0,1,2,3};
    for(size_t i=0;i<4;++i){quad.corners[i].n=(i==1||i==2)?1:0;quad.corners[i].uv=0;}
    check(strip.SetGeometry({{0,0,0},{2,0,0},{2,2,0},{0,2,0}}, {quad},
        {{0.3f,0.7f}}, {{0,0,1},{0.6f,0,0.8f}}),"Cannot prepare independently indexed strip");
    check(strip.SynchronizeBoundaryVertices({{1,0,0},{1,2,0}},0.01f)>0,"Quad strip was not split");
    int inserted=0;
    for(const auto& cell:strip.GetFaces())for(const auto& corner:cell.corners)if(corner.v>=4){
        ++inserted;
        check(corner.n<strip.GetNormals().size()&&corner.uv<strip.GetUVs().size(),"Split lost corner attributes");
        check(dot(strip.GetNormals()[corner.n],normalize(Vec3{0.3f,0,0.9f}))>0.9999f,"Split interpolated the wrong normal");
        const auto uv=strip.GetUVs()[corner.uv];
        check(std::abs(uv.u-0.3f)<1.e-6&&std::abs(uv.v-0.7f)<1.e-6,"Split changed shared UV");
    }
    check(inserted==4,"Unexpected quad-strip topology");
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load cylinder/prism fixture");
    int tested=0, failures=0;
    for(const auto& object:doc.GetObjects()) {
        auto* source=dynamic_cast<CSolid*>(object.get()); if(!source)continue;
        std::vector<std::unique_ptr<CSolid>> variants;
        gp_Trsf rotated;rotated.SetRotation(gp_Ax1(gp_Pnt(2,3,4),gp_Dir(1,2,3)),0.73);
        gp_Trsf mirrored;mirrored.SetMirror(gp_Ax2(gp_Pnt(1,2,3),gp_Dir(1,1,0.5)));
        variants.push_back(std::make_unique<CSolid>(source->m_Shape));
        TopoDS_Shape rotated_shape=BRepBuilderAPI_Transform(source->m_Shape,rotated,true).Shape();
        TopoDS_Shape mirrored_shape=BRepBuilderAPI_Transform(source->m_Shape,mirrored,true).Shape();
        variants.push_back(std::make_unique<CSolid>(rotated_shape));
        variants.push_back(std::make_unique<CSolid>(mirrored_shape));
        for(size_t variant=0;variant<variants.size();++variant){
        auto* solid=variants[variant].get();
        solid->MeshQuadro=true; solid->MeshQuadroHoleSLX=false;
        for(float density:{0.1f,0.25f,0.5f,0.75f,1.f,0.5f}) {
            check(solid->ReBuldMesh(1.f/density),"Cannot rebuild cylinder/prism mesh");
            std::vector<const CMesh3D*> parts;
            for(int i=0;i<solid->GetNumSurfaces();++i){
                const auto* s=solid->GetSurfaceFace(i);parts.push_back(s->pMesh3D);
                const auto& mesh=*s->pMesh3D;
                for(const auto& cell:mesh.GetFaces())if(!cell.deleted){
                    const auto a=mesh.GetVertices().at(cell.corners[0].v);
                    Vec3 expected{};for(const auto& corner:cell.corners)expected=expected+mesh.GetNormals().at(corner.n);
                    Vec3 polygon_area{};
                    for(size_t k=1;k+1<cell.corners.size();++k){
                        const auto b=mesh.GetVertices().at(cell.corners[k].v)-a,c=mesh.GetVertices().at(cell.corners[k+1].v)-a;
                        const auto area=cross(b,c);polygon_area=polygon_area+area;
                        const float roundoff=1.e-5f*std::sqrt(dot(b,b)*dot(c,c)*dot(expected,expected));
                        check(dot(area,expected)>=-roundoff,"Folded triangle in cylinder/prism body");
                    }
                    check(dot(polygon_area,expected)>1.e-10f,"Reversed or collapsed cell in cylinder/prism body");
                }
            }
            auto welded=CMesh3D::CreateWelded(parts);check(bool(welded),"Cannot weld cylinder/prism mesh");
            std::map<std::pair<size_t,size_t>,int> edges,directions;
            for(const auto& f:welded->GetFaces())if(!f.deleted)for(size_t k=0;k<f.corners.size();++k){auto a=f.corners[k].v,b=f.corners[(k+1)%f.corners.size()].v;++edges[std::minmax(a,b)];directions[std::minmax(a,b)]+=a<b?1:-1;}
            int open=0,nonmanifold=0;
            for(const auto& e:edges){if(e.second==1)++open;else if(e.second!=2||directions[e.first]!=0)++nonmanifold;}
            std::cout<<"variant="<<variant<<" density="<<density<<" open="<<open<<" nonmanifold="<<nonmanifold<<std::endl;
            failures+=open+nonmanifold;
            for(int i=0;i<solid->GetNumSurfaces();++i) {
                const auto* surface=solid->GetSurfaceFace(i);
                BRepAdaptor_Surface cad(TopoDS::Face(surface->m_Face));
                if(cad.GetType()!=GeomAbs_Cylinder)continue;
                ++tested;
                check(surface->pMesh3D,"Missing cylindrical mesh");
                const auto& mesh=*surface->pMesh3D;
                const auto cylinder=cad.Cylinder();
                const gp_Vec axis(cylinder.Axis().Direction());
                int badNormals=0,badWinding=0,corners=0,cells=0;
                for(const auto& cell:mesh.GetFaces()) {
                    if(cell.deleted || cell.corners.size()<3)continue;
                    ++cells;Vec3 area{};const auto a=mesh.GetVertices().at(cell.corners[0].v);
                    for(size_t k=1;k+1<cell.corners.size();++k)
                        area=area+cross(mesh.GetVertices().at(cell.corners[k].v)-a,mesh.GetVertices().at(cell.corners[k+1].v)-a);
                    gp_Vec expectedMean;
                    for(const auto& corner:cell.corners) {
                        const auto p=mesh.GetVertices().at(corner.v);
                        gp_Vec radial(cylinder.Location(),gp_Pnt(p.x,p.y,p.z));
                        radial-=axis.Multiplied(radial.Dot(axis));radial.Normalize();
                        if(!cylinder.Position().Direct())radial.Reverse();
                        if(surface->m_Face.Orientation()==TopAbs_REVERSED)radial.Reverse();
                        expectedMean+=radial;++corners;
                        if(corner.n>=mesh.GetNormals().size()){++badNormals;continue;}
                        const auto n=mesh.GetNormals()[corner.n];
                        if(gp_Vec(n.x,n.y,n.z).Dot(radial)<0.98){
                            if(badNormals<8)std::cout<<" bad v="<<corner.v<<" n="<<corner.n<<" xyz="<<p.x<<","<<p.y<<","<<p.z<<" normal="<<n.x<<","<<n.y<<","<<n.z<<" expected="<<radial.X()<<","<<radial.Y()<<","<<radial.Z()<<'\n';
                            ++badNormals;
                        }
                    }
                    if(expectedMean.Dot(gp_Vec(area.x,area.y,area.z))<=0)++badWinding;
                }
                std::cout<<"Cylinder density="<<density<<" face="<<i<<" cells="<<cells<<" corners="<<corners<<" badNormals="<<badNormals<<" badWinding="<<badWinding<<std::endl;
                failures+=badNormals+badWinding;
            }
        }
        }
    }
    check(tested>0,"Cylinder fixture contains no cylindrical faces");
    check(failures==0,"Cylinder/prism seams, normals or winding are invalid");
}

void TestFilletMeshNormals(const char* path)
{
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load fillet fixture");
    int spherical=0, failures=0, triangulated=0;
    for(const auto& object:doc.GetObjects()) {
        auto* solid=dynamic_cast<CSolid*>(object.get());
        if(!solid)continue;
        check(solid->InitSurfaces(),"Cannot initialize fillet faces");
        for(int mode : {0,1,2}) {
        solid->MeshQuadro=mode!=0;
        solid->MeshQuadroHoleSLX=mode==2;
        const std::vector<float> deflections = mode==0
            ? std::vector<float>{0.25f,1.f,5.f} : std::vector<float>{5.f,2.f,1.f};
        for(float deflection:deflections) {
        check(solid->ReBuldMesh(deflection),"Cannot rebuild hybrid fillet mesh");
        for(int i=0;i<solid->GetNumSurfaces();++i) {
            auto* face=solid->GetSurfaceFace(i);
            BRepAdaptor_Surface cad(TopoDS::Face(face->m_Face));
            if(cad.GetType()!=GeomAbs_Sphere)continue;
            ++spherical;
            const auto& mesh=*face->pMesh3D;
            int wrong=0,checked=0;
            for(const auto& cell:mesh.GetFaces()) {
                if(cell.deleted||cell.corners.size()<3)continue;
                const auto a=mesh.GetVertices()[cell.corners[0].v];
                const auto b=mesh.GetVertices()[cell.corners[1].v];
                const auto c=mesh.GetVertices()[cell.corners[2].v];
                const gp_Vec n=gp_Vec(gp_Pnt(a.x,a.y,a.z),gp_Pnt(b.x,b.y,b.z)).Crossed(gp_Vec(gp_Pnt(a.x,a.y,a.z),gp_Pnt(c.x,c.y,c.z)));
                // A collapsed pole side can make the first triangle of a
                // quad degenerate. Its corner normals still reach rendering.
                for(const auto& corner:cell.corners) {
                    check(corner.n<mesh.GetNormals().size(),"Missing corner normal");
                    const auto normal=mesh.GetNormals()[corner.n];
                    const auto vertex=mesh.GetVertices().at(corner.v);
                    gp_Vec radial(cad.Sphere().Location(),gp_Pnt(vertex.x,vertex.y,vertex.z));
                    radial.Normalize();
                    if(!cad.Sphere().Position().Direct())radial.Reverse();
                    if(face->m_Face.Orientation()==TopAbs_REVERSED)radial.Reverse();
                    check(radial.Dot(gp_Vec(normal.x,normal.y,normal.z))>0.9999,
                          "Spherical fillet normal differs from analytic radial normal");
                    ++checked;
                    // Preserve the triangulator winding regression. Quadro's
                    // normals are checked independently against the CAD sphere
                    // above, including cells with collapsed pole sides.
                    if(mode==0 && n.SquareMagnitude()>=1.e-20 && n.Dot(gp_Vec(normal.x,normal.y,normal.z))<=0)++wrong;
                }
            }
            std::cout<<"sphere face="<<i<<" direct="<<cad.Sphere().Position().Direct()<<" reversed="<<(face->m_Face.Orientation()==TopAbs_REVERSED)<<" checked="<<checked<<" wrong="<<wrong<<std::endl;
            failures+=wrong;
            if(checked>0)++triangulated;
        }
    }
    }
    }
    check(spherical>=4&&triangulated>=12,"Missing triangulated rounded corners in fixture");
    check(failures==0,"Fillet normals oppose triangle winding");
}

void TestTwoSketchCornerNormals(const char* path)
{
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load Two_SL fixture");
    int tested=0;
    for(const auto& object:doc.GetObjects()) {
        auto* solid=dynamic_cast<CSolid*>(object.get());
        if(!solid)continue;
        ++tested;
        solid->MeshQuadro=true;
        solid->MeshQuadroHoleSLX=false;
        for(float density:{0.20f,0.35f,0.50f,0.80f,0.20f}) {
            check(solid->ReBuldMesh(1.f/density),"Cannot rebuild Two_SL mesh");
            int checked=0,wrong=0;
            for(int i=0;i<solid->GetNumSurfaces();++i) {
                const auto* face=solid->GetSurfaceFace(i);
                BRepAdaptor_Surface cad(TopoDS::Face(face->m_Face));
                if(cad.GetType()!=GeomAbs_BSplineSurface)continue;
                const auto& mesh=*face->pMesh3D;
                int faceWrong=0;
                for(const auto& cell:mesh.GetFaces()) {
                    if(cell.deleted||cell.corners.size()<3)continue;
                    Vec3 area{};
                    const auto a=mesh.GetVertices().at(cell.corners[0].v);
                    for(size_t k=1;k+1<cell.corners.size();++k) {
                        const auto b=mesh.GetVertices().at(cell.corners[k].v);
                        const auto c=mesh.GetVertices().at(cell.corners[k+1].v);
                        area=area+cross(b-a,c-a);
                    }
                    if(dot(area,area)<1.e-16f)continue;
                    for(const auto& corner:cell.corners) {
                        check(corner.n<mesh.GetNormals().size(),"Missing Two_SL normal");
                        const auto n=mesh.GetNormals()[corner.n];
                        ++checked;
                        if(!std::isfinite(dot(n,area))||dot(n,area)<=0) {
                            ++faceWrong;
                        }
                    }
                }
                if(faceWrong)std::cout<<"Two_SL face="<<i<<" wrong="<<faceWrong<<'\n';
                wrong+=faceWrong;
            }
            std::cout<<"Two_SL density="<<density<<" checked="<<checked<<" wrong="<<wrong<<std::endl;
            check(checked>0&&wrong==0,"Two_SL normals oppose mesh winding");
        }
    }
    check(tested==1,"Expected one Two_SL solid");
}
