#include "ui/MainWindow.h"
#include "solid/Solid.h"
#include "solid/SolidCylinderTool.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include <QCheckBox>
#include "solid/SurfaceFace.h"
#include "solid/SurfaceSet.h"
#include <QPushButton>
#include "solid/QuadroBodyMesher.h"
#include "CMesh3D.h"
#include "SmartLine.h"
#include "CPolyline.h"
#include "CBSpline.h"
#include "SweptSolidBuilder.h"
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include "Dom3DProjectSerializer.h"
#include "SketchProfileBuilder.h"
#include "Sketch.h"
#include "solid/TrimShapeBuilder.h"

#include <QAction>
#include <QMenu>
#include <QTreeWidget>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QSurfaceFormat>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <array>
#include <map>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
double parameter(const std::vector<ToolParameter>& parameters, const char* id) {
    for (const auto& p : parameters) if (p.id == id) return p.value;
    require(false, id);
    return 0;
}
void click(OpenGLViewport& viewport, Vec3 world, bool finish_height = false) {
    DomPoint position;
    QtSceneRenderer renderer;
    require(renderer.WorldToScreen(world, viewport.GetCamera(), viewport.IsOrthographicProjection(),
        viewport.width(), viewport.height(), position), "Cannot project test point");
    const QPointF point(position.x, position.y);
    QMouseEvent press(QEvent::MouseButtonPress, point, point,
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, point, point,
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &release);
    if (finish_height) {
        const QPointF height_point = point + QPointF(0, -80);
        QMouseEvent move(QEvent::MouseMove, height_point, height_point,
            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&viewport, &move);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&viewport, &enter);
    }
}
}

int TestSolidPrimitiveTool(int argc, char** argv) {
    QApplication app(argc, argv);
    if (app.arguments().contains("--work-plane-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        app.processEvents();
        auto* view = window.viewport_;
        window.RefreshSceneTree();
        auto* root = window.scene_tree_->topLevelItem(0);
        require(root->text(1) == "Origin" && root->childCount() == 3, "Origin must contain exactly three planes");
        view->SetTool(ToolMode::Select);
        view->SetOriginPlaneVisible(0, true);
        const auto polygon = view->ReferencePlanePolygon(0);
        require(!polygon.isEmpty(), "Origin plane is not visible in the scene");
        QPointF center;
        for (const auto& point : polygon) center += point;
        center /= polygon.size();
        QMouseEvent press(QEvent::MouseButtonPress, center, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, center, center, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(view, &press);
        QApplication::sendEvent(view, &release);
        require(view->SelectedOriginPlane() == 0, "Scene click did not select origin plane");
        for (int plane = 0; plane < 3; ++plane) {
            root = window.scene_tree_->topLevelItem(0);
            window.OnSceneTreeItemClicked(root->child(plane), 1);
            require(view->SelectedOriginPlane() == plane && view->IsOriginPlaneVisible(plane), "Tree plane selection failed");
            QMenu menu;
            window.AddWorkPlaneAction(menu);
            require(menu.actions().size() == 1 && menu.actions()[0]->text() == "Work Plane", "Missing Work Plane action");
            menu.actions()[0]->trigger();
            const Vec3 normal = plane == 0 ? Vec3{0,0,1} : plane == 1 ? Vec3{0,1,0} : Vec3{1,0,0};
            require(view->IsWorkPlane({}, normal), "Origin work plane not applied");
            QMenu disable_menu;
            window.AddWorkPlaneAction(disable_menu);
            require(disable_menu.actions()[0]->isChecked(), "Active origin plane must be checked");
            disable_menu.actions()[0]->trigger();
            require(!view->IsWorkPlane({}, normal), "Origin work plane was not cleared");
            require(!view->IsSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane), "Clearing origin plane must disable snapping");
            require(window.work_origin_plane_ == -1 && window.work_plane_object_id_ == 0, "Work plane tree marker was not cleared");
            require(window.scene_tree_->topLevelItem(0)->child(plane)->text(2) == "Reference Plane", "Tree still labels cleared origin as Work Plane");
            QMenu enable_menu;
            window.AddWorkPlaneAction(enable_menu);
            require(!enable_menu.actions()[0]->isChecked(), "Cleared origin plane remains checked in menu");
            enable_menu.actions()[0]->trigger();
            require(view->IsWorkPlane({}, normal), "Cannot re-enable origin work plane");
        }
        ToolRegistry registry;
        auto parameters = registry.Find("PlaneTool")->defaults;
        for (auto& p : parameters) {
            if (p.id == "mode") p.value = 0;
            if (p.id == "a") p.value = 1;
            if (p.id == "b") p.value = 2;
            if (p.id == "c") p.value = 3;
            if (p.id == "d") p.value = -40;
        }
        const auto plane = registry.CreateParametricObject("PlaneTool", window.document_, parameters);
        require(plane.object_index < window.document_.GetObjects().size(), "Cannot create reference plane");
        auto* object = window.document_.GetObjects()[plane.object_index].get();
        window.document_.SelectObjectById(object->m_id, SelectionAction::Replace);
        Vec3 origin, normal;
        require(window.document_.GetObjectPlane(object->m_id, origin, normal), "Cannot read reference plane");
        QMenu menu;
        window.AddWorkPlaneAction(menu);
        require(menu.actions().size() == 1, "Reference plane lacks Work Plane command");
        menu.actions()[0]->trigger();
        require(view->IsWorkPlane(origin, normal), "Reference work plane not applied");
        require(view->IsSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane), "Work Plane must enable snapping");
        const unsigned long reference_id = object->m_id;
        require(window.document_.RotateSelectedObjects({}, {1,0,0}, 0.6f), "Cannot rotate work plane");
        window.RefreshSceneTree();
        require(window.document_.GetObjectPlane(reference_id, origin, normal), "Cannot read rotated plane");
        require(view->IsWorkPlane(origin, normal), "Work plane did not follow rotation");
        require(std::abs(dot(view->sketch_normal_, normal)) > 0.999f, "Rotated drawing frame is stale");
        require(window.document_.MoveSelectedObjects({17,-23,41}), "Cannot move work plane");
        // Tool entry also resolves geometry without depending on a tree refresh.
        view->SetTool(ToolMode::DrawBSpline);
        require(window.document_.GetObjectPlane(reference_id, origin, normal), "Cannot read moved plane");
        require(view->IsWorkPlane(origin, normal), "Work plane did not follow translation");
        require(std::abs(dot(view->sketch_origin_ - origin, normal)) < 0.001f, "Translated drawing frame is stale");
        view->SetSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane, false);
        require(window.document_.MoveSelectedObjects({0,0,10}), "Cannot move plane with snapping off");
        window.RefreshSceneTree();
        require(!view->IsSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane), "Refreshing work plane re-enabled snapping");
        require(window.document_.GetObjectPlane(reference_id, origin, normal), "Cannot read final plane");
        view->SetSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane, true);
        view->BeginSolidBoxRectangle(OpenGLViewport::SketchPlane::XY);
        view->SetTool(ToolMode::DrawCurve);
        require(std::abs(dot(view->sketch_normal_, normal)) > 0.999f, "Work plane lost after switching tools");
        Camera work_plane_camera = view->GetCamera();
        work_plane_camera.target = origin;
        work_plane_camera.distance = 500;
        work_plane_camera.orientation = camera_orientation_from_yaw_pitch(35, 30);
        view->SetCamera(work_plane_camera);
        CPoint3d hit;
        require(view->ScreenToSketchPlane(QPoint(view->width()/2, view->height()/2), hit), "Cannot intersect work plane");
        require(std::abs(dot(Vec3{float(hit.x),float(hit.y),float(hit.z)} - origin, normal)) < 0.01f, "Point is outside work plane");
        view->SetTool(ToolMode::Select);
        QMenu disable_reference_menu;
        window.AddWorkPlaneAction(disable_reference_menu);
        require(disable_reference_menu.actions().size() == 1 && disable_reference_menu.actions()[0]->isChecked(), "Reference plane must be checked before disabling");
        disable_reference_menu.actions()[0]->trigger();
        require(!view->IsWorkPlane(origin, normal), "Reference work plane was not cleared");
        require(!view->IsSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane), "Clearing reference plane must disable snapping");
        require(window.work_origin_plane_ == -1 && window.work_plane_object_id_ == 0, "Reference plane tree marker was not cleared");
        view->SetTool(ToolMode::DrawBSpline);
        require(!view->work_plane_assigned_ && !view->IsSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane), "Spline tool restored disabled work plane");
        QMenu enable_reference_menu;
        window.AddWorkPlaneAction(enable_reference_menu);
        require(!enable_reference_menu.actions()[0]->isChecked(), "Cleared reference plane remains checked in menu");
        enable_reference_menu.actions()[0]->trigger();
        require(view->IsWorkPlane(origin, normal), "Cannot re-enable reference work plane");
        view->SelectOriginPlane(0);
        window.RefreshSceneTree();
        app.processEvents();
        view->grabFramebuffer().save("C:/My_projects/Dom3D_Pro/output/work-plane-scene.png");
        window.grab().save("C:/My_projects/Dom3D_Pro/output/work-plane-tree.png");
        std::cout << "Origin and Reference Plane work-plane checks passed.\n";
        return 0;
    }
    if (app.arguments().contains("--face-drag-cut-only")) {
        QDir().mkpath("output/face-drag-cut");
        auto volume = [](const TopoDS_Shape& shape) {
            GProp_GProps properties; BRepGProp::VolumeProperties(shape, properties);
            return properties.Mass();
        };
        for (bool cylinder : {false,true}) {
            CAlfaDoc doc;
            doc.GetObjects().clear();
            TopoDS_Shape shape = BRepPrimAPI_MakeBox(30,30,10).Shape();
            auto body = std::make_unique<CSolid>(shape);
            require(body->ReBuldMesh(), "Cannot mesh face-drag fixture");
            auto* target = body.get();
            doc.AddObject(std::move(body),false);
            doc.EnsureObjectId(*target);
            OpenGLViewport view;
            view.SetDocument(&doc); view.resize(1000,800); view.move(-30000,-30000);
            Camera camera; camera.target={15,15,5}; camera.distance=100;
            camera.orientation=camera_orientation_from_yaw_pitch(35,30);
            view.SetCamera(camera); view.SetOrthographicProjection(true);
            view.snapping_enabled_=false;
            view.SetFloorGridVisible(false); view.SetCoordinateAxesVisible(false);
            CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
            CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceGray);
            view.show(); app.processEvents();
            int finished=0;
            auto finish=[&](const std::vector<ToolParameter>& parameters) {
                ++finished;
                require(parameter(parameters,"boolean.body_id")==target->m_id
                        && parameter(parameters,cylinder?"height":"depth")<0,
                        "Confirmation lost the cut target or signed height");
            };
            QObject::connect(&view,&OpenGLViewport::SolidBoxRectangleFinished,finish);
            QObject::connect(&view,&OpenGLViewport::SolidCylinderCircleFinished,finish);
            auto begin=[&] {
                if(cylinder) view.BeginSolidCylinderFaceSelection();
                else view.BeginSolidBoxFaceSelection();
                view.SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY,true);
                click(view,{15,15,10});
                click(view,cylinder?Vec3{15,15,10}:Vec3{10,10,10});
                click(view,cylinder?Vec3{20,15,10}:Vec3{20,20,10});
                require(view.primitive_height_active_,"Base did not enter height preview");
                require(view.solid_box_target_body_id_ == target->m_id && target->m_id != 0,
                        "Face gesture did not retain its target body");
            };
            auto move=[&](float height) {
                const auto anchor=view.sketch_rectangle_preview_point_;
                const auto n=view.sketch_normal_;
                QPoint pixel;
                require(view.ProjectWorldPoint({float(anchor.x)+n.x*height,
                    float(anchor.y)+n.y*height,float(anchor.z)+n.z*height},pixel),"Cannot project height gesture");
                QMouseEvent event(QEvent::MouseMove,QPointF(pixel),QPointF(pixel),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(&view,&event);
                require(target->m_Shape.IsSame(shape) && target->IsVisible() && doc.GetObjects().size()==1,
                        "Dragging must not mutate or hide the document body");
            };
            begin(); move(-0.5f);
            require(view.primitive_preview_solid_ && finished==0,"Cut must appear before confirmation");
            const double shallow=volume(view.primitive_preview_solid_->m_Shape);
            std::cout << (cylinder?"Cylinder":"Box") << " drag height=" << view.primitive_height_
                      << " preview volume=" << shallow << " target=" << view.solid_box_target_body_id_ << std::endl;
            require(shallow<8999 && shallow>4500,"Shallow preview must be the cut body, not the cutter");
            move(-4);
            require(view.primitive_preview_solid_
                    && volume(view.primitive_preview_solid_->m_Shape)<shallow-100,
                    "Cut must deepen during mouse movement");
            app.processEvents();
            require(view.grabFramebuffer().save(cylinder?"output/face-drag-cut/cylinder.png":"output/face-drag-cut/box.png"),
                    "Cannot capture live cut preview");
            require(target->IsVisible(),"Rendering left target hidden");
            move(4);
            require(view.primitive_preview_solid_ && volume(view.primitive_preview_solid_->m_Shape)<1000,
                    "Moving back above face must restore positive primitive preview");
            move(-4);
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(&view,&escape);
            require(!view.primitive_preview_solid_ && finished==0 && target->m_Shape.IsSame(shape)
                    && target->IsVisible(),"Escape did not restore original body");
            begin(); move(-4);
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
            QApplication::sendEvent(&view,&enter);
            require(finished==1 && !view.primitive_preview_solid_,"Confirmation retained transient preview");
            view.SetDocument(nullptr);
        }
        std::cout << "Face-based Box/Cylinder cuts preview during height movement" << std::endl;
        return 0;
    }
    if(app.arguments().contains("--surface-swept-only")) {
        MainWindow window;auto& doc=window.document_;doc.GetObjects().clear();
        auto profile=std::make_unique<CBSpline>();
        profile->AddPoint({-10,0,0});profile->AddPoint({0,5,0});profile->AddPoint({10,0,0});
        auto guide=std::make_unique<CBSpline>();
        guide->AddPoint({0,0,0});guide->AddPoint({0,0,40});guide->AddPoint({20,0,90});guide->AddPoint({40,0,140});
        auto* profilePtr=profile.get();auto* guidePtr=guide.get();
        doc.AddObject(std::move(profile),false);doc.AddObject(std::move(guide),false);
        doc.EnsureObjectId(*profilePtr);doc.EnsureObjectId(*guidePtr);
        require(!SweepGuideHasKinks(*guidePtr),"Smooth spline incorrectly classified as sharp");
        CBSpline straightGuide;straightGuide.AddPoint({0,0,0});straightGuide.AddPoint({0,0,100});
        const auto terminalVector=[&](const TopoDS_Shape& shape) {
            require(!shape.IsNull()&&BRepCheck_Analyzer(shape).IsValid(),"Twisted/scaled sweep failed");
            std::vector<gp_Pnt> points;
            for(TopExp_Explorer it(shape,TopAbs_VERTEX);it.More();it.Next()) {
                const auto p=BRep_Tool::Pnt(TopoDS::Vertex(it.Current()));
                if(std::abs(p.Z()-100)<1e-4&&std::none_of(points.begin(),points.end(),[&](const auto& q){return p.Distance(q)<1e-5;}))points.push_back(p);
            }
            require(points.size()==2,"Cannot identify final profile endpoints");
            auto delta=gp_Vec(points[0],points[1]);if(delta.X()<0)delta.Reverse();return delta;
        };
        const auto fixed=terminalVector(BuildSweptSurfaceShape(*profilePtr,straightGuide,1,0,0,0,{}, {},false,0,1));
        require(std::abs(fixed.X()-20)<1e-4&&std::abs(fixed.Y())<1e-4,"Auto Orientation off did not preserve the profile direction");
        const auto twisted=terminalVector(BuildSweptSurfaceShape(*profilePtr,straightGuide,1,0,0,0,{}, {},false,60,0.4));
        require(std::abs(twisted.Magnitude()-8)<1e-4&&std::abs(twisted.X()-4)<1e-4
            &&std::abs(twisted.Y()-std::sqrt(48.0))<1e-4,"Twist or final scale does not match the requested value");
        CBSpline remoteProfile;remoteProfile.AddPoint({100,30,20});remoteProfile.AddPoint({110,35,20});remoteProfile.AddPoint({120,30,20});
        const auto authored=BuildSweptSurfaceShape(remoteProfile,straightGuide,1,10,15,40,{}, {},false,0,1);
        require(!authored.IsNull(),"Cannot sweep the profile from its authored location");
        for(double z:{20.0,120.0})for(double x:{100.0,120.0}) {
            BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(gp_Pnt(x,30,z)).Shape(),authored);
            require(distance.IsDone()&&distance.Value()<1e-6,"Auto Orientation off moved/rotated the source profile or applied disabled offsets");
        }
        CBSpline sharpSpline;sharpSpline.AddPoint({0,0,0});sharpSpline.AddPoint({0,40,0});sharpSpline.AddPoint({40,40,0});sharpSpline.SetDegree(1);
        require(SweepGuideHasKinks(sharpSpline),"Internal spline knot corner was not detected");
        auto scaled=BuildSweptSurfaceShape(*profilePtr,*guidePtr,1,0,0,0,{1,1.2,1.5,1.2,1},{});
        require(!scaled.IsNull()&&BRepCheck_Analyzer(scaled).IsValid()
            &&!TopExp_Explorer(scaled,TopAbs_SOLID).More(),"Surface scale graph failed or created caps");
        doc.SelectObjectById(profilePtr->m_id,SelectionAction::Replace);
        window.ActivateParametricTool("SurfaceSweptTool");
        require(window.pending_sweep_section_id_==profilePtr->m_id,"Surface sweep did not preserve the selected profile");
        window.pending_sweep_section_id_=0;
        doc.SelectObjectById(profilePtr->m_id,SelectionAction::Replace);
        doc.SelectObjectById(guidePtr->m_id,SelectionAction::Add);
        window.undo_redo_.Reset();window.ActivateParametricTool("SurfaceSweptTool");
        auto* surface=dynamic_cast<CSurfaceSet*>(doc.GetSelectedObject());
        require(surface&&doc.GetObjects().size()==3,"Surface sweep did not create a surface");
        const auto surfaceId=surface->m_id;
        require(BRepCheck_Analyzer(surface->m_Shape).IsValid(),"Invalid sweep surface");
        require(!TopExp_Explorer(surface->m_Shape,TopAbs_SOLID).More(),"Surface sweep unexpectedly capped the profile");
        require(window.property_panel_->findChild<QPushButton*>("parameter_width.graph")->isEnabled(),"Smooth guide graph disabled");
        require(!window.property_panel_->findChild<QComboBox*>("parameter_transition"),"Surface Corner parameter is still visible");
        window.property_panel_->findChild<QDoubleSpinBox*>("parameter_twist_angle")->setValue(60);
        window.property_panel_->findChild<QDoubleSpinBox*>("parameter_end_scale")->setValue(0.4);
        window.property_panel_->findChild<QCheckBox*>("parameter_auto_orientation")->setChecked(false);
        for(const auto* id:{"angle","dx","dy"})require(!window.property_panel_->findChild<QDoubleSpinBox*>(QString("parameter_%1").arg(id))->isEnabled(),
            "Authored sweep placement fields remain enabled");
        window.property_panel_->findChild<QCheckBox*>("parameter_auto_orientation")->setChecked(true);
        for(const auto* id:{"angle","dx","dy"})require(window.property_panel_->findChild<QDoubleSpinBox*>(QString("parameter_%1").arg(id))->isEnabled(),
            "Automatic sweep placement fields did not become enabled");
        window.property_panel_->findChild<QCheckBox*>("parameter_auto_orientation")->setChecked(false);
        window.property_panel_->findChild<QDoubleSpinBox*>("parameter_dx")->setValue(3);
        window.AcceptActiveProperties();window.UndoDocumentChange();
        require(doc.GetObjects().size()==2,"Surface sweep Undo did not preserve only the source curves");
        window.RedoDocumentChange();require(doc.FindObjectById(surfaceId),"Surface sweep Redo failed");
        QTemporaryDir temp;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        require(serializer.Save(temp.filePath("sweep.dom3d"),doc,"Surface",view,{},error),"Cannot save sweep surface");
        CAlfaDoc loaded;require(serializer.Load(temp.filePath("sweep.dom3d"),loaded,room,view,error),"Cannot reload sweep surface");
        ToolRegistry tools;require(tools.ReplayOperations(loaded.FindObjectIndexById(surfaceId),loaded),"Surface sweep history replay failed");
        require(dynamic_cast<CSurfaceSet*>(loaded.FindObjectById(surfaceId)),"Replay changed surface into a solid");
        auto saved=tools.ActiveObjectFromDocument(loaded.FindObjectIndexById(surfaceId),*loaded.FindObjectById(surfaceId),0,&loaded);
        require(parameter(saved.parameters,"auto_orientation")==0&&parameter(saved.parameters,"twist_angle")==60&&std::abs(parameter(saved.parameters,"end_scale")-0.4)<1e-6,
            "Sweep twist and scale were not preserved");
        window.property_panel_->SetActiveObject(saved);
        require(!window.property_panel_->findChild<QDoubleSpinBox*>("parameter_dx")->isEnabled(),"Reopened authored sweep placement is not disabled");
        const auto output=app.arguments().value(app.arguments().indexOf("--surface-swept-only")+1);
        if(!output.isEmpty())require(serializer.Save(output,doc,"Surface",view,{},error),"Cannot save sweep example");
        require(doc.CreateSketchPolyline({{0,0,0},{0,40,0},{40,40,0}},false,"Sharp guide",{},{1,0,0},{0,1,0}),"Cannot create sharp guide");
        auto* sharp=doc.GetSelectedObject();require(SweepGuideHasKinks(*sharp),"Guide corner was not detected");
        auto shape=BRepPrimAPI_MakeBox(10,10,10).Shape();CSolid metadata(shape);
        metadata.SetParametricOperation(0,"SolidSweptTool","Swept",{{"guide.id",double(sharp->m_id)}});
        auto active=tools.ActiveObjectFromDocument(0,metadata,0,&doc);window.property_panel_->SetActiveObject(active);
        require(!window.property_panel_->findChild<QPushButton*>("parameter_width.graph")->isEnabled()
            &&!window.property_panel_->findChild<QPushButton*>("parameter_height.graph")->isEnabled(),"Sharp guide scale graphs remain enabled");
        require(doc.CreateSketchPolyline({{0,0,0},{0,40,0},{0,80,0}},false,"Straight guide",{},{1,0,0},{0,1,0}),"Cannot create straight guide");
        require(!SweepGuideHasKinks(*doc.GetSelectedObject()),"Collinear guide junction incorrectly classified as sharp");
        return 0;
    }
    if (app.arguments().contains("--bent-trim-only")) {
        const int arg=app.arguments().indexOf("--bent-trim-only");
        CAlfaDoc doc;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        require(serializer.Load(app.arguments().value(arg+1),doc,room,view,error),"Cannot load bent trim fixture");
        auto* body=dynamic_cast<CSolid*>(doc.FindObjectById(10));
        require(body,"Missing bent trim body");
        GProp_GProps original;BRepGProp::VolumeProperties(body->m_Shape,original);
        doc.SelectObjectById(10,SelectionAction::Replace);doc.SelectObjectById(19,SelectionAction::Add);
        ToolRegistry tools;auto active=tools.ApplyTrimToSelection("TrimBySketch",doc);
        require(!active.tool_id.empty(),"Bent open sketch trim failed to start");
        double sum=0;
        for(int direction:{0,1}) {
            for(auto& p:active.parameters)if(p.id=="direction")p.value=direction;
            tools.Rebuild(active,doc);
            body=dynamic_cast<CSolid*>(doc.FindObjectById(10));
            require(BRepCheck_Analyzer(body->m_Shape).IsValid(),"Invalid bent trim result");
            GProp_GProps mass;BRepGProp::VolumeProperties(body->m_Shape,mass);sum+=mass.Mass();
            require(mass.Mass()>0&&mass.Mass()<original.Mass(),"Bent trim did not remove material");
            BRepClass3d_SolidClassifier top(body->m_Shape,gp_Pnt(320,230,-10),1e-6);
            BRepClass3d_SolidClassifier notch(body->m_Shape,gp_Pnt(320,120,-10),1e-6);
            require(top.State()==(direction==0?TopAbs_IN:TopAbs_OUT),"Wrong retained upper region");
            require(notch.State()==(direction==0?TopAbs_OUT:TopAbs_IN),"Wrong retained notch region");
            std::cout<<"Bent trim direction="<<direction<<" volume="<<mass.Mass()<<std::endl;
        }
        require(std::abs(sum-original.Mass())<0.01,"Bent trim did not partition the body");
        for(auto& p:active.parameters)if(p.id=="direction")p.value=0;
        tools.Rebuild(active,doc);
        const auto output=app.arguments().value(arg+2);
        if(!output.isEmpty())require(serializer.Save(output,doc,room,view,{},error),"Cannot save bent trim result");
        return 0;
    }
    if (app.arguments().contains("--open-trim-only")) {
        MainWindow window;
        auto& doc=window.document_;
        doc.GetObjects().clear();
        auto shape=BRepPrimAPI_MakeBox(gp_Pnt(-10,-10,-10),20,20,20).Shape();
        auto body=std::make_unique<CSolid>(shape);
        auto* solid=body.get();
        doc.AddObject(std::move(body),false);
        doc.EnsureObjectId(*solid);
        const auto bodyId=solid->m_id;
        require(doc.CreateSketchPolyline({{-20,0,0},{20,0,0}},false,"Open cutter",{},{1,0,0},{0,1,0}),
            "Cannot create open trim sketch");
        const auto cutterId=doc.GetSelectedObject()->m_id;
        window.ActivateParametricTool("TrimBySketch");
        require(window.pending_trim_cutter_id_==cutterId,"Open sketch was not accepted as the trim cutter");
        require(doc.SelectObjectById(bodyId,SelectionAction::Replace),"Cannot select trim body");
        require(doc.SelectObjectById(cutterId,SelectionAction::Add),"Cannot select trim cutter");
        require(window.TryApplyPendingTrim(),"Open sketch trim could not start");
        auto* direction=window.property_panel_->findChild<QComboBox*>("parameter_direction");
        require(direction,"Trim direction control missing");
        for(int side:{0,1}) {
            direction->setCurrentIndex(side);
            auto* trimmed=dynamic_cast<CSolid*>(doc.FindObjectById(bodyId));
            require(trimmed&&BRepCheck_Analyzer(trimmed->m_Shape).IsValid(),"Invalid open trim result");
            GProp_GProps mass;BRepGProp::VolumeProperties(trimmed->m_Shape,mass);
            require(std::abs(mass.Mass()-4000)<1e-5,"Open trim did not halve the body");
            require(std::abs(mass.CentreOfMass().Y()-(side==0?5:-5))<1e-5,"Direction did not switch the retained side");
        }
        window.CancelActiveProperties();
        GProp_GProps restored;BRepGProp::VolumeProperties(dynamic_cast<CSolid*>(doc.FindObjectById(bodyId))->m_Shape,restored);
        require(std::abs(restored.Mass()-8000)<1e-5,"Cancel did not restore the untrimmed body");
        return 0;
    }
    if (app.arguments().contains("--trim-sketch-only")) {
        const int arg=app.arguments().indexOf("--trim-sketch-only");
        CAlfaDoc doc;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        require(serializer.Load(app.arguments().value(arg+1),doc,room,view,error),"Cannot load trim fixture");
        ToolRegistry tools;
        const auto volume=[](const TopoDS_Shape& shape){GProp_GProps g;BRepGProp::VolumeProperties(shape,g,1e-10);return g.Mass();};
        for(auto ids:{std::pair<unsigned long,unsigned long>{8,9},{10,11},{12,13},{16,13}}) {
            auto* body=dynamic_cast<CSolid*>(doc.FindObjectById(ids.first));
            auto* sketch=dynamic_cast<CSmartLine*>(doc.FindObjectById(ids.second));
            require(body&&sketch,"Missing trim body/sketch");
            const auto trimParameters=body->GetOperation(1)->Parameters;
            require(body->RemoveParametricOperation(1),"Cannot remove old trim");
            require(tools.ReplayOperations(doc.FindObjectIndexById(ids.first),doc),"Cannot rebuild untrimmed body");
            body=dynamic_cast<CSolid*>(doc.FindObjectById(ids.first));
            TopoDS_Face profile;Vec3 normal;
            require(BuildSketchProfileFace(*sketch,profile,normal),"Cannot build trim profile");
            const auto n=sketch->GetCoordinateSystem().normal;
            double sum=0;
            std::cout<<"Body "<<ids.first<<" volume="<<volume(body->m_Shape)<<std::endl;
            for(bool inside:{true,false}) {
                TopoDS_Shape result;
                require(TrimSolidByClosedProfile(body->m_Shape,profile,{float(n.x),float(n.y),float(n.z)},inside,result),"Trim failed");
                const double v=volume(result);sum+=v;
                std::cout<<" inside="<<inside<<" volume="<<v<<std::endl;
                require(v>0&&v<volume(body->m_Shape),"Trim added material or inverted the solid");
            }
            std::cout<<" partition error="<<std::abs(sum-volume(body->m_Shape))<<std::endl;
            // Boolean curve tolerances contribute a small volume error on
            // the revolved arc; use a scale-relative budget (one part in 1e8).
            require(std::abs(sum-volume(body->m_Shape))<std::max(0.001,volume(body->m_Shape)*1e-8),
                "Trim does not partition the original body");
            body->SetParametricOperation(1,"TrimBySketch","Trim By Sketch",trimParameters);
            require(tools.ReplayOperations(doc.FindObjectIndexById(ids.first),doc),"Cannot replay complete trim history");
            require(volume(dynamic_cast<CSolid*>(doc.FindObjectById(ids.first))->m_Shape)>0,
                "Trim history produced an inverted result");
        }
        const auto output=app.arguments().value(arg+2);
        if(!output.isEmpty())require(serializer.Save(output,doc,room,view,{},error),"Cannot save corrected trim project");
        return 0;
    }
    if (app.arguments().contains("--pipe-spline-only")) {
        const int argument = app.arguments().indexOf("--pipe-spline-only");
        CAlfaDoc doc;
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room, error;
        require(serializer.Load(app.arguments().value(argument+1),doc,room,view,error), "Cannot load Pipe fixture");
        auto* spline = dynamic_cast<CBSpline*>(doc.FindObjectById(2));
        require(spline, "Pipe spline missing");
        auto spine = BuildSolidCenterPath(*spline);
        TopExp_Explorer edge(spine,TopAbs_EDGE);
        require(edge.More(), "Pipe spine missing");
        BRepAdaptor_Curve guide(TopoDS::Edge(edge.Current()));
        double deviation = 0;
        for (int i=0;i<=100;++i) {
            const float t = float(i)/100;
            const auto p = spline->Evaluate(t);
            deviation = std::max(deviation, guide.Value(guide.FirstParameter()+t*(guide.LastParameter()-guide.FirstParameter()))
                .Distance(gp_Pnt(p.x,p.y,p.z)));
        }
        std::cout << "Spine deviation=" << deviation << std::endl;
        require(deviation < 0.001, "Wire spine does not preserve the source B-spline");
        ToolRegistry tools;
        require(tools.ReplayOperations(doc.FindObjectIndexById(3),doc), "Cannot rebuild Pipe");
        auto* pipe = dynamic_cast<CSolid*>(doc.FindObjectById(3));
        require(pipe && BRepCheck_Analyzer(pipe->m_Shape).IsValid(), "Invalid rebuilt pipe");
        std::vector<std::pair<double,TopoDS_Face>> sides;
        for(TopExp_Explorer face(pipe->m_Shape,TopAbs_FACE);face.More();face.Next()) {
            auto f = TopoDS::Face(face.Current());
            if(BRepAdaptor_Surface(f).GetType()==GeomAbs_Plane)continue;
            GProp_GProps area; BRepGProp::SurfaceProperties(f,area);
            sides.push_back({area.Mass(),f});
        }
        std::sort(sides.begin(),sides.end(),[](const auto& a,const auto& b){return a.first>b.first;});
        require(sides.size()==2,"Pipe lost its inner/outer walls");
        double error_radius = 0;
        for(double t:{.5,.7,.8,.85,.9,.95,.98})for(size_t side=0;side<sides.size();++side) {
            auto point=BRepBuilderAPI_MakeVertex(guide.Value(t)).Shape();
            BRepExtrema_DistShapeShape distance(point,sides[side].second);
            require(distance.IsDone(),"Cannot measure pipe radius");
            error_radius=std::max(error_radius,std::abs(distance.Value()-(34.356-2*side)));
        }
        std::cout<<"Maximum wall radius error="<<error_radius<<std::endl;
        require(error_radius<0.01,"Pipe wall develops bulges near its end");
        // Check actual exported topology, not merely the mesher's success flag.
        // Exact coordinate keys also catch cap/wall cracks hidden by loose weld
        // tolerances. Every edge of this annular solid must have two opposite
        // uses, including the periodic seam.
        for(bool quadro:{false,true})for(float density:{0.25f,0.9f,2.0f}) {
            pipe->MeshQuadro=quadro;
            require(pipe->ReBuldMesh(1/density),"Cannot rebuild tube mesh");
            std::map<std::array<float,3>,int> vertices;
            std::map<std::pair<int,int>,std::pair<int,int>> edges;
            int faces=0;double maxEdge=0;
            for(int s=0;s<pipe->GetNumSurfaces();++s) {
                const auto* mesh=pipe->GetSurfaceFace(s)->pMesh3D;
                for(const auto& face:mesh->GetFaces()) {
                    require(face.corners.size()==4,"Tube mesh contains non-quads");++faces;
                    for(int k=0;k<4;++k) {
                        const auto a=mesh->GetVertices()[face.corners[k].v];
                        const auto b=mesh->GetVertices()[face.corners[(k+1)%4].v];
                        const auto index=[&](Vec3 p){return vertices.emplace(std::array<float,3>{p.x,p.y,p.z},int(vertices.size())).first->second;};
                        const int ia=index(a),ib=index(b);
                        auto& edge=edges[std::minmax(ia,ib)];++edge.first;edge.second+=ia<ib?1:-1;
                        const double length=std::sqrt(double(dot(b-a,b-a)));maxEdge=std::max(maxEdge,length);
                        require(length>1e-5,"Collapsed tube edge");
                    }
                }
            }
            for(const auto& edge:edges)require(edge.second.first==2&&edge.second.second==0,
                "Tube has a crack, nonmanifold edge or inverted face");
            require(int(vertices.size())-int(edges.size())+faces==0,"Tube bore topology changed");
            require(maxEdge<40,"Tube mesh has a stretched seam cell");
            std::cout<<"Tube mesh quadro="<<quadro<<" density="<<density<<" quads="<<faces<<" maxEdge="<<maxEdge<<std::endl;
        }
        pipe->MeshQuadro=true;
        require(pipe->ReBuldMesh(1/0.9f),"Cannot prepare final tube mesh");
        const auto output=app.arguments().value(argument+2);
        if(!output.isEmpty())require(serializer.Save(output,doc,room,view,{},error),"Cannot save rebuilt Pipe");
        return 0;
    }
    if (app.arguments().contains("--wire-thick-only")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        CPolyline path;
        path.AddPoint({0,0,0}); path.AddPoint({0,0,100});
        const auto volume = [](const TopoDS_Shape& shape) {
            GProp_GProps properties; BRepGProp::VolumeProperties(shape, properties); return properties.Mass();
        };
        for (double thick : {0.0, 2.0, 9.0}) {
            auto shape = BuildWireSolidShape(path, 10, nullptr, thick);
            require(!shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(), "Wire/tube geometry failed");
            const double expected = std::acos(-1.0) * 100 * (100 - (thick == 0 ? 0 : std::pow(10-thick,2)));
            require(std::abs(volume(shape)-expected) < 1e-5, "Wrong tube wall volume");
            BRepClass3d_SolidClassifier center(shape, gp_Pnt(0,0,50), 1e-6);
            require(center.State() == (thick == 0 ? TopAbs_IN : TopAbs_OUT), "Tube bore is not empty");
            if (thick > 0) for (double z : {0.0, 100.0}) {
                BRepClass3d_SolidClassifier end(shape, gp_Pnt(0,0,z), 1e-6);
                require(end.State() == TopAbs_OUT, "Tube end is capped across the bore");
            }
        }
        for (double invalid : {-1.0, 10.0, 12.0})
            require(BuildWireSolidShape(path,10,nullptr,invalid).IsNull(), "Invalid thickness accepted");
        CPolyline bent;
        bent.AddPoint({0,0,0}); bent.AddPoint({0,0,100}); bent.AddPoint({50,0,150});
        auto bent_tube = BuildWireSolidShape(bent,10,nullptr,2);
        require(!bent_tube.IsNull() && BRepCheck_Analyzer(bent_tube).IsValid(), "Bent tube failed");
        CBSpline spline;
        spline.AddPoint({0,0,0}); spline.AddPoint({0,0,30});
        spline.AddPoint({10,0,70}); spline.AddPoint({30,0,100});
        auto spline_tube = BuildWireSolidShape(spline,2,nullptr,0.5);
        require(!spline_tube.IsNull() && BRepCheck_Analyzer(spline_tube).IsValid(), "Spline tube failed");

        MainWindow window;
        auto& doc = window.document_;
        doc.GetObjects().clear();
        require(doc.CreateSketchPolyline({{0,0,0},{0,0,100}}, false, "Path", {}, {1,0,0}, {0,0,1}),
                "Cannot create tube sketch");
        const auto profile = doc.GetSelectedObject()->m_id;
        window.undo_redo_.Reset();
        window.ActivateParametricTool("SolidWireTool");
        require(parameter(window.active_parametric_object_.parameters,"thick") == 0, "Thick default is not zero");
        const auto id = doc.GetSelectedObject()->m_id;
        auto* thick_editor = window.property_panel_->findChild<QDoubleSpinBox*>("parameter_thick");
        require(thick_editor, "Missing Thick editor");
        window.property_panel_->findChild<QDoubleSpinBox*>("parameter_radius")->setValue(12);
        thick_editor->setValue(2);
        auto* straightTube=dynamic_cast<CSolid*>(doc.FindObjectById(id));
        require(straightTube&&quadro::BuildTubeCadBody(*straightTube,1/0.9f)==quadro::BodyMeshAttempt::Built,
            "Straight cylindrical tube did not use the shared ring mesh");
        const double tube_volume = volume(dynamic_cast<CSolid*>(doc.FindObjectById(id))->m_Shape);
        require(std::abs(tube_volume - std::acos(-1.0)*4400) < 1e-5, "Live Thick update did not create a tube");
        thick_editor->setValue(14);
        require(parameter(window.active_parametric_object_.parameters,"thick") == 2,
                "Invalid thickness did not restore last accepted parameters");
        QApplication::processEvents();
        window.AcceptActiveProperties();
        window.UndoDocumentChange();
        require(!doc.FindObjectById(id) && doc.FindObjectById(profile), "Tube Undo failed");
        window.RedoDocumentChange();
        require(std::abs(volume(dynamic_cast<CSolid*>(doc.FindObjectById(id))->m_Shape)-tube_volume)<1e-5,
                "Tube Redo lost wall thickness");
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room, error;
        const QString filename = settings.filePath("tube.dom3d");
        require(serializer.Save(filename,doc,room,view,{},error), "Cannot save tube");
        CAlfaDoc loaded;
        require(serializer.Load(filename,loaded,room,view,error), "Cannot load tube");
        require(window.tool_registry_.ReplayOperations(loaded.FindObjectIndexById(id),loaded), "Tube history replay failed");
        require(std::abs(volume(dynamic_cast<CSolid*>(loaded.FindObjectById(id))->m_Shape)-tube_volume)<1e-5,
                "Saved tube lost thickness on replay");
        MainWindow next;
        next.ActivateParametricTool("SolidWireTool");
        require(parameter(next.active_parametric_object_.parameters,"radius") == 12
                && parameter(next.active_parametric_object_.parameters,"thick") == 2,
                "Wire parameters were not remembered");
        next.property_panel_->findChild<QDoubleSpinBox*>("parameter_thick")->setValue(3);
        next.CancelActiveProperties();
        next.ActivateParametricTool("SolidWireTool");
        require(parameter(next.active_parametric_object_.parameters,"thick") == 2, "Cancel overwrote remembered thickness");
        next.CancelActiveProperties();
        std::cout << "Wire/Tube thickness, paths, volume, openings, live validation, Undo/Redo, persistence and remembered settings passed\n";
        return 0;
    }
    if (app.arguments().contains("--creation-undo-only")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        for (const char* tool : {"SolidBox", "SolidCylinder"}) {
            for (int finish : {0,1,2,3}) {
                MainWindow primitive;
                auto& doc=primitive.document_;doc.GetObjects().clear();primitive.undo_redo_.Reset();
                primitive.ActivateParametricTool(tool);
                auto parameters=primitive.tool_registry_.Find(tool)->defaults;
                if(std::string(tool)=="SolidBox")primitive.viewport_->SolidBoxRectangleFinished(parameters);
                else primitive.viewport_->SolidCylinderCircleFinished(parameters);
                require(doc.GetObjects().size()==1,"Drawn primitive missing");
                const auto id=doc.GetObjects().back()->m_id;
                require(primitive.undo_redo_.UndoCount()==0,"Primitive preview added an Undo step");
                if(finish==0){primitive.CancelActiveProperties();require(doc.GetObjects().empty()&&!primitive.undo_redo_.CanUndo(),"Canceled primitive changed history");continue;}
                primitive.active_parametric_object_.parameters.front().value+=2;
                primitive.tool_registry_.Rebuild(primitive.active_parametric_object_,doc);
                GProp_GProps before;BRepGProp::VolumeProperties(dynamic_cast<CSolid*>(doc.FindObjectById(id))->m_Shape,before);
                if(finish==1)primitive.AcceptActiveProperties();else if(finish==2)primitive.BeginTransformTool(TransformOperation::Universal);
                require(primitive.undo_action_->isEnabled()&&primitive.undo_redo_.UndoCount()==(finish==3?0:1),"Drawn primitive has no single Undo step");
                primitive.undo_action_->trigger();require(doc.GetObjects().empty(),"Undo did not remove drawn primitive");
                primitive.RedoDocumentChange();auto* restored=dynamic_cast<CSolid*>(doc.FindObjectById(id));require(restored&&BRepCheck_Analyzer(restored->m_Shape).IsValid(),"Redo did not restore drawn primitive");
                GProp_GProps after;BRepGProp::VolumeProperties(restored->m_Shape,after);require(std::abs(before.Mass()-after.Mass())<1.e-6,"Redo changed primitive dimensions");
            }
            std::cout<<tool<<" drawn creation Cancel, Undo/Redo and tool-switch passed\n";
        }
        for (const char* tool : {"SolidSphereTool", "SolidTorusTool", "SolidWireTool",
                                 "SolidPolyhedronTool", "SolidFrameTool", "SolidPrismTool", "SolidBeamTool"}) {
            for (bool accept : {false, true}) {
                MainWindow window;
                auto& doc = window.document_;
                doc.GetObjects().clear();
                unsigned long profile = 0;
                const std::string id = tool;
                if (id == "SolidWireTool" || id == "SolidFrameTool") {
                    const double size = id == "SolidFrameTool" ? 2.0 : 100.0;
                    require(doc.CreateSketchPolyline({{0,0,0},{size,0,0},{size,size,0},{0,size,0}},
                        id == "SolidFrameTool", "Profile", {}, {1,0,0}, {0,1,0}), "Cannot create path");
                    profile = doc.GetSelectedObject()->m_id;
                } else if (id == "SolidPolyhedronTool") {
                    require(doc.CreateSketchPolyline({{10,0,0},{20,0,0},{20,0,30},{10,0,30}},
                        true, "Profile", {}, {1,0,0}, {0,0,1}), "Cannot create polyhedron profile");
                    profile = doc.GetSelectedObject()->m_id;
                }
                const auto before = doc.GetObjects().size();
                window.undo_redo_.Reset();
                window.ActivateParametricTool(tool);
                require(!window.active_parametric_object_.transient && doc.GetObjects().size() == before + 1,
                        "Creation did not build a preview");
                const auto created = doc.GetObjects().back()->m_id;
                require(window.undo_redo_.UndoCount() == 0, "Preview added an Undo entry");
                if (!accept) {
                    window.CancelActiveProperties();
                    require(doc.GetObjects().size() == before && !window.undo_redo_.CanUndo(),
                            "Canceled creation changed Undo history");
                    if (profile) require(doc.FindObjectById(profile), "Cancel deleted the profile");
                    continue;
                }
                window.active_parametric_object_.parameters.front().value += 1.0;
                window.tool_registry_.Rebuild(window.active_parametric_object_, doc);
                GProp_GProps accepted_volume;
                BRepGProp::VolumeProperties(dynamic_cast<CSolid*>(doc.FindObjectById(created))->m_Shape, accepted_volume);
                window.AcceptActiveProperties();
                require(window.undo_redo_.UndoCount() == 1 && window.undo_action_->isEnabled(),
                        "Accepted creation is missing its single Undo entry");
                window.UndoDocumentChange();
                require(doc.GetObjects().size() == before && !doc.FindObjectById(created),
                        "Undo did not remove just the new body");
                if (profile) require(doc.FindObjectById(profile)->IsVisible(), "Undo did not restore profile visibility");
                window.RedoDocumentChange();
                auto* restored = dynamic_cast<CSolid*>(doc.FindObjectById(created));
                require(restored && BRepCheck_Analyzer(restored->m_Shape).IsValid(),
                        "Redo did not restore a valid solid");
                GProp_GProps restored_volume;
                BRepGProp::VolumeProperties(restored->m_Shape, restored_volume);
                require(std::abs(restored_volume.Mass() - accepted_volume.Mass()) < 1e-6,
                        "Redo lost the accepted size parameters");
                // A second creation after a lazy commit must snapshot the live scene.
                window.ActivateParametricTool("SolidSphereTool");
                window.AcceptActiveProperties();
                window.UndoDocumentChange();
                require(doc.FindObjectById(created) && doc.GetObjects().size() == before + 1,
                        "Undo of next creation removed the previous body");
                std::cout << tool << " creation Undo/Redo passed\n";
            }
        }
        MainWindow window;
        window.document_.GetObjects().clear();
        window.undo_redo_.Reset();
        window.ActivateParametricTool("SolidSphereTool");
        window.UndoDocumentChange();
        require(window.document_.GetObjects().empty() && window.undo_redo_.CanRedo(),
                "Undo while adjusting creation did not remove the preview");
        for (const char* tool : {"SolidWireTool", "SolidPolyhedronTool"}) {
            window.ActivateParametricTool(tool);
            window.AcceptActiveProperties();
            window.CancelActiveProperties();
            require(window.document_.GetObjects().empty() && !window.undo_redo_.CanUndo()
                    && window.undo_redo_.CanRedo(), "Missing profile altered Undo/Redo history");
        }
        window.RedoDocumentChange();
        window.ActivateParametricTool("SolidTorusTool");
        window.ActivateParametricTool("SolidSphereTool");
        window.AcceptActiveProperties();
        window.UndoDocumentChange();
        require(window.document_.GetObjects().size() == 2, "Tool switch merged creation commands");
        window.UndoDocumentChange();
        require(window.document_.GetObjects().size() == 1, "Tool switch lost the preceding creation");
        return 0;
    }
    if (app.arguments().contains("--cylinder-chamfer-only")) {
        SolidCylinderTool tool;
        auto parameters = tool.GetDefaultParameters();
        const auto set = [&](const char* id, double value) {
            for (auto& p : parameters) if (p.id == id) { p.value = value; return; }
            parameters.push_back({id, id, value});
        };
        require(parameter(parameters, "base_chamfer") == 0.0
            && parameter(parameters, "base_chamfer_size") == 1.0,
            "Entrance chamfer must be optional with a default size of 1 mm");
        set("diameter", 20); set("height", -8); set("base_chamfer", 1);
        CSolid solid;
        require(tool.RebuildShape(solid, parameters), "Standalone cylinder build failed");
        const auto volume = [](const CSolid& body) {
            GProp_GProps properties; BRepGProp::VolumeProperties(body.m_Shape, properties);
            return properties.Mass();
        };
        require(std::abs(volume(solid) - kPi * 100 * 8) < 0.01,
            "Entrance chamfer must not affect a standalone cylinder");
        set("boolean.body_id", 1);
        for (int axis = 0; axis < 3; ++axis) {
            set("axis.n.x", axis == 0); set("axis.n.y", axis == 1); set("axis.n.z", axis == 2);
            set("axis.u.x", axis != 0); set("axis.u.y", axis == 0); set("axis.u.z", 0);
            set("origin.x", 17); set("origin.y", 23); set("origin.z", 31);
            require(tool.RebuildShape(solid, parameters) && BRepCheck_Analyzer(solid.m_Shape).IsValid(),
                "Chamfered cutter must be a valid solid on every base plane");
            const double d = 1.001;
            require(std::abs(volume(solid) - kPi * (100 * 8.001 + 10 * d*d + d*d*d/3)) < 0.02,
                "Chamfer must widen the entrance and compensate Boolean overlap");
            int cones = 0;
            for (TopExp_Explorer faces(solid.m_Shape, TopAbs_FACE); faces.More(); faces.Next()) {
                BRepAdaptor_Surface surface(TopoDS::Face(faces.Current()));
                if (surface.GetType() != GeomAbs_Cone) continue;
                ++cones;
                require(std::abs(std::abs(surface.Cone().SemiAngle()) - std::acos(-1.0) / 4) < 1e-8,
                    "Entrance chamfer angle must be exactly 45 degrees");
            }
            require(cones == 1, "Only the entrance must have a chamfer");
        }
        set("base_chamfer_size", 8);
        require(!tool.RebuildShape(solid, parameters), "Oversized chamfer must be rejected");
        set("height", 8);
        require(tool.RebuildShape(solid, parameters)
            && std::abs(volume(solid) - kPi * 100 * 8.001) < 0.02,
            "Positive extrusion must not get a hole entrance chamfer");
        std::cout << "Cylinder entrance chamfer geometry passed\n";
        return 0;
    }
    if (app.arguments().contains("--initial-boolean-only")) {
        QTemporaryDir settings_directory;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_directory.path());
        for (bool cylinder : {false, true}) for (bool cancel : {false, true})
        for (bool chamfer : {false, true}) {
            if (chamfer && !cylinder) continue;
            MainWindow window;
            auto* document = GetAlfaDoc();
            document->GetObjects().clear();
            ToolRegistry registry;
            const auto set = [](std::vector<ToolParameter>& parameters, const char* id, double value) {
                for (auto& p : parameters) if (p.id == id) { p.value = value; return; }
                parameters.push_back({id, id, value, -1000000, 1000000, 0.001});
            };
            auto host_parameters = registry.Find("SolidBox")->defaults;
            set(host_parameters, "width", 60); set(host_parameters, "height", 60); set(host_parameters, "depth", 50);
            const auto host = registry.CreateParametricObject("SolidBox", *document, host_parameters);
            const auto host_id = document->GetObjects()[host.object_index]->m_id;
            const auto volume = [](const CSolid& solid) {
                GProp_GProps properties;
                BRepGProp::VolumeProperties(solid.m_Shape, properties);
                return properties.Mass();
            };
            auto* viewport = window.findChild<OpenGLViewport*>();
            auto parameters = registry.Find(cylinder ? "SolidCylinder" : "SolidBox")->defaults;
            if (cylinder) { set(parameters, "diameter", 20); set(parameters, "height", -8); }
            else { set(parameters, "width", 20); set(parameters, "height", 20); set(parameters, "depth", -8); }
            set(parameters, "origin.x", cylinder ? 20 : 10);
            set(parameters, "origin.y", cylinder ? 20 : 10);
            set(parameters, "origin.z", 50);
            set(parameters, "boolean.body_id", static_cast<double>(host_id));
            // This is the first notification sent by the completed height gesture.
            // Do not edit a parameter: the initial panel must already show the cut.
            if (cylinder) viewport->SolidCylinderCircleFinished(parameters);
            else viewport->SolidBoxRectangleFinished(parameters);
            auto* body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
            double expected = 180000 - (cylinder ? kPi * 100 : 400) * 8;
            require(body && std::abs(volume(*body) - expected) < 0.1,
                "Initial negative height must cut the host before parameter edits or OK");
            PropertyPanel* panel = nullptr;
            for (auto* candidate : window.findChildren<PropertyPanel*>())
                if (candidate->ActiveObject().tool_id == (cylinder ? "SolidCylinder" : "SolidBox")) panel = candidate;
            require(panel, "Initial cut must retain the parameter editor");
            if (chamfer) {
                QCheckBox* enable = nullptr;
                for (auto* checkbox : panel->findChildren<QCheckBox*>()) {
                    if (!checkbox->isChecked()) { enable = checkbox; break; }
                }
                require(enable, "Hole editor must expose the entrance chamfer checkbox");
                enable->setChecked(true);
                expected -= kPi * (10.0 + 1.0 / 3.0);
                body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
                require(body && std::abs(volume(*body) - expected) < 0.1,
                    "Enabling chamfer must preview the widened hole before OK");
            }
            const auto tool_index = panel->ActiveObject().object_index;
            require(tool_index < document->GetObjects().size() && !document->GetObjects()[tool_index]->IsVisible(),
                "The cutter must not obscure the initial cut preview");
            if (app.arguments().contains("--render") && !cancel) {
                QDir().mkpath("output/initial-boolean");
                window.resize(1100, 900); window.move(-20000, -20000); window.show();
                Camera camera; camera.target = {25,25,30}; camera.distance = 135;
                viewport->SetCamera(camera); viewport->SetOrthographicProjection(true);
                app.processEvents();
                require(viewport->grabFramebuffer().save(cylinder ? (chamfer ? "output/initial-boolean/cylinder-chamfer-before-ok.png" : "output/initial-boolean/cylinder-before-ok.png")
                    : "output/initial-boolean/box-before-ok.png"), "Cannot save initial cut preview");
            }
            if (cancel) panel->Canceled(); else panel->Accepted();
            body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
            require(body && document->GetObjects().size() == 1
                && std::abs(volume(*body) - (cancel ? 180000 : expected)) < 0.1,
                "Cancel must restore the host; OK must keep the initial preview result");
            if (chamfer && !cancel) {
                Dom3DProjectSerializer serializer;
                QString error, room; ProjectViewState view;
                const QString path = settings_directory.filePath("chamfer.dom3d");
                require(serializer.Save(path, *document, {}, view, {}, error),
                    "Chamfered hole must save");
                CAlfaDoc restored;
                require(serializer.Load(path, restored, room, view, error),
                    "Chamfered hole must load");
                auto* saved_body = dynamic_cast<CSolid*>(restored.GetObjects().front().get());
                require(saved_body && std::abs(volume(*saved_body) - expected) < 0.1,
                    "Saved chamfered hole must retain its geometry");
                const ParametricFunction* operation = nullptr;
                for (size_t i = 0; i < body->GetBooleanToolCount(); ++i) {
                    const auto* cutter = body->GetBooleanTool(i);
                    const auto* candidate = cutter ? cutter->GetOperation(0) : nullptr;
                    if (candidate && candidate->ToolId == "SolidCylinder") operation = candidate;
                }
                require(operation, "Confirmed hole must retain its parametric cutter");
                bool enabled = false, size_saved = false;
                for (const auto& p : operation->Parameters) {
                    if (p.id == "base_chamfer") enabled = p.value == 1.0;
                    if (p.id == "base_chamfer_size") size_saved = p.value == 1.0;
                }
                require(enabled && size_saved, "Chamfer settings must survive confirmation");
            }
        }
        std::cout << "Initial Boolean preview, OK and Cancel passed\n";
        return 0;
    }
    if (app.arguments().contains("--primitive-creation-only")) {
        QTemporaryDir settings_directory;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_directory.path());
        QSettings("Dom3D", "Dom3D_Pro").setValue("preferences/modeling/snappingEnabled", false);
        for (bool ortho : {true, false}) for (int plane = 0; plane < 3; ++plane)
        for (bool cylinder : {false, true}) for (bool drag_base : {false, true}) {
            CAlfaDoc document;
            OpenGLViewport view;
            view.resize(1000, 800);
            view.SetDocument(&document);
            view.SetOrthographicProjection(ortho);
            Camera camera; camera.target = {10, 10, 10}; camera.distance = 200;
            view.SetCamera(camera);
            const auto placement = static_cast<OpenGLViewport::SketchPlane>(plane);
            const Vec3 u = plane == 2 ? Vec3{0,1,0} : Vec3{1,0,0};
            const Vec3 v = plane == 0 ? Vec3{0,1,0} : Vec3{0,0,1};
            const Vec3 normal = plane == 0 ? Vec3{0,0,1} : plane == 1 ? Vec3{0,1,0} : Vec3{1,0,0};
            if (cylinder) view.BeginSolidCylinderCircle(placement);
            else view.BeginSolidBoxRectangle(placement);
            view.SetSolidPrimitivePlacement(placement, false);
            std::vector<ToolParameter> completed;
            int completions = 0;
            const auto finish = [&](std::vector<ToolParameter> p) { completed = std::move(p); ++completions; };
            QObject::connect(&view, &OpenGLViewport::SolidBoxRectangleFinished, &view, finish);
            QObject::connect(&view, &OpenGLViewport::SolidCylinderCircleFinished, &view, finish);
            const auto project = [&](Vec3 world) {
                DomPoint p; QtSceneRenderer renderer;
                require(renderer.WorldToScreen(world, view.GetCamera(), ortho, 1000, 800, p), "Creation point must project");
                return QPointF(p.x, p.y);
            };
            const auto mouse = [&](QEvent::Type type, QPointF p, Qt::MouseButton button,
                                   Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                QMouseEvent e(type, p, p, button, buttons, modifiers);
                QApplication::sendEvent(&view, &e);
            };
            const auto initial_object_count = document.GetObjects().size();
            const QPointF first = project(u * 4 + v * 4);
            const QPointF second = project(u * 24 + v * 14);
            mouse(QEvent::MouseButtonPress, first, Qt::LeftButton, Qt::LeftButton);
            if (!drag_base) {
                mouse(QEvent::MouseButtonRelease, first, Qt::LeftButton, Qt::NoButton);
                mouse(QEvent::MouseMove, second, Qt::NoButton, Qt::NoButton);
                mouse(QEvent::MouseButtonPress, second, Qt::LeftButton, Qt::LeftButton);
            }
            mouse(QEvent::MouseMove, second, Qt::NoButton, Qt::LeftButton, cylinder ? Qt::NoModifier : Qt::ShiftModifier);
            require(completions == 0, "Base press must not create a solid");
            mouse(QEvent::MouseButtonRelease, second, Qt::LeftButton, Qt::NoButton, cylinder ? Qt::NoModifier : Qt::ShiftModifier);
            require(completions == 0 && document.GetObjects().size() == initial_object_count, "Base release must enter height preview without creating a solid");
            const bool render_preview = app.arguments().contains("--render") && ortho && plane == 0 && !drag_base;
            if (render_preview) {
                QDir().mkpath("output/primitive-creation");
                view.move(-20000, -20000);
                view.show();
                app.processEvents();
                require(view.grabFramebuffer().save(cylinder ? "output/primitive-creation/cylinder-base.png"
                    : "output/primitive-creation/box-base.png"), "Cannot save base preview");
            }
            // Square corner follows Shift; the radius point remains unchanged.
            const Vec3 anchor = u * 24 + v * (cylinder ? 14.0f : 24.0f);
            const double desired_height = drag_base ? -35 : 45;
            const QPointF height = project(anchor + normal * static_cast<float>(desired_height));
            mouse(QEvent::MouseMove, height, Qt::NoButton, Qt::NoButton);
            require(completions == 0, "Height movement must remain a preview");
            require(view.primitive_preview_solid_ && !view.primitive_preview_solid_->m_Shape.IsNull(),
                "Height movement must already build the real solid");
            Vec3 live_min{}, live_max{};
            require(view.primitive_preview_solid_->GetBounds(live_min, live_max)
                && std::abs(dot(live_max - live_min, normal) - std::abs(view.primitive_height_)) < 0.01,
                "Live solid must have the current height along the working plane normal");
            if (render_preview) {
                require(view.grabFramebuffer().save(cylinder ? "output/primitive-creation/cylinder-height.png"
                    : "output/primitive-creation/box-height.png"), "Cannot save height preview");
            }
            if (drag_base) {
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(&view, &enter);
            } else {
                mouse(QEvent::MouseButtonPress, height, Qt::LeftButton, Qt::LeftButton);
                mouse(QEvent::MouseButtonRelease, height, Qt::LeftButton, Qt::NoButton);
            }
            require(completions == 1, "Height confirmation must create exactly one primitive");
            require(!view.primitive_preview_solid_, "Confirmation must release the temporary solid");
            require(std::abs(parameter(completed, cylinder ? "height" : "depth") - desired_height) < 2,
                "Signed height must follow the working plane normal in either projection");
            if (!cylinder) require(std::abs(parameter(completed,"width") - parameter(completed,"height")) < 1.e-5,
                "Shift must create a square base");
            if (cylinder) view.BeginSolidCylinderCircle(placement); else view.BeginSolidBoxRectangle(placement);
            view.SetSolidPrimitivePlacement(placement, false);
            mouse(QEvent::MouseButtonPress, first, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseMove, second, Qt::NoButton, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, second, Qt::LeftButton, Qt::NoButton);
            mouse(QEvent::MouseMove, height, Qt::NoButton, Qt::NoButton);
            require(view.primitive_preview_solid_ != nullptr, "Cancellation fixture must contain a live body");
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(&view, &escape);
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(&view, &enter);
            require(completions == 1, "Escape must discard height preview");
            require(!view.primitive_preview_solid_, "Escape must discard the temporary body");
            view.SetDocument(nullptr);
        }
        std::cout << "Primitive creation gestures passed\n";
        return 0;
    }
    if (app.arguments().contains("--navigation-only")) {
        QTemporaryDir settings_directory;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_directory.path());
        CAlfaDoc document;
        TopoDS_Shape navigation_box = BRepPrimAPI_MakeBox(30, 30, 10).Shape();
        document.AddObject(std::make_unique<CSolid>(navigation_box), false);
        for (bool ortho : {true, false}) {
            OpenGLViewport view;
            view.resize(1000, 800);
            view.SetDocument(&document);
            view.SetTool(ToolMode::Orbit);
            view.SetOrthographicProjection(ortho);
            Camera initial;
            initial.orientation = {1, 0, 0, 0};
            initial.target = {10, 10, 0};
            initial.distance = 100;
            const auto project = [&](Vec3 world) {
                DomPoint p;
                QtSceneRenderer renderer;
                require(renderer.WorldToScreen(world, view.GetCamera(), ortho, 1000, 800, p), "Navigation point must project");
                return QPoint(p.x, p.y);
            };
            const auto zoom = [&](QPoint p, int delta) {
                QWheelEvent event(QPointF(p), QPointF(p), {}, QPoint(0, delta),
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                QApplication::sendEvent(&view, &event);
            };
            const auto drag = [&](QPoint start, Vec3 pivot) {
                const QPoint before = project(pivot);
                const Camera camera_before = view.GetCamera();
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(start), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&view, &press);
                const Vec3 displacement = view.GetCamera().target - camera_before.target;
                require(dot(displacement, displacement) < 1.e-8f, "Orbit press must not recenter scene");
                const QPoint end = start + QPoint(53, 27);
                QMouseEvent move(QEvent::MouseMove, QPointF(end), QPointF(end), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&view, &move);
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(end), QPointF(end), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&view, &release);
                require((project(pivot) - before).manhattanLength() <= 2, "Orbit pivot must stay at the same screen position");
                require(std::abs(view.GetCamera().orientation.x - camera_before.orientation.x) > 0.001f,
                    "Navigation drag must rotate camera");
            };
            // Use the precise world point of the integer cursor ray on z=10.
            view.SetCamera(initial);
            const QPoint hit(570, 345);
            const float scale = ortho ? 100 * 0.42f / 400 : 90 * std::tan(deg_to_rad(50) / 2) / 400;
            const Vec3 surface{10 + 70 * scale, 10 + 55 * scale, 10};
            for (int delta : {120, -240, 360, -120}) {
                const QPoint before = project(surface);
                zoom(hit, delta);
                require((project(surface) - before).manhattanLength() <= 1, "Wheel must preserve surface point under cursor");
            }
            view.SetCamera(initial);
            drag(hit, surface);
            view.SetCamera(initial);
            drag({50, 50}, {15, 15, 5});
            view.SetCamera(initial);
            view.SetRotationPivot({5, 7, 2});
            zoom({800, 650}, 240);
            drag({50, 50}, {5, 7, 2});
            require(view.HasRotationPivot(), "Wheel and orbit must preserve explicit pivot mode");
            view.SetDocument(nullptr);
            view.SetCamera(initial);
            const float empty_scale = ortho ? 100 * 0.42f / 400 : 100 * std::tan(deg_to_rad(50) / 2) / 400;
            const Vec3 empty_anchor{10 + 300 * empty_scale, 10 - 250 * empty_scale, 0};
            const QPoint before = project(empty_anchor);
            zoom({800, 650}, 120);
            require((project(empty_anchor) - before).manhattanLength() <= 1, "Wheel must anchor empty background on target plane");
            // A sloped mesh has a different hit depth at every pixel.
            CAlfaDoc mesh_document;
            auto mesh = std::make_unique<CMesh3D>();
            mesh->GetVertices() = {{-50,-50,0}, {50,-50,50}, {50,50,50}, {-50,50,0}};
            mesh->GetFaces().push_back(MeshFace{0, 1, 2, 3});
            mesh_document.AddObject(std::move(mesh), false);
            view.SetDocument(&mesh_document);
            view.SetCamera(initial);
            const float slope = ortho ? 0 : 70 * std::tan(deg_to_rad(50) / 2) / 400;
            const float mesh_x = ortho ? 10 + 70 * 0.105f : (10 + 75 * slope) / (1 + 0.5f * slope);
            const float mesh_z = 25 + 0.5f * mesh_x;
            const float mesh_y = ortho ? 10 + 55 * 0.105f : 10 + (100 - mesh_z) * 55 * std::tan(deg_to_rad(50) / 2) / 400;
            const Vec3 mesh_hit{mesh_x, mesh_y, mesh_z};
            const QPoint mesh_before = project(mesh_hit);
            zoom(hit, 240);
            require((project(mesh_hit) - mesh_before).manhattanLength() <= 1, "Wheel must preserve exact sloped mesh hit");
            view.SetCamera(initial);
            drag(hit, mesh_hit);
            view.SetDocument(nullptr);
        }
        std::cout << "Scene navigation regression passed\n";
        return 0;
    }
    if (app.arguments().contains("--primitive-enter-only")) {
        QWidget host;
        PropertyPanel panel(&host);
        OpenGLViewport scene(&host);
        host.show(); panel.show(); scene.show();
        int accepted = 0;
        QObject::connect(&panel, &PropertyPanel::Accepted, &host, [&] { ++accepted; });
        const auto press = [](QWidget* target, int key) {
            QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
        };
        for (const char* id : {"SolidBox", "SolidCylinder"}) {
            ActiveParametricObject object;
            object.tool_id = id;
            object.parameters = {{"height", "Height", 50, 0.1, 100, 0.1}};
            panel.SetActiveObject(object);
            auto* spin = panel.findChild<QDoubleSpinBox*>();
            require(spin, "Primitive height editor missing");
            for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
                const int before = accepted;
                spin->setKeyboardTracking(false);
                spin->findChild<QLineEdit*>()->setText(spin->locale().toString(61.5, 'f', 1));
                press(spin->findChild<QLineEdit*>(), key);
                std::cerr << "enter accepted=" << accepted << " expected=" << before+1
                          << " value=" << parameter(panel.ActiveObject().parameters,"height") << '\n';
                require(accepted == before + 1 && parameter(panel.ActiveObject().parameters,"height") == 61.5,
                        "Enter must commit edited value and accept exactly once");
                press(&scene, key);
                require(accepted == before + 2, "Enter in viewport must confirm visible primitive parameters");
            }
        }
        const int before = accepted;
        panel.hide();
        press(&scene, Qt::Key_Return);
        require(accepted == before, "Hidden primitive panel must not handle Enter");
        panel.show();
        QDialog modal(&host); modal.setModal(true); modal.show(); app.processEvents();
        press(&scene, Qt::Key_Return);
        require(accepted == before, "Another modal dialog must retain its Enter key");
        modal.hide();
        std::cout << "Primitive Enter confirms editor and viewport input without stealing other dialogs\n";
        return 0;
    }
    QTemporaryDir settings;
    require(settings.isValid(), "Missing temporary settings directory");
    QCoreApplication::setOrganizationName("Dom3DPrimitiveTests");
    QCoreApplication::setApplicationName("SolidPrimitiveToolTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    MainWindow window;
    auto* viewport = window.findChild<OpenGLViewport*>();
    auto action = [&window](const char* key) -> QAction* {
        for (auto* candidate : window.findChildren<QAction*>())
            if (candidate->property("toolKey").toString() == key) return candidate;
        require(false, "Missing primitive action");
        return nullptr;
    };
    Camera camera;
    camera.orientation = quaternion_from_axis_angle({0, 0, 1}, 0.6f)
        * quaternion_from_axis_angle({1, 0, 0}, 0.8f);
    camera.target = {10, 10, 0};
    camera.distance = 80;
    viewport->resize(1000, 800);
    viewport->SetCamera(camera);
    viewport->SetOrthographicProjection(true);
    std::vector<ToolParameter> completed;
    QObject::connect(viewport, &OpenGLViewport::SolidBoxRectangleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    QObject::connect(viewport, &OpenGLViewport::SolidCylinderCircleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    if (app.arguments().contains("--coordinate-axis-only")) {
        camera.target = {}; camera.distance = 800;
        camera.orientation = kDefaultCameraOrientation;
        viewport->SetCamera(camera);
        viewport->SetCoordinateAxesVisible(false);
        int picked = -1;
        QObject::connect(viewport, &OpenGLViewport::CoordinateAxisSelected, &window, [&](int axis) { picked = axis; });
        for (const char* tool : {"SurfaceOfRevolution", "SurfaceRevolve", "SolidPolyhedronTool"}) {
            window.ClearActiveProperties();
            require(window.document_.CreateSketchPolyline({{10,0,10},{20,0,10},{20,0,20},{10,0,20}},
                    true, "Axis test", {}, {1,0,0}, {0,0,1}), "Axis profile creation failed");
            const auto profileId = window.document_.GetSelectedObject()->m_id;
            window.document_.ClearSelection();
            window.ActivateParametricTool(tool);
            require(window.property_panel_->isVisible(), "Command must wait without preselection");
            window.document_.SelectObjectById(profileId);
            viewport->SelectionChanged();
            auto* combo = window.property_panel_->findChild<QComboBox*>("parameter_axis");
            require(combo && window.property_panel_->isVisible(), "Axis parameter must be available");
            for (int axis : {0,1,2}) {
                picked = -1;
                click(*viewport, axis == 0 ? Vec3{100,0,0} : axis == 1 ? Vec3{0,100,0} : Vec3{0,0,100});
                require(picked == axis, "Viewport must pick the requested axis");
                app.processEvents();
                combo = window.property_panel_->findChild<QComboBox*>("parameter_axis");
                const int expected = std::string(tool) == "SolidPolyhedronTool" && axis == 1 ? 0 : axis;
                require(combo->currentIndex() == expected,
                        "Valid axis must rebuild; invalid axis must restore previous parameters");
            }
            require(!viewport->IsCoordinateAxesVisible(), "Temporary axis display must preserve user preference");
        }
        window.ClearActiveProperties();
        picked = -1;
        click(*viewport, {100,0,0});
        require(picked == -1, "Axis picking must end when parameter editing ends");
        for (const char* tool : {"SolidFrameTool", "SolidWireTool", "SolidPolyhedronTool"}) {
            window.document_.ClearSelection();
            const auto count = window.document_.GetObjects().size();
            window.ActivateParametricTool(tool);
            require(window.active_parametric_object_.transient, "Profile tool must wait for selection");
            window.AcceptActiveProperties();
            require(window.active_parametric_object_.transient, "OK without profile must keep waiting");
            window.CancelActiveProperties();
            require(window.document_.GetObjects().size() == count, "Cancel while waiting must preserve scene objects");
            if (std::string(tool) == "SolidPolyhedronTool") continue;
            require(window.document_.CreateSketchPolyline({{0,0,0},{100,0,0},{100,100,0},{0,100,0}},
                std::string(tool) == "SolidFrameTool", "Pending profile", {}, {1,0,0}, {0,1,0}), "Pending profile fixture failed");
            const auto profile = window.document_.GetSelectedObject()->m_id;
            window.document_.ClearSelection();
            window.ActivateParametricTool(tool);
            window.document_.SelectObjectById(profile);
            viewport->SelectionChanged();
            require(!window.active_parametric_object_.transient && window.document_.GetSelectedSolid(),
                "Profile selection must start pending tool");
            window.AcceptActiveProperties();
        }
        require(window.document_.CreateSketchPolyline({{0,10,0},{20,10,0},{20,30,0},{0,30,0}},
            true, "XY profile", {}, {1,0,0}, {0,1,0}), "XY profile fixture failed");
        window.ActivateParametricTool("SolidPolyhedronTool");
        require(window.active_parametric_object_.transient, "Invalid default axis must keep the tool open");
        click(*viewport, {100,0,0});
        require(!window.active_parametric_object_.transient && window.document_.GetSelectedSolid(),
            "Picking a valid axis must create the waiting polyhedron");
        window.AcceptActiveProperties();
        // Moving only the sketch coordinate-system origin must not move a world axis.
        double referenceVolume = 0;
        for (const auto origin : {CPoint3d{0,0,0}, CPoint3d{5,0,17}}) {
            CAlfaDoc axisDocument;
            require(axisDocument.CreateSketchPolyline({{10,0,30},{20,0,30},{20,0,40},{10,0,40}},
                true, "Shifted sketch origin", origin, {1,0,0}, {0,0,1}), "Shifted sketch creation failed");
            const auto profileId = axisDocument.GetSelectedObject()->m_id;
            require(axisDocument.CreatePolyhedronSolid(profileId, 0, 8), "World X polyhedron creation failed");
            auto* solid = axisDocument.GetSelectedSolid();
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid->m_Shape, props);
            if (referenceVolume == 0) referenceVolume = props.Mass();
            require(std::abs(props.Mass()-referenceVolume) < referenceVolume*1e-7,
                "World X geometry must not depend on sketch origin");
            const auto checkAxis = [](const CSolid& body) {
                require(!body.GetCenterlines().empty(), "Missing polyhedron axis");
                for (const auto& point : body.GetCenterlines().front().points)
                    require(std::abs(point.Y()) < 1e-5 && std::abs(point.Z()) < 1e-5,
                        "Polyhedron centerline must coincide with world X");
            };
            checkAxis(*solid);
            auto edit = window.tool_registry_.ActiveObjectFromDocument(
                axisDocument.GetSelectedObjectIndex(), *solid, 0, &axisDocument);
            require(window.tool_registry_.TryRebuildPolyhedron(edit, axisDocument), "World X rebuild failed");
            solid = axisDocument.GetSelectedSolid();
            checkAxis(*solid);
            BRepGProp::VolumeProperties(solid->m_Shape, props);
            require(std::abs(props.Mass()-referenceVolume) < referenceVolume*1e-7,
                "Rebuild must preserve the world X geometry");
        }
        CAlfaDoc offsetPlane;
        require(offsetPlane.CreateSketchPolyline({{10,5,30},{20,5,30},{20,5,40},{10,5,40}},
            true, "Offset plane", {0,5,0}, {1,0,0}, {0,0,1}), "Offset plane fixture failed");
        require(!offsetPlane.CreatePolyhedronSolid(offsetPlane.GetSelectedObject()->m_id, 0, 8),
            "World axis outside the sketch plane must be rejected");
        CAlfaDoc fixture;
        Dom3DProjectSerializer serializer;
        QString room, error; ProjectViewState view;
        require(serializer.Load(QDir(QCoreApplication::applicationDirPath()).filePath("../../tests/data/mesh-regression/Polyhedron.dom3d"), fixture, room, view, error),
                "Cannot load Polyhedron regression project");
        const auto index = fixture.FindObjectIndexById(3);
        auto active = window.tool_registry_.ActiveObjectFromDocument(index, *fixture.FindObjectById(3), 0, &fixture);
        const auto before = serializer.DocumentFingerprint(fixture);
        require(!fixture.CreatePolyhedronSolid(2, 0, 8), "Creation must reject a crossing axis too");
        require(serializer.DocumentFingerprint(fixture) == before, "Failed creation must preserve the document");
        for (auto& p : active.parameters) if (p.id == "axis") p.value = 0;
        require(!window.tool_registry_.TryRebuildPolyhedron(active, fixture), "Crossing axis must reject invalid polyhedron");
        require(serializer.DocumentFingerprint(fixture) == before, "Rejected rebuild must preserve geometry and history");
        std::cout << "Coordinate axis selection passed for solid/surface Revolve and Polyhedron\n";
        return 0;
    }
    if (app.arguments().contains("--camera-animation-only")) {
        CAlfaDoc document;
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(gp_Pnt(50,50,0),30,30,10).Shape();
        auto body = std::make_unique<CSolid>(shape);
        require(body->ReBuldMesh(), "Hover fixture mesh failed");
        document.AddObject(std::move(body), false);
        OpenGLViewport view;
        view.SetDocument(&document);
        view.resize(800,600);
        view.move(-20000,-20000);
        view.show();
        const auto wait = [](int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); };
        Camera initial;
        initial.orientation = Quaternion{};
        initial.target = {65,65,10};
        initial.distance = 100;
        view.SetCamera(initial);
        view.SetOrthographicProjection(true);
        view.SetReferencePlaneSelection(true);
        wait(30);
        const QPointF pos(400,300);
        QMouseEvent move(QEvent::MouseMove,pos,pos,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(&view,&move);
        require(view.cursor().shape() == Qt::PointingHandCursor, "Planar body face must react to hover");
        require(!document.HasSelectedSolidFace(), "Hover must not select a face");
        const QString screenshot = qEnvironmentVariable("DOM3D_CAMERA_ANIMATION_SCREENSHOT");
        if (!screenshot.isEmpty()) require(view.grabFramebuffer().save(screenshot + "-hover.png"), "Hover screenshot failed");
        view.SetReferencePlaneSelection(false);
        initial.orientation = kDefaultCameraOrientation;
        view.SetCamera(initial);
        view.BeginSketchOnFace("Animated", {50,50,10},{1,0,0},{0,1,0},{0,0,1},0,-1);
        const auto similarity = [](Quaternion a, Quaternion b) {
            return std::abs(a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z);
        };
        require(similarity(view.GetCamera().orientation, initial.orientation) > 0.9999f,
                "Sketch camera must start from current orientation");
        wait(180);
        require(similarity(view.GetCamera().orientation, initial.orientation) < 0.999f
                && similarity(view.GetCamera().orientation, Quaternion{}) < 0.999f,
                "Camera must pass through intermediate rotations");
        if (!screenshot.isEmpty()) require(view.grabFramebuffer().save(screenshot + "-turn.png"), "Turn screenshot failed");
        wait(350);
        require(similarity(view.GetCamera().orientation, Quaternion{}) > 0.9999f
                && std::abs(view.GetCamera().distance-initial.distance) < 0.001f,
                "Camera must reach face normal without changing orthographic scale");
        view.SetCamera(initial);
        view.BeginSketchOnFace("Interrupted", {50,50,10},{1,0,0},{0,1,0},{0,0,1},0,-1);
        wait(80);
        Camera manual = view.GetCamera(); manual.distance = 177;
        view.SetCamera(manual);
        wait(500);
        require(view.GetCamera().distance == 177 && similarity(view.GetCamera().orientation, manual.orientation) > 0.9999f,
                "Animation must not overwrite manual camera changes");
        view.SetDocument(nullptr);
        std::cout << "Camera animation and non-destructive face hover passed\n";
        return 0;
    }
    if (app.arguments().contains("--reference-planes-only")) {
        for (bool ortho : {true, false}) {
            OpenGLViewport view;
            Camera before;
            before.target = {240, 180, 30};
            before.distance = 350;
            view.SetCamera(before);
            view.SetOrthographicProjection(ortho);
            const Vec3 focus{240,180,10};
            const float depth = dot(focus - camera_position(before),
                                    rotate(before.orientation, {0,0,-1}));
            const float expected = ortho ? before.distance
                : depth * std::tan(deg_to_rad(before.vertical_fov_degrees) * 0.5f) / 0.42f;
            view.BeginSketchOnFace("Scale", {0,0,10}, {1,0,0}, {0,1,0}, {0,0,1}, 0, -1);
            const auto after = view.GetCamera();
            require(std::abs(after.distance - expected) < 0.001f,
                    "Face sketch must preserve visible scale in either projection");
            require(std::abs(after.target.x - focus.x) < 0.001f
                    && std::abs(after.target.y - focus.y) < 0.001f
                    && std::abs(after.target.z - focus.z) < 0.001f,
                    "Face sketch must keep the viewed area centered on the plane");
        }
        const QString screenshot = qEnvironmentVariable("DOM3D_REFERENCE_PLANES_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            window.move(-20000, -20000);
            window.resize(1100, 800);
            window.show();
            camera.target = {};
            camera.distance = 800;
            viewport->SetCamera(camera);
            action("SolidBox")->trigger();
            app.processEvents();
            require(viewport->grabFramebuffer().save(screenshot), "Reference plane screenshot failed");
            const float half_patch = camera.distance * 0.42f * 105.0f / viewport->height();
            DomPoint hover;
            QtSceneRenderer projector;
            require(projector.WorldToScreen({half_patch, half_patch, 0}, camera, true,
                    viewport->width(), viewport->height(), hover), "Cannot project reference hover");
            const QPointF position(hover.x, hover.y);
            QMouseEvent move(QEvent::MouseMove, position, position, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &move);
            require(viewport->cursor().shape() == Qt::PointingHandCursor, "Reference plane hover cursor missing");
            app.processEvents();
            require(viewport->grabFramebuffer().save(screenshot + ".hover.png"), "Reference hover screenshot failed");
        }
        int selected = -1;
        QObject::connect(viewport, &OpenGLViewport::ReferencePlaneSelected, &window,
                         [&](int plane) { selected = plane; });
        for (bool ortho : {true, false}) for (bool cylinder : {false, true}) for (int plane : {0,1,2}) {
            camera.target = {};
            camera.distance = 80;
            camera.orientation = plane == 0 ? Quaternion{}
                : quaternion_from_axis_angle(plane == 1 ? Vec3{1,0,0} : Vec3{0,1,0}, 1.5707963f);
            viewport->SetCamera(camera);
            viewport->SetOrthographicProjection(ortho);
            action(cylinder ? "SolidCylinder" : "SolidBox")->trigger();
            selected = -1;
            completed.clear();
            const auto point = [plane](float u, float v) {
                return plane == 0 ? Vec3{u,v,0} : plane == 1 ? Vec3{u,0,v} : Vec3{0,u,v};
            };
            click(*viewport, point(4,4));
            require(selected == plane, "Viewport plane click must select the facing reference plane");
            auto* combo = window.findChild<QComboBox*>("SolidPlacementType");
            require(combo && combo->currentIndex() == plane, "Plane click must update placement dialog");
            require(completed.empty(), "Plane selection must not finish a primitive");
            click(*viewport, point(4,4));
            require(completed.empty(), "Plane selection must not create the first anchor");
            click(*viewport, point(14,16), true);
            require(!completed.empty(), "Primitive must finish after base and height confirmation");
            require(parameter(completed, plane == 0 ? "axis.n.z" : plane == 1 ? "axis.n.y" : "axis.n.x") == 1,
                    "Primitive must use the plane selected in the viewport");
            viewport->SetTool(ToolMode::Select);
        }
        viewport->SetTool(ToolMode::Select);
        viewport->SetReferencePlaneSelection(true);
        camera.orientation = Quaternion{};
        viewport->SetCamera(camera);
        selected = -1;
        click(*viewport, {4,4,0});
        require(selected == 0, "Dialog plane selection must work outside primitive tools");
        viewport->SetReferencePlaneSelection(false);
        selected = -1;
        click(*viewport, {4,4,0});
        require(selected == -1, "Reference planes must stop intercepting clicks after cancellation");
        camera.orientation = quaternion_from_axis_angle({0,1,0}, 1.5707963f);
        viewport->SetCamera(camera);
        QTimer::singleShot(0, &window, [&] {
            QDialog* sketch_dialog = nullptr;
            for (auto* dialog : window.findChildren<QDialog*>())
                if (dialog->windowTitle() == "New Sketch" && dialog->isVisible()) sketch_dialog = dialog;
            require(sketch_dialog && !sketch_dialog->isModal(), "Sketch plane dialog must allow viewport input");
            click(*viewport, {0,4,4});
            auto* combo = sketch_dialog->findChild<QComboBox*>();
            require(combo && combo->currentIndex() == 2, "Sketch dialog must follow viewport plane selection");
            require(sketch_dialog->result() == QDialog::Accepted && !sketch_dialog->isVisible(),
                    "Plane click must accept sketch creation without OK");
        });
        action("NewSketch")->trigger();
        require(viewport->CurrentTool() == ToolMode::SketchRectangle,
                "Plane click must immediately start sketch drawing");
        viewport->EndSketch();
        selected = -1;
        click(*viewport, {0,4,4});
        require(selected == -1, "Closing sketch dialog must clear reference planes");
        for (int kind = 0; kind < 3; ++kind) {
            const float x = 50.0f + kind * 60.0f;
            TopoDS_Shape body_shape = BRepPrimAPI_MakeBox(gp_Pnt(x,50,0),30,30,10).Shape();
            auto body = std::make_unique<CSolid>(body_shape);
            require(body->ReBuldMesh(), "Cannot mesh direct face placement fixture");
            window.document_.AddObject(std::move(body), false);
            const auto body_id = window.document_.GetObjects().back()->m_id;
            camera.orientation = Quaternion{};
            camera.target = {x+15,65,10};
            camera.distance = 150;
            viewport->SetCamera(camera);
            viewport->SetOrthographicProjection(true);
            if (kind < 2) {
                action(kind == 0 ? "SolidBox" : "SolidCylinder")->trigger();
                completed.clear();
                click(*viewport, {x+15,65,10});
                require(window.findChild<QComboBox*>("SolidPlacementType")->currentIndex() == 3,
                        "Direct face click must update Placement to Solid Face");
                click(*viewport, {x+8,58,10});
                require(completed.empty(), "Face choice must not create the first primitive anchor");
                click(*viewport, {x+18,68,10}, true);
                require(!completed.empty() && std::abs(parameter(completed,"origin.z")-10) < 0.01,
                        "Direct face primitive must be placed on the clicked face");
                viewport->SetTool(ToolMode::Select);
            } else {
                QTimer::singleShot(0, &window, [&] {
                    QDialog* dialog = nullptr;
                    for (auto* candidate : window.findChildren<QDialog*>())
                        if (candidate->windowTitle() == "New Sketch" && candidate->isVisible()) dialog = candidate;
                    require(dialog, "New sketch plane dialog missing");
                    click(*viewport, {x+15,65,10});
                    require(dialog->result() == QDialog::Accepted && !dialog->isVisible(),
                            "Body face click must accept sketch creation without OK");
                });
                action("NewSketch")->trigger();
                require(viewport->CurrentTool() == ToolMode::SketchRectangle,
                        "Body face click must immediately start sketch drawing");
                auto drawing_camera = viewport->GetCamera();
                drawing_camera.target = {x+15,65,10};
                drawing_camera.distance = 150;
                viewport->SetCamera(drawing_camera);
                click(*viewport, {x+5,55,10});
                click(*viewport, {x+20,70,10});
                const auto* sketch = dynamic_cast<const CSketch*>(window.document_.GetSelectedObject());
                require(sketch && std::abs(sketch->GetCoordinateSystem().origin.z - 10) < 0.01,
                        "Direct face click must immediately create a sketch on that face");
                require(sketch->GetFaceAttachment().body_id == body_id
                        && sketch->GetFaceAttachment().face_index >= 0,
                        "Direct face sketch must retain its body/face attachment");
                viewport->EndSketch();
            }
        }
        {
            CAlfaDoc inclined_document;
            OpenGLViewport inclined;
            inclined.SetDocument(&inclined_document);
            inclined.resize(1000,800);
            const Vec3 origin{12345.125f, -3456.75f, 789.375f};
            const Vec3 u = normalize(Vec3{1,2,0});
            const Vec3 normal = normalize(Vec3{2,-1,3});
            const Vec3 v = normalize(cross(normal,u));
            inclined.BeginSketchOnFace("Inclined rectangle", origin, u, v, normal, 0, -1);
            auto view = inclined.GetCamera(); view.distance = 1000;
            inclined.SetCamera(view);
            const auto before = inclined_document.GetObjects().size();
            click(inclined, origin + u*35 + v*45);
            click(inclined, origin + u*230 + v*175);
            require(inclined_document.GetObjects().size() == before + 1,
                    "Second rectangle click on a translated inclined plane must create a contour");
            const auto* rectangle = dynamic_cast<const CSketch*>(inclined_document.GetSelectedObject());
            require(rectangle && rectangle->GetContourCount()==1 && rectangle->GetLocalContour(0).IsClosed(),
                    "Face rectangle must form a closed contour in CSketch");
        }
        std::cout << "Reference plane selection passed for Box, Cylinder and dialog mode in both projections\n";
        return 0;
    }
    for (bool cylinder : {false, true}) {
        const char* key = cylinder ? "SolidCylinder" : "SolidBox";
        const ToolMode mode = cylinder ? ToolMode::SolidCylinderCircle : ToolMode::SolidBoxRectangle;
        for (int plane : {0, 1, 2}) {
            std::cerr << key << ": live plane " << plane << '\n';
            action(key)->trigger();
            auto* options = window.findChild<QDialog*>("SolidPlacementOptions");
            auto* combo = window.findChild<QComboBox*>("SolidPlacementType");
            require(options && combo && options->isVisible() && !options->isModal()
                && options->findChildren<QDialogButtonBox*>().isEmpty()
                && viewport->CurrentTool() == mode,
                "Primitive must start drawing immediately without OK");
            combo->setCurrentIndex(0);
            // This regression exercises explicit coordinate-plane placement,
            // independent of the automatic face choice tested above.
            viewport->SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY, false);
            completed.clear();
            click(*viewport, {8, 12, 0});
            require(completed.empty(), "First click must only set the anchor");
            combo->setCurrentIndex(plane);
            require(window.statusBar()->currentMessage().contains(cylinder ? "radius" : "opposite"),
                "Changing plane must retain the first click");
            click(*viewport, plane == 0 ? Vec3{18, 22, 0}
                : plane == 1 ? Vec3{18, 0, 6} : Vec3{0, 22, 6}, true);
            require(!completed.empty() && !options->isVisible(),
                "Height confirmation must finish creation and hide placement options");
            const char* normal = plane == 0 ? "axis.n.z" : plane == 1 ? "axis.n.y" : "axis.n.x";
            const char* zero_coordinate = plane == 0 ? "origin.z" : plane == 1 ? "origin.y" : "origin.x";
            require(parameter(completed, normal) == 1
                && std::fabs(parameter(completed, zero_coordinate)) < 1e-5,
                "Saved placement must use the changed plane");
            require(std::fabs(parameter(completed, plane == 2 ? "origin.y" : "origin.x")
                - (plane == 2 ? 12.0 : 8.0)) < 0.5,
                "First click must be projected to the new plane");
            bool saved = false;
            for (auto* panel : window.findChildren<PropertyPanel*>()) {
                const auto& active = panel->ActiveObject();
                if (active.tool_id == key && !active.parameters.empty()) {
                    saved = parameter(active.parameters, normal) == 1;
                }
            }
            if (cylinder) for (auto* panel : window.findChildren<PropertyPanel*>()) {
                if (panel->ActiveObject().tool_id != key) continue;
                for (const char* id : {"diameter", "height"}) for (bool base : {false, true}) {
                    const auto before = panel->ActiveObject();
                    const double old_value = parameter(before.parameters, id);
                    viewport->SolidDimensionGripChanged(int(before.operation_index), id, old_value + 2, false, base);
                    const auto after = panel->ActiveObject();
                    require(std::fabs(parameter(after.parameters,id)-old_value-2)<1e-8,"Cylinder handle did not resize");
                    const bool diameter = std::string(id) == "diameter";
                    const double shift = diameter ? (base ? -1 : 1) : (base ? -2 : 0);
                    for (const char* coord : {"x","y","z"}) {
                        const std::string origin = std::string("origin.")+coord;
                        const std::string axis = std::string(diameter ? "axis.u." : "axis.n.")+coord;
                        require(std::fabs(parameter(after.parameters,origin.c_str())-parameter(before.parameters,origin.c_str())
                            -shift*parameter(before.parameters,axis.c_str()))<1e-7,"Cylinder opposite side moved during resize");
                    }
                    viewport->SolidDimensionGripChanged(int(before.operation_index), id, old_value, true, base);
                }
            }
            require(saved, "Created solid must expose the final placement parameters");
        }
        auto* repeat = window.findChild<QAction*>("RepeatLastCommandAction");
        repeat->trigger();
        auto* options = window.findChild<QDialog*>("SolidPlacementOptions");
        auto* combo = window.findChild<QComboBox*>("SolidPlacementType");
        require(viewport->CurrentTool() == mode && options->isVisible() && combo->currentIndex() == 2,
            "Repeat must restart drawing with the last plane for this primitive");
        options->close();
        require(viewport->CurrentTool() == ToolMode::Select, "Closing placement must cancel drawing");
        action(key)->trigger();
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(viewport, &escape);
        require(!options->isVisible() && viewport->CurrentTool() == ToolMode::Select,
            "Escape before the first click must close placement");
        action(key)->trigger();
        action("select")->trigger();
        require(!options->isVisible(), "Switching tools must hide placement");
    }

    action("SolidBeamTool")->trigger();
    PropertyPanel* beam_panel = nullptr;
    for (auto* panel : window.findChildren<PropertyPanel*>())
        if (panel->ActiveObject().tool_id == "SolidBeamTool") beam_panel = panel;
    require(beam_panel, "Beam must expose its parametric object");
    for (const char* id : {"width", "height", "length", "thick"}) {
        const auto before = beam_panel->ActiveObject();
        const double value = parameter(before.parameters, id);
        viewport->SolidDimensionGripChanged(int(before.operation_index), id, value + 0.5, false, false);
        require(std::abs(parameter(beam_panel->ActiveObject().parameters, id) - value - 0.5) < 1.e-8,
            "Beam dimension grip did not update its parameter");
        viewport->SolidDimensionGripChanged(int(before.operation_index), id, value, true, false);
    }
    action("select")->trigger();

    // Face placement needs a target in the same document as the viewport.
    CAlfaDoc document;
    TopoDS_Shape shape = BRepPrimAPI_MakeBox(30, 30, 10).Shape();
    auto solid = std::make_unique<CSolid>(shape);
    require(solid->ReBuldMesh(), "Could not mesh placement body");
    document.AddObject(std::move(solid), false);
    const auto body_id = document.GetObjects().back()->m_id;
    OpenGLViewport face_viewport;
    face_viewport.SetDocument(&document);
    face_viewport.resize(1000, 800);
    camera.orientation = {1, 0, 0, 0};
    face_viewport.SetCamera(camera);
    face_viewport.SetOrthographicProjection(true);
    QObject::connect(&face_viewport, &OpenGLViewport::SolidBoxRectangleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    QObject::connect(&face_viewport, &OpenGLViewport::SolidCylinderCircleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    for (bool cylinder : {false, true}) {
        completed.clear();
        if (cylinder) face_viewport.BeginSolidCylinderCircle(OpenGLViewport::SketchPlane::XY);
        else face_viewport.BeginSolidBoxRectangle(OpenGLViewport::SketchPlane::XY);
        face_viewport.SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY, false);
        click(face_viewport, {8, 12, 0});
        face_viewport.SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY, true);
        click(face_viewport, {15, 15, 10});
        require(completed.empty(), "Face click must only choose the placement face");
        click(face_viewport, {18, 22, 10}, true);
        require(!completed.empty()
            && parameter(completed, "boolean.body_id") == body_id
            && std::fabs(parameter(completed, "origin.z") - 10) < 1e-5,
            "Face placement must preserve the anchor and record its target body");
        require(std::fabs(parameter(completed, "boolean.overlap")
            - 0.001) < 1e-6,
            "Face overlap must be fixed at 0.001 model units");
        {
            const char* height_id = cylinder ? "height" : "depth";
            const double initial_height=parameter(completed,height_id);
            Camera zoomed=camera;zoomed.distance*=2;
            face_viewport.SetCamera(zoomed);
            if(cylinder) face_viewport.BeginSolidCylinderFaceSelection();
            else face_viewport.BeginSolidBoxFaceSelection();
            click(face_viewport,{15,15,10});
            click(face_viewport,{8,12,10});
            click(face_viewport,{18,22,10}, true);
            require(std::fabs(parameter(completed,height_id)-2*initial_height)<1e-5,
                "The same height gesture must scale with the viewport");
            const double chosen_height=parameter(completed,height_id);
            face_viewport.SetCamera(camera);
            require(parameter(completed,height_id)==chosen_height,"Camera changed an already chosen height");
        }
    }
    std::cout << "Box and Cylinder immediate drawing and live placement tests passed.\n";
    return 0;
}

int TestFacePrimitiveCut(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    if(app.arguments().contains("--orbit-dimensions")) {
        CAlfaDoc local_document; CAlfaDoc* document=&local_document;
        ToolRegistry registry; OpenGLViewport widget; auto* viewport=&widget;
        viewport->SetDocument(document);viewport->resize(1000,800);
        viewport->move(-20000,-20000);viewport->show();
        viewport->SetOrthographicProjection(true);
        Camera view;view.orientation=quaternion_from_axis_angle({0,0,1},-.785398f)
            *quaternion_from_axis_angle({1,0,0},.95f);
        document->GetObjects().clear();
        auto beam_parameters = registry.Find("SolidBeamTool")->defaults;
        for (auto& p : beam_parameters) if(p.id=="length") p.value=40;
        const auto beam = registry.CreateParametricObject("SolidBeamTool", *document, beam_parameters);
        view.target={0,0,20}; view.distance=140;
        viewport->SetCamera(view); viewport->SetTool(ToolMode::Orbit);
        viewport->SetSolidDimensionEdit(beam,"length");
        app.processEvents(); viewport->grabFramebuffer();
        QtSceneRenderer projector; DomPoint start{},end{};
        require(projector.WorldToScreen({14,-10,0},view,true,viewport->width(),viewport->height(),start)
            && projector.WorldToScreen({14,-10,40},view,true,viewport->width(),viewport->height(),end),
            "Cannot project beam dimension");
        const QPointF anchor(end.x,end.y);
        const auto mouse = [&](QEvent::Type type,QPointF pos,Qt::MouseButton button,Qt::MouseButtons buttons) {
            QMouseEvent event(type,pos,pos,button,buttons,Qt::NoModifier);
            QApplication::sendEvent(viewport,&event);
        };
        mouse(QEvent::MouseMove,anchor,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseMove,anchor+QPointF(1,0),Qt::NoButton,Qt::NoButton);
        require(viewport->cursor().shape()==Qt::OpenHandCursor,"Orbit reset dimension hover cursor");
        int dimension_changes=0;
        const auto connection=QObject::connect(viewport,&OpenGLViewport::SolidDimensionGripChanged,
            [&](int,const QString& id,double,bool,bool){if(id=="length")++dimension_changes;});
        const auto before=viewport->GetCamera().orientation;
        const QPointF direction(end.x-start.x,end.y-start.y);
        mouse(QEvent::MouseButtonPress,anchor,Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,anchor+direction*0.1,Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,anchor+direction*0.1,Qt::LeftButton,Qt::NoButton);
        const auto after=viewport->GetCamera().orientation;
        require(dimension_changes>0,"Orbit consumed dimension drag");
        require(before.w==after.w&&before.x==after.x&&before.y==after.y&&before.z==after.z,
            "Dragging dimension rotated camera");
        QObject::disconnect(connection);
        mouse(QEvent::MouseButtonPress,{20,20},Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,{65,40},Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,{65,40},Qt::LeftButton,Qt::NoButton);
        const auto rotated=viewport->GetCamera().orientation;
        require(after.w!=rotated.w||after.x!=rotated.x||after.y!=rotated.y||after.z!=rotated.z,
            "Orbit stopped working outside dimensions");
        viewport->ClearSolidDimensionEdit();
        std::cout<<"Orbit dimension hover, drag and background navigation passed.\n";
        return 0;
    }

    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("Dom3DPrimitiveTests");
    QCoreApplication::setApplicationName("FacePrimitiveCutTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    MainWindow window;
    CAlfaDoc* document = GetAlfaDoc();
    const bool render = app.arguments().contains("--render");
    if (render) {
        window.resize(1200, 1000);
        window.move(-20000, -20000);
        window.show();
        app.processEvents();
    }
    require(document, "Missing UI document");
    document->GetObjects().clear();
    ToolRegistry registry;
    auto host_parameters = registry.Find("SolidBox")->defaults;
    for (auto& p : host_parameters) {
        if (p.id == "width" || p.id == "height") p.value = 60;
        if (p.id == "depth") p.value = 50;
    }
    const auto host = registry.CreateParametricObject("SolidBox", *document, host_parameters);
    const auto host_id = document->GetObjects()[host.object_index]->m_id;
    auto* viewport = window.findChild<OpenGLViewport*>();
    viewport->resize(1000, 800);
    Camera camera;
    camera.orientation = {1, 0, 0, 0};
    camera.target = {30, 30, 25};
    camera.distance = 150;
    viewport->SetCamera(camera);
    viewport->SetOrthographicProjection(true);
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("toolKey").toString() == "SolidBox") { action->trigger(); break; }
    }
    window.findChild<QComboBox*>("SolidPlacementType")->setCurrentIndex(3);
    click(*viewport, {30, 30, 50});
    click(*viewport, {0, 0, 50});
    click(*viewport, {17.892f, 22.259f, 50}, true);
    PropertyPanel* panel = nullptr;
    for (auto* candidate : window.findChildren<PropertyPanel*>()) {
        if (candidate->ActiveObject().tool_id == "SolidBox") { panel = candidate; break; }
    }
    require(panel, "No Box parameters after drawing on face");
    // Both handles resize the box. The base handle must hold the opposite
    // plane fixed by changing the origin together with the dimension.
    for (const auto& pair : {std::pair<const char*,const char*>{"width","axis.u."}, {"height","axis.v."}, {"depth","axis.n."}}) {
        const auto initial=panel->ActiveObject();
        const double original=parameter(initial.parameters,pair.first);
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original+2.0,false,false);
        for(const char* coord:{"x","y","z"}) {
            const std::string id=std::string("origin.")+coord;
            require(std::fabs(parameter(panel->ActiveObject().parameters,id.c_str())-parameter(initial.parameters,id.c_str()))<1e-8,
                "Far handle moved the box origin");
        }
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original,true,false);
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original+2.0,false,true);
        const auto shifted=panel->ActiveObject();
        require(std::fabs(parameter(shifted.parameters,pair.first)-original-2.0)<1e-8,"Base handle did not resize the box");
        for(const char* coord:{"x","y","z"}) {
            const std::string origin=std::string("origin.")+coord,axis=std::string(pair.second)+coord;
            require(std::fabs(parameter(shifted.parameters,origin.c_str())+2.0*parameter(initial.parameters,axis.c_str())
                -parameter(initial.parameters,origin.c_str()))<1e-8,"Base handle moved the opposite box plane");
        }
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original,true,true);
    }

    panel->UpdateParameterValue("depth", -8.571);
    panel->ParametersChanged();
    const auto volume = [](const CSolid& solid) {
        GProp_GProps p;
        BRepGProp::VolumeProperties(solid.m_Shape, p);
        return p.Mass();
    };
    const auto check_mesh = [](const CSolid& solid, const char* stage) {
        bool valid = true;
        for (int i = 0; i < solid.GetNumSurfaces(); ++i) {
            auto* surface = solid.GetSurfaceFace(i);
            require(surface && surface->pMesh3D, "Missing rendered surface");
            GProp_GProps p;
            BRepGProp::SurfaceProperties(surface->m_Face, p);
            const auto& vertices = surface->pMesh3D->GetVertices();
            double area = 0;
            for (const auto& f : surface->pMesh3D->GetFaces()) {
                if (f.deleted) continue;
                const auto count = CMesh3D::FaceVertexCount(f);
                const Vec3 a = vertices[CMesh3D::GetFaceVertexIndex(f, 0)];
                for (size_t k = 1; k + 1 < count; ++k) {
                    const Vec3 b = vertices[CMesh3D::GetFaceVertexIndex(f, k)];
                    const Vec3 c = vertices[CMesh3D::GetFaceVertexIndex(f, k + 1)];
                    const Vec3 n = cross(b - a, c - a);
                    area += 0.5 * std::sqrt(dot(n, n));
                }
            }
            std::cerr << stage << " face " << i << " CAD=" << p.Mass() << " mesh=" << area << '\n';
            valid = valid && std::fabs(area - p.Mass()) < std::max(0.01, p.Mass() * 1e-5);
        }
        return valid;
    };
    auto* body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
    require(body && volume(*body) < 180000, "Negative height must cut the host in preview");
    const double preview_volume = volume(*body);
    const auto capture = [&](const char* stage) {
        if (!render) return;
        Camera view = camera;
        view.orientation = quaternion_from_axis_angle({0, 0, 1}, -0.785398f)
            * quaternion_from_axis_angle({1, 0, 0}, 0.95f);
        viewport->SetCamera(view);
        CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceColored);
        viewport->update();
        app.processEvents();
        if (std::string(stage)=="preview") {
            const auto frame=viewport->grabFramebuffer();
            require(frame.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/box-handles.png"),"Cannot capture dimension handles");
            unsigned yellow=0;
            for(int y=60;y<frame.height();++y) for(int x=0;x<frame.width();++x) {
                const auto c=frame.pixelColor(x,y);
                if(c.red()>200 && c.green()>150 && c.blue()<70)++yellow;
            }
            require(yellow>100,"Filled box arrows are not visible in the GL framebuffer");
        }
        require(viewport->CaptureSceneImage(QSize(900, 900)).save(
            QString("C:/My_projects/Dom3D_Pro/tmp/boolean-tool/face-cut-%1.png").arg(stage)),
            "Could not capture face cut");
    };
    capture("preview");
    require(check_mesh(*body, "preview"), "Preview mesh must match the trimmed faces");
    panel->Accepted();
    body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
    require(body && document->GetObjects().size() == 1
        && std::fabs(volume(*body) - preview_volume) < 0.01,
        "Commit must keep the preview geometry and remove its cutter");
    capture("commit");
    require(check_mesh(*body, "commit"), "Committed mesh must match the trimmed faces");
    if (app.arguments().contains("--quadro")) {
        body->MeshQuadro = true;
        body->MeshQuadroHoleSLX = true;
        require(body->ReBuldMesh(2.0f), "Could not rebuild the cut in Quadro mode");
        capture("quadro");
        require(check_mesh(*body, "quadro"), "Quadro mesh must match the trimmed faces");
    }
    if (render) {
        document->GetObjects().clear();
        TopoDS_Shape shape=BRepPrimAPI_MakeBox(20,20,20).Shape();
        auto block=std::make_unique<CSolid>(shape);require(block->ReBuldMesh(),"Cannot mesh fillet arrow fixture");
        document->AddObject(std::move(block));
        Camera view=camera;view.target={10,10,10};view.distance=65;
        view.orientation=quaternion_from_axis_angle({0,0,1},-0.785398f)*quaternion_from_axis_angle({1,0,0},0.95f);
        viewport->ClearSolidDimensionEdit();viewport->SetCamera(view);
        viewport->SetTool(ToolMode::Select);viewport->SetSelectionMode(SelectionMode::Edge);
        click(*viewport,{7,0,20});
        require(document->HasSelectedSolidEdge(),"Cannot pick fillet arrow fixture edge");
        require(document->BeginLiveFilletSelectedEdges(false)&&document->UpdateLiveFillet(1),"Cannot build fillet arrow fixture");
        ActiveParametricObject fillet;fillet.tool_id="fillet_edge";fillet.object_index=0;
        fillet.parameters=registry.Find("fillet_edge")->defaults;
        for(auto& p:fillet.parameters)if(p.id=="radius")p.value=1;
        viewport->SetSolidDimensionEdit(fillet,"radius");
        app.processEvents();
        const auto frame=viewport->grabFramebuffer();
        require(frame.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/fillet-arrow.png"),"Cannot capture fillet arrow");
        DomPoint pick;QtSceneRenderer projector;
        require(projector.WorldToScreen({7,0,20},view,true,viewport->width(),viewport->height(),pick),"Cannot project picked edge");
        bool yellow=false;
        for(int y=std::max(0,pick.y-5);y<std::min(frame.height(),pick.y+6);++y)
            for(int x=std::max(0,pick.x-5);x<std::min(frame.width(),pick.x+6);++x){auto c=frame.pixelColor(x,y);yellow|=c.red()>245&&c.green()>200&&c.blue()<20;}
        require(yellow,"Fillet arrow does not start at the picked edge");
        const auto tip = [&](const QImage& image) {
            QPointF result;double farthest=0;
            for(int y=std::max(60,pick.y-150);y<std::min(image.height(),pick.y+151);++y)
                for(int x=std::max(0,pick.x-150);x<std::min(image.width(),pick.x+151);++x) {
                    const auto c=image.pixelColor(x,y);
                    if(c.red()>245&&c.green()>200&&c.blue()<20) {
                        const double distance=std::hypot(x-pick.x,y-pick.y);
                        if(distance>farthest){farthest=distance;result=QPointF(x-pick.x,y-pick.y);}
                    }
                }
            return result;
        };
        const auto first_tip=tip(frame);
        DomPoint along;
        require(projector.WorldToScreen({8,0,20},view,true,viewport->width(),viewport->height(),along),"Cannot project edge tangent");
        const double tx=along.x-pick.x,ty=along.y-pick.y;
        require(std::fabs(first_tip.x()*ty-first_tip.y()*tx)/std::hypot(tx,ty)<3,
            "Radius handle does not follow the picked edge tangent");
        // A one-unit radius cannot project longer than one model unit in
        // this orthographic view, regardless of the chosen direction.
        const double pixels_per_unit=viewport->height()/(2.0*view.distance*0.42);
        require(std::hypot(first_tip.x(),first_tip.y())<=pixels_per_unit+3,
            "Radius arrow uses an artificial scale instead of model units");
        require(document->UpdateLiveFillet(2),"Cannot enlarge fillet fixture");
        for(auto& p:fillet.parameters)if(p.id=="radius")p.value=2;
        viewport->SetSolidDimensionEdit(fillet,"radius");app.processEvents();
        const auto larger=viewport->grabFramebuffer();
        const auto second_tip=tip(larger);
        const double first_length=std::hypot(first_tip.x(),first_tip.y()),second_length=std::hypot(second_tip.x(),second_tip.y());
        std::cerr<<"Arrow lengths "<<first_length<<" "<<second_length<<"\n";
        require(first_length>1 && std::fabs(second_length-2*first_length)<4,
            "Radius arrow endpoint did not move with the radius");
        require(std::fabs(first_tip.x()*second_tip.y()-first_tip.y()*second_tip.x())/second_length<4,
            "Radius arrow direction jumped after rebuilding the fillet");
        require(larger.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/fillet-arrow-larger.png"),"Cannot capture enlarged radius handle");

        document->CancelLiveFillet();viewport->ClearSolidDimensionEdit();
        {
        click(*viewport,{7,0,20});
        require(document->HasSelectedSolidEdge(),"Cannot pick fillet arrow fixture edge");
        require(document->BeginLiveChamferSelectedEdges()&&document->UpdateLiveChamfer(1),"Cannot build fillet arrow fixture");
        ActiveParametricObject fillet;fillet.tool_id="ChamferSolid";fillet.object_index=0;
        fillet.parameters=registry.Find("ChamferSolid")->defaults;
        for(auto& p:fillet.parameters)if(p.id=="distance")p.value=1;
        viewport->SetSolidDimensionEdit(fillet,"distance");
        app.processEvents();
        const auto frame=viewport->grabFramebuffer();
        require(frame.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/chamfer-arrow.png"),"Cannot capture fillet arrow");
        DomPoint pick;QtSceneRenderer projector;
        require(projector.WorldToScreen({7,0,20},view,true,viewport->width(),viewport->height(),pick),"Cannot project picked edge");
        bool yellow=false;
        for(int y=std::max(0,pick.y-5);y<std::min(frame.height(),pick.y+6);++y)
            for(int x=std::max(0,pick.x-5);x<std::min(frame.width(),pick.x+6);++x){auto c=frame.pixelColor(x,y);yellow|=c.red()>245&&c.green()>200&&c.blue()<20;}
        require(yellow,"Chamfer arrow does not start at the picked edge");
        const auto tip = [&](const QImage& image) {
            QPointF result;double farthest=0;
            for(int y=std::max(60,pick.y-150);y<std::min(image.height(),pick.y+151);++y)
                for(int x=std::max(0,pick.x-150);x<std::min(image.width(),pick.x+151);++x) {
                    const auto c=image.pixelColor(x,y);
                    if(c.red()>245&&c.green()>200&&c.blue()<20) {
                        const double distance=std::hypot(x-pick.x,y-pick.y);
                        if(distance>farthest){farthest=distance;result=QPointF(x-pick.x,y-pick.y);}
                    }
                }
            return result;
        };
        const auto first_tip=tip(frame);
        DomPoint along;
        require(projector.WorldToScreen({8,0,20},view,true,viewport->width(),viewport->height(),along),"Cannot project edge tangent");
        const double tx=along.x-pick.x,ty=along.y-pick.y;
        require(std::fabs(first_tip.x()*ty-first_tip.y()*tx)/std::hypot(tx,ty)<3,
            "Radius handle does not follow the picked edge tangent");
        // A one-unit radius cannot project longer than one model unit in
        // this orthographic view, regardless of the chosen direction.
        const double pixels_per_unit=viewport->height()/(2.0*view.distance*0.42);
        require(std::hypot(first_tip.x(),first_tip.y())<=pixels_per_unit+3,
            "Radius arrow uses an artificial scale instead of model units");
        require(document->UpdateLiveChamfer(2),"Cannot enlarge chamfer fixture");
        for(auto& p:fillet.parameters)if(p.id=="distance")p.value=2;
        viewport->SetSolidDimensionEdit(fillet,"distance");app.processEvents();
        const auto larger=viewport->grabFramebuffer();
        const auto second_tip=tip(larger);
        const double first_length=std::hypot(first_tip.x(),first_tip.y()),second_length=std::hypot(second_tip.x(),second_tip.y());
        std::cerr<<"Arrow lengths "<<first_length<<" "<<second_length<<"\n";
        require(first_length>1 && std::fabs(second_length-2*first_length)<4,
            "Radius arrow endpoint did not move with the radius");
        require(std::fabs(first_tip.x()*second_tip.y()-first_tip.y()*second_tip.x())/second_length<4,
            "Radius arrow direction jumped after rebuilding the fillet");
        require(larger.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/chamfer-arrow-larger.png"),"Cannot capture enlarged radius handle");

        double dragged_distance = 0;
        bool drag_finished = false;
        const auto drag_connection = QObject::connect(viewport, &OpenGLViewport::SolidDimensionGripChanged,
            &window, [&](int, const QString& id, double value, bool finished, bool) {
                if (id == "distance") { dragged_distance = value; drag_finished = finished; }
            });
        const QPointF direction = second_tip / second_length;
        const QPointF grip = QPointF(pick.x, pick.y) + second_tip - direction * 7.5;
        const QPointF moved = grip + second_tip;
        QMouseEvent press(QEvent::MouseButtonPress, grip, grip, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &press);
        QMouseEvent move(QEvent::MouseMove, moved, moved, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, moved, moved, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &release);
        QObject::disconnect(drag_connection);
        require(drag_finished && std::fabs(dragged_distance - 4.0) < 0.3,
            "Chamfer drag distance does not match the projected model scale");
        document->CancelLiveChamfer();viewport->ClearSolidDimensionEdit();
        }

        document->GetObjects().clear();
        TopoDS_Shape cylinder_shape = BRepPrimAPI_MakeCylinder(10,20).Shape();
        auto cylinder_body = std::make_unique<CSolid>(cylinder_shape);
        require(cylinder_body->ReBuldMesh(), "Cannot mesh cylinder handle fixture");
        document->AddObject(std::move(cylinder_body));
        ActiveParametricObject cylinder; cylinder.tool_id="SolidCylinder"; cylinder.object_index=0;
        cylinder.parameters=registry.Find("SolidCylinder")->defaults;
        for(auto& p:cylinder.parameters) {
            if(p.id=="diameter" || p.id=="height") p.value=20;
        }
        view.target={0,0,10};viewport->SetCamera(view);
        viewport->SetSolidDimensionEdit(cylinder,"height");app.processEvents();
        require(viewport->grabFramebuffer().save("C:/My_projects/Dom3D_Pro/output/box-union-failed/cylinder-handles.png"),
            "Cannot capture cylinder handles");
        viewport->ClearSolidDimensionEdit();
        document->GetObjects().clear();
        auto prism_parameters = registry.Find("SolidPrismTool")->defaults;
        for (auto& p : prism_parameters) {
            if (p.id == "length") p.value = 23;
            if (p.id == "height") p.value = 44;
            if (p.id == "axis") p.value = 1;
        }
        const auto prism = registry.CreateParametricObject("SolidPrismTool", *document, prism_parameters);
        view.target={0,22,0};view.distance=140;viewport->SetCamera(view);
        viewport->SetSolidDimensionEdit(prism,"height");app.processEvents();
        require(viewport->grabFramebuffer().save("C:/My_projects/Dom3D_Pro/output/box-union-failed/prism-handles.png"),
            "Cannot capture Prism handles");
        viewport->ClearSolidDimensionEdit();

    }
    std::cout << "Face primitive preview and committed mesh agree.\n";
    return 0;
}
