// Included inside DraftingScene: association, placement and editing of dimensions.
struct DimensionFeature {
    drafting::Reference reference;
    drafting::Geometry geometry;
    QPolygonF samples;
};
QVector<DimensionFeature> dimension_features_;
drafting::Dimension pending_dimension_;
QGraphicsItem* dimension_preview_ = nullptr;
QGraphicsItem* dimension_snap_ = nullptr;

bool IsDimensionTool() const {
    using T=DraftingWorkspace::Tool;
    return tool_==T::Dimension || tool_>=T::HorizontalDimension;
}
drafting::DimensionKind DimensionKind() const {
    using T=DraftingWorkspace::Tool;
    return tool_==T::Dimension?drafting::DimensionKind::Parallel
        :drafting::DimensionKind(int(tool_)-int(T::HorizontalDimension));
}
double SnapTolerance() const {
    return views().isEmpty()?3.0:9.0/std::max(.01,std::abs(views().front()->transform().m11()));
}
static double DimDot(QPointF a,QPointF b) {return a.x()*b.x()+a.y()*b.y();}
QPointF ProjectDimensionPoint(const Primitive& view,const gp_Pnt& point) const {
    gp_Dir normal(0,-1,0),x(1,0,0);
    if(view.projection=="top")normal=gp_Dir(0,0,1);
    if(view.projection=="right") {normal=gp_Dir(1,0,0);x=gp_Dir(0,1,0);}
    if(view.projection=="isometric") {normal=gp_Dir(1,-1,1);x=gp_Dir(1,1,0);}
    const gp_Ax2 axes(gp_Pnt(0,0,0),normal,x);
    const gp_Dir y=axes.YDirection();
    return view.p1+QPointF((point.X()*x.X()+point.Y()*x.Y()+point.Z()*x.Z()-view.view_center.x())/view.view_scale,
        -(point.X()*y.X()+point.Y()*y.Y()+point.Z()*y.Z()-view.view_center.y())/view.view_scale);
}
DimensionFeature ModelDimensionFeature(const Primitive& view,const CSolid& solid,
                                      const TopoDS_Edge& edge,int index,int count) const {
    DimensionFeature f;
    BRepAdaptor_Curve curve(edge);
    const double first=curve.FirstParameter(),last=curve.LastParameter();
    if(!std::isfinite(first)||!std::isfinite(last)||view.view_scale<=0)return f;
    f.reference.primitive=view.id;f.reference.body=solid.m_id;f.reference.edge=index;
    f.reference.edge_count=count;f.reference.curve_type=int(curve.GetType());
    for(int i=0;i<9;++i){const auto p=curve.Value(first+(last-first)*i/8.);f.reference.edge_points.push_back({p.X(),p.Y(),p.Z()});}
    const int samples=curve.GetType()==GeomAbs_Line?2:65;
    for(int i=0;i<samples;++i)f.samples.push_back(ProjectDimensionPoint(view,curve.Value(first+(last-first)*i/(samples-1))));
    auto& g=f.geometry;g.valid=true;g.scale=view.view_scale;g.a=f.samples.front();g.b=f.samples.back();
    g.point=g.a;g.line=curve.GetType()==GeomAbs_Line&&QLineF(g.a,g.b).length()>1.e-7;
    g.circle=drafting::FitCircle(f.samples,g.center,g.radius);
    g.arc=g.circle && QLineF(g.a,g.b).length()>1.e-5;
    if(curve.GetType()==GeomAbs_Circle)g.center=ProjectDimensionPoint(view,curve.Circle().Location());
    return f;
}
drafting::Geometry ResolveDimensionReference(const drafting::Reference& ref) const {
    drafting::Geometry g;
    const Primitive* p=FindPrimitive(ref.primitive);
    if(!p)return g;
    if(ref.body) {
        if(!document_||p->type!="model_view")return g;
        for(const auto& object:document_->GetObjects()) {
            const auto* solid=object?dynamic_cast<const CSolid*>(object.get()):nullptr;
            if(!solid||solid->m_id!=ref.body||!solid->IsVisible()||solid->m_Shape.IsNull())continue;
            TopTools_IndexedMapOfShape edges;TopExp::MapShapes(solid->m_Shape,TopAbs_EDGE,edges);
            auto resolved=ref;if(!drafting::ResolveEdge(resolved,edges))return g;
            const TopoDS_Edge edge=TopoDS::Edge(edges(resolved.edge));BRepAdaptor_Curve curve(edge);
            g=ModelDimensionFeature(*p,*solid,edge,resolved.edge,edges.Extent()).geometry;
            g.point=resolved.anchor==1 && curve.GetType()==GeomAbs_Circle ? g.center
                :ProjectDimensionPoint(*p,curve.Value(curve.FirstParameter()+(curve.LastParameter()-curve.FirstParameter())*resolved.parameter));
            return g;
        }
        return g;
    }
    g.scale=data_->scale;g.point=AnchorPoint(*p,ref.anchor);g.valid=true;
    if(p->type=="line") {g.a=p->p1;g.b=p->p2;g.line=true;}
    else if(p->type=="rectangle") {
        const QRectF r=QRectF(p->p1,p->p2).normalized();
        const QPointF corners[]={r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()};
        g.a=corners[std::clamp(ref.anchor,0,3)];g.b=corners[(std::clamp(ref.anchor,0,3)+1)%4];g.line=true;
    } else if(p->type=="ellipse") {
        const QRectF r=QRectF(p->p1,p->p2).normalized();g.center=r.center();g.radius=r.width()/2;
        g.circle=g.radius>1.e-6&&std::abs(r.width()-r.height())<1.e-5;
    } else if(p->type!="model_view")g.valid=false;
    if(ref.whole_edge&&g.line)g.point=g.a+(g.b-g.a)*ref.parameter;
    return g;
}
void BuildDimensionFeatures() {
    dimension_features_.clear();
    // Upgrade still-valid legacy references before the next model edit, and
    // persist remapped indices/parameters after a topology change.
    if(document_)for(auto& p:data_->primitives)if(p.has_dimension)for(auto& ref:p.dimension.refs)if(ref.body) {
        for(const auto& object:document_->GetObjects()) {
            const auto* solid=object?dynamic_cast<const CSolid*>(object.get()):nullptr;
            if(!solid || solid->m_id!=ref.body || solid->m_Shape.IsNull())continue;
            try {TopTools_IndexedMapOfShape edges;TopExp::MapShapes(solid->m_Shape,TopAbs_EDGE,edges);drafting::ResolveEdge(ref,edges);}
            catch(const Standard_Failure&) {}
            break;
        }
    }
    for(const Primitive& p:data_->primitives) {
        if(p.type=="model_view"&&document_) {
            for(const auto& object:document_->GetObjects()) {
                const auto* solid=object?dynamic_cast<const CSolid*>(object.get()):nullptr;
                if(!solid||!solid->IsVisible()||solid->m_Shape.IsNull())continue;
                TopTools_IndexedMapOfShape edges;TopExp::MapShapes(solid->m_Shape,TopAbs_EDGE,edges);
                for(int i=1;i<=edges.Extent();++i) {
                    try {auto f=ModelDimensionFeature(p,*solid,TopoDS::Edge(edges(i)),i,edges.Extent());
                        if(f.geometry.valid)dimension_features_.push_back(f);
                    } catch(const Standard_Failure&) {}
                }
            }
        } else if(p.type=="line"||p.type=="rectangle"||p.type=="ellipse") {
            const int count=p.type=="rectangle"?4:1;
            for(int i=0;i<count;++i) {
                DimensionFeature f;f.reference.primitive=p.id;f.reference.anchor=i;
                f.geometry=ResolveDimensionReference(f.reference);
                if(f.geometry.line)f.samples=QPolygonF{f.geometry.a,f.geometry.b};
                else {
                    const QRectF r=QRectF(p.p1,p.p2).normalized();
                    for(int k=0;k<=64;++k) {double t=2*3.14159265358979323846*k/64;
                        f.samples.push_back(r.center()+QPointF(r.width()/2*std::cos(t),r.height()/2*std::sin(t)));}
                }
                dimension_features_.push_back(f);
            }
        }
    }
}
bool DimensionSnap(QPointF position,bool edge,bool circle,drafting::Reference& result,QPointF& snapped) const {
    double best=SnapTolerance();bool found=false;
    for(const auto& feature:dimension_features_) {
        if(circle&&!feature.geometry.circle)continue;
        if(edge&&!circle&&!feature.geometry.line)continue;
        const auto consider=[&](QPointF point,double t,int anchor) {
            const double distance=QLineF(position,point).length();
            if(distance>=best)return;
            best=distance;found=true;snapped=point;result=feature.reference;
            result.parameter=t;result.whole_edge=edge;result.anchor=anchor;
        };
        if(edge) {
            for(int i=1;i<feature.samples.size();++i) {
                QPointF a=feature.samples[i-1],v=feature.samples[i]-a;double vv=DimDot(v,v);
                if(vv<1.e-12)continue;
                double t=std::clamp(DimDot(position-a,v)/vv,0.0,1.0);
                consider(a+v*t,(i-1+t)/(feature.samples.size()-1),feature.reference.anchor);
            }
        } else if(feature.reference.body) {
            const int count=feature.samples.size()>2?5:3;
            for(int i=0;i<count;++i) {
                const double t=double(i)/(count-1),sample=t*(feature.samples.size()-1);
                const int left=std::min(int(sample),int(feature.samples.size())-2);
                consider(feature.samples[left]+(feature.samples[left+1]-feature.samples[left])*(sample-left),t,0);
            }
            if(feature.reference.curve_type==int(GeomAbs_Circle))consider(feature.geometry.center,0,1);
        } else {
            const Primitive* p=FindPrimitive(feature.reference.primitive);if(!p)continue;
            const int count=p->type=="line"?3:5;
            for(int i=0;i<count;++i)consider(AnchorPoint(*p,i),0,i);
        }
    }
    return found;
}
QVector<drafting::Geometry> DimensionGeometry(const drafting::Dimension& d) const {
    QVector<drafting::Geometry> result;
    for(const auto& ref:d.refs) {
        try {result.push_back(ResolveDimensionReference(ref));}
        catch(const Standard_Failure&) {result.push_back({});}
    }
    return result;
}
void UpgradeDimension(Primitive& p) const {
    if(p.has_dimension)return;
    p.has_dimension=true;p.dimension.kind=drafting::DimensionKind::Parallel;
    drafting::Reference a,b;a.primitive=p.ref1;a.anchor=p.anchor1;b.primitive=p.ref2;b.anchor=p.anchor2;
    p.dimension.refs={a,b};
    const QPointF first=ResolveReferencePoint(p.ref1,p.anchor1,p.p1),second=ResolveReferencePoint(p.ref2,p.anchor2,p.p2);
    p.dimension.offset=(second-first)/2+QPointF(0,-10);
}
QGraphicsItem* DrawDimension(const Primitive& primitive,bool preview=false) {
    Primitive p=primitive;UpgradeDimension(p);
    const auto& d=p.dimension;auto layout=drafting::Layout(d,DimensionGeometry(d));
    auto* group=createItemGroup({});group->setData(1,layout.value);group->setData(2,int(d.kind));group->setData(3,layout.valid);
    if(!layout.valid) {
        auto* warning=addSimpleText(QString::fromUtf8("⚠ Размер: нет привязки"));
        warning->setScale(.3);warning->setBrush(Qt::red);warning->setPos(p.p1);group->addToGroup(warning);
        group->setToolTip(layout.error+QString::fromUtf8(". Двойной щелчок — перепривязать."));return group;
    }
    const QColor color=preview?QColor(0,110,215):d.color;
    const QPen pen(color,d.line_width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    group->addToGroup(addPath(layout.lines,pen));group->addToGroup(addPath(layout.arrows,pen,QBrush(color)));
    QFont font(d.font);font.setPixelSize(100);
    const auto add_label=[&](QString text,QPointF center,double height,bool background) {
        auto* label=new QGraphicsSimpleTextItem(text);label->setFont(font);label->setBrush(color);
        const double scale=height/100;const QSizeF size=label->boundingRect().size()*scale;
        if(background)group->addToGroup(addRect(QRectF(center-QPointF(size.width()/2+.4,size.height()/2),size+QSizeF(.8,0)),Qt::NoPen,QBrush(Qt::white)));
        addItem(label);label->setScale(scale);label->setPos(center-QPointF(size.width()/2,size.height()/2));group->addToGroup(label);
        return size;
    };
    // Keep text and its paper-coloured mask together when aligned to the line.
    auto* text_group=createItemGroup({});
    const auto before=group->childItems();
    const QSizeF size=add_label(layout.text,layout.label,d.height,true);
    if(!d.upper.isEmpty())add_label(d.upper,layout.label+QPointF(size.width()/2+d.height,d.height*-.4),d.height*.65,true);
    if(!d.lower.isEmpty())add_label(d.lower,layout.label+QPointF(size.width()/2+d.height,d.height*.45),d.height*.65,true);
    if(!d.below.isEmpty())add_label(d.below,layout.label+QPointF(0,d.height*1.25),d.height*.85,true);
    const auto children=group->childItems();for(auto* child:children)if(!before.contains(child))text_group->addToGroup(child);
    text_group->setTransformOriginPoint(layout.label);text_group->setRotation(layout.angle);group->addToGroup(text_group);
    group->setToolTip(drafting::DimensionName(d.kind)+QString::fromUtf8(" — перетащите для переноса, двойной щелчок для редактирования"));
    return group;
}
void CancelDimension() {
    if(dimension_preview_) {delete dimension_preview_;dimension_preview_=nullptr;}
    if(dimension_snap_) {delete dimension_snap_;dimension_snap_=nullptr;}
    pending_dimension_.refs.clear();
    if(replacing_dimension_id_>=0)tool_=DraftingWorkspace::Tool::Select;
    replacing_dimension_id_=-1;
}
void ShowDimensionPrompt(QString error={}) {
    using K=drafting::DimensionKind;
    const K kind=DimensionKind();const int count=pending_dimension_.refs.size();
    QString prompt;
    if(kind==K::Radius||kind==K::Diameter)prompt=count?QString::fromUtf8("Укажите положение размера"):QString::fromUtf8("Выберите окружность или круговую дугу");
    else if(count>=2)prompt=QString::fromUtf8("Укажите положение размерной линии");
    else if(kind==K::Angular)prompt=count?QString::fromUtf8("Выберите вторую прямую"):QString::fromUtf8("Выберите первую прямую");
    else if(kind==K::Perpendicular&&count==1)prompt=QString::fromUtf8("Выберите прямую для перпендикуляра");
    else prompt=count?QString::fromUtf8("Выберите вторую опорную точку"):QString::fromUtf8("Выберите первую опорную точку");
    prompt=drafting::DimensionName(kind)+": "+(error.isEmpty()?prompt:error)+QString::fromUtf8(". Esc — отмена");
    for(auto* view:views()) {view->setStatusTip(prompt);view->setToolTip(prompt);view->setCursor(Qt::CrossCursor);}
    if(dimensionPromptChanged)dimensionPromptChanged(prompt);
    if(!error.isEmpty())QToolTip::showText(QCursor::pos(),prompt);
}
int RequiredDimensionRefs() const {auto k=DimensionKind();return k==drafting::DimensionKind::Radius||k==drafting::DimensionKind::Diameter?1:2;}
bool NextDimensionReference(QPointF position,drafting::Reference& ref,QPointF& snap) const {
    using K=drafting::DimensionKind;const K kind=DimensionKind();
    const bool circle=kind==K::Radius||kind==K::Diameter;
    const bool edge=circle||kind==K::Angular||(kind==K::Perpendicular&&pending_dimension_.refs.size()==1);
    return DimensionSnap(position,edge,circle,ref,snap);
}
void SetDimensionPosition(QPointF position) {
    const auto g=DimensionGeometry(pending_dimension_);if(g.isEmpty()||!g[0].valid)return;
    const bool radial=DimensionKind()==drafting::DimensionKind::Radius||DimensionKind()==drafting::DimensionKind::Diameter;
    pending_dimension_.offset=position-(radial?g[0].center:g[0].point);
}
void DimensionHover(QGraphicsSceneMouseEvent* event) {
    if(dimension_snap_){delete dimension_snap_;dimension_snap_=nullptr;}
    if(pending_dimension_.refs.size()<RequiredDimensionRefs()) {
        drafting::Reference ref;QPointF snap;
        if(NextDimensionReference(event->scenePos(),ref,snap)) {
            double r=SnapTolerance()*.4;dimension_snap_=addEllipse(QRectF(snap-QPointF(r,r),QSizeF(r*2,r*2)),QPen(QColor(0,120,215),.4));
            dimension_snap_->setZValue(1001);dimension_snap_->setAcceptedMouseButtons(Qt::NoButton);
        }
    } else {
        SetDimensionPosition(event->scenePos());
        if(dimension_preview_)delete dimension_preview_;
        Primitive p;p.type="dimension";p.has_dimension=true;p.dimension=pending_dimension_;p.p1=event->scenePos();
        dimension_preview_=DrawDimension(p,true);dimension_preview_->setZValue(1000);dimension_preview_->setAcceptedMouseButtons(Qt::NoButton);
    }
    event->accept();
}
void DimensionClick(QGraphicsSceneMouseEvent* event) {
    event->accept();
    if(event->button()==Qt::RightButton){CancelDimension();if(IsDimensionTool())ShowDimensionPrompt();else SetTool(DraftingWorkspace::Tool::Select);return;}
    if(event->button()!=Qt::LeftButton)return;
    if(pending_dimension_.refs.isEmpty())pending_dimension_.kind=DimensionKind();
    if(pending_dimension_.refs.size()<RequiredDimensionRefs()) {
        drafting::Reference ref;QPointF snap;
        if(!NextDimensionReference(event->scenePos(),ref,snap)) {ShowDimensionPrompt(QString::fromUtf8("Нет подходящей геометрии под курсором"));return;}
        if(!pending_dimension_.refs.isEmpty()) {
            const auto first=pending_dimension_.refs.front();
            const auto* a=FindPrimitive(first.primitive);const auto* b=FindPrimitive(ref.primitive);
            if(a&&b&&(a->type=="model_view"||b->type=="model_view")&&a->id!=b->id) {
                ShowDimensionPrompt(QString::fromUtf8("Обе опоры должны принадлежать одному виду"));return;
            }
        }
        pending_dimension_.refs.push_back(ref);ShowDimensionPrompt();return;
    }
    SetDimensionPosition(event->scenePos());
    auto layout=drafting::Layout(pending_dimension_,DimensionGeometry(pending_dimension_));
    if(!layout.valid) {ShowDimensionPrompt(layout.error);return;}
    Primitive p;p.id=data_->next_primitive_id++;p.type="dimension";p.has_dimension=true;p.dimension=pending_dimension_;p.p1=event->scenePos();
    const bool replacing=replacing_dimension_id_>=0;
    if(replacing) {
        for(auto& old:data_->primitives)if(old.id==replacing_dimension_id_) {p.id=old.id;old=p;break;}
    } else data_->primitives.push_back(p);
    CancelDimension();if(replacing)SetTool(DraftingWorkspace::Tool::Select);
    Rebuild();if(changed)changed();if(IsDimensionTool())ShowDimensionPrompt();
}
bool EditDimensionAt(QPointF point) {
    auto* item=itemAt(point,QTransform());if(!item)return false;
    while(item->parentItem())item=item->parentItem();
    if(!item->data(0).isValid())return false;
    const int index=item->data(0).toInt();if(index<0||index>=data_->primitives.size()||data_->primitives[index].type!="dimension")return false;
    Primitive& p=data_->primitives[index];UpgradeDimension(p);drafting::Dimension edit=p.dimension;
    QDialog dialog(QApplication::activeWindow());dialog.setObjectName("draftingDimensionEditor");
    dialog.setWindowTitle(QString::fromUtf8("Параметры размера — ")+drafting::DimensionName(edit.kind));
    QFormLayout form(&dialog);
    const auto layout=drafting::Layout(edit,DimensionGeometry(edit));
    form.addRow(QString::fromUtf8("Измерено"),new QLabel(layout.valid?QString::number(layout.value,'g',12):layout.error));
    const auto field=[&](QString title,QString value,QString name) {auto* e=new QLineEdit(value,&dialog);e->setObjectName(name);form.addRow(title,e);return e;};
    auto* prefix=field(QString::fromUtf8("Префикс"),edit.prefix,"prefix");
    auto* symbol=field(QString::fromUtf8("Символ (пусто — автоматически)"),edit.symbol,"symbol");
    auto* value=field(QString::fromUtf8("Текст (пусто — измеренное значение)"),edit.override_text,"override");
    auto* suffix=field(QString::fromUtf8("Суффикс"),edit.suffix,"suffix");
    auto* upper=field(QString::fromUtf8("Верхний допуск"),edit.upper,"upper");
    auto* lower=field(QString::fromUtf8("Нижний допуск"),edit.lower,"lower");
    auto* below=field(QString::fromUtf8("Подпись снизу"),edit.below,"below");
    auto* font=new QFontComboBox(&dialog);font->setCurrentFont(QFont(edit.font));form.addRow(QString::fromUtf8("Шрифт"),font);
    const auto spin=[&](QString title,double value,double low,double high) {auto* s=new QDoubleSpinBox(&dialog);s->setRange(low,high);s->setDecimals(2);s->setValue(value);form.addRow(title,s);return s;};
    auto* height=spin(QString::fromUtf8("Высота текста, мм"),edit.height,1,30);
    auto* arrowsize=spin(QString::fromUtf8("Длина стрелки, мм"),edit.arrow_size,.5,15);
    auto* weight=spin(QString::fromUtf8("Толщина линии, мм"),edit.line_width,.05,2);
    auto* precision=new QSpinBox(&dialog);precision->setRange(0,6);precision->setValue(edit.precision);form.addRow(QString::fromUtf8("Знаков после запятой"),precision);
    auto* arrows=new QComboBox(&dialog);arrows->addItems({QString::fromUtf8("Закрашенные"),QString::fromUtf8("Открытые"),QString::fromUtf8("Засечки")});arrows->setCurrentIndex(edit.arrows);form.addRow(QString::fromUtf8("Стрелки"),arrows);
    auto* textpos=new QComboBox(&dialog);textpos->addItems({QString::fromUtf8("Вдоль размера"),QString::fromUtf8("Горизонтально"),QString::fromUtf8("На выноске")});textpos->setCurrentIndex(edit.text_position);form.addRow(QString::fromUtf8("Положение текста"),textpos);
    auto* outside=new QCheckBox(QString::fromUtf8("Стрелки снаружи"));outside->setChecked(edit.outside);form.addRow(outside);
    auto* center=new QCheckBox(QString::fromUtf8("Отметка центра"));center->setChecked(edit.center_mark);form.addRow(center);
    auto* centerline=new QCheckBox(QString::fromUtf8("Линия до центра радиуса"));centerline->setChecked(edit.center_line);form.addRow(centerline);
    auto* color=new QPushButton(QString::fromUtf8("Цвет…"));form.addRow(color);
    QObject::connect(color,&QPushButton::clicked,&dialog,[&]{auto c=QColorDialog::getColor(edit.color,&dialog);if(c.isValid())edit.color=c;});
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);
    bool reattach=false;auto* attach=buttons.addButton(QString::fromUtf8("Перепривязать"),QDialogButtonBox::ActionRole);
    QObject::connect(attach,&QPushButton::clicked,&dialog,[&]{reattach=true;dialog.accept();});
    QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return true;
    edit.prefix=prefix->text();edit.symbol=symbol->text();edit.override_text=value->text();edit.suffix=suffix->text();
    edit.upper=upper->text();edit.lower=lower->text();edit.below=below->text();edit.font=font->currentFont().family();
    edit.height=height->value();edit.arrow_size=arrowsize->value();edit.line_width=weight->value();edit.precision=precision->value();
    edit.arrows=arrows->currentIndex();edit.text_position=textpos->currentIndex();edit.outside=outside->isChecked();edit.center_mark=center->isChecked();edit.center_line=centerline->isChecked();
    p.dimension=edit;
    if(reattach) {
        CancelDimension();pending_dimension_=edit;pending_dimension_.refs.clear();
        // Preserve the old dimension until a replacement has been placed.
        replacing_dimension_id_=p.id;tool_=DraftingWorkspace::Tool(int(DraftingWorkspace::Tool::HorizontalDimension)+int(edit.kind));
        ShowDimensionPrompt();
    }
    Rebuild();if(changed)changed();return true;
}
int replacing_dimension_id_ = -1;
