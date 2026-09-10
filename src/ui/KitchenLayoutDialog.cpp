#include "KitchenLayoutDialog.h"
#include "CAlfaDoc.h"
#include "CAssembled.h"
#include "OpenGLViewport.h"
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QGridLayout>
#include "FurnitureMaterialFactory.h"
#include <QCheckBox>
#include <QTabWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPainter>
#include <set>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QSignalBlocker>
#include <algorithm>
#include <functional>
#include <cmath>
namespace {
const QStringList types{"Single door","Double door","2 drawers","3 drawers","4 drawers","Stove / oven","Refrigerator","Hood","Empty space","Blind corner"};
const QStringList styles{"Plain","Frame","Screen","Milled","Milano"};
QString rowName(const KitchenRow& row) {return QString("%1 row %2").arg(row.upper?"Upper":"Lower").arg(row.uid+1);}
QDoubleSpinBox* number(QFormLayout* form,const char* label,double value,double low,double high,const char* suffix=" mm") {
    auto* box=new QDoubleSpinBox;box->setRange(low,high);box->setValue(value);box->setDecimals(0);box->setSingleStep(10);box->setSuffix(suffix);form->addRow(label,box);return box;
}
QComboBox* choice(QFormLayout* form,const char* label,const QStringList& items,int value,bool inherit=false) {
    auto* box=new QComboBox;if(inherit)box->addItem("From kitchen",-1);
    for(int i=0;i<items.size();++i)box->addItem(items[i],i);
    box->setCurrentIndex(box->findData(value));form->addRow(label,box);return box;
}
QComboBox* materialChoice(QFormLayout* form,const char* label,double value,bool inherit) {
    auto* box=new QComboBox;if(inherit)box->addItem("From kitchen",-1.0);box->addItem("Automatic",0.0);
    if(auto* doc=GetAlfaDoc()){FurnitureMaterialFactory::EnsureStandardMaterials(*doc);for(const auto& m:doc->GetMaterials())box->addItem(QString::fromStdString(m.name),double(m.id));}
    if(box->findData(value)<0)box->addItem(QString("Material %1").arg(value),value);
    box->setCurrentIndex(box->findData(value));form->addRow(label,box);return box;
}
const char* materialLabels[]={"Carcass material","Facade material","Handles / legs material","Worktop material"};
const QStringList handles{"Modern","Classic","Knob","None"};
class KitchenPlan:public QGraphicsView {
public:
    std::function<void(int)> picked;
    std::function<void(int)> edited;
    int rowFilter=2;
    bool sceneMode=false;
    explicit KitchenPlan(QWidget* parent=nullptr):QGraphicsView(parent){setScene(new QGraphicsScene(this));setMinimumSize(320,260);setRenderHint(QPainter::Antialiasing);setBackgroundBrush(QColor(245,247,250));}
    void showLayout(const KitchenLayout& k,int selectedRow,int selectedModule) {
        scene()->clear();setBackgroundBrush(sceneMode?QColor(16,19,22):QColor(245,247,250));
        try {
            const auto places=k.Placements();
            for(size_t i=0;i<places.size();++i) {
                const auto& p=places[i];const auto& m=k.modules[i];const auto* row=k.Row(m.row);
                if((rowFilter==0&&row->upper)||(rowFilter==1&&!row->upper))continue;
                const bool active=m.row==selectedRow;QPolygonF polygon;
                for(const auto& c:p.corners)polygon<<QPointF(c[0],-c[1]);
                QColor color=active?QColor(170,209,240):QColor(218,224,230,100);
                if(m.type>=5&&m.type<=7)color=active?QColor(193,208,190):QColor(208,214,208,100);
                if(m.type==8)color=Qt::transparent;
                auto* item=scene()->addPolygon(polygon,QPen(m.uid==selectedModule?QColor(225,125,25):QColor(65,92,117),m.uid==selectedModule?8:3,row->upper?Qt::DashLine:Qt::SolidLine),color);
                item->setData(0,m.uid);item->setZValue(active?2:0);
                item->setToolTip(QString("%1: %2, %3 mm").arg(m.uid).arg(types[m.type]).arg(m.width));
                if(active || sceneMode) {
                    const bool separate=rowFilter==2;
                    auto* label=scene()->addSimpleText(QString("%1%2\n%3 mm").arg(row->upper?"U":"L").arg(m.uid).arg(m.width));
                    label->setBrush(sceneMode&&separate?QColor(225,233,240):QColor(25,40,55));
                    QPointF labelPosition=polygon.boundingRect().center();
                    if(separate){
                        const QPointF back=(polygon[0]+polygon[1])/2,front=(polygon[2]+polygon[3])/2;
                        QPointF normal=front-back;const double length=std::hypot(normal.x(),normal.y());
                        if(length>0)normal/=length;
                        labelPosition=row->upper?back-normal*140:front+normal*140;
                        auto* leader=scene()->addLine(QLineF(row->upper?back:front,row->upper?back-normal*70:front+normal*70),QPen(sceneMode?QColor(140,158,173):QColor(95,113,128),0));
                        leader->setZValue(4);leader->setAcceptedMouseButtons(Qt::NoButton);
                    }
                    label->setFont(QFont("Segoe UI",10));label->setFlag(QGraphicsItem::ItemIgnoresTransformations);label->setPos(labelPosition);
                    label->setTransform(QTransform::fromTranslate(-label->boundingRect().width()/2,-label->boundingRect().height()/2));label->setZValue(6);label->setData(0,m.uid);
                    auto* front=scene()->addLine(QLineF(polygon[2],polygon[3]),QPen(QColor(35,90,145),10));front->setZValue(3);front->setData(0,m.uid);
                }
            }
            auto bounds=scene()->itemsBoundingRect().adjusted(-180,-180,180,180);scene()->setSceneRect(bounds);fitInView(bounds,Qt::KeepAspectRatio);
        }catch(...){scene()->addText("Check row connections");}
    }
protected:
    void mousePressEvent(QMouseEvent* event) override {auto* item=itemAt(event->pos());if(item&&item->data(0).isValid()&&picked)picked(item->data(0).toInt());QGraphicsView::mousePressEvent(event);}
    void mouseDoubleClickEvent(QMouseEvent* event) override {auto* item=itemAt(event->pos());if(item&&item->data(0).isValid()&&edited){edited(item->data(0).toInt());event->accept();return;}QGraphicsView::mouseDoubleClickEvent(event);}
    void resizeEvent(QResizeEvent* event) override {QGraphicsView::resizeEvent(event);fitInView(scene()->sceneRect(),Qt::KeepAspectRatio);}
};
}
bool EditKitchenModuleDialog(QWidget* parent,KitchenLayout& value,int uid) {
    auto found=std::find_if(value.modules.begin(),value.modules.end(),[&](const KitchenModule& m){return m.uid==uid;});
    if(found==value.modules.end())return false;
    const auto original=*found;QDialog dialog(parent);dialog.setWindowTitle(QString("Module %1 - parameters and facades").arg(uid));
    auto* layout=new QVBoxLayout(&dialog);auto* tabs=new QTabWidget;layout->addWidget(tabs);
    auto* general=new QWidget;auto* form=new QFormLayout(general);tabs->addTab(general,"Cabinet");
    auto* finishes=new QWidget;auto* finishForm=new QFormLayout(finishes);tabs->addTab(finishes,"Materials");
    std::array<QComboBox*,4> materialBoxes;for(int i=0;i<4;++i)materialBoxes[i]=materialChoice(finishForm,materialLabels[i],original.materials[i],true);
    finishForm->addRow(new QLabel("From kitchen keeps this module linked to the common finish."));
    auto* type=new QComboBox;type->addItems(types);type->setCurrentIndex(original.type);form->addRow("Module",type);
    auto* width=number(form,"Width",original.width,200,1800);auto* height=number(form,"Height (0 = row)",original.height,0,2400);
    auto* depth=number(form,"Depth (0 = row)",original.depth,0,900);
    auto* style=choice(form,"Facade style",styles,original.facadeStyle,true);style->setObjectName("moduleFacadeStyle");
    auto* handle=choice(form,"Handles",handles,original.handleType,true);
    auto* feet=choice(form,"Legs",{"No","Yes"},original.addLegs,true);
    auto* thickness=number(form,"Panel thickness (0 = kitchen)",original.panelThickness,0,40);
    auto* glass=choice(form,"Showcase filling",{"Glass","Lattice","Muntin bars","Stained glass","None"},original.showcaseFill);
    auto* shelves=new QSpinBox;shelves->setRange(0,8);shelves->setValue(original.shelves);form->addRow("Shelves",shelves);
    auto* axis=choice(form,"Door axis",{"Vertical","Horizontal"},original.doorAxis);axis->setObjectName("moduleDoorAxis");
    auto* door=number(form,"Open doors",original.doorAngle,0,120," deg");
    auto* drawer=new QSpinBox;drawer->setRange(0,6);drawer->setValue(original.openDrawer);form->addRow("Open drawer (0 = closed)",drawer);
    auto* pullout=number(form,"Drawer extension",original.pullout,0,800);
    auto* ratios=new QGroupBox("Drawer front proportions, bottom to top");auto* ratioForm=new QFormLayout(ratios);tabs->addTab(ratios,"Drawers");
    auto* count=new QSpinBox;count->setRange(1,6);count->setValue(original.type>=2&&original.type<=4?value.Drawers(original):3);ratioForm->addRow("Drawer count",count);
    std::array<QDoubleSpinBox*,6> ratioBoxes;
    for(int i=0;i<6;++i){auto* box=new QDoubleSpinBox;box->setRange(0.1,10000);box->setSingleStep(0.1);box->setValue(original.drawerRatios[i]);ratioForm->addRow(QString("Drawer %1").arg(i+1),box);ratioBoxes[i]=box;}
    auto update=[&](){int t=type->currentIndex();bool drawers=t>=2&&t<=4;for(int i=2;i<=4;++i)type->setItemText(i,types[i]);if(drawers)type->setItemText(t,QString("%1 drawers").arg(count->value()));drawer->setMaximum(drawers?count->value():0);drawer->setEnabled(drawers);pullout->setEnabled(drawers);ratios->setEnabled(drawers);axis->setEnabled(t<=1||t==9||(t==7&&height->value()>0));door->setEnabled(t<=1||t==9||(t==7&&height->value()>0));shelves->setEnabled(t<=1||t==9);style->setEnabled(t<=4||t==9||(t==7&&height->value()>0));for(int i=0;i<6;++i)ratioBoxes[i]->setEnabled(drawers&&i<count->value());};
    QObject::connect(count,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&](int){update();});
    QObject::connect(type,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int t){if(t>=2&&t<=4)count->setValue(t);update();});QObject::connect(height,QOverload<double>::of(&QDoubleSpinBox::valueChanged),&dialog,[&](double){update();});update();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&](){auto candidate=value;auto index=size_t(found-value.modules.begin());auto& m=candidate.modules[index];
        m.type=type->currentIndex();m.width=width->value();m.height=height->value();m.depth=depth->value();m.facadeStyle=style->currentData().toInt();m.shelves=shelves->value();m.doorAxis=axis->currentData().toInt();m.doorAngle=door->value();m.openDrawer=drawer->value();m.pullout=pullout->value();
        m.handleType=handle->currentData().toInt();m.addLegs=feet->currentData().toInt();m.panelThickness=thickness->value();m.showcaseFill=glass->currentData().toInt();
        m.drawerCount=m.type>=2&&m.type<=4?count->value():0;
        for(int i=0;i<4;++i)m.materials[i]=materialBoxes[i]->currentData().toDouble();
        for(int i=0;i<6;++i)m.drawerRatios[i]=ratioBoxes[i]->value();std::string error;
        if(!candidate.Validate(error)){QMessageBox::warning(&dialog,"Module",QString::fromStdString(error));return;}
        value=std::move(candidate);dialog.accept();});
    return dialog.exec()==QDialog::Accepted;
}
bool EditKitchenLayoutDialog(QWidget* parent,KitchenLayout& value,bool creating,QWidget* sceneHost) {
    QSettings settings("Dom3D", "Dom3D_Pro");
    const QString settingsKey="KitchenLayoutEditor/";
    // Keep editor preferences separate from each kitchen's actual parameters.
    // Document values always win when editing an existing kitchen.
    static KitchenLayout lastCreated;
    static bool haveLastCreated=false;
    if(sceneHost&&!haveLastCreated){const auto stored=settings.value(settingsKey+"creationParameters").toMap();
        if(!stored.isEmpty()){std::vector<ParametricParameterValue> parameters;
            for(auto it=stored.begin();it!=stored.end();++it)parameters.push_back({it.key().toStdString(),it.value().toDouble()});
            auto candidate=KitchenLayout::Decode(parameters);std::string error;
            if(candidate.Validate(error)){lastCreated=std::move(candidate);haveLastCreated=true;}}}
    KitchenLayout working=value;
    if(creating&&sceneHost&&haveLastCreated) {
        working=lastCreated;
        for(auto& material:working.materials)material=0;
        for(auto& m:working.modules)for(auto& material:m.materials)material=-1;
    }
    if(working.upperOffset!=0){for(auto& row:working.rows)if(row.uid==1)row.x+=working.upperOffset;working.upperOffset=0;}
    QDialog dlg(parent);dlg.setWindowTitle(creating?"Kitchen Layout - create":"Kitchen Layout - edit");dlg.resize(sceneHost?520:1180,800);dlg.setObjectName("kitchenLayoutEditor");
    auto* layout=new QVBoxLayout(&dlg);auto* presetLine=new QHBoxLayout;
    presetLine->addWidget(new QLabel("Preset library:"));
    auto* preset=new QComboBox;preset->addItems({"Straight 1 - stove and refrigerator","Straight 2 - drawers","Straight 3 - chimney hood","Corner kitchen - connected rows"});
    auto* load=new QPushButton("Load preset");presetLine->addWidget(preset,1);presetLine->addWidget(load);layout->addLayout(presetLine);
    auto* editorTools=new QHBoxLayout;layout->addLayout(editorTools);
    auto* stage=new QComboBox;stage->setObjectName("kitchenStage");stage->addItems({"1. Placement and dimensions","2. Modules and facades"});editorTools->addWidget(stage,1);
    auto* viewMode=new QComboBox;viewMode->setObjectName("kitchenView");viewMode->addItems({"Plan","3D"});editorTools->addWidget(viewMode);
    auto* rowFilter=new QComboBox;rowFilter->setObjectName("kitchenRowFilter");rowFilter->addItems({"Lower","Upper","Together"});rowFilter->setCurrentIndex(2);editorTools->addWidget(rowFilter);
    auto* commonToggle=new QCheckBox("Common kitchen parameters");layout->addWidget(commonToggle);
    auto* commonTabs=new QTabWidget;layout->addWidget(commonTabs);commonTabs->setVisible(!sceneHost);commonToggle->setChecked(!sceneHost||settings.value(settingsKey+"commonExpanded",false).toBool());commonTabs->setVisible(commonToggle->isChecked());
    QObject::connect(commonToggle,&QCheckBox::toggled,commonTabs,&QWidget::setVisible);
    auto* dimensions=new QWidget;commonTabs->addTab(dimensions,"Kitchen dimensions");
    auto* finishes=new QWidget;commonTabs->addTab(finishes,"Common facades and materials");
    auto* finishLayout=new QHBoxLayout(finishes);auto* finishForm=new QFormLayout;auto* materialForm=new QFormLayout;finishLayout->addLayout(finishForm);finishLayout->addLayout(materialForm);
    auto* commonStyle=choice(finishForm,"Facade style",styles,working.facadeStyle);commonStyle->setObjectName("kitchenFacadeStyle");
    auto* commonHandle=choice(finishForm,"Handles",handles,working.handleType);
    auto* thickness=number(finishForm,"Panel thickness",working.panelThickness,5,40);
    auto* makeLegs=new QCheckBox("Add legs to lower cabinets");makeLegs->setChecked(working.makeLegs);finishForm->addRow(makeLegs);
    std::array<QComboBox*,4> materialBoxes;for(int i=0;i<4;++i)materialBoxes[i]=materialChoice(materialForm,materialLabels[i],working.materials[i],false);
    auto* showPlan=new QCheckBox("Linked 2D plans in scene (double-click a module to edit)");showPlan->setChecked(working.showPlan);layout->addWidget(showPlan);
    auto* common=new QHBoxLayout(dimensions);auto* f1=new QFormLayout;auto* f2=new QFormLayout;auto* f3=new QFormLayout;common->addLayout(f1);common->addLayout(f2);common->addLayout(f3);
    auto* baseHeight=number(f1,"Base height",working.baseHeight,400,1200);auto* baseDepth=number(f1,"Base depth",working.baseDepth,300,900);
    auto* upperHeight=number(f2,"Upper height",working.upperHeight,200,1200);auto* upperDepth=number(f2,"Upper depth",working.upperDepth,200,600);
    auto* recess=number(f3,"Plinth setback from facade",working.plinthRecess,0,150);
    auto* legs=number(f3,"Plinth",working.legs,50,250);auto* gap=number(f3,"Gap above worktop",working.gap,200,1200);auto* top=number(f3,"Worktop thickness",working.top,10,100);
    auto* splitter=new QSplitter;layout->addWidget(splitter,1);auto* rowPanel=new QWidget;auto* rowLayout=new QVBoxLayout(rowPanel);
    auto* rowList=new QListWidget;rowList->setObjectName("kitchenRows");rowList->setMinimumHeight(0);rowLayout->addWidget(new QLabel("Rows - edited independently"));rowLayout->addWidget(rowList);
    auto* addLower=new QPushButton("Add lower row");auto* addUpper=new QPushButton("Add upper row");auto* connectRow=new QPushButton("Add corner row");auto* removeRow=new QPushButton("Remove row");
    auto* rowButtons=new QHBoxLayout;rowButtons->addWidget(addLower);rowButtons->addWidget(addUpper);rowLayout->addLayout(rowButtons);
    auto* cornerButtons=new QHBoxLayout;cornerButtons->addWidget(connectRow);cornerButtons->addWidget(removeRow);rowLayout->addLayout(cornerButtons);
    auto* rowForm=new QFormLayout;rowLayout->addLayout(rowForm);
    auto* rowX=number(rowForm,"X offset",0,-20000,20000);auto* rowY=number(rowForm,"Y offset",0,-20000,20000);auto* rowAngle=number(rowForm,"Direction / turn",0,-360,360," deg");
    auto* parentRow=new QComboBox;rowForm->addRow("Connected to",parentRow);
    auto* rowHint=new QLabel("A connected row follows its parent's end. X/Y are additional offsets; direction is a relative turn.");rowHint->setWordWrap(true);rowLayout->addWidget(rowHint);splitter->addWidget(rowPanel);
    auto* plan=new KitchenPlan;splitter->addWidget(plan);splitter->setStretchFactor(1,1);splitter->setSizes({320,820});
    auto* table=new QTableWidget(0,5);table->setObjectName("kitchenModules");table->setHorizontalHeaderLabels({"Module","Width (mm)","Height (0 = row)","Depth (0 = row)","Facades"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setMaximumHeight(240);layout->addWidget(table);
    auto* actions=new QGridLayout;layout->addLayout(actions);auto* add=new QPushButton("Add module");auto* remove=new QPushButton("Remove module");auto* up=new QPushButton("Move left");auto* down=new QPushButton("Move right");auto* properties=new QPushButton("Module parameters / facades");
    {int i=0;for(auto* b:{add,remove,up,down,properties}){actions->addWidget(b,i/3,i%3);++i;}}
    auto* status=new QLabel;status->setWordWrap(true);layout->addWidget(status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    int selectedRow=working.rows.empty()?-1:working.rows.front().uid,selectedModule=0;
    if(sceneHost){selectedRow=settings.value(settingsKey+"row",selectedRow).toInt();if(!working.Row(selectedRow))selectedRow=working.rows.empty()?-1:working.rows.front().uid;
        selectedModule=settings.value(settingsKey+"module",0).toInt();
        stage->setCurrentIndex(std::clamp(settings.value(settingsKey+"stage",0).toInt(),0,1));
        rowFilter->setCurrentIndex(std::clamp(settings.value(settingsKey+"filter",2).toInt(),0,2));
        viewMode->setCurrentIndex(std::clamp(settings.value(settingsKey+"view",0).toInt(),0,1));}
    // A child tool window remains interactive during the modal edit transaction.
    // It occupies the scene area, while the document is untouched until OK.
    auto* sceneWindow=new QWidget(&dlg,Qt::Tool|Qt::FramelessWindowHint);
    sceneWindow->setObjectName("kitchenEditorScene");
    auto* sceneLayout=new QVBoxLayout(sceneWindow);sceneLayout->setContentsMargins(0,0,0,0);
    auto* sceneStack=new QStackedWidget;sceneLayout->addWidget(sceneStack);
    auto* sceneStatus=new QLabel("Kitchen working plan");sceneStatus->setStyleSheet("background:#20262c;color:white;padding:6px");sceneLayout->addWidget(sceneStatus);
    CAlfaDoc* sourceDoc=GetAlfaDoc();
    auto previewDoc=std::make_unique<CAlfaDoc>();SetAlfaDoc(sourceDoc);
    if(sourceDoc)previewDoc->GetMaterials()=sourceDoc->GetMaterials();
    auto* preview=new OpenGLViewport;preview->setObjectName("kitchen3DPreview");preview->SetDocument(previewDoc.get());preview->SetTool(ToolMode::Select);previewDoc->SetGroupInteractionEnabled(false);
    sceneStack->addWidget(preview);
    bool previewDirty=true,previewBuilt=false,syncing=false;
    std::string previewParameters;
    std::function<void()> updateScene;
    QTimer previewTimer;previewTimer.setSingleShot(true);previewTimer.setInterval(500);
    QTimer positionTimer;positionTimer.setInterval(150);
    auto positionScene=[&](){if(!sceneHost||!showPlan->isChecked())return;
        QRect area(sceneHost->mapToGlobal(QPoint(0,0)),sceneHost->size());
        const auto panel=dlg.frameGeometry();if(area.intersects(panel))area.setLeft(std::min(panel.right()+8,area.right()-320));
        if(area.width()<200||area.height()<200)return;
        if(sceneWindow->geometry()!=area)sceneWindow->setGeometry(area);
    };
    QObject::connect(&positionTimer,&QTimer::timeout,&dlg,positionScene);
    auto highlightPreview=[&](){if(!previewBuilt)return;syncing=true;previewDoc->ClearSelection();
        for(const auto& object:previewDoc->GetObjects())if(object&&object->GetGroupName()=="Kitchen group module "+std::to_string(selectedModule)) {previewDoc->SelectObjectById(object->m_id);break;}
        preview->update();syncing=false;};
    auto rebuildPreview=[&](){if(!sceneHost||!showPlan->isChecked()||viewMode->currentIndex()!=1)return;
        if(previewDirty){std::string error;if(!working.Validate(error)){sceneStatus->setText(QString::fromStdString(error));return;}
            auto draft=working;draft.showPlan=false;previewDoc->GetObjects().clear();previewBuilt=false;
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const bool built=BuildKitchenLayout(*previewDoc,draft,nullptr,error);SetAlfaDoc(sourceDoc);
            QApplication::restoreOverrideCursor();
            if(!built){sceneStatus->setText(QString::fromStdString(error));return;}
            previewBuilt=true;previewDirty=false;previewDoc->SetGroupInteractionEnabled(false);
            preview->FitToDocument();}
        // Filtering changes visibility only in this disposable preview document.
        for(const auto& object:previewDoc->GetObjects())if(object)object->CAlfaObject::SetVisible(true);
        for(const auto& row:working.rows)if((rowFilter->currentIndex()==0&&row.upper)||(rowFilter->currentIndex()==1&&!row.upper))
            for(const auto& object:previewDoc->GetObjects())if(object&&object->GetGroupName()=="Kitchen group row "+std::to_string(row.uid))previewDoc->SetObjectVisibility(object->m_id,false);
        highlightPreview();sceneStatus->setText("3D preview - changes apply on OK");
    };
    QObject::connect(&previewTimer,&QTimer::timeout,&dlg,rebuildPreview);
    updateScene=[&](){const bool linked=sceneHost&&showPlan->isChecked();
        common->setDirection(linked?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
        finishLayout->setDirection(linked?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
        if(linked){if(plan->parentWidget()!=sceneStack){sceneStack->addWidget(plan);plan->show();}
            plan->sceneMode=true;sceneStack->setCurrentWidget(viewMode->currentIndex()==0?static_cast<QWidget*>(plan):preview);
            positionScene();sceneWindow->show();positionTimer.start();
            if(viewMode->currentIndex()==1)previewTimer.start();else sceneStatus->setText("Plan: L = lower, U = upper. Click a module or its label to select it.");
        }else{positionTimer.stop();sceneWindow->hide();if(plan->parentWidget()!=splitter){splitter->addWidget(plan);plan->show();}plan->sceneMode=false;}
        layout->setStretchFactor(splitter,linked?0:1);
        layout->setStretchFactor(table,linked?1:0);
        table->setMaximumHeight(linked?QWIDGETSIZE_MAX:240);
        rowPanel->setSizePolicy(QSizePolicy::Preferred,linked?QSizePolicy::Maximum:QSizePolicy::Preferred);
        viewMode->setEnabled(linked);rowFilter->setEnabled(true);
        plan->rowFilter=rowFilter->currentIndex();
        for(int c:{2,3,4})table->setColumnHidden(c,stage->currentIndex()==0||linked);
        properties->setVisible(stage->currentIndex()==1);
        plan->showLayout(working,selectedRow,selectedModule);
    };
    bool filling=false;
    auto rowPtr=[&]()->KitchenRow*{for(auto& row:working.rows)if(row.uid==selectedRow)return &row;return nullptr;};
    auto readCommon=[&](){working.baseHeight=baseHeight->value();working.baseDepth=baseDepth->value();working.upperHeight=upperHeight->value();working.upperDepth=upperDepth->value();working.legs=legs->value();working.gap=gap->value();working.top=top->value();working.plinthRecess=recess->value();working.showPlan=showPlan->isChecked();working.facadeStyle=commonStyle->currentData().toInt();working.handleType=commonHandle->currentData().toInt();working.panelThickness=thickness->value();working.makeLegs=makeLegs->isChecked();for(int i=0;i<4;++i)working.materials[i]=materialBoxes[i]->currentData().toDouble();};
    auto refreshPlan=[&](){if(filling)return;readCommon();std::string encoded;for(const auto& param:working.Encode())encoded+=param.id+"="+std::to_string(param.value)+";";
        if(encoded!=previewParameters){previewDirty=true;previewParameters=std::move(encoded);}
        updateScene();highlightPreview();std::string error;
        if(working.Validate(error)){double length=selectedRow>=0?working.Frame(selectedRow).length:0;status->setText(QString("Selected row: %1 mm. Click a module on the plan or double-click its table row to edit. Solid lines: lower; dashed: upper.").arg(length));}
        else status->setText(QString::fromStdString(error));};
    std::function<void()> populate;
    auto readTable=[&](){if(filling)return;for(int r=0;r<table->rowCount();++r){int uid=table->item(r,0)->data(Qt::UserRole).toInt();for(auto& m:working.modules)if(m.uid==uid){int nextType=static_cast<QComboBox*>(table->cellWidget(r,0))->currentIndex();if(nextType!=m.type)m.drawerCount=0;m.type=nextType;m.width=static_cast<QDoubleSpinBox*>(table->cellWidget(r,1))->value();m.height=static_cast<QDoubleSpinBox*>(table->cellWidget(r,2))->value();m.depth=static_cast<QDoubleSpinBox*>(table->cellWidget(r,3))->value();m.facadeStyle=static_cast<QComboBox*>(table->cellWidget(r,4))->currentData().toInt();table->cellWidget(r,4)->setEnabled(m.type<=4||m.type==9||(m.type==7&&m.height>0));if(m.type<2||m.type>4)m.openDrawer=0;else m.openDrawer=std::min(m.openDrawer,working.Drawers(m));}}
        refreshPlan();};
    populate=[&](){filling=true;
        auto* activeRow=rowPtr();const int filter=rowFilter->currentIndex();
        if(!activeRow||(filter==0&&activeRow->upper)||(filter==1&&!activeRow->upper)) {
            selectedRow=-1;for(const auto& r:working.rows)if(filter==2||r.upper==(filter==1)){selectedRow=r.uid;break;}}
        bool foundSelection=false;for(const auto& m:working.modules)if(m.uid==selectedModule&&m.row==selectedRow)foundSelection=true;
        if(!foundSelection){selectedModule=0;for(const auto& m:working.modules)if(m.row==selectedRow){selectedModule=m.uid;break;}}
rowList->clear();for(auto& row:working.rows){if((rowFilter->currentIndex()==0&&row.upper)||(rowFilter->currentIndex()==1&&!row.upper))continue;auto* item=new QListWidgetItem(rowName(row),rowList);item->setData(Qt::UserRole,row.uid);if(row.uid==selectedRow)rowList->setCurrentItem(item);}
        const int rowHeight=std::max(rowList->sizeHintForRow(0),rowList->fontMetrics().height()+10);
        rowList->setFixedHeight(std::clamp(rowList->count(),1,5)*rowHeight+2*rowList->frameWidth()+4);
        auto* row=rowPtr();parentRow->clear();parentRow->addItem("Independent",-1);
        if(row){rowX->setValue(row->x);rowY->setValue(row->y);rowAngle->setValue(row->angle);
            for(const auto& candidate:working.rows)if(candidate.uid!=row->uid&&candidate.upper==row->upper){std::set<int> chain;const KitchenRow* ancestor=&candidate;bool cycle=false;while(ancestor&&ancestor->parent>=0&&chain.insert(ancestor->uid).second){if(ancestor->parent==row->uid){cycle=true;break;}ancestor=working.Row(ancestor->parent);}if(!cycle)parentRow->addItem(rowName(candidate),candidate.uid);}
            parentRow->setCurrentIndex(parentRow->findData(row->parent));}
        for(auto* w:std::array<QWidget*,4>{rowX,rowY,rowAngle,parentRow})w->setEnabled(row!=nullptr);
        table->setRowCount(0);for(auto& m:working.modules)if(m.row==selectedRow){int r=table->rowCount();table->insertRow(r);auto* item=new QTableWidgetItem;item->setData(Qt::UserRole,m.uid);table->setItem(r,0,item);table->setVerticalHeaderItem(r,new QTableWidgetItem(QString::number(m.uid)));
            auto* type=new QComboBox;type->addItems(types);type->setCurrentIndex(m.type);if(m.type>=2&&m.type<=4)type->setItemText(m.type,QString("%1 drawers").arg(working.Drawers(m)));table->setCellWidget(r,0,type);
            for(int c=1;c<=3;++c){auto* n=new QDoubleSpinBox;n->setDecimals(0);n->setSingleStep(50);n->setRange(c==1?200:0,c==1?1800:(c==2?2400:900));n->setValue(c==1?m.width:(c==2?m.height:m.depth));table->setCellWidget(r,c,n);QObject::connect(n,QOverload<double>::of(&QDoubleSpinBox::valueChanged),&dlg,[&](double){readTable();});}
            auto* style=new QComboBox;style->addItem("From kitchen",-1);for(int i=0;i<styles.size();++i)style->addItem(styles[i],i);style->setCurrentIndex(style->findData(m.facadeStyle));style->setEnabled(m.type<=4||m.type==9||(m.type==7&&m.height>0));table->setCellWidget(r,4,style);
            QObject::connect(type,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int){readTable();});QObject::connect(style,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int){readTable();});
            if(m.uid==selectedModule)table->selectRow(r);
        }
        filling=false;refreshPlan();};
    QObject::connect(rowList,&QListWidget::currentRowChanged,&dlg,[&](int r){if(filling||r<0)return;selectedRow=rowList->item(r)->data(Qt::UserRole).toInt();selectedModule=0;populate();});
    QObject::connect(table,&QTableWidget::itemSelectionChanged,&dlg,[&](){if(filling)return;int r=table->currentRow();if(r>=0)selectedModule=table->item(r,0)->data(Qt::UserRole).toInt();refreshPlan();});
    auto edit=[&](){readTable();if(selectedModule&&EditKitchenModuleDialog(&dlg,working,selectedModule))populate();};
    QObject::connect(properties,&QPushButton::clicked,&dlg,edit);QObject::connect(table,&QTableWidget::cellDoubleClicked,&dlg,[&](int,int){edit();});
    plan->picked=[&](int uid){for(const auto& m:working.modules)if(m.uid==uid){selectedRow=m.row;selectedModule=uid;populate();break;}};
    plan->edited=[&](int uid){plan->picked(uid);edit();};
    for(auto* size:{baseHeight,baseDepth,upperHeight,upperDepth,legs,gap,top,thickness,recess})QObject::connect(size,QOverload<double>::of(&QDoubleSpinBox::valueChanged),&dlg,[&](double){refreshPlan();});
    for(auto* box:{commonStyle,commonHandle,materialBoxes[0],materialBoxes[1],materialBoxes[2],materialBoxes[3]})QObject::connect(box,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int){refreshPlan();});
    QObject::connect(makeLegs,&QCheckBox::toggled,&dlg,[&](bool){refreshPlan();});
    QObject::connect(showPlan,&QCheckBox::toggled,&dlg,[&](bool){refreshPlan();dlg.resize(showPlan->isChecked()&&sceneHost?520:1000,dlg.height());});
    QObject::connect(stage,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int){refreshPlan();});
    QObject::connect(viewMode,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int){refreshPlan();});
    QObject::connect(rowFilter,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int filter){
        if(auto* row=rowPtr())if((filter==0&&row->upper)||(filter==1&&!row->upper)){
            selectedRow=-1;selectedModule=0;for(const auto& r:working.rows)if(filter==2||r.upper==(filter==1)){selectedRow=r.uid;break;}}
        populate();});
    QObject::connect(preview,&OpenGLViewport::SelectionChanged,&dlg,[&](){if(syncing)return;
        if(auto* object=previewDoc->GetSelectedObject()){int uid=FindKitchenModule(*previewDoc,object->m_id);if(uid)plan->picked(uid);}});
    auto rowChanged=[&](){if(filling)return;if(auto* row=rowPtr()){row->x=rowX->value();row->y=rowY->value();row->angle=rowAngle->value();row->parent=parentRow->currentData().toInt();}refreshPlan();};
    for(auto* n:{rowX,rowY,rowAngle})QObject::connect(n,QOverload<double>::of(&QDoubleSpinBox::valueChanged),&dlg,[&](double){rowChanged();});
    QObject::connect(parentRow,QOverload<int>::of(&QComboBox::currentIndexChanged),&dlg,[&](int){rowChanged();});
    auto nextModule=[&](){int uid=0;for(const auto& m:working.modules)uid=std::max(uid,m.uid);return uid+1;};
    auto newRow=[&](bool upper,bool connected){readTable();int uid=0;for(const auto& row:working.rows)uid=std::max(uid,row.uid+1);KitchenRow row;row.uid=uid;row.upper=upper;
        if(connected&&rowPtr()) {row.upper=rowPtr()->upper;row.parent=selectedRow;row.angle=-90;
            KitchenModule* last=nullptr;for(auto& m:working.modules)if(m.row==selectedRow)last=&m;
            if(!last||last->type!=9){KitchenModule corner;corner.uid=nextModule();corner.row=selectedRow;corner.type=9;corner.width=std::max(900.0,(row.upper?working.upperDepth:working.baseDepth)+300);working.modules.push_back(corner);}
        }else row.y=-1800*uid;
        working.rows.push_back(row);KitchenModule m;m.uid=nextModule();m.row=uid;working.modules.push_back(m);selectedRow=uid;selectedModule=m.uid;populate();};
    QObject::connect(addLower,&QPushButton::clicked,&dlg,[&](){newRow(false,false);});QObject::connect(addUpper,&QPushButton::clicked,&dlg,[&](){newRow(true,false);});QObject::connect(connectRow,&QPushButton::clicked,&dlg,[&](){newRow(false,true);});
    QObject::connect(removeRow,&QPushButton::clicked,&dlg,[&](){if(!rowPtr())return;readTable();for(auto& row:working.rows)if(row.parent==selectedRow){try{auto f=working.Frame(row.uid);row.parent=-1;row.x=f.x;row.y=f.y;row.angle=f.angle;}catch(...){row.parent=-1;}}
        working.modules.erase(std::remove_if(working.modules.begin(),working.modules.end(),[&](const KitchenModule& m){return m.row==selectedRow;}),working.modules.end());working.rows.erase(std::remove_if(working.rows.begin(),working.rows.end(),[&](const KitchenRow& r){return r.uid==selectedRow;}),working.rows.end());selectedRow=working.rows.empty()?-1:working.rows.front().uid;selectedModule=0;populate();});
    QObject::connect(add,&QPushButton::clicked,&dlg,[&](){if(!rowPtr())return;readTable();KitchenModule m;m.uid=nextModule();m.row=selectedRow;auto it=std::find_if(working.modules.begin(),working.modules.end(),[&](const KitchenModule& a){return a.uid==selectedModule;});if(it!=working.modules.end())++it;working.modules.insert(it,m);selectedModule=m.uid;populate();});
    QObject::connect(remove,&QPushButton::clicked,&dlg,[&](){readTable();working.modules.erase(std::remove_if(working.modules.begin(),working.modules.end(),[&](const KitchenModule& m){return m.uid==selectedModule;}),working.modules.end());selectedModule=0;populate();});
    for(auto* button:{up,down})QObject::connect(button,&QPushButton::clicked,&dlg,[&,button](){readTable();int r=-1;for(size_t i=0;i<working.modules.size();++i)if(working.modules[i].uid==selectedModule)r=int(i);if(r<0)return;int step=button==up?-1:1;int t=r+step;while(t>=0&&t<int(working.modules.size())&&working.modules[t].row!=selectedRow)t+=step;if(t>=0&&t<int(working.modules.size()))std::swap(working.modules[r],working.modules[t]);populate();});
    QObject::connect(load,&QPushButton::clicked,&dlg,[&](){int next=nextModule();working=KitchenLayout::Preset(preset->currentIndex());for(auto& m:working.modules)m.uid+=next;filling=true;baseHeight->setValue(working.baseHeight);baseDepth->setValue(working.baseDepth);upperHeight->setValue(working.upperHeight);upperDepth->setValue(working.upperDepth);legs->setValue(working.legs);gap->setValue(working.gap);top->setValue(working.top);recess->setValue(working.plinthRecess);showPlan->setChecked(working.showPlan);commonStyle->setCurrentIndex(working.facadeStyle);commonHandle->setCurrentIndex(working.handleType);thickness->setValue(working.panelThickness);makeLegs->setChecked(working.makeLegs);for(int i=0;i<4;++i)materialBoxes[i]->setCurrentIndex(materialBoxes[i]->findData(working.materials[i]));filling=false;selectedRow=working.rows.front().uid;selectedModule=0;populate();});
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dlg,[&](){readTable();std::string error;if(!working.Validate(error)){QMessageBox::warning(&dlg,"Kitchen Layout",QString::fromStdString(error));return;}value=working;dlg.accept();});
    if(sceneHost){dlg.restoreGeometry(settings.value(settingsKey+"geometry").toByteArray());
        if(settings.value(settingsKey+"geometry").toByteArray().isEmpty())dlg.move(parent->mapToGlobal(QPoint(8,80)));}
    populate();const bool accepted=dlg.exec()==QDialog::Accepted;
    previewTimer.stop();positionTimer.stop();sceneWindow->hide();
    if(sceneHost){settings.setValue(settingsKey+"geometry",dlg.saveGeometry());settings.setValue(settingsKey+"row",selectedRow);settings.setValue(settingsKey+"module",selectedModule);
        settings.setValue(settingsKey+"stage",stage->currentIndex());settings.setValue(settingsKey+"filter",rowFilter->currentIndex());settings.setValue(settingsKey+"view",viewMode->currentIndex());
        settings.setValue(settingsKey+"commonExpanded",commonToggle->isChecked());
        if(accepted&&creating){lastCreated=value;haveLastCreated=true;QVariantMap parameters;
            for(const auto& parameter:value.Encode())parameters[QString::fromStdString(parameter.id)]=parameter.value;
            settings.setValue(settingsKey+"creationParameters",parameters);}}
    delete sceneWindow; // GL viewport must release its document before the preview document dies.
    SetAlfaDoc(sourceDoc);
    return accepted;
}
