#include "KitchenLayout.h"
#include "CKitchenCabinet.h"
#include "CPolyline.h"
#include "DrawingText.h"
#include "CFurnitureAssemblies.h"
#include "ui/ToolRegistry.h"
#include "ui/OpenGLViewport.h"
#include <QMouseEvent>
#include "CAlfaDoc.h"
#include "CAssembled.h"
#include "UndoRedo.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "ui/KitchenLayoutDialog.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsSimpleTextItem>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <set>
namespace {
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
CAssembled* root(CAlfaDoc& doc){for(auto& o:doc.GetObjects())if(o&&o->GetParametricToolId()=="kitchen_layout")return dynamic_cast<CAssembled*>(o.get());return nullptr;}
std::vector<unsigned long> leaves(CAlfaDoc& doc,const CGroup& group) {
    std::vector<unsigned long> ids;
    for(auto id:group.GetElementIds()) {
        auto* object=doc.FindObjectById(id);check(object,"Dangling kitchen part");
        if(auto* child=dynamic_cast<CGroup*>(object)){auto nested=leaves(doc,*child);ids.insert(ids.end(),nested.begin(),nested.end());}
        else ids.push_back(id);
    }
    return ids;
}
unsigned long moduleId(CAlfaDoc& doc,int uid) {
    for(auto& o:doc.GetObjects())if(o&&o->GetGroupName().rfind("Kitchen group module ",0)==0&&o->GetParametricToolId()!="kitchen_layout_facade")
        for(const auto& p:o->GetParametricParameters())if(p.id=="uid"&&p.value==uid)return o->m_id;
    return 0;
}
}
int TestKitchenLayout(int argc,char** argv) {
    QApplication app(argc,argv);CAlfaDoc doc;std::string error;
    Dom3DProjectSerializer serializer;QString err;ProjectViewState view;QDir().mkpath("output/kitchen-composer");
    for(int i=0;i<4;++i) {
        doc.GetObjects().clear();auto k=KitchenLayout::Preset(i);
        check(BuildKitchenLayout(doc,k,nullptr,error),error.c_str());auto* a=root(doc);
        check(a&&a->GetElementIds().size()==k.rows.size(),"Missing independent row assemblies");
        auto geometry=leaves(doc,*a);check(geometry.size()>40,"Incomplete kitchen geometry");
        std::set<unsigned long> unique(geometry.begin(),geometry.end());check(unique.size()==geometry.size(),"Kitchen part is owned twice");
        for(auto id:geometry){auto* solid=dynamic_cast<CSolid*>(doc.FindObjectById(id));check(solid&&!solid->m_Shape.IsNull(),"Invalid kitchen solid");
            check(BRepCheck_Analyzer(solid->m_Shape).IsValid(),"Invalid cabinet or facade BRep");}
        for(const auto& m:k.modules)if(m.type!=8) {
            auto id=moduleId(doc,m.uid);check(id,"Module is not independently parametric");
            auto* module=dynamic_cast<CAssembled*>(doc.FindObjectById(id));
            if(m.type<=4||m.type==9){bool facade=false;for(auto child:leaves(doc,*module))if(doc.FindObjectById(child)->GetName().find("Facade")!=std::string::npos)facade=true;check(facade,"Module has no facade geometry");
                if(m.type<=1)check(dynamic_cast<CKitchenCabinet*>(module),"Cabinet is not a native parametric object");
                if(m.type>=2&&m.type<=4)check(dynamic_cast<CDrawerBoxFurniture*>(module),"Drawers are not native parametric furniture");}
            auto parts=leaves(doc,*module);check(FindKitchenLayout(doc,parts.front())==a&&FindKitchenModule(doc,parts.front())==m.uid,"Cannot edit a module by clicking its part");
        }
        for(const auto& m:k.modules)if(m.type==5){
            CAlfaObject* plinth=nullptr;
            for(auto member:geometry)if(doc.FindObjectById(member)->GetGroupName()=="Kitchen module "+std::to_string(m.uid)+" / plinth")plinth=doc.FindObjectById(member);
            check(plinth,"Stove has no plinth");check(plinth->GetMaterialId()!=0,"Stove plinth has no assigned material");Vec3 lo,hi;check(plinth->GetBounds(lo,hi),"Stove plinth has no bounds");
            check(std::abs(lo.z)<0.001&&std::abs(hi.z-k.legs)<0.001,"Stove plinth does not reach appliance bottom");
        }
        if(i==3){auto places=k.Placements();
            check(std::abs(places[6].y-places[6].width/2-places[9].y+places[9].width/2)<0.001,"Corner hood is offset from stove");
            auto old=k.Encode();for(auto& p:old){if(p.id=="version")p.value=4;if(p.id=="module_8_width")p.value=600;}
            auto migrated=KitchenLayout::Decode(old);check(std::abs(migrated.modules[8].width-840)<0.001,"Old corner hood was not aligned");
            check(std::abs(KitchenLayout::Decode(migrated.Encode()).modules[8].width-840)<0.001,"Hood correction repeated on reload");
        }
        int tops=0;for(auto member:geometry)if(doc.FindObjectById(member)->GetGroupName().find(" / worktop")!=std::string::npos){
            ++tops;bool radius20=false;
            auto* top=dynamic_cast<CSolid*>(doc.FindObjectById(member));
            for(TopExp_Explorer faces(top->m_Shape,TopAbs_FACE);faces.More();faces.Next()) {
                BRepAdaptor_Surface surface(TopoDS::Face(faces.Current()));
                if(surface.GetType()==GeomAbs_Cylinder&&std::abs(surface.Cylinder().Radius()-20)<1e-6)radius20=true;
            }
            check(radius20,"Worktop is missing its R20 front edge");
        }
        check(tops==(i==1?1:(i==3?3:2)),"Worktop did not follow continuous lower runs");
        if(i==3) {
            CSolid* parentTop=nullptr;CSolid* childTop=nullptr;
            for(auto member:geometry) {
                auto* object=doc.FindObjectById(member);
                if(object->GetGroupName()=="Kitchen module 1 / worktop")parentTop=dynamic_cast<CSolid*>(object);
                if(object->GetGroupName()=="Kitchen module 6 / worktop")childTop=dynamic_cast<CSolid*>(object);
            }
            check(parentTop&&childTop,"Missing corner worktops");
            BRepAlgoAPI_Common topCommon(parentTop->m_Shape,childTop->m_Shape);topCommon.Build();
            check(topCommon.IsDone(),"Cannot check worktop joint");
            GProp_GProps topProps;BRepGProp::VolumeProperties(topCommon.Shape(),topProps);
            check(std::abs(topProps.Mass())<0.001,"Corner worktops overlap");
            BRepClass3d_SolidClassifier joint(childTop->m_Shape,gp_Pnt(2000,-580,k.legs+k.baseHeight+k.top-1),1e-6);
            check(joint.State()==TopAbs_IN,"Gap above rounded parent at corner worktop joint");
            CSolid* blind=nullptr;CSolid* adjacent=nullptr;
            for(auto child:geometry){auto* o=doc.FindObjectById(child);const auto& key=o->GetGroupName();
                if(key=="Kitchen module 3 / blind corner front")blind=dynamic_cast<CSolid*>(o);
                if(key.find("Kitchen module 6 / Cabinet Left Side")==0)adjacent=dynamic_cast<CSolid*>(o);}
            check(blind&&adjacent,"Missing corner closure or adjacent side");
            BRepAlgoAPI_Common common(blind->m_Shape,adjacent->m_Shape);common.Build();check(common.IsDone(),"Cannot verify corner clearance");
            GProp_GProps props;BRepGProp::VolumeProperties(common.Shape(),props);check(std::abs(props.Mass())<0.001,"Connected cabinet intersects the corner front");
        }
        check(serializer.Save(QString("output/kitchen-composer/kitchen-%1.dom3d").arg(i+1),doc,"",view,{},err),"Cannot save kitchen fixture");
    }
    auto* a=root(doc);const auto id=a->m_id;auto k=KitchenLayout::Decode(a->GetParametricParameters());
    const auto geometry=leaves(doc,*a);const auto first=geometry.front();auto* part=doc.FindObjectById(first);part->SetColor({0.2f,0.4f,0.6f});
    const auto firstModule=moduleId(doc,k.modules[0].uid);
    a->Translate({120,230,40});Vec3 before,hi;part->GetBounds(before,hi);
    auto connectedBefore=k.Frame(2),upperBefore=k.Frame(3);
    CUndoRedo undo(doc);undo.BeginChange();k.modules[0].width+=100;
    check(BuildKitchenLayout(doc,k,a,error),error.c_str());undo.CommitChange("Resize kitchen");
    check(std::abs(k.Frame(2).x-connectedBefore.x-100)<0.01&&std::abs(k.Frame(3).x-upperBefore.x)<0.01,"Connected rows did not follow independently");
    check(root(doc)->m_id==id&&moduleId(doc,k.modules[0].uid)==firstModule&&doc.FindObjectById(first),"Kitchen/module identity changed");
    Vec3 after;doc.FindObjectById(first)->GetBounds(after,hi);
    check(std::abs(after.x-before.x)<0.01&&std::abs(after.y-before.y)<0.01&&std::abs(after.z-before.z)<0.01,"Kitchen placement lost or applied twice");
    check(std::abs(doc.FindObjectById(first)->GetColor().r-0.2f)<0.001,"Material appearance lost");
    check(undo.Undo(),"Cannot undo kitchen edit");check(KitchenLayout::Decode(root(doc)->GetParametricParameters()).modules[0].width==600,"Undo did not restore dimensions");
    check(undo.Redo(),"Cannot redo kitchen edit");
    QTemporaryDir temp;check(serializer.Save(temp.filePath("k.dom3d"),doc,"",view,{},err),"Cannot save edited kitchen");
    CAlfaDoc loaded;QString room;check(serializer.Load(temp.filePath("k.dom3d"),loaded,room,view,err),"Cannot reload kitchen");
    auto* loadedRoot=root(loaded);check(loadedRoot&&loadedRoot->GetAssemblyTransform()[3]==120,"Lost saved kitchen transform");
    auto decoded=KitchenLayout::Decode(loadedRoot->GetParametricParameters());check(decoded.modules[0].width==700&&decoded.rows.size()==4&&decoded.rows[2].parent==0,"Lost saved module/row parameters");
    decoded.modules[1].openDrawer=2;decoded.modules[1].drawerRatios={1,2,3,1};decoded.modules[1].facadeStyle=3;
    check(BuildKitchenLayout(loaded,decoded,loadedRoot,error),"Cannot edit reloaded drawers/facades");
    auto* drawers=dynamic_cast<CGroup*>(loaded.FindObjectById(moduleId(loaded,decoded.modules[1].uid)));bool opened=false;
    for(auto child:leaves(loaded,*drawers)){auto* o=loaded.FindObjectById(child);if(o->GetName().find("Drawer 2 Facade")!=std::string::npos){Vec3 lo; o->GetBounds(lo,hi);if(lo.y<230-560-250)opened=true;}}
    check(opened,"Parametric drawer did not open with its facade");
    // Editing native furniture inside a kitchen updates its layout and retains inheritance.
    SetAlfaDoc(&loaded);ToolRegistry registry;
    auto* slxTool=registry.Find("cabinet_advanced_slx");check(slxTool,"Missing SLX cabinet tool");
    std::set<std::string> parameterIds;for(const auto& p:slxTool->defaults)check(parameterIds.insert(p.id).second,"Duplicate SLX parameter after adding cabinet handles");
    auto drawerId=moduleId(loaded,decoded.modules[1].uid);
    auto* native=loaded.FindObjectById(drawerId);
    auto active=registry.ActiveObjectFromDocument(loaded.FindObjectIndexById(drawerId),*native,0,&loaded);
    for(auto& p:active.parameters)if(p.id=="width")p.value=650;
    registry.Rebuild(active,loaded);
    decoded=KitchenLayout::Decode(root(loaded)->GetParametricParameters());
    check(decoded.modules[1].width==650&&decoded.modules[1].handleType==-1,"Native edit lost layout/inheritance");
    decoded.facadeStyle=4;decoded.handleType=2;decoded.panelThickness=20;
    const auto gold=loaded.FindMaterial("Gold")->id;
    const auto marble=loaded.FindMaterial("Marble")->id;
    decoded.materials[1]=double(gold);decoded.modules[1].materials[1]=double(marble);
    check(BuildKitchenLayout(loaded,decoded,root(loaded),error),error.c_str());
    auto* cabinet=dynamic_cast<CKitchenCabinet*>(loaded.FindObjectById(moduleId(loaded,decoded.modules[0].uid)));
    check(cabinet,"Missing native cabinet after rebuild");
    check(cabinet->GetFacadeStyle()==KitchenCabinetFacadeStyle::Milano&&cabinet->GetDefinition().handle_type==2&&cabinet->GetDefinition().make_legs&&cabinet->GetPanelThickness()==20,"Cabinet did not inherit kitchen construction");
    for(const auto& m:decoded.modules)if(m.type<=4){auto* g=dynamic_cast<CGroup*>(loaded.FindObjectById(moduleId(loaded,m.uid)));for(auto child:leaves(loaded,*g)) {
        auto* o=loaded.FindObjectById(child);if(o->GetName().find("Facade")!=std::string::npos)
            check(o->GetMaterialId()==(m.uid==decoded.modules[1].uid?marble:gold),"Common or individual facade material failed");
    }}
    check(serializer.Save(temp.filePath("native.dom3d"),loaded,"",view,{},err),"Cannot save native kitchen");
    CAlfaDoc restored;check(serializer.Load(temp.filePath("native.dom3d"),restored,room,view,err),"Cannot load native kitchen");
    auto restoredDefinition=KitchenLayout::Decode(root(restored)->GetParametricParameters());
    check(restoredDefinition.handleType==2&&restoredDefinition.modules[1].materials[1]==double(marble),"Native inheritance lost on disk");
    auto* savedCabinet=dynamic_cast<CKitchenCabinet*>(restored.FindObjectById(moduleId(restored,decoded.modules[0].uid)));
    check(savedCabinet&&savedCabinet->GetDefinition().make_legs&&savedCabinet->GetDefinition().handle_type==2,"Cabinet construction lost on disk");
    CAlfaDoc imported;Material occupied;occupied.name="Existing finish";imported.UpsertMaterial(occupied);
    check(serializer.ImportPart(temp.filePath("native.dom3d"),imported,"Kitchen",{0,0,0},{1,1,1},false,true,err),"Cannot import parametric kitchen");
    auto importedDefinition=KitchenLayout::Decode(root(imported)->GetParametricParameters());
    check(importedDefinition.materials[1]==double(imported.FindMaterial("Gold")->id)&&importedDefinition.modules[1].materials[1]==double(imported.FindMaterial("Marble")->id),"Imported kitchen material bindings were not remapped");
    // Once detached, a drawer unit remains editable through the normal furniture tool.
    SetAlfaDoc(&restored);auto detachedId=moduleId(restored,decoded.modules[1].uid);
    auto* detached=dynamic_cast<CAssembled*>(restored.FindObjectById(detachedId));
    for(auto& o:restored.GetObjects())if(auto* g=dynamic_cast<CGroup*>(o.get()))if(g!=detached){auto ids=g->GetElementIds();ids.erase(std::remove(ids.begin(),ids.end(),detachedId),ids.end());g->SetElementIds(ids);}
    auto matrix=detached->GetAssemblyTransform();Vec3 detachLo,detachHi;detached->GetBounds(detachLo,detachHi);
    active=registry.ActiveObjectFromDocument(restored.FindObjectIndexById(detachedId),*detached,0,&restored);
    for(auto& p:active.parameters)if(p.id=="facade_type")p.value=1;
    registry.Rebuild(active,restored);auto* rebuilt=dynamic_cast<CAssembled*>(restored.FindObjectById(detachedId));
    check(rebuilt&&rebuilt->GetAssemblyTransform()==matrix,"Detached drawer placement lost");
    Vec3 rebuiltLo,rebuiltHi;rebuilt->GetBounds(rebuiltLo,rebuiltHi);
    check(std::abs(rebuiltLo.z-detachLo.z)<0.01&&std::abs(rebuiltHi.z-detachHi.z)<0.01,"Detached drawer height changed on rebuild");
    SetAlfaDoc(&loaded);loadedRoot=root(loaded);
    auto overlap=KitchenLayout::Preset(0);overlap.modules.back().height=720;check(!overlap.Validate(error),"Overlapping refrigerator accepted");
    auto cycle=KitchenLayout::Preset(3);cycle.rows[0].parent=2;check(!cycle.Validate(error),"Cyclic row connection accepted");
    auto crossed=KitchenLayout::Preset(3);crossed.rows[2].parent=-1;crossed.rows[2].x=0;crossed.rows[2].y=0;crossed.rows[2].angle=0;check(!crossed.Validate(error),"Overlapping rows accepted");
    auto varyingDepth=KitchenLayout::Preset(3);varyingDepth.modules[0].depth=650;check(std::abs(varyingDepth.Frame(2).y+578)<0.01,"Corner must follow the last module depth");
    auto independent=KitchenLayout::Preset(1);independent.rows.push_back({4,false,0,-2000,37,-1});KitchenModule added;added.uid=100;added.row=4;independent.modules.push_back(added);check(independent.Validate(error),"Arbitrary independent row rejected");
    auto* accessorySource=loaded.FindObjectById(leaves(loaded,*loadedRoot).front());auto accessory=accessorySource->Clone();accessory->m_id=0;accessory->SetGroupName("User accessory");
    loaded.AddObject(std::move(accessory),false);auto accessoryId=loaded.GetObjects().back()->m_id;auto ids=loadedRoot->GetElementIds();ids.push_back(accessoryId);loadedRoot->SetElementIds(ids);
    Vec3 accessoryBefore,accessoryAfter;loaded.FindObjectById(accessoryId)->GetBounds(accessoryBefore,hi);
    check(BuildKitchenLayout(loaded,decoded,loadedRoot,error),"Cannot rebuild with accessory");check(loaded.FindObjectById(accessoryId),"User accessory removed");loaded.FindObjectById(accessoryId)->GetBounds(accessoryAfter,hi);
    check(std::abs(accessoryAfter.x-accessoryBefore.x)<0.01,"Accessory transformed twice");
    auto count=loaded.GetObjects().size();decoded.modules[0].width=-1;check(!BuildKitchenLayout(loaded,decoded,loadedRoot,error)&&loaded.GetObjects().size()==count,"Invalid edit mutated scene");
    // Upgrade an existing flat kitchen without losing its geometry identities.
    CAlfaDoc legacy;auto legacyDefinition=KitchenLayout::Preset(1);check(BuildKitchenLayout(legacy,legacyDefinition,nullptr,error),"Legacy fixture build failed");auto* legacyRoot=root(legacy);auto oldLeaves=leaves(legacy,*legacyRoot);auto params=legacyDefinition.Encode();
    params.erase(std::remove_if(params.begin(),params.end(),[](const ParametricParameterValue& p){return p.id.rfind("row_",0)==0||p.id.find("facadeStyle")!=std::string::npos;}),params.end());
    legacyRoot->SetElementIds(oldLeaves);legacyRoot->SetParametricDefinition("kitchen_layout",params);
    for(auto& o:legacy.GetObjects())if(o&&o.get()!=legacyRoot&&dynamic_cast<CGroup*>(o.get()))o.reset();
    auto upgrade=KitchenLayout::Decode(params);check(upgrade.rows.size()==2&&upgrade.modules[0].facadeStyle==0,"Version-1 defaults changed");check(BuildKitchenLayout(legacy,upgrade,legacyRoot,error),"Cannot upgrade flat kitchen");check(legacy.FindObjectById(oldLeaves.front()),"Upgrade lost body identity");
    KitchenLayout dialogValue=KitchenLayout::Preset(0);const auto originalCount=dialogValue.modules.size();
    QTimer::singleShot(0,[&](){auto* dlg=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dlg,"Missing kitchen dialog");auto* table=dlg->findChild<QTableWidget*>("kitchenModules");auto* rows=dlg->findChild<QListWidget*>("kitchenRows");
        check(table&&rows&&rows->count()==2&&table->rowCount()==5,"Rows are not independently editable");rows->setCurrentRow(1);check(table->rowCount()==5,"Upper row selector failed");rows->setCurrentRow(0);
        for(auto* b:dlg->findChildren<QPushButton*>())if(b->text()=="Add module")b->click();
        check(dlg->grab().save("output/kitchen-composer/editor.png"),"Cannot capture editor");
        dlg->findChild<QTabWidget*>()->setCurrentIndex(1);check(dlg->grab().save("output/kitchen-composer/materials-editor.png"),"Cannot capture materials");dlg->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
    check(EditKitchenLayoutDialog(nullptr,dialogValue,true)&&dialogValue.modules.size()==originalCount+1,"Dialog did not add module");
    QTimer::singleShot(0,[&](){auto* dlg=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dlg,"Missing module dialog");auto* style=dlg->findChild<QComboBox*>("moduleFacadeStyle");check(style,"Missing facade selector");style->setCurrentIndex(style->findData(4));check(dlg->grab().save("output/kitchen-composer/module-editor.png"),"Cannot capture module editor");dlg->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
    check(EditKitchenModuleDialog(nullptr,dialogValue,dialogValue.modules.front().uid)&&dialogValue.modules.front().facadeStyle==4,"Module facade is not independently editable");
    auto cornerDialog=KitchenLayout::Preset(1);
    QTimer::singleShot(0,[&](){auto* dlg=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dlg,"Missing layout dialog");
        for(auto* b:dlg->findChildren<QPushButton*>())if(b->text()=="Add corner row")b->click();
        auto* rows=dlg->findChild<QListWidget*>("kitchenRows");check(rows->count()==3,"Connected row command failed");
        check(dlg->grab().save("output/kitchen-composer/corner-editor.png"),"Cannot capture corner editor");
        dlg->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
    check(EditKitchenLayoutDialog(nullptr,cornerDialog,true)&&cornerDialog.rows.back().parent==0,"New corner row was not linked");
    check(cornerDialog.modules[cornerDialog.modules.size()-2].type==9,"Corner command omitted the blind corner cabinet");
    // Document-owned visibility must not depend on the legacy global document.
    SetAlfaDoc(&doc);auto* visibilityRoot=root(loaded);auto visibleIds=leaves(loaded,*visibilityRoot);
    loaded.SetObjectVisibility(visibilityRoot->m_id,false);
    for(auto member:visibleIds)check(!loaded.IsObjectVisible(*loaded.FindObjectById(member)),"Hidden group leaves remained visible");
    loaded.SetObjectVisibility(visibilityRoot->m_id,true);
    for(auto member:visibleIds)check(loaded.IsObjectVisible(*loaded.FindObjectById(member)),"Group did not become visible again");
    // Setback measured from the facade; front feet must lie behind the plinth.
    auto base=KitchenLayout::Decode(root(doc)->GetParametricParameters());
    double plinthFront=0,plinthBack=0,footFront=1e10;
    for(const auto& o:doc.GetObjects())if(o){Vec3 lo,hi;if(!o->GetBounds(lo,hi))continue;
        if(o->GetGroupName()=="Kitchen module 1 / plinth"){plinthFront=lo.y;plinthBack=hi.y;}
        if(o->GetGroupName().rfind("Kitchen module 1 / Cabinet Leg",0)==0)footFront=std::min(footFront,double(lo.y));}
    check(footFront>=plinthBack+1.9,"Front legs protrude through plinth");
    check(std::abs(plinthFront-(230-base.baseDepth-base.panelThickness)-base.plinthRecess)<0.01,"Plinth is not recessed from facade by 50 mm");
    // Plans are generated geometry, linked through stable module UIDs.
    CAlfaDoc planDoc;auto plan=KitchenLayout::Preset(3);plan.showPlan=true;
    check(BuildKitchenLayout(planDoc,plan,nullptr,error),error.c_str());auto* planRoot=root(planDoc);
    auto outline=[&]()->CPolyline*{for(const auto& o:planDoc.GetObjects())if(o&&o->GetGroupName()=="Kitchen plan 1 / module outline")return dynamic_cast<CPolyline*>(o.get());return nullptr;};
    auto* outlineObject=outline();check(outlineObject&&FindKitchenModule(planDoc,outlineObject->m_id)==1&&FindKitchenLayout(planDoc,outlineObject->m_id)==planRoot,"Plan did not identify its source module");
    const auto planId=outlineObject->m_id;auto point=outlineObject->GetPoints().front();
    check(PickKitchenPlanModule(planDoc,{int(point.x+300),int(point.y-200)},[](Vec3 p,DomPoint& q){q={int(p.x),int(p.y)};return true;})==planId,"Plan module interior cannot be picked");
    check(planDoc.SelectPolylineAtScreen({int(point.x),int(point.y)},[](Vec3 p,DomPoint& q){q={int(p.x),int(p.y)};return true;},2),"Cannot select scene plan");
    check(FindKitchenModule(planDoc,planDoc.GetSelectedObject()->m_id)==1,"Plan selection resolved to whole kitchen");
    OpenGLViewport planViewport;planViewport.resize(900,900);planViewport.SetDocument(&planDoc);planViewport.SetXYPlaneViewEnabled(true);
    auto planCamera=planViewport.GetCamera();planCamera.target={float(point.x+300),float(point.y-200),0};planCamera.distance=1200;planViewport.SetCamera(planCamera);
    int planDoubleClicks=0;QObject::connect(&planViewport,&OpenGLViewport::ObjectDoubleClicked,[&](){++planDoubleClicks;});
    QMouseEvent doubleClick(QEvent::MouseButtonDblClick,QPointF(450,450),QPointF(450,450),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&planViewport,&doubleClick);
    check(planDoubleClicks==1&&FindKitchenModule(planDoc,planDoc.GetSelectedObject()->m_id)==1,"Plan double-click entered curve editing instead of module parameters");
    CUndoRedo planUndo(planDoc);planUndo.BeginChange();plan.modules[0].width=750;
    check(BuildKitchenLayout(planDoc,plan,planRoot,error),error.c_str());planUndo.CommitChange("Resize linked plan");
    check(outline()->m_id==planId&&std::abs(outline()->GetPoints()[1].x-outline()->GetPoints()[0].x-750)<0.01,"Plan width not linked to module");
    check(planUndo.Undo()&&std::abs(outline()->GetPoints()[1].x-outline()->GetPoints()[0].x-600)<0.01,"Undo did not restore plan");check(planUndo.Redo(),"Plan redo failed");
    ProjectViewState planView=view;planView.camera=planViewport.GetCamera();
    check(serializer.Save("output/kitchen-composer/linked-plan.dom3d",planDoc,"",planView,{},err),"Cannot save scene plan");
    CAlfaDoc planLoaded;check(serializer.Load("output/kitchen-composer/linked-plan.dom3d",planLoaded,room,view,err),"Cannot load scene plan");
    check(KitchenLayout::Decode(root(planLoaded)->GetParametricParameters()).showPlan,"Lost scene plan setting");
    if(QApplication::platformName()!="offscreen") {
        SetAlfaDoc(&planDoc);planDoc.ClearSelection();planViewport.SetFloorGridVisible(false);planViewport.SetCoordinateAxesVisible(false);
        Vec3 lo{1e9f,1e9f,0},hi{-1e9f,-1e9f,0};
        for(const auto& o:planDoc.GetObjects())if(o&&o->GetParametricToolId()=="kitchen_plan_item"){bool upper=false;for(const auto& p:o->GetParametricParameters())if(p.id=="upper")upper=p.value!=0;if(upper)continue;Vec3 a,b;if(o->GetBounds(a,b)){lo.x=std::min(lo.x,a.x);lo.y=std::min(lo.y,a.y);hi.x=std::max(hi.x,b.x);hi.y=std::max(hi.y,b.y);}}
        planCamera.target={(lo.x+hi.x)/2,(lo.y+hi.y)/2,0};planCamera.distance=std::max(hi.x-lo.x,hi.y-lo.y)/0.75f;planViewport.SetCamera(planCamera);
        planViewport.move(-20000,-20000);planViewport.show();app.processEvents();
        check(planViewport.CaptureSceneImage(QSize(900,900)).save("output/kitchen-composer/linked-plan.png"),"Cannot capture linked scene plan");planViewport.hide();
    }
    planRoot=root(planDoc);SetAlfaDoc(&planDoc);planRoot->Translate({120,230,40});auto beforePlan=outline()->GetPoints().front();
    check(BuildKitchenLayout(planDoc,plan,planRoot,error),"Cannot rebuild moved plan");auto afterPlan=outline()->GetPoints().front();
    check(std::abs(beforePlan.x-afterPlan.x)<0.01&&std::abs(beforePlan.y-afterPlan.y)<0.01&&std::abs(beforePlan.z-afterPlan.z)<0.01,"Plan placement changed on rebuild");
    planDoc.SetObjectVisibility(planRoot->m_id,false);plan.modules[0].type=1;
    check(BuildKitchenLayout(planDoc,plan,planRoot,error),"Cannot rebuild hidden kitchen");
    for(auto member:leaves(planDoc,*planRoot))check(!planDoc.IsObjectVisible(*planDoc.FindObjectById(member)),"Rebuild exposed hidden geometry");
    plan.showPlan=false;check(BuildKitchenLayout(planDoc,plan,planRoot,error),"Cannot remove generated plan");
    check(!outline(),"Disabled plan left stale geometry");
    std::cout<<"Kitchen composer tests passed\n";return 0;
}

int TestKitchenEditor(int argc,char** argv) {
    KitchenCabinetDefinition hingeTest;hingeTest.width=800;hingeTest.height=700;hingeTest.depth=320;hingeTest.door_axis=1;hingeTest.facade_type=KitchenCabinetFacadeType::DoubleDoor;
    auto closed=CKitchenCabinet::BuildParts(hingeTest);check(!closed.empty(),"Horizontal doors did not build");
    Vec3 upperLo,upperHi,lowerLo,lowerHi;bool upperFound=false,lowerFound=false;
    for(const auto& part:closed){if(part->GetName().find("Left Facade")!=std::string::npos){part->GetBounds(upperLo,upperHi);upperFound=true;}
        if(part->GetName().find("Right Facade")!=std::string::npos){part->GetBounds(lowerLo,lowerHi);lowerFound=true;}}
    check(upperFound&&lowerFound&&upperLo.z>=lowerHi.z-0.001,"Horizontal double doors are not stacked");
    hingeTest.left_door_open_angle=90;hingeTest.right_door_open_angle=90;auto opened=CKitchenCabinet::BuildParts(hingeTest);check(!opened.empty(),"Horizontal door failed to open");
    bool lifted=false;for(const auto& part:opened)if(part->GetName().find("Left Facade")!=std::string::npos){Vec3 lo,hi;part->GetBounds(lo,hi);lifted=lo.y<upperLo.y-200&&hi.z-lo.z<50;}
    check(lifted,"Horizontal door did not rotate about top edge");
    for(const auto& part:closed){auto* solid=dynamic_cast<CSolid*>(part.get());check(solid&&BRepCheck_Analyzer(solid->m_Shape).IsValid(),"Invalid horizontal facade geometry");
        if(part->GetName().find("Handle")!=std::string::npos){Vec3 lo,hi;part->GetBounds(lo,hi);check(std::abs(lo.x+hi.x)<0.01&&hi.x-lo.x>hi.z-lo.z,"Horizontal door handle is not centered and horizontal");}}

    QApplication app(argc,argv);CAlfaDoc source;SetAlfaDoc(&source);const auto sourceCount=source.GetObjects().size();
    {
        CAlfaDoc fixture;auto model=KitchenLayout::Preset(0);model.modules[6].doorAxis=1;model.modules[7].type=1;model.modules[7].doorAxis=1;model.modules[9].doorAxis=1;
        std::string buildError;check(BuildKitchenLayout(fixture,model,nullptr,buildError),"Cannot build horizontal kitchen fixture");
        for(auto& object:fixture.GetObjects())if(object&&object->GetGroupName()=="Kitchen module 2 / plinth")object->SetMaterialId(0);
        check(BuildKitchenLayout(fixture,model,root(fixture),buildError),"Cannot rebuild old untextured plinth");
        for(const auto& object:fixture.GetObjects())if(object&&object->GetGroupName()=="Kitchen module 2 / plinth")check(object->GetMaterialId()!=0,"Old plinth material was not repaired");
        QDir().mkpath("output/kitchen-composer");Dom3DProjectSerializer serializer;ProjectViewState view;QString error,room;
        check(serializer.Save("output/kitchen-composer/horizontal-doors.dom3d",fixture,"",view,{},error),"Cannot save horizontal doors");
        CAlfaDoc restored;check(serializer.Load("output/kitchen-composer/horizontal-doors.dom3d",restored,room,view,error),"Cannot load horizontal doors");
        auto* cabinet=dynamic_cast<CKitchenCabinet*>(restored.FindObjectById(moduleId(restored,8)));
        check(cabinet&&cabinet->GetDefinition().door_axis==1&&KitchenLayout::Decode(root(restored)->GetParametricParameters()).modules[7].doorAxis==1,"Door axis lost during save/load");
    }
    SetAlfaDoc(&source);
    QSettings settings("Dom3D","Dom3D_Pro");settings.beginGroup("KitchenLayoutEditor");
    QMap<QString,QVariant> saved;for(const auto& key:settings.allKeys())saved[key]=settings.value(key);settings.remove("");settings.endGroup();
    QWidget main;main.resize(1400,880);main.move(0,0);QWidget host(&main);host.setGeometry(0,0,1400,880);main.show();
    auto value=KitchenLayout::Preset(0);value.showPlan=true;const double oldWidth=value.modules[0].width;
    QDir().mkpath("output/kitchen-composer");
    QTimer::singleShot(50,[&](){auto* dlg=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dlg,"Missing scene editor");
        auto* filter=dlg->findChild<QComboBox*>("kitchenRowFilter");auto* stage=dlg->findChild<QComboBox*>("kitchenStage");auto* mode=dlg->findChild<QComboBox*>("kitchenView");
        auto* table=dlg->findChild<QTableWidget*>("kitchenModules");auto* rows=dlg->findChild<QListWidget*>("kitchenRows");
        check(filter&&stage&&mode&&table&&rows,"Missing scene editor controls");filter->setCurrentIndex(0);check(rows->count()==1,"Lower filter failed");
        auto* graphics=dlg->findChild<QGraphicsView*>();check(graphics&&graphics->isVisible(),"Plan was not moved into scene");
        QGraphicsItem* picked=nullptr;for(auto* item:graphics->scene()->items())if(item->data(0).toInt()==2){picked=item;break;}
        check(picked,"Missing plan module");const QPoint point=graphics->mapFromScene(picked->sceneBoundingRect().center());
        QMouseEvent click(QEvent::MouseButtonPress,QPointF(point),QPointF(graphics->viewport()->mapToGlobal(point)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(graphics->viewport(),&click);
        check(table->item(table->currentRow(),0)->data(Qt::UserRole).toInt()==2,"Plan selection did not select table row");
        stage->setCurrentIndex(1);static_cast<QDoubleSpinBox*>(table->cellWidget(0,1))->setValue(oldWidth+100);
        check(dlg->grab().save("output/kitchen-composer/compact-editor.png"),"Cannot capture compact editor");
        auto* scene=dlg->findChild<QWidget*>("kitchenEditorScene");check(scene&&scene->grab().save("output/kitchen-composer/working-plan.png"),"Cannot capture plan");
        filter->setCurrentIndex(2);
        check(rows->height()<150,"Short row list still consumes excessive vertical space");
        std::vector<QRectF> labelBounds;
        for(auto* item:graphics->scene()->items())if(auto* label=dynamic_cast<QGraphicsSimpleTextItem*>(item)){
            const auto bounds=label->deviceTransform(graphics->viewportTransform()).mapRect(label->boundingRect());
            for(const auto& other:labelBounds)check(!bounds.intersects(other),"Upper and lower labels overlap in Together view");
            labelBounds.push_back(bounds);
        }
        check(labelBounds.size()==value.modules.size(),"Together view omitted module labels");
        check(scene->grab().save("output/kitchen-composer/working-plan-together.png"),"Cannot capture Together plan");
        filter->setCurrentIndex(0);
        mode->setCurrentIndex(1);
        QTimer::singleShot(1800,dlg,[&,dlg](){auto* scene=dlg->findChild<QWidget*>("kitchenEditorScene");
            check(scene->grab().save("output/kitchen-composer/working-3d.png"),"Cannot capture 3D preview");dlg->reject();});
    });
    check(!EditKitchenLayoutDialog(&main,value,false,&host),"Cancel accepted preview");
    check(value.modules[0].width==oldWidth&&source.GetObjects().size()==sourceCount&&GetAlfaDoc()==&source,"Preview changed source or canceled parameters");
    QTimer::singleShot(0,[&](){auto* dlg=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dlg,"Missing reopened editor");
        check(dlg->findChild<QComboBox*>("kitchenStage")->currentIndex()==1,"Stage was not remembered");
        check(dlg->findChild<QComboBox*>("kitchenRowFilter")->currentIndex()==0,"Filter was not remembered");
        auto* table=dlg->findChild<QTableWidget*>("kitchenModules");check(table->item(table->currentRow(),0)->data(Qt::UserRole).toInt()==2,"Module was not remembered");
        check(static_cast<QDoubleSpinBox*>(table->cellWidget(0,1))->value()==oldWidth,"Canceled dimensions survived reopening");dlg->reject();});
    check(!EditKitchenLayoutDialog(&main,value,false,&host),"Reopened cancel failed");
    settings.beginGroup("KitchenLayoutEditor");settings.remove("");for(auto it=saved.begin();it!=saved.end();++it)settings.setValue(it.key(),it.value());settings.endGroup();
    std::cout<<"Kitchen scene editor tests passed\n";return 0;
}
