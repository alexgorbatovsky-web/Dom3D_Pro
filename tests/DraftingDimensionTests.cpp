#include "ui/DraftingWorkspace.h"
#include "ui/DraftingDimensions.h"
#include "ui/DraftingDimensionReferences.h"
#include "ui/ToolRegistry.h"
#include "Dom3DProjectSerializer.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include "CAlfaDoc.h"
#include "solid/Solid.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPainter>
#include <QPdfWriter>
#include <QDir>
#include <QAction>
#include <QTimer>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void expectNear(double a,double b,const char* message) {check(std::abs(a-b)<1.e-6,message);}
QJsonObject primitive(int id,QString type,QPointF a,QPointF b={}) {
    return {{"id",id},{"type",type},{"x1",a.x()},{"y1",a.y()},{"x2",b.x()},{"y2",b.y()}};
}
void save(CAlfaDoc& doc,QJsonArray primitives,double scale=1) {
    QJsonObject sheet{{"name","Dimensions"},{"width",210},{"height",297},{"landscape",true},
        {"stampVisible",false},{"scale",scale},{"primitives",primitives}};
    doc.SetDraftingData(QJsonDocument(QJsonObject{{"version",1},{"sheets",QJsonArray{sheet}}}).toJson().toStdString());
}
QJsonArray primitives(const CAlfaDoc& doc) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(doc.GetDraftingData())).object()["sheets"].toArray()[0].toObject()["primitives"].toArray();
}
QGraphicsItem* dimension(QGraphicsScene* scene,int kind) {
    for(auto* item:scene->items())if(!item->parentItem()&&item->data(2).isValid()&&item->data(2).toInt()==kind)return item;
    throw std::runtime_error("Dimension not rendered");
}
void click(QGraphicsScene* scene,QPointF point,QEvent::Type type=QEvent::GraphicsSceneMousePress) {
    QGraphicsSceneMouseEvent event(type);event.setScenePos(point);event.setButton(Qt::LeftButton);
    event.setButtons(type==QEvent::GraphicsSceneMouseRelease?Qt::NoButton:Qt::LeftButton);
    QApplication::sendEvent(scene,&event);
}
}
int TestDraftingDimensions(int argc,char** argv) {
    QApplication app(argc,argv);
    using namespace drafting;
    try {
        if(argc>=4 && QString::fromLocal8Bit(argv[2])=="--recover-detail") {
            CAlfaDoc current,previous;Dom3DProjectSerializer serializer;QString room,error;ProjectViewState camera;
            check(serializer.Load(QString::fromLocal8Bit(argv[3]),current,room,camera,error),"Cannot load Detail");
            check(serializer.Load(QString::fromLocal8Bit(argv[3]),previous,room,camera,error),"Cannot load recovery copy");
            size_t bodyIndex=0;while(bodyIndex<previous.GetObjects().size() && previous.GetObjects()[bodyIndex]->m_id!=15)++bodyIndex;
            check(bodyIndex<previous.GetObjects().size(),"Missing Detail body");
            auto* body=dynamic_cast<CSolid*>(previous.GetObjects()[bodyIndex].get());check(body,"Wrong Detail body");
            check(body->GetOperation(body->GetNumOperations()-1)->ToolId=="SolidHole","Expected final hole");
            body->RemoveParametricOperation(body->GetNumOperations()-1);
            ToolRegistry registry;check(registry.ReplayOperations(bodyIndex,previous),"Cannot replay pre-hole history");
            body=dynamic_cast<CSolid*>(previous.GetObjects()[bodyIndex].get());
            TopTools_IndexedMapOfShape oldEdges,newEdges;TopExp::MapShapes(body->m_Shape,TopAbs_EDGE,oldEdges);
            TopExp::MapShapes(dynamic_cast<CSolid*>(current.GetObjects()[bodyIndex].get())->m_Shape,TopAbs_EDGE,newEdges);
            std::cout<<"Detail edges before="<<oldEdges.Extent()<<" after="<<newEdges.Extent()<<std::endl;
            auto root=QJsonDocument::fromJson(QByteArray::fromStdString(current.GetDraftingData())).object();auto sheets=root["sheets"].toArray();int recovered=0;
            for(int i=0;i<sheets.size();++i){auto sheet=sheets[i].toObject();auto pp=sheet["primitives"].toArray();
                for(int j=0;j<pp.size();++j){auto p=pp[j].toObject();if(!p["dimension"].isObject())continue;
                    auto dim=FromJson(p["dimension"].toObject());bool repaired=false;
                    for(auto& ref:dim.refs)if(ref.body==15 && ref.edge_count!=newEdges.Extent()) {
                        check(ref.edge_count==oldEdges.Extent(),"Wrong historical edge count");
                        CaptureEdge(ref,oldEdges,ref.edge);check(ResolveEdge(ref,newEdges),"Cannot uniquely restore old edge");repaired=true;
                    }
                    if(repaired)++recovered;p["dimension"]=ToJson(dim);pp[j]=p;
                }sheet["primitives"]=pp;sheets[i]=sheet;
            }
            check(recovered==5,"Expected five lost dimensions");root["sheets"]=sheets;current.SetDraftingData(QJsonDocument(root).toJson().toStdString());
            QDir().mkpath("output/dimension-topology");
            check(serializer.Save("output/dimension-topology/Detail-1-dimensions-restored.dom3d",current,room,camera,QImage(),error),"Cannot save restored copy");
            DraftingWorkspace workspace;workspace.SetDocument(&current);auto* scene=workspace.findChild<QGraphicsView*>()->scene();int dimensions=0;
            for(auto* item:scene->items())if(!item->parentItem()&&item->data(2).isValid()){check(item->data(3).toBool(),"Restored dimension unresolved");++dimensions;}
            check(dimensions==9,"Restoration lost dimensions");
            QImage image(2100,1485,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
            scene->render(&painter,QRectF(0,0,2100,1485),QRectF(0,0,420,297));painter.end();image.save("output/dimension-topology/restored.png");
            auto* diameter=dimension(scene,5);QPointF labelPoint;bool foundLabel=false;
            for(auto* item:scene->items())if(auto* text=dynamic_cast<QGraphicsSimpleTextItem*>(item)) {
                auto* rootItem=item;while(rootItem->parentItem())rootItem=rootItem->parentItem();
                if(rootItem==diameter){labelPoint=text->mapToScene(text->boundingRect().center());foundLabel=true;break;}
            }
            check(foundLabel,"Missing diameter label");
            QTimer::singleShot(0,[]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                check(dialog,"Diameter editor not opened");dialog->findChild<QLineEdit*>("prefix")->setText("TEST ");dialog->accept();});
            click(scene,labelPoint,QEvent::GraphicsSceneMouseDoubleClick);
            check(FromJson(primitives(current)[8].toObject()["dimension"].toObject()).prefix=="TEST " ||
                [&]{for(const auto& v:primitives(current)){auto p=v.toObject();if(p["dimension"].isObject() && FromJson(p["dimension"].toObject()).prefix=="TEST ")return true;}return false;}(),"Diameter edit not saved");
            dimensions=0;for(auto* item:scene->items())if(!item->parentItem()&&item->data(2).isValid()) {
                check(item->data(3).toBool(),"Editing diameter invalidated another dimension");++dimensions;
            }
            check(dimensions==9,"Editing diameter removed dimensions");
            std::cout<<"Restored all 9 dimensions; repaired "<<recovered<<std::endl;return 0;
        }
        Geometry a,b;a.valid=b.valid=true;a.point={0,0};b.point={30,40};a.scale=b.scale=2;
        Dimension d;d.offset={15,-10};
        for(auto entry: {std::pair{DimensionKind::Horizontal,60.0},std::pair{DimensionKind::Vertical,80.0},std::pair{DimensionKind::Parallel,100.0}}) {
            d.kind=entry.first;auto l=Layout(d,{a,b});check(l.valid,"Linear layout invalid");expectNear(l.value,entry.second,"Wrong linear measurement");
        }
        d.kind=DimensionKind::Perpendicular;a.point={5,8};b.line=true;b.a={0,0};b.b={10,0};
        expectNear(Layout(d,{a,b}).value,16,"Wrong point-line measurement");
        a.circle=true;a.radius=5;a.center={0,0};d.kind=DimensionKind::Radius;
        expectNear(Layout(d,{a}).value,10,"Wrong radius");d.kind=DimensionKind::Diameter;expectNear(Layout(d,{a}).value,20,"Wrong diameter");
        a.line=true;a.a={0,0};a.b={10,0};a.point={5,0};b.a={0,0};b.b={5,std::sqrt(75.)};b.point=b.b;
        d.kind=DimensionKind::Angular;d.offset={5,5};expectNear(Layout(d,{a,b}).value,60,"Wrong angle");
        d.offset={-10,10};expectNear(Layout(d,{a,b}).value,120,"Cannot select supplementary angle");
        d.prefix="2x ";d.upper="+0.2";d.lower="-0.1";d.below="TEST";d.outside=true;d.refs={{1},{2}};
        check(ToJson(FromJson(ToJson(d)))==ToJson(d),"Dimension properties lost in JSON");
        QPolygonF circle;for(int i=0;i<65;++i){double t=i*6.283185307179586/64;circle<<QPointF(4+7*std::cos(t),8+7*std::sin(t));}
        QPointF center;double radius;check(FitCircle(circle,center,radius),"Circle not recognized");expectNear(radius,7,"Bad fitted radius");
        circle[5]+=QPointF(0,1);check(!FitCircle(circle,center,radius),"Noncircle accepted");

        CAlfaDoc doc;
        QJsonArray items{primitive(1,"rectangle",{45,40},{100,80}),primitive(2,"line",{130,75},{175,45}),
            primitive(3,"line",{125,105},{180,105}),primitive(4,"ellipse",{215,40},{245,70}),
            primitive(5,"ellipse",{215,110},{245,140}),primitive(6,"line",{60,160},{110,160}),
            primitive(7,"line",{60,160},{85,120})};
        const auto add=[&](DimensionKind kind,QVector<Reference> refs,QPointF offset) {
            Dimension dim;dim.kind=kind;dim.refs=refs;dim.offset=offset;
            auto p=primitive(20+int(kind),"dimension",{30,30});p["dimension"]=ToJson(dim);items.append(p);
        };
        add(DimensionKind::Horizontal,{{1,0,-1,0,-1,0},{1,0,-1,0,-1,1}},{25,-15});
        add(DimensionKind::Vertical,{{1,0,-1,0,-1,1},{1,0,-1,0,-1,2}},{15,20});
        add(DimensionKind::Parallel,{{2,0,-1,0,-1,0},{2,0,-1,0,-1,1}},{12,-25});
        Reference line;line.primitive=3;line.whole_edge=true;line.parameter=.5;
        add(DimensionKind::Perpendicular,{{2,0,-1,0,-1,0},line},{-10,10});
        add(DimensionKind::Radius,{{4}},{20,-15});add(DimensionKind::Diameter,{{5}},{22,15});
        Reference l1;l1.primitive=6;l1.whole_edge=true;l1.parameter=.7;Reference l2=l1;l2.primitive=7;
        add(DimensionKind::Angular,{l1,l2},{-5,-15});
        save(doc,items);
        DraftingWorkspace workspace;workspace.resize(1200,850);workspace.SetDocument(&doc);workspace.show();app.processEvents();
        auto* view=workspace.findChild<QGraphicsView*>();check(view,"Missing scene");auto* scene=view->scene();
        for(int kind=0;kind<7;++kind)check(dimension(scene,kind)->data(3).toBool(),"Invalid rendered dimension");
        expectNear(dimension(scene,0)->data(1).toDouble(),55,"Wrong rectangle width");
        QDir().mkpath("output/drafting-dimensions");
        QImage image(1782,1260,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);
        QPainter painter(&image);painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);
        scene->render(&painter,QRectF(0,0,1782,1260),QRectF(0,0,297,210));painter.end();
        check(image.save("output/drafting-dimensions/seven-types.png"),"Image save failed");
        QPdfWriter pdf("output/drafting-dimensions/seven-types.pdf");pdf.setPageSize(QPageSize(QSizeF(297,210),QPageSize::Millimeter));
        QPainter pp(&pdf);scene->render(&pp,QRectF(0,0,pdf.width(),pdf.height()),QRectF(0,0,297,210));pp.end();
        // Moving only a dimension retains source references and measured value.
        auto* moved=dimension(scene,0);moved->setSelected(true);moved->setPos(3,7);
        click(scene,{70,32},QEvent::GraphicsSceneMouseRelease);
        auto stored=primitives(doc);auto savedDim=FromJson(stored[7].toObject()["dimension"].toObject());
        expectNear(savedDim.offset.x(),28,"Dimension drag not saved");expectNear(savedDim.offset.y(),-8,"Dimension drag not saved");
        check(savedDim.refs[0].primitive==1,"Drag broke association");
        // Resize the source and reopen: the value must be recomputed.
        auto rect=stored[0].toObject();rect["x2"]=120;stored[0]=rect;save(doc,stored);workspace.ReloadFromDocument();
        scene=workspace.findChild<QGraphicsView*>()->scene();expectNear(dimension(scene,0)->data(1).toDouble(),75,"Resize did not update dimension");
        // Real placement through three scene clicks, then editing via double click.
        save(doc,QJsonArray{primitive(1,"line",{40,40},{90,40})});workspace.ReloadFromDocument();
        for(auto* action:workspace.findChildren<QAction*>())if(action->text()==DimensionName(DimensionKind::Horizontal))action->trigger();
        scene=workspace.findChild<QGraphicsView*>()->scene();click(scene,{40,40});click(scene,{90,40});click(scene,{60,25});
        check(primitives(doc).size()==2,"Three-click placement failed");
        for(auto* action:workspace.findChildren<QAction*>())if(action->text()==QString::fromUtf8("Выбор"))action->trigger();
        QTimer::singleShot(0,[]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(dialog,"Dimension editor not opened");dialog->findChild<QLineEdit*>("prefix")->setText("2x ");dialog->accept();});
        click(scene,{65,22},QEvent::GraphicsSceneMouseDoubleClick);
        check(FromJson(primitives(doc)[1].toObject()["dimension"].toObject()).prefix=="2x ","Editor changes not saved");
        // Reattachment replaces the annotation instead of creating a duplicate.
        QTimer::singleShot(0,[]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(dialog,"Reattach editor not opened");for(auto* button:dialog->findChildren<QPushButton*>())
                if(button->text()==QString::fromUtf8("Перепривязать")){button->click();return;}
            throw std::runtime_error("Missing reattach button");});
        click(scene,{65,22},QEvent::GraphicsSceneMouseDoubleClick);
        click(scene,{90,40});click(scene,{40,40});click(scene,{65,55});
        check(primitives(doc).size()==2,"Reattach duplicated dimension");
        check(FromJson(primitives(doc)[1].toObject()["dimension"].toObject()).refs[0].anchor==1,"Reattach kept old supports");
        // Exercise every tool through the same picking path used by the viewport.
        QJsonArray geometry;for(int i=0;i<7;++i)geometry.append(items[i]);save(doc,geometry);workspace.ReloadFromDocument();
        scene=workspace.findChild<QGraphicsView*>()->scene();
        const QVector<QVector<QPointF>> picks={{{45,40},{100,40},{70,25}},{{100,40},{100,80},{115,60}},
            {{130,75},{175,45},{145,55}},{{130,75},{150,105},{120,90}},{{245,55},{250,35}},
            {{245,125},{260,140}},{{90,160},{72.5,140},{90,140}}};
        for(int kind=0;kind<7;++kind) {
            for(auto* action:workspace.findChildren<QAction*>())if(action->text()==DimensionName(DimensionKind(kind)))action->trigger();
            for(int i=0;i<picks[kind].size();++i){
                QGraphicsSceneMouseEvent hover(QEvent::GraphicsSceneMouseMove);hover.setScenePos(picks[kind][i]);QApplication::sendEvent(scene,&hover);
                click(scene,picks[kind][i]);
            }
            check(primitives(doc).size()==8+kind,"Dimension tool failed to place");
            check(dimension(scene,kind)->data(3).toBool(),"Picked dimension invalid");
        }
        // Moving a source together with its dimension must translate it only once.
        for(auto* action:workspace.findChildren<QAction*>())if(action->text()==QString::fromUtf8("Выбор"))action->trigger();
        auto prior=FromJson(primitives(doc)[7].toObject()["dimension"].toObject()).offset;
        for(auto* item:scene->items())if(!item->parentItem()&&item->data(0).isValid()&&(item->data(0).toInt()==0||item==dimension(scene,0))) {
            item->setSelected(true);item->setPos(5,5);
        }
        click(scene,{70,30},QEvent::GraphicsSceneMouseRelease);
        check(FromJson(primitives(doc)[7].toObject()["dimension"].toObject()).offset==prior,"Group drag moved dimension twice");
        // CAD edge association survives model parameter changes and rejects missing bodies.
        TopoDS_Shape shape=BRepPrimAPI_MakeBox(40,20,10).Shape();auto body=std::make_unique<CSolid>(shape);auto* solid=body.get();doc.AddObject(std::move(body));
        TopTools_IndexedMapOfShape edges;TopExp::MapShapes(solid->m_Shape,TopAbs_EDGE,edges);int index=0;
        for(int i=1;i<=edges.Extent();++i){BRepAdaptor_Curve c(TopoDS::Edge(edges(i)));auto u=c.Value(c.FirstParameter()),v=c.Value(c.LastParameter());if(std::abs(u.X()-v.X())>39){index=i;break;}}
        check(index>0,"No box X edge");Reference r;r.primitive=1;r.body=solid->m_id;r.edge=index;r.edge_count=edges.Extent();r.curve_type=int(GeomAbs_Line);
        Dimension cad;cad.kind=DimensionKind::Horizontal;cad.refs={r,r};cad.refs[1].parameter=1;cad.offset={10,-10};
        auto mv=primitive(1,"model_view",{100,80});mv["projection"]="top";mv["viewScale"]=2;mv["viewCenterX"]=20;mv["viewCenterY"]=10;
        auto dim=primitive(2,"dimension",{80,60});dim["dimension"]=ToJson(cad);save(doc,{mv,dim});workspace.ReloadFromDocument();
        scene=workspace.findChild<QGraphicsView*>()->scene();expectNear(dimension(scene,0)->data(1).toDouble(),40,"CAD scale wrong");
        solid->m_Shape=BRepPrimAPI_MakeBox(60,20,10).Shape();workspace.ReloadFromDocument();scene=workspace.findChild<QGraphicsView*>()->scene();
        expectNear(dimension(scene,0)->data(1).toDouble(),60,"CAD parameter change lost association");
        workspace.hide();solid->m_Shape=BRepPrimAPI_MakeBox(80,20,10).Shape();workspace.show();
        for(int i=0;i<4;++i)app.processEvents();scene=workspace.findChild<QGraphicsView*>()->scene();
        expectNear(dimension(scene,0)->data(1).toDouble(),80,"Returning to Drafting did not refresh model dimension");
        check(!primitives(doc)[0].toObject()["visibleLines"].toArray().isEmpty(),"Automatic refresh did not update projection");
        // A hole adds edges but must not invalidate dimensions on unchanged edges.
        for(double x:{30.,55.}) {
            workspace.hide();solid->m_Shape=BRepAlgoAPI_Cut(solid->m_Shape,BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x,10,-1),gp_Dir(0,0,1)),3,12).Shape()).Shape();workspace.show();
            for(int i=0;i<4;++i)app.processEvents();scene=workspace.findChild<QGraphicsView*>()->scene();
            check(dimension(scene,0)->data(3).toBool(),"Hole invalidated unrelated dimension");
            expectNear(dimension(scene,0)->data(1).toDouble(),80,"Hole changed unrelated measurement");
        }
        workspace.ReloadFromDocument();scene=workspace.findChild<QGraphicsView*>()->scene();check(dimension(scene,0)->data(3).toBool(),"Hole remapping lost on reload");
        doc.GetObjects().clear();workspace.ReloadFromDocument();scene=workspace.findChild<QGraphicsView*>()->scene();
        check(!dimension(scene,0)->data(3).toBool(),"Deleted body retained a false measurement");
        std::cout<<"Drafting dimensions: geometry, 7 layouts, placement, drag, edit, reload, CAD association and PDF passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}


