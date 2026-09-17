#include "solid/PanelContourShapeBuilder.h"
#include "CAlfaDoc.h"
#include "CPolyline.h"
#include "solid/SurfaceSet.h"
#include "ui/ToolRegistry.h"
#include "Dom3DProjectSerializer.h"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepTools.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <limits>
#include <BRepClass_FaceClassifier.hxx>
#include <gp_Pln.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Cylinder.hxx>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const std::string& error) { if (!ok) { std::cerr << error << std::endl; std::exit(EXIT_FAILURE); } }
double area(const TopoDS_Shape& shape) {
    GProp_GProps properties; BRepGProp::SurfaceProperties(shape, properties); return properties.Mass();
}
TopoDS_Wire rectangle(double x0, double y0, double x1, double y1, double z) {
    BRepBuilderAPI_MakePolygon p;
    for (auto point : {gp_Pnt(x0,y0,z),gp_Pnt(x1,y0,z),gp_Pnt(x1,y1,z),gp_Pnt(x0,y1,z)}) p.Add(point);
    p.Close(); return p.Wire();
}
void expect_z(const TopoDS_Shape& shape, double z) {
    Bnd_Box box; BRepBndLib::Add(shape, box);
    double x0,y0,z0,x1,y1,z1; box.Get(x0,y0,z0,x1,y1,z1);
    check(std::abs(z0-z) < 1.e-4 && std::abs(z1-z) < 1.e-4, "Panel depth is wrong");
}
}

void TestPanelContour() {
    const QString panelFixture=qEnvironmentVariable("DOM3D_PANEL_CONTOUR_INPUT");
    if(!panelFixture.isEmpty()) {
        CAlfaDoc doc;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,message;
        check(serializer.Load(panelFixture,doc,room,view,message),message.toStdString());
        auto* body=dynamic_cast<CSolid*>(doc.FindObjectById(6));check(body,"Missing panel fixture body");
        const auto wire=doc.BuildCurveWire(8);check(!wire.IsNull(),"Missing panel fixture contour");
        Vec3 f,r,u;camera_basis(view.camera,f,r,u);const gp_Dir direction(f.x,f.y,f.z);
        TopExp_Explorer faces(body->m_Shape,TopAbs_FACE);check(faces.More(),"No fixture face");
        const auto face=TopoDS::Face(faces.Current());
        PanelContourResult result;std::string error;
        const double sourceArea=area(face);
        for(bool larger:{false,true}) {
            check(BuildPanelContourShape(body->m_Shape,face,wire,direction,larger,0,0,result,error),error);
            check(std::abs(area(result.panel)+area(result.remainder)-sourceArea)<sourceArea*1.e-6,
                "Saved panel split lost surface area");
            if(!larger) for(double profile:{0.25,0.5,0.7,0.75}) {
                TopoDS_Shape bulged,solid;
                check(BuildBulgedPanelFace(TopoDS::Face(result.panel),6,bulged,error,profile),error);
                BRepAdaptor_Surface surface(TopoDS::Face(bulged));
                double u0,u1,v0,v1;BRepTools::UVBounds(TopoDS::Face(bulged),u0,u1,v0,v1);
                double peak=0;
                for(int i=1;i<32;++i) for(int j=1;j<32;++j) {
                    const double u=u0+(u1-u0)*i/32,v=v0+(v1-v0)*j/32;
                    BRepClass_FaceClassifier c(TopoDS::Face(bulged),gp_Pnt2d(u,v),1.e-7);
                    if(c.State()!=TopAbs_IN) continue;
                    BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(surface.Value(u,v)).Vertex(),result.panel);
                    if(distance.IsDone()) peak=std::max(peak,distance.Value());
                }
                std::cout<<"Profile "<<profile<<" peak "<<peak<<std::endl;
                check(peak<=6.03,"Profile shoulders exceed the intended apex height");
                check(BuildThickenedPanel(bulged,1,solid,error),error);
                CSolid display(solid);
                check(display.ReBuldMesh(),"Cannot mesh saved panel profile");
            }
            check(BuildPanelContourShape(body->m_Shape,face,wire,direction,larger,0,0,result,error,1),error);
            check(TopExp_Explorer(result.panel,TopAbs_SOLID).More() && BRepCheck_Analyzer(result.panel).IsValid(),"Panel fixture did not produce solid");
            check(BRepCheck_Analyzer(result.remainder).IsValid(),"Invalid fixture remainder");
        }
        std::cout<<"Saved panel contour passed\n";return;
    }
    const QString fillet_fixture=qEnvironmentVariable("DOM3D_HEADLAMP_FILLET_INPUT");
    if (!fillet_fixture.isEmpty()) {
        CAlfaDoc document; Dom3DProjectSerializer serializer; ProjectViewState view;
        QString message, room;
        check(serializer.Load(fillet_fixture,document,room,view,message),"Cannot load headlamp shell");
        bool done=false;
        for (auto& object:document.GetObjects()) {
            auto* solid=dynamic_cast<CSolid*>(object.get());
            if (!solid || object->GetParametricToolId()!="SolidShell") continue;
            unsigned long source_id=0;
            for (const auto& p:object->GetParametricParameters()) if(p.id=="surface.id") source_id=static_cast<unsigned long>(p.value);
            auto* source=dynamic_cast<CSolid*>(document.FindObjectById(source_id));
            if (!source || source->GetParametricToolId()!="SurfaceBulge") continue;
            check(solid->ReBuldMesh(),"Cannot prepare shell edges");
            int front=-1; double nearest=1.e100;
            for(int i=0;i<solid->GetNumSurfaces();++i) {
                const auto face=TopoDS::Face(solid->GetSurfaceFace(i)->m_Face);
                if(area(face)<area(source->m_Shape)*0.5) continue;
                double u0,u1,v0,v1; BRepTools::UVBounds(face,u0,u1,v0,v1);
                const auto point=BRepAdaptor_Surface(face).Value((u0+u1)*0.5,(v0+v1)*0.5);
                BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(point).Vertex(),source->m_Shape);
                if(distance.IsDone() && distance.Value()<nearest) {nearest=distance.Value();front=i;}
            }
            check(front>=0 && nearest<0.005,"Cannot identify shell front surface");
            const int count=solid->GetSurfaceFace(front)->GetEdgeCount();
            std::vector<ParametricParameterValue> parameters={{"radius",0.3},{"edge.count",double(count)}};
            for(int e=0;e<count;++e) {
                parameters.push_back({"edge."+std::to_string(e)+".surface",double(front)});
                parameters.push_back({"edge."+std::to_string(e)+".edge",double(e)});
            }
            std::cout<<"Fillet front "<<front<<" edges "<<count<<" radius 0.3"<<std::endl;
            solid->SetParametricOperation(solid->GetNumOperations(),"fillet_edge","Headlamp rim R0.3",parameters);
            ToolRegistry registry;
            check(registry.ReplayOperations(document.FindObjectIndexById(object->m_id),document),"Headlamp rim fillet failed");
            check(BRepCheck_Analyzer(solid->m_Shape).IsValid(),"Invalid filleted shell");
            solid->SetName("Headlamp Shell - Rim R0.3");
            done=true;
        }
        check(done,"No bulged headlamp shell found");
        const QString output=qEnvironmentVariable("DOM3D_HEADLAMP_FILLET_OUTPUT");
        check(!output.isEmpty() && serializer.Save(output,document,room,view,{},message),"Cannot save headlamp fillet");
        CAlfaDoc reloaded;
        check(serializer.Load(output,reloaded,room,view,message),"Cannot reload headlamp fillet");
        bool verified=false;
        for(auto& object:reloaded.GetObjects()) {
            auto* shell=dynamic_cast<CSolid*>(object.get());
            if(!shell || shell->GetName()!="Headlamp Shell - Rim R0.3") continue;
            check(shell->GetNumOperations()==2,"Lost shell fillet history");
            check(BRepCheck_Analyzer(shell->m_Shape).IsValid(),"Saved fillet is invalid");
            check(TopExp_Explorer(shell->m_Shape,TopAbs_SOLID).More(),"Fillet lost the solid");
            check(shell->ReBuldMesh(),"Cannot mesh saved fillet");
            verified=true;
        }
        check(verified,"Saved filleted shell missing");
        std::cout<<"Headlamp rim saved"<<std::endl;
        return;
    }
    const QString mesh_fixture=qEnvironmentVariable("DOM3D_PANEL_MESH_FIXTURE");
    if (!mesh_fixture.isEmpty()) {
        CAlfaDoc document; Dom3DProjectSerializer serializer; ProjectViewState view;
        QString message, room;
        check(serializer.Load(mesh_fixture,document,room,view,message),"Cannot load panel fixture");
        size_t tested=0;
        for (auto& object:document.GetObjects()) {
            auto* solid=dynamic_cast<CSolid*>(object.get());
            if (!solid) continue;
            if (object->GetParametricToolId()!="SurfaceBulge") continue;
            ++tested;
            ToolRegistry registry;
            auto active=registry.ActiveObjectFromDocument(document.FindObjectIndexById(object->m_id),*object,0,&document);
            check(registry.TryRebuildBulge(active,document),"Cannot rebuild the car headlamp bulge");
            const QString repaired_path=qEnvironmentVariable("DOM3D_BULGE_REPAIRED");
            if (!repaired_path.isEmpty())
                check(serializer.Save(repaired_path,document,room,view,{},message),"Cannot save repaired bulge project");
            for (bool slx:{false,true}) for (float density:{0.30f,0.25f,0.5f}) {
                solid->MeshQuadro=true; solid->MeshQuadroHoleSLX=slx;
                solid->ptchDensity=density;
                check(solid->ReBuldMesh(1/density),"Cannot build bulged panel mesh");
                double mesh_area=0;
                for (int i=0;i<solid->GetNumSurfaces();++i) {
                    const auto* mesh=solid->GetSurfaceFace(i)->pMesh3D;
                    check(mesh!=nullptr,"Missing panel mesh");
                    const auto& vertices=mesh->GetVertices();
                    check(mesh->GetUVs().size()==vertices.size(),"Panel mesh has no surface coordinates");
                    const QString preview_path=qEnvironmentVariable("DOM3D_BULGE_PREVIEW_OBJ");
                    if (!preview_path.isEmpty() && !slx && std::abs(density-0.3f)<1.e-5)
                        check(mesh->ExportToObj(preview_path.toStdString()),"Cannot export bulge preview mesh");
                    BRepAdaptor_Surface evaluator(TopoDS::Face(solid->GetSurfaceFace(i)->m_Face));
                    double error=0;
                    double source_distance=0; int outside=0;
                    unsigned long source_id=0;
                    for (auto p:object->GetParametricParameters()) if (p.id=="surface.id") source_id=static_cast<unsigned long>(p.value);
                    const auto* source=dynamic_cast<const CSolid*>(document.FindObjectById(source_id));
                    check(source!=nullptr,"Missing source panel for bulge validation");
                    for (size_t v=0;v<vertices.size();++v) {
                        const auto uv=mesh->GetUVs()[v];
                        const auto p=evaluator.Value(uv.u,uv.v);
                        const auto actual=vertices[v];
                        BRepClass_FaceClassifier classifier(TopoDS::Face(solid->GetSurfaceFace(i)->m_Face),gp_Pnt2d(uv.u,uv.v),1.e-5);
                        if (classifier.State()==TopAbs_OUT) ++outside;
                        BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(gp_Pnt(actual.x,actual.y,actual.z)).Vertex(),source->m_Shape);
                        if (distance.IsDone()) source_distance=std::max(source_distance,distance.Value());
                        error=std::max(error,p.Distance(gp_Pnt(actual.x,actual.y,actual.z)));
                    }
                    std::cout << "vertex error " << error << " outside " << outside << " source distance " << source_distance << std::endl;
                    check(error<0.005,"Mesh vertices left their CAD surface");
                    check(outside==0,"Mesh vertices escaped the trimmed CAD face");
                    check(source_distance<8.6*1.25,"Bulge height 8.6 produced an interior spike");
                    for (const auto& face:mesh->GetFaces()) {
                        if (face.deleted || face.corners.size()<3) continue;
                        const auto a=vertices[face.corners[0].v];
                        for (size_t j=1;j+1<face.corners.size();++j) {
                            const auto b=vertices[face.corners[j].v]-a,c=vertices[face.corners[j+1].v]-a;
                            const auto n=cross(b,c); mesh_area+=0.5*std::sqrt(dot(n,n));
                        }
                    }
                }
                const double ratio=mesh_area/area(solid->m_Shape);
                std::cout << "density " << density << " coverage " << ratio << std::endl;
                check(ratio>0.95 && ratio<1.05,"Bulged panel mesh does not cover the complete CAD face");
            }
        }
        check(tested>0,"Fixture has no bulged panel");
        return;
    }
    std::cout << "Panel: planar split and inset" << std::endl;
    TopoDS_Face face = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,100,0,100).Face();
    {
        std::cout << "Panel: bulge with fixed boundary" << std::endl;
        TopoDS_Shape bulged; std::string error;
        check(BuildBulgedPanelFace(face,5,bulged,error),error);
        check(BRepCheck_Analyzer(bulged).IsValid(),"Invalid bulged face");
        BRepExtrema_DistShapeShape peak(BRepBuilderAPI_MakeVertex(gp_Pnt(50,50,5)).Vertex(),bulged);
        check(peak.IsDone() && peak.Value()<0.01,"Bulge did not reach the requested height");
        for (TopExp_Explorer it(face,TopAbs_EDGE); it.More(); it.Next()) {
            BRepAdaptor_Curve edge(TopoDS::Edge(it.Current()));
            for (int i=0;i<=10;++i) {
                auto point=edge.Value(edge.FirstParameter()+(edge.LastParameter()-edge.FirstParameter())*i/10);
                BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(point).Vertex(),bulged);
                check(distance.IsDone() && distance.Value()<0.005,"Bulge moved its boundary");
            }
        }
        check(BuildBulgedPanelFace(face,-5,bulged,error),error);
        BRepExtrema_DistShapeShape inward(BRepBuilderAPI_MakeVertex(gp_Pnt(50,50,-5)).Vertex(),bulged);
        check(inward.IsDone() && inward.Value()<0.01,"Negative bulge points in the wrong direction");
        check(BuildBulgedPanelFace(face,0,bulged,error) && bulged.IsSame(face),"Zero bulge must preserve the source exactly");
    }
    BRepBuilderAPI_MakePolygon open;
    open.Add(gp_Pnt(-10,30,3)); open.Add(gp_Pnt(110,30,3));
    PanelContourResult result; std::string error;
    BRepBuilderAPI_MakePolygon boundary_cut;
    boundary_cut.Add(gp_Pnt(0,30,0)); boundary_cut.Add(gp_Pnt(100,30,0));
    check(BuildPanelContourShape(face,face,boundary_cut.Wire(),gp_Dir(0,0,-1),false,0,0,result,error),
          "Exact boundary endpoints must split a panel: "+error);
    check(std::abs(area(result.panel)-3000)<1.e-4,"Boundary-snapped panel has wrong area");
    check(BuildPanelContourShape(face,face,open.Wire(),gp_Dir(0,0,-1),false,0,0,result,error), error);
    check(std::abs(area(result.panel)-3000)<1.e-4 && std::abs(area(result.remainder)-7000)<1.e-4,
          "Open curve did not split into the expected regions");
    check(BuildPanelContourShape(face,face,open.Wire(),gp_Dir(0,0,-1),false,2,5,result,error), error);
    check(std::abs(area(result.panel)-2496)<1.e-3, "Panel gap has the wrong width");
    expect_z(result.panel,-5);
    check(BuildPanelContourShape(face,face,open.Wire(),gp_Dir(0,0,-1),false,2,-5,result,error),error);
    expect_z(result.panel,5);
    check(BuildPanelContourShape(face,face,open.Wire(),gp_Dir(0,0,-1),false,2,-5,result,error,2),error);
    {
        Bnd_Box bounds; BRepBndLib::Add(result.panel,bounds);
        double x0,y0,z0,x1,y1,z1; bounds.Get(x0,y0,z0,x1,y1,z1);
        check(std::abs(z0-3)<1.e-4 && std::abs(z1-5)<1.e-4
            && BRepCheck_Analyzer(result.panel).IsValid(),"Outward panel lost one-sided thickness");
    }
    check(std::abs(area(result.remainder)-7000)<1.e-4, "Panel gap changed the surrounding surface");
    std::cout << "Panel: reversed normals and closed contour" << std::endl;
    face.Reverse();
    check(BuildPanelContourShape(face,face,open.Wire(),gp_Dir(0,0,-1),false,0,5,result,error), error);
    expect_z(result.panel,5);
    face.Reverse();
    check(BuildPanelContourShape(face,face,rectangle(20,20,40,40,3),gp_Dir(0,0,1),false,0,0,result,error), error);
    check(std::abs(area(result.panel)-400)<1.e-4 && std::abs(area(result.remainder)-9600)<1.e-4,
          "Closed contour did not extract its inner panel");
    check(BuildPanelContourShape(face,face,rectangle(20,20,40,40,3),gp_Dir(0,0,1),true,2,0,result,error), error);
    for (TopExp_Explorer it(result.panel,TopAbs_FACE); it.More(); it.Next()) {
        BRepClass_FaceClassifier at_gap(TopoDS::Face(it.Current()),gp_Pnt(19,30,0),1.e-6);
        check(at_gap.State()==TopAbs_OUT, "Gap did not expand the panel's inner hole");
    }
    check(!BuildPanelContourShape(face,face,open.Wire(),gp_Dir(0,0,-1),false,1000,0,result,error), "Oversized gap accepted");
    BRepBuilderAPI_MakePolygon short_line; short_line.Add(gp_Pnt(20,30,2)); short_line.Add(gp_Pnt(40,30,2));
    check(!BuildPanelContourShape(face,face,short_line.Wire(),gp_Dir(0,0,-1),false,0,0,result,error), "Interior open cut accepted");
    BRepBuilderAPI_MakePolygon nearly_snapped;
    nearly_snapped.Add(gp_Pnt(1.e-5,30,2)); nearly_snapped.Add(gp_Pnt(100-1.e-5,30,2));
    check(BuildPanelContourShape(face,face,nearly_snapped.Wire(),gp_Dir(0,0,-1),false,0,0,result,error,1),error);
    check(BRepCheck_Analyzer(result.panel).IsValid(),"Near-boundary panel is invalid");
    BRepBuilderAPI_MakePolygon real_gap;
    real_gap.Add(gp_Pnt(.01,30,2)); real_gap.Add(gp_Pnt(99.99,30,2));
    check(!BuildPanelContourShape(face,face,real_gap.Wire(),gp_Dir(0,0,-1),false,0,0,result,error),
        "Boundary tolerance bridged a real gap");
    check(BRepCheck_Analyzer(face).IsValid() && std::abs(area(face)-10000)<1.e-4, "Construction modified the source face");

    std::cout << "Panel: curved surface" << std::endl;
    for(double profile:{0.25,0.5,0.7,0.75}) for(double height:{-5.0,5.0}) {
        TopoDS_Shape bulged,thick;
        check(BuildBulgedPanelFace(face,height,bulged,error,profile),error);
        const double shoulder=height*std::pow(std::sqrt(0.5),std::exp2(2-4*profile));
        for(const gp_Pnt point:{gp_Pnt(50,50,height),gp_Pnt(25,50,shoulder),gp_Pnt(0,50,0)}) {
            BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(point).Vertex(),bulged);
            check(distance.IsDone() && distance.Value()<0.005,"Profile lost apex, shoulder or fixed boundary");
        }
        check(BuildThickenedPanel(bulged,1,thick,error),error);
    }
    // The panel follows a curved source and recedes along its normals.
    TopoDS_Face cylinder = BRepBuilderAPI_MakeFace(
        gp_Cylinder(gp_Ax3(gp_Pnt(0,0,0),gp_Dir(0,0,1)),100),-.6,.6,0,50).Face();
    BRepBuilderAPI_MakePolygon contour;
    for (auto p : {gp_Pnt(105,-20,10),gp_Pnt(105,20,10),gp_Pnt(105,20,30),gp_Pnt(105,-20,30)}) contour.Add(p);
    contour.Close();
    check(BuildPanelContourShape(cylinder,cylinder,contour.Wire(),gp_Dir(-1,0,0),false,2,5,result,error), error);
    check(BRepCheck_Analyzer(result.panel).IsValid(), "Curved panel is invalid");
    bool curved = false;
    for (TopExp_Explorer it(result.panel,TopAbs_FACE); it.More(); it.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
        if (surface.GetType() == GeomAbs_Cylinder) {
            curved = true; check(std::abs(surface.Cylinder().Radius()-95)<1.e-5, "Curved recess is not a normal offset");
        }
    }
    check(curved, "Panel was flattened instead of following the surface");
    TopoDS_Shape thickCurved;
    check(BuildThickenedPanel(result.panel,2,thickCurved,error),error);
    check(TopExp_Explorer(thickCurved,TopAbs_SOLID).More() && BRepCheck_Analyzer(thickCurved).IsValid(),
        "Curved contour panel did not become a valid solid");
    {
        TopExp_Explorer it(result.panel,TopAbs_FACE);
        TopoDS_Shape raised;
        check(BuildBulgedPanelFace(TopoDS::Face(it.Current()),2,raised,error),error);
        check(BRepCheck_Analyzer(raised).IsValid(),"Curved bulge is invalid");
        check(BuildThickenedPanel(raised,2,thickCurved,error),error);
        check(TopExp_Explorer(thickCurved,TopAbs_SOLID).More(),"Curved bulge thickening lost solid");
        auto surface=std::make_unique<CSurfaceSet>(raised);
        check(surface->ReBuldMesh(),"Cannot mesh the curved bulge");
        const auto shell=CSolid::Shell(surface->GetSurfaceFace(0),1,&error);
        check(!shell.IsNull() && BRepCheck_Analyzer(shell).IsValid(),"Shell failed on curved bulge: "+error);
        bool filleted=false;
        for (TopExp_Explorer edge(shell,TopAbs_EDGE); edge.More() && !filleted; edge.Next()) {
            BRepFilletAPI_MakeFillet fillet(shell);
            fillet.Add(0.1,TopoDS::Edge(edge.Current())); fillet.Build();
            filleted=fillet.IsDone() && BRepCheck_Analyzer(fillet.Shape()).IsValid();
        }
        check(filleted,"No shell edge could be filleted after bulging");
    }

    std::cout << "Panel: spline and saved replay" << std::endl;
    CAlfaDoc document; document.GetObjects().clear();
    TopoDS_Shape shape = face;
    document.AddObject(std::make_unique<CSurfaceSet>(shape));
    const auto source_id = document.GetSelectedObject()->m_id;
    auto curve = std::make_unique<CPolyline>("Panel cut"); curve->AddPoint({-10,30,3}); curve->AddPoint({110,30,3});
    document.AddObject(std::move(curve));
    const auto curve_id = document.GetSelectedObject()->m_id;
    // Use an actual cubic spline wire as well as the straight clipping fixture.
    auto spline_object = std::make_unique<CBSpline>("Curved panel contour");
    spline_object->SetDegree(3);
    for (auto p : {CPoint3d(-10,30,3),CPoint3d(20,20,3),CPoint3d(80,40,3),CPoint3d(110,30,3)}) spline_object->AddPoint(p);
    document.AddObject(std::move(spline_object));
    const auto spline_wire = document.BuildCurveWire(document.GetSelectedObject()->m_id);
    check(BuildPanelContourShape(face,face,spline_wire,gp_Dir(0,0,-1),false,0,1,result,error), error);
    check(area(result.panel)>0 && area(result.remainder)>0, "Cubic contour did not divide the surface");
    document.GetObjects().pop_back();
    document.ClearSelection();
    document.SelectObjectById(curve_id);
    ToolRegistry tools; const auto* definition = tools.Find("SolidContourPanel");
    check(definition, "Panel tool is not registered"); auto parameters = definition->defaults;
    for (auto& p : parameters) {
        if (p.id=="surface.id") p.value=source_id;
        if (p.id=="profile.id") p.value=curve_id;
        if (p.id=="gap") p.value=2;
        if (p.id=="depth") p.value=5;
    }
    auto invalid_parameters = parameters;
    for (auto& p : invalid_parameters) if (p.id=="gap") p.value=1000;
    tools.CreateParametricObject("SolidContourPanel",document,invalid_parameters);
    check(document.GetObjects().size()==2 && document.GetSelectedObject()->GetParametricToolId().empty(),
          "Rejected panel changed the selected contour metadata");
    tools.CreateParametricObject("SolidContourPanel",document,parameters);
    check(document.GetObjects().size()==3, "Parametric panel was not created");
    const auto panel_id = document.GetSelectedObject()->m_id;
    for (auto& p : parameters) if (p.id=="output") p.value=1;
    tools.CreateParametricObject("SolidContourPanel",document,parameters);
    check(document.GetObjects().size()==4, "Surround output was not created");
    const auto surround_id = document.GetSelectedObject()->m_id;
    document.FindObjectById(source_id)->SetVisible(false);
    QTemporaryDir directory; Dom3DProjectSerializer serializer; ProjectViewState view;
    QString message, room="Solid", path=directory.filePath("panel.dom3d");
    check(serializer.Save(path,document,room,view,{},message), "Cannot save panel");
    CAlfaDoc loaded; check(serializer.Load(path,loaded,room,view,message), "Cannot reload panel");
    auto* changed = dynamic_cast<CPolyline*>(loaded.FindObjectById(curve_id));
    changed->SetPoint(0,{-10,40,3}); changed->SetPoint(1,{110,40,3});
    check(tools.ReplayProfileDependents(curve_id,loaded), "Panel curve dependency did not replay");
    const auto* panel = dynamic_cast<const CSolid*>(loaded.FindObjectById(panel_id));
    check(panel && std::abs(area(panel->m_Shape)-3456)<1.e-3, "Saved panel did not follow the changed contour");
    expect_z(panel->m_Shape,-5);
    const auto* surround = dynamic_cast<const CSolid*>(loaded.FindObjectById(surround_id));
    check(surround && std::abs(area(surround->m_Shape)-6000)<1.e-3,
          "Saved surrounding surface did not follow the changed contour");
    auto bulge_parameters=tools.Find("SurfaceBulge")->defaults;
    for (auto& p:bulge_parameters) {
        if (p.id=="surface.id") p.value=panel_id;
        if (p.id=="height") p.value=2;
    }
    auto active=tools.CreateParametricObject("SurfaceBulge",loaded,bulge_parameters);
    check(!active.tool_id.empty(),"Cannot create parametric bulge");
    const auto bulge_id=loaded.GetSelectedObject()->m_id;
    auto* bulge=dynamic_cast<CSolid*>(loaded.FindObjectById(bulge_id));
    const auto saved_shape=bulge->m_Shape;
    for (auto& p:active.parameters) if (p.id=="height") p.value=std::numeric_limits<double>::quiet_NaN();
    check(!tools.TryRebuildBulge(active,loaded) && bulge->m_Shape.IsSame(saved_shape),
        "Failed bulge modified the previous shape");
    auto shell_parameters=tools.Find("SolidShell")->defaults;
    for (auto& p:shell_parameters) {
        if (p.id=="surface.id") p.value=bulge_id;
        if (p.id=="distance") p.value=1;
    }
    tools.CreateParametricObject("SolidShell",loaded,shell_parameters);
    const auto shell_id=loaded.GetSelectedObject()->m_id;
    check(shell_id!=bulge_id,"Cannot create dependent Shell");
    check(serializer.Save(path,loaded,room,view,{},message),"Cannot save bulge chain");
    CAlfaDoc restored; check(serializer.Load(path,restored,room,view,message),"Cannot load bulge chain");
    auto* restored_curve=dynamic_cast<CPolyline*>(restored.FindObjectById(curve_id));
    restored_curve->SetPoint(0,{-10,45,3}); restored_curve->SetPoint(1,{110,45,3});
    const size_t count=restored.GetObjects().size();
    check(tools.ReplayProfileDependents(curve_id,restored),"Bulge chain did not replay");
    auto* restored_bulge=dynamic_cast<CSolid*>(restored.FindObjectById(bulge_id));
    auto* restored_shell=dynamic_cast<CSolid*>(restored.FindObjectById(shell_id));
    check(restored_bulge && restored_shell && restored.GetObjects().size()==count,
        "Bulge replay changed result identities");
    check(BRepCheck_Analyzer(restored_shell->m_Shape).IsValid(),"Replayed Shell is invalid");
    BRepExtrema_DistShapeShape moved_peak(BRepBuilderAPI_MakeVertex(gp_Pnt(50,22.5,-3)).Vertex(),restored_bulge->m_Shape);
    check(moved_peak.IsDone() && moved_peak.Value()<0.01,"Bulge did not follow the changed panel");
    {
        CAlfaDoc thickDoc;
        const TopoDS_Face base=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,100,0,100).Face();
        TopoDS_Shape baseShape=base;
        auto baseObject=std::make_unique<CSurfaceSet>(baseShape); check(baseObject->ReBuldMesh(),"Thick panel fixture mesh failed");
        thickDoc.AddObject(std::move(baseObject)); const auto baseId=thickDoc.GetSelectedObject()->m_id;
        auto cut=std::make_unique<CPolyline>();cut->AddPoint({-10,30,3});cut->AddPoint({110,30,3});
        thickDoc.AddObject(std::move(cut)); const auto cutId=thickDoc.GetSelectedObject()->m_id;
        auto thickParameters=tools.Find("SolidContourPanel")->defaults;
        for(auto& p:thickParameters) {
            if(p.id=="surface.id")p.value=baseId;if(p.id=="profile.id")p.value=cutId;
            if(p.id=="thickness")p.value=2;
        }
        const auto panelActive=tools.CreateParametricObject("SolidContourPanel",thickDoc,thickParameters);
        check(!panelActive.tool_id.empty(),"Thick panel creation failed");
        const auto thickId=thickDoc.GetSelectedObject()->m_id;
        auto* thick=dynamic_cast<CSolid*>(thickDoc.FindObjectById(thickId));
        check(thick && !dynamic_cast<CSurfaceSet*>(thick),"Thick panel is still a surface object");
        GProp_GProps volume;BRepGProp::VolumeProperties(thick->m_Shape,volume);
        check(std::abs(std::abs(volume.Mass())-6000)<.01,"Panel thickness produced wrong volume");
        const auto bulgeBody=[&](unsigned long sourceId) {
            auto* body=dynamic_cast<CSolid*>(thickDoc.FindObjectById(sourceId)); int faceIndex=-1,index=0;
            for(TopExp_Explorer f(body->m_Shape,TopAbs_FACE);f.More();f.Next(),++index) {
                BRepExtrema_DistShapeShape atTop(BRepBuilderAPI_MakeVertex(gp_Pnt(50,15,0)).Vertex(),f.Current());
                if(atTop.IsDone() && atTop.Value()<1.e-5 && area(f.Current())>1000) {faceIndex=index;break;}
            }
            check(faceIndex>=0,"Panel outer face not found");
            auto params=tools.Find("SurfaceBulge")->defaults;
            for(auto& p:params) {if(p.id=="surface.id")p.value=sourceId;if(p.id=="face.index")p.value=faceIndex;if(p.id=="height")p.value=2;}
            auto created=tools.CreateParametricObject("SurfaceBulge",thickDoc,params);
            check(!created.tool_id.empty(),"Bulge on solid failed");
            auto* result=dynamic_cast<CSolid*>(thickDoc.GetSelectedObject());
            check(result && !dynamic_cast<CSurfaceSet*>(result) && TopExp_Explorer(result->m_Shape,TopAbs_SOLID).More()
                && BRepCheck_Analyzer(result->m_Shape).IsValid(),"Bulge lost closed solid");
            return result->m_id;
        };
        const auto thickBulgeId=bulgeBody(thickId);
        auto shellParams=tools.Find("SolidShell")->defaults;
        for(auto& p:shellParams) {if(p.id=="surface.id")p.value=baseId;if(p.id=="distance")p.value=-2;}
        tools.CreateParametricObject("SolidShell",thickDoc,shellParams);
        bulgeBody(thickDoc.GetSelectedObject()->m_id);
        check(serializer.Save(directory.filePath("thick-panel.dom3d"),thickDoc,"Solid",view,{},message),message.toStdString());
        CAlfaDoc reopened;
        check(serializer.Load(directory.filePath("thick-panel.dom3d"),reopened,room,view,message),message.toStdString());
        tools.ReplayProfileDependents(cutId,reopened);
        auto* solidBulge=dynamic_cast<CSolid*>(reopened.FindObjectById(thickBulgeId));
        check(solidBulge && TopExp_Explorer(solidBulge->m_Shape,TopAbs_SOLID).More()
            && BRepCheck_Analyzer(solidBulge->m_Shape).IsValid(),"Reload/replay lost thick bulge body");
    }
    std::cout << "Panel contour, bulge, Shell, fillet and saved replay tests passed.\n";
}
