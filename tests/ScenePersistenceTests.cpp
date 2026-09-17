#include "CBSpline.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <QStatusBar>
#include "ui/MainWindow.h"
#include "ui/MaterialPreviewGL.h"
#include "CPolyline.h"
#include "CPart.h"
#include "solid/Solid.h"
#include "solid/AssociativeClone.h"
#include "solid/SurfaceSet.h"
#include "CMesh3D.h"
#include "SmartLine.h"
#include "Dom3DProjectSerializer.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>
#include "solid/SurfaceFace.h"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <GeomConvert.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <QApplication>
#include <QElapsedTimer>
#include <QAbstractButton>
#include <QCheckBox>
#include <QToolButton>
#include <QMenu>
#include <QDir>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <gp_Pln.hxx>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPointer>
#include <QWheelEvent>
#include <QAbstractItemView>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdlib>
#include <cmath>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void answer(QMessageBox::StandardButton button) {
    auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(dialog && dialog->button(button), "Expected message box button missing");
    dialog->button(button)->click();
}
}

int TestScenePersistence(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    if (application.arguments().contains("--bridge-defaults-only")) {
        QTemporaryDir settingsDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
        MainWindow window; auto& doc=window.document_; doc.GetObjects().clear();
        const auto add=[&](const char* name,CPoint3d start,CPoint3d end) {
            auto curve=std::make_unique<CBSpline>(name); curve->AddPoint(start); curve->AddPoint(end);
            doc.AddObject(std::move(curve)); return doc.GetSelectedObject()->m_id;
        };
        const auto first=add("First",{0,0,0},{100,0,0});
        const auto second=add("Second",{100,20,0},{0,5,0});
        window.undo_redo_.Reset();
        const auto invoke=[&](int a,int b,bool accept,bool overrideEnds=false) {
            doc.SelectObjectById(first); doc.SelectObjectById(second,SelectionAction::Add);
            QTimer::singleShot(0,[=] {
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                require(dialog && dialog->windowTitle()=="Bridge Curves","Bridge dialog missing");
                auto* left=dialog->findChild<QComboBox*>("BridgeFirstEndpoint");
                auto* right=dialog->findChild<QComboBox*>("BridgeSecondEndpoint");
                require(left && right && left->currentIndex()==a && right->currentIndex()==b,
                        "Bridge proposed occupied ends instead of free ends");
                if(overrideEnds) {left->setCurrentIndex(0);right->setCurrentIndex(1);}
                if(accept) dialog->accept(); else dialog->reject();
            });
            require(window.BridgeSelectedCurves()==accept,"Unexpected Bridge result");
        };
        invoke(0,1,true);
        require(doc.GetObjects().size()==3,"First bridge missing");
        invoke(1,0,false);
        invoke(1,0,true);
        require(doc.GetObjects().size()==4,"Second bridge missing");
        invoke(0,1,false); // Both pairs occupied: retain deterministic nearest default.
        require(window.undo_redo_.Undo(),"Cannot undo second bridge");
        invoke(1,0,false);
        require(window.undo_redo_.Redo(),"Cannot redo second bridge");
        invoke(0,1,false);
        require(window.undo_redo_.Undo(),"Cannot restore one-bridge state");
        const QString path=settingsDir.filePath("bridges.dom3d");
        Dom3DProjectSerializer serializer; ProjectViewState view; QString room,error;
        require(serializer.Save(path,doc,"Lines",view,{},error),"Cannot save bridge fixture");
        require(serializer.Load(path,doc,room,view,error),"Cannot reload bridge fixture");
        window.undo_redo_.Reset();
        invoke(1,0,false);
        invoke(1,0,true,true); // An explicit manual choice must still be honoured.
        const auto& parameters=doc.GetSelectedObject()->GetParametricParameters();
        for(const auto& p:parameters) {
            if(p.id=="curve1.end") require(p.value==0,"Manual first end ignored");
            if(p.id=="curve2.end") require(p.value==1,"Manual second end ignored");
        }
        std::cout<<"Bridge endpoint defaults checks passed.\n"; return 0;
    }
    if (application.arguments().contains("--hidden-grid-only")) {
        QTemporaryDir settingsDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
        CAlfaDoc document;
        OpenGLViewport viewport; viewport.resize(1000,700); viewport.SetDocument(&document);
        Camera camera=viewport.GetCamera(); camera.target={17,29,43}; camera.distance=300;
        const int fixtureIndex=application.arguments().indexOf("--camera-fixture");
        if(fixtureIndex>=0) {
            CAlfaDoc fixture; Dom3DProjectSerializer serializer; ProjectViewState view; QString room,error;
            require(serializer.Load(application.arguments().value(fixtureIndex+1),fixture,room,view,error),
                    "Cannot load curve snap camera fixture"); camera=view.camera;
        }
        viewport.SetCamera(camera); viewport.xy_plane_view_enabled_=false;
        viewport.SetFloorGridVisible(false); viewport.snapping_enabled_=true;
        using Target=OpenGLViewport::SnapTarget;
        using Kind=OpenGLViewport::SpatialCurvePreviewKind;
        const auto vec=[](CPoint3d p){return Vec3{float(p.x),float(p.y),float(p.z)};};
        for(bool ortho:{true,false}) for(int mode=0;mode<7;++mode) {
            viewport.SetOrthographicProjection(ortho);
            for(int i=0;i<int(Target::Count);++i) viewport.SetSnapTargetEnabled(static_cast<Target>(i),false);
            if(mode<6) viewport.SetSnapTargetEnabled(static_cast<Target>(mode==5?int(Target::Surface):mode),true);
            else for(Target t:{Target::Grid,Target::AuxLine,Target::AuxLine45,Target::Knot,Target::Line,Target::Surface})
                viewport.SetSnapTargetEnabled(t,true);
            CPoint3d grid(0,0,0); float distance=10000;
            require(!viewport.SnapSketchGridPoint(QPoint(500,350),{}, {1,0,0},{0,1,0},grid,distance),
                    "Hidden grid still captures points");
            for(Kind kind:{Kind::Polyline,Kind::BSpline,Kind::Bezier,Kind::Nurbs}) {
                viewport.SetTool(kind==Kind::Polyline?ToolMode::DrawCurve:ToolMode::DrawBSpline);
                viewport.BeginSpatialCurvePreview(kind);
                CPoint3d first; require(viewport.PickModelingPoint(QPoint(310,400),first),"Cannot place first node");
                std::vector<CPoint3d> points{first}; viewport.SetSpatialCurvePreviewPoints(points);
                const Vec3 normal=viewport.spatial_curve_plane_normal_;
                for(int i=1;i<25;++i) {
                    CPoint3d p;
                    require(viewport.PickModelingPoint(QPoint(310+i*14,400-int(50*std::sin(i*.14))),p),"Cannot place node");
                    require(std::abs(dot(vec(p)-vec(first),normal))<.002f,"Enabled snaps pulled free curve out of plane");
                    points.push_back(p);
                    if(i==12) {
                        const Vec3 off=vec(p)+normal*25.0f;
                        points.push_back(CPoint3d(off.x,off.y,off.z));
                    }
                    viewport.SetSpatialCurvePreviewPoints(points);
                }
                viewport.EndSpatialCurvePreview();
            }
            viewport.SetTool(ToolMode::DrawSpline);
            viewport.BeginDrawSplineStroke(QPoint(310,430));
            require(!viewport.draw_spline_raw_points_.empty(),"Pencil stroke did not start");
            const CPoint3d first=viewport.draw_spline_raw_points_.front();
            // Capture the same view normal without changing the pencil's points.
            viewport.BeginSpatialCurvePreview(Kind::BSpline); viewport.SetSpatialCurvePreviewPoints({first});
            const Vec3 normal=viewport.spatial_curve_plane_normal_; viewport.EndSpatialCurvePreview();
            for(int i=1;i<25;++i) viewport.AppendDrawSplineStroke(QPoint(310+i*14,430-int(55*std::sin(i*.15))));
            for(auto p:viewport.draw_spline_raw_points_)
                require(std::abs(dot(vec(p)-vec(first),normal))<.002f,"Enabled snaps pulled pencil stroke out of plane");
            viewport.CancelDrawSplineStroke();
        }
        require(document.GetObjects().size()<=2,"Hover created extra curves");
        viewport.SetFloorGridVisible(true); viewport.SetSnapTargetEnabled(Target::Grid,true);
        CPoint3d grid(camera.target.x,camera.target.y,camera.target.z); float distance=10000;
        require(viewport.SnapSketchGridPoint(QPoint(500,350),camera.target,{1,0,0},{0,1,0},grid,distance),
                "Visible grid snapping no longer works");
        std::cout<<"Hidden grid and curve snapping checks passed.\n"; return 0;
    }
    if (application.arguments().contains("--curve-plane-only")) {
        QTemporaryDir settingsDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
        CAlfaDoc document;
        OpenGLViewport viewport;
        viewport.resize(800,600);
        viewport.SetDocument(&document);
        viewport.xy_plane_view_enabled_ = false;
        viewport.snapping_enabled_ = false;
        Camera camera = viewport.GetCamera();
        camera.target = {0,0,0}; camera.distance = 100;
        viewport.SetCamera(camera);
        const auto vector = [](CPoint3d p) { return Vec3{float(p.x),float(p.y),float(p.z)}; };
        using Kind = OpenGLViewport::SpatialCurvePreviewKind;
        for (bool orthographic : {true,false}) for (Kind kind :
             {Kind::Polyline,Kind::BSpline,Kind::Bezier,Kind::Nurbs}) {
            viewport.SetCamera(camera);
            viewport.SetOrthographicProjection(orthographic);
            viewport.SetTool(kind == Kind::Polyline ? ToolMode::DrawCurve : ToolMode::DrawBSpline);
            viewport.BeginSpatialCurvePreview(kind);
            const CPoint3d first(3,7,11);
            viewport.SetSpatialCurvePreviewPoints({first});
            const Vec3 normal = viewport.spatial_curve_plane_normal_;
            // An off-plane snap must not move the plane for later free points.
            viewport.SetSpatialCurvePreviewPoints({first,CPoint3d(20,-15,32)});
            Camera moved = camera; moved.target = {9,-8,5};
            viewport.SetCamera(moved);
            for (const QPoint pixel : {QPoint(320,240),QPoint(480,330),QPoint(510,210)}) {
                CPoint3d free{};
                require(viewport.PickModelingPoint(pixel,free),"Curve plane projection failed");
                require(std::abs(dot(vector(free)-vector(first),normal)) < 1.e-4,
                        "Free curve point left first-point plane after snap or camera move");
            }
            viewport.BeginPick3DPoint("Next curve node");
            const QPoint pixel(490,320);
            QMouseEvent hover(QEvent::MouseMove,pixel,pixel,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&hover);
            require(viewport.curve_preview_valid_,"No curve preview");
            const CPoint3d preview=viewport.curve_preview_point_;
            bool committed=false;
            const auto connection=QObject::connect(&viewport,&OpenGLViewport::Point3DPicked,
                [&](CPoint3d p) { committed=true;
                    require(dot(vector(p)-vector(preview),vector(p)-vector(preview))<1.e-8,
                            "Click differs from preview"); });
            QMouseEvent click(QEvent::MouseButtonPress,pixel,pixel,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&click);
            QObject::disconnect(connection);
            require(committed,"Curve click was not committed");
            viewport.EndSpatialCurvePreview();
            require(!viewport.spatial_curve_plane_valid_,"Finished curve retained plane");
        }
        viewport.SetCamera(camera);
        viewport.SetOrthographicProjection(true);
        viewport.BeginSpatialCurvePreview(Kind::BSpline);
        viewport.SetSpatialCurvePreviewPoints({CPoint3d(3,7,11)});
        const Vec3 snapPosition = Vec3{3,7,11} + viewport.spatial_curve_plane_normal_*15.0f;
        auto target=std::make_unique<CPolyline>("Off-plane snap");
        target->AddPoint(CPoint3d(snapPosition.x,snapPosition.y,snapPosition.z));
        document.AddObject(std::move(target));
        viewport.snapping_enabled_=true;
        for(int i=0;i<int(OpenGLViewport::SnapTarget::Count);++i)
            viewport.SetSnapTargetEnabled(static_cast<OpenGLViewport::SnapTarget>(i),false);
        viewport.SetSnapTargetEnabled(OpenGLViewport::SnapTarget::Knot,true);
        DomPoint snapScreen{};
        require(viewport.renderer_.WorldToScreen(snapPosition,camera,true,800,600,snapScreen),"Cannot project snap target");
        CPoint3d snapped{};
        require(viewport.PickModelingPoint(QPoint(snapScreen.x,snapScreen.y),snapped)
                && dot(vector(snapped)-snapPosition,vector(snapped)-snapPosition)<1.e-8,
                "Locked drawing plane blocked off-plane node snap");
        viewport.EndSpatialCurvePreview();
        viewport.snapping_enabled_=false;
        viewport.BeginCurvePointDrag(CPoint3d(3,7,11));
        require(viewport.curve_point_drag_has_plane_,"Node drag has no locked plane");
        require(dot(viewport.curve_point_drag_plane_point_-Vec3{3,7,11},
                    viewport.curve_point_drag_plane_point_-Vec3{3,7,11})<1.e-8,
                "Node drag plane does not pass through node");
        std::cout << "Curve plane checks passed.\n"; return 0;
    }
    if (application.arguments().contains("--edge-tools-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_dir.path());
        for (const std::string tool:{"fillet_edge","ChamferSolid"}) {
            MainWindow window; auto& doc=window.document_;
            doc.GetObjects().clear();
            TopoDS_Shape shape=BRepPrimAPI_MakeBox(20,20,20).Shape();
            auto body=std::make_unique<CSolid>(shape);
            require(body->ReBuldMesh(),"Cannot mesh edge-tool test box");
            doc.AddObject(std::move(body));
            const auto id=doc.GetSelectedObject()->m_id;
            doc.ClearSelection();
            window.last_fillet_radius_=100; window.last_chamfer_distance_=100;
            window.ActivateParametricTool(tool);
            require(window.active_parametric_object_.tool_id==tool,"Empty selection closed edge tool");
            auto* all=window.property_panel_->findChild<QPushButton*>("AllEdgesButton");
            require(all,"Missing All Edges button");
            all->click();
            require(!doc.HasLiveFillet() && !doc.HasLiveChamfer(),"All Edges without selection changed geometry");
            doc.SelectObjectById(id); window.viewport_->SelectionChanged();
            require(!doc.HasLiveFillet() && !doc.HasLiveChamfer(),"Body selection started implicit all-edge preview");
            require(doc.GetSelectedSolid()->m_Shape.IsSame(shape),"Selecting a body altered geometry");
            all->click();
            require(tool=="fillet_edge"?doc.HasLiveFillet():doc.HasLiveChamfer(),"All Edges did not retain an invalid-size session");
            window.AcceptActiveProperties();
            require(window.active_parametric_object_.tool_id==tool,"Invalid-size OK closed edge tool");
            auto active=window.property_panel_->ActiveObject();
            for (auto& p:active.parameters) if(p.id=="radius" || p.id=="distance") p.value=1;
            window.property_panel_->SetActiveObject(active); window.property_panel_->ParametersChanged();
            require(!doc.GetSelectedSolid()->m_Shape.IsSame(shape),"Reducing size did not recover preview");
            window.AcceptActiveProperties();
            require(window.active_parametric_object_.tool_id.empty(),"Valid edge operation did not finish");
            require(window.undo_redo_.Undo(),"All-edge Undo failed");
            require(window.undo_redo_.Redo(),"All-edge Redo failed");
            // A selected body still waits when the tool is launched again.
            doc.SelectObjectById(id);
            window.ActivateParametricTool(tool);
            require(!doc.HasLiveFillet() && !doc.HasLiveChamfer(),"Relaunch on body started all edges");
            window.CancelActiveProperties();
            doc.GetObjects().clear();
            auto face_body=std::make_unique<CSolid>(shape);
            require(face_body->ReBuldMesh(),"Cannot prepare face-selection fixture");
            doc.AddObject(std::move(face_body));
            const auto face_id=doc.GetSelectedObject()->m_id;
            window.viewport_->SetSelectionMode(SelectionMode::Face);
            doc.SelectObjectById(face_id);
            doc.GetSelectedSolid()->SetSelectedFace(0);
            window.last_fillet_radius_=1; window.last_chamfer_distance_=1;
            window.ActivateParametricTool(tool);
            const auto face_edges=tool=="fillet_edge"?doc.GetLiveFilletEdgeRefs():doc.GetLiveChamferEdgeRefs();
            require(face_edges.size()==4,"Selected face did not restrict operation to its four edges");
            window.CancelActiveProperties();
            require(!doc.HasLiveFillet() && !doc.HasLiveChamfer(),"Cancel left an edge-tool session");
            doc.SelectObjectById(face_id);
            doc.GetSelectedSolid()->SetSelectedEdge(0,0);
            window.ActivateParametricTool(tool);
            const auto one_edge=tool=="fillet_edge"?doc.GetLiveFilletEdgeRefs():doc.GetLiveChamferEdgeRefs();
            require(one_edge.size()==1,"Selected edge was lost or expanded to all edges");
            window.CancelActiveProperties();
        }
        std::cout<<"Explicit all-edge selection and invalid-size recovery passed\n";
        return 0;
    }
    if (application.arguments().contains("--curve-release-profile") || application.arguments().contains("--curve-release-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_dir.path());
        MainWindow window;
        auto& doc=window.document_;
        Dom3DProjectSerializer serializer; ProjectViewState view; QString room,error;
        const bool regression=application.arguments().contains("--curve-release-only");
        const QString input=regression
            ? application.arguments().value(application.arguments().indexOf("--curve-release-only")+1)
            : qEnvironmentVariable("DOM3D_CURVE_RELEASE_INPUT");
        require(serializer.Load(input,doc,room,view,error),"Cannot load curve release fixture");
        unsigned long id=0;
        for (auto& object:doc.GetObjects()) if (auto* spline=dynamic_cast<CBSpline*>(object.get())) {
            std::cout<<"Spline "<<object->m_id<<" "<<object->GetName()<<" points "<<spline->GetPoints().size()<<std::endl;
            if (spline->GetPoints().size()>2) id=object->m_id;
        }
        require(id!=0,"No editable spline in fixture");
        doc.SelectObjectById(id);
        auto* spline=dynamic_cast<CBSpline*>(doc.FindObjectById(id));
        std::vector<TopoDS_Shape> before;
        for (const auto& object:doc.GetObjects()) {
            auto* solid=dynamic_cast<const CSolid*>(object.get());
            before.push_back(solid?solid->m_Shape:TopoDS_Shape{});
        }
        window.viewport_->CaptureCurvePointChangeBefore();
        auto p=spline->GetPoints()[1]; p.y+=0.1; spline->SetPoint(1,p);
        window.viewport_->FinalizeCurvePointChange();
        QElapsedTimer timer; timer.start();
        window.viewport_->DocumentChanged();
        std::cout<<"Curve release "<<id<<": "<<timer.elapsed()<<" ms; links "<<doc.GetCurveEndpointLinks().size()<<std::endl;
        for (size_t i=0;i<before.size();++i) if (!before[i].IsNull()) {
            auto* solid=dynamic_cast<CSolid*>(doc.GetObjects()[i].get());
            if(solid && !solid->m_Shape.IsSame(before[i])) {
                std::cout<<"Rebuilt "<<solid->m_id<<" "<<solid->GetName()<<std::endl;
                require(!regression,"Independent curve edit rebuilt unrelated CAD geometry");
            }
        }
        require(window.undo_redo_.Undo(),"Curve release Undo failed");
        require(window.undo_redo_.Redo(),"Curve release Redo failed");
        return 0;
    }
    if(application.arguments().contains("--extract-face-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_dir.path());
        MainWindow window;auto& doc=window.document_;
        doc.GetObjects().clear();
        TopoDS_Shape shape=BRepPrimAPI_MakeBox(20,30,40).Shape();
        auto source=std::make_unique<CSolid>(shape);
        require(source->ReBuldMesh(),"Cannot mesh extract fixture");
        doc.AddObject(std::move(source));
        const auto id=doc.GetSelectedObject()->m_id;
        window.undo_redo_.Reset();
        window.ActivateParametricTool("ExtractFaceTool");
        require(doc.GetObjects().size()==1 && window.pending_group_command_==MainWindow::PendingGroupCommand::ExtractFace,
            "Extract Face must wait for a pick");
        window.viewport_->SelectionCommandCanceled();
        require(window.pending_group_command_==MainWindow::PendingGroupCommand::None && doc.GetObjects().size()==1,"Extract cancellation failed");
        window.ActivateParametricTool("ExtractFaceTool");
        doc.SelectObjectById(id);
        require(doc.SelectSolidFaceAtScreen({200,250},[](Vec3 p,DomPoint& screen,float& depth) {
            screen={static_cast<int>(100+p.x*10),static_cast<int>(100+p.y*10)};depth=100-p.z;return true;
        }),"Cannot pick test face");
        const auto* picked=doc.GetSelectedFaceSolid();
        const auto originalFace=picked->GetSurfaceFace(picked->GetSelectedFaceIndex())->m_Face;
        window.viewport_->SelectionChanged();
        QCoreApplication::processEvents();
        require(doc.GetObjects().size()==2,"Face click did not extract exactly one surface");
        auto* extracted=dynamic_cast<CSurfaceSet*>(doc.GetSelectedObject());
        require(extracted && extracted->GetNumSurfaces()==1 && !extracted->m_Shape.IsSame(originalFace),"Extraction did not copy the face independently");
        require(doc.FindObjectById(id)->IsVisible(),"Extract hid source body");
        require(window.undo_redo_.Undo() && doc.GetObjects().size()==1,"Extract undo failed");
        require(window.undo_redo_.Redo() && doc.GetObjects().size()==2,"Extract redo failed");
        QSettings().remove("tools/SolidShell/lastAcceptedDistance");
        auto startShell=[&]() {
            window.ActivateParametricTool("SolidShell");
            require(window.pending_group_command_==MainWindow::PendingGroupCommand::ShellPick,"Shell did not request a face");
            require(doc.SelectSolidFaceAtScreen({200,250},[](Vec3 p,DomPoint& screen,float& depth) {
                screen={static_cast<int>(100+p.x*10),static_cast<int>(100+p.y*10)};depth=100-p.z;return true;
            }),"Cannot pick Shell face");
            window.viewport_->SelectionChanged();QCoreApplication::processEvents();
            require(window.active_parametric_object_.tool_id=="SolidShell","Shell pick did not open parameters");
        };
        auto distance=[&]() -> double& {
            for(auto& p:window.active_parametric_object_.parameters) if(p.id=="distance") return p.value;
            throw std::runtime_error("Missing Shell distance");
        };
        startShell();require(distance()==2,"Wrong initial Shell thickness");
        distance()=3;
        window.AcceptActiveProperties();
        require(QSettings().value("tools/SolidShell/lastAcceptedDistance").toDouble()==3,"Shell OK did not remember distance");
        startShell();require(distance()==3,"Shell did not reuse accepted distance");
        distance()=8;window.CancelActiveProperties();
        startShell();require(distance()==3,"Shell Cancel replaced the remembered distance");
        window.CancelActiveProperties();
        return 0;
    }
    if (application.arguments().contains("--panel-contour-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
        MainWindow window;
        auto& doc = window.document_;
        doc.GetObjects().clear();
        window.viewport_->SetXYView();
        TopoDS_Shape shape = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,100,0,100).Shape();
        auto source = std::make_unique<CSurfaceSet>(shape);
        require(source->ReBuldMesh(), "Cannot build panel UI fixture");
        doc.AddObject(std::move(source));
        const auto source_id = doc.GetSelectedObject()->m_id;
        auto curve = std::make_unique<CPolyline>("Panel contour");
        curve->AddPoint({-10,30,3}); curve->AddPoint({110,30,3});
        doc.AddObject(std::move(curve));
        const auto open_dialog = [&](bool accept) {
            QTimer::singleShot(0, [&window, accept, &application] {
                auto* dialog = window.findChild<QDialog*>("PanelFromContourDialog");
                require(dialog, "Panel dialog did not open");
                if (!accept) { dialog->reject(); return; }
                auto* gap = dialog->findChild<QDoubleSpinBox*>("PanelGap");
                auto* depth = dialog->findChild<QDoubleSpinBox*>("PanelDepth");
                auto* thickness = dialog->findChild<QDoubleSpinBox*>("PanelThickness");
                require(gap && depth && thickness && thickness->minimum()>0, "Panel distances missing");
                thickness->setValue(2.5);
                depth->setValue(-5);
                require(depth->value()==-5,"Panel dialog rejected outward displacement");
                gap->setValue(2); depth->setValue(5);
                if (application.arguments().contains("--capture-panel-ui")) {
                    QDir().mkpath("output/panel-contour");
                    require(dialog->grab().save("output/panel-contour/dialog.png"), "Cannot capture panel dialog");
                }
                dialog->accept();
            });
            window.CreatePanelFromContour();
        };
        open_dialog(false);
        require(doc.GetObjects().size()==2 && doc.FindObjectById(source_id)->IsVisible(), "Cancel changed panel sources");
        open_dialog(true);
        const auto* thickPanel=dynamic_cast<CSolid*>(doc.GetSelectedObject());
        const auto thickPanelId=doc.GetSelectedObject()->m_id;
        require(thickPanel && !dynamic_cast<const CSurfaceSet*>(thickPanel)
            && TopExp_Explorer(thickPanel->m_Shape,TopAbs_SOLID).More(),"Panel dialog produced a surface instead of a body");
        require(doc.GetObjects().size()==4 && !doc.FindObjectById(source_id)->IsVisible(), "Panel UI did not preserve a hidden source and two outputs");
        auto* panel = dynamic_cast<CSolid*>(doc.GetSelectedObject());
        require(panel && panel->GetParametricToolId()=="SolidContourPanel", "Panel UI lost parametric metadata");
        GProp_GProps properties; BRepGProp::VolumeProperties(panel->m_Shape,properties);
        require(std::abs(std::abs(properties.Mass())-2496*2.5)<1.e-3, "Panel UI ignored gap or thickness");
        require(window.undo_redo_.Undo() && doc.GetObjects().size()==2
                    && doc.FindObjectById(source_id)->IsVisible(), "Panel undo did not restore source body");
        require(window.undo_redo_.Redo() && doc.GetObjects().size()==4
                    && !doc.FindObjectById(source_id)->IsVisible(), "Panel redo lost outputs or source visibility");
        const auto contour_id=doc.GetObjects()[1]->m_id;
        auto peer=std::make_unique<CPolyline>("Unrelated linked contour end");
        peer->AddPoint({110,30,3}); peer->AddPoint({115,30,3}); peer->AddPoint({120,30,3});
        doc.AddObject(std::move(peer));
        const auto peer_id=doc.GetSelectedObject()->m_id;
        doc.SelectObjectById(contour_id,SelectionAction::Add);
        require(doc.LinkTouchingCurveEnds(0.001)==1,"Cannot link panel test contours");
        auto edited=std::make_unique<CBSpline>();
        edited->AddPoint({0,50,0}); edited->AddPoint({40,60,0}); edited->AddPoint({100,50,0});
        doc.AddObject(std::move(edited));
        const auto edited_id=doc.GetSelectedObject()->m_id;
        const auto unchanged_panel=dynamic_cast<CSolid*>(doc.GetObjects()[2].get())->m_Shape;
        auto* spline=dynamic_cast<CBSpline*>(doc.GetSelectedObject());
        window.viewport_->CaptureCurvePointChangeBefore();
        spline->SetPoint(1,{40,65,0});
        window.viewport_->FinalizeCurvePointChange();
        window.viewport_->DocumentChanged();
        require(dynamic_cast<CSolid*>(doc.GetObjects()[2].get())->m_Shape.IsSame(unchanged_panel),
            "Editing an independent spline rebuilt an unrelated linked panel");
        require(window.undo_redo_.Undo() && dynamic_cast<CBSpline*>(doc.FindObjectById(edited_id))->GetPoints()[1].y==60,
            "Scoped curve edit Undo failed");
        require(window.undo_redo_.Redo() && dynamic_cast<CBSpline*>(doc.FindObjectById(edited_id))->GetPoints()[1].y==65,
            "Scoped curve edit Redo failed");
        doc.SelectObjectById(peer_id);
        auto* linked=dynamic_cast<CPolyline*>(doc.FindObjectById(peer_id));
        window.viewport_->CaptureCurvePointChangeBefore();
        linked->SetPoint(1,{115,32,3});
        window.viewport_->FinalizeCurvePointChange();
        window.viewport_->DocumentChanged();
        require(dynamic_cast<CSolid*>(doc.GetObjects()[2].get())->m_Shape.IsSame(unchanged_panel),
            "An interior node unnecessarily rebuilt an endpoint peer's panel");
        window.viewport_->CaptureCurvePointChangeBefore();
        linked->SetPoint(0,{110,32,3});
        doc.SynchronizeCurveEndpointLinks();
        window.viewport_->FinalizeCurvePointChange();
        window.viewport_->DocumentChanged();
        require(!dynamic_cast<CSolid*>(doc.GetObjects()[2].get())->m_Shape.IsSame(unchanged_panel),
            "Moving a linked endpoint failed to update the dependent panel");
        require(window.undo_redo_.Undo()
            && dynamic_cast<CPolyline*>(doc.FindObjectById(contour_id))->GetPoints().back().y==30,
            "Linked endpoint Undo failed to restore its peer");
        require(window.undo_redo_.Redo()
            && dynamic_cast<CPolyline*>(doc.FindObjectById(contour_id))->GetPoints().back().y==32,
            "Linked endpoint Redo failed to restore its peer");
        doc.ClearSelection();doc.SelectObjectById(thickPanelId);
        window.ActivateParametricTool("SurfaceBulge");
        require(window.active_parametric_object_.tool_id=="SurfaceBulge","Bulge did not accept whole panel body");
        for(auto& p:window.active_parametric_object_.parameters) if(p.id=="height")p.value=2;
        require(window.tool_registry_.TryRebuildBulge(window.active_parametric_object_,doc),"Bulge on selected panel body failed");
        const auto* bulged=dynamic_cast<CSolid*>(doc.GetObjects()[window.active_parametric_object_.object_index].get());
        require(bulged && !dynamic_cast<const CSurfaceSet*>(bulged)
            && TopExp_Explorer(bulged->m_Shape,TopAbs_SOLID).More(),"Bulge result is not a body");
        std::cout << "Panel contour UI, cancel, undo and redo tests passed.\n";
        return 0;
    }
    if (application.arguments().contains("--bulge-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_dir.path());
        MainWindow window;
        auto& document=window.document_;
        document.GetObjects().clear();
        TopoDS_Shape shape=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,40,0,30).Face();
        auto source=std::make_unique<CSurfaceSet>(shape);
        require(source->ReBuldMesh(),"Cannot prepare bulge source");
        document.AddObject(std::move(source));
        window.undo_redo_.Reset();
        window.ActivateParametricTool("SurfaceBulge");
        require(window.active_parametric_object_.tool_id=="SurfaceBulge" && document.GetObjects().size()==2,
                "Bulge UI did not create its linked surface");
        for (auto& p:window.active_parametric_object_.parameters) if (p.id=="height") p.value=2;
        require(window.tool_registry_.TryRebuildBulge(window.active_parametric_object_,document),"Cannot update UI bulge");
        window.AcceptActiveProperties();
        require(window.undo_redo_.Undo() && document.GetObjects().size()==1,"Undo did not remove created bulge");
        require(window.undo_redo_.Redo() && document.GetObjects().size()==2,"Redo did not restore created bulge");
        document.ClearSelection();
        document.SelectObjectById(document.GetObjects().front()->m_id);
        window.ActivateParametricTool("SurfaceBulge");
        window.CancelActiveProperties();
        require(document.GetObjects().size()==2,"Cancel left an unfinished bulge");
        std::cout << "Bulge UI creation, accept, undo, redo and cancel passed.\n";
        return 0;
    }
    if (application.arguments().contains("--material-brush-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
        MainWindow window;
        auto& document = window.document_;
        auto& viewport = *window.viewport_;
        document.GetObjects().clear();
        viewport.resize(800, 600);
        viewport.SetXYView();
        viewport.SetOrthographicProjection(true);
        auto camera = viewport.GetCamera();
        camera.target = {0, 0, 0}; camera.distance = 100;
        viewport.SetCamera(camera);
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(gp_Pnt(-10,-10,-10),20,20,20).Shape();
        auto body = std::make_unique<CSolid>(shape);
        auto* solid = body.get();
        document.AddObject(std::move(body));
        require(solid->ReBuldMesh(), "Cannot prepare brush test body for screen picking");
        window.undo_redo_.Reset();
        int geometry_changes = 0;
        QObject::connect(&viewport, &OpenGLViewport::DocumentChanged, &viewport,
                         [&geometry_changes]() { ++geometry_changes; });
        std::vector<CMesh3D*> original_meshes;
        for (int i = 0; i < solid->GetNumSurfaces(); ++i)
            original_meshes.push_back(solid->GetSurfaceFace(i)->pMesh3D);
        const auto original_id = solid->GetMaterialId();
        const auto original_name = solid->GetMaterial().name;
        Material paint;
        paint.name = "Face brush regression";
        viewport.BeginMaterialPaint(paint);
        const QPoint center(viewport.width()/2, viewport.height()/2);
        require(viewport.FindObjectForMaterialAt(center) == solid, "Brush fixture is not under the cursor");
        QMouseEvent click(QEvent::MouseButtonPress, QPointF(center), QPointF(center),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&viewport, &click);
        size_t painted = 0;
        int painted_index = -1;
        for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
            const auto& material = solid->GetSurfaceFace(i)->MaterialOverride;
            if (material.enabled) {
                ++painted;
                painted_index = i;
                require(material.material.name == paint.name, "Brush assigned the wrong face material");
            }
        }
        require(painted == 1, "Brush must paint exactly the clicked face");
        require(solid->GetMaterialId() == original_id && solid->GetMaterial().name == original_name,
                "Brush changed the whole-body material");
        require(geometry_changes == 0, "Brush must not trigger scene geometry replay");
        require(window.undo_redo_.Undo(), "Cannot undo face painting");
        require(!solid->GetSurfaceFace(painted_index)->MaterialOverride.enabled,
                "Undo did not restore the inherited body material");
        require(window.undo_redo_.Redo(), "Cannot redo face painting");
        require(solid->GetSurfaceFace(painted_index)->MaterialOverride.enabled &&
                solid->GetSurfaceFace(painted_index)->MaterialOverride.material.name == paint.name,
                "Redo did not restore face painting");
        Material repaint;
        repaint.name = "Second face material";
        viewport.BeginMaterialPaint(repaint);
        QApplication::sendEvent(&viewport, &click);
        require(solid->GetSurfaceFace(painted_index)->MaterialOverride.material.name == repaint.name,
                "Repainting did not update the face");
        require(window.undo_redo_.Undo() &&
                solid->GetSurfaceFace(painted_index)->MaterialOverride.material.name == paint.name,
                "Undo did not restore the previous face override");
        require(geometry_changes == 0 && solid->m_Shape.IsSame(shape),
                "Painting or its undo changed geometry");
        for (int i = 0; i < solid->GetNumSurfaces(); ++i)
            require(solid->GetSurfaceFace(i)->pMesh3D == original_meshes[i],
                    "Painting rebuilt a display mesh");
        viewport.CancelMaterialInteraction();
        Material dropped;
        dropped.name = "Whole body drop regression";
        require(viewport.ApplyMaterialDrop(center, dropped), "Material drop failed");
        require(solid->GetMaterial().name == dropped.name, "Material drop no longer paints the body");
        std::cout << "Material brush paints one face; dropping a material paints the body.\n";
        return 0;
    }
    if (application.arguments().contains("--surface-snapping-only")) {
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
        QCoreApplication::setOrganizationName("Dom3D-SurfaceSnapTest");
        QCoreApplication::setApplicationName("SurfaceSnapTest");
        MainWindow window;
        auto& document = window.document_;
        auto& viewport = *window.viewport_;
        document.GetObjects().clear();
        viewport.resize(800, 600);
        viewport.SetXYView();
        viewport.SetOrthographicProjection(true);
        Camera camera = viewport.GetCamera();
        camera.target = {0, 0, 0}; camera.distance = 100;
        viewport.SetCamera(camera);
        TopoDS_Shape far_shape = BRepPrimAPI_MakeSphere(gp_Pnt(0,0,-15),10).Shape();
        auto far_sphere = std::make_unique<CSolid>(far_shape);
        document.AddObject(std::move(far_sphere));
        TopoDS_Shape near_shape = BRepPrimAPI_MakeSphere(gp_Pnt(0,0,10),8).Shape();
        auto near_sphere = std::make_unique<CSolid>(near_shape);
        auto* near_body = near_sphere.get();
        near_body->m_Shape.Reverse(); // Picking must be two-sided.
        document.AddObject(std::move(near_sphere));
        using Target = OpenGLViewport::SnapTarget;
        auto* surface_check = window.findChild<QCheckBox*>("SnapTarget6");
        require(surface_check && !surface_check->isChecked(), "Surface snapping checkbox missing or default enabled");
        surface_check->setChecked(true);
        require(viewport.IsSnapTargetEnabled(Target::Surface), "Surface checkbox does not control picking");
        OpenGLViewport restored;
        require(restored.IsSnapTargetEnabled(Target::Surface), "Surface snap preference was not restored");
        const QPoint center(viewport.width()/2, viewport.height()/2);
        CPoint3d point;
        for (bool orthographic : {true, false}) {
            viewport.SetOrthographicProjection(orthographic);
            require(viewport.PickModelingPoint(center, point) && std::abs(point.z - 18) < 1.e-4,
                    "Surface snap did not choose the nearest camera intersection");
            near_body->SetVisible(false);
            require(viewport.PickModelingPoint(center, point) && std::abs(point.z + 5) < 1.e-4,
                    "Surface snap hit a hidden body");
            near_body->SetVisible(true);
            require(viewport.PickModelingPoint(QPoint(0,0), point), "Surface snapping blocked drawing outside a surface");
            viewport.point_pick_object_id_ = near_body->m_id;
            require(!viewport.PickModelingPoint(QPoint(0,0), point), "Explicit body-only picking escaped the body");
            viewport.point_pick_object_id_ = 0;
            const QPoint offset = center + QPoint(12, 8);
            require(viewport.PickModelingPoint(offset, point), "Off-axis camera ray missed sphere");
            require(std::abs(point.x*point.x + point.y*point.y + (point.z-10)*(point.z-10) - 64) < 1.e-3
                    && point.z > 10, "Off-axis surface point is not on nearest sphere side");
        }
        // A trimmed face in front must not cover its hole with a UV rectangle.
        BRepBuilderAPI_MakePolygon outer, hole;
        for (auto p : {gp_Pnt(-15,-15,25), gp_Pnt(15,-15,25), gp_Pnt(15,15,25), gp_Pnt(-15,15,25)}) outer.Add(p);
        outer.Close();
        for (auto p : {gp_Pnt(-3,-3,25), gp_Pnt(-3,3,25), gp_Pnt(3,3,25), gp_Pnt(3,-3,25)}) hole.Add(p);
        hole.Close();
        BRepBuilderAPI_MakeFace ring(outer.Wire()); ring.Add(hole.Wire());
        TopoDS_Shape ring_shape = ring.Face();
        document.AddObject(std::make_unique<CSurfaceSet>(ring_shape));
        require(viewport.PickModelingPoint(center, point) && std::abs(point.z - 18) < 1.e-4,
                "Surface snapping ignored a face's trimming hole");
        viewport.SetTool(ToolMode::DrawBSpline);
        viewport.BeginSpatialCurvePreview(OpenGLViewport::SpatialCurvePreviewKind::BSpline);
        viewport.SetSpatialCurvePreviewPoints({CPoint3d(0,0,18)});
        viewport.BeginPick3DPoint("Next surface point");
        QMouseEvent move(QEvent::MouseMove, center, center, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&viewport, &move);
        require(viewport.curve_preview_valid_ && std::abs(viewport.curve_preview_point_.z - 18) < 1.e-4,
                "Curve preview does not follow the surface");
        bool picked = false;
        QObject::connect(&viewport, &OpenGLViewport::Point3DPicked, &viewport, [&](CPoint3d p) {
            picked = true; require(std::abs(p.z - 18) < 1.e-4, "Committed point differs from surface preview");
        });
        QMouseEvent click(QEvent::MouseButtonPress, center, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&viewport, &click);
        require(picked, "Surface point click was not committed");
        viewport.SetTool(ToolMode::DrawSpline);
        require(viewport.ScreenToCurvePlane(center, point) && std::abs(point.z - 18) < 1.e-4,
                "Freehand spline did not project to surface");
        require(viewport.ScreenToCurvePlane(QPoint(0,0), point), "Surface snapping blocked a freehand spline outside a surface");
        surface_check->setChecked(false);
        require(viewport.PickModelingPoint(QPoint(0,0), point), "Disabling surface snap did not restore free drawing");
        viewport.EndSpatialCurvePreview();
        viewport.SetTool(ToolMode::Select);
        document.GetObjects().clear();
        auto guide = std::make_unique<CPolyline>("Snap test line");
        guide->AddPoint({0,0,30}); guide->AddPoint({15,0,30});
        document.AddObject(std::move(guide));
        viewport.SetOrthographicProjection(true);
        for (int i = 0; i < static_cast<int>(Target::Count); ++i)
            viewport.SetSnapTargetEnabled(static_cast<Target>(i), false);
        require(!viewport.SnapCreationPoint(center, point, false), "Disabled targets still snap");
        viewport.SetSnapTargetEnabled(Target::Knot, true);
        require(viewport.SnapCreationPoint(center, point, false) && std::abs(point.z - 30) < 1.e-4,
                "Knot checkbox does not enable vertex snapping");
        viewport.SetSnapTargetEnabled(Target::Knot, false);
        viewport.SetSnapTargetEnabled(Target::Line, true);
        DomPoint line_pixel;
        QtSceneRenderer renderer;
        require(renderer.WorldToScreen({7,0,30}, viewport.GetCamera(), true, viewport.width(), viewport.height(), line_pixel), "Cannot project snap test line");
        require(viewport.SnapCreationPoint(QPoint(line_pixel.x,line_pixel.y), point, false)
                    && std::abs(point.z - 30) < 1.e-4 && point.x > 1 && point.x < 14,
                "Line checkbox does not enable curve snapping independently");
        // Surface boundaries participate in Line without extracted curves.
        document.GetObjects().clear();
        TopoDS_Shape snap_face = BRepBuilderAPI_MakeFace(
            gp_Pln(gp_Pnt(0,0,12),gp_Dir(0,0,1)),-15,15,-15,15).Face();
        auto edge_body = std::make_unique<CSurfaceSet>(snap_face);
        auto* edge_body_ptr = edge_body.get();
        document.AddObject(std::move(edge_body));
        for (bool ortho : {true,false}) for (bool surface : {false,true}) {
            viewport.SetOrthographicProjection(ortho);
            viewport.SetSnapTargetEnabled(Target::Surface,surface);
            require(renderer.WorldToScreen({15,2,12},viewport.GetCamera(),ortho,
                viewport.width(),viewport.height(),line_pixel),"Cannot project boundary");
            const QPoint near_edge(line_pixel.x-3,line_pixel.y);
            require(viewport.PickModelingPoint(near_edge,point) && std::abs(point.x-15)<1.e-7
                && std::abs(point.z-12)<1.e-7,"Line did not snap exactly to the surface boundary");
            require(viewport.ScreenToCurvePlane(near_edge,point) && std::abs(point.x-15)<1.e-7,
                "Freehand boundary snap differs from modeling snap");
        }
        viewport.SetSnapTargetEnabled(Target::Surface,false);
        viewport.SetOrthographicProjection(true);
        renderer.WorldToScreen({15,2,12},viewport.GetCamera(),true,viewport.width(),viewport.height(),line_pixel);
        edge_body_ptr->SetVisible(false);
        require(!viewport.SnapCreationPoint(QPoint(line_pixel.x,line_pixel.y),point,false),"Hidden surface edge snapped");
        edge_body_ptr->SetVisible(true);
        viewport.SetSnapTargetEnabled(Target::Line,false);
        require(!viewport.SnapCreationPoint(QPoint(line_pixel.x,line_pixel.y),point,false),"Disabled Line still snaps edges");
        document.GetObjects().clear();
        TopoDS_Shape cylinder_shape=BRepPrimAPI_MakeCylinder(15,12).Shape();
        document.AddObject(std::make_unique<CSolid>(cylinder_shape));
        viewport.SetSnapTargetEnabled(Target::Line,true);
        for (bool ortho : {true,false}) {
            viewport.SetOrthographicProjection(ortho);
            renderer.WorldToScreen({9,12,12},viewport.GetCamera(),ortho,viewport.width(),viewport.height(),line_pixel);
            require(viewport.SnapCreationPoint(QPoint(line_pixel.x+1,line_pixel.y+1),point,false)
                && std::abs(point.x*point.x+point.y*point.y-225)<1.e-7 && std::abs(point.z-12)<1.e-7,
                "Circle snap lies on a tessellation chord or on the hidden bottom rim");
        }
        viewport.SetOrthographicProjection(true);
        renderer.WorldToScreen({9,12,12},viewport.GetCamera(),true,viewport.width(),viewport.height(),line_pixel);
        TopoDS_Shape cover=BRepBuilderAPI_MakeFace(
            gp_Pln(gp_Pnt(0,0,25),gp_Dir(0,0,1)),-30,30,-30,30).Face();
        document.AddObject(std::make_unique<CSurfaceSet>(cover));
        require(!viewport.SnapCreationPoint(QPoint(line_pixel.x,line_pixel.y),point,false),
            "Occluded CAD edge snapped through a foreground face");
        document.GetObjects().clear();
        viewport.SetSnapTargetEnabled(Target::Line,false);
        renderer.WorldToScreen({7,0,30},viewport.GetCamera(),true,viewport.width(),viewport.height(),line_pixel);
        viewport.SetSnapTargetEnabled(Target::WorkPlane, true);
        viewport.sketch_origin_ = {0,0,6};
        require(viewport.PickModelingPoint(center, point) && std::abs(point.z - 6) < 1.e-4,
                "Work Plane checkbox does not select the current plane");
        viewport.SetSnapTargetEnabled(Target::Grid, true);
        require(viewport.SnapCreationPoint(center + QPoint(1,1), point, false)
                    && std::abs(point.x) < 1.e-4 && std::abs(point.y) < 1.e-4 && std::abs(point.z - 6) < 1.e-4,
                "Grid checkbox does not snap on the work plane");
        viewport.SetSnapTargetEnabled(Target::Grid, false);
        viewport.SetSnapTargetEnabled(Target::WorkPlane, false);
        viewport.SetSpatialCurvePreviewPoints({CPoint3d(0,0,30)});
        viewport.SetSnapTargetEnabled(Target::AuxLine, true);
        require(viewport.SnapCreationPoint(QPoint(line_pixel.x,line_pixel.y+1), point, false)
                    && std::abs(point.y) < 1.e-4 && std::abs(point.z - 30) < 1.e-4,
                "Auxiliary guide checkbox does not snap independently");
        viewport.SetSnapTargetEnabled(Target::AuxLine, false);
        viewport.SetSnapTargetEnabled(Target::AuxLine45, true);
        DomPoint diagonal_pixel;
        require(renderer.WorldToScreen({7,7,30}, viewport.GetCamera(), true, viewport.width(), viewport.height(), diagonal_pixel), "Cannot project diagonal guide");
        require(viewport.SnapCreationPoint(QPoint(diagonal_pixel.x+1,diagonal_pixel.y), point, false)
                    && std::abs(point.x-point.y) < 1.e-4 && std::abs(point.z-30) < 1.e-4,
                "45-degree guide checkbox does not snap independently");
        if (application.arguments().contains("--capture-snap-ui")) {
            for (int i = 0; i < static_cast<int>(Target::Count); ++i)
                viewport.SetSnapTargetEnabled(static_cast<Target>(i), i < 4 || i == 6);
            auto* button = window.findChild<QToolButton*>("SnappingTargets");
            require(button && button->menu(), "Snap dropdown missing");
            button->menu()->popup(QPoint(20,20));
            application.processEvents();
            QDir().mkpath("output/surface-snapping");
            require(button->menu()->grab().save("output/surface-snapping/menu.png"), "Cannot capture snap menu");
            button->menu()->hide();
        }
        std::cout << "Surface snapping tests passed.\n";
        return 0;
    }

    if (application.arguments().contains("--axes-clipping-only")) {
        CAlfaDoc document;
        OpenGLViewport viewport;
        viewport.SetDocument(&document);
        viewport.resize(640, 480);
        viewport.move(-20000, -20000);
        viewport.SetCoordinateAxesVisible(true);
        viewport.show();
        for (bool orthographic : {true, false}) {
            viewport.SetOrthographicProjection(orthographic);
            for (float distance : {1.0f, 100.0f, 1000.0f}) {
                auto camera = viewport.GetCamera();
                camera.target = {};
                camera.distance = distance;
                viewport.SetCamera(camera);
                int counts[2][3]{};
                for (int grid = 0; grid < 2; ++grid) {
                    viewport.SetFloorGridVisible(grid != 0);
                    application.processEvents();
                    const QImage frame = viewport.grabFramebuffer();
                    require(!frame.isNull(), "Axes framebuffer unavailable");
                    for (int y = 0; y < frame.height(); ++y) {
                        for (int x = 0; x < frame.width(); ++x) {
                            const QColor c = frame.pixelColor(x, y);
                            const int rgb[] = {c.red(), c.green(), c.blue()};
                            for (int axis = 0; axis < 3; ++axis) {
                                if (rgb[axis] > 120 && rgb[axis] > rgb[(axis+1)%3] * 2
                                    && rgb[axis] > rgb[(axis+2)%3] * 2) ++counts[grid][axis];
                            }
                        }
                    }
                }
                for (int axis = 0; axis < 3; ++axis) {
                    std::cout << "projection=" << orthographic << " distance=" << distance
                              << " axis=" << axis << " pixels=" << counts[0][axis]
                              << "/" << counts[1][axis] << std::endl;
                    require(counts[0][axis] > 30, "Axis disappeared with grid hidden at close zoom");
                    require(counts[1][axis] > 30, "Axis disappeared with grid visible at close zoom");
                }
            }
        }
        viewport.SetDocument(nullptr);
        std::cout << "Axes remain visible across zoom and grid toggles in both projections\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--revolve-tools-only")) {
        QTemporaryDir settings;
        QCoreApplication::setOrganizationName("Dom3DRevolveTests");
        QCoreApplication::setApplicationName("RevolveTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        MainWindow window;
        window.ActivateParametricTool("SurfaceRevolve");
        require(window.statusBar()->currentMessage().contains("Spline") &&
                window.statusBar()->currentMessage().contains("open or closed"), "Surface input help missing");
        window.ClearActiveProperties();
        window.ActivateParametricTool("SurfaceOfRevolution");
        require(window.statusBar()->currentMessage().contains("Sketch"), "Solid input help missing");
        window.ClearActiveProperties();
        auto spline = std::make_unique<CBSpline>();
        spline->AddPoint({10,0,0}); spline->AddPoint({12,0,10});
        spline->AddPoint({14,0,20}); spline->AddPoint({10,0,30});
        auto* source = spline.get();
        window.document_.AddObject(std::move(spline));
        window.document_.SelectObjectById(source->m_id,SelectionAction::Replace);
        window.ActivateParametricTool("SurfaceRevolve");
        require(window.document_.HasLivePolylineRevolve(), "Surface tool did not accept a spline");
        auto* surface = dynamic_cast<CSurfaceSet*>(window.document_.GetSelectedObject());
        require(surface && BRepCheck_Analyzer(surface->m_Shape).IsValid(), "Invalid spline revolve surface");
        require(!TopExp_Explorer(surface->m_Shape,TopAbs_SOLID).More(), "Surface revolve unexpectedly made a solid");
        require(window.document_.UpdateLiveRevolveSelectedPolyline(180,2), "Partial revolution failed");
        require(window.document_.FinishLiveRevolveSelectedPolyline(), "Surface revolve commit failed");
        const auto surface_id=window.document_.GetSelectedObject()->m_id;
        window.ClearActiveProperties();
        Vec3 before_min,before_max,after_min,after_max;
        window.document_.FindObjectById(surface_id)->GetBounds(before_min,before_max);
        source->Translate({10,0,0});
        require(window.tool_registry_.ReplayProfileDependents(source->m_id,window.document_), "Spline dependent replay failed");
        surface=dynamic_cast<CSurfaceSet*>(window.document_.FindObjectById(surface_id));
        require(surface && BRepCheck_Analyzer(surface->m_Shape).IsValid(), "Replay changed surface type or validity");
        surface->GetBounds(after_min,after_max);
        require(after_max.x>before_max.x+5, "Spline edit did not change revolved surface");
        Dom3DProjectSerializer serializer; ProjectViewState view; QString error,room;
        const auto path=settings.path()+"/revolve.dom3d";
        require(serializer.Save(path,window.document_,"Revolve",view,{},error),"Revolve save failed");
        CAlfaDoc restored; require(serializer.Load(path,restored,room,view,error),"Revolve load failed");
        require(dynamic_cast<CSurfaceSet*>(restored.FindObjectById(surface_id)),"Saved surface lost its type");
        require(window.tool_registry_.ReplayProfileDependents(source->m_id,restored),"Saved revolve replay failed");
        // Closed splines produce a periodic surface, never implicit solid caps.
        auto closed=std::make_unique<CBSpline>();
        closed->AddPoint({20,0,0}); closed->AddPoint({25,0,5});
        closed->AddPoint({20,0,10}); closed->AddPoint({15,0,5}); closed->SetClosed(true);
        window.document_.AddObject(std::move(closed));
        require(window.document_.BeginLiveRevolveSelectedPolyline(360,2,true),"Closed spline surface failed");
        require(dynamic_cast<CSurfaceSet*>(window.document_.GetSelectedObject()),"Closed spline created a solid");
        window.document_.CancelLiveRevolveSelectedPolyline();
        for (auto type : {SplineCurveType::Bezier, SplineCurveType::Nurbs}) {
            auto curve=std::make_unique<CBSpline>();
            curve->SetCurveType(type);
            curve->AddPoint({10,0,0}); curve->AddPoint({12,0,10});
            curve->AddPoint({15,0,20}); curve->AddPoint({10,0,30});
            window.document_.AddObject(std::move(curve));
            require(window.document_.BeginLiveRevolveSelectedPolyline(270,2,true),"Bezier/NURBS revolve failed");
            require(BRepCheck_Analyzer(window.document_.GetSelectedSolid()->m_Shape).IsValid(),"Invalid Bezier/NURBS revolve");
            window.document_.CancelLiveRevolveSelectedPolyline();
        }
        auto xy=std::make_unique<CSmartLine>();
        require(xy->CreateFromWorldPoints({{10,0,0},{10,20,0}},false,{},{1,0,0},{0,1,0}),"XY sketch failed");
        auto* xy_source=xy.get(); window.document_.AddObject(std::move(xy));
        window.ActivateParametricTool("SurfaceOfRevolution");
        require(window.active_parametric_object_.tool_id=="SurfaceOfRevolution" &&
                window.document_.GetSelectedSketch()==xy_source,"Failed revolve discarded the panel or selection");
        window.active_parametric_object_.parameters[1].value=1;
        require(window.TryStartLivePolylineRevolveFromSelection(),"Cannot retry with an in-plane axis");
        window.ClearActiveProperties();
        // Existing solid operation and its saved ID retain their semantics.
        auto sketch=std::make_unique<CSmartLine>();
        require(sketch->CreateFromWorldPoints({{10,0,0},{10,0,20}},false,{},{1,0,0},{0,0,1}),"Open sketch failed");
        window.document_.AddObject(std::move(sketch));
        require(window.document_.BeginLiveRevolveSelectedPolyline(360,2),"Legacy solid revolve failed");
        auto* body=window.document_.GetSelectedSolid();
        require(body && !dynamic_cast<CSurfaceSet*>(body) && TopExp_Explorer(body->m_Shape,TopAbs_SOLID).More(),"Solid revolve no longer makes a solid");
        window.document_.CancelLiveRevolveSelectedPolyline();
        require(window.document_.BeginLiveRevolveSelectedPolyline(360,2,true),"Open sketch surface failed");
        window.document_.CancelLiveRevolveSelectedPolyline();
        auto line=std::make_unique<CPolyline>();
        line->AddPoint(CPoint3d(10,0,0)); line->AddPoint(CPoint3d(12,2,20));
        window.document_.AddObject(std::move(line));
        require(window.document_.BeginLiveRevolveSelectedPolyline(90,2,true),"3D polyline surface failed");
        window.document_.CancelLiveRevolveSelectedPolyline();
        std::cout << "Revolve UI, spline preview, history, persistence and solid compatibility passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--sketch-transaction-only")) {
        QTemporaryDir settings;
        QCoreApplication::setOrganizationName("Dom3DSketchTransactions");
        QCoreApplication::setApplicationName("SketchTransactions");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        MainWindow window;
        auto* viewport=window.findChild<OpenGLViewport*>(); require(viewport,"Viewport missing");
        auto sketch=std::make_unique<CSmartLine>();
        require(sketch->CreateFromWorldPoints({{0,0,0},{20,0,0},{20,20,0},{0,20,0}},true,{},{1,0,0},{0,1,0}),"Sketch creation failed");
        auto* shape=sketch.get(); window.document_.AddObject(std::move(sketch));
        window.document_.SelectObjectById(shape->m_id,SelectionAction::Replace);
        window.undo_redo_.Reset();
        viewport->resize(800,600); viewport->SetOrthographicProjection(true); viewport->SetXYView();
        auto camera=viewport->GetCamera(); camera.target={10,10,0}; camera.distance=80; viewport->SetCamera(camera);
        require(viewport->BeginEditSelectedSketch(),"Sketch edit mode failed");
        const auto screen=[&](CPoint3d p) {
            DomPoint result; QtSceneRenderer renderer;
            require(renderer.WorldToScreen({float(p.x),float(p.y),float(p.z)},viewport->GetCamera(),true,viewport->width(),viewport->height(),result),"Cannot project sketch node");
            return QPointF(result.x,result.y);
        };
        const auto mouse=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons) {
            QMouseEvent event(type,p,p,button,buttons,Qt::NoModifier); QApplication::sendEvent(viewport,&event);
        };
        const auto initial=shape->GetNodeWorld(0); const auto line_id=shape->GetLine(0)->GetID();
        int events=0; shape->SetChangeCallback([&](unsigned,SketchRevisions){++events;});
        mouse(QEvent::MouseButtonPress,screen(initial),Qt::LeftButton,Qt::LeftButton);
        require(shape->IsEditing(),"Mouse press did not begin a sketch transaction");
        mouse(QEvent::MouseMove,screen({25,2,0}),Qt::NoButton,Qt::LeftButton);
        require(events==0,"Drag published an intermediate edit");
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(viewport,&escape);
        require(!shape->IsEditing() && events==0 && shape->GetNodeWorld(0).x==initial.x && window.undo_redo_.UndoCount()==0,"Escape did not cancel the sketch edit");
        mouse(QEvent::MouseButtonPress,screen(initial),Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,screen({25,2,0}),Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,screen({26,3,0}),Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,screen({26,3,0}),Qt::LeftButton,Qt::NoButton);
        require(!shape->IsEditing() && events==1 && window.undo_redo_.UndoCount()==1,"Drag must commit exactly once");
        const auto changed=shape->GetNodeWorld(0);
        require(std::abs(changed.x-initial.x)>1,"Node did not move");
        require(window.undo_redo_.Undo() && shape->GetNodeWorld(0).x==initial.x,"Sketch Undo failed");
        require(window.undo_redo_.Redo() && shape->GetNodeWorld(0).x==changed.x,"Sketch Redo failed");
        require(shape->GetLine(0)->GetID()==line_id,"Undo/Redo changed line identity");
        Dom3DProjectSerializer serializer; ProjectViewState view; QString error,room;
        const QString path=settings.path()+"/sketch.dom3d";
        require(serializer.Save(path,window.document_,"Sketch",view,{},error),"UV sketch save failed");
        CAlfaDoc restored; require(serializer.Load(path,restored,room,view,error),"UV sketch load failed");
        const auto* loaded=dynamic_cast<const CSmartLine*>(restored.FindObjectById(shape->m_id));
        require(loaded && loaded->GetLine(0)->GetID()==line_id
                && loaded->GetEndpointId(0,1)==shape->GetEndpointId(0,1),"Saved sketch identity changed");
        require(loaded->GetEndpointId(0,1)==loaded->GetEndpointId(1,0),"Shared node lost on load");
        std::cout << "Sketch UV, drag transaction, Escape, Undo/Redo and persistence passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--polyline-shift-only")) {
        CAlfaDoc document;
        OpenGLViewport viewport;
        viewport.SetDocument(&document);
        viewport.resize(800, 600);
        viewport.SetXYView();
        viewport.SetXYPlaneViewEnabled(true);
        auto camera = viewport.GetCamera();
        camera.target = {0, 0, 0}; camera.distance = 200;
        viewport.SetCamera(camera);
        viewport.SetTool(ToolMode::DrawCurve);
        viewport.BeginSpatialCurvePreview(OpenGLViewport::SpatialCurvePreviewKind::Polyline);
        viewport.SetSpatialCurvePreviewPoints({CPoint3d(0, 0, 0)});
        viewport.snapping_enabled_ = false;
        int clicks = 0;
        CPoint3d committed;
        QObject::connect(&viewport, &OpenGLViewport::Point3DPicked, [&](CPoint3d p) { ++clicks; committed = p; });
        for (const CPoint3d target : {CPoint3d(40, 10, 0), CPoint3d(10, 40, 0)}) {
            viewport.BeginPick3DPoint();
            DomPoint screen{};
            require(viewport.renderer_.WorldToScreen({float(target.x), float(target.y), 0}, camera,
                        true, viewport.width(), viewport.height(), screen), "Cannot project Shift target");
            const QPointF local(screen.x, screen.y), global(viewport.mapToGlobal(QPoint(screen.x, screen.y)));
            QMouseEvent move(QEvent::MouseMove, local, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&viewport, &move);
            require(std::abs(viewport.curve_preview_point_.x) > 1 && std::abs(viewport.curve_preview_point_.y) > 1,
                    "Unmodified polyline point was constrained");
            QKeyEvent shift(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
            QApplication::sendEvent(&viewport, &shift);
            const bool horizontal = target.x > target.y;
            require(std::abs(horizontal ? viewport.curve_preview_point_.y : viewport.curve_preview_point_.x) < 1.e-6,
                    "Shift did not update the segment without mouse movement");
            auto preview = viewport.curve_preview_point_;
            QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
            QApplication::sendEvent(&viewport, &press);
            require(committed.DistTo(&preview) < 1.e-6, "Shift click differs from the constrained preview");
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
            QApplication::sendEvent(&viewport, &release);
            require(std::abs(viewport.curve_preview_point_.x) > 1 && std::abs(viewport.curve_preview_point_.y) > 1,
                    "Releasing Shift did not restore the free segment");
        }
        require(clicks == 2, "Shift clicks bypassed the spatial polyline command");
        viewport.SetSpatialCurvePreviewPoints({});
        CPoint3d first(40, 10, 0);
        viewport.ConstrainPolylinePoint(first, Qt::ShiftModifier);
        require(first.x == 40 && first.y == 10, "Shift constrained the first point");
        std::cout << "Polyline Shift horizontal/vertical preview and clicks passed\n";
        return 0;
    }
    if (application.arguments().contains("--fillet-handle-only")) {
        CAlfaDoc moved_document;
        TopoDS_Shape shape = BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(100, 200, 30), gp_Dir(0, 0, 1)), 10, 20).Shape();
        auto body = std::make_unique<CSolid>(shape);
        require(body->ReBuldMesh(), "Cannot build moved cylinder");
        body->SetParametricOperation(0, "SolidCylinder", "Cylinder", {});
        body->SetParametricOperation(1, "SolidTransform", "Move",
            {{"type", 0}, {"dx", 70}, {"dy", 120}, {"dz", 10}});
        body->SetParametricOperation(2, "SolidTransform", "Move",
            {{"type", 0}, {"dx", 30}, {"dy", 80}, {"dz", 20}});
        moved_document.AddObject(std::move(body));
        auto* selected = moved_document.GetSelectedSolid();
        bool edge_selected = false;
        for (int i = 0; i < selected->GetNumSurfaces(); ++i) {
            if (BRepAdaptor_Surface(TopoDS::Face(selected->GetSurfaceFace(i)->m_Face)).GetType()
                == GeomAbs_Plane) {
                selected->SetSelectedEdge(i, 0);
                edge_selected = true;
                break;
            }
        }
        require(edge_selected && moved_document.BeginLiveFilletSelectedEdges(false),
                "Cannot start moved cylinder fillet");
        OpenGLViewport viewport;
        viewport.SetDocument(&moved_document);
        ActiveParametricObject active;
        active.tool_id = "fillet_edge";
        active.object_index = moved_document.GetSelectedObjectIndex();
        // A newly created fillet has no committed operation index yet.
        active.operation_index = 0;
        for (double radius : {1.5, 2.5}) {
            require(moved_document.UpdateLiveFillet(radius), "Cannot rebuild moved cylinder fillet");
            selected = moved_document.GetSelectedSolid();
            require(selected->ReBuldMesh(), "Cannot mesh moved cylinder preview");
            ToolParameter parameter;
            parameter.id = "radius";
            parameter.value = radius;
            active.parameters = {parameter};
            viewport.SetSolidDimensionEdit(active, "radius");
            require(viewport.solid_dimensions_.size() == 1, "Missing live radius handle");
            CPoint3d anchor, tangent;
            require(moved_document.GetLiveEdgeToolFrame(0.5, anchor, tangent), "Missing edge frame");
            const auto& dimension = viewport.solid_dimensions_.front();
            CPoint3d actual_start = dimension.GetStart();
            CPoint3d actual_end = dimension.GetEnd();
            require(actual_start.DistTo(&anchor) < 1.e-6,
                    "Live fillet anchor applied cylinder Move history twice");
            CPoint3d tip(anchor.x + tangent.x * radius,
                anchor.y + tangent.y * radius, anchor.z + tangent.z * radius);
            require(actual_end.DistTo(&tip) < 1.e-6,
                    "Live fillet tip moved away from the picked edge frame");
        }
        moved_document.CancelLiveFillet();
        std::cout << "Moved cylinder fillet handle passed\n";
        return 0;
    }
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary test directory missing");
    QCoreApplication::setOrganizationName("Dom3DScenePersistenceTests");
    QCoreApplication::setApplicationName("ScenePersistenceTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QTimer::singleShot(45000, [] { require(false, "Unexpected modal dialog / test timeout"); });

    MainWindow window;
    window.auto_save_timer_->stop();
    auto& document = window.document_;
    if (application.arguments().contains("--group-transform-only")) {
        const int option = application.arguments().indexOf("--group-transform-only");
        require(option + 1 < application.arguments().size(), "Missing group fixture");
        QString room, error;
        ProjectViewState view;
        require(window.dom3d_serializer_.Load(application.arguments()[option + 1], document, room, view, error),
                "Cannot load group transform fixture");
        const auto preview = RenderMaterialSphereGL(Material{}, 73);
        require(!preview.isNull(), "Material preview did not render");
        require(GetAlfaDoc() == &document, "Material preview detached the editing document");
        require(RenderMaterialSphereGL(Material{}, 73) == preview && GetAlfaDoc() == &document,
                "Cached material preview changed the editing document");
        const auto mass_center = [](const CSolid& solid) {
            GProp_GProps properties;
            BRepGProp::VolumeProperties(solid.m_Shape, properties);
            const gp_Pnt p = properties.CentreOfMass();
            return Vec3{float(p.X()), float(p.Y()), float(p.Z())};
        };
        for (unsigned long id : {11UL, 31UL}) {
            require(document.SelectObjectById(id), "Cannot select assembly");
            const auto* group = dynamic_cast<const CGroup*>(document.GetSelectedObject());
            require(group != nullptr, "Fixture selection is not a group");
            for (auto operation : {TransformOperation::Move, TransformOperation::Rotate, TransformOperation::Scale}) {
                window.BeginTransformTool(operation);
                Vec3 center{};
                require(document.GetTransformGizmoCenter(center), "Assembly has no transform gizmo center");
                std::vector<std::pair<unsigned long, Vec3>> before;
                for (const auto& object : document.GetObjects()) {
                    if (const auto* solid = dynamic_cast<const CSolid*>(object.get()))
                        before.emplace_back(object->m_id, mass_center(*solid));
                }
                if (operation == TransformOperation::Move) {
                    const auto finish_move = [&](bool escape) {
                    require(document.PreviewMoveSelectedObjects({11,17,23}), "Group move preview failed");
                    window.viewport_->dragging_transform_ = true;
                    window.viewport_->transform_drag_has_preview_ = true;
                    window.viewport_->transform_drag_move_delta_ = {11,17,23};
                    QElapsedTimer timer;
                    timer.start();
                    if (escape) {
                        QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                        QApplication::sendEvent(window.viewport_, &event);
                    } else {
                        QMouseEvent event(QEvent::MouseButtonRelease, QPointF(100,100), QPointF(100,100),
                                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QApplication::sendEvent(window.viewport_, &event);
                    }
                    std::cout << "Group " << id << (escape ? " Escape" : " LMB release")
                              << " move completion: " << timer.elapsed() << " ms\n";
                    require(timer.elapsed() < 1500, "Move completion repeatedly rebuilds clones");
                    };
                    finish_move(false);
                    require(window.undo_redo_.Undo(), "Group move Undo failed");
                    for (const auto& entry : before) {
                        const Vec3 actual = mass_center(*dynamic_cast<CSolid*>(document.FindObjectById(entry.first)));
                        const Vec3 error = actual - entry.second;
                        require(dot(error, error) < 0.01f, "Group move Undo did not restore members");
                    }
                    finish_move(true);
                    require(window.undo_redo_.Undo() && window.undo_redo_.Redo(), "Group move Undo/Redo failed");
                }
                else if (operation == TransformOperation::Rotate)
                    require(window.viewport_->ApplyPreciseRotate(center, {0,0,1}, 1.57079632679f), "Group rotate failed");
                else
                    require(window.viewport_->ApplyPreciseScale(center, 1.2f, 1.2f, 1.2f, true), "Group scale failed");
                for (const auto& entry : before) {
                    Vec3 expected = entry.second;
                    if (group->Contains(entry.first)) {
                        if (operation == TransformOperation::Move) expected = expected + Vec3{11,17,23};
                        else if (operation == TransformOperation::Rotate) {
                            const Vec3 offset = expected - center;
                            expected = center + Vec3{-offset.y, offset.x, offset.z};
                        } else expected = center + (expected - center) * 1.2f;
                    }
                    const auto* solid = dynamic_cast<const CSolid*>(document.FindObjectById(entry.first));
                    require(solid != nullptr, "Lost group member");
                    const Vec3 actual = mass_center(*solid);
                    if (std::fabs(actual.x - expected.x) >= 0.1f || std::fabs(actual.y - expected.y) >= 0.1f || std::fabs(actual.z - expected.z) >= 0.1f)
                        std::cerr << "group=" << id << " operation=" << int(operation) << " member=" << entry.first
                                  << " expected=" << expected.x << ',' << expected.y << ',' << expected.z
                                  << " actual=" << actual.x << ',' << actual.y << ',' << actual.z << '\n';
                    require(std::fabs(actual.x - expected.x) < 0.1f
                         && std::fabs(actual.y - expected.y) < 0.1f
                         && std::fabs(actual.z - expected.z) < 0.1f,
                            "Transform moved a group member incorrectly or changed another assembly");
                }
            }
        }
        // A clone can precede its source in the object list. Later passes
        // must still update downstream clones after the source was rebuilt.
        auto* source = dynamic_cast<CSolid*>(document.FindObjectById(3));
        auto* first_clone = dynamic_cast<CAssociativeClone*>(document.FindObjectById(4));
        require(source && first_clone, "Missing linked table leg");
        auto chained = std::make_unique<CAssociativeClone>(first_clone->m_Shape, 4);
        document.AddObject(std::move(chained), false);
        auto& objects = document.GetObjects();
        const unsigned long chained_id = objects.back()->m_id;
        std::rotate(objects.begin(), objects.end() - 1, objects.end());
        document.ClearSelection();
        const Vec3 clone_before = mass_center(*first_clone);
        const TopoDS_Shape unrelated_before = dynamic_cast<CSolid*>(document.FindObjectById(8))->m_Shape;
        source->Translate({6,0,0});
        require(document.RebuildAssociativeClones(3), "Clone chain did not rebuild");
        for (unsigned long clone_id : {4UL, chained_id}) {
            const Vec3 error = mass_center(*dynamic_cast<CSolid*>(document.FindObjectById(clone_id)))
                            - (clone_before + Vec3{6,0,0});
            require(dot(error,error) < 0.01f, "Out-of-order clone chain did not follow its source");
        }
        require(dynamic_cast<CSolid*>(document.FindObjectById(8))->m_Shape.IsSame(unrelated_before),
                "Rebuilt an unrelated clone");
        std::cout << "Group Move/Rotate/Scale after material preview passed\n";
        return 0;
    }
    if (application.arguments().contains("--sheet-bend-ui-only")) {
        const int option = application.arguments().indexOf("--sheet-bend-ui-only");
        require(option + 1 < application.arguments().size(), "Missing bend fixture");
        QString room, error;
        ProjectViewState view;
        require(window.dom3d_serializer_.Load(application.arguments()[option + 1], document, room, view, error),
                "Cannot load bend UI fixture");
        require(document.SelectObjectById(15) && document.SelectObjectById(91, SelectionAction::Add),
                "Cannot select sheet and line");
        CSolid* body = nullptr;
        for (const auto& object : document.GetObjects())
            if (object->m_id == 15) body = dynamic_cast<CSolid*>(object.get());
        require(body != nullptr, "Missing sheet");
        const TopoDS_Shape original = body->m_Shape;
        const int operations = body->GetNumOperations();
        bool completed_pick_sequence = false;
        QTimer::singleShot(0, [&] {
            auto* dialog = window.findChild<QDialog*>("sheetBendDialog");
            require(dialog && dialog->isVisible(), "Missing bend dialog");
            require(!dialog->isModal() && !QApplication::activeModalWidget(), "Bend dialog blocks viewport navigation");
            const float distance_before = window.viewport_->GetCamera().distance;
            QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(), QPoint(0, 120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(window.viewport_, &wheel);
            require(window.viewport_->GetCamera().distance != distance_before,
                    "Viewport zoom did not work with bend parameters open");
            auto* side = dialog->findChild<QComboBox*>("sheetBendMovingSide");
            auto* preview = dialog->findChild<QPushButton*>("sheetBendPreview");
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            require(side && preview && buttons, "Missing side/preview controls");
            require(!buttons->button(QDialogButtonBox::Ok)->isEnabled(), "Bend did not require a side choice");
            preview->click();
            require(body->m_Shape.IsSame(original), "Preview chose a moving side without user input");
            side->setCurrentIndex(1);
            preview->click();
            require(buttons->button(QDialogButtonBox::Ok)->isEnabled()
                        && !body->m_Shape.IsSame(original), "Valid side preview did not appear");
            side->setCurrentIndex(2);
            require(body->m_Shape.IsSame(original)
                        && !buttons->button(QDialogButtonBox::Ok)->isEnabled(),
                    "Side change retained an obsolete preview");
            auto* pick = dialog->findChild<QPushButton*>("sheetBendPickSide");
            require(pick, "Missing surface pick control");
            pick->click();
            require(!dialog->isVisible() && window.viewport_->picking_solid_surface_, "Surface picking did not start");
            // A user clicks only after the button handler has returned and
            // Qt has processed hiding the dialog. Direct signals in the same
            // callback conceal QDialog::exec() returning on hide().
            QPointer<QDialog> guarded_dialog(dialog);
            QTimer::singleShot(0, &window, [&, guarded_dialog] {
                require(guarded_dialog && window.viewport_->picking_solid_surface_,
                        "Hiding the dialog ended Sheet Bend before the user could click");
                QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(window.viewport_, &escape);
                require(guarded_dialog->isVisible() && !window.viewport_->picking_solid_surface_,
                        "Escape did not return to the bend dialog");
                guarded_dialog->findChild<QPushButton*>("sheetBendPickSide")->click();
                QTimer::singleShot(0, &window, [&, guarded_dialog] {
                    require(guarded_dialog && window.viewport_->picking_solid_surface_,
                            "Second surface pick ended before a mouse event");
                    window.viewport_->resize(800, 600);
                    window.viewport_->SetXYView();
                    window.viewport_->FitToDocument();
                    DomPoint screen{};
                    require(window.viewport_->renderer_.WorldToScreen({-260, 350, 0}, window.viewport_->camera_,
                                window.viewport_->orthographic_projection_, window.viewport_->width(),
                                window.viewport_->height(), screen), "Cannot project flange for surface picking");
                    const QPointF local(screen.x, screen.y);
                    const QPointF global(window.viewport_->mapToGlobal(local.toPoint()));
                    QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton,
                                      Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(window.viewport_, &press);
                    QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton,
                                        Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(window.viewport_, &release);
                    auto* side = guarded_dialog->findChild<QComboBox*>("sheetBendMovingSide");
                    auto* buttons = guarded_dialog->findChild<QDialogButtonBox*>();
                    require(guarded_dialog->isVisible() && side->currentIndex() == 1
                                && buttons->button(QDialogButtonBox::Ok)->isEnabled(),
                            "Mouse click did not restore the dialog with a flange preview");
                    require(window.viewport_->sheet_bend_guide_.size() > 2, "Missing direction arc");
                    QComboBox* direction = nullptr;
                    for (auto* combo : guarded_dialog->findChildren<QComboBox*>())
                        if (combo->count() == 2 && combo->itemText(0) == "Clockwise") direction = combo;
                    require(direction, "Missing bend direction combo");
                    direction->showPopup();
                    QTimer::singleShot(250, &window, [&, guarded_dialog, direction] {
                        auto* list = direction->view();
                        const QPointF pos = list->visualRect(list->model()->index(1, 0)).center();
                        const QPointF global = list->viewport()->mapToGlobal(pos.toPoint());
                        QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton,
                                          Qt::LeftButton, Qt::NoModifier);
                        QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton,
                                            Qt::NoButton, Qt::NoModifier);
                        QApplication::sendEvent(list->viewport(), &press);
                        QApplication::sendEvent(list->viewport(), &release);
                        std::cout << "Direction mouse: index=" << direction->currentIndex()
                                  << " popup=" << list->isVisible() << std::endl;
                        require(direction->currentIndex() == 1 && !list->isVisible(),
                                "Direction popup trapped input instead of changing the bend direction");
                        auto* preview = guarded_dialog->findChild<QPushButton*>("sheetBendPreview");
                        auto* buttons = guarded_dialog->findChild<QDialogButtonBox*>();
                        require(body->m_Shape.IsSame(original) && !buttons->button(QDialogButtonBox::Ok)->isEnabled(),
                                "Direction change kept the obsolete preview");
                        preview->click();
                        require(buttons->button(QDialogButtonBox::Ok)->isEnabled(), "Cannot preview the opposite direction");
                        direction->showPopup();
                        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
                        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                        QApplication::sendEvent(direction->view(), &up);
                        QApplication::sendEvent(direction->view(), &enter);
                        require(direction->currentIndex() == 0 && !direction->view()->isVisible(),
                                "Direction popup trapped keyboard input");
                        preview->click();
                        require(buttons->button(QDialogButtonBox::Ok)->isEnabled(), "Cannot restore the original direction");
                        const int capture_option = application.arguments().indexOf("--bend-guide-capture");
                        if (capture_option >= 0) {
                            require(capture_option + 1 < application.arguments().size(), "Missing guide capture path");
                            window.resize(1100, 850);
                            window.show();
                            window.viewport_->SetCamera(view.camera);
                            window.viewport_->SetOrthographicProjection(view.orthographic_projection);
                            QApplication::processEvents();
                            const QImage frame = window.viewport_->grabFramebuffer();
                            require(!frame.isNull() && frame.save(application.arguments()[capture_option + 1]),
                                    "Cannot capture the bend direction guide");
                        }
                        completed_pick_sequence = true;
                        buttons->button(QDialogButtonBox::Cancel)->click();
                    });
                });
            });
        });
        window.ApplySheetBend();
        require(completed_pick_sequence, "Sheet Bend returned while waiting for a surface click");
        require(body->m_Shape.IsSame(original) && body->GetNumOperations() == operations,
                "Cancel changed the source sheet or its history");
        require(window.viewport_->sheet_bend_guide_.empty(), "Bend guide survived Cancel");
        std::cout << "Sheet bend preview/cancel passed\n";
        return 0;
    }
    require(!window.HasUnsavedProjectChanges(), "A fresh scene must be clean");
    QCloseEvent clean_close;
    window.closeEvent(&clean_close);
    require(clean_close.isAccepted(), "A clean scene must close without a prompt");

    const auto add_curve = [&document](const char* name) {
        auto curve = std::make_unique<CPolyline>();
        curve->SetName(name);
        curve->AddPoint(CPoint3d(0, 0, 0));
        curve->AddPoint(CPoint3d(10, 0, 0));
        auto* result = curve.get();
        document.AddObject(std::move(curve));
        return result;
    };
    CPolyline* first = add_curve("Selection A");
    CPolyline* second = add_curve("Visibility B");
    window.scene_tree_figures_filter_->setChecked(true);
    window.scene_tree_layers_filter_->setChecked(true);
    window.scene_tree_parts_filter_->setChecked(true);
    document.SelectObjectById(first->m_id);
    window.RefreshSceneTree();
    const auto row = [&window](const QString& name) {
        for (QTreeWidgetItemIterator it(window.scene_tree_); *it; ++it) {
            if ((*it)->text(1) == name) return *it;
        }
        std::cerr << "Missing row: " << name.toStdString() << '\n';
        require(false, "Scene tree row missing");
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    const auto click_visibility = [&](const QString& name) {
        auto* item = row(name);
        // Reproduce Qt's selection change before dispatching itemClicked.
        window.scene_tree_->clearSelection();
        item->setSelected(true);
        window.scene_tree_->itemClicked(item, 0);
    };
    click_visibility("Visibility B");
    require(!second->IsVisible() && document.GetSelectedObject() == first,
            "Hiding another object must preserve scene selection");
    require(row("Selection A")->isSelected() && !row("Visibility B")->isSelected(),
            "Visibility clicks must preserve tree selection too");
    click_visibility("Visibility B");
    require(document.GetSelectedObject() == first,
            "Showing another object must preserve selection");
    document.SelectObjectById(second->m_id, SelectionAction::Add);
    click_visibility("Visibility B");
    require(document.GetSelectedObjectCount() == 1 && document.GetSelectedObject() == first,
            "Only the hidden member of a multiselection must be removed");
    click_visibility("Selection A");
    require(!document.HasSelection(), "Hiding the selected object must deselect it");

    first->SetVisible(true);
    second->SetVisible(true);
    auto* layer = document.AddLayer("Visibility layer");
    second->m_LayerID = layer->ID();
    document.SelectObjectById(first->m_id);
    window.RefreshSceneTree();
    click_visibility("Visibility layer");
    require(document.GetSelectedObject() == first, "Hiding an unrelated layer must preserve selection");
    click_visibility("Visibility layer");
    document.SelectObjectById(second->m_id, SelectionAction::Add);
    click_visibility("Visibility layer");
    require(document.GetSelectedObjectCount() == 1 && document.GetSelectedObject() == first,
            "Hiding a layer must remove only its selected objects");
    layer->Visible = true;
    second->SetGroupName("Legacy visibility group");
    window.RefreshSceneTree();
    click_visibility("Legacy visibility group");
    require(document.GetSelectedObject() == first && !second->IsVisible(),
            "Hiding a legacy group must preserve unrelated selection");

    auto part = std::make_unique<CPart>();
    part->SetName("Visibility part");
    document.AddObject(std::move(part));
    document.SelectObjectById(first->m_id);
    window.RefreshSceneTree();
    click_visibility("Parts");
    require(document.GetSelectedObject() == first, "Hiding all parts must preserve unrelated selection");

    std::cerr << "Visibility regressions passed; checking save/close...\n";
    TopoDS_Shape box = BRepPrimAPI_MakeBox(2, 3, 4).Shape();
    auto solid = std::make_unique<CSolid>(box);
    document.AddObject(std::move(solid));
    const auto content = window.dom3d_serializer_.DocumentFingerprint(document);
    require(!content.isEmpty() && content == window.dom3d_serializer_.DocumentFingerprint(document),
            "Solid scene fingerprints must be deterministic");
    document.ClearSelection();
    document.SelectObjectById(first->m_id);
    require(content == window.dom3d_serializer_.DocumentFingerprint(document),
            "Selection must not affect persistent content");
    require(window.HasUnsavedProjectChanges(), "Added geometry must be dirty");

    const auto close_with = [&window](QMessageBox::StandardButton choice) {
        QTimer::singleShot(0, [choice] { answer(choice); });
        QCloseEvent event;
        window.closeEvent(&event);
        return event.isAccepted();
    };
    require(!close_with(QMessageBox::Cancel), "Cancel must block closing");
    require(window.HasUnsavedProjectChanges(), "Cancel must retain the dirty state");
    require(close_with(QMessageBox::Discard), "Discard must allow closing");
    require(window.HasUnsavedProjectChanges(), "Discard must not mark unsaved data as saved");

    QTimer::singleShot(0, [] {
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            require(dialog, "Save on an untitled scene must ask for a filename");
            dialog->reject();
        });
        answer(QMessageBox::Save);
    });
    QCloseEvent cancel_save;
    window.closeEvent(&cancel_save);
    require(!cancel_save.isAccepted() && window.HasUnsavedProjectChanges(),
            "Cancelling Save As must keep the program open and dirty");

    window.project_path_ = temporary.filePath("missing/scene.dom3d").toStdString();
    QTimer::singleShot(0, [] {
        QTimer::singleShot(0, [] { answer(QMessageBox::Ok); });
        answer(QMessageBox::Save);
    });
    QCloseEvent failed_save;
    window.closeEvent(&failed_save);
    require(!failed_save.isAccepted() && window.HasUnsavedProjectChanges(),
            "A failed save must keep the program open and dirty");

    const QString saved_path = temporary.filePath("scene.dom3d");
    window.project_path_ = saved_path.toStdString();
    require(close_with(QMessageBox::Save), "Successful Save must allow closing");
    require(QFileInfo::exists(saved_path) && !window.HasUnsavedProjectChanges(),
            "Successful Save must create the file and mark the scene clean");
    window.undo_redo_.Reset();
    require(!window.HasUnsavedProjectChanges(),
            "Building undo snapshots/render caches must not dirty the saved scene");
    Camera camera = window.viewport_->GetCamera();
    camera.distance += 10;
    window.viewport_->SetCamera(camera);
    require(!window.HasUnsavedProjectChanges(), "Camera movement must not dirty the scene");
    first->SetVisible(false);
    require(window.HasUnsavedProjectChanges(), "Visibility-only changes must be dirty");
    first->SetVisible(true);
    require(!window.HasUnsavedProjectChanges(), "Restoring saved visibility must be clean");
    window.undo_redo_.BeginChange();
    first->AddPoint(CPoint3d(20, 0, 0));
    window.undo_redo_.CommitChange("Edit curve");
    require(window.HasUnsavedProjectChanges(), "Geometry edits must be dirty");
    require(window.undo_redo_.Undo(), "Undo failed");
    require(!window.HasUnsavedProjectChanges(), "Undo to saved content must be clean");
    require(window.undo_redo_.Redo() && window.HasUnsavedProjectChanges(),
            "Redo after the save point must be dirty");
    require(window.undo_redo_.Undo() && !window.HasUnsavedProjectChanges(),
            "Undo after Redo must restore the save point");
    document.GetMaterials().front().name += " changed";
    require(window.HasUnsavedProjectChanges(), "Material changes must be dirty");
    window.AutoSaveProject();
    require(!window.HasUnsavedProjectChanges(), "Successful autosave must mark clean");
    document.SetDraftingData("changed drafting data");
    require(window.HasUnsavedProjectChanges(), "Drafting changes must be dirty");
    window.OpenProjectFromPath(saved_path);
    require(!window.HasUnsavedProjectChanges(), "Opening a saved project must be clean");
    window.NewProject();
    require(!window.HasUnsavedProjectChanges(), "New scene must reset the saved baseline");
    // Native files contain OCCT triangles. Opening must replace that cache
    // with the display net after restoring (or fitting) the camera.
    TColgp_Array2OfPnt poles(1, 4, 1, 4);
    for (int u = 1; u <= 4; ++u) {
        for (int v = 1; v <= 4; ++v) {
            poles.SetValue(u, v, gp_Pnt((u - 1) * 100.0, (v - 1) * 40.0,
                (v == 2 || v == 3 ? 60.0 : 0.0)
                + (u == 2 ? 40.0 : u == 3 ? -40.0 : 0.0)));
        }
    }
    Handle(Geom_BezierSurface) bezier = new Geom_BezierSurface(poles);
    TopoDS_Shape spline_shape = BRepBuilderAPI_MakeFace(
        GeomConvert::SurfaceToBSplineSurface(bezier), 1.e-7).Shape();
    CAlfaDoc source;
    source.GetObjects().clear();
    auto patch = std::make_unique<CSurfaceSet>(spline_shape);
    require(patch->BuildImportedRenderMesh(false), "Fixture triangulation failed");
    for (const auto& face : patch->GetSurfaceFace(0)->pMesh3D->GetFaces())
        require(face.corners.size() == 3, "Fixture must start with triangles");
    source.AddObject(std::move(patch), false);
    window.viewport_->resize(900, 600);
    for (bool restore_camera : {false, true}) {
        ProjectViewState view;
        view.has_camera = restore_camera;
        view.camera = window.viewport_->GetCamera();
        view.camera.distance = 750.0f;
        view.has_orthographic_projection = true;
        view.orthographic_projection = true;
        const QString spline_path = temporary.filePath(
            restore_camera ? "spline-camera.dom3d" : "spline-fit.dom3d");
        QString error;
        require(window.dom3d_serializer_.Save(
                    spline_path, source, "", view, {}, error),
                "Spline fixture save failed");
        window.OpenProjectFromPath(spline_path);
        CSolid* loaded = nullptr;
        for (const auto& object : document.GetObjects()) {
            if (auto* candidate = dynamic_cast<CSolid*>(object.get())) loaded = candidate;
        }
        require(loaded && loaded->GetNumSurfaces() == 1, "Loaded patch missing");
        auto* mesh = loaded->GetSurfaceFace(0)->pMesh3D;
        require(mesh && !mesh->GetFaces().empty(), "Loaded display net missing");
        const size_t initial_faces = mesh->GetFaces().size();
        for (const auto& face : mesh->GetFaces())
            require(face.deleted || face.corners.size() == 4,
                    "Opening retained stored triangles instead of the CNet display");
        require(!window.HasUnsavedProjectChanges(), "Automatic display rebuild dirtied project");
        window.viewport_->RefreshSurfaceMeshQuality();
        require(loaded->GetSurfaceFace(0)->pMesh3D->GetFaces().size() == initial_faces,
                "Update Scene changed the initial display net density");
        require(!window.HasUnsavedProjectChanges(), "Repeated display rebuild dirtied project");
    }
    window.NewProject();
    // Pivot selection must own both hover and click while Orbit stays active.
    auto pivot_curve = std::make_unique<CPolyline>();
    pivot_curve->AddPoint(CPoint3d(0, 0, 10));
    pivot_curve->AddPoint(CPoint3d(40, 0, 10));
    document.AddObject(std::move(pivot_curve), false);
    window.viewport_->SetTool(ToolMode::Orbit);
    camera = window.viewport_->GetCamera();
    camera.target = {0, 0, 10};
    camera.distance = 500;
    window.viewport_->SetCamera(camera);
    window.pending_transform_point_pick_ = MainWindow::PendingTransformPointPick::RotationPivot;
    window.viewport_->BeginPick3DPoint("Pick rotation pivot");
    const QPointF pivot_pixel(window.viewport_->width() / 2.0,
                              window.viewport_->height() / 2.0);
    for (int hover = 0; hover < 2; ++hover) {
        QMouseEvent move(QEvent::MouseMove, pivot_pixel, pivot_pixel,
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(window.viewport_, &move);
        require(window.viewport_->cursor().shape() == Qt::BitmapCursor,
                "Orbit replaced the pivot snap cursor on repeated hover");
    }
    QMouseEvent pivot_press(QEvent::MouseButtonPress, pivot_pixel, pivot_pixel,
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &pivot_press);
    QMouseEvent pivot_release(QEvent::MouseButtonRelease, pivot_pixel, pivot_pixel,
                             Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &pivot_release);
    require(window.viewport_->HasRotationPivot(), "Orbit consumed the pivot selection click");
    const auto pivot_camera = window.viewport_->GetCamera();
    require(pivot_camera.target.x == 0 && pivot_camera.target.y == 0
                && pivot_camera.target.z == 10,
            "Rotation pivot did not snap to the curve endpoint");
    require(window.viewport_->CurrentTool() == ToolMode::Orbit,
            "Picking a pivot must retain Orbit mode");
    window.NewProject();
    auto editable = std::make_unique<CPolyline>();
    editable->AddPoint(CPoint3d(0, 0, 10));
    editable->AddPoint(CPoint3d(-80, 0, 10));
    CPolyline* edited = editable.get();
    document.AddObject(std::move(editable), false);
    auto target = std::make_unique<CPolyline>();
    CPolyline* linked_peer = target.get();
    const CPoint3d destination(60, 25, 35);
    target->AddPoint(destination);
    target->AddPoint(CPoint3d(100, 25, 35));
    document.AddObject(std::move(target), false);
    document.SelectObjectById(edited->m_id);
    window.viewport_->SetCamera(camera);
    require(window.viewport_->BeginEditSelectedCurve(), "Curve edit did not start");
    QtSceneRenderer projector;
    DomPoint target_pixel{};
    require(projector.WorldToScreen({60, 25, 35}, camera, true,
                window.viewport_->width(), window.viewport_->height(), target_pixel),
            "Snap target projection failed");
    QApplication::sendEvent(window.viewport_, &pivot_press);
    const QPointF tiny_move = pivot_pixel + QPointF(2, 0);
    QMouseEvent free_drag(QEvent::MouseMove, tiny_move, tiny_move,
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &free_drag);
    require(edited->GetPoints()[0].x != 0 || edited->GetPoints()[0].y != 0,
            "Dragged node snapped to itself instead of moving");
    const QPointF near_target(target_pixel.x + 2, target_pixel.y);
    QMouseEvent snap_drag(QEvent::MouseMove, near_target, near_target,
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &snap_drag);
    const auto& snapped_node = edited->GetPoints()[0];
    require(snapped_node.x == destination.x && snapped_node.y == destination.y
                && snapped_node.z == destination.z,
            "Curve drag did not snap to the target in 3D");
    QMouseEvent drag_release(QEvent::MouseButtonRelease, near_target, near_target,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &drag_release);
    document.SelectObjectById(edited->m_id);
    document.SelectObjectById(linked_peer->m_id, SelectionAction::Add);
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        require(dialog && dialog->windowTitle().startsWith("Link Curves"),
                "Endpoint linking dialog did not open");
        for (auto* button : dialog->findChildren<QAbstractButton*>()) {
            if (button->text() == "Link") { button->click(); return; }
        }
        require(false, "Endpoint linking action missing");
    });
    require(window.LinkSelectedCurves(), "Automatic endpoint linking failed");
    require(document.GetCurveEndpointLinks().size() == 1,
            "Wrong endpoint pair linked by dialog");
    document.SelectObjectById(edited->m_id);
    require(window.viewport_->BeginEditSelectedCurve(), "Linked curve editing failed");
    QMouseEvent linked_press(QEvent::MouseButtonPress, near_target, near_target,
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &linked_press);
    const QPointF linked_small_pixel = near_target + QPointF(2, 0);
    QMouseEvent linked_small_drag(QEvent::MouseMove, linked_small_pixel, linked_small_pixel,
                                 Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &linked_small_drag);
    require(edited->GetPoints()[0].x != destination.x
                || edited->GetPoints()[0].y != destination.y
                || edited->GetPoints()[0].z != destination.z,
            "Linked endpoint snapped to its own moving peer");
    const QPointF linked_pixel = near_target + QPointF(70, 40);
    QMouseEvent linked_drag(QEvent::MouseMove, linked_pixel, linked_pixel,
                           Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &linked_drag);
    require(linked_peer->GetPoints()[0].x == edited->GetPoints()[0].x
                && linked_peer->GetPoints()[0].y == edited->GetPoints()[0].y
                && linked_peer->GetPoints()[0].z == edited->GetPoints()[0].z,
            "Linked peer did not follow live drag");
    QMouseEvent linked_release(QEvent::MouseButtonRelease, linked_pixel, linked_pixel,
                              Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(window.viewport_, &linked_release);
    require(window.undo_redo_.Undo(), "Undo linked live drag failed");
    require(linked_peer->GetPoints()[0].x == destination.x
                && linked_peer->GetPoints()[0].y == destination.y
                && linked_peer->GetPoints()[0].z == destination.z,
            "Compact curve Undo did not restore the linked peer");
    // Exercise the compact gizmo Move undo command with a linked endpoint.
    document.SelectObjectById(edited->m_id);
    window.undo_redo_.Reset();
    window.viewport_->SetTool(ToolMode::Transform);
    window.viewport_->transform_operation_ = TransformOperation::Move;
    window.viewport_->selection_mode_ = SelectionMode::Object;
    window.viewport_->transform_drag_move_delta_ = {3,4,5};
    require(document.PreviewMoveSelectedObjects({3,4,5}), "Spline gizmo preview failed");
    window.viewport_->transform_drag_has_preview_ = true;
    window.viewport_->CommitTransformDrag();
    require(linked_peer->GetPoints()[0].x == destination.x + 3,
            "Linked peer did not follow whole-curve Move");
    require(window.undo_redo_.Undo()
                && linked_peer->GetPoints()[0].x == destination.x
                && linked_peer->GetPoints()[0].y == destination.y
                && linked_peer->GetPoints()[0].z == destination.z,
            "Gizmo Move Undo did not restore linked peer");
    require(window.undo_redo_.Redo() && linked_peer->GetPoints()[0].x == destination.x + 3,
            "Gizmo Move Redo did not restore linked peer");
    require(window.undo_redo_.Undo(), "Could not restore curve preview fixture");

    // Drawing commands request the next 3D point after every click. That
    // request must not short-circuit the rubber-band update on mouse motion.
    using PreviewKind = OpenGLViewport::SpatialCurvePreviewKind;
    for (PreviewKind kind : {PreviewKind::Polyline, PreviewKind::BSpline,
                             PreviewKind::Bezier, PreviewKind::Nurbs}) {
        auto* viewport = window.viewport_;
        viewport->SetTool(kind == PreviewKind::Polyline
            ? ToolMode::DrawCurve : ToolMode::DrawBSpline);
        viewport->BeginSpatialCurvePreview(kind);
        viewport->SetSpatialCurvePreviewPoints({CPoint3d(0, 0, 10)});
        viewport->BeginPick3DPoint("Next curve point");
        QMouseEvent preview_move(QEvent::MouseMove, near_target, near_target,
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &preview_move);
        require(viewport->curve_preview_valid_,
                "Pending 3D point pick blocked the curve rubber band");
        require(viewport->curve_preview_point_.x == destination.x
                    && viewport->curve_preview_point_.y == destination.y
                    && viewport->curve_preview_point_.z == destination.z,
                "Rubber-band endpoint did not use the snapped next point");
        const QPointF next_pixel = near_target + QPointF(70, 40);
        QMouseEvent preview_next(QEvent::MouseMove, next_pixel, next_pixel,
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &preview_next);
        require(viewport->curve_preview_valid_
                    && (viewport->curve_preview_point_.x != destination.x
                        || viewport->curve_preview_point_.y != destination.y
                        || viewport->curve_preview_point_.z != destination.z),
                "Rubber band stopped following the cursor");
        require(viewport->spatial_curve_preview_points_.size() == 1,
                "Preview committed a point without a click");
    }
    std::cout << "Scene persistence tests passed.\n";
    return 0;
}
