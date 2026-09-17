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

int TestSurfaceDisplayNet()
{
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
    const auto new_index=doc.GetObjects().size(); doc.AddObject(curve.Clone());
    QTemporaryDir temp;
    check(serializer.Save(temp.filePath("curves.dom3d"),doc,room,view,QImage(),error),"Cannot save curve project");
    CAlfaDoc loaded;
    check(serializer.Load(temp.filePath("curves.dom3d"),loaded,room,view,error),"Cannot reload curve project");
    legacy=dynamic_cast<CBSpline*>(loaded.FindObjectById(7));
    for(int i=0;i<=40;++i)check(points_close(legacy->Evaluate(i/40.f),old_points[i]),"Legacy shape changed after roundtrip");
    auto* periodic=dynamic_cast<CBSpline*>(loaded.GetObjects()[new_index].get());
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
        solid->MeshQuadro=false;
        for(float deflection:{0.25f,1.f,5.f}) {
        check(solid->ReBuldMesh(deflection),"Cannot rebuild hybrid fillet mesh");
        for(int i=0;i<solid->GetNumSurfaces();++i) {
            auto* face=solid->GetSurfaceFace(i);
            BRepAdaptor_Surface cad(TopoDS::Face(face->m_Face));
            if(cad.GetType()!=GeomAbs_Sphere)continue;
            ++spherical;
            const auto& mesh=*face->pMesh3D;
            int wrong=0,checked=0;
            for(const auto& cell:mesh.GetFaces()) {
                if(cell.deleted||cell.corners.size()!=3)continue;
                const auto a=mesh.GetVertices()[cell.corners[0].v];
                const auto b=mesh.GetVertices()[cell.corners[1].v];
                const auto c=mesh.GetVertices()[cell.corners[2].v];
                const gp_Vec n=gp_Vec(gp_Pnt(a.x,a.y,a.z),gp_Pnt(b.x,b.y,b.z)).Crossed(gp_Vec(gp_Pnt(a.x,a.y,a.z),gp_Pnt(c.x,c.y,c.z)));
                if(n.SquareMagnitude()<1.e-20)continue;
                for(const auto& corner:cell.corners) {
                    check(corner.n<mesh.GetNormals().size(),"Missing corner normal");
                    const auto normal=mesh.GetNormals()[corner.n];
                    ++checked;
                    if(n.Dot(gp_Vec(normal.x,normal.y,normal.z))<=0)++wrong;
                }
            }
            std::cout<<"sphere face="<<i<<" direct="<<cad.Sphere().Position().Direct()<<" reversed="<<(face->m_Face.Orientation()==TopAbs_REVERSED)<<" checked="<<checked<<" wrong="<<wrong<<std::endl;
            failures+=wrong;
            if(checked>0)++triangulated;
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
