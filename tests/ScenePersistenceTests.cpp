#include "CBSpline.h"
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <QStatusBar>
#include "ui/MainWindow.h"
#include "CPolyline.h"
#include "CPart.h"
#include "solid/Solid.h"
#include "solid/SurfaceSet.h"
#include "CMesh3D.h"
#include "SmartLine.h"
#include "Dom3DProjectSerializer.h"

#include <BRepPrimAPI_MakeBox.hxx>
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
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
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
