#include "solid/FilletEdgeIdentity.h"
#include "ui/ExtrudeFaceDialog.h"
#include "HybridExtrude.h"
#include "SurfacePointTransform.h"
#include <QSpinBox>
#include "SurfaceGraphOffsetBuilder.h"
#include "solid/SubdivideFace.h"
#include <BRepFilletAPI_MakeFillet.hxx>
#include "ExtrudeShapeBuilder.h"
#include "SurfaceTopologyBuilder.h"
#include "ExtractedEdgeCurve.h"
#include "SplineBezierSections.h"
#include "SurfaceOffsetBuilder.h"
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <gp_Circ.hxx>
#include "SaddleSurfaceBuilder.h"
#include "Body1Builder.h"
#include "BottleSection.h"
#include "BodySectionSketchBuilder.h"
#include <QLineEdit>
#include "Sketch.h"
#include "SketchArcLine.h"
#include "MultiSketchProfileBuilder.h"
#include "SketchProfileBuilder.h"
#include <BRepAlgoAPI_Cut.hxx>
#include "ui/SectionGraphEditor.h"
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include "SurfaceSketchTrimBuilder.h"
#include "TwoViewSurfaceBuilder.h"
#include "ui/PropertyPanel.h"
#include "ui/ZebraDialog.h"
#include "SurfaceEdgePatchBuilder.h"
#include "NSidedSurfaceBuilder.h"
#include "ui/NSidedSurfaceDialog.h"
#include "ui/LanguageManager.h"
#include <QListWidget>
#include "SurfaceFilletBuilder.h"
#include <set>
#include <BRepAlgoAPI_Splitter.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <QLabel>
#include <QShortcut>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QCursor>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomLProp_SLProps.hxx>
#include <BRepTools.hxx>
#include "SurfaceBridgeBuilder.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <Geom_Surface.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include "CBSpline.h"
#include "CurveCutGeometry.h"
#include <gp_Lin.hxx>
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
#include <TColStd_Array2OfReal.hxx>
#include <QApplication>
#include <QFontDatabase>
#include <QElapsedTimer>
#include <QAbstractButton>
#include <QCheckBox>
#include <QToolButton>
#include <QMenu>
#include <QDir>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QToolBar>
#include <QDockWidget>
#include <QSlider>
#include <gp_Pln.hxx>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabBar>
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
#include <QProgressDialog>
#include <QProgressBar>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <sstream>

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

#include "BodySectionSketchTestCases.inc"

int TestScenePersistence(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    if(application.arguments().contains("--curve-quick-menu-only")) {
        QTemporaryDir settings;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        const char* ids[]={"SurfaceRuled","SolidWireTool","SurfaceRevolve","CurveTrimByPlane","ProjectCurveToPlane","ProjectCurveToSurface","ProjectCurveToFace"};
        const QStringList labels={"Ruled","Wire","Revolve","Trim by Plane","Put on Plane","Put on Surface","Put on Face"};
        for(int choice=-1;choice<7;++choice) {
            MainWindow window;auto& doc=window.document_;doc.GetObjects().clear();
            auto spline=std::make_unique<CBSpline>();
            spline->AddPoint({10,0,10});spline->AddPoint({20,10,20});spline->AddPoint({30,0,30});
            doc.AddObject(std::move(spline));const auto source=doc.GetSelectedObject()->m_id;
            window.UpdateActiveToolUi("select");window.viewport_->SetTool(ToolMode::Select);
            bool shown=false;
            QTimer::singleShot(0,&window,[&] {
                auto* menu=window.findChild<QMenu*>("CurveQuickMenu");require(menu,"Curve menu missing");shown=true;
                auto* cap=menu->actions().front();
                require(cap->text()=="Cap"&&cap->data().toString()=="SurfaceTangentCap"&&!cap->isEnabled(),"Cap must require a closed spline");
                auto* nsided=menu->findChild<QAction*>("CurveQuickSurfaceNSided");
                require(nsided&&nsided->isEnabled(),"Open spline menu lost N-Sided Surface");
                for(int i=0;i<7;++i) {
                    auto* action=menu->actions().at(i+2);
                    require(action->text()==labels[i] && action->data().toString()==ids[i] && action->isEnabled(),"Wrong curve command order or availability");
                }
                require(menu->findChild<QAction*>("CurveQuickCopy"),"Curve menu lost Copy");
                if(choice<0)menu->close();
                else {
                    menu->setActiveAction(menu->actions().at(choice+2));
                    QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(menu,&enter);
                }
            });
            window.viewport_->ObjectQuickMenuRequested(QPoint(100,100));require(shown,"Curve menu was not shown");
            if(choice<0)require(doc.GetObjects().size()==1 && doc.GetSelectedObject()->m_id==source,"Dismissing curve menu changed selection");
            else if(choice<3)require(window.active_parametric_object_.tool_id==ids[choice],"Curve menu started wrong modeling tool");
            else if(choice==3)require(window.pending_curve_edit_command_==MainWindow::CurveEditCommand::TrimByPlane,"Curve trim did not start");
            else {
                const auto expected=choice==4 ? MainWindow::PendingGroupCommand::ProjectCurveToPlane : choice==6 ? MainWindow::PendingGroupCommand::ProjectCurveToFace : MainWindow::PendingGroupCommand::ProjectCurveToSurface;
                require(window.pending_group_command_==expected,"Projection target selection did not start");
                require(window.pending_projection_curve_id_==source,"Projection forgot the selected source curve");
                window.viewport_->SelectionCommandCanceled();
                require(!window.pending_projection_curve_id_ && doc.GetSelectedObject()->m_id==source,"Cancel did not restore source curve");
                window.ProjectCurveToSurface(choice==4,choice==6);
                {
                    auto planeShape=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),15,25,-100,100).Shape();
                    if(choice==6) {
                        planeShape=BRepPrimAPI_MakeBox(gp_Pnt(15,-100,-5),10,200,5).Shape();
                        doc.AddObject(std::make_unique<CSolid>(planeShape));
                    } else doc.AddObject(std::make_unique<CSurfaceSet>(planeShape));
                    const auto target=doc.GetSelectedObject()->m_id;
                    doc.SelectObjectById(target,SelectionAction::Replace);
                    if(choice==6) {
                        auto* body=dynamic_cast<CSolid*>(doc.FindObjectById(target));
                        require(body->EnsureRenderMesh(),"Cannot prepare target face");
                        int top=-1;
                        for(int f=0;f<body->GetNumSurfaces();++f) {
                            BRepAdaptor_Surface geometry(TopoDS::Face(body->GetSurfaceFace(f)->m_Face));
                            if(geometry.GetType()==GeomAbs_Plane && std::abs(geometry.Plane().Axis().Direction().Z())>.99
                                && std::abs(geometry.Plane().Location().Z())<1.e-8)top=f;
                        }
                        require(top>=0,"Cannot locate top face");
                        require(doc.SelectSolidFaceAtScreen({200,0},[](Vec3 p,DomPoint& screen,float& depth) {
                            screen={int(p.x*10),int(p.y*10)};depth=100-p.z;return true;
                        }),"Cannot pick target face");
                        require(body->GetSelectedFaceIndex()==top,"Picked wrong target face");
                    }
                    window.viewport_->SelectionChanged();
                    bool directionShown=false;
                    if(choice==5)QTimer::singleShot(0,&window,[&] {
                        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                        require(dialog && dialog->parentWidget()==&window
                            && dialog->findChildren<QDoubleSpinBox*>().size()==3,"Surface target did not advance to projection direction");
                        directionShown=true;
                        auto* pick=dialog->findChild<QPushButton*>("PickProjectionDirection");
                        require(pick,"Projection direction picker missing");pick->click();
                        QTimer::singleShot(0,&window,[&,dialog] {
                            require(!dialog->isVisible(),"Vector dialog blocks viewport picking");
                            // Same shared picker signal is emitted by a straight
                            // segment and by a coordinate-axis hit.
                            window.viewport_->RotationAxisPicked({0,0,0},{0,0,4});
                            QTimer::singleShot(0,&window,[&,dialog] {
                                const auto inputs=dialog->findChildren<QDoubleSpinBox*>();
                                require(inputs[0]->value()==0 && inputs[1]->value()==0 && inputs[2]->value()==1,"Picked vector was not normalized into the dialog");
                                require(!window.precise_rotate_axis_ready_,"Projection pick started Rotate");
                                // Clear the simulated pick just as a real mouse
                                // hit does before emitting RotationAxisPicked.
                                QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
                                QApplication::sendEvent(window.viewport_,&escape);
                                dialog->accept();
                            });
                        });
                    });
                    application.processEvents();
                    require(choice!=5 || directionShown,"Surface projection did not start automatically");
                    require(window.pending_group_command_==MainWindow::PendingGroupCommand::None && !window.pending_projection_curve_id_,"Completed projection kept stale picking state");
                    require(doc.GetObjects().size()>2,"Clicking projection target did not produce a curve");
                    for(size_t i=2;i<doc.GetObjects().size();++i) {
                        Vec3 lo,hi;require(doc.GetObjects()[i]->GetBounds(lo,hi),"Projected curve has no bounds");
                        require(std::abs(lo.z)<1.e-4 && std::abs(hi.z)<1.e-4,"Put on Plane missed target plane");
                        if(choice==4)require(lo.x<10.01 && hi.x>29.99,"Put on Plane clipped the curve to the plane rectangle");
                        else require(lo.x>=14.99 && hi.x<=25.01,"Bounded projection escaped the target face");
                    }
                    const auto projected_count=doc.GetObjects().size();
                    require(window.undo_redo_.Undo(),"Projection has no Undo");
                    require(doc.GetObjects().size()==2 && doc.FindObjectById(source) && doc.FindObjectById(target),"Undo removed projection inputs or kept results");
                    require(window.undo_redo_.Redo() && doc.GetObjects().size()==projected_count,"Projection Redo failed");
                }
                if(choice==4)for(int axis=0;axis<3;++axis) {
                    doc.SelectObjectById(source,SelectionAction::Replace);
                    window.ProjectCurveToSurface(true);
                    const auto before=doc.GetObjects().size();
                    window.viewport_->SelectOriginPlane(axis);application.processEvents();
                    require(doc.GetObjects().size()==before+1,"Reference plane click failed to project curve");
                    Vec3 lo,hi;require(doc.GetObjects().back()->GetBounds(lo,hi),"No projected reference-plane bounds");
                    const double low=axis==0?lo.z:axis==1?lo.y:lo.x,high=axis==0?hi.z:axis==1?hi.y:hi.x;
                    require(std::abs(low)<1.e-4 && std::abs(high)<1.e-4,"Wrong reference plane projection");
                    require(hi.x-lo.x>19.9 || hi.z-lo.z>19.9,"Reference plane clipped the original curve");
                }
            }
        }
        // A non-planar projection must become lighter without moving its ends
        // or exceeding the requested modeling tolerance.
        CAlfaDoc projectedDoc;
        projectedDoc.GetObjects().clear();
        auto input=std::make_unique<CBSpline>();
        for(const Vec3 p : {Vec3{20,-8,-5},Vec3{20,-3,10},Vec3{20,3,-10},Vec3{20,8,5}})
            input->AddPoint({p.x,p.y,p.z});
        projectedDoc.AddObject(std::move(input));
        const auto inputId=projectedDoc.GetSelectedObject()->m_id;
        Handle(Geom_CylindricalSurface) cylinder=new Geom_CylindricalSurface(
            gp_Ax3(gp_Pnt(0,0,0),gp_Dir(0,0,1)),10);
        const auto target=BRepBuilderAPI_MakeFace(cylinder,0,6.283185307179586,-20,20,1.e-7).Face();
        const auto rawCount=projectedDoc.ProjectCurveToFace(inputId,target,{-1,0,0},0);
        require(rawCount>0,"Cylinder projection failed");
        std::vector<const CBSpline*> raw;
        for(size_t i=1;i<=rawCount;++i) {
            raw.push_back(dynamic_cast<CBSpline*>(projectedDoc.GetObjects()[i].get()));
            require(raw.back(),"Raw projection is not editable NURBS");
        }
        for(double tolerance : {0.02,0.002}) {
            const auto before=projectedDoc.GetObjects().size();
            require(projectedDoc.ProjectCurveToFace(inputId,target,{-1,0,0},tolerance)==rawCount,
                "Simplification changed projection branch count");
            size_t rawPoles=0,resultPoles=0;
            for(size_t i=0;i<rawCount;++i) {
                auto* result=dynamic_cast<CBSpline*>(projectedDoc.GetObjects()[before+i].get());
                require(result,"Projected result is not editable NURBS");
                rawPoles+=raw[i]->GetPoints().size();resultPoles+=result->GetPoints().size();
                double worst=0;
                for(int j=0;j<=10000;++j) {
                    const float t=float(j)/10000;
                    const auto p=result->Evaluate(t);
                    const auto q=raw[i]->Evaluate(t);
                    const double error=gp_Pnt(q.x,q.y,q.z).Distance(gp_Pnt(p.x,p.y,p.z));
                    worst=std::max(worst,error);
                    if(j==0 || j==10000) {
                        if(error>=1.e-5)std::cerr<<"Projection endpoint branch="<<i<<" t="<<t<<" error="<<error
                            <<" raw="<<q.x<<","<<q.y<<","<<q.z<<" result="<<p.x<<","<<p.y<<","<<p.z<<std::endl;
                        require(error<1.e-5,"Simplification moved a projection endpoint");
                    }
                }
                require(worst<=tolerance,"Projected NURBS exceeds modeling tolerance");
            }
            require(resultPoles<rawPoles,"Curved projection was not simplified");
            std::cout<<"Projection poles "<<rawPoles<<" -> "<<resultPoles<<" tolerance="<<tolerance<<std::endl;
        }
        require(dynamic_cast<CBSpline*>(projectedDoc.FindObjectById(inputId))->GetPoints().size()==4,
            "Projection simplification modified the source curve");
        std::cout<<"Curve quick menu commands, projection simplification and Undo passed\n";return 0;
    }
    if (application.arguments().contains("--quick-menu-cancel-only")) {
        CAlfaDoc document;
        auto mesh=std::make_unique<CMesh3D>("Quick menu fixture");
        CMesh3D::Face face; face.corners={{0,0,0},{1,0,0},{2,0,0},{3,0,0}};
        require(mesh->SetGeometry({{-50,-50,0},{50,-50,0},{50,50,0},{-50,50,0}}, {face}), "Cannot create menu fixture");
        document.AddMesh(std::move(mesh));
        OpenGLViewport viewport; viewport.SetDocument(&document); viewport.SetTool(ToolMode::Select);
        viewport.resize(800,600); viewport.show(); viewport.SetXYView(); viewport.FitToDocument();
        int shown=0;
        QObject::connect(&viewport,&OpenGLViewport::ObjectQuickMenuRequested,&viewport,[&](QPoint){++shown;});
        const auto wait=[&](int ms) {
            QEventLoop loop; QTimer::singleShot(ms,&loop,&QEventLoop::quit); loop.exec();
        };
        const auto arm=[&]() {
            QApplication::setActiveWindow(&viewport); viewport.setFocus();
            const QPoint point(viewport.width()/2,viewport.height()/2);
            QCursor::setPos(viewport.mapToGlobal(point)); wait(100);
            const QPointF local(point), global(viewport.mapToGlobal(point));
            QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&press);
            QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&release);
            require(document.HasSelection(),"Quick menu fixture was not selected");
        };
        wait(200); arm(); wait(1200);
        require(shown==1,"Uninterrupted selection must still show its quick menu");
        arm();
        QMouseEvent right(QEvent::MouseButtonPress,QPointF(400,300),QPointF(viewport.mapToGlobal(QPoint(400,300))),
            Qt::RightButton,Qt::RightButton,Qt::NoModifier);
        QApplication::sendEvent(&viewport,&right);
        QMouseEvent right_release(QEvent::MouseButtonRelease,QPointF(400,300),QPointF(viewport.mapToGlobal(QPoint(400,300))),
            Qt::RightButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(&viewport,&right_release); wait(1200);
        require(shown==1,"Right click did not cancel delayed quick menu");
        arm();
        QMenu popup; popup.addAction("Context menu"); popup.popup(viewport.mapToGlobal(QPoint(400,300)));
        wait(1200); require(shown==1,"Quick menu appeared over a popup"); popup.close();
        arm();
        QDialog dialog(&viewport); QTimer::singleShot(150,&dialog,&QDialog::accept); dialog.exec();
        wait(1200); require(shown==1,"Closed modal dialog allowed stale quick menu");
        arm();
        QDialog modeless(&viewport); modeless.show(); wait(100); modeless.close(); wait(1200);
        require(shown==1,"Modeless dialog allowed stale quick menu");
        arm(); QEvent leave(QEvent::Leave); QApplication::sendEvent(&viewport,&leave); wait(1200);
        require(shown==1,"Leaving viewport allowed stale quick menu");
        arm(); wait(1200); require(shown==2,"Quick menu did not recover for a fresh selection");
        std::cout << "Delayed quick menu cancellation passed.\n";
        return EXIT_SUCCESS;
    }
    if(application.arguments().contains("--body-section-sketch"))return TestBodySectionSketch(application);
    if(application.arguments().contains("--body-copy-placement")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;auto& doc=window.document_;auto settings=BodyPrimitiveDefaults(0);
        settings.guides=CreateBodyPrimitiveGuides(doc,0);std::string error;
        auto shape=BuildBody1(doc,settings,error);require(!shape.IsNull(),"Cannot build Body-1 fixture");
        auto source=std::make_unique<CSolid>(shape);source->SetName("Body-1");
        source->SetParametricOperation(0,"Body1","Body-1",Body1Parameters(settings));
        auto* ptr=source.get();doc.AddObject(std::move(source));const auto sourceId=ptr->m_id;
        doc.SelectObjectById(sourceId);require(doc.DuplicateSelectedObject(),"Cannot copy Body-1");
        const auto copyId=doc.GetSelectedObject()->m_id;const auto index=doc.FindObjectIndexById(copyId);
        const Vec3 delta{75,40,250},axis{0,1,0};const float angle=.35f;
        doc.MoveSelectedObjects(delta);doc.RotateSelectedObjects({0,0,0},axis,angle);
        const auto originalShape=dynamic_cast<CSolid*>(doc.FindObjectById(sourceId))->m_Shape;
        window.active_parametric_edit_existing_=true;
        for(double value:{.22,.31}) {
            auto* copy=dynamic_cast<CSolid*>(doc.FindObjectById(copyId));
            window.active_parametric_object_=window.tool_registry_.ActiveObjectFromDocument(index,*copy,0,&doc);
            window.property_panel_->SetActiveObject(window.active_parametric_object_);
            auto* editor=window.property_panel_->findChild<QDoubleSpinBox*>("parameter_front.up.y");
            require(editor,"Missing Body-1 parameter editor");editor->setValue(value);
            // Exercise the same ParametersChanged path as the operation editor.
            window.property_panel_->ParametersChanged();
            require(window.property("surfaceFilletError").toString().isEmpty(),"Copy parameter rebuild failed");
            settings.curvature[4]=value;auto expectedShape=BuildBody1(doc,settings,error);
            CSolid expected(expectedShape);expected.Translate(delta);expected.Rotate({0,0,0},axis,angle);
            copy=dynamic_cast<CSolid*>(doc.FindObjectById(copyId));
            GProp_GProps actualMass,expectedMass;BRepGProp::VolumeProperties(copy->m_Shape,actualMass);BRepGProp::VolumeProperties(expected.m_Shape,expectedMass);
            require(actualMass.CentreOfMass().Distance(expectedMass.CentreOfMass())<.001,"Body copy lost placement while editing parameters");
            require(std::abs(actualMass.Mass()-expectedMass.Mass())<.01,"Body copy parameter edit produced wrong geometry");
            require(copy->GetNumOperations()==3,"Copy lost its Move/Rotate history");
            require(dynamic_cast<CSolid*>(doc.FindObjectById(sourceId))->m_Shape.IsSame(originalShape),"Editing copy changed source body");
        }
        auto* guide=dynamic_cast<CBSpline*>(doc.FindObjectById(settings.guides[0]));auto point=guide->GetPoints()[1];point.y+=5;guide->SetPointDirect(1,point);
        require(window.tool_registry_.ReplayProfileDependents(settings.guides[0],doc),"Shared guide no longer updates bodies");
        auto expectedShape=BuildBody1(doc,settings,error);CSolid expected(expectedShape);expected.Translate(delta);expected.Rotate({0,0,0},axis,angle);
        GProp_GProps actualMass,expectedMass;BRepGProp::VolumeProperties(dynamic_cast<CSolid*>(doc.FindObjectById(copyId))->m_Shape,actualMass);BRepGProp::VolumeProperties(expected.m_Shape,expectedMass);
        require(actualMass.CentreOfMass().Distance(expectedMass.CentreOfMass())<.001,"Guide replay lost edited copy placement");
        std::cout<<"Body-1 copy: parameter preview preserves Move/Rotate, repeated edits and shared guides passed\n";
        return EXIT_SUCCESS;
    }
    #include "ClonePlacementTestCases.inc"
    if(application.arguments().contains("--freeze-object-only")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;auto& doc=window.document_;doc.Clear();
        TopoDS_Shape boxShape=BRepPrimAPI_MakeBox(40,30,25).Shape();
        auto box=std::make_unique<CSolid>(boxShape);box->SetName("Freeze test box");
        auto* raw=box.get();doc.AddObject(std::move(box));const auto id=raw->m_id;
        doc.SelectObjectById(id);window.undo_redo_.Reset();window.RefreshSceneTree();
        QTreeWidgetItem* row=nullptr;
        std::function<void(QTreeWidgetItem*)> find=[&](QTreeWidgetItem* item) {
            if(item->text(1)=="Freeze test box")row=item;
            for(int i=0;i<item->childCount();++i)find(item->child(i));
        };
        for(int i=0;i<window.scene_tree_->topLevelItemCount();++i)find(window.scene_tree_->topLevelItem(i));
        require(row&&!row->icon(3).isNull(),"Freeze icon missing");
        const auto alpha=doc.FindObjectById(id)->GetMaterial().alpha;
        window.OnSceneTreeItemClicked(row,3);
        require(doc.FindObjectById(id)->IsFrozen()&&doc.IsObjectVisible(*doc.FindObjectById(id)),"Freeze hid the object or did not set flag");
        require(!doc.HasSelection()&&!doc.SelectObjectById(id),"Frozen object is selectable");
        doc.SelectAllVisibleObjects();require(!doc.IsObjectSelected(doc.FindObjectIndexById(id)),"Select all includes frozen object");
        require(doc.FindObjectById(id)->GetMaterial().alpha==alpha,"Freeze modified material");
        require(window.undo_redo_.Undo()&&!doc.FindObjectById(id)->IsFrozen(),"Cannot undo freeze");
        require(window.undo_redo_.Redo()&&doc.FindObjectById(id)->IsFrozen(),"Cannot redo freeze");
        Dom3DProjectSerializer serializer;ProjectViewState view;QString error,room;
        const auto path=temp.filePath("frozen.dom3d");
        require(serializer.Save(path,static_cast<const CAlfaDoc&>(doc),room,view,{},error),"Cannot save frozen object");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,error),"Cannot load frozen object");
        require(loaded.FindObjectById(id)->IsFrozen()&&!loaded.SelectObjectById(id),"Frozen state lost on load");
        SetAlfaDoc(&doc);
        if(application.arguments().contains("--freeze-preview")) {
            window.resize(1100,800);window.show();window.RefreshSceneTree();window.viewport_->FitToDocument();
            QElapsedTimer wait;wait.start();while(wait.elapsed()<600)application.processEvents();
            require(window.viewport_->grabFramebuffer().save("C:/My_projects/Dom3D_Pro/tmp/frozen-object.png"),"Cannot save frozen preview");
            require(window.grab().save("C:/My_projects/Dom3D_Pro/tmp/frozen-tree.png"),"Cannot save freeze tree preview");
        }
        doc.SetObjectFrozen(id,false);require(doc.SelectObjectById(id),"Unfreeze did not restore selection");
        auto line=std::make_unique<CPolyline>();line->AddPoint({0,0,0});line->AddPoint({10,0,0});
        auto* lineRaw=line.get();doc.AddObject(std::move(line));const auto lineId=lineRaw->m_id;
        doc.SelectObjectById(id);doc.SelectObjectById(lineId,SelectionAction::Add);require(doc.CreateGroupFromSelection(),"Cannot create freeze group");
        const auto groupId=doc.GetSelectedObject()->m_id;
        doc.SetObjectFrozen(groupId,true);
        require(doc.FindObjectById(id)->IsFrozen()&&doc.FindObjectById(lineId)->IsFrozen(),"Group freeze missed children");
        doc.SetObjectFrozen(groupId,false);doc.SetObjectFrozen(lineId,true);
        require(!doc.SelectObjectById(groupId),"Group can edit frozen child");
        doc.SetObjectFrozen(groupId,false);require(doc.SelectObjectById(groupId),"Group thaw failed");
        std::cout<<"Freeze icon, selection protection, groups, persistence, undo/redo passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--array-pick-only")) {
        QTemporaryDir temporary;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
        MainWindow window;application.processEvents();auto& doc=window.document_;
        const auto launch=[&](int kind) {
            if(kind==0)window.ShowLinearArrayDialog();
            else if(kind==1)window.ShowRadialArrayDialog();
            else if(kind==2)window.ShowRectangularArrayDialog();
            else window.ShowCurveArrayDialog();
        };
        for(int kind=0;kind<4;++kind) {
            doc.Clear();
            auto source=std::make_unique<CPolyline>();source->AddPoint({9,0,0});source->AddPoint({11,0,0});
            const auto* sourcePtr=source.get();doc.AddObject(std::move(source));const auto sourceId=sourcePtr->m_id;
            auto guide=std::make_unique<CPolyline>();guide->AddPoint({0,0,0});guide->AddPoint({0,100,0});
            doc.AddObject(std::move(guide));window.undo_redo_.Reset();doc.ClearSelection();
            const auto base=doc.GetObjects().size();
            launch(kind);
            require(window.pending_group_command_!=MainWindow::PendingGroupCommand::None,"Array did not wait for a click");
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(window.viewport_,&escape);
            require(window.pending_group_command_==MainWindow::PendingGroupCommand::None,"Esc did not cancel array selection");
            // Changing tools before the queued click handler runs must cancel it.
            launch(kind);doc.SelectObjectById(sourceId);window.viewport_->SelectionChanged();
            window.SetTool(ToolMode::Select,{});application.processEvents();
            require(!window.findChild<QDialog*>("LinearArrayDialog")&&!window.findChild<QDialog*>("RadialArrayDialog")
                &&!window.findChild<QDialog*>("RectangularArrayDialog")&&!window.findChild<QDialog*>("CurveArrayDialog"),"Stale array click opened a dialog");
            for(bool accept:{false,true}) {
                doc.ClearSelection();launch(kind);
                const QString name=kind==0?"LinearArrayDialog":kind==1?"RadialArrayDialog":kind==2?"RectangularArrayDialog":"CurveArrayDialog";
                bool opened=false;QTimer finish;
                QObject::connect(&finish,&QTimer::timeout,[&]() {
                    auto* dialog=window.findChild<QDialog*>(name);if(!dialog)return;
                    opened=true;finish.stop();
                    require(window.pending_group_command_==MainWindow::PendingGroupCommand::None,"Array pick remained armed during preview");
                    if(kind==3)dialog->findChild<QComboBox*>("CurveArrayGuide")->setCurrentIndex(1);
                    dialog->findChild<QDialogButtonBox*>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
                });
                finish.start(100);
                doc.SelectObjectById(sourceId);window.viewport_->SelectionChanged();
                QElapsedTimer wait;wait.start();while(!opened&&wait.elapsed()<2000)application.processEvents();
                require(opened,"Selecting an object did not open array parameters without Enter");
                std::cout<<"Array pick kind="<<kind<<" accept="<<accept<<" objects="<<doc.GetObjects().size()<<std::endl;
                require(accept?doc.GetObjects().size()>base:doc.GetObjects().size()==base,"Array pick accept/cancel changed wrong objects");
                require(window.undo_redo_.UndoCount()==(accept?1:0),"Array pick changed undo history incorrectly");
                if(accept)require(window.undo_redo_.Undo()&&doc.GetObjects().size()==base,"Cannot undo picked array");
            }
        }
        doc.Clear();
        std::vector<unsigned long> ids;
        for(int i=0;i<3;++i) {
            auto line=std::make_unique<CPolyline>();line->AddPoint({float(i*10),0,0});line->AddPoint({float(i*10),100,0});
            auto* raw=line.get();doc.AddObject(std::move(line));ids.push_back(raw->m_id);
        }
        doc.ClearSelection();doc.SelectObjectById(ids[0]);
        doc.FindObjectById(ids[2])->SetVisible(false);
        QTimer::singleShot(0,[&]() {
            auto* dialog=window.findChild<QDialog*>("CurveArrayDialog");require(dialog,"Single visible guide was not automatic");
            auto* combo=dialog->findChild<QComboBox*>("CurveArrayGuide");
            require(combo->currentData().toULongLong()==ids[1],"Automatic array guide is wrong");
            require(combo->findData(QVariant::fromValue<qulonglong>(ids[2]))<0,"Hidden guide offered for array");
            dialog->reject();
        });
        window.ShowCurveArrayDialog();
        doc.FindObjectById(ids[2])->SetVisible(true);
        window.ShowCurveArrayDialog();
        require(window.pending_group_command_==MainWindow::PendingGroupCommand::ArrayCurveGuide,"Multiple guides were assigned automatically");
        doc.SelectObjectById(ids[0]);window.viewport_->SelectionChanged();application.processEvents();
        require(window.pending_group_command_==MainWindow::PendingGroupCommand::ArrayCurveGuide,"Source accepted as its own guide");
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(window.viewport_,&escape);
        require(doc.GetSelectedObject()->m_id==ids[0],"Cancel lost source selection");
        window.ShowCurveArrayDialog();
        bool picked=false;QTimer closeGuide;
        QObject::connect(&closeGuide,&QTimer::timeout,[&]() {
            auto* dialog=window.findChild<QDialog*>("CurveArrayDialog");if(!dialog)return;
            picked=true;closeGuide.stop();
            require(dialog->findChild<QComboBox*>("CurveArrayGuide")->currentData().toULongLong()==ids[2],"Clicked guide was not assigned");
            dialog->reject();
        });
        closeGuide.start(100);doc.SelectObjectById(ids[2]);window.viewport_->SelectionChanged();
        QElapsedTimer wait;wait.start();while(!picked&&wait.elapsed()<2000)application.processEvents();
        require(picked&&doc.GetSelectedObject()->m_id==ids[0],"Guide click lost sources or did not open parameters");
        std::cout<<"All four arrays: click selection, Escape, tool switching, accept/cancel and undo passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--basic-array-preview-only")) {
        QTemporaryDir temporary;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
        MainWindow window;
        auto& doc=window.document_;
        for(int kind=0;kind<3;++kind) {
            doc.Clear();
            auto source=std::make_unique<CPolyline>(); source->AddPoint({9,0,0}); source->AddPoint({11,0,0});
            auto* ptr=source.get(); doc.AddObject(std::move(source)); doc.SelectObjectById(ptr->m_id);
            window.undo_redo_.Reset(); window.undo_redo_.BeginChange(); doc.MoveSelectedObjects({1,0,0});
            window.undo_redo_.CommitChange("Seed redo"); require(window.undo_redo_.Undo(),"Cannot seed redo history");
            const auto base=doc.GetObjects().size(), undo=window.undo_redo_.UndoCount(), redo=window.undo_redo_.RedoCount();
            // A toolbar nested inside a dock reproduces the inherited-enabled-state bug.
            auto* regression_dock=new QDockWidget("Array test dock",&window);
            auto* regression_toolbar=new QToolBar(regression_dock);
            regression_dock->setWidget(regression_toolbar);
            window.addDockWidget(Qt::LeftDockWidgetArea,regression_dock);
            auto* action=regression_toolbar->addAction("Test command");
            require(regression_dock->isEnabled() && regression_toolbar->isEnabled() && action->isEnabled(),"Toolbar fixture disabled");
            for(bool accept:{false,true}) {
                QTimer::singleShot(0,[&,kind,accept]() {
                    const QString name=kind==0?"LinearArrayDialog":kind==1?"RadialArrayDialog":"RectangularArrayDialog";
                    auto* dialog=window.findChild<QDialog*>(name); require(dialog,"Array dialog missing");
                    require(!dialog->isModal() && QApplication::activeModalWidget()==nullptr && window.viewport_->isEnabled(),
                            "Array window blocks viewport navigation");
                    const auto distance=window.viewport_->GetCamera().distance;
                    QWheelEvent wheel(QPointF(100,100),QPointF(100,100),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                    QApplication::sendEvent(window.viewport_,&wheel);
                    require(window.viewport_->GetCamera().distance!=distance,"Cannot zoom scene with array window open");
                    require(dialog->findChildren<QSlider*>().empty(), "Array still contains sliders");
                    // Exercise the label gesture, not just programmatic spin-box changes.
                    QLabel* drag_label = nullptr;
                    for (auto* label : dialog->findChildren<QLabel*>())
                        if (label->cursor().shape() == Qt::SizeHorCursor) { drag_label = label; break; }
                    require(drag_label, "Array drag label missing");
                    auto* count = dialog->findChild<QSpinBox*>();
                    const int old_count = count->value();
                    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5,5), QPointF(100,100), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent move(QEvent::MouseMove, QPointF(25,5), QPointF(120,100), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(25,5), QPointF(120,100), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(drag_label, &press);
                    QApplication::sendEvent(drag_label, &move);
                    QApplication::sendEvent(drag_label, &release);
                    require(count->value() == old_count + 2, "Dragging quantity label did not change count");

                    require(regression_dock->isEnabled() && regression_toolbar->isEnabled() && action->isEnabled(),
                            "Array preview grays out panels or commands");
                    const auto counts=dialog->findChildren<QSpinBox*>();
                    counts[0]->setValue(kind==2?2:4); if(kind==2) counts[1]->setValue(2);
                    const auto values=dialog->findChildren<QDoubleSpinBox*>();
                    if(kind==0) values[0]->setValue(5);
                    if(kind==1) { values[0]->setValue(180); for(int i=1;i<values.size();++i) values[i]->setValue(0); }
                    if(kind==2) {values[0]->setValue(5); values[1]->setValue(7);}
                    dialog->findChild<QComboBox*>()->setCurrentIndex(kind==1?2:1);
                    QTimer::singleShot(160,dialog,[&,dialog,kind,accept]() {
                        require(doc.GetObjects().size()==base+3,"Initial basic array preview missing");
                        require(window.undo_redo_.UndoCount()==undo && window.undo_redo_.RedoCount()==redo,"Preview changed history");
                        dialog->findChildren<QSpinBox*>()[0]->setValue(kind==2?150:1000);
                        QTimer::singleShot(160,dialog,[&,dialog,kind,accept]() {
                            require(doc.GetObjects().size()==base,"Oversized preview generated copies");
                            auto* notice=dialog->findChild<QLabel*>("ArrayPreviewBudgetNotice");
                            require(notice && !notice->isHidden(),"Preview limit not explained");
                            dialog->findChildren<QSpinBox*>()[0]->setValue(3);
                        QTimer::singleShot(160,dialog,[&,dialog,kind,accept]() {
                            require(dialog->findChild<QLabel*>("ArrayPreviewBudgetNotice")->isHidden(),"Preview limit did not reset");
                            require(doc.GetObjects().size()==base+(kind==2?5:2),"Preview accumulated copies");
                            const auto* last=dynamic_cast<const CPolyline*>(doc.GetObjects().back().get()); require(last,"Preview curve missing");
                            const auto a=last->GetPoints()[0],b=last->GetPoints()[1];
                            const Vec3 expected=kind==0?Vec3{10,10,0}:kind==1?Vec3{-10,0,0}:Vec3{20,0,7};
                            require(std::abs((a.x+b.x)/2-expected.x)<0.001 && std::abs((a.y+b.y)/2-expected.y)<0.001
                                    && std::abs((a.z+b.z)/2-expected.z)<0.001,"Preview ignored geometry settings");
                            dialog->findChild<QDialogButtonBox*>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
                        });
                    });
                    });
                });
                if(kind==0) window.ShowLinearArrayDialog(); else if(kind==1) window.ShowRadialArrayDialog(); else window.ShowRectangularArrayDialog();
                require(regression_dock->isEnabled() && regression_toolbar->isEnabled() && action->isEnabled(),
                        "Array window left a toolbar disabled after closing");
                require(doc.GetObjects().size()==base+(accept?(kind==2?5:2):0),"Array accept/cancel result wrong");
                require(window.undo_redo_.UndoCount()==undo+(accept?1:0),"Array confirmation not a single undo step");
                if(!accept) require(window.undo_redo_.RedoCount()==redo,"Cancel discarded redo history");
            }
            require(window.undo_redo_.Undo() && doc.GetObjects().size()==base,"Cannot undo confirmed array");
            delete regression_dock;
        }
        std::cout<<"Linear, radial and rectangular live preview, replacement, geometry, cancellation and undo passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--curve-array-only")) {
        QTemporaryDir temporary;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
        MainWindow window;
        auto& doc=window.document_;
        doc.Clear();
        auto source=std::make_unique<CPolyline>(); source->AddPoint({100,0,0}); source->AddPoint({102,0,0});
        auto* original=source.get(); doc.AddObject(std::move(source)); const auto source_id=original->m_id;
        auto guide=std::make_unique<CPolyline>();
        guide->AddPoint({0,0,0}); guide->AddPoint({10,0,0}); guide->AddPoint({10,30,0});
        auto* path=guide.get(); doc.AddObject(std::move(guide)); const auto guide_id=path->m_id;
        const size_t before=doc.GetObjects().size();
        doc.SelectObjectById(source_id); doc.SelectObjectById(guide_id,SelectionAction::Add);
        window.undo_redo_.Reset();
        require(window.CreateCurveArray(guide_id,5,true,true,0.5,{101,0,0}), "Cannot create curve array");
        require(doc.GetObjects().size()==before+5,"Guide was copied or array count is incorrect");
        const std::array<Vec3,5> centers{Vec3{0,0,0},Vec3{10,0,0},Vec3{10,10,0},Vec3{10,20,0},Vec3{10,30,0}};
        for(size_t i=0;i<5;++i) {
            const auto* copy=dynamic_cast<CPolyline*>(doc.GetObjects()[before+i].get());
            require(copy && copy->GetPointCount()==2,"Array copy geometry missing");
            const auto a=copy->GetPoints()[0], b=copy->GetPoints()[1];
            require(std::abs((a.x+b.x)/2-centers[i].x)<0.001 && std::abs((a.y+b.y)/2-centers[i].y)<0.001,
                    "Array members are not equally spaced by curve length");
            const double length=std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
            require(std::abs(length-2*(1-0.5*i/4))<0.001,"Array scale ramp is wrong");
            if(i>0) require(std::abs(a.x-b.x)<0.001,"Array did not follow tangent");
        }
        require(original->GetPoints()[0].x==100 && path->GetPointCount()==3,"Array modified its sources");
        Dom3DProjectSerializer serializer; QString error,room; ProjectViewState view;
        require(serializer.Save(temporary.filePath("array.dom3d"),doc,"Curves",view,{},error),"Cannot save curve array");
        CAlfaDoc loaded; require(serializer.Load(temporary.filePath("array.dom3d"),loaded,room,view,error)
            && loaded.GetObjects().size()==doc.GetObjects().size(),"Curve array did not survive reload");
        SetAlfaDoc(&doc);
        require(window.undo_redo_.Undo() && doc.GetObjects().size()==before,"Curve array undo failed");
        require(window.undo_redo_.Redo() && doc.GetObjects().size()==before+5,"Curve array redo failed");
        require(window.undo_redo_.Undo(),"Cannot reset array fixture");
        doc.SelectObjectById(source_id);
        require(window.CreateCurveArray(guide_id,5,false,false,2,{101,0,0}),"Cannot create offset curve array");
        require(doc.GetObjects().size()==before+4,"Offset array duplicated original member");
        const auto* last=dynamic_cast<CPolyline*>(doc.GetObjects().back().get());
        require(last && std::abs(last->GetPoints()[0].x-109)<0.001 && std::abs(last->GetPoints()[1].x-113)<0.001
            && std::abs(last->GetPoints()[0].y-30)<0.001,"Offset/orientation/scale mode is incorrect");
        require(window.undo_redo_.Undo(),"Cannot undo offset array");
        auto* closed=dynamic_cast<CPolyline*>(doc.FindObjectById(guide_id));
        closed->GetPoints()={{0,0,0},{10,0,0},{10,10,0},{0,10,0}}; closed->SetClosed(true);
        doc.SelectObjectById(source_id);
        require(window.CreateCurveArray(guide_id,4,true,false,1,{101,0,0}),"Cannot create closed array");
        last=dynamic_cast<CPolyline*>(doc.GetObjects().back().get());
        require(last && std::abs(last->GetPoints()[0].x+1)<0.001 && std::abs(last->GetPoints()[0].y-10)<0.001,
                "Closed array duplicates seam or misses final interval");
        require(window.undo_redo_.Undo(),"Cannot undo closed array");
        doc.SelectObjectById(source_id);
        QTimer::singleShot(0,[&]() {
            auto* dialog=window.findChild<QDialog*>("CurveArrayDialog"); require(dialog,"Curve array dialog missing");
            auto* combo=dialog->findChild<QComboBox*>("CurveArrayGuide");
            for(int i=0;i<combo->count();++i) if(combo->itemData(i).toULongLong()==guide_id) combo->setCurrentIndex(i);
            dialog->findChild<QSpinBox*>("CurveArrayCount")->setValue(3);
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        window.ShowCurveArrayDialog();
        require(doc.GetObjects().size()==before+3,"Curve array UI did not apply settings");
        require(window.undo_redo_.Undo(),"Cannot reset preview fixture");
        doc.SelectObjectById(source_id);
        const auto preview_count=doc.GetObjects().size();
        const auto undo_count=window.undo_redo_.UndoCount();
        const auto redo_count=window.undo_redo_.RedoCount();
        for (bool accept : {false,true}) {
            QTimer::singleShot(0,[&,accept]() {
                auto* dialog=window.findChild<QDialog*>("CurveArrayDialog"); require(dialog,"Preview dialog missing");
                require(!dialog->isModal() && QApplication::activeModalWidget()==nullptr && window.viewport_->isEnabled(),
                        "Curve array window is still modal");
                auto* combo=dialog->findChild<QComboBox*>("CurveArrayGuide");
                for(int i=0;i<combo->count();++i) if(combo->itemData(i).toULongLong()==guide_id) combo->setCurrentIndex(i);
                require(dialog->findChildren<QSlider*>().empty(), "Curve array still contains sliders");
                dialog->findChild<QSpinBox*>("CurveArrayCount")->setValue(5);
                dialog->findChild<QDoubleSpinBox*>("CurveArrayEndScale")->setValue(0.5);
                QTimer::singleShot(160,dialog,[&,dialog,accept]() {
                    require(doc.GetObjects().size()==preview_count+5,"Array preview not visible before OK");
                    require(window.undo_redo_.UndoCount()==undo_count && window.undo_redo_.RedoCount()==redo_count,
                            "Preview modified undo/redo history");
                    dialog->findChild<QSpinBox*>("CurveArrayCount")->setValue(10000);
                    QTimer::singleShot(160,dialog,[&,dialog,accept]() {
                        require(doc.GetObjects().size()==preview_count,"Oversized curve preview generated copies");
                        require(!dialog->findChild<QLabel*>("ArrayPreviewBudgetNotice")->isHidden(),"Curve preview limit not explained");
                        dialog->findChild<QSpinBox*>("CurveArrayCount")->setValue(4);
                    QTimer::singleShot(160,dialog,[&,dialog,accept]() {
                        require(doc.GetObjects().size()==preview_count+4,"Preview accumulated old copies");
                        dialog->findChild<QDialogButtonBox*>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
                    });
                });
                });
            });
            window.ShowCurveArrayDialog();
            require(doc.GetObjects().size()==preview_count+(accept?4:0),"Preview confirmation/cancellation left wrong objects");
            require(window.undo_redo_.UndoCount()==undo_count+(accept?1:0),"Preview did not create exactly one undo on OK");
            if(!accept) require(window.undo_redo_.RedoCount()==redo_count,"Cancel lost redo history");
        }
        require(window.undo_redo_.Undo() && doc.GetObjects().size()==preview_count,"Undo did not remove confirmed preview");
        // Exercise a genuine spatial Bezier spline with nonuniform parameter speed.
        auto spline=std::make_unique<CBSpline>(); spline->SetCurveType(SplineCurveType::Bezier);
        for(auto p : {CPoint3d(0,0,0),CPoint3d(20,0,10),CPoint3d(20,40,-10),CPoint3d(30,50,20)}) spline->AddPoint(p);
        auto* spatial=spline.get(); doc.AddObject(std::move(spline)); const auto spatial_id=spatial->m_id;
        std::vector<CPoint3d> dense; std::vector<double> lengths{0};
        for(int i=0;i<=2048;++i) {
            dense.push_back(spatial->Evaluate(i/2048.f));
            if(i) { const auto a=dense.back(), b=dense[dense.size()-2]; const CPoint3d d(a.x-b.x,a.y-b.y,a.z-b.z); lengths.push_back(lengths.back()+std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z)); }
        }
        doc.SelectObjectById(source_id); const auto spatial_before=doc.GetObjects().size();
        require(window.CreateCurveArray(spatial_id,7,true,true,1,{101,0,0}),"Spatial spline array failed");
        for(size_t i=0;i<7;++i) {
            const auto* copy=dynamic_cast<const CPolyline*>(doc.GetObjects()[spatial_before+i].get());
            const auto a=copy->GetPoints()[0],b=copy->GetPoints()[1];
            const CPoint3d center((a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2);
            size_t nearest=0; double best=1e30;
            for(size_t j=0;j<dense.size();++j) { const CPoint3d d(dense[j].x-center.x,dense[j].y-center.y,dense[j].z-center.z);const double sq=d.x*d.x+d.y*d.y+d.z*d.z;if(sq<best){best=sq;nearest=j;} }
            require(std::sqrt(best)<0.04 && std::abs(lengths[nearest]-lengths.back()*i/6)<0.04,
                    "Spatial spline array is not spaced by arc length");
        }
        TopoDS_Shape shape=BRepPrimAPI_MakeBox(gp_Pnt(100,0,0),2,2,2).Shape();
        auto solid=std::make_unique<CSolid>(shape); require(solid->ReBuldMesh(),"Cannot mesh array source");
        auto* solid_ptr=solid.get(); doc.AddObject(std::move(solid));
        auto group=std::make_unique<CGroup>("Source group",std::vector<unsigned long>{solid_ptr->m_id,source_id});
        auto* group_ptr=group.get(); doc.AddObject(std::move(group)); const auto group_id=group_ptr->m_id;
        const auto solid_id = solid_ptr->m_id;
        doc.SelectObjectById(group_id); const auto group_before=doc.GetObjects().size();
        const auto group_snapshot = doc.CreateSnapshot();
        QElapsedTimer array_timer; array_timer.start();
        require(window.CreateCurveArray(spatial_id,3,true,true,0.5,{101,1,1},false,true), "Cannot preview solid group array");
        const auto preview_ms = array_timer.elapsed();
        std::vector<std::pair<Vec3,Vec3>> preview_bounds;
        for (size_t i=group_before; i<doc.GetObjects().size(); ++i) {
            Vec3 low{}, high{};
            require(doc.GetObjects()[i]->GetBounds(low,high), "Preview bounds missing");
            preview_bounds.emplace_back(low,high);
        }
        require(doc.RestoreSnapshot(*group_snapshot), "Cannot restore solid array fixture");
        array_timer.restart();
        require(window.CreateCurveArray(spatial_id,3,true,true,0.5,{101,1,1}),"Cannot array a group of solid and curve");
        std::cout << "Solid group array: preview " << preview_ms << " ms, commit " << array_timer.elapsed() << " ms\n";
        for (size_t i=group_before; i<doc.GetObjects().size(); ++i) {
            Vec3 low{}, high{};
            require(doc.GetObjects()[i]->GetBounds(low,high), "Committed bounds missing");
            const auto& expected = preview_bounds.at(i-group_before);
            require(dot(low-expected.first,low-expected.first)<0.0001f && dot(high-expected.second,high-expected.second)<0.0001f,
                    "Solid group preview differs from committed geometry");
        }
        require(doc.GetObjects().size()==group_before+9,"Group array lost or duplicated members");
        for(auto index:doc.GetSelectedObjectIndices()) {
            const auto* copy=dynamic_cast<const CGroup*>(doc.GetObjects()[index].get());
            require(copy && copy->GetElementIds().size()==2,"Array group membership missing");
            for(auto id:copy->GetElementIds()) require(id!=source_id && id!=solid_id && doc.FindObjectById(id),"Array group references original children");
        }
        std::cout<<"Curve array spacing, spatial spline, groups, rotation, scale, offset, closed seam, undo, persistence and dialog passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--validate-save-only")) {
        QTemporaryDir temporary;
        require(temporary.isValid(), "Cannot create save-validation directory");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        MainWindow window;
        auto& doc = window.document_;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        const auto args = application.arguments();
        require(serializer.Load(args.value(args.indexOf("--validate-save-only")+1), doc, room, view, error),
                "Cannot load Work_Plane validation fixture");
        require(doc.GetObjects().size() == 6 && doc.HasInvalidObjectsForSave(), "Missing empty Curve 1 fixture");
        auto empty_spline = std::make_unique<CBSpline>();
        auto* empty_pointer = empty_spline.get();
        doc.AddObject(std::move(empty_spline));
        const auto empty_id = empty_pointer->m_id;
        doc.AddObject(std::make_unique<CSmartLine>());
        doc.AddObject(std::make_unique<CMesh3D>());
        doc.AddObject(std::make_unique<CSolid>());
        auto broken = std::make_unique<CPolyline>();
        broken->AddPoint({std::numeric_limits<double>::quiet_NaN(), 0, 0});
        doc.AddObject(std::move(broken));
        auto group = std::make_unique<CGroup>("Retained group", std::vector<unsigned long>{1, empty_id, 5});
        auto* group_pointer = group.get();
        doc.AddObject(std::move(group));
        const auto group_id = group_pointer->m_id;
        doc.SetGroupInteractionEnabled(false);
        doc.SelectObjectById(5);
        doc.SetDraftingData("{\"testSheet\":true}");
        SetAlfaDoc(&doc);
        const auto old_count = doc.GetObjects().size();
        const auto old_index = doc.FindObjectIndexById(5);
        window.active_parametric_object_.tool_id = "BSpline";
        window.active_parametric_object_.object_index = old_index;
        require(!window.SaveValidatedProject(temporary.filePath("missing/file.dom3d"), room, view, error),
                "Saving to nonexistent directory unexpectedly succeeded");
        require(doc.GetObjects().size() == old_count && doc.IsObjectSelected(old_index),
                "Failed save changed live document or selection");
        const QString path = temporary.filePath("clean.dom3d");
        require(window.SaveValidatedProject(path, room, view, error), "Validated save failed");
        require(!doc.HasInvalidObjectsForSave() && doc.GetObjects().size() == 6,
                "Invalid geometry survived or valid hidden geometry was deleted");
        require(GetAlfaDoc() == &doc, "Save changed current document");
        require(doc.GetSelectedObjectCount() == 1 && doc.IsObjectSelected(doc.FindObjectIndexById(5)),
                "Cleanup lost selected spline");
        require(window.active_parametric_object_.object_index == doc.FindObjectIndexById(5),
                "Cleanup left stale editor index");
        const auto* kept_group = dynamic_cast<const CGroup*>(doc.GetObjects()[doc.FindObjectIndexById(group_id)].get());
        require(kept_group && kept_group->GetElementIds() == std::vector<unsigned long>{5},
                "Cleanup left dangling group members");
        CAlfaDoc loaded;
        require(serializer.Load(path, loaded, room, view, error), "Cannot reload sanitized project");
        require(loaded.GetDraftingData() == doc.GetDraftingData(), "Cleanup lost drawing sheets");
        require(loaded.GetObjects().size() == doc.GetObjects().size() && !loaded.HasInvalidObjectsForSave(),
                "Invalid objects were serialized");
        require(window.SaveValidatedProject(path, room, view, error) && doc.GetObjects().size() == 6,
                "Second save was not idempotent");
        CAlfaDoc empty;
        empty.AddObject(std::make_unique<CPolyline>());
        require(serializer.Save(temporary.filePath("empty.dom3d"), empty, room, view, {}, error)
                && empty.GetObjects().empty(), "Saving an empty scene recreated a placeholder");
        std::cout << "Save validation, failure preservation, selection and group cleanup passed" << std::endl;
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--face-selection-display-only")) {
        CAlfaDoc doc;
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(gp_Pnt(-10,-10,-10),20,20,20).Shape();
        auto body = std::make_unique<CSolid>(shape);
        require(body->ReBuldMesh(), "Cannot mesh face-selection fixture");
        auto* solid = body.get();
        doc.AddObject(std::move(body));
        doc.ClearSelection();
        OpenGLViewport viewport;
        viewport.SetDocument(&doc);
        viewport.resize(800,600);
        viewport.move(-30000,-30000);
        viewport.SetFloorGridVisible(false);
        viewport.SetCoordinateAxesVisible(false);
        viewport.SetOrthographicProjection(true);
        Camera camera;
        camera.target = {0,0,0}; camera.distance = 85;
        camera.orientation = camera_orientation_from_yaw_pitch(35,25);
        viewport.SetCamera(camera);
        viewport.SetSelectionMode(SelectionMode::Face);
        viewport.show(); application.processEvents();
        CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        CSolid::SetHiddenEdgeDrawingEnabled(false);
        CSolid::SetSurfaceTransparencyEnabled(false);
        CMesh3D::SetSurfaceOpacity(1.f);
        QPoint front, side;
        require(viewport.ProjectWorldPoint({0,0,10},front) && viewport.ProjectWorldPoint({10,0,0},side),
                "Cannot project face-selection probes");
        auto patch = [](const QImage& frame, QPoint pixel) {
            return frame.copy(pixel.x()-3,pixel.y()-3,7,7);
        };
        QDir().mkpath("output/face-selection");
        for (auto mode : {MeshDisplayMode::SurfaceGray, MeshDisplayMode::SurfaceMaterial,
                          MeshDisplayMode::SurfaceColored}) {
            for (bool edges : {false,true}) {
                CMesh3D::SetDisplayMode(mode);
                CSolid::SetEdgeDrawingEnabled(edges);
                doc.ClearSelection();
                const auto baseline = viewport.CaptureSceneImage({800,600});
                require(!baseline.isNull(), "Face-selection capture failed");
                viewport.SelectAt(front,SelectionAction::Replace);
                require(doc.HasSelectedSolidFace() && doc.GetSelectedObjectCount() == 0
                        && !doc.IsObjectSelected(0), "Picking a face selected its body");
                const auto selected = viewport.CaptureSceneImage({800,600});
                require(patch(selected,front) != patch(baseline,front), "Selected face is not highlighted");
                require(patch(selected,side) == patch(baseline,side), "Face selection recolored another face");
                viewport.SelectAt(side,SelectionAction::Add);
                require(solid->GetSelectedFaceIndices().size() == 2, "Cannot select multiple faces");
                const auto multiple = viewport.CaptureSceneImage({800,600});
                require(patch(multiple,front) == patch(selected,front)
                        && patch(multiple,side) != patch(baseline,side), "Multiple face highlight is incorrect");
                doc.ClearSelection();
                require(viewport.CaptureSceneImage({800,600}) == baseline, "Clearing faces did not restore body appearance");
                doc.SelectObjectById(solid->m_id);
                const auto whole = viewport.CaptureSceneImage({800,600});
                require(patch(whole,front) != patch(baseline,front)
                        && patch(whole,side) != patch(baseline,side), "Whole-body selection lost highlighting");
                if (mode == MeshDisplayMode::SurfaceGray && edges) {
                    baseline.save("output/face-selection/plain.png");
                    selected.save("output/face-selection/face.png");
                    whole.save("output/face-selection/body.png");
                }
            }
        }
        viewport.SetDocument(nullptr);
        std::cout << "Face-only display and whole-body selection passed" << std::endl;
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--work-plane-trim-scene")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        MainWindow window;
        auto& doc = window.document_;
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room, error;
        const auto args = application.arguments();
        const QString path = args.value(args.indexOf("--work-plane-trim-scene")+1);
        require(serializer.Load(path, doc, room, view, error), "Cannot load reported Work_Plane scene");
        for (const auto& object : doc.GetObjects()) {
            std::cout << "Object " << object->m_id << " " << object->GetName() << " tool=" << object->GetParametricToolId() << "\n";
        }
        for (int tab=0; tab<window.tool_tabs_->count(); ++tab)
            if (window.tool_tabs_->tabData(tab).toString()=="Curves") window.tool_tabs_->setCurrentIndex(tab);
        doc.SelectObjectById(2);
        doc.SelectObjectById(5, SelectionAction::Add);
        window.RefreshSceneTree();
        QPushButton* button = nullptr;
        for (auto* candidate : window.findChildren<QPushButton*>())
            if (candidate->property("toolKey").toString()=="CurveTrimByPlane") { button=candidate; break; }
        require(button && button->isEnabled(), "Trim button unavailable on reported scene");
        button->click();
        std::cout << "Status: " << window.statusBar()->currentMessage().toStdString() << "\nSelection:";
        for (size_t index : doc.GetSelectedObjectIndices()) std::cout << " " << doc.GetObjects()[index]->m_id;
        std::cout << "\n";
        require(window.viewport_->point_pick_object_id_ == 5, "Reported scene preselection was not accepted");
        window.CancelCurveEditCommand();
        require(doc.SelectAllVisibleObjects() == 2, "Select All Visible included invisible empty Curve 1 placeholder");
        window.RefreshSceneTree();
        button->click();
        require(window.viewport_->point_pick_object_id_ == 5 && !window.waiting_curve_plane_selection_,
                "Ctrl+A preselection in Work_Plane scene was rejected");
        window.CancelCurveEditCommand();
        doc.SelectObjectById(1, SelectionAction::Add);
        button->click();
        require(window.viewport_->point_pick_object_id_ == 5 && !window.waiting_curve_plane_selection_,
                "Legacy selection containing empty Curve 1 still blocks trim");
        for (bool tree : {false, true}) {
            window.CancelCurveEditCommand();
            doc.ClearSelection();
            button->click();
            require(window.waiting_curve_plane_selection_, "Trim must wait for inputs");
            doc.SelectObjectById(2);
            emit window.viewport_->SelectionChanged();
            application.processEvents();
            require(window.waiting_curve_plane_selection_, "Plane alone started trim");
            doc.SelectObjectById(5, SelectionAction::Add);
            if (tree) {
                window.RefreshSceneTree();
                QTreeWidgetItem* row = nullptr;
                QTreeWidgetItemIterator items(window.scene_tree_);
                while (*items) {
                    const auto value = (*items)->data(0, Qt::UserRole+1);
                    if (value.isValid() && value.toULongLong() == doc.FindObjectIndexById(5)) { row=*items; break; }
                    ++items;
                }
                require(row != nullptr, "Cannot find reported spline tree row");
                // Selection was already applied above; exercise the deferred tree notification.
                QMetaObject::invokeMethod(&window, [&window]() { window.TryPrepareCurvePlaneSelection(); }, Qt::QueuedConnection);
            } else emit window.viewport_->SelectionChanged();
            application.processEvents();
            require(!window.waiting_curve_plane_selection_ && window.viewport_->point_pick_object_id_ == 5,
                    "Trim did not accept selected curve and plane without Enter");
            require(window.statusBar()->currentMessage().contains("part of the curve"), "Trim still prompts for selection");
        }
        // Plain replacement clicks: the second input must not need Shift.
        window.CancelCurveEditCommand();
        doc.ClearSelection();
        button->click();
        doc.SelectObjectById(5, SelectionAction::Replace);
        emit window.viewport_->SelectionChanged();
        application.processEvents();
        require(window.pending_trim_plane_face_pick_ && window.pending_trim_plane_curve_id_ == 5
                && window.viewport_->GetSelectionMode() == SelectionMode::Face,
                "First curve click did not enter plane/face picking");
        window.RefreshSceneTree();
        QTreeWidgetItem* plane_row = nullptr;
        for (QTreeWidgetItemIterator items(window.scene_tree_); *items; ++items) {
            const auto value = (*items)->data(0, Qt::UserRole+1);
            if (value.isValid() && value.toULongLong() == doc.FindObjectIndexById(2)) { plane_row=*items; break; }
        }
        require(plane_row, "Cannot find plane tree row");
        window.OnSceneTreeItemClicked(plane_row, 1);
        application.processEvents();
        require(!window.waiting_curve_plane_selection_ && window.viewport_->point_pick_object_id_ == 5,
                "Plain plane click lost the remembered spline");
        for (int plane = 0; plane < 3; ++plane) {
            window.CancelCurveEditCommand();
            doc.SelectObjectById(5);
            button->click();
            require(window.pending_trim_plane_face_pick_, "Curve preselection opened a dialog instead of waiting for a plane");
            window.viewport_->SelectOriginPlane(plane);
            application.processEvents();
            require(!window.waiting_curve_plane_selection_ && window.pending_curve_trim_plane_points_.size() == 3
                    && window.viewport_->point_pick_object_id_ == 5, "Origin plane click did not prepare trim");
        }
        window.CancelCurveEditCommand();
        doc.Clear();
        TopoDS_Shape cutting_body = BRepPrimAPI_MakeBox(gp_Pnt(0,0,0),20,20,20).Shape();
        auto body = std::make_unique<CSolid>(cutting_body);
        require(body->ReBuldMesh(), "Cannot mesh body for face trimming");
        doc.AddObject(std::move(body));
        auto curve = std::make_unique<CBSpline>();
        curve->AddPoint({10,10,-10}); curve->AddPoint({10,10,30}); curve->SetDegree(1);
        auto* curve_pointer = curve.get();
        doc.AddObject(std::move(curve));
        const auto curve_id = curve_pointer->m_id;
        doc.ClearSelection();
        button->click();
        doc.SelectObjectById(curve_id);
        emit window.viewport_->SelectionChanged(); application.processEvents();
        require(window.pending_trim_plane_face_pick_, "Curve click did not wait for body face");
        require(doc.SelectSolidFaceAtScreen({200,200}, [](Vec3 p, DomPoint& screen, float& depth) {
            screen={static_cast<int>(100+p.x*10),static_cast<int>(100+p.y*10)}; depth=100-p.z; return true;
        }), "Cannot select cutting body face");
        emit window.viewport_->SelectionChanged(); application.processEvents();
        require(!window.waiting_curve_plane_selection_ && !window.pending_trim_plane_face_pick_
                && window.viewport_->point_pick_object_id_ == curve_id, "Body face did not prepare trim");
        window.CompleteCurveEditPoint({10,10,30});
        const auto* trimmed = dynamic_cast<const CBSpline*>(doc.FindObjectById(curve_id));
        require(trimmed && std::abs(trimmed->Evaluate(1).z-20) < 0.001
                && std::abs(trimmed->Evaluate(0).z+10) < 0.001,
                "Face trim kept the clicked side or used the wrong plane");
        std::cout << "Reported Work_Plane preselection and selection after tool startup passed\n";
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--trim-curve-plane-only")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        MainWindow window;
        auto& doc = window.document_;
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room, failure;
        const auto args = application.arguments();
        const QString path = args.value(args.indexOf("--trim-curve-plane-only") + 1);
        auto load = [&] {
            require(serializer.Load(path, doc, room, view, failure), "Cannot load plane trim fixture");
            window.pending_curve_trim_plane_points_.clear();
            window.undo_redo_.Reset();
        };
        auto verify = [&](const CBSpline& original, const CBSpline* trimmed, bool positive) {
            require(trimmed && trimmed->GetCurveType() == original.GetCurveType(),
                    "Plane trim must preserve B-spline/Bezier type");
            require(trimmed->GetDegree() == original.GetDegree(), "Plane trim changed degree");
            if (original.IsBezierChain()) require(trimmed->IsBezierChain(), "Trim broke Bezier handles");
            const auto exact = curve_cut::Spline(original);
            for (int i = 0; i <= 200; ++i) {
                const auto p = trimmed->Evaluate(static_cast<float>(i) / 200);
                require(positive ? p.x >= -0.001 : p.x <= 0.001, "Trim retained the wrong side");
                GeomAPI_ProjectPointOnCurve projection(gp_Pnt(p.x, p.y, p.z), exact);
                require(projection.NbPoints() && projection.LowerDistance() < 0.001,
                        "Plane trim changed the authored curve shape");
            }
            const auto start = trimmed->Evaluate(0), end = trimmed->Evaluate(1);
            require(std::min(std::abs(start.x), std::abs(end.x)) < 0.001,
                    "Trim endpoint must lie on the plane");
            const auto retained = positive ? end : start;
            const auto expected = original.Evaluate(positive ? 1 : 0);
            require(gp_Pnt(retained.x, retained.y, retained.z).Distance(
                gp_Pnt(expected.x, expected.y, expected.z)) < 0.001, "Trim lost the retained endpoint");
        };
        for (auto mode : {SelectionMode::Object, SelectionMode::Face, SelectionMode::Edge, SelectionMode::Point}) {
            load();
            window.viewport_->SetSelectionMode(mode);
            doc.SelectObjectById(3);
            doc.SelectObjectById(5, SelectionAction::Add);
            window.BeginCurveEditCommand(MainWindow::CurveEditCommand::TrimByPlane);
            require(doc.GetSelectedObjectIndices().size() == 2, "Trim startup discarded preselected curve and plane");
            require(window.viewport_->picking_3d_point_ && window.viewport_->point_pick_object_id_ == 3,
                    "Trim startup did not advance to choosing the curve fragment");
            require(window.statusBar()->currentMessage().contains("part of the curve"), "Trim startup asks to select valid inputs again");
            window.CancelCurveEditCommand();
        }
        for (int tab = 0; tab < window.tool_tabs_->count(); ++tab)
            if (window.tool_tabs_->tabData(tab).toString() == "Curves") window.tool_tabs_->setCurrentIndex(tab);
        for (bool remove_positive : {false, true}) {
            load();
            const CBSpline original = *dynamic_cast<CBSpline*>(doc.FindObjectById(3));
            // A pending spatial curve tool clears selection during cleanup.
            window.spatial_curve_kind_ = MainWindow::SpatialCurveKind::BSpline;
            window.spatial_curve_object_id_ = 0;
            doc.SelectObjectById(3);
            doc.SelectObjectById(5, SelectionAction::Add);
            window.RefreshSceneTree();
            QPushButton* trim_button = nullptr;
            for (auto* button : window.findChildren<QPushButton*>())
                if (button->property("toolKey").toString() == "CurveTrimByPlane") { trim_button = button; break; }
            require(trim_button != nullptr, "Cannot find Trim by Plane button");
            trim_button->click();
            require(doc.GetSelectedObjectIndices().size() == 2 && window.viewport_->point_pick_object_id_ == 3,
                    "Toolbar startup lost preselection while clearing previous tool");
            require(window.statusBar()->currentMessage().contains("part of the curve"), "Toolbar did not accept preselection");
            window.CompleteCurveEditPoint(original.Evaluate(remove_positive ? 1 : 0));
            verify(original, dynamic_cast<CBSpline*>(doc.FindObjectById(3)), !remove_positive);
            require(window.pending_curve_edit_command_ == MainWindow::CurveEditCommand::None, "Trim click did not finish command");
        }
        {
            load();
            doc.SelectObjectById(3);
            doc.SelectObjectById(5, SelectionAction::Add);
            require(doc.RotateSelectedObjects({}, {0,0,1}, 0.7f), "Cannot rotate trim fixture");
            require(doc.MoveSelectedObjects({31,17,-9}), "Cannot translate trim fixture");
            const CBSpline original = *dynamic_cast<CBSpline*>(doc.FindObjectById(3));
            Vec3 origin{}, normal{};
            require(doc.GetObjectPlane(5, origin, normal), "Cannot resolve transformed cutting plane");
            const auto distance = [&](const CPoint3d& p) {
                return (p.x-origin.x)*normal.x + (p.y-origin.y)*normal.y + (p.z-origin.z)*normal.z;
            };
            const auto keep = original.Evaluate(1);
            require(window.TrimSelectedCurveByPlane(original.Evaluate(0)), "Cannot trim with transformed reference plane");
            const auto* trimmed = dynamic_cast<CBSpline*>(doc.FindObjectById(3));
            require(trimmed != nullptr, "Transformed plane trim lost spline");
            require(std::min(std::abs(distance(trimmed->Evaluate(0))), std::abs(distance(trimmed->Evaluate(1)))) < 0.001,
                    "Trim endpoint does not lie on transformed plane");
            const auto exact = curve_cut::Spline(original);
            for (int i=0; i<=100; ++i) {
                const auto point = trimmed->Evaluate(i/100.f);
                require(distance(point)*distance(keep) >= -0.001, "Transformed plane kept wrong side");
                GeomAPI_ProjectPointOnCurve projection(gp_Pnt(point.x,point.y,point.z),exact);
                require(projection.NbPoints() && projection.LowerDistance()<0.001, "Transformed plane trim changed spline shape");
            }
        }
        for (unsigned long id : {3ul, 4ul}) {
            for (bool positive : {false, true}) {
                load();
                const CBSpline original = *dynamic_cast<CBSpline*>(doc.FindObjectById(id));
                const size_t count = doc.GetObjects().size();
                doc.SelectObjectById(id);
                if (positive) {
                    // Exercise the alternate three-point/selected-face plane route.
                    window.pending_curve_trim_plane_points_ = {
                        CPoint3d(0, 0, 0), CPoint3d(0, 1, 0), CPoint3d(0, 0, 1)};
                } else doc.SelectObjectById(5, SelectionAction::Add);
                require(window.TrimSelectedCurveByPlane(original.Evaluate(positive ? 0 : 1)),
                        "Trim By Plane failed on reported curve");
                require(doc.GetObjects().size() == count, "One crossing must retain one fragment");
                verify(original, dynamic_cast<CBSpline*>(doc.FindObjectById(id)), positive);
                require(window.undo_redo_.Undo(), "Plane trim Undo failed");
                const auto* restored = dynamic_cast<CBSpline*>(doc.FindObjectById(id));
                require(restored && restored->GetPoints().size() == original.GetPoints().size(),
                        "Plane trim Undo did not restore poles");
                for (int i = 0; i <= 50; ++i) {
                    const auto a = restored->Evaluate(i / 50.f), b = original.Evaluate(i / 50.f);
                    require(gp_Pnt(a.x,a.y,a.z).Distance(gp_Pnt(b.x,b.y,b.z)) < 1.e-8,
                            "Plane trim Undo changed source shape");
                }
                require(window.undo_redo_.Redo(), "Plane trim Redo failed");
                verify(original, dynamic_cast<CBSpline*>(doc.FindObjectById(id)), positive);
                const QString saved = settings.filePath("trimmed.dom3d");
                require(serializer.Save(saved, doc, room, view, {}, failure), "Cannot save trimmed curves");
                CAlfaDoc loaded;
                require(serializer.Load(saved, loaded, room, view, failure), "Cannot reload trimmed curves");
                verify(original, dynamic_cast<CBSpline*>(loaded.FindObjectById(id)), positive);
            }
        }
        for (auto type : {SplineCurveType::BSpline, SplineCurveType::Bezier, SplineCurveType::Nurbs}) {
            CBSpline arch;
            arch.SetCurveType(type);
            for (auto p : {CPoint3d(0,0,0), CPoint3d(0,10,0), CPoint3d(10,10,0), CPoint3d(10,0,0)})
                arch.AddPoint(p);
            if (type == SplineCurveType::Nurbs) arch.SetWeights({1,2,2,1});
            const gp_Pln plane(gp_Pnt(0,5,0), gp_Dir(0,1,0));
            const auto pieces = curve_cut::TrimByPlane(arch, plane, false);
            require(pieces.size() == 2, "Plane trim must retain disconnected fragments");
            require(curve_cut::TrimByPlane(arch, plane, true).size() == 1,
                    "Plane trim must retain middle interval");
            const auto original = curve_cut::Spline(arch);
            for (const auto& piece : pieces) {
                CBSpline result;
                result.SetCurveType(type);
                require(curve_cut::Assign(result, piece), "Cannot assign plane trim fragment");
                for (int i = 0; i <= 50; ++i) {
                    const auto p = result.Evaluate(i / 50.f);
                    require(p.y <= 5.00001, "Plane trim joined across discarded interval");
                    GeomAPI_ProjectPointOnCurve projection(gp_Pnt(p.x,p.y,p.z), original);
                    require(projection.NbPoints() && projection.LowerDistance() < 1.e-5,
                            "Multiple-crossing plane trim changed shape");
                }
            }
            if (type != SplineCurveType::Nurbs)
                require(curve_cut::TrimByPlane(arch, gp_Pln(gp_Pnt(0,7.5,0),gp_Dir(0,1,0)), false).empty(),
                        "Tangency must not modify curve or create fragments");
            require(curve_cut::TrimByPlane(arch, gp_Pln(gp_Pnt(0,9,0),gp_Dir(0,1,0)), false).empty(),
                    "Crossing control poles alone must not trim the curve");
            require(curve_cut::TrimByPlane(arch, gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)), true).empty(),
                    "Coplanar curve must remain unchanged");
        }
        if (args.contains("--output")) {
            load();
            for (unsigned long id : {3ul,4ul}) {
                const auto* curve = dynamic_cast<const CBSpline*>(doc.FindObjectById(id));
                const auto remove = curve->Evaluate(0);
                doc.SelectObjectById(id);
                doc.SelectObjectById(5, SelectionAction::Add);
                require(window.TrimSelectedCurveByPlane(remove), "Cannot trim example");
            }
            require(serializer.Save(args.value(args.indexOf("--output")+1), doc, room, view, {}, failure),
                    "Cannot save fixed plane trim example");
        }
        std::cout << "Trim By Plane B-spline/Bezier regressions passed" << std::endl;
        return EXIT_SUCCESS;
    }
    if (application.arguments().contains("--cut-curve-only")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        MainWindow window;
        auto& doc = window.document_;
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room, failure;
        const auto args = application.arguments();
        require(serializer.Load(args.value(args.indexOf("--cut-curve-only") + 1),
                                doc, room, view, failure), "Cannot load Cut Curve regression");
        auto* target = dynamic_cast<CBSpline*>(doc.FindObjectById(2));
        const auto* copy = dynamic_cast<const CBSpline*>(doc.FindObjectById(3));
        auto* cutter = dynamic_cast<CPolyline*>(doc.FindObjectById(4));
        require(target && copy && cutter, "Cut Curve fixture objects missing");
        // The report was saved after the faulty cut. The translated copy still
        // contains all original cubic poles; restore the target from that copy.
        target->Clear();
        target->SetDegree(copy->GetDegree());
        for (auto p : copy->GetPoints()) { p.z = 0; target->AddPoint(p); }
        target->SetName("B-Spline 3D");
        auto& objects = doc.GetObjects();
        objects.erase(std::remove_if(objects.begin(), objects.end(), [](const auto& object) {
            return object->m_id == 5; // Remove the other fragment from the faulty cut.
        }), objects.end());
        const CBSpline original = *target;
        const auto copy_points = copy->GetPoints();
        const auto cutter_points = cutter->GetPoints();
        auto distance = [](CPoint3d a, CPoint3d b) {
            return gp_Pnt(a.x, a.y, a.z).Distance(gp_Pnt(b.x, b.y, b.z));
        };
        auto verify = [&](const CBSpline& before, const std::vector<const CBSpline*>& pieces,
                          double endpoint_tolerance = 1.e-6) {
            const auto exact = curve_cut::Spline(before);
            double previous = exact->FirstParameter();
            for (const auto* piece : pieces) {
                require(piece->GetDegree() == before.GetDegree(), "Cut reduced spline degree");
                GeomAPI_ProjectPointOnCurve end(
                    gp_Pnt(piece->GetPoints().back().x, piece->GetPoints().back().y,
                           piece->GetPoints().back().z), exact);
                require(end.NbPoints() > 0 && end.LowerDistance() < endpoint_tolerance,
                        "Cut endpoint moved off the original curve");
                const double next = end.LowerDistanceParameter();
                for (int i = 0; i <= 100; ++i) {
                    const float t = static_cast<float>(i) / 100;
                    require(distance(piece->Evaluate(t), before.Evaluate(
                        static_cast<float>(previous + (next - previous) * t))) < 0.001,
                        "Cut changed spline shape");
                }
                previous = next;
            }
            require(std::abs(previous - exact->LastParameter()) < 1.e-7,
                    "Cut did not cover the whole source curve");
        };
        window.undo_redo_.Reset();
        const size_t count = doc.GetObjects().size();
        require(window.CutCurveWithCurve(2, 4), "Cut Curve failed on reported geometry");
        require(doc.GetObjects().size() == count + 1, "Single intersection must create two pieces");
        const auto* second = dynamic_cast<const CBSpline*>(doc.GetObjects().back().get());
        require(second, "Cut must retain editable splines");
        verify(original, {target, second});
        const auto join = target->Evaluate(1);
        require(distance(join, second->Evaluate(0)) < 1.e-8, "Cut fragments do not meet");
        const gp_Lin line(gp_Pnt(cutter_points[0].x, cutter_points[0].y, cutter_points[0].z),
            gp_Dir(cutter_points[1].x - cutter_points[0].x,
                   cutter_points[1].y - cutter_points[0].y,
                   cutter_points[1].z - cutter_points[0].z));
        require(line.Distance(gp_Pnt(join.x, join.y, join.z)) < 1.e-6,
                "Cut endpoint does not lie on cutter");
        for (size_t i = 0; i < copy_points.size(); ++i)
            require(distance(copy->GetPoints()[i], copy_points[i]) == 0, "Cut changed the copy");
        for (size_t i = 0; i < cutter_points.size(); ++i)
            require(distance(cutter->GetPoints()[i], cutter_points[i]) == 0, "Cut changed the cutter");
        require(!window.CutCurveWithCurve(3, 4), "Projected intersection at different Z must not cut");
        require(window.undo_redo_.Undo(), "Cut Undo failed");
        require(doc.GetObjects().size() == count, "Cut Undo did not restore object count");
        require(window.undo_redo_.Redo(), "Cut Redo failed");
        verify(original, {dynamic_cast<const CBSpline*>(doc.FindObjectById(2)),
                         dynamic_cast<const CBSpline*>(doc.GetObjects().back().get())});
        const QString saved = settings.filePath("Cut Curve Fixed.dom3d");
        require(serializer.Save(saved, doc, room, view, {}, failure), "Cannot save cut curves");
        CAlfaDoc loaded;
        require(serializer.Load(saved, loaded, room, view, failure), "Cannot reload cut curves");
        // The project loader reads coordinates through float; allow its normal
        // sub-micron roundoff while retaining the strict in-memory cut checks.
        verify(original, {dynamic_cast<const CBSpline*>(loaded.FindObjectById(2)),
                         dynamic_cast<const CBSpline*>(loaded.FindObjectById(doc.GetObjects().back()->m_id))}, 0.001);
        if (args.contains("--output"))
            require(serializer.Save(args.value(args.indexOf("--output") + 1),
                                    doc, room, view, {}, failure), "Cannot save fixed example");

        // Multiple intersections, explicit knots, and rational weights must
        // preserve the authored curve too.
        for (auto type : {SplineCurveType::BSpline, SplineCurveType::Nurbs, SplineCurveType::Bezier}) {
            CBSpline source;
            source.SetCurveType(type);
            for (auto p : {CPoint3d(0, 0, 0), CPoint3d(0, 10, 0),
                           CPoint3d(10, 10, 0), CPoint3d(10, 0, 0)}) source.AddPoint(p);
            if (type == SplineCurveType::Nurbs) source.SetWeights({1, 2, 2, 1});
            CPolyline tool;
            tool.AddPoint(CPoint3d(-1, 5, 0)); tool.AddPoint(CPoint3d(11, 5, 0));
            const auto geometry = curve_cut::Split(source, tool);
            require(geometry.size() == 3, "Two intersections must create three fragments");
            std::vector<CBSpline> fragments(geometry.size());
            std::vector<const CBSpline*> pointers;
            for (size_t i = 0; i < geometry.size(); ++i) {
                fragments[i].SetCurveType(type);
                require(curve_cut::Assign(fragments[i], geometry[i]), "Cannot assign cut geometry");
                pointers.push_back(&fragments[i]);
            }
            verify(source, pointers);
            tool.Translate({0, 0, 0.1f});
            require(curve_cut::Split(source, tool).empty(), "Nearby disjoint curves must not be cut");
        }
        std::cout << "Cut Curve geometry and Undo/Redo regressions passed" << std::endl;
        return EXIT_SUCCESS;
    }
    if(application.arguments().contains("--two-guides")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;auto& doc=window.document_;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,failure;
        const auto args=application.arguments();
        require(serializer.Load(args.value(args.indexOf("--two-guides")+1),doc,room,view,failure),"Cannot load fairing guides");
        TwoViewSettings s;s.guides={5,6,0};s.two_guides=true;std::string error;
        auto shape=BuildTwoViewSurface(doc,s,error);require(!shape.IsNull(),error.c_str());
        auto faceCount=[](const TopoDS_Shape& shape){int n=0;for(TopExp_Explorer ex(shape,TopAbs_FACE);ex.More();ex.Next())++n;return n;};
        require(faceCount(shape)==1,"Two guides must create one conic surface");
        s.reverse=true;auto reversed=BuildTwoViewSurface(doc,s,error);require(!reversed.IsNull(),error.c_str());
        require(TopExp_Explorer(shape,TopAbs_FACE).Current().Orientation()!=TopExp_Explorer(reversed,TopAbs_FACE).Current().Orientation(),"Reverse Normal failed");
        s.mirror=true;s.graphs=true;s.top_graph={.3,.4,.6,.5,.4};auto mirrored=BuildTwoViewSurface(doc,s,error);require(!mirrored.IsNull()&&faceCount(mirrored)==2,"Mirrored two-guide graph failed");
        s.split=true;s.patches_u=3;s.patches_v=2;auto patches=BuildTwoViewSurface(doc,s,error);require(!patches.IsNull()&&faceCount(patches)==12,"Two-guide patch count incorrect");
        auto restored=ReadTwoViewParameters(TwoViewParameters(s));require(restored.two_guides&&restored.reverse&&restored.guides[2]==0,"Two-guide parameters lost");
        auto noMode=ReadTwoViewParameters({});require(!noMode.two_guides&&!noMode.reverse,"Legacy three-guide defaults changed");
        const auto count=doc.GetObjects().size();window.undo_redo_.Reset();window.ActivateParametricTool("SurfaceTwoView");
        auto* dialog=window.findChild<QDialog*>("TwoViewSurfaceDialog");auto* panel=dialog->findChild<PropertyPanel*>("TwoViewParameters");
        panel->findChild<QComboBox*>("parameter_two.guides")->setCurrentIndex(1);application.processEvents();
        for(auto id:{5ul,6ul}){doc.SelectObjectById(id);window.viewport_->SelectionChanged();}
        auto okButton=[&](){QPushButton* ok=nullptr;for(auto* b:panel->findChildren<QPushButton*>())if(b->text()=="OK")ok=b;return ok;};
        require(okButton()&&okButton()->isEnabled()&&doc.GetObjects().size()==count+1,"Two-guide OK disabled");
        panel->findChild<QComboBox*>("parameter_two.guides")->setCurrentIndex(0);application.processEvents();
        require(!okButton()->isEnabled()&&doc.GetObjects().size()==count,"Three-guide mode retained incomplete preview");
        panel->findChild<QComboBox*>("parameter_two.guides")->setCurrentIndex(1);application.processEvents();
        panel->findChild<QCheckBox*>("parameter_mirror")->setChecked(true);
        require(okButton()->isEnabled(),"Two-guide preview did not recover");
        okButton()->click();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(window.undo_redo_.Undo()&&doc.GetObjects().size()==count,"Two-guide Undo failed");
        require(window.undo_redo_.Redo()&&doc.GetObjects().size()==count+1,"Two-guide Redo failed");
        const auto saved=temp.filePath("fairing.dom3d");require(serializer.Save(saved,doc,room,view,{},failure),"Cannot save fairing");
        CAlfaDoc loaded;require(serializer.Load(saved,loaded,room,view,failure),"Cannot load fairing");
        auto settings=ReadTwoViewParameters(loaded.GetObjects().back()->GetParametricParameters());
        require(settings.two_guides&&RebuildTwoViewSurface(loaded,loaded.GetObjects().size()-1,settings,error),"Saved fairing failed to rebuild");
        if(args.contains("--output"))require(serializer.Save(args.value(args.indexOf("--output")+1),doc,room,view,{},failure),"Cannot save fairing example");
        std::cout<<"Two guides: Helicopter fairing, mirror, reverse, graphs, patches, mode switching, Undo/Redo and saved rebuild passed\n";return 0;
    }
    if(application.arguments().contains("--helicopter-trim")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;auto& doc=window.document_;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,failure;
        const auto args=application.arguments();
        require(serializer.Load(args.value(args.indexOf("--helicopter-trim")+1),doc,room,view,failure),"Cannot load Helicopter trim fixture");
        auto area=[](const TopoDS_Shape& shape){GProp_GProps p;BRepGProp::SurfaceProperties(shape,p,1.e-9);return p.Mass();};
        const double original=area(dynamic_cast<CSurfaceSet*>(doc.FindObjectById(9))->m_Shape);
        for(unsigned long id:{10ul,11ul}) {
            TopoDS_Shape outside,inside;std::string error;
            require(BuildSurfaceSketchParts(doc,{9,id,false},outside,inside,error),error.c_str());
            require(BRepCheck_Analyzer(outside).IsValid()&&BRepCheck_Analyzer(inside).IsValid(),"Invalid Helicopter trim topology");
            require(std::abs(area(outside)+area(inside)-original)<original*1.e-6,"Helicopter trim lost surface area");
            int faces=0;for(TopExp_Explorer ex(inside,TopAbs_FACE);ex.More();ex.Next())++faces;
            require(faces>=2,"Missing opposite-side window cutout");
            const auto count=doc.GetObjects().size();
            window.ActivateParametricTool("SurfaceTrimSketch");auto* dialog=window.findChild<QDialog*>("SurfaceSketchTrimDialog");require(dialog,"No Surface Trim dialog");
            doc.SelectObjectById(9);window.viewport_->SelectionChanged();doc.SelectObjectById(id);window.viewport_->SelectionChanged();
            auto* ok=dialog->findChild<QPushButton*>("CreateSurfaceTrim");require(ok&&ok->isEnabled(),"Helicopter Trim OK is disabled");
            dialog->findChild<QCheckBox*>("SurfaceTrimKeepCutout")->setChecked(true);
            require(doc.GetObjects().size()==count+2,"Helicopter Keep cutout lost a result");
            dialog->reject();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
            require(doc.GetObjects().size()==count&&doc.FindObjectById(9)->IsVisible(),"Helicopter trim Cancel failed");
        }
        // Apply both user sketches in succession, retaining each glazing piece.
        unsigned long source=9;
        for(unsigned long id:{10ul,11ul}) {
            SurfaceTrimSettings settings{source,id,false};TopoDS_Shape outside,inside;std::string error;
            require(BuildSurfaceSketchParts(doc,settings,outside,inside,error),error.c_str());
            doc.FindObjectById(source)->SetVisible(false);
            for(bool cutout:{false,true}) {
                auto result=std::make_unique<CSurfaceSet>(cutout?inside:outside);settings.inside=cutout;
                result->SetName(cutout?"Window cutout":"Helicopter trimmed");
                result->SetParametricOperation(0,"SurfaceTrimSketch","Trim By Sketch",SurfaceTrimParameters(settings));
                require(result->ReBuldMesh(),"Cannot mesh Helicopter trim");
                auto* raw=result.get();doc.AddObject(std::move(result));if(!cutout)source=raw->m_id;
            }
        }
        const auto saved=temp.filePath("Helicopter-windows.dom3d");
        require(serializer.Save(saved,doc,room,view,{},failure),"Cannot save Helicopter windows");
        CAlfaDoc loaded;require(serializer.Load(saved,loaded,room,view,failure),"Cannot load Helicopter windows");
        for(size_t i=0;i<loaded.GetObjects().size();++i) {
            const auto params=loaded.GetObjects()[i]->GetParametricParameters();
            if(ReadSurfaceTrimParameters(params).source) {std::string error;require(RebuildSurfaceSketchTrim(loaded,i,ReadSurfaceTrimParameters(params),error),error.c_str());}
        }
        if(args.contains("--output"))require(serializer.Save(args.value(args.indexOf("--output")+1),doc,room,view,{},failure),"Cannot save Helicopter example");
        if(args.contains("--preview")) {
            for(auto& object:doc.GetObjects())if(object->GetName()=="Window cutout")object->SetVisible(false);
            window.resize(1100,800);window.show();window.viewport_->SetCamera(view.camera);window.viewport_->FitToDocument();
            QElapsedTimer wait;wait.start();while(wait.elapsed()<700)application.processEvents();
            require(window.viewport_->grabFramebuffer().save(args.value(args.indexOf("--preview")+1)),"Cannot save Helicopter holes preview");
        }
        std::cout<<"Helicopter: both sketches, opposite-side cutouts, enabled OK, Cancel, consecutive cuts and saved rebuild passed\n";
        return 0;
    }
    if(application.arguments().contains("--surface-sketch-trim")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;auto& doc=window.document_;
        auto plane=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-50,50,-50,50).Shape();
        auto body=std::make_unique<CSurfaceSet>(plane);auto* source=body.get();doc.AddObject(std::move(body));
        auto sketch=std::make_unique<CSmartLine>();
        require(sketch->CreateFromWorldPoints({{-20,-20,1000},{20,-20,1000},{20,20,1000},{-20,20,1000}},true,{0,0,1000},{1,0,0},{0,1,0}),"Cannot create window Sketch");
        auto* cutter=sketch.get();doc.AddObject(std::move(sketch));SurfaceTrimSettings settings{source->m_id,cutter->m_id,false};
        const auto sourceId=source->m_id,sketchId=cutter->m_id;
        auto area=[](const TopoDS_Shape& shape){GProp_GProps p;BRepGProp::SurfaceProperties(shape,p);return p.Mass();};
        TopoDS_Shape outside,inside;std::string error;
        require(BuildSurfaceSketchParts(doc,settings,outside,inside,error),error.c_str());
        require(std::abs(area(inside)-1600)<1.e-5&&std::abs(area(outside)-8400)<1.e-5,"Window cut areas are incorrect");
        require(!TopExp_Explorer(inside,TopAbs_SOLID).More(),"Cutout became a solid");
        auto open=std::make_unique<CSmartLine>();
        require(open->CreateFromWorldPoints({{-70,0,0},{0,10,0},{70,0,0}},false,{},{1,0,0},{0,1,0}),"Cannot create open Sketch");
        auto* openRaw=open.get();doc.AddObject(std::move(open));auto openSettings=settings;openSettings.sketch=openRaw->m_id;
        require(BuildSurfaceSketchParts(doc,openSettings,outside,inside,error),error.c_str());
        require(std::abs(area(inside)+area(outside)-10000)<1.e-5,"Open Sketch lost surface area");
        window.undo_redo_.Reset();const auto count=doc.GetObjects().size();
        auto openDialog=[&](){window.ActivateParametricTool("SurfaceTrimSketch");auto* d=window.findChild<QDialog*>("SurfaceSketchTrimDialog");require(d,"No Surface Trim dialog");
            doc.SelectObjectById(sourceId);window.viewport_->SelectionChanged();doc.SelectObjectById(sketchId);window.viewport_->SelectionChanged();
            require(d->findChild<QPushButton*>("CreateSurfaceTrim")->isEnabled(),"Surface Trim preview failed");return d;};
        auto* dialog=openDialog();require(doc.GetObjects().size()==count+1&&!doc.FindObjectById(sourceId)->IsVisible(),"Hole preview did not replace the source");
        dialog->findChild<QCheckBox*>("SurfaceTrimKeepCutout")->setChecked(true);
        require(doc.GetObjects().size()==count+2,"Keep cutout did not create two objects");
        dialog->findChild<QComboBox*>("SurfaceTrimDirection")->setCurrentIndex(0);
        require(std::abs(area(dynamic_cast<CSolid*>(doc.GetObjects()[count].get())->m_Shape)-1600)<1.e-5,"Direction did not switch pieces");
        dialog->reject();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(doc.GetObjects().size()==count&&doc.FindObjectById(sourceId)->IsVisible(),"Cancel did not restore the original surface");
        dialog=openDialog();dialog->findChild<QCheckBox*>("SurfaceTrimKeepCutout")->setChecked(true);dialog->accept();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        const auto trimId=doc.GetObjects()[count]->m_id,cutId=doc.GetObjects()[count+1]->m_id;
        require(window.undo_redo_.Undo()&&doc.GetObjects().size()==count&&doc.FindObjectById(sourceId)->IsVisible(),"Surface trim Undo failed");
        require(window.undo_redo_.Redo()&&doc.GetObjects().size()==count+2&&!doc.FindObjectById(sourceId)->IsVisible(),"Surface trim Redo failed");
        Dom3DProjectSerializer serializer;ProjectViewState view;QString room,failure;auto path=temp.filePath("surface-cutout.dom3d");
        require(serializer.Save(path,doc,room,view,{},failure),"Cannot save surface cutout");CAlfaDoc loaded;
        require(serializer.Load(path,loaded,room,view,failure),"Cannot load surface cutout");
        for(size_t i=count;i<count+2;++i){auto s=ReadSurfaceTrimParameters(loaded.GetObjects()[i]->GetParametricParameters());require(RebuildSurfaceSketchTrim(loaded,i,s,error),"Saved trim cannot rebuild");}
        auto* changed=dynamic_cast<CSmartLine*>(loaded.FindObjectById(sketchId));require(changed->CreateFromWorldPoints({{-25,-20,1000},{25,-20,1000},{25,20,1000},{-25,20,1000}},true,{0,0,1000},{1,0,0},{0,1,0}),"Cannot resize Sketch");
        ToolRegistry registry;require(registry.ReplayProfileDependents(sketchId,loaded),"Sketch did not rebuild its two surface dependents");
        auto* cutBody=dynamic_cast<CSolid*>(loaded.FindObjectById(cutId));auto* trimBody=dynamic_cast<CSolid*>(loaded.FindObjectById(trimId));
        require(area(cutBody->m_Shape)>1600&&std::abs(area(cutBody->m_Shape)+area(trimBody->m_Shape)-10000)<1.e-5,"Paired surfaces lost their complement after editing Sketch");
        // Curved fuselage: keep the actual surface fragments, without planar caps.
        const auto args=application.arguments();
        if(args.contains("--fuselage")) {
            CAlfaDoc curved;require(serializer.Load(args.value(args.indexOf("--fuselage")+1),curved,room,view,failure),"Cannot load fuselage guides");
            TwoViewSettings ts;ts.guides={4,3,2};ts.mirror=true;auto shape=BuildTwoViewSurface(curved,ts,error);require(!shape.IsNull(),"Cannot build test fuselage");
            auto fuselage=std::make_unique<CSurfaceSet>(shape);auto* f=fuselage.get();curved.AddObject(std::move(fuselage));
            auto windowSketch=std::make_unique<CSmartLine>();require(windowSketch->CreateFromWorldPoints({{60,20,0},{100,20,0},{100,32,0},{60,32,0}},true,{},{1,0,0},{0,1,0}),"Cannot create fuselage window");
            auto* w=windowSketch.get();curved.AddObject(std::move(windowSketch));SurfaceTrimSettings ss{f->m_id,w->m_id,false};
            require(BuildSurfaceSketchParts(curved,ss,outside,inside,error),error.c_str());
            require(std::abs(area(inside)+area(outside)-area(shape))/area(shape)<1.e-5,"Fuselage trim changed area");
            for(bool keepInside:{false,true}){auto piece=keepInside?inside:outside;auto result=std::make_unique<CSurfaceSet>(piece);ss.inside=keepInside;
                result->SetName(keepInside?"Window cutout":"Fuselage with window");
                if(keepInside){auto glass=Material::DefaultSurface();glass.name="Window Glass";glass.diffuse={.12f,.35f,.55f};glass.alpha=.4f;glass.specular=.9f;result->SetMaterial(glass);}
                result->SetParametricOperation(0,"SurfaceTrimSketch","Trim By Sketch",SurfaceTrimParameters(ss));require(result->ReBuldMesh(),"Cannot mesh window");curved.AddObject(std::move(result));}
            f->SetVisible(false);
            if(args.contains("--output"))require(serializer.Save(args.value(args.indexOf("--output")+1),curved,room,view,{},failure),"Cannot save fuselage window example");
            if(args.contains("--preview")&&args.contains("--output")) {
                window.OpenProjectFromPath(args.value(args.indexOf("--output")+1));window.resize(1100,800);window.show();window.viewport_->SetCamera(view.camera);window.viewport_->FitToDocument();
                QElapsedTimer wait;wait.start();while(wait.elapsed()<700)application.processEvents();
                require(window.viewport_->grabFramebuffer().save(args.value(args.indexOf("--preview")+1)),"Cannot save window preview");
                window.document_.GetObjects().back()->SetVisible(false);window.viewport_->update();application.processEvents();
                require(window.viewport_->grabFramebuffer().save(args.value(args.indexOf("--preview")+1)+".hole.png"),"Cannot save open window preview");

            }

        }
        std::cout<<"Surface Trim: closed/open Sketch, Keep cutout, directions, Cancel, Undo, persistence and dependent rebuild passed\n";return 0;
    }
    if(application.arguments().contains("--two-view-file")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        const auto args=application.arguments();auto& doc=window.document_;
        require(serializer.Load(args.value(args.indexOf("--two-view-file")+1),doc,room,view,error),"Cannot load Plane_Fuselag");
        const bool helicopter=args.contains("--helicopter");
        TwoViewSettings s;s.guides=helicopter?std::array<unsigned long,3>{2,4,7}:std::array<unsigned long,3>{4,3,2};s.mirror=true;std::string failure;
        auto shape=BuildTwoViewSurface(doc,s,failure);
        if(shape.IsNull())std::cerr<<failure<<std::endl;
        require(!shape.IsNull(),"Spatial Middle guide was rejected");
        auto surface=BRep_Tool::Surface(TopoDS::Face(TopExp_Explorer(shape,TopAbs_FACE).Current()));
        require(std::abs(surface->Value(.5,1).Y())>1,"Middle guide was flattened to XZ");
        if(helicopter) {
            require(std::abs(surface->Value(0,1).Z())<1.e-9,"Imported nose was not normalized to XY");
            s.mirror=false;auto half=BuildTwoViewSurface(doc,s,failure);
            require(!half.IsNull()&&BRepCheck_Analyzer(half).IsValid(),"Helicopter half failed");
            s.split=true;auto patches=BuildTwoViewSurface(doc,s,failure);
            require(!patches.IsNull()&&BRepCheck_Analyzer(patches).IsValid(),"Helicopter patches failed");
            s.split=false;s.mirror=true;
        }
        const auto count=doc.GetObjects().size();window.undo_redo_.Reset();window.ActivateParametricTool("SurfaceTwoView");
        auto* dialog=window.findChild<QDialog*>("TwoViewSurfaceDialog");require(dialog,"No two-view dialog");
        for(auto id:s.guides){doc.SelectObjectById(id);window.viewport_->SelectionChanged();}
        auto* panel=dialog->findChild<PropertyPanel*>("TwoViewParameters");
        panel->findChild<QCheckBox*>("parameter_mirror")->setChecked(true);
        QPushButton* ok=nullptr;for(auto* b:panel->findChildren<QPushButton*>())if(b->text()=="OK")ok=b;
        require(ok&&ok->isEnabled()&&doc.GetObjects().size()==count+1,"Plane_Fuselag OK is disabled");
        if(args.contains("--preview")) {
            window.resize(1100,800);window.show();window.viewport_->SetCamera(view.camera);window.viewport_->FitToDocument();
            QElapsedTimer wait;wait.start();while(wait.elapsed()<700)application.processEvents();
            const auto path=args.value(args.indexOf("--preview")+1);
            require(window.viewport_->grabFramebuffer().save(path),"Cannot save Plane_Fuselag preview");
            require(dialog->grab().save(path+".dialog.png"),"Cannot save Plane_Fuselag dialog");
        }
        ok->click();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(doc.GetObjects().size()==count+1,"Plane_Fuselag acceptance failed");
        if(args.contains("--output"))require(serializer.Save(args.value(args.indexOf("--output")+1),doc,room,view,{},error),"Cannot save Plane_Fuselag result");
        std::cout<<"Two-view imported spatial guide and enabled OK passed\n";return 0;
    }
    #include "SaddleTestCases.inc"
    #include "Body2TestCases.inc"
    #include "SurfaceOffsetTestCases.inc"
    #include "SewnBodyHistoryTestCases.inc"
    #include "OffsetBodyFilletTestCases.inc"
    #include "ExtractSurfaceEdgeTestCases.inc"
    #include "LoftBezierTestCases.inc"
    #include "FilletResponsiveTestCases.inc"
    #include "SurfaceDisplayOptionsTestCases.inc"
    #include "SurfaceReverseNormalsTestCases.inc"
    #include "SurfaceGraphOffsetTestCases.inc"
    #include "SurfaceBoundaryGraphTestCases.inc"
    #include "SurfaceSeamUITestCases.inc"
    #include "SurfaceAlignmentUITestCases.inc"
    #include "SurfaceTopologyTestCases.inc"
    #include "SubdivideExtrudeTestCases.inc"
    #include "BodyPrimitiveTestCases.inc"
    #include "SectionGraphTestCases.inc"
    #include "Bottle3TestCases.inc"
    #include "SurfaceCapTestCases.inc"
    #include "NSidedContinuityTestCases.inc"
    #include "NSidedSurfaceTestCases.inc"
    #include "Body1TestCases.inc"
    if(application.arguments().contains("--join-surface-edges-file")) {
        const auto args=application.arguments();MainWindow window;auto& doc=window.document_;
        Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        require(serializer.Load(args.value(args.indexOf("--join-surface-edges-file")+1),doc,room,view,error),"Cannot load edge-join fixture");
        std::vector<CPoint3d> samples;std::vector<unsigned long> ids;
        doc.ClearSelection();
        for(const auto& object:doc.GetObjects())if(auto* spline=dynamic_cast<CBSpline*>(object.get());spline&&spline->GetName().find("Extracted Edge")==0) {
            ids.push_back(spline->m_id);
            for(int i=0;i<=100;++i)samples.push_back(spline->Evaluate(float(i)/100));
        }
        require(ids.size()==4,"Expected four original extracted edges");
        // Scrambled selection and one reversed input exercise ordering.
        dynamic_cast<CBSpline*>(doc.FindObjectById(ids[2]))->Reverse();
        for(int i:{0,2,3,1})doc.SelectObjectById(ids[i],SelectionAction::Add);
        const auto count=doc.GetObjects().size();
        require(window.JoinSelectedCurves(),"Join extracted edges failed");
        const auto verify=[&](CAlfaDoc& document) {
            auto* joined=dynamic_cast<CBSpline*>(document.FindObjectById(ids[0]));
            require(joined&&joined->IsClosed()&&!joined->GetKnots().empty(),"Join lost closure or knots");
            auto copy=joined->Clone();auto* open=dynamic_cast<CBSpline*>(copy.get());open->SetClosed(false);
            const auto exact=curve_cut::Spline(*open);require(!exact.IsNull(),"Cannot read joined NURBS");
            for(const auto& p:samples){GeomAPI_ProjectPointOnCurve projection(gp_Pnt(p.x,p.y,p.z),exact);
                require(projection.NbPoints()>0&&projection.LowerDistance()<2.e-5,"Joining changed an original edge");}
        };
        verify(doc);require(doc.GetObjects().size()==count-3,"Join removed wrong objects");
        require(window.undo_redo_.Undo()&&doc.GetObjects().size()==count,"Join Undo failed");
        require(window.undo_redo_.Redo(),"Join Redo failed");verify(doc);
        QTemporaryDir temp;const auto path=temp.filePath("joined.dom3d");
        require(serializer.Save(path,doc,room,view,{},error),"Cannot save joined contour");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,error),"Cannot reload joined contour");verify(loaded);
        if(args.contains("--output"))require(serializer.Save(args.value(args.indexOf("--output")+1),doc,room,view,{},error),"Cannot save repaired copy");
        std::cout<<"Extracted edge join preserves geometry, closure, Undo and persistence\n";return 0;
    }
    if(application.arguments().contains("--two-view")) {
        QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        MainWindow window;auto& doc=window.document_;const auto baseCount=doc.GetObjects().size();TwoViewSettings settings;
        const std::array<std::array<CPoint3d,4>,3> points{{
            {{{0,0,0},{20,40,0},{70,30,0},{100,10,0}}},
            {{{0,0,0},{20,0,-30},{70,0,-25},{100,0,-10}}},
            {{{0,0,0},{20,-20,0},{70,-15,0},{100,-10,0}}}
        }};
        for(int i=0;i<3;++i){auto p=std::make_unique<CBSpline>(i==0?"Up":i==1?"Middle":"Down");
            p->SetCurveType(SplineCurveType::Bezier);for(auto q:points[i])p->AddPoint(q);
            auto* raw=p.get();doc.AddObject(std::move(p));settings.guides[i]=raw->m_id;}
        std::string error;auto shape=BuildTwoViewSurface(doc,settings,error);
        if(shape.IsNull())std::cerr<<error<<std::endl;
        require(!shape.IsNull(),"Two-view collapsed nose failed");
        std::vector<Handle(Geom_Surface)> surfaces;
        for(TopExp_Explorer ex(shape,TopAbs_FACE);ex.More();ex.Next())surfaces.push_back(BRep_Tool::Surface(TopoDS::Face(ex.Current())));
        require(surfaces.size()==2,"Expected upper and lower conic surfaces");
        for(double u:{.1,.35,.7,.95}) {
            const auto upper=surfaces[0],lower=surfaces[1];
            require(upper->Value(u,1).Distance(lower->Value(u,1))<1.e-7,"Gap at Middle guide");
            GeomLProp_SLProps a(upper,u,1,1,1.e-9),b(lower,u,1,1,1.e-9);
            require(std::abs(a.Normal().Dot(b.Normal()))>.99999,"Conics do not meet tangentially");
            for(double v:{0.,.25,.5,.75,1.})require(std::abs(upper->Value(u,v).X()-100*u)<1.e-6,"Conic left the X-constant section plane");
        }
        // Quarter-circle rho: equal height/width gives a circle, not a polynomial approximation.
        CAlfaDoc round;TwoViewSettings circular;
        for(int i=0;i<3;++i){auto p=std::make_unique<CBSpline>();p->SetCurveType(SplineCurveType::Bezier);
            for(double x:{0.,33.333333333333,66.666666666667,100.})p->AddPoint({x,i==0?10.:i==2?-10.:0.,i==1?-10.:0.});
            auto* raw=p.get();round.AddObject(std::move(p));circular.guides[i]=raw->m_id;}
        auto cylinder=BuildTwoViewSurface(round,circular,error);require(!cylinder.IsNull(),"Circular conics failed");
        circular.mirror=true;
        for(int side:{1,2}) {
            circular.tangent_cap=side;
            auto capped=BuildTwoViewSurface(round,circular,error);
            if(capped.IsNull())std::cerr<<error<<std::endl;
            require(!capped.IsNull()&&BRepCheck_Analyzer(capped).IsValid(),"Rounded two-view cap failed");
            auto cap=BuildTwoViewTangentCap(round,circular,side,.55,error);
            require(!cap.IsNull(),"Standalone conic cap failed");
            int count=0;
            for(TopExp_Explorer ex(cap,TopAbs_FACE);ex.More();ex.Next()) {
                ++count;const auto surf=BRep_Tool::Surface(TopoDS::Face(ex.Current()));
                for(double v:{.1,.3,.5,.7,.9}) {
                    gp_Pnt p;gp_Vec du,dv;surf->D1(0,v,p,du,dv);
                    require(std::abs(p.X()-(side==1?0:100))<1.e-7,"Cap moved the rim");
                    require(std::hypot(du.Y(),du.Z())<1.e-7&&du.X()*(side==1?-1:1)>0,"Cap lost cylindrical G1 tangency");
                    double previous=0;
                    for(int j=0;j<=100;++j) {
                        const auto q=surf->Value(j/100.,v);const double distance=(q.X()-p.X())*(side==1?-1:1);
                        require(distance>=previous-1.e-7,"Cap folds back along the axis");previous=distance;
                        require(q.Y()*q.Y()+q.Z()*q.Z()<=100.000001,"Cap has radial bulges");
                    }
                }
            }
            require(count==4,"Expected four fair cap patches");
            const auto saved=ReadTwoViewParameters(TwoViewParameters(circular));
            require(saved.tangent_cap==side&&saved.cap_length==circular.cap_length,"Cap settings did not round-trip");
        }
        auto circle=BRep_Tool::Surface(TopoDS::Face(TopExp_Explorer(cylinder,TopAbs_FACE).Current()));
        for(double v:{.1,.3,.5,.8}){auto p=circle->Value(.45,v);require(std::abs(p.Y()*p.Y()+p.Z()*p.Z()-100)<1.e-6,"Rho 0.4142 is not an exact circular section");}
        GProp_GProps original;BRepGProp::SurfaceProperties(shape,original);
        settings.split=true;settings.patches_u=4;settings.patches_v=3;
        auto split=BuildTwoViewSurface(doc,settings,error);require(!split.IsNull(),"Patch split failed");
        int faces=0;for(TopExp_Explorer ex(split,TopAbs_FACE);ex.More();ex.Next())++faces;
        require(faces==24,"Wrong patch count");GProp_GProps splitArea;BRepGProp::SurfaceProperties(split,splitArea);
        require(std::abs(splitArea.Mass()-original.Mass())/original.Mass()<1.e-5,"Splitting changed surface geometry");
        auto firstPatch=Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(TopoDS::Face(TopExp_Explorer(split,TopAbs_FACE).Current())));
        auto originalPatch=Handle(Geom_BSplineSurface)::DownCast(surfaces[0]);
        require(firstPatch->NbUPoles()<originalPatch->NbUPoles(),"Patches still expose the whole surface control net");
        settings.split=false;settings.mirror=true;auto mirrored=BuildTwoViewSurface(doc,settings,error);
        require(!mirrored.IsNull(),"Mirrored nose failed");GProp_GProps mirrorArea;BRepGProp::SurfaceProperties(mirrored,mirrorArea);
        require(std::abs(mirrorArea.Mass()/original.Mass()-2)<1.e-5,"Mirror does not make the second half");
        settings.graphs=true;settings.top_graph={.3,.5,.7,.5,.4};settings.bottom_graph={.4,.3,.5,.6,.4};
        auto variable=BuildTwoViewSurface(doc,settings,error);require(!variable.IsNull(),"Variable rho failed");
        for(TopExp_Explorer ex(variable,TopAbs_FACE);ex.More();ex.Next()) {
            const auto surf=BRep_Tool::Surface(TopoDS::Face(ex.Current()));
            for(double u:{.2,.4,.6,.8})for(double v:{.1,.5,.9})
                require(std::abs(surf->Value(u,v).X()-100*u)<1.e-6,"Variable rho moved conics outside their section plane");
        }
        for(auto id:settings.guides)dynamic_cast<CBSpline*>(doc.FindObjectById(id))->Reverse();
        auto reversed=BuildTwoViewSurface(doc,settings,error);require(!reversed.IsNull(),"Reversed guides failed");
        GProp_GProps va,ra;BRepGProp::SurfaceProperties(variable,va);BRepGProp::SurfaceProperties(reversed,ra);
        require(std::abs(va.Mass()-ra.Mass())<1.e-5,"Reversing input direction changed geometry");
        auto bad=settings;bad.top_graph[2]=1;require(BuildTwoViewSurface(doc,bad,error).IsNull(),"Invalid rho accepted");
        auto caps=BuildTwoViewTangentCap(doc,settings,2,.55,error);require(!caps.IsNull(),"Variable-rho cap failed");
        TopExp_Explorer support_face(variable,TopAbs_FACE),cap_face(caps,TopAbs_FACE);
        for(;support_face.More()&&cap_face.More();support_face.Next(),cap_face.Next()) {
            const auto a=BRep_Tool::Surface(TopoDS::Face(support_face.Current())),b=BRep_Tool::Surface(TopoDS::Face(cap_face.Current()));
            for(double v:{.05,.25,.5,.75,.95}) {
                require(a->Value(1,v).Distance(b->Value(0,v))<1.e-7,"Variable-rho cap rim has a gap");
                GeomLProp_SLProps na(a,1,v,1,1.e-9),nb(b,0,v,1,1.e-9);
                require(std::abs(na.Normal().Dot(nb.Normal()))>.999999,"Variable-rho cap lost G1 continuity");
            }
        }
        settings.tangent_cap=2;
        auto capped_settings=settings;capped_settings.split=true;
        require(!BuildTwoViewSurface(doc,capped_settings,error).IsNull(),"Split surface cap failed");
        circular.tangent_cap=0;circular.split=true;circular.patches_u=3;circular.patches_v=2;
        auto cap_support_shape=BuildTwoViewSurface(round,circular,error);
        auto cap_support=std::make_unique<CSurfaceSet>(cap_support_shape);
        cap_support->SetParametricOperation(0,"SurfaceTwoView","Surface by two View",TwoViewParameters(circular));
        require(cap_support->ReBuldMesh(),"Cap support mesh failed");round.AddObject(std::move(cap_support));
        const auto cap_support_id=round.GetSelectedObject()->m_id;
        auto rim=std::make_unique<CBSpline>();
        for(int j=0;j<48;++j){double a=2*3.141592653589793*j/48;rim->AddPoint({0,10*std::cos(a),10*std::sin(a)});}
        rim->SetClosed(true);round.AddObject(std::move(rim));const auto rim_id=round.GetSelectedObject()->m_id;
        const bool cap_created=round.CreateTangentCap(rim_id,cap_support_id,.55,&error);
        if(!cap_created)std::cerr<<error<<std::endl;
        require(cap_created,"Tangent Cap tool failed on split Two Views");
        auto* cap_object=dynamic_cast<CSurfaceSet*>(round.GetSelectedObject());
        require(cap_object&&cap_object->GetNumSurfaces()==8,"Tangent Cap did not use the fair conic construction");
        window.undo_redo_.Reset();window.ActivateParametricTool("SurfaceTwoView");
        auto* dialog=window.findChild<QDialog*>("TwoViewSurfaceDialog");require(dialog,"No two-view dialog");
        for(auto id:settings.guides){doc.SelectObjectById(id);window.viewport_->SelectionChanged();}
        if(doc.GetObjects().size()!=baseCount+4)std::cerr<<dialog->findChild<QLabel*>("TwoViewStatus")->text().toStdString()<<std::endl;
        require(doc.GetObjects().size()==baseCount+4,"Three clicks did not create a preview");
        dialog->reject();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(doc.GetObjects().size()==baseCount+3,"Cancel left a preview");
        window.ActivateParametricTool("SurfaceTwoView");dialog=window.findChild<QDialog*>("TwoViewSurfaceDialog");
        for(auto id:settings.guides){doc.SelectObjectById(id);window.viewport_->SelectionChanged();}
        auto* panel=dialog->findChild<PropertyPanel*>("TwoViewParameters");require(panel,"Missing parameters");
        panel->findChild<QCheckBox*>("parameter_mirror")->setChecked(true);
        auto* cap_selector=panel->findChild<QComboBox*>("parameter_tangent.cap");require(cap_selector,"Missing Tangent Cap selector");
        cap_selector->setCurrentIndex(2);
        if(application.arguments().contains("--preview")) {
            window.resize(1100,800);window.show();window.viewport_->FitToDocument();
            QElapsedTimer wait;wait.start();while(wait.elapsed()<700)application.processEvents();
            const auto path=application.arguments().value(application.arguments().indexOf("--preview")+1);
            require(window.viewport_->grabFramebuffer().save(path),"Cannot save preview");
            require(dialog->grab().save(path+".dialog.png"),"Cannot save parameter dialog preview");
        }
        dialog->accept();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);require(doc.GetObjects().size()==baseCount+4,"Acceptance failed");
        require(window.undo_redo_.Undo()&&doc.GetObjects().size()==baseCount+3,"Two-view Undo failed");
        require(window.undo_redo_.Redo()&&doc.GetObjects().size()==baseCount+4,"Two-view Redo failed");
        require(RebuildTwoViewSurface(doc,baseCount+3,settings,error),"Cannot apply rho graphs to saved surface");
        Dom3DProjectSerializer serializer;ProjectViewState view;QString saveError,room;
        const auto path=temp.filePath("two-view.dom3d");require(serializer.Save(path,doc,room,view,{},saveError),"Save failed");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,saveError),"Reload failed");
        auto saved=ReadTwoViewParameters(loaded.GetObjects().back()->GetParametricParameters());
        require(saved.guides==settings.guides&&saved.mirror&&saved.graphs&&saved.top_graph==settings.top_graph&&saved.bottom_graph==settings.bottom_graph,"Lost two-view settings");
        require(saved.tangent_cap==2&&saved.cap_length==settings.cap_length,"Lost Tangent Cap settings");
        require(RebuildTwoViewSurface(loaded,loaded.GetObjects().size()-1,saved,error),"Saved two-view surface cannot rebuild");
        if(application.arguments().contains("--output"))require(serializer.Save(application.arguments().value(application.arguments().indexOf("--output")+1),doc,room,view,{},saveError),"Cannot save example");
        std::cout<<"Two-view geometry, conics, graphs, patches, UI, Undo and persistence passed\n";return 0;
    }
    if(application.arguments().contains("--fillet-loft-file")) {
        CAlfaDoc doc;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        const auto args=application.arguments();
        require(serializer.Load(args.value(args.indexOf("--fillet-loft-file")+1),doc,room,view,error),"Cannot load Loft-3");
        for(const auto& object:doc.GetObjects())if(auto* body=dynamic_cast<CSolid*>(object.get())) {
            int count=0;for(TopExp_Explorer ex(body->m_Shape,TopAbs_FACE);ex.More();ex.Next())++count;
            std::cerr<<"body "<<body->m_id<<" faces "<<count<<std::endl;
        }
        SurfaceFilletSettings s;s.bodies={8,13};
        BRepAlgoAPI_Splitter split;TopTools_ListOfShape inputs;std::vector<TopoDS_Shape> inputFaces;
        for(auto id:s.bodies) {
            TopExp_Explorer ex(dynamic_cast<CSolid*>(doc.FindObjectById(id))->m_Shape,TopAbs_FACE);
            inputFaces.push_back(ex.Current());inputs.Append(ex.Current());
        }
        split.SetArguments(inputs);split.SetNonDestructive(true);split.Build();
        for(size_t side=0;side<2;++side) {
            int count=0;
            for(TopTools_ListIteratorOfListOfShape it(split.Modified(inputFaces[side]));it.More();it.Next())
                for(TopExp_Explorer ex(it.Value(),TopAbs_FACE);ex.More();ex.Next())++count;
            std::cerr<<"split body "<<s.bodies[side]<<" parts "<<count<<std::endl;
        }
        require(SurfaceFilletSolutionCount(doc,s)==2,"Loft-3 should expose two real support pairs");
        for(double radius:{29.,69.})for(int solution=0;solution<4;++solution) {
            s.radius=radius;s.solution=solution;std::string failure;
            const auto shape=BuildSurfaceFillet(doc,s,failure);
            if(radius==69)require(!shape.IsNull(),"Loft-3 R69 failed");
            std::cerr<<"radius "<<radius<<" solution "<<solution<<" ok "<<!shape.IsNull()<<" "<<failure<<std::endl;
            for(const auto& n:SurfaceFilletNormalGuides(doc,s,shape))std::cerr<<"normal "<<n.origin.x<<" "<<n.origin.y<<" "<<n.origin.z
                <<" dir "<<n.direction.x<<" "<<n.direction.y<<" "<<n.direction.z<<std::endl;
        }
        s.radius=69;s.solution=3;std::string failure;
        auto legacyShape=BuildSurfaceFillet(doc,s,failure);
        auto result=std::make_unique<CSurfaceSet>(legacyShape);
        result->SetParametricOperation(0,"SurfaceFillet","Surface Fillet",SurfaceFilletParameters(s));
        const auto index=doc.GetObjects().size();doc.AddObject(std::move(result));
        ToolRegistry registry;
        const auto active=registry.ActiveObjectFromDocument(index,*doc.GetObjects()[index],0,&doc);
        bool found=false;
        for(const auto& p:active.parameters)if(p.id=="solution") {
            found=true;require(p.options.size()==2&&p.value==1&&p.maximum==1,"Saved duplicate solution did not normalize");
        }
        require(found,"Loft-3 editor lost solution parameter");
        s.directed_normals=true;AnchorSurfaceFilletNormal(doc,s,0);AnchorSurfaceFilletNormal(doc,s,1);
        int available=0;
        for(int mask=0;mask<4;++mask) {
            s.reverse={bool(mask&1),bool(mask&2)};
            auto firstResult=BuildSurfaceFillet(doc,s,failure);
            auto swapped=s;
            std::swap(swapped.bodies[0],swapped.bodies[1]);std::swap(swapped.faces[0],swapped.faces[1]);
            std::swap(swapped.reverse[0],swapped.reverse[1]);std::swap(swapped.u[0],swapped.u[1]);std::swap(swapped.v[0],swapped.v[1]);
            auto secondResult=BuildSurfaceFillet(doc,swapped,failure);
            require(firstResult.IsNull()==secondResult.IsNull(),"Normal-side availability depends on pick order");
            if(!firstResult.IsNull()) {
                ++available;GProp_GProps a,b;BRepGProp::SurfaceProperties(firstResult,a);BRepGProp::SurfaceProperties(secondResult,b);
                require(std::abs(a.Mass()-b.Mass())<1.e-5,"Normal-side geometry depends on pick order");
            }
            require(SurfaceFilletNormalGuides(doc,s,firstResult).size()==2,"Failed fillet lost anchored normals");
            std::cerr<<"directed mask "<<mask<<" available "<<!firstResult.IsNull()<<std::endl;
        }
        require(available>0,"No directed fillet can be built on Loft-3");
        return 0;
    }
    if(application.arguments().contains("--surface-fillet-file")) {
        QTemporaryDir temp;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        CAlfaDoc doc;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        const auto arg=application.arguments().indexOf("--surface-fillet-file");
        require(serializer.Load(application.arguments().value(arg+1),doc,room,view,error),"Cannot load fillet file");
        SurfaceFilletSettings s;s.bodies={14,18};s.radius=10;s.end_radius=20;
        require(SurfaceFilletHasCommonEdge(doc,s),"Fixture surfaces have no common edge");
        require(SurfaceFilletSolutionCount(doc,s)==1,"Common-edge fillet has phantom solutions");
        for(bool variable:{false,true}) for(bool trim:{false,true}) {
            s.variable=variable;s.trim=trim;std::string failure;
            const auto shape=BuildSurfaceFillet(doc,s,failure);
            std::cerr<<"variable="<<variable<<" trim="<<trim<<" "<<failure<<std::endl;
            require(!shape.IsNull(),"Surface fillet failed");
            const auto normals=SurfaceFilletNormalGuides(doc,s,shape);
            require(normals.size()==2,"Missing fillet construction normals");
        }
        // Non-adjacent but intersecting surfaces: fillet only, trimming disabled.
        CAlfaDoc crossing;
        TopoDS_Shape firstShape=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-50,50,-50,50).Face();
        TopoDS_Shape secondShape=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,1,0)),-50,50,-50,50).Face();
        auto first=std::make_unique<CSurfaceSet>(firstShape);
        auto second=std::make_unique<CSurfaceSet>(secondShape);
        auto* a=first.get();auto* b=second.get();crossing.AddObject(std::move(first));crossing.AddObject(std::move(second));
        // Exercise the real screen-pick path; face selection is not object selection.
        b->SetVisible(false);
        require(a->EnsureRenderMesh(),"Cannot mesh pick-test plane");
        OpenGLViewport pickViewport;pickViewport.resize(900,700);pickViewport.SetDocument(&crossing);
        pickViewport.SetSelectionMode(SelectionMode::Face);pickViewport.FitToDocument();
        QPoint pixel;require(pickViewport.ProjectWorldPoint({17,23,0},pixel),"Cannot project pick-test point");
        pickViewport.SelectAt(pixel,SelectionAction::Replace);
        CPoint3d hit;
        require(pickViewport.GetSurfaceSelectionPoint(a->m_id,0,hit),"Screen pick did not capture the selected face point");
        require(std::abs(hit.x-17)<1&&std::abs(hit.y-23)<1&&std::abs(hit.z)<1.e-6,"Normal anchor is not the clicked point");
        b->SetVisible(true);crossing.ClearSelection();
        SurfaceFilletSettings cross;cross.bodies={a->m_id,b->m_id};cross.radius=5;
        require(!SurfaceFilletHasCommonEdge(crossing,cross),"Crossing faces falsely share a boundary");
        require(SurfaceFilletSolutionCount(crossing,cross)==4,"Crossing planes lost a side combination");
        std::string failure;
        const auto crossingFillet=BuildSurfaceFillet(crossing,cross,failure);
        std::cerr<<"Crossing fillet: "<<!crossingFillet.IsNull()<<" "<<failure<<std::endl;
        require(!crossingFillet.IsNull(),failure.c_str());
        std::set<std::pair<int,int>> normalSides;
        for(int solution=0;solution<4;++solution) {
            cross.solution=solution;
            const auto shape=BuildSurfaceFillet(crossing,cross,failure);
            require(!shape.IsNull(),"Intersection solution failed");
            const auto normals=SurfaceFilletNormalGuides(crossing,cross,shape);
            require(normals.size()==2,"Missing intersection normals");
            require(std::abs(std::abs(normals[0].direction.z)-1)<1.e-6
                &&std::abs(std::abs(normals[1].direction.y)-1)<1.e-6,"Arrows are not support normals");
            normalSides.insert({normals[0].direction.z>0?1:-1,normals[1].direction.y>0?1:-1});
        }
        require(normalSides.size()==4,"Intersection arrows do not follow the four construction sides");
        cross.directed_normals=true;
        require(AnchorSurfaceFilletNormal(crossing,cross,0)&&AnchorSurfaceFilletNormal(crossing,cross,1),"Cannot anchor plane normals");
        for(int mask=0;mask<4;++mask) {
            cross.reverse={bool(mask&1),bool(mask&2)};
            const auto shape=BuildSurfaceFillet(crossing,cross,failure);
            require(!shape.IsNull(),"Directed crossing-plane combination failed");
            auto legacy=cross;legacy.directed_normals=false;
            const auto actual=SurfaceFilletNormalGuides(crossing,legacy,shape);
            const auto requested=SurfaceFilletNormalGuides(crossing,cross,shape);
            require(actual.size()==2&&requested.size()==2,"Cannot verify construction sides");
            for(int side=0;side<2;++side) {
                const auto& x=actual[side].direction;const auto& y=requested[side].direction;
                require(x.x*y.x+x.y*y.y+x.z*y.z>.999999,"Reverse flag changed arrow but not fillet side");
            }
        }
        cross.trim=true;require(BuildSurfaceFillet(crossing,cross,failure).IsNull(),"Trim accepted without common boundary");

        MainWindow window;
        require(serializer.Load(application.arguments().value(arg+1),window.document_,room,view,error),"Cannot load UI fixture");
        if(application.arguments().contains("--normal-preview")) {
            window.resize(1100,800);window.show();
            window.viewport_->SetCamera(view.camera);window.viewport_->FitToDocument();
            QElapsedTimer timer;timer.start();
            while(timer.elapsed()<700)application.processEvents();
        }
        window.undo_redo_.Reset();
        auto& live=window.document_;
        const auto count=live.GetObjects().size();
        auto select=[&](unsigned long id){
            live.SelectObjectById(id);dynamic_cast<CSolid*>(live.FindObjectById(id))->SetSelectedFace(0);
            window.viewport_->SelectionChanged();
        };
        auto open=[&](){
            window.ActivateParametricTool("SurfaceFillet");
            auto* dialog=window.findChild<QDialog*>("SurfaceFilletDialog");require(dialog,"No surface fillet dialog");
            select(14);
            require(window.viewport_->surface_fillet_normals_.size()==1,"First pick does not show a normal");
            const auto firstAnchor=window.viewport_->surface_fillet_normals_[0].origin;
            select(18);
            require(window.viewport_->surface_fillet_normals_.size()==2,"Second pick does not show a normal");
            const auto secondAnchor=window.viewport_->surface_fillet_normals_[0].origin;
            require(std::abs(firstAnchor.x-secondAnchor.x)+std::abs(firstAnchor.y-secondAnchor.y)+std::abs(firstAnchor.z-secondAnchor.z)<1.e-9,
                "Second pick moved the first normal");
            auto* ok=dialog->findChild<QPushButton*>("CreateSurfaceFillet");
            for(int mask=0;mask<4&&!ok->isEnabled();++mask) {
                dialog->findChild<QCheckBox*>("SurfaceFilletReverse1")->setChecked(mask&1);
                dialog->findChild<QCheckBox*>("SurfaceFilletReverse2")->setChecked(mask&2);
            }
            if(!ok->isEnabled())std::cerr<<dialog->findChild<QLabel*>("SurfaceFilletStatus")->text().toStdString()<<std::endl;
            require(ok->isEnabled(),"No fillet preview");return dialog;
        };
        auto* dialog=open();
        const auto beforeReverse=window.viewport_->surface_fillet_normals_;
        auto* reverse=dialog->findChild<QCheckBox*>("SurfaceFilletReverse1");reverse->toggle();
        const auto afterReverse=window.viewport_->surface_fillet_normals_;
        require(afterReverse.size()==2,"Reverse lost anchored normals");
        const auto& x=beforeReverse[0];const auto& y=afterReverse[0];
        require(std::abs(x.origin.x-y.origin.x)+std::abs(x.origin.y-y.origin.y)+std::abs(x.origin.z-y.origin.z)<1.e-9,
            "Reverse moved the pick point");
        require(x.direction.x*y.direction.x+x.direction.y*y.direction.y+x.direction.z*y.direction.z<-.999999,"Reverse did not flip the normal");
        require(std::abs(beforeReverse[1].direction.x-afterReverse[1].direction.x)<1.e-9,"Reverse 1 changed normal 2");
        reverse->toggle();
        require(window.viewport_->surface_fillet_normals_.size()==2,"Preview normal arrows missing");
        auto* normalsToggle=dialog->findChild<QCheckBox*>("SurfaceFilletNormals");
        require(normalsToggle,"Normal toggle missing");
        normalsToggle->setChecked(false);
        require(window.viewport_->surface_fillet_normals_.empty(),"Normal arrows cannot be hidden");
        normalsToggle->setChecked(true);
        require(window.viewport_->surface_fillet_normals_.size()==2,"Normal arrows cannot be restored");
        auto* trim=dialog->findChild<QCheckBox*>("SurfaceFilletTrim");require(trim->isEnabled(),"Common-edge trim disabled");
        trim->setChecked(true);
        require(!live.FindObjectById(14)->IsVisible()&&!live.FindObjectById(18)->IsVisible(),"Trim preview leaves original faces visible");
        if(application.arguments().contains("--normal-preview")) {
            QElapsedTimer timer;timer.start();
            while(timer.elapsed()<700)application.processEvents();
            require(window.viewport_->surface_fillet_normals_.size()==2,"Visible preview lost normal arrows");
            const auto path=application.arguments().value(application.arguments().indexOf("--normal-preview")+1);
            require(window.viewport_->grabFramebuffer().save(path),"Cannot save normal-arrow preview");
        }
        dialog->reject();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(window.viewport_->surface_fillet_normals_.empty(),"Cancel left normal arrows behind");
        require(live.GetObjects().size()==count&&live.FindObjectById(14)->IsVisible()&&live.FindObjectById(18)->IsVisible(),"Cancel lost source surfaces");
        dialog=open();
        dialog->findChild<QComboBox*>("SurfaceFilletRadiusType")->setCurrentIndex(1);
        dialog->findChild<QDoubleSpinBox*>("SurfaceFilletEndRadius")->setValue(20);
        dialog->findChild<QCheckBox*>("SurfaceFilletTrim")->setChecked(true);
        dialog->accept();application.sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(live.GetObjects().size()==count+1,"Fillet was not committed");
        require(live.GetSelectedObject()!=nullptr,"Accepted fillet is not selected");
        const auto id=live.GetSelectedObject()->m_id;
        require(window.undo_redo_.Undo(),"Fillet undo failed");
        require(live.GetObjects().size()==count&&live.FindObjectById(14)->IsVisible(),"Undo did not restore sources");
        require(window.undo_redo_.Redo(),"Fillet redo failed");
        require(live.FindObjectById(id)&&!live.FindObjectById(14)->IsVisible(),"Redo lost trimmed fillet");
        const auto path=temp.filePath("fillet.dom3d");
        require(serializer.Save(path,live,room,view,{},error),"Cannot save fillet");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,error),"Cannot reload fillet");
        size_t index=0;while(index<loaded.GetObjects().size()&&loaded.GetObjects()[index]->m_id!=id)++index;
        require(index<loaded.GetObjects().size(),"Saved fillet missing");
        auto settings=ReadSurfaceFilletParameters(loaded.GetObjects()[index]->GetParametricParameters());
        require(settings.trim&&settings.variable&&settings.end_radius==20,"Fillet settings not persisted");
        const auto originalSettings=ReadSurfaceFilletParameters(live.FindObjectById(id)->GetParametricParameters());
        require(settings.directed_normals&&settings.anchor_valid[0]&&settings.anchor_valid[1]
            &&settings.reverse==originalSettings.reverse&&settings.u==originalSettings.u&&settings.v==originalSettings.v,
            "Normal directions or pick anchors were not persisted");
        require(RebuildSurfaceFilletObject(loaded,index,settings,failure),"Saved fillet cannot rebuild");
        ToolRegistry registry;
        require(registry.ReplayOperations(index,loaded),"Fillet operation history cannot replay");
        auto active=registry.ActiveObjectFromDocument(index,*loaded.GetObjects()[index],0,&loaded);
        require(active.tool_id=="SurfaceFillet","Fillet editor is not registered");
        OpenGLViewport editViewport;editViewport.SetDocument(&loaded);editViewport.SetSolidDimensionEdit(active);
        require(editViewport.surface_fillet_normals_.size()==2,"Saved fillet editor has no normal arrows");
        editViewport.ClearSolidDimensionEdit();
        require(editViewport.surface_fillet_normals_.empty(),"Editor close left normal arrows behind");
        for(auto& p:active.parameters)if(p.id=="radius")p.value=12;
        registry.Rebuild(active,loaded);
        require(ReadSurfaceFilletParameters(loaded.GetObjects()[index]->GetParametricParameters()).radius==12,
                "Fillet radius edit was not applied");
        const auto old=dynamic_cast<CSolid*>(loaded.GetObjects()[index].get())->m_Shape;
        settings.radius=0;
        require(!RebuildSurfaceFilletObject(loaded,index,settings,failure),"Invalid fillet radius accepted");
        require(dynamic_cast<CSolid*>(loaded.GetObjects()[index].get())->m_Shape.IsSame(old),"Failed edit destroyed previous fillet");
        if(application.arguments().contains("--output")) {
            const auto target=application.arguments().value(application.arguments().indexOf("--output")+1);
            require(serializer.Save(target,loaded,room,view,{},error),"Cannot save fillet example");
        }
        std::cout<<"Surface fillet geometry, preview, cancel, undo, save and rebuild passed\n";return 0;
    }
    if (application.arguments().contains("--swept-curve-input")) {
        QVariantMap preferences;
        if (application.arguments().contains("--user-preferences")) {
            QSettings source(QSettings::NativeFormat,QSettings::UserScope,"Dom3D","Dom3D_Pro");
            for (const auto& key : source.allKeys()) preferences.insert(key,source.value(key));
        }
        QTemporaryDir settingsDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Dom3D","Dom3D_Pro");
        for (auto it=preferences.cbegin();it!=preferences.cend();++it) settings.setValue(it.key(),it.value());
        settings.sync();
        MainWindow window;
        const int arg = application.arguments().indexOf("--swept-curve-input");
        window.OpenProjectFromPath(application.arguments().value(arg+1));
        auto& viewport = *window.viewport_;
        viewport.resize(900,700);
        // Exercise the edge extraction command before starting a fresh curve.
        bool extracted = false;
        for (const auto& object : window.document_.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid || solid->m_Shape.IsNull()) continue;
            solid->SetVisible(true);
            require(solid->EnsureRenderMesh(),"Cannot prepare extraction surface");
            window.document_.SelectObjectById(solid->m_id);
            solid->SetSelectedEdge(0,0);
            const auto count = window.document_.GetObjects().size();
            window.ExtractSurfaceEdge();
            extracted = window.document_.GetObjects().size() > count;
            break;
        }
        require(extracted,"Cannot extract test edge");
        for (bool workPlane : {false,true}) for (bool snap : {false,true}) for (auto kind : {
                MainWindow::SpatialCurveKind::Polyline, MainWindow::SpatialCurveKind::BSpline,
                MainWindow::SpatialCurveKind::Bezier, MainWindow::SpatialCurveKind::Nurbs}) {
            viewport.SetSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane,workPlane);
            viewport.snapping_enabled_ = snap;
            window.ActivateParametricTool(kind == MainWindow::SpatialCurveKind::Polyline ? "PolylineCurve"
                : kind == MainWindow::SpatialCurveKind::BSpline ? "BSplineCurve"
                : kind == MainWindow::SpatialCurveKind::Bezier ? "BezierCurve3D" : "NurbsCurve3D");
            for (const QPoint point : {QPoint(200,200),QPoint(300,250),QPoint(400,200)}) {
                QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(point),
                                  Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(&viewport,&press);
                QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),QPointF(point),
                                    Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(&viewport,&release);
            }
            require(window.spatial_curve_point_count_ == 3,"Saved Swept project blocks spline input");
            window.CancelSpatialCurve();
        }
        viewport.SetSnapTargetEnabled(OpenGLViewport::SnapTarget::WorkPlane,true);
        viewport.SetTool(ToolMode::DrawSpline);
        viewport.BeginDrawSplineStroke(QPoint(200,200));
        require(!viewport.draw_spline_raw_points_.empty(),"Work plane blocks freehand spline");
        viewport.AppendDrawSplineStroke(QPoint(300,250));
        require(viewport.draw_spline_raw_points_.size() > 1,"Work plane blocks next freehand sample");
        viewport.CancelDrawSplineStroke();
        // Explicit plane picking must still reject an edge-on plane.
        viewport.SetTool(ToolMode::Select);
        viewport.BeginPick3DPointOnPlane({}, {0,0,1}, "Plane test");
        CPoint3d rejected;
        require(!viewport.ScreenToWorldPlane(QPoint(200,200),{}, {0,0,1},rejected),
                "Explicit plane constraint was lost");
        std::cout << "Saved Swept spline input passed\n";
        return 0;
    }
    if (application.arguments().contains("--surface-bridge-closed-only")) {
        for(bool reverse:{false,true}) {
            CAlfaDoc doc;SurfaceBridgeEdges refs;
            for(int side=0;side<2;++side) {
                Handle(Geom_CylindricalSurface) surface=new Geom_CylindricalSurface(gp_Ax3(gp_Pnt(0,0,side?30:0),gp_Dir(0,0,1)),10);
                if(side && reverse)surface->UReverse();
                const double phase=side?.731:0.;
                auto shape=BRepBuilderAPI_MakeFace(surface,phase,phase+2*std::acos(-1.),0.,10.,1.e-7).Shape();
                auto body=std::make_unique<CSurfaceSet>(shape);
                require(body->ReBuldMesh(),"Cannot mesh periodic support");
                auto* ptr=body.get();doc.AddObject(std::move(body));refs[side].body=ptr->m_id;refs[side].face=0;
                auto* face=ptr->GetSurfaceFace(0);
                for(int e=0;e<face->GetEdgeCount();++e) {
                    BRepAdaptor_Curve edge(*face->GetTopoEdge(e));
                    const auto a=edge.Value(edge.FirstParameter()),b=edge.Value(edge.LastParameter());
                    if(a.Distance(b)<1.e-7 && std::abs(a.Z()-(side?30:10))<1.e-7)refs[side].edge=e;
                }
                require(refs[side].edge>=0,"Cannot locate closed support edge");
            }
            for(int mode=0;mode<=2;++mode) {
                refs[0].continuity=refs[1].continuity=mode;
                std::string error;const auto shape=BuildSurfaceBridge(doc,refs,error);
                require(!shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(),"Shifted/reversed closed bridge failed");
                const auto surface=Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(TopoDS::Face(shape)));
                require(surface->IsUPeriodic(),"Closed bridge has an open seam");
                for(int u=0;u<79;++u)for(int v=0;v<=8;++v) {
                    const auto p=surface->Value(u/79.,v/8.);
                    require(std::abs(std::hypot(p.X(),p.Y())-10)<1.e-4,"Closed bridge twists between shifted/reversed seams");
                    require(std::abs(p.Z()-(10+20*v/8.))<1.e-5,"Closed bridge deviates from cylinder");
                }
            }
        }
        std::cout<<"Closed bridges with shifted and reversed seams passed\n";
        return 0;
    }
    if (application.arguments().contains("--surface-bridge-file")) {
        CAlfaDoc doc;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,errorText;
        const int arg=application.arguments().indexOf("--surface-bridge-file");
        require(serializer.Load(application.arguments().value(arg+1),doc,room,view,errorText),"Cannot load bridge fixture");
        std::vector<std::pair<SurfaceBridgeEdge,gp_Pnt>> edges;
        for(const auto& object:doc.GetObjects()) {
            auto* body=dynamic_cast<CSolid*>(object.get());if(!body)continue;
            // Preserve the exact just-loaded BRep for this regression.
            require(body->EnsureRenderMesh(),"Cannot initialize support edges");
            for(int f=0;f<body->GetNumSurfaces();++f) {
                auto* face=body->GetSurfaceFace(f);
                for(int e=0;e<face->GetEdgeCount();++e) {
                    BRepAdaptor_Curve c(*face->GetTopoEdge(e));
                    auto point=c.Value((c.FirstParameter()+c.LastParameter())*.5);
                    edges.push_back({{body->m_id,f,e,1},point});
                }
            }
        }
        SurfaceBridgeEdges refs;double best=1.e100;
        for(const auto& a:edges)for(const auto& b:edges) if(a.first.body<b.first.body) {
            const double distance=a.second.SquareDistance(b.second);
            if(distance<best){best=distance;refs={a.first,b.first};}
        }
        require(best<1.e99,"Need two support surfaces");
        for(const auto& object:doc.GetObjects())
            if(object->GetParametricToolId()=="SurfaceBridge") {
                refs=ReadSurfaceBridgeParameters(object->GetParametricParameters());break;
            }
        std::cout<<"Pair "<<refs[0].body<<":"<<refs[0].face<<":"<<refs[0].edge<<" / "
            <<refs[1].body<<":"<<refs[1].face<<":"<<refs[1].edge<<std::endl;
        for(int firstMode=0;firstMode<=2;++firstMode) for(int secondMode=0;secondMode<=2;++secondMode)
        for(bool reversed:{false,true}) {
            auto pair=refs;pair[0].continuity=firstMode;pair[1].continuity=secondMode;
            if(reversed)std::swap(pair[0],pair[1]);
            std::string error;
            auto shape=BuildSurfaceBridge(doc,pair,error);
            if(shape.IsNull())std::cerr<<"G"<<firstMode<<"/G"<<secondMode<<": "<<error<<std::endl;
            require(!shape.IsNull(),"Bridge failed on curved support fixture");
            require(BRepCheck_Analyzer(shape).IsValid(),"Invalid bridge BRep");
            auto surface=Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(TopoDS::Face(shape)));
            require(!surface.IsNull(),"Bridge must be a natural B-spline patch");
            double u0,u1,v0,v1;surface->Bounds(u0,u1,v0,v1);
            if(application.arguments().contains("--closed-bridge")) {
                require(surface->IsUPeriodic(),"Closed bridge must be periodic");
                for(int k=0;k<=20;++k) {
                    gp_Pnt a,b;gp_Vec au,av,bu,bv;
                    surface->D1(u0,k/20.,a,au,av);surface->D1(u1,k/20.,b,bu,bv);
                    require(a.Distance(b)<1.e-9 && au.Subtracted(bu).Magnitude()<1.e-8
                        && av.Subtracted(bv).Magnitude()<1.e-8,"Periodic bridge seam is not smooth");
                }
            }
            int edgeCount=0;
            for(TopExp_Explorer ex(shape,TopAbs_EDGE);ex.More();ex.Next())++edgeCount;
            require(edgeCount==4,"Bridge must have four natural sides");
            for(int side=0;side<2;++side) {
                auto* solid=dynamic_cast<CSolid*>(doc.FindObjectById(pair[side].body));
                auto* face=solid->GetSurfaceFace(pair[side].face);
                const auto support=BRep_Tool::Surface(TopoDS::Face(face->m_Face));
                BRepAdaptor_Curve edge(*face->GetTopoEdge(pair[side].edge));
                for(int i=0;i<=30;++i) {
                    const auto point=edge.Value(edge.FirstParameter()+(edge.LastParameter()-edge.FirstParameter())*i/30.);
                    GeomAPI_ProjectPointOnSurf bridgeProjection(point,surface),supportProjection(point,support);
                    require(bridgeProjection.NbPoints()>0 && bridgeProjection.LowerDistance()<1.e-4,"Bridge seam deviates from input edge");
                    double u,v,x,y;bridgeProjection.LowerDistanceParameters(u,v);supportProjection.LowerDistanceParameters(x,y);
                    require(std::abs(v-(side?v1:v0))<1.e-5,"Input edge is not a natural V boundary");
                    GeomLProp_SLProps a(surface,u,v,2,1.e-9),b(support,x,y,2,1.e-9);
                    if(pair[side].continuity>=1)
                        require(std::abs(a.Normal().Dot(b.Normal()))>std::cos(.005),"G1 seam normal mismatch");
                    if(pair[side].continuity==2) {
                        const double sign=a.Normal().Dot(b.Normal())<0?-1.:1.;
                        const double mean=a.MeanCurvature(),target=sign*b.MeanCurvature();
                        require(std::abs(mean-target)<2.e-5+.03*std::abs(target),"G2 mean curvature mismatch");
                        require(std::abs(a.GaussianCurvature()-b.GaussianCurvature())<1.e-5+.03*std::abs(b.GaussianCurvature()),"G2 Gaussian curvature mismatch");
                    }
                }
            }
        }
        QTemporaryDir settings;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        for(bool reverse:{false,true}) {
            MainWindow window;auto& live=window.document_;
            require(serializer.Load(application.arguments().value(arg+1),live,room,view,errorText),"Cannot load UI fixture");
            for(const auto& o:live.GetObjects())if(auto* solid=dynamic_cast<CSolid*>(o.get()))solid->EnsureRenderMesh();
            window.ActivateParametricTool("SurfaceBridge");
            auto* dialog=window.findChild<QDialog*>("SurfaceBridgeDialog");
            require(dialog,"Bridge dialog missing");
            for(int i: reverse ? std::vector<int>{1,0} : std::vector<int>{0,1}) {
                const auto& ref=refs[i];live.ClearSelection();live.SelectObjectById(ref.body);
                dynamic_cast<CSolid*>(live.FindObjectById(ref.body))->SetSelectedEdge(ref.face,ref.edge);
                window.viewport_->SelectionChanged();
            }
            auto* create=dialog->findChild<QPushButton*>("CreateSurfaceBridge");
            auto* status=dialog->findChild<QLabel*>("SurfaceBridgeStatus");
            require(create && create->isEnabled(),"Create unavailable for valid G1 bridge");
            require(status && !status->text().trimmed().isEmpty(),"Missing bridge status");
            const auto capture=qEnvironmentVariable("DOM3D_SURFACE_BRIDGE_CAPTURE");
            if(!capture.isEmpty() && !reverse) {
                window.show();window.viewport_->SetCamera(view.camera);window.viewport_->FitToDocument();
                QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();
                window.viewport_->grab().save(capture);dialog->grab().save(capture+".dialog.png");
            }
            auto* first=dialog->findChild<QComboBox*>("SurfaceBridgeContinuity1");
            auto* second=dialog->findChild<QComboBox*>("SurfaceBridgeContinuity2");
            first->setCurrentIndex(2);second->setCurrentIndex(2);
            require(create->isEnabled(),"G2 preview failed on curved support fixture");
            first->setCurrentIndex(1);second->setCurrentIndex(1);
            require(create->isEnabled(),"Cannot recover G1 preview after G2 failure");
            if(application.arguments().contains("--closed-bridge") && !reverse) {
                QTemporaryDir saved;
                require(serializer.Save(saved.filePath("bridge.dom3d"),live,room,view,{},errorText),"Cannot save display regression");
                size_t before=0;
                for(const auto& o:live.GetObjects())if(o->GetParametricToolId()=="SurfaceBridge")
                {
                    auto* face=dynamic_cast<CSolid*>(o.get())->GetSurfaceFace(0);
                    before=face->pMesh3D->GetVertices().size();
                    std::cout<<"Bridge grid "<<face->m_QtyU<<" x "<<face->m_QtyV<<std::endl;
                    const auto geom=BRep_Tool::Surface(TopoDS::Face(face->m_Face));
                    const auto& vertices=face->pMesh3D->GetVertices();
                    // Check meridian chords against the analytic profile, not
                    // just the total count (many circular rows can hide 4 bands).
                    double maxGap=0,normalError=0;
                    const auto& normals=face->pMesh3D->GetNormals();
                    for(int j=0;j+1<face->m_QtyV;++j) {
                        const size_t a=j*face->m_QtyU,b=(j+1)*face->m_QtyU;
                        const auto mid=(vertices[a]+vertices[b])*.5f;
                        GeomAPI_ProjectPointOnSurf projection(gp_Pnt(mid.x,mid.y,mid.z),geom);
                        require(projection.NbPoints()>0,"Cannot evaluate display meridian");
                        maxGap=std::max(maxGap,projection.LowerDistance());
                        double u,v;projection.LowerDistanceParameters(u,v);
                        GeomLProp_SLProps props(geom,u,v,1,1.e-9);
                        const auto n=(normals[a]+normals[b])*.5f;
                        const auto expected=gp_Vec(n.x,n.y,n.z).Normalized();
                        normalError=std::max(normalError,std::acos(std::clamp(std::abs(expected.Dot(props.Normal())),0.,1.)));
                    }
                    std::cout<<"Meridian max chord gap="<<maxGap<<", normal error="<<normalError<<std::endl;
                    require(normalError<.005,"Bridge display meridian normal interpolation is visibly faceted");
                }
                CAlfaDoc reopened;require(serializer.Load(saved.filePath("bridge.dom3d"),reopened,room,view,errorText),"Cannot reload display regression");
                for(const auto& o:reopened.GetObjects())if(o->GetParametricToolId()=="SurfaceBridge") {
                    auto* solid=dynamic_cast<CSolid*>(o.get());require(solid->EnsureRenderMesh(),"Cannot display saved bridge");
                    const auto after=solid->GetSurfaceFace(0)->pMesh3D->GetVertices().size();
                    std::cout<<"Bridge display vertices: preview="<<before<<", reopened="<<after<<std::endl;
                    require(before==after,"Reopening a bridge loses adaptive display mesh quality");
                }
            }
            if(!capture.isEmpty() && !reverse) {
                if(qEnvironmentVariableIsSet("DOM3D_BRIDGE_CAPTURE_G2")) {
                    first->setCurrentIndex(2);second->setCurrentIndex(2);
                    require(create->isEnabled(),"Cannot capture G2 bridge");
                }
                dialog->accept();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
                live.ClearSelection();window.viewport_->update();application.processEvents();
                window.viewport_->grab().save(capture+".result.png");
                CMesh3D::SetZebraAnalysisEnabled(true);
                window.viewport_->update();application.processEvents();
                window.viewport_->grab().save(capture+".zebra.png");
                CMesh3D::SetZebraAnalysisEnabled(false);
                require(serializer.Save(capture+".dom3d",live,room,view,{},errorText),"Cannot save bridge result");
                CAlfaDoc loaded;require(serializer.Load(capture+".dom3d",loaded,room,view,errorText),"Cannot reload closed bridge");
                for(const auto& o:loaded.GetObjects())if(auto* solid=dynamic_cast<CSolid*>(o.get()))
                    require(solid->EnsureRenderMesh(),"Cannot initialize reloaded bridge edges");
                bool found=false;
                for(const auto& o:loaded.GetObjects())if(o->GetParametricToolId()=="SurfaceBridge") {
                    std::string failure;const auto rebuilt=BuildSurfaceBridge(loaded,ReadSurfaceBridgeParameters(o->GetParametricParameters()),failure);
                    require(!rebuilt.IsNull(),"Saved bridge cannot replay its support references");found=true;
                }
                require(found,"Saved result lost bridge history");
                continue;
            }
            dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        std::cout<<"Four-spline bridge: G1 preview, both selection orders, errors and recovery passed\n";
        return 0;
    }
    if (application.arguments().contains("--surface-patch-file")) {
        MainWindow window;auto& doc=window.document_;Dom3DProjectSerializer serializer;
        ProjectViewState view;QString room,errorText;
        int argument=application.arguments().indexOf("--surface-patch-file");
        require(serializer.Load(application.arguments().value(argument+1),doc,room,view,errorText),"Cannot load user's patch fixture");
        CSolid* patch=nullptr;size_t index=0;
        for(size_t i=0;i<doc.GetObjects().size();++i) {
            auto* solid=dynamic_cast<CSolid*>(doc.GetObjects()[i].get());if(!solid)continue;
            require(solid->EnsureRenderMesh(),"Cannot mesh fixture support");
            if(solid->GetParametricToolId()=="SurfacePatch"){patch=solid;index=i;}
        }
        require(patch && patch->IsParametric() && patch->GetNumOperations()>0,"Saved Patch lost parametric history");
        auto refs=ReadSurfaceEdgePatchParameters(patch->GetParametricParameters());
        for(const auto& ref:refs)require(ref.continuity==0,"User's successful patch should still be G0");
        auto active=window.tool_registry_.ActiveObjectFromDocument(index,*patch,0,&doc);
        const auto original=patch->m_Shape;
        for(auto& parameter:active.parameters)if(parameter.id.find(".continuity")!=std::string::npos)parameter.value=1;
        std::string error;
        require(!window.tool_registry_.TryRebuildSurfacePatch(active,doc,error),"Incompatible G1 patch unexpectedly succeeded");
        require(error.find("Edges ")!=std::string::npos && error.find("degrees")!=std::string::npos,"Missing numbered corner diagnosis");
        std::cout<<error<<std::endl;
        require(patch->m_Shape.IsSame(original),"Failed G1 replaced original patch");
        for(const auto& ref:ReadSurfaceEdgePatchParameters(patch->GetParametricParameters()))require(ref.continuity==0,"Failed G1 changed saved continuity");
        doc.SelectObjectById(patch->m_id);window.active_parametric_object_=active;
        window.property_panel_->SetActiveObject(active);window.property_panel_->ParametersChanged();application.processEvents();
        for(const auto& parameter:window.property_panel_->ActiveObject().parameters)
            if(parameter.id.find(".continuity")!=std::string::npos)require(parameter.value==0,"UI still shows unapplied G1");
        require(window.statusBar()->currentMessage().contains("NOT rebuilt"),"Missing failure status after property edit");
        window.ActivateParametricTool("SurfacePatch");auto* dialog=window.findChild<QDialog*>("SurfacePatchDialog");
        for(const auto& ref:refs){doc.ClearSelection();doc.SelectObjectById(ref.body);
            dynamic_cast<CSolid*>(doc.FindObjectById(ref.body))->SetSelectedEdge(ref.face,ref.edge);window.viewport_->SelectionChanged();}
        require(!dialog->findChild<QPushButton*>("CreateSurfacePatch")->isEnabled(),"Create enabled for failed preview");
        require(dialog->findChild<QLabel*>("SurfacePatchStatus")->text().contains("NOT BUILT"),"Ambiguous failed-preview status");
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        std::cout<<"User Patch: saved G0 history intact, G1 incompatibility diagnosed, fields restored, failed preview cannot be accepted"<<std::endl;
        return 0;
    }
    if (application.arguments().contains("--surface-edge-patch")) {
        MainWindow window;auto& doc=window.document_;doc.GetObjects().clear();
        SurfacePatchEdges refs;
        const double ranges[4][4]={{0,10,-8,0},{10,18,0,10},{0,10,10,18},{-8,0,0,10}};
        const double mid[4][2]={{5,0},{10,5},{5,10},{0,5}};
        const auto height=[](double x,double y){return .015*x*x+.01*x*y+.02*y*y;};
        for(int side=0;side<4;++side) {
            const auto* r=ranges[side];double dx=r[1]-r[0],dy=r[3]-r[2];
            TColgp_Array2OfPnt poles(1,3,1,3);
            for(int i=0;i<3;++i)for(int j=0;j<3;++j) {
                double x=r[0]+dx*i/2.,y=r[2]+dy*j/2.;
                poles(i+1,j+1)=gp_Pnt(x,y,height(x,y)-(i==1?.015*dx*dx/4:0)-(j==1?.02*dy*dy/4:0));
            }
            Handle(Geom_BezierSurface) support=new Geom_BezierSurface(poles);
            TopoDS_Shape shape=BRepBuilderAPI_MakeFace(support,1.e-7).Shape();
            auto object=std::make_unique<CSurfaceSet>(shape);object->SetName("Patch support");
            require(object->ReBuldMesh(),"Cannot mesh patch support");auto* body=object.get();doc.AddObject(std::move(object));
            refs[side].body=body->m_id;refs[side].face=0;
            auto* face=body->GetSurfaceFace(0);gp_Pnt target(mid[side][0],mid[side][1],height(mid[side][0],mid[side][1]));
            for(int e=0;e<face->GetEdgeCount();++e){BRepAdaptor_Curve curve(*face->GetTopoEdge(e));
                if(curve.Value((curve.FirstParameter()+curve.LastParameter())*.5).Distance(target)<1.e-6)refs[side].edge=e;}
            require(refs[side].edge>=0,"Missing patch boundary");
        }
        std::string error;
        for(int mode=0;mode<=2;++mode) {
            auto pair=refs;for(auto& ref:pair)ref.continuity=mode;
            std::swap(pair[1],pair[2]); // Deliberately select a non-cyclic order.
            auto shape=BuildSurfaceEdgePatch(doc,pair,error);
            if(shape.IsNull())std::cerr<<"Patch G"<<mode<<": "<<error<<std::endl;
            require(!shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(),"Four-sided patch failed");
            auto surface=Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(TopoDS::Face(shape)));
            require(!surface.IsNull(),"Patch must be natural B-spline");
            for(const auto& ref:pair) {
                auto* face=dynamic_cast<CSolid*>(doc.FindObjectById(ref.body))->GetSurfaceFace(0);
                auto support=BRep_Tool::Surface(TopoDS::Face(face->m_Face));BRepAdaptor_Curve edge(*face->GetTopoEdge(ref.edge));
                for(int i=0;i<=17;++i) {
                    auto point=edge.Value(edge.FirstParameter()+(edge.LastParameter()-edge.FirstParameter())*i/17.);
                    GeomAPI_ProjectPointOnSurf bp(point,surface),sp(point,support);
                    require(bp.NbPoints()>0 && bp.LowerDistance()<1.e-4,"Patch boundary gap");
                    double u,v,x,y;bp.LowerDistanceParameters(u,v);sp.LowerDistanceParameters(x,y);
                    require(std::min({u,1-u,v,1-v})<1.e-5,"Patch has trimmed boundaries");
                    GeomLProp_SLProps a(surface,u,v,2,1.e-9),b(support,x,y,2,1.e-9);
                    if(mode)require(std::abs(a.Normal().Dot(b.Normal()))>std::cos(.005),"Patch G1 mismatch");
                    if(mode==2){double sign=a.Normal().Dot(b.Normal())<0?-1.:1.;
                        require(std::abs(a.MeanCurvature()-sign*b.MeanCurvature())<2.e-5+.03*std::abs(b.MeanCurvature()),"Patch G2 mismatch");}
                }
            }
            std::cout<<"Patch G"<<mode<<" passed"<<std::endl;
        }
        auto invalid=refs;invalid[3]=invalid[0];require(BuildSurfaceEdgePatch(doc,invalid,error).IsNull() && !error.empty(),"Patch accepted duplicate edge");
        invalid=refs;invalid[0].edge=(invalid[0].edge+1)%4;
        require(BuildSurfaceEdgePatch(doc,invalid,error).IsNull() && !error.empty(),"Patch accepted open contour");
        // Same boundary, but tilt one support away from its neighbours at both corners.
        auto* tilted=dynamic_cast<CSolid*>(doc.FindObjectById(refs[1].body));const auto original=tilted->m_Shape;
        TColgp_Array2OfPnt tiltedPoles(1,3,1,3);
        for(int i=0;i<3;++i)for(int j=0;j<3;++j){double x=10+4*i,y=5*j;
            tiltedPoles(i+1,j+1)=gp_Pnt(x,y,height(x,y)-(i==1?.015*64/4:0)-(j==1?.02*100/4:0)+.5*(x-10));}
        Handle(Geom_BezierSurface) tiltedSurface=new Geom_BezierSurface(tiltedPoles);
        tilted->m_Shape=BRepBuilderAPI_MakeFace(tiltedSurface,1.e-7).Shape();require(tilted->ReBuldMesh(),"Cannot mesh tilted support");
        require(BuildSurfaceEdgePatch(doc,refs,error).IsNull() && error.find("corner")!=std::string::npos,"Patch accepted incompatible corner normals");
        tilted->m_Shape=original;require(tilted->ReBuldMesh(),"Cannot restore support");
        doc.ClearSelection();window.undo_redo_.Reset();
        const auto pick=[&]{for(int index:{2,0,3,1}) {auto& ref=refs[index];doc.ClearSelection();doc.SelectObjectById(ref.body);
            dynamic_cast<CSolid*>(doc.FindObjectById(ref.body))->SetSelectedEdge(ref.face,ref.edge);window.viewport_->SelectionChanged();}};
        window.ActivateParametricTool("SurfacePatch");auto* dialog=window.findChild<QDialog*>("SurfacePatchDialog");
        require(dialog,"Patch dialog missing");pick();
        auto* create=dialog->findChild<QPushButton*>("CreateSurfacePatch");
        require(create && create->isEnabled() && doc.GetObjects().size()==5,"Patch preview missing");
        for(int i=1;i<=4;++i)dialog->findChild<QComboBox*>(QString("SurfacePatchContinuity%1").arg(i))->setCurrentIndex(2);
        require(create->isEnabled(),"Patch G2 preview missing");create->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(window.undo_redo_.Undo() && doc.GetObjects().size()==4,"Cannot undo patch");
        require(window.undo_redo_.Redo() && doc.GetObjects().size()==5,"Cannot redo patch");
        for(const auto& ref:refs)require(window.tool_registry_.ReplayProfileDependents(ref.body,doc),"Patch dependency missing");
        QTemporaryDir temporary;Dom3DProjectSerializer serializer;ProjectViewState view;QString room,loadError;
        const auto path=temporary.filePath("patch.dom3d");
        require(serializer.Save(path,doc,"Surfaces",view,{},loadError),"Cannot save patch");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,loadError),"Cannot load patch");
        require(loaded.GetObjects().back()->GetParametricToolId()=="SurfacePatch","Patch history missing after load");
        for(const auto& ref:refs)require(window.tool_registry_.ReplayProfileDependents(ref.body,loaded),"Patch lost dependencies after load");
        window.ActivateParametricTool("SurfacePatch");pick();dialog=window.findChild<QDialog*>("SurfacePatchDialog");
        require(doc.GetObjects().size()==6,"Second patch preview missing");dialog->reject();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);require(doc.GetObjects().size()==5,"Cancel retained patch preview");
        const auto capture=qEnvironmentVariable("DOM3D_PATCH_CAPTURE");
        if(!capture.isEmpty()) {
            window.show();doc.ClearSelection();window.viewport_->FitToDocument();application.processEvents();
            view.camera=window.viewport_->GetCamera();
            require(serializer.Save(capture+".dom3d",doc,"Surfaces",view,{},loadError),"Cannot save patch demo");
            require(window.viewport_->CaptureSceneImage({1000,700}).save(capture+".png"),"Cannot capture patch demo");
        }
        std::cout<<"Surface Patch geometry, order, invalid inputs, UI, undo, persistence and dependencies passed"<<std::endl;
        return 0;
    }
    if (application.arguments().contains("--surface-bridge-complex")) {
        const char* names[]={"DoubleCurvature","TwistedTaper","Rational","ReversedParameters"};
        double maxGap=0,maxAngle=0,maxMeanError=0;
        for(int scenario=0;scenario<4;++scenario) {
            CAlfaDoc doc;SurfaceBridgeEdges refs;
            for(int side=0;side<2;++side) {
                TColgp_Array2OfPnt poles(1,4,1,4);TColStd_Array2OfReal weights(1,4,1,4);
                for(int i=0;i<4;++i)for(int j=0;j<4;++j) {
                    const double u=i/3.,v=j/3.;
                    const double x=(side?30.:0.)+10*v;
                    const double width=side && scenario==1?24.:36.;
                    const double y=(u-.5)*width+(side?3.:0.);
                    const double bend=(i==1||i==2?8.:-3.)*(side?-.7:1.);
                    const double cross=(j==1||j==2?2.:-1.)*(side?1.:-1.);
                    const double twist=(scenario==1?10.:3.)*(u-.5)*(v-.5)*(side?-1.:1.);
                    const double z=(side?5.:0.)+bend+cross+twist;
                    const int row=scenario==3 && side?4-i:i+1;
                    poles(row,j+1)=gp_Pnt(x,y,z);
                    weights(row,j+1)=scenario==2?1.+.25*std::sin((i+1.)*(j+1.)):1.;
                }
                Handle(Geom_BezierSurface) geom=new Geom_BezierSurface(poles,weights);
                TopoDS_Shape shape=BRepBuilderAPI_MakeFace(geom,1.e-7).Shape();
                auto body=std::make_unique<CSurfaceSet>(shape);body->SetName(side?"Support B":"Support A");
                require(body->ReBuldMesh(),"Cannot mesh complex support");
                auto* ptr=body.get();doc.AddObject(std::move(body));refs[side].body=ptr->m_id;refs[side].face=0;
                const auto target=geom->Value(.5,side?0.:1.);
                auto* face=ptr->GetSurfaceFace(0);
                for(int e=0;e<face->GetEdgeCount();++e) {
                    BRepAdaptor_Curve curve(*face->GetTopoEdge(e));
                    if(curve.Value((curve.FirstParameter()+curve.LastParameter())*.5).Distance(target)<1.e-6)refs[side].edge=e;
                }
                require(refs[side].edge>=0,"Cannot locate complex support boundary");
            }
            TopoDS_Shape saved;
            for(int first=0;first<=2;++first)for(int second=0;second<=2;++second)for(bool reversed:{false,true}) {
                auto pair=refs;pair[0].continuity=first;pair[1].continuity=second;
                if(reversed)std::swap(pair[0],pair[1]);
                std::string error;auto shape=BuildSurfaceBridge(doc,pair,error);
                if(shape.IsNull())std::cerr<<names[scenario]<<" G"<<first<<"/G"<<second<<" reverse="<<reversed<<": "<<error<<std::endl;
                require(!shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(),"Complex bridge failed");
                auto surface=Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(TopoDS::Face(shape)));
                require(!surface.IsNull(),"Complex bridge is not B-spline");
                for(int side=0;side<2;++side) {
                    auto* body=dynamic_cast<CSolid*>(doc.FindObjectById(pair[side].body));auto* face=body->GetSurfaceFace(0);
                    auto support=BRep_Tool::Surface(TopoDS::Face(face->m_Face));
                    BRepAdaptor_Curve edge(*face->GetTopoEdge(pair[side].edge));
                    for(int i=0;i<=40;++i) {
                        auto point=edge.Value(edge.FirstParameter()+(edge.LastParameter()-edge.FirstParameter())*i/40.);
                        GeomAPI_ProjectPointOnSurf bp(point,surface),sp(point,support);
                        require(bp.NbPoints()>0 && sp.NbPoints()>0,"Cannot project seam sample");
                        maxGap=std::max(maxGap,bp.LowerDistance());
                        require(bp.LowerDistance()<1.e-4,"Complex bridge boundary gap");
                        double u,v,x,y;bp.LowerDistanceParameters(u,v);sp.LowerDistanceParameters(x,y);
                        require(std::abs(v-side)<1.e-5,"Complex bridge boundary is trimmed");
                        GeomLProp_SLProps a(surface,u,v,2,1.e-9),b(support,x,y,2,1.e-9);
                        if(pair[side].continuity) {
                            const double dot=a.Normal().Dot(b.Normal());
                            maxAngle=std::max(maxAngle,std::acos(std::min(1.,std::abs(dot))));
                            require(std::abs(dot)>std::cos(.005),"Complex bridge G1 mismatch");
                            if(pair[side].continuity==2) {
                                const double target=(dot<0?-1.:1.)*b.MeanCurvature();
                                const double diff=std::abs(a.MeanCurvature()-target);maxMeanError=std::max(maxMeanError,diff);
                                require(diff<2.e-5+.03*std::abs(target),"Complex bridge G2 mean curvature mismatch");
                                require(std::abs(a.GaussianCurvature()-b.GaussianCurvature())<1.e-5+.03*std::abs(b.GaussianCurvature()),"Complex bridge G2 Gaussian curvature mismatch");
                            }
                        }
                    }
                }
                if(first==2 && second==2 && !reversed)saved=shape;
            }
            const auto output=qEnvironmentVariable("DOM3D_BRIDGE_COMPLEX_OUTPUT");
            if(!output.isEmpty()) {
                QDir().mkpath(output);auto bridge=std::make_unique<CSurfaceSet>(saved);
                refs[0].continuity=refs[1].continuity=2;
                bridge->SetName("Bridge G2");bridge->SetParametricDefinition("SurfaceBridge",SurfaceBridgeParameters(refs));
                require(bridge->ReBuldMesh(),"Cannot mesh complex G2 bridge");doc.AddObject(std::move(bridge));
                Dom3DProjectSerializer serializer;ProjectViewState view;QString error;
                require(serializer.Save(QDir(output).filePath(QString::fromLatin1(names[scenario])+".dom3d"),doc,"Surfaces",view,{},error),"Cannot save complex bridge example");
            }
            std::cout<<names[scenario]<<": all 18 combinations passed"<<std::endl;
        }
        std::cout<<"Max seam gap="<<maxGap<<", normal angle="<<maxAngle<<" rad, mean curvature error="<<maxMeanError<<std::endl;
        return 0;
    }
    if (application.arguments().contains("--surface-bridge-only")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        MainWindow window;
        auto& doc = window.document_;
        doc.GetObjects().clear();
        const auto add = [&](double x, double z, double edgeX) {
            TopoDS_Shape shape = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(x,0,z),gp_Dir(0,0,1)),0,10,0,10).Shape();
            auto surface = std::make_unique<CSurfaceSet>(shape);
            require(surface->ReBuldMesh(), "Cannot mesh bridge support");
            auto* body = surface.get(); doc.AddObject(std::move(surface));
            SurfaceBridgeEdge ref; ref.body=body->m_id; ref.face=0;
            auto* face=body->GetSurfaceFace(0);
            for(int i=0;i<face->GetEdgeCount();++i) {
                BRepAdaptor_Curve curve(*face->GetTopoEdge(i));
                auto p=curve.Value((curve.FirstParameter()+curve.LastParameter())*.5);
                if(std::abs(p.X()-edgeX)<1.e-6) ref.edge=i;
            }
            require(ref.edge>=0,"Cannot locate bridge edge"); return ref;
        };
        SurfaceBridgeEdges refs{add(0,0,10),add(20,5,20)};
        std::string error;
        for(int a=0;a<=2;++a) for(int b=0;b<=2;++b) {
            refs[0].continuity=a; refs[1].continuity=b;
            auto shape=BuildSurfaceBridge(doc,refs,error);
            if(shape.IsNull()) std::cerr<<a<<"/"<<b<<": "<<error<<std::endl;
            require(!shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(),"Bridge continuity combination failed");
            TopExp_Explorer resultFaces(shape,TopAbs_FACE);
            auto resultSurface=BRep_Tool::Surface(TopoDS::Face(resultFaces.Current()));
            for(int i=0;i<2;++i) for(double y:{2.5,5.,7.5}) {
                gp_Pnt point(i==0?10.:20.,y,i==0?0.:5.);
                GeomAPI_ProjectPointOnSurf projection(point,resultSurface);
                require(projection.NbPoints()>0 && projection.LowerDistance()<1.e-3,"Bridge does not follow source edge");
                double u,v;projection.LowerDistanceParameters(u,v);
                GeomLProp_SLProps props(resultSurface,u,v,2,1.e-7);
                if(refs[i].continuity>=1) require(props.IsNormalDefined() && std::abs(props.Normal().Z())>.999,
                    "Bridge failed independent tangent continuity check");
                if(refs[i].continuity==2) require(props.IsCurvatureDefined() && std::abs(props.MaxCurvature())<.04
                    && std::abs(props.MinCurvature())<.04,"Bridge failed independent curvature continuity check");
            }
        }
        auto invalid=refs;invalid[1]=invalid[0];
        require(BuildSurfaceBridge(doc,invalid,error).IsNull(),"Same edge accepted twice");
        doc.ClearSelection(); window.undo_redo_.Reset();
        window.ActivateParametricTool("SurfaceBridge");
        auto* dialog=window.findChild<QDialog*>("SurfaceBridgeDialog");
        require(dialog,"Surface Bridge dialog missing");
        for(const auto& ref:refs) {
            doc.ClearSelection();doc.SelectObjectById(ref.body);
            dynamic_cast<CSolid*>(doc.FindObjectById(ref.body))->SetSelectedEdge(ref.face,ref.edge);
            window.viewport_->SelectionChanged();
        }
        auto* create=dialog->findChild<QPushButton*>("CreateSurfaceBridge");
        require(create && create->isEnabled() && doc.GetObjects().size()==3,"Bridge preview missing");
        auto* marker=window.viewport_->findChild<QPushButton*>("SurfaceBridgeMarker1");
        require(marker,"Edge continuity marker missing"); marker->click();
        require(dialog->findChild<QComboBox*>("SurfaceBridgeContinuity1")->currentIndex()==2,"Marker did not select G2");
        const auto controlsCapture=qEnvironmentVariable("DOM3D_SURFACE_BRIDGE_CAPTURE");
        if(!controlsCapture.isEmpty()) {
            window.show();window.viewport_->FitToDocument();
            QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();
            require(marker->isVisible(),"Continuity marker is not visible in viewport");
            require(window.viewport_->grab().save(controlsCapture+".controls.png"),"Cannot capture continuity markers");
            require(dialog->grab().save(controlsCapture+".dialog.png"),"Cannot capture bridge dialog");
        }
        create->click(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(doc.GetObjects().size()==3,"Accept lost bridge");
        require(window.undo_redo_.Undo() && doc.GetObjects().size()==2,"Cannot undo Surface Bridge");
        require(window.undo_redo_.Redo() && doc.GetObjects().size()==3,"Cannot redo Surface Bridge");
        auto* bridge=dynamic_cast<CSolid*>(doc.GetObjects().back().get());
        require(bridge && bridge->GetParametricToolId()=="SurfaceBridge","Bridge lost parameters");
        const auto before=bridge->m_Shape;
        require(window.tool_registry_.ReplayProfileDependents(refs[0].body,doc),"Bridge dependency missing");
        require(!before.IsSame(bridge->m_Shape),"Bridge did not rebuild");
        Dom3DProjectSerializer serializer; ProjectViewState view;QString room,errorText;
        const auto path=settings.filePath("bridge.dom3d");
        require(serializer.Save(path,doc,"Surfaces",view,{},errorText),"Cannot save bridge");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,errorText),"Cannot reload bridge");
        require(window.tool_registry_.ReplayProfileDependents(refs[0].body,loaded),"Reloaded bridge lost dependencies");
        window.ActivateParametricTool("SurfaceBridge");
        dialog=window.findChild<QDialog*>("SurfaceBridgeDialog");
        for(const auto& ref:refs) {
            doc.ClearSelection();doc.SelectObjectById(ref.body);
            dynamic_cast<CSolid*>(doc.FindObjectById(ref.body))->SetSelectedEdge(ref.face,ref.edge);
            window.viewport_->SelectionChanged();
        }
        require(doc.GetObjects().size()==4,"Second preview missing");
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(doc.GetObjects().size()==3,"Cancel retained temporary bridge");
        const auto capture=qEnvironmentVariable("DOM3D_SURFACE_BRIDGE_CAPTURE");
        if(!capture.isEmpty()) {
            doc.ClearSelection();window.show();window.viewport_->FitToDocument();application.processEvents();
            require(window.viewport_->CaptureSceneImage({1000,700}).save(capture),"Cannot capture Surface Bridge result");
            view.camera=window.viewport_->GetCamera();
            require(serializer.Save(capture+".dom3d",doc,"Surfaces",view,{},errorText),"Cannot save bridge demo");
        }
        std::cout<<"Surface Bridge geometry, picking, markers, cancel, history and persistence passed\n";
        return 0;
    }
    if (application.arguments().contains("--four-curves-associative-only")) {
        QTemporaryDir settings; QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        MainWindow window;auto& doc=window.document_;doc.GetObjects().clear();
        std::vector<unsigned long> ids;
        const auto add=[&](std::initializer_list<CPoint3d> points) {
            auto curve=std::make_unique<CBSpline>("Boundary");
            for(auto point:points)curve->AddPoint(point);
            auto* ptr=curve.get();doc.AddObject(std::move(curve));ids.push_back(ptr->m_id);
        };
        add({{0,0,0},{5,0,1},{10,0,0}});add({{10,0,0},{10,5,0},{10,10,0}});
        add({{10,10,0},{5,10,0},{0,10,0}});add({{0,10,0},{0,5,0},{0,0,0}});
        doc.ClearSelection();for(auto id:ids)doc.SelectObjectById(id,SelectionAction::Add);
        require(doc.CreateFourSplineSurfaceFromSelection(),"Cannot create four-curve surface");
        const auto surfaceId=doc.GetSelectedObject()->m_id;
        window.undo_redo_.Reset();
        auto* surface=dynamic_cast<CSolid*>(doc.FindObjectById(surfaceId));
        auto original=surface->m_Shape;
        const auto center=[](const TopoDS_Shape& shape) {
            TopExp_Explorer faces(shape,TopAbs_FACE);auto face=TopoDS::Face(faces.Current());
            double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
            return BRep_Tool::Surface(face)->Value((u0+u1)*.5,(v0+v1)*.5);
        };
        const auto originalCenter=center(original);
        auto* curve=dynamic_cast<CBSpline*>(doc.FindObjectById(ids[0]));
        curve->SetPoint(1,{5,0,5});
        doc.SelectObjectById(ids[0]);
        window.viewport_->DocumentChanged();
        require(!surface->m_Shape.IsSame(original),"Four-curve surface unchanged after spline edit");
        require(center(surface->m_Shape).Distance(originalCenter)>.1,"Spline edit did not change surface geometry");
        require(window.undo_redo_.Undo() && window.undo_redo_.Redo(),"Four-curve undo/redo failed");
        Dom3DProjectSerializer serializer;ProjectViewState view;QString room,error;
        auto path=settings.filePath("four.dom3d");require(serializer.Save(path,doc,"Surfaces",view,{},error),"Cannot save four-curve surface");
        CAlfaDoc loaded;require(serializer.Load(path,loaded,room,view,error),"Cannot reload four-curve surface");
        require(window.tool_registry_.ReplayProfileDependents(ids[0],loaded),"Reloaded surface lost spline dependencies");
        require(window.tool_registry_.ReplayAllProfileDependents(loaded),"Bulk spline replay missed four-curve surface");
        std::cout<<"Four-curve associative updates, history and persistence passed\n";return 0;
    }
    if (application.arguments().contains("--low-poly-state-only")) {
        QTemporaryDir settingsDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        MainWindow window;
        auto& doc = window.document_;
        doc.GetObjects().clear();
        const auto add = [&](bool quadro) {
            TopoDS_Shape shape = BRepPrimAPI_MakeBox(20, 20, 20).Shape();
            auto body = std::make_unique<CSolid>(shape);
            body->MeshQuadro = quadro;
            body->MeshQuadroHoleSLX = quadro;
            body->ptchDensity = 0.5f;
            require(body->ReBuldMesh(2.f), "Cannot mesh Low Poly test box");
            auto* result = body.get();
            doc.AddObject(std::move(body));
            return result;
        };
        auto* hybrid = add(false);
        auto* quadro = add(true);
        const auto open = [&]() {
            window.ShowLowPolyTool();
            auto* dialog = window.findChild<QDialog*>("SolidLowPolyDialog");
            require(dialog, "Low Poly dialog missing");
            return dialog;
        };
        const auto mode = [](QDialog* dialog, const char* text) {
            for (auto* box : dialog->findChildren<QCheckBox*>())
                if (box->text() == text) return box;
            require(false, "Low Poly mesh control missing");
            return static_cast<QCheckBox*>(nullptr);
        };
        const auto close = [](QDialog* dialog) {
            dialog->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        };
        for (auto* body : {hybrid, quadro, hybrid}) {
            doc.SelectObjectById(body->m_id);
            auto* mesh = body->GetSurfaceFace(0)->pMesh3D;
            const auto faces = mesh->GetFaces().size();
            auto* dialog = open();
            require(mode(dialog, "Mesh Quadro")->isChecked() == body->MeshQuadro,
                    "Dialog does not reflect existing body mesh");
            require(mode(dialog, "Mesh Quadro Hole SLX")->isChecked() == body->MeshQuadroHoleSLX,
                    "Dialog does not reflect existing SLX mode");
            close(dialog);
            require(body->GetSurfaceFace(0)->pMesh3D == mesh && mesh->GetFaces().size() == faces,
                    "Opening or closing Low Poly rebuilt existing mesh");
        }
        doc.SelectObjectById(quadro->m_id);
        auto* sharp_dialog=open();
        require(!sharp_dialog->findChild<QCheckBox*>("LowPolyTrimByPline"), "Experimental TrimByPline remains in UI");
        auto* sharp = sharp_dialog->findChild<QCheckBox*>("LowPolyAddSharpEdges");
        auto* angle = sharp_dialog->findChild<QDoubleSpinBox*>("LowPolySharpAngle");
        require(sharp && angle && !sharp->isChecked() && angle->isHidden(), "Sharp controls initial state");
        auto* sharp_preview=quadro->GetSurfaceFace(0)->pMesh3D;
        const bool trim_mode = quadro->MeshQuadroTrimByPline;
        sharp->click();
        require(!angle->isHidden() && angle->value()==30.0, "Angle must appear with default 30 degrees");
        require(quadro->GetSurfaceFace(0)->pMesh3D==sharp_preview && quadro->MeshQuadroTrimByPline==trim_mode,
            "Sharp setting rebuilt the mesh or changed the retained trim mode");
        for(auto* button:sharp_dialog->findChildren<QPushButton*>())
            if(button->text()=="Create Low Poly")button->click();
        auto* sharp_mesh=dynamic_cast<CMesh3D*>(doc.GetObjects().back().get());
        require(sharp_mesh && !sharp_mesh->GetSharpEdges().empty(), "Automatic box sharp edges missing");
        require(quadro->GetSurfaceFace(0)->pMesh3D==sharp_preview,"Sharp assignment rebuilt accepted preview");
        angle->setValue(100.0);
        for(auto* button:sharp_dialog->findChildren<QPushButton*>())
            if(button->text()=="Create Low Poly")button->click();
        auto* smooth_mesh=dynamic_cast<CMesh3D*>(doc.GetObjects().back().get());
        require(smooth_mesh && smooth_mesh->GetSharpEdges().empty(), "Angle threshold ignored");
        sharp->click(); require(angle->isHidden(), "Angle remains visible when disabled");
        close(sharp_dialog);
        doc.SelectObjectById(hybrid->m_id);
        auto* dialog = open();
        window.ShowLowPolyTool();
        require(window.findChildren<QDialog*>("SolidLowPolyDialog").size()==1,
                "Reopening Low Poly created a second dialog");
        bool busy_checked=false;
        QTimer::singleShot(0,dialog,[&]() {
            require(dialog->property("lowPolyBusy").toBool(),"Low Poly has no busy state");
            require(!dialog->findChild<QDoubleSpinBox*>()->isEnabled(),"Density remains editable during meshing");
            window.ShowLowPolyTool();
            require(window.findChildren<QDialog*>("SolidLowPolyDialog").size()==1,"Busy Low Poly duplicated");
            dialog->close();
            require(dialog->isVisible(),"Busy Low Poly closed during calculation");
            busy_checked=true;
        });
        mode(dialog, "Mesh Quadro")->click();
        require(busy_checked && !dialog->property("lowPolyBusy").toBool(),"Busy UI was not serviced or restored");
        require(hybrid->MeshQuadro, "Enabling Quadro did not rebuild body in Quadro mode");
        close(dialog);
        dialog = open();
        require(mode(dialog, "Mesh Quadro")->isChecked(), "Reopening lost Quadro mode");
        mode(dialog, "Mesh Quadro")->click();
        require(!hybrid->MeshQuadro, "Disabling Quadro did not restore hybrid mode");
        auto* ready_surface=hybrid->GetSurfaceFace(0);
        const auto object_count=doc.GetObjects().size();
        for (auto* button:dialog->findChildren<QPushButton*>())
            if(button->text()=="Create Low Poly") button->click();
        require(doc.GetObjects().size()==object_count+1 && hybrid->GetSurfaceFace(0)==ready_surface,
                "Create Low Poly rebuilt an already valid preview");
        close(dialog);
        doc.SelectObjectById(hybrid->m_id);
        dialog=open();
        bool stopped=false;
        QTimer::singleShot(0,dialog,[&]() {
            auto* progress=dialog->findChild<QProgressDialog*>("LowPolyMeshProgress");
            require(progress,"Cancelable progress missing");
            auto* bar=progress->findChild<QProgressBar*>();
            require(bar,"Surface progress bar missing");
            QObject::connect(bar,&QProgressBar::valueChanged,progress,[&,progress](int value) {
                if(value>0 && !stopped) {
                    require(progress->maximum()==hybrid->GetNumSurfaces(),"Wrong total surface count");
                    auto* stop=progress->findChild<QPushButton*>();
                    require(stop && stop->isEnabled(),"Stop button is disabled");
                    stopped=true; stop->click();
                }
            });
        });
        mode(dialog,"Mesh Quadro")->click();
        require(stopped && !hybrid->MeshQuadro && hybrid->GetSurfaceFace(0)==ready_surface,
                "Stopping meshing did not restore the exact previous preview");
        require(!dialog->property("lowPolyBusy").toBool(),"Stopped dialog remains busy");
        close(dialog);
        doc.SelectObjectById(hybrid->m_id);
        doc.SelectObjectById(quadro->m_id, SelectionAction::Add);
        dialog = open();
        require(mode(dialog, "Mesh Quadro")->checkState() == Qt::PartiallyChecked
                && mode(dialog, "Mesh Quadro Hole SLX")->checkState() == Qt::PartiallyChecked,
                "Different body modes must display mixed state");
        dialog->findChild<QDoubleSpinBox*>()->setValue(0.6);
        require(!hybrid->MeshQuadro && quadro->MeshQuadro
                && !hybrid->MeshQuadroHoleSLX && quadro->MeshQuadroHoleSLX,
                "Density change overwrote mixed body modes");
        mode(dialog, "Mesh Quadro Hole SLX")->click();
        require(hybrid->MeshQuadro && quadro->MeshQuadro
                && mode(dialog, "Mesh Quadro")->checkState()==Qt::Checked
                && !mode(dialog, "Mesh Quadro")->isTristate(),
                "SLX did not enable Quadro for all mixed bodies and its checkbox");
        require(hybrid->MeshQuadroHoleSLX && quadro->MeshQuadroHoleSLX,
                "Choosing SLX from mixed state did not apply to all bodies");
        const auto* previous_hybrid=hybrid->GetSurfaceFace(0);
        const auto* previous_quadro=quadro->GetSurfaceFace(0);
        const float previous_density=hybrid->ptchDensity;
        bool second_body_stopped=false;
        QTimer::singleShot(0,dialog,[&]() {
            auto* progress=dialog->findChild<QProgressDialog*>("LowPolyMeshProgress");
            require(progress,"Multi-body progress missing");
            QObject::connect(progress->findChild<QProgressBar*>(),&QProgressBar::valueChanged,progress,
                [&,progress](int value) {
                    if(value>hybrid->GetNumSurfaces() && !second_body_stopped) {
                        require(progress->maximum()==hybrid->GetNumSurfaces()+quadro->GetNumSurfaces(),"Multi-body total incorrect");
                        second_body_stopped=true; progress->cancel();
                    }
                });
        });
        dialog->findChild<QDoubleSpinBox*>()->setValue(.65);
        require(second_body_stopped && hybrid->GetSurfaceFace(0)==previous_hybrid
            && quadro->GetSurfaceFace(0)==previous_quadro && hybrid->ptchDensity==previous_density,
            "Cancellation did not roll back every selected body");
        close(dialog);
        auto* slx_body=add(false);
        doc.SelectObjectById(slx_body->m_id);
        dialog=open();
        require(!mode(dialog,"Mesh Quadro")->isChecked(),"SLX test must start in hybrid mode");
        mode(dialog,"Mesh Quadro Hole SLX")->click();
        require(slx_body->MeshQuadro && slx_body->MeshQuadroHoleSLX
                && mode(dialog,"Mesh Quadro")->isChecked(),
                "SLX must enable both the Quadro checkbox and body mode");
        mode(dialog,"Mesh Quadro Hole SLX")->click();
        require(slx_body->MeshQuadro && !slx_body->MeshQuadroHoleSLX
                && mode(dialog,"Mesh Quadro")->isChecked(),
                "Disabling SLX must preserve ordinary Quadro mode");
        close(dialog);
        const int large_file=application.arguments().indexOf("--low-poly-large-file");
        if (large_file>=0 && large_file+1<application.arguments().size()) {
            doc.GetObjects().clear();
            Dom3DProjectSerializer serializer; ProjectViewState view; QString room,error;
            require(serializer.Load(application.arguments().at(large_file+1),doc,room,view,error),"Cannot load large Low Poly fixture");
            CSolid* body=nullptr;
            for (const auto& object:doc.GetObjects())
                if (auto* candidate=dynamic_cast<CSolid*>(object.get())) { body=candidate; break; }
            require(body,"Large Low Poly fixture has no solid");
            const bool cylinder_step=qEnvironmentVariableIsSet("DOM3D_CYLINDER_STEP");
            body->MeshQuadroHoleDivideFace=false;
            if (cylinder_step) { body->MeshQuadro=true; body->MeshQuadroHoleSLX=true; }
            doc.SelectObjectById(body->m_id);
            dialog=open();
            int heartbeats=0;
            QElapsedTimer elapsed; elapsed.start();
            qint64 last_heartbeat=0, maximum_pause=0;
            QTimer heartbeat;
            QObject::connect(&heartbeat,&QTimer::timeout,dialog,[&]() {
                maximum_pause=std::max(maximum_pause,elapsed.elapsed()-last_heartbeat);
                last_heartbeat=elapsed.elapsed();
                ++heartbeats;
                require(dialog->isVisible(),"Large Low Poly dialog disappeared");
                window.ShowLowPolyTool();
                require(window.findChildren<QDialog*>("SolidLowPolyDialog").size()==1,"Large Low Poly duplicate dialog");
            });
            heartbeat.start(100);
            if (cylinder_step) {
                for (double density : {.5,.6,.4,.6})
                    dialog->findChild<QDoubleSpinBox*>()->setValue(density);
            } else mode(dialog,"Mesh Quadro Hole DivideFace")->click();
            heartbeat.stop();
            maximum_pause=std::max(maximum_pause,elapsed.elapsed()-last_heartbeat);
            const QString dump=qEnvironmentVariable("DOM3D_LOW_POLY_DUMP");
            if (!dump.isEmpty()) {
                QDir().mkpath(dump);
                QFile info(dump+"/surfaces.txt"); info.open(QIODevice::WriteOnly|QIODevice::Text);
                QTextStream meta(&info);
                for (int i=0;i<body->GetNumSurfaces();++i) {
                    auto* surface=body->GetSurfaceFace(i);
                    if(surface && surface->pMesh3D) {
                        surface->pMesh3D->ExportToObj((dump+QString("/surface_%1.obj").arg(i)).toStdString());
                        if (i==13 || i==35)
                            BRepTools::Write(surface->m_Face,(dump+QString("/surface_%1.brep").arg(i)).toStdString().c_str());
                        GProp_GProps props; BRepGProp::SurfaceProperties(surface->m_Face,props);
                        Vec3 center,normal; surface->GetCenterAndNormal(center,normal);
                        meta<<i<<' '<<int(BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType())
                            <<' '<<surface->m_TypeMesh<<' '<<surface->IsTrimmed<<' '<<props.Mass()
                            <<' '<<normal.x<<' '<<normal.y<<' '<<normal.z
                            <<' '<<surface->m_QtyU<<' '<<surface->m_QtyV<<'\n';
                    }
                }
            }
            require(heartbeats>1,"Long calculation did not service UI events");
            require(maximum_pause<5000,"Low Poly left the UI unserviced for five seconds");
            require(dialog->isVisible() && !dialog->property("lowPolyBusy").toBool(),"Large Low Poly did not restore its dialog");
            std::cout << "Large Low Poly: " << elapsed.elapsed() << " ms, UI heartbeats=" << heartbeats
                      << ", maximum pause=" << maximum_pause << " ms\n";
            close(dialog);
        }
        std::cout << "Low Poly existing mesh, reopen, toggles and mixed selection passed\n";
        return 0;
    }
    if (application.arguments().contains("--rgb-opacity-only")) {
        QTemporaryDir settingsDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
        CAlfaDoc doc; Dom3DProjectSerializer serializer; ProjectViewState view; QString room,error;
        require(serializer.Load(application.arguments().at(3),doc,room,view,error), "Cannot load RGB opacity fixture");
        OpenGLViewport viewport; viewport.SetDocument(&doc); viewport.resize(900,700);
        viewport.SetCamera(view.camera); viewport.SetOrthographicProjection(view.orthographic_projection);
        CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        CSolid::SetHiddenEdgeDrawingEnabled(false);
        CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceColored);
        viewport.show(); application.processEvents();
        QDir().mkpath("output/rgb-opacity");
        for(bool selected : {false,true}) {
            doc.ClearSelection();if(selected)doc.SelectAllVisibleObjects();
            CMesh3D::SetSurfaceOpacity(1.f);CSolid::SetSurfaceTransparencyEnabled(false);
            const auto baseline=viewport.CaptureSceneImage({900,700});
            require(!baseline.isNull(),"RGB OpenGL capture failed");
            baseline.save(selected?"output/rgb-opacity/selected.png":"output/rgb-opacity/plain.png");
            for(float opacity : {.91f,.5f}) {
                CMesh3D::SetSurfaceOpacity(opacity);
                require(viewport.CaptureSceneImage({900,700})==baseline,
                        "RGB normals are corrupted by surface opacity");
            }
            CSolid::SetSurfaceTransparencyEnabled(true);
            require(viewport.CaptureSceneImage({900,700})==baseline,
                    "RGB normals are corrupted by transparent-solid mode");
        }
        doc.ClearSelection();CSolid::SetSurfaceTransparencyEnabled(false);
        CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceGray);CMesh3D::SetSurfaceOpacity(1.f);
        const auto opaque=viewport.CaptureSceneImage({900,700});CMesh3D::SetSurfaceOpacity(.5f);
        require(viewport.CaptureSceneImage({900,700})!=opaque,"Ordinary shading lost transparency");
        CMesh3D::SetSurfaceOpacity(1.f);viewport.SetDocument(nullptr);
        std::cout<<"RGB and RGB selection ignore opacity; ordinary shading retains transparency\n";
        return 0;
    }
    if (application.arguments().contains("--sketch-joints-only")) {
        QTemporaryDir settings;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        MainWindow window;
        auto& doc = window.document_;
        auto* viewport = window.viewport_;
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room,error;
        const int argument = application.arguments().indexOf("--sketch-joints-only");
        require(serializer.Load(application.arguments().value(argument+1),doc,room,view,error),
                "Cannot load SL_Extrude joint fixture");
        auto* sketch = dynamic_cast<CSmartLine*>(doc.FindObjectById(3));
        require(sketch && sketch->GetNodeCount()==4, "Missing four-node Bezier sketch");
        auto* original_body = dynamic_cast<CSolid*>(doc.FindObjectById(82));
        require(original_body != nullptr, "Missing dependent extrusion");
        const TopoDS_Shape original_shape = original_body->m_Shape;
        doc.SelectObjectById(sketch->m_id);
        viewport->resize(800,600); viewport->SetOrthographicProjection(true); viewport->SetXYView();
        auto camera=viewport->GetCamera(); camera.target={220,180,0}; camera.distance=1200; viewport->SetCamera(camera);
        require(viewport->BeginEditSelectedSketch(),"Cannot start sketch editing");
        window.ShowSketchPanel();
        auto* smooth=window.findChild<QPushButton*>("SketchConstraintButton2");
        auto* sharp=window.findChild<QPushButton*>("SketchConstraintButton3");
        require(smooth && sharp,"Joint buttons missing");
        const auto click_world=[&](CPoint3d p) {
            DomPoint screen{}; QtSceneRenderer renderer;
            require(renderer.WorldToScreen({float(p.x),float(p.y),float(p.z)},viewport->GetCamera(),true,
                    viewport->width(),viewport->height(),screen),"Cannot project joint");
            const QPointF position(screen.x,screen.y);
            QMouseEvent event(QEvent::MouseButtonPress,position,position,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(viewport,&event);
        };
        const auto is_smooth=[](const CSmartLine& s,size_t first,size_t second) {
            const auto a=s.GetLine(first)->GetTangent(1),b=s.GetLine(second)->GetTangent(0);
            const double magnitude=std::hypot(a.x,a.y)*std::hypot(b.x,b.y);
            return magnitude>1.e-12 && std::abs(a.x*b.y-a.y*b.x)/magnitude<1.e-10 && a.x*b.x+a.y*b.y>0;
        };
        require(!is_smooth(*sketch,2,3) && !is_smooth(*sketch,3,0), "Original left joints are unexpectedly smooth");
        window.undo_redo_.Reset();
        smooth->click();
        require(viewport->CurrentTool()==ToolMode::SketchSmoothJoint,"Left button does not select smooth-node tool");
        click_world(sketch->LocalToWorld(sketch->GetLine(2)->GetPoint(0.5)));
        require(sketch->GetNumConstraints()==0 && window.undo_redo_.UndoCount()==0,
                "Joint tool selected a segment interior instead of a node");
        click_world(sketch->GetNodeWorld(2));
        require(sketch->GetNumConstraints()==1 && is_smooth(*sketch,2,3),"Left node click did not smooth both segments");
        require(window.undo_redo_.Undo() && !is_smooth(*sketch,2,3),"Smooth joint Undo failed");
        require(window.undo_redo_.Redo() && is_smooth(*sketch,2,3),"Smooth joint Redo failed");
        smooth->click(); click_world(sketch->GetNodeWorld(3));
        require(sketch->GetNumConstraints()==2 && is_smooth(*sketch,3,0),"Closing left node click failed");
        sharp->click();
        require(viewport->CurrentTool()==ToolMode::SketchSharpJoint,"Right button does not remove smoothness");
        click_world(sketch->GetNodeWorld(2));
        require(sketch->GetNumConstraints()==1 && is_smooth(*sketch,2,3),
                "Sharp tool must release only the picked joint without moving geometry");
        require(window.undo_redo_.Undo() && sketch->GetNumConstraints()==2,"Sharp joint Undo failed");
        auto* rebuilt_body = dynamic_cast<CSolid*>(doc.FindObjectById(82));
        require(rebuilt_body && !rebuilt_body->m_Shape.IsSame(original_shape)
                && BRepCheck_Analyzer(rebuilt_body->m_Shape).IsValid(),
                "Smooth joint changes did not rebuild a valid dependent extrusion");
        CAlfaDoc::LiveFilletBuildRequest fillet_request;
        fillet_request.source_shape = fillet_request.base_shape = rebuilt_body->m_Shape;
        double cap_height = -1.e100;
        for (TopExp_Explorer faces(rebuilt_body->m_Shape,TopAbs_FACE); faces.More(); faces.Next()) {
            const auto face = TopoDS::Face(faces.Current());
            BRepAdaptor_Surface surface(face);
            if (surface.GetType()!=GeomAbs_Plane || surface.Plane().Location().Z()<=cap_height) continue;
            cap_height = surface.Plane().Location().Z();
            fillet_request.edges.clear();
            for (TopExp_Explorer edges(face,TopAbs_EDGE); edges.More(); edges.Next())
                fillet_request.edges.push_back(TopoDS::Edge(edges.Current()));
        }
        require(fillet_request.edges.size()==4,"Missing tapered upper contour");
        for (double radius : {1.0,2.0}) {
            TopoDS_Shape filleted;
            std::vector<int> generated;
            require(CAlfaDoc::BuildLiveFilletShape(fillet_request,{radius},filleted,generated)
                    && !generated.empty() && BRepCheck_Analyzer(filleted).IsValid(),
                    "Smoothed sketch taper does not support a valid R1/R2 contour fillet");
        }
        const QString saved=settings.filePath("smooth.dom3d");
        require(serializer.Save(saved,doc,room,view,{},error),"Cannot save smooth joints");
        CAlfaDoc loaded;
        require(serializer.Load(saved,loaded,room,view,error),"Cannot reload smooth joints");
        auto* restored=dynamic_cast<CSmartLine*>(loaded.FindObjectById(3));
        require(restored && restored->GetNumConstraints()==2 && is_smooth(*restored,2,3) && is_smooth(*restored,3,0),
                "Saved project lost smooth joint constraints");
        require(restored->MoveBezierControlPointWorld(6,restored->LocalToWorld({-300,-10,0})) && is_smooth(*restored,2,3),
                "Reloaded joint did not retain bidirectional control");
        const QString output=qEnvironmentVariable("DOM3D_SMOOTH_SKETCH_OUTPUT");
        if(!output.isEmpty()) require(serializer.Save(output,doc,room,view,{},error),"Cannot save corrected sketch project");
        std::cout<<"SL_Extrude node picking, smooth/sharp, Undo/Redo and persistence passed\n";
        return 0;
    }
    if (application.arguments().contains("--extrude-contour-fillet")
        || application.arguments().contains("--prism-hollow-fillet-ui")) {
        const bool hollow_prism = application.arguments().contains("--prism-hollow-fillet-ui");
        QTemporaryDir settings_dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
        const int argument = application.arguments().indexOf(
            hollow_prism ? "--prism-hollow-fillet-ui" : "--extrude-contour-fillet");
        MainWindow window;
        auto& doc = window.document_;
        Dom3DProjectSerializer serializer;
        ProjectViewState view;
        QString room, error;
        require(serializer.Load(application.arguments().value(argument + 1), doc, room, view, error),
                "Cannot load extruded contour fixture");
        CSolid* body = nullptr;
        for (const auto& object : doc.GetObjects())
            if (auto* solid = dynamic_cast<CSolid*>(object.get())) body = solid;
        require(body != nullptr, "Missing extruded body");
        require(body->ReBuldMesh(), "Cannot initialize extruded body display edges");
        const auto id = body->m_id;
        int top = -1;
        double height = -1.e100;
        for (int face = 0; face < body->GetNumSurfaces(); ++face) {
            BRepAdaptor_Surface surface(TopoDS::Face(body->GetSurfaceFace(face)->m_Face));
            if (surface.GetType() == GeomAbs_Plane && surface.Plane().Location().Z() > height) {
                height = surface.Plane().Location().Z(); top = face;
            }
        }
        require(top >= 0, "Missing top planar face");
        window.viewport_->SetSelectionMode(SelectionMode::Face);
        doc.SelectObjectById(id);
        body->SetSelectedFace(top);
        std::cout << "Top face=" << top << " height=" << height << " edges=" << body->GetSurfaceFace(top)->GetEdgeCount()
                  << " selected=" << (doc.GetSelectedSolid() == body) << " face=" << body->HasSelectedFace() << std::endl;
        window.last_fillet_radius_ = 1.0;
        window.ActivateParametricTool("fillet_edge");
        std::cout << "Live refs=" << doc.GetLiveFilletEdgeRefs().size() << " status=" << window.statusBar()->currentMessage().toStdString() << std::endl;
        require(doc.GetLiveFilletEdgeRefs().size() == (hollow_prism ? 18 : 4),
                "Top face did not select all contour edges");
        QElapsedTimer fillet_wait; fillet_wait.start();
        while ((window.live_fillet_build_pending_ || window.live_fillet_build_running_)
               && fillet_wait.elapsed() < 30000)
            application.processEvents(QEventLoop::AllEvents, 50);
        require(doc.IsLiveFilletPreviewValid(), "Top contour fillet preview failed");
        window.AcceptActiveProperties();
        require(window.active_parametric_object_.tool_id.empty(), "Top contour fillet did not commit");
        body = doc.GetSelectedSolid();
        require(body && BRepCheck_Analyzer(body->m_Shape).IsValid(), "Committed contour is invalid");
        if (hollow_prism) {
            auto active = window.tool_registry_.ActiveObjectFromDocument(
                doc.GetSelectedObjectIndex(), *body, body->GetNumOperations() - 1, &doc);
            require(active.tool_id == "fillet_edge", "Hollow prism lost fillet history");
            window.tool_registry_.Rebuild(active, doc);
            body = doc.GetSelectedSolid();
            require(body && BRepCheck_Analyzer(body->m_Shape).IsValid(),
                    "Hollow prism fillet history rebuilt an invalid shell");
        }
        const QString output = application.arguments().value(argument + 2);
        if (!output.isEmpty()) require(serializer.Save(output, doc, room, view, {}, error), "Cannot save filleted project");
        require(window.undo_redo_.Undo(), "Cannot undo contour fillet");
        require(window.undo_redo_.Redo(), "Cannot redo contour fillet");
        std::cout << "Extruded top contour R1 fillet, commit, undo and redo passed\n";
        return 0;
    }
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
        // Use a real visible XY grid node in 3D as well as in the XY view.
        // The former XZ snap plane could pass the helper-only check above.
        document.GetObjects().clear();
        camera.target={0,0,0}; camera.distance=300;
        viewport.SetCamera(camera);
        viewport.grid_step_=100; viewport.grid_subdivisions_=4;
        for(int i=0;i<int(Target::Count);++i)
            viewport.SetSnapTargetEnabled(static_cast<Target>(i),false);
        viewport.SetSnapTargetEnabled(Target::Grid,true);
        const Vec3 node{50,25,0};
        const auto atNode=[&](CPoint3d p) {
            const Vec3 delta=vec(p)-node; return dot(delta,delta)<1.e-8f;
        };
        for(bool ortho:{true,false}) for(bool xy:{false,true}) {
            if(xy && !ortho) continue; // The XY view always uses orthographic projection.
            viewport.xy_plane_view_enabled_=xy;
            viewport.SetOrthographicProjection(ortho);
            DomPoint screen{};
            require(viewport.renderer_.WorldToScreen(node,camera,ortho,1000,700,screen),
                    "Cannot project visible grid node");
            const QPoint pixel(screen.x+2,screen.y+2);
            for(Kind kind:{Kind::Polyline,Kind::BSpline,Kind::Bezier,Kind::Nurbs}) {
                viewport.SetTool(kind==Kind::Polyline?ToolMode::DrawCurve:ToolMode::DrawBSpline);
                viewport.BeginSpatialCurvePreview(kind);
                CPoint3d p;
                require(viewport.PickModelingPoint(pixel,p) && atNode(p),
                        "First curve point did not snap to visible XY grid");
                viewport.SetSpatialCurvePreviewPoints({CPoint3d(-50,-25,0)});
                require(viewport.PickModelingPoint(pixel,p) && atNode(p),
                        "Following curve point did not snap to visible XY grid");
                viewport.SetSnapTargetEnabled(Target::Grid,false);
                require(viewport.PickModelingPoint(pixel,p) && !atNode(p),
                        "Disabled grid snap still captures curve points");
                viewport.SetSnapTargetEnabled(Target::Grid,true);
                viewport.snapping_enabled_=false;
                require(viewport.PickModelingPoint(pixel,p) && !atNode(p),
                        "Disabled global snapping still captures curve points");
                viewport.snapping_enabled_=true;
                for(bool constant_x:{true,false}) {
                    const Vec3 line_point=constant_x ? Vec3{50,12.5f,0} : Vec3{37.5f,25,0};
                    DomPoint line_screen{};
                    require(viewport.renderer_.WorldToScreen(line_point,camera,ortho,1000,700,line_screen),
                            "Cannot project grid line midpoint");
                    const QPoint line_pixel(line_screen.x+1,line_screen.y+1);
                    require(viewport.PickModelingPoint(line_pixel,p)
                            && std::abs(p.z)<1.e-6
                            && std::abs((constant_x?p.x:p.y)-(constant_x?50:25))<1.e-6
                            && std::abs((constant_x?p.y:p.x)-(constant_x?12.5:37.5))<2,
                            "Curve point did not snap along grid line between nodes");
                    viewport.SetSnapTargetEnabled(Target::Grid,false);
                    require(!viewport.SnapCreationPoint(line_pixel,p,false),
                            "Disabled grid still captures its lines");
                    viewport.SetSnapTargetEnabled(Target::Grid,true);
                    viewport.SetFloorGridVisible(false);
                    require(!viewport.SnapCreationPoint(line_pixel,p,false),
                            "Hidden grid still captures its lines");
                    viewport.SetFloorGridVisible(true);
                }
                viewport.EndSpatialCurvePreview();
            }
            viewport.SetTool(ToolMode::DrawSpline);
            viewport.BeginDrawSplineStroke(pixel);
            require(!viewport.draw_spline_raw_points_.empty()
                    && atNode(viewport.draw_spline_raw_points_.front()),
                    "Pencil stroke did not snap to visible grid");
            viewport.CancelDrawSplineStroke();
        }
        viewport.xy_plane_view_enabled_=false;
        viewport.SetSnapTargetEnabled(Target::Grid,false);
        viewport.SetSnapTargetEnabled(Target::AuxLine,true);
        viewport.SetWorkPlane({0,0,0},{0,0,1});
        for(bool ortho:{true,false}) {
            viewport.SetOrthographicProjection(ortho);
            viewport.SetTool(ToolMode::DrawCurve);
            viewport.BeginSpatialCurvePreview(Kind::Polyline);
            viewport.SetSpatialCurvePreviewPoints({{3,7,0},{80,7,0},{80,60,0}});
            for(bool vertical:{true,false}) {
                const Vec3 target=vertical ? Vec3{3,-40,0} : Vec3{-40,7,0};
                DomPoint screen{};
                require(viewport.renderer_.WorldToScreen(target,camera,ortho,1000,700,screen),
                        "Cannot project first-point auxiliary guide");
                const QPoint pixel(screen.x+1,screen.y+1);
                CPoint3d p;
                require(viewport.SnapCreationPoint(pixel,p,false)
                        && std::abs(vertical?p.x-3:p.y-7)<1.e-4
                        && std::abs(p.z)<1.e-4,
                        "Aux Line did not capture alignment with the first polyline point");
                require(viewport.auxiliary_guide_visible_
                        && std::abs(viewport.auxiliary_guide_origin_.x-3)<1.e-4
                        && std::abs(viewport.auxiliary_guide_origin_.y-7)<1.e-4,
                        "Captured auxiliary guide is not available for rendering");
                viewport.SetSnapTargetEnabled(Target::AuxLine,false);
                require(!viewport.SnapCreationPoint(pixel,p,false) && !viewport.auxiliary_guide_visible_,
                        "Disabled auxiliary guide remains active");
                viewport.SetSnapTargetEnabled(Target::AuxLine,true);
            }
            viewport.EndSpatialCurvePreview();
            require(!viewport.auxiliary_guide_visible_,"Completed curve retained an auxiliary guide");
            viewport.BeginSpatialCurvePreview(Kind::Polyline);
            viewport.SetSpatialCurvePreviewPoints({{3,7,0},{80,7,0},{80,60,0}});
            DomPoint previous_screen{};
            require(viewport.renderer_.WorldToScreen({80,-40,0},camera,ortho,1000,700,previous_screen),
                    "Cannot project previous-point alignment");
            CPoint3d previous_snap;
            require(viewport.SnapCreationPoint(QPoint(previous_screen.x+1,previous_screen.y+1),previous_snap,false)
                    && std::abs(previous_snap.x-80)<1.e-4,
                    "Previous-point alignment no longer snaps");
            require(!viewport.auxiliary_guide_visible_,"Previous point must not display a dashed guide");
            viewport.EndSpatialCurvePreview();
        }
        // A joined spline's second pole follows the source endpoint tangent.
        for(auto type:{SplineCurveType::BSpline,SplineCurveType::Bezier,SplineCurveType::Nurbs}) {
            auto source=std::make_unique<CBSpline>("Tangent source");
            source->SetCurveType(type); source->SetDegree(3); // Effective degree is 2 for three poles.
            source->AddPoint({-60,-20,0}); source->AddPoint({-17,-13,0}); source->AddPoint({3,7,0});
            if(type==SplineCurveType::Nurbs) source->SetWeights({1,2,1});
            auto* source_ptr=source.get(); document.AddObject(std::move(source));
            for(bool ortho:{true,false}) for(bool at_start:{true,false}) {
                viewport.SetOrthographicProjection(ortho);
                viewport.BeginSpatialCurvePreview(Kind::BSpline);
                const CPoint3d joint=at_start?CPoint3d(-60,-20,0):CPoint3d(3,7,0);
                const Vec3 direction=normalize(at_start?Vec3{-43,-7,0}:Vec3{20,20,0});
                viewport.SetSpatialCurvePreviewPoints({joint});
                DomPoint screen{};
                require(viewport.renderer_.WorldToScreen(vec(joint)+direction*40,camera,ortho,1000,700,screen),
                        "Cannot project tangent guide");
                const QPoint pixel(screen.x+1,screen.y+1);
                CPoint3d snapped;
                require(viewport.SnapCreationPoint(pixel,snapped,false) && viewport.tangent_guide_visible_,
                        "Joined spline did not capture its endpoint tangent");
                const Vec3 delta=vec(snapped)-vec(joint);
                const Vec3 perpendicular=delta-direction*dot(delta,direction);
                require(dot(perpendicular,perpendicular)<1.e-6f && dot(delta,direction)>0,
                        "Second pole does not give a smooth outward continuation");
                viewport.SetSpatialCurvePreviewPoints({joint,snapped});
                viewport.SnapCreationPoint(pixel,snapped,false);
                require(!viewport.tangent_guide_visible_,"Tangent guide remained active after the second pole");
                viewport.SetSpatialCurvePreviewPoints({joint});
                source_ptr->SetVisible(false);
                viewport.SnapCreationPoint(pixel,snapped,false);
                require(!viewport.tangent_guide_visible_,"Hidden spline supplied a tangent guide");
                source_ptr->SetVisible(true);
                viewport.SetSnapTargetEnabled(Target::AuxLine,false);
                viewport.SnapCreationPoint(pixel,snapped,false);
                require(!viewport.tangent_guide_visible_,"Disabled Aux Line still supplied a tangent");
                viewport.SetSnapTargetEnabled(Target::AuxLine,true);
                viewport.EndSpatialCurvePreview();
            }
            document.GetObjects().clear();
        }
        {
            auto source=std::make_unique<CBSpline>("Dwell tangent");
            source->SetDegree(2);
            source->AddPoint({-60,-20,0}); source->AddPoint({-17,-13,0}); source->AddPoint({3,7,0});
            document.AddObject(std::move(source));
            const auto id=document.GetSelectedObject()->m_id;
            viewport.SetOrthographicProjection(true);
            viewport.BeginSpatialCurvePreview(Kind::BSpline);
            viewport.SetSpatialCurvePreviewPoints({{-90,90,0},{-50,80,0}});
            DomPoint end_pixel{},start_pixel{},tangent_pixel{};
            require(viewport.renderer_.WorldToScreen({3,7,0},camera,true,1000,700,end_pixel)
                    && viewport.renderer_.WorldToScreen({-60,-20,0},camera,true,1000,700,start_pixel)
                    && viewport.renderer_.WorldToScreen({33,37,0},camera,true,1000,700,tangent_pixel),"Dwell projection failed");
            const auto wait_ms=[&](int ms) {
                QEventLoop loop; QTimer::singleShot(ms,Qt::PreciseTimer,&loop,&QEventLoop::quit); loop.exec();
            };
            viewport.UpdateTangentHover(QPoint(end_pixel.x,end_pixel.y),true);
            wait_ms(300);
            require(!viewport.active_tangent_id_,"Tangent activated before 0.5 seconds");
            viewport.UpdateTangentHover(QPoint(0,0),true);
            wait_ms(250);
            require(!viewport.active_tangent_id_,"Passing over endpoint activated a tangent");
            viewport.UpdateTangentHover(QPoint(end_pixel.x,end_pixel.y),true);
            wait_ms(550);
            require(viewport.active_tangent_id_==id && !viewport.active_tangent_start_,"Dwell failed to activate endpoint");
            viewport.UpdateTangentHover(QPoint(tangent_pixel.x,tangent_pixel.y),true);
            CPoint3d snapped;
            require(viewport.SnapCreationPoint(QPoint(tangent_pixel.x,tangent_pixel.y),snapped,false)
                    && viewport.tangent_guide_visible_ && std::abs((snapped.x-3)-(snapped.y-7))<1.e-4,
                    "Activated endpoint tangent did not remain available away from node");
            viewport.UpdateTangentHover(QPoint(start_pixel.x,start_pixel.y),true);
            wait_ms(550);
            require(viewport.active_tangent_id_==id && viewport.active_tangent_start_,"Dwell did not switch endpoints");
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&escape);
            require(!viewport.active_tangent_id_ && !viewport.pending_tangent_id_,"Escape retained tangent activation");
            viewport.EndSpatialCurvePreview();
            document.GetObjects().clear();
        }
        if (application.arguments().contains("--render-aux-guide")) {
            viewport.SetTool(ToolMode::DrawCurve);
            viewport.SetOrthographicProjection(true);
            viewport.SetFloorGridVisible(false);
            viewport.show_coordinate_axes_=false;
            viewport.show();
            application.processEvents();
            viewport.BeginSpatialCurvePreview(Kind::Polyline);
            viewport.SetSpatialCurvePreviewPoints({{3,7,0},{80,7,0},{80,60,0}});
            viewport.BeginPick3DPoint("Guide render regression");
            DomPoint screen{};
            require(viewport.renderer_.WorldToScreen({3,-40,0},camera,true,1000,700,screen),"Guide projection failed");
            const QPoint pixel(screen.x+1,screen.y+1);
            QMouseEvent hover(QEvent::MouseMove,pixel,pixel,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&hover);
            require(viewport.auxiliary_guide_visible_ && viewport.creation_snap_active_,"Hover lost guide state");
            const QImage with_guide=viewport.grabFramebuffer();
            viewport.auxiliary_guide_visible_=false;
            const QImage without_guide=viewport.grabFramebuffer();
            require(!with_guide.isNull() && with_guide.size()==without_guide.size(),"No OpenGL render for guide check");
            int changed=0;
            for(int y=0;y<with_guide.height();++y) for(int x=0;x<with_guide.width();++x) {
                const QColor a=with_guide.pixelColor(x,y), b=without_guide.pixelColor(x,y);
                if(a.red()>b.red()+30 && a.green()>b.green()+30 && a.blue()>b.blue()+30) ++changed;
            }
            with_guide.save("output/aux-guide-render.png");
            require(changed>150,"Auxiliary guide state is active but its dashed line is not rendered");
            auto tangent_source=std::make_unique<CBSpline>("Rendered tangent");
            tangent_source->SetDegree(2);
            tangent_source->AddPoint({-60,-20,0}); tangent_source->AddPoint({-17,-13,0}); tangent_source->AddPoint({3,7,0});
            document.AddObject(std::move(tangent_source));
            viewport.SetTool(ToolMode::DrawBSpline);
            viewport.BeginSpatialCurvePreview(Kind::BSpline);
            viewport.SetSpatialCurvePreviewPoints({{3,7,0}});
            viewport.BeginPick3DPoint("Tangent guide render regression");
            require(viewport.renderer_.WorldToScreen({33,37,0},camera,true,1000,700,screen),"Tangent projection failed");
            const QPoint tangent_pixel(screen.x+1,screen.y+1);
            QMouseEvent tangent_hover(QEvent::MouseMove,tangent_pixel,tangent_pixel,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&tangent_hover);
            require(viewport.tangent_guide_visible_,"Hover lost tangent guide state");
            const QImage tangent_image=viewport.grabFramebuffer();
            int purple=0;
            for(int y=0;y<tangent_image.height();++y) for(int x=0;x<tangent_image.width();++x) {
                const QColor color=tangent_image.pixelColor(x,y);
                if(color.red()>80 && color.blue()>100 && color.blue()>color.red()*.9
                    && color.green()<color.red()*.8) ++purple;
            }
            require(purple>150,"Purple tangent guide is not rendered");
            tangent_image.save("output/tangent-guide-render.png");
            viewport.BeginSpatialCurvePreview(Kind::BSpline);
            viewport.SetSpatialCurvePreviewPoints({{-90,90,0},{-50,80,0}});
            require(viewport.renderer_.WorldToScreen({3,7,0},camera,true,1000,700,screen),"Dwell render projection failed");
            viewport.UpdateTangentHover(QPoint(screen.x,screen.y),true);
            QEventLoop dwell_loop;
            QTimer::singleShot(550,Qt::PreciseTimer,&dwell_loop,&QEventLoop::quit);
            dwell_loop.exec();
            require(viewport.active_tangent_id_,"Visible endpoint failed dwell activation");
            viewport.UpdateTangentHover(QPoint(80,80),true);
            CPoint3d free_point;
            viewport.SnapCreationPoint(QPoint(80,80),free_point,false);
            viewport.SetCreationSnapCursor(false);
            const QImage persistent_image=viewport.grabFramebuffer();
            purple=0;
            for(int y=0;y<persistent_image.height();++y) for(int x=0;x<persistent_image.width();++x) {
                const QColor color=persistent_image.pixelColor(x,y);
                if(color.red()>80 && color.blue()>100 && color.blue()>color.red()*.9
                    && color.green()<color.red()*.8) ++purple;
            }
            persistent_image.save("output/tangent-dwell-render.png");
            require(purple>150,"Activated tangent disappeared when cursor left it");
            viewport.hide();
        }
        std::cout<<"Grid and auxiliary curve snapping checks passed.\n"; return 0;
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
    #include "FilletEdgeIdentityTestCases.inc"
    if(application.arguments().contains("--solid-extrude-gizmo")) {
        document.GetObjects().clear();
        auto defaults=window.tool_registry_.Find("SolidBox")->defaults;for(auto& p:defaults){if(p.id=="width")p.value=40;if(p.id=="height")p.value=30;if(p.id=="depth")p.value=20;}
        auto active=window.tool_registry_.CreateParametricObject("SolidBox",document,defaults);auto* source=dynamic_cast<CSolid*>(document.GetObjects().back().get());require(source,"Cannot create parametric fixture");const auto id=source->m_id;auto shape=source->m_Shape;require(source->GetNumOperations()==1,"Missing base history");window.undo_redo_.Reset();
        std::vector<int> selected;for(int i=0;i<source->GetNumSurfaces();++i){Vec3 c,n;source->GetFaceCenterAndNormal(i,c,n);if(n.z>.9||n.x>.9)selected.push_back(i);}require(selected.size()==2,"Missing adjacent faces");
        window.resize(1100,800);window.show();window.viewport_->FitToDocument();application.processEvents();
        auto open=[&]{window.viewport_->SetSelectionMode(SelectionMode::Face);document.ClearSelection();document.SelectObjectById(id);for(int i:selected)source->AddSelectedFace(i);window.ActivateParametricTool("SolidExtrudeGizmo");application.processEvents();auto* d=window.findChild<QDialog*>("HybridExtrudeDialog");require(d&&d->property("solidOnly").toBool()&&d->windowTitle()=="Solid Extrude with Gizmo","Missing solid extrusion command");return d;};
        for(bool accept:{false,true}){
            auto* d=open();d->findChild<QDoubleSpinBox*>("HybridExtrudeValue1")->setValue(8);d->findChild<QDoubleSpinBox*>("HybridExtrudeValue4")->setValue(5);d->findChild<QDoubleSpinBox*>("HybridExtrudeValue6")->setValue(.9);
            require(d->findChild<QPushButton*>("HybridExtrudeAccept")->isEnabled(),d->findChild<QLabel*>("HybridExtrudeStatus")->text().toStdString().c_str());
            auto* result=dynamic_cast<CSolid*>(document.GetObjects().back().get());require(result&&result!=source&&BRepCheck_Analyzer(result->m_Shape).IsValid(),"Invalid solid extrusion preview");int solids=0;for(TopExp_Explorer e(result->m_Shape,TopAbs_SOLID);e.More();e.Next())++solids;require(solids==1,"Extrusion is not one closed solid");require(source->m_Shape.IsSame(shape),"Extrusion mutated source");
            const int output=application.arguments().indexOf("--output");if(output>=0){application.processEvents();window.viewport_->grabFramebuffer().save(application.arguments().value(output+1));}
            if(!accept){d->reject();require(window.viewport_->GetSelectionMode()==SelectionMode::Face && !document.HasSelection(),"Gizmo Cancel changed Face mode or selected body");application.sendPostedEvents(nullptr,QEvent::DeferredDelete);if(document.GetObjects().size()!=1||!source->IsVisible())std::cerr<<"Cancel objects="<<document.GetObjects().size()<<" visible="<<source->IsVisible()<<"\n";require(document.GetObjects().size()==1&&source->IsVisible(),"Solid extrusion Cancel failed");}
            else {d->accept();require(window.viewport_->GetSelectionMode()==SelectionMode::Face && !document.HasSelection(),"Gizmo OK changed Face mode or selected body");application.sendPostedEvents(nullptr,QEvent::DeferredDelete);require(window.undo_redo_.CanUndo(),"Extrusion not recorded in Undo");window.UndoDocumentChange();require(document.GetObjects().size()==1&&document.FindObjectById(id)->IsVisible(),"Solid extrusion Undo failed");window.RedoDocumentChange();require(document.GetObjects().size()==1&&document.FindObjectById(id)->IsVisible(),"Solid extrusion Redo failed");}
        }
        auto* restored=dynamic_cast<CSolid*>(document.FindObjectById(id));require(restored&&restored->GetNumOperations()==2&&restored->GetOperation(0)->ToolId=="SolidBox"&&restored->GetOperation(1)->ToolId=="SolidExtrudeGizmo","Parametric history lost");
        GProp_GProps before;BRepGProp::VolumeProperties(restored->m_Shape,before);require(window.tool_registry_.ReplayOperations(document.FindObjectIndexById(id),document),"Gizmo operation replay failed");restored=dynamic_cast<CSolid*>(document.FindObjectById(id));GProp_GProps replayed;BRepGProp::VolumeProperties(restored->m_Shape,replayed);require(std::abs(before.Mass()-replayed.Mass())<1.e-5,"Replay changed result");
        auto edit=window.tool_registry_.ActiveObjectFromDocument(document.FindObjectIndexById(id),*restored,1,&document);for(auto& p:edit.parameters)if(p.id=="move.y")p.value=12;window.tool_registry_.Rebuild(edit,document);restored=dynamic_cast<CSolid*>(document.FindObjectById(id));GProp_GProps edited;BRepGProp::VolumeProperties(restored->m_Shape,edited);require(std::abs(edited.Mass()-replayed.Mass())>1,"Editing gizmo parameters has no effect");
        QString failure,room="Solid";ProjectViewState view;const auto file=temporary.filePath("gizmo-history.dom3d");require(window.dom3d_serializer_.Save(file,document,room,view,{},failure),"Cannot save gizmo history");CAlfaDoc loaded;require(window.dom3d_serializer_.Load(file,loaded,room,view,failure),"Cannot reload gizmo history");require(window.tool_registry_.ReplayOperations(loaded.FindObjectIndexById(id),loaded),"Reloaded gizmo history cannot replay");auto* loadedBody=dynamic_cast<CSolid*>(loaded.FindObjectById(id));GProp_GProps saved;BRepGProp::VolumeProperties(loadedBody->m_Shape,saved);require(std::abs(saved.Mass()-edited.Mass())<1.e-5,"Reload changed edited extrusion");SetAlfaDoc(&document);
        auto face=TopoDS::Face(TopExp_Explorer(shape,TopAbs_FACE).Current());TopoDS_Shape openShape=face;auto sheet=std::make_unique<CSolid>(openShape);auto* openBody=sheet.get();require(sheet->ReBuldMesh(),"Cannot mesh open fixture");document.AddObject(std::move(sheet),false);document.ClearSelection();document.SelectObjectById(openBody->m_id);openBody->AddSelectedFace(0);window.ActivateParametricTool("SolidExtrudeGizmo");application.processEvents();auto* dialog=window.findChild<QDialog*>("HybridExtrudeDialog");require(dialog&&!dialog->findChild<QPushButton*>("HybridExtrudeAccept")->isEnabled()&&dialog->findChild<QLabel*>("HybridExtrudeStatus")->text().contains("closed solid"),"Solid command accepted open surface");dialog->reject();
        std::cout<<"Solid Extrude with Gizmo connected faces, Move/Rotate/Scale, closed result, Cancel, Undo/Redo and open-surface rejection passed\n";return 0;
    }
    if(application.arguments().contains("--extrude-taper-preview")) {
        {ExtrudeFaceDialog dialog;dialog.findChild<QDoubleSpinBox*>()->setValue(12.5);dialog.accept();}
        {ExtrudeFaceDialog dialog;require(dialog.TaperAngle()==12.5,"Taper angle not remembered");dialog.SetCurvedFace(true);require(dialog.TaperAngle()==12.5,"Curved face reset remembered angle");dialog.findChild<QDoubleSpinBox*>()->setValue(-7);dialog.reject();}
        {ExtrudeFaceDialog dialog;require(dialog.TaperAngle()==12.5,"Cancel overwrote remembered angle");}
        auto shape=BRepPrimAPI_MakeBox(40,30,20).Shape();auto body=std::make_unique<CSolid>(shape);auto* solid=body.get();require(solid->ReBuldMesh(),"Cannot mesh taper fixture");document.AddObject(std::move(body),false);
        const auto pick=[&]{return document.SelectSolidFaceAtScreen({200,150},[](Vec3 p,DomPoint& screen,float& depth){screen={int(p.x*10),int(p.y*10)};depth=100-p.z;return true;});};
        window.resize(1100,800);window.show();window.viewport_->FitToDocument();window.viewport_->SetTool(ToolMode::FaceExtrude);QApplication::processEvents();
        for(double angle:{-12.,12.})for(float distance:{-4.f,8.f}) {
            require(pick(),"Cannot select taper face");const auto face=solid->GetTopoFace(solid->GetSelectedFaceIndex());Vec3 center,normal;require(document.GetSelectedSolidFaceCenterAndNormal(center,normal),"Missing taper normal");
            require(document.BeginLiveExtrudeSelectedSolidFace(angle),"Cannot start taper preview");
            QElapsedTimer timer;timer.start();for(int i=1;i<=5;++i)require(document.PreviewLiveExtrudeSelectedSolidFace(distance*i/5),"Taper preview failed");std::cout<<"5 planar taper previews: "<<timer.elapsed()<<" ms\n";
            std::string error;auto expected=BuildExtrudedFaceSolid(shape,face,normal,distance,angle,error);require(!expected.IsNull(),error.c_str());GProp_GProps actualVolume,expectedVolume;BRepGProp::VolumeProperties(solid->m_Shape,actualVolume);BRepGProp::VolumeProperties(expected,expectedVolume);require(std::abs(actualVolume.Mass()-expectedVolume.Mass())<1.e-5,"Taper preview differs from final geometry");
            auto straight=BuildExtrudedFaceSolid(shape,face,normal,distance,0,error);GProp_GProps straightVolume;BRepGProp::VolumeProperties(straight,straightVolume);if(distance>0)require(std::abs(actualVolume.Mass()-straightVolume.Mass())>1,"Preview ignored taper angle");Vec3 moved,n;require(document.GetSelectedSolidFaceCenterAndNormal(moved,n),"Preview lost gizmo");auto delta=moved-(center+normal*distance);require(dot(delta,delta)<1.e-6,"Gizmo did not follow tapered preview");
            const int output=application.arguments().indexOf("--output");if(output>=0 && angle>0 && distance>0){QApplication::processEvents();window.viewport_->grabFramebuffer().save(application.arguments().value(output+1));}
            require(document.UpdateLiveExtrudeSelectedSolidFace(distance),"Taper final build failed");document.CancelLiveExtrudeSelectedSolidFace();require(solid->m_Shape.IsSame(shape),"Taper Cancel did not restore source");
        }
        std::cout<<"Extrude taper settings, positive/negative preview, gizmo and Cancel passed\n";return 0;
    }
    if (application.arguments().contains("--universal-transform")) {
        auto shape=BRepPrimAPI_MakeBox(40,25,15).Shape();auto body=std::make_unique<CSolid>(shape);
        require(body->ReBuldMesh(),"Cannot mesh transform fixture");auto* solid=body.get();document.AddObject(std::move(body),false);document.SelectObjectById(solid->m_id);
        window.resize(1100,800);window.show();window.viewport_->FitToDocument();QApplication::processEvents();
        window.BeginTransformTool(TransformOperation::Universal);auto* viewport=window.viewport_;
        require(window.active_tool_key_=="transform" && viewport->universal_transform_,"Transform activates Move instead of universal mode");
        for(auto desired:{TransformOperation::Move,TransformOperation::Rotate,TransformOperation::Scale}) {
            QPoint picked;bool found=false;
            for(int y=10;y<viewport->height()-10&&!found;y+=5)for(int x=10;x<viewport->width()-10&&!found;x+=5){TransformOperation operation;auto axis=viewport->HitTestTransformGizmo(QPoint(x,y),&operation);if(axis!=TransformAxis::None&&operation==desired&&(desired!=TransformOperation::Move||axis==TransformAxis::X)){picked=QPoint(x,y);found=true;}}
            require(found,"Universal gizmo handle missing");
            GProp_GProps before;BRepGProp::VolumeProperties(solid->m_Shape,before);
            viewport->HandleTransformClick(picked,false);require(viewport->dragging_transform_&&viewport->transform_operation_==desired,"Wrong operation selected by gizmo");
            viewport->HandleTransformDrag(picked+QPoint(27,-19),Qt::NoModifier);require(viewport->transform_drag_has_preview_,"Universal drag has no preview");
            if(desired==TransformOperation::Rotate)require(std::abs(viewport->transform_drag_rotation_angle_)>.001,"Rotation handle did not rotate");
            QMouseEvent release(QEvent::MouseButtonRelease,QPointF(picked+QPoint(27,-19)),QPointF(picked+QPoint(27,-19)),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(viewport,&release);
            GProp_GProps after;BRepGProp::VolumeProperties(solid->m_Shape,after);
            if(desired==TransformOperation::Move)require(before.CentreOfMass().Distance(after.CentreOfMass())>.001,"Move handle did not move geometry");
            if(desired==TransformOperation::Scale)require(after.Mass()>before.Mass()*1.1,"Scale handle did not scale geometry");
            require(viewport->universal_transform_&&window.active_tool_key_=="transform","Drag left universal Transform mode");
        }
        const int output=application.arguments().indexOf("--output");if(output>=0&&output+1<application.arguments().size()){QApplication::processEvents();viewport->grabFramebuffer().save(application.arguments()[output+1]);}
        window.BeginTransformTool(TransformOperation::Move);require(!viewport->universal_transform_&&window.active_tool_key_=="move","Dedicated Move did not leave universal mode");
        std::cout<<"Universal Transform move/rotate/scale geometry and command state passed\n";return 0;
    }
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
