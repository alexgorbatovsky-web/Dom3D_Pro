#include "DraftingDimensions.h"
#include <QJsonArray>
#include <QLineF>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace drafting {
namespace {
constexpr double pi = 3.14159265358979323846;
double dot(QPointF a, QPointF b) { return a.x()*b.x()+a.y()*b.y(); }
double cross(QPointF a, QPointF b) { return a.x()*b.y()-a.y()*b.x(); }
double length(QPointF p) { return std::hypot(p.x(),p.y()); }
QPointF unit(QPointF p) { const double l=length(p); return l>1.e-10?p/l:QPointF(1,0); }
double positive(double a) { a=std::fmod(a,2*pi); return a<0?a+2*pi:a; }
void line(QPainterPath& path,QPointF a,QPointF b) { path.moveTo(a);path.lineTo(b); }
void arrow(DimensionLayout& out,const Dimension& d,QPointF tip,QPointF toward) {
    toward=unit(toward); const QPointF side(-toward.y(),toward.x());
    if(d.arrows==2) { line(out.lines,tip-(toward+side)*d.arrow_size*.45,tip+(toward+side)*d.arrow_size*.45); return; }
    const QPointF back=tip+toward*d.arrow_size;
    QPainterPath& path=d.arrows==1?out.lines:out.arrows;
    path.moveTo(back+side*d.arrow_size*.22);path.lineTo(tip);path.lineTo(back-side*d.arrow_size*.22);
    if(d.arrows==0) path.closeSubpath();
}
QString number(double value,int precision) {
    QString text=QString::number(value,'f',std::clamp(precision,0,6));
    if(text.contains('.')) { while(text.endsWith('0'))text.chop(1); if(text.endsWith('.'))text.chop(1); }
    return text;
}
}
QString DimensionName(DimensionKind kind) {
    static const char* names[]={"Горизонтальный","Вертикальный","Параллельный","Перпендикулярный","Радиусный","Диаметральный","Угловой"};
    return QString::fromUtf8(names[std::clamp(int(kind),0,6)]);
}
QJsonObject ToJson(const Dimension& d) {
    QJsonArray refs;
    for(const auto&r:d.refs) {
        QJsonArray points;for(const auto& p:r.edge_points)points.append(QJsonArray{p[0],p[1],p[2]});
        refs.push_back(QJsonObject{{"primitive",r.primitive},{"body",QString::number(r.body)},
        {"edge",r.edge},{"edgeCount",r.edge_count},{"curveType",r.curve_type},{"anchor",r.anchor},
        {"parameter",r.parameter},{"wholeEdge",r.whole_edge},{"edgePoints",points}});
    }
    return {{"version",1},{"kind",int(d.kind)},{"refs",refs},{"offsetX",d.offset.x()},{"offsetY",d.offset.y()},
        {"prefix",d.prefix},{"suffix",d.suffix},{"override",d.override_text},{"below",d.below},{"upper",d.upper},{"lower",d.lower},
        {"symbol",d.symbol},{"font",d.font},{"height",d.height},{"arrowSize",d.arrow_size},{"lineWidth",d.line_width},
        {"precision",d.precision},{"arrows",d.arrows},{"textPosition",d.text_position},{"outside",d.outside},
        {"centerMark",d.center_mark},{"centerLine",d.center_line},{"color",d.color.name()}};
}
Dimension FromJson(const QJsonObject& j) {
    Dimension d; d.kind=DimensionKind(std::clamp(j["kind"].toInt(2),0,6));
    for(auto value:j["refs"].toArray()) {auto r=value.toObject();Reference ref;
        ref.primitive=r["primitive"].toInt(-1);ref.body=r["body"].toString().toULong();ref.edge=r["edge"].toInt(-1);
        ref.edge_count=r["edgeCount"].toInt();ref.curve_type=r["curveType"].toInt(-1);ref.anchor=r["anchor"].toInt();
        ref.parameter=std::clamp(r["parameter"].toDouble(),0.0,1.0);ref.whole_edge=r["wholeEdge"].toBool();
        for(const auto& v:r["edgePoints"].toArray()){const auto a=v.toArray();if(a.size()==3)ref.edge_points.push_back({a[0].toDouble(),a[1].toDouble(),a[2].toDouble()});}
        d.refs.push_back(ref);}
    d.offset={j["offsetX"].toDouble(),j["offsetY"].toDouble(-10)};
    d.prefix=j["prefix"].toString();d.suffix=j["suffix"].toString();d.override_text=j["override"].toString();d.below=j["below"].toString();
    d.upper=j["upper"].toString();d.lower=j["lower"].toString();d.symbol=j["symbol"].toString();d.font=j["font"].toString("Arial");
    d.height=std::clamp(j["height"].toDouble(3.5),1.0,30.0);d.arrow_size=std::clamp(j["arrowSize"].toDouble(3),.5,15.0);
    d.line_width=std::clamp(j["lineWidth"].toDouble(.25),.05,2.0);d.precision=std::clamp(j["precision"].toInt(2),0,6);
    d.arrows=std::clamp(j["arrows"].toInt(),0,2);d.text_position=std::clamp(j["textPosition"].toInt(),0,2);d.outside=j["outside"].toBool();
    d.center_mark=j["centerMark"].toBool();d.center_line=j["centerLine"].toBool(true);d.color=QColor(j["color"].toString("#000000"));
    if(!d.color.isValid())d.color=Qt::black;return d;
}
bool FitCircle(const QPolygonF& points,QPointF& center,double& radius) {
    if(points.size()<5)return false;
    const QPointF a=points[0],b=points[points.size()/3],c=points[2*points.size()/3];
    const QPointF u=b-a,v=c-a;const double det=2*cross(u,v);
    if(std::abs(det)<1.e-9)return false;
    center=a+QPointF((dot(u,u)*v.y()-dot(v,v)*u.y())/det,(u.x()*dot(v,v)-v.x()*dot(u,u))/det);
    radius=length(a-center);if(radius<1.e-6)return false;
    for(auto p:points)if(std::abs(length(p-center)-radius)>std::max(1.e-5,radius*1.e-5))return false;
    return true;
}
DimensionLayout Layout(const Dimension& d,const QVector<Geometry>& g) {
    DimensionLayout out;
    const bool radial=d.kind==DimensionKind::Radius||d.kind==DimensionKind::Diameter;
    if(g.size()<(radial?1:2)||std::any_of(g.begin(),g.end(),[](const Geometry& v){return !v.valid;})) {
        out.error=QString::fromUtf8("Потеряна привязка — выберите геометрию заново");return out;
    }
    const QPointF base=radial?g[0].center:g[0].point;
    const QPointF position=base+d.offset;
    if(radial) {
        if(!g[0].circle) {out.error=QString::fromUtf8("Выберите окружность или круговую дугу");return out;}
        const double r=g[0].radius;const QPointF u=unit(g[0].arc?g[0].point-base:position-base),a=base+u*r;
        const bool diameter=d.kind==DimensionKind::Diameter;
        out.value=r*g[0].scale*(diameter?2:1);
        if(diameter) {line(out.lines,base-u*r,a);arrow(out,d,base-u*r,d.outside?-u:u);}
        else if(d.center_line)line(out.lines,base,a);
        line(out.lines,a,position);
        arrow(out,d,a,diameter?(d.outside?u:-u):((length(position-base)>r||d.outside)?u:-u));
        out.label=position+QPointF(d.height, -d.height*.6);
        if(d.text_position==2)line(out.lines,position,out.label+QPointF(d.height*2, d.height*.6));
        if(d.center_mark) {line(out.lines,base-QPointF(2,0),base+QPointF(2,0));line(out.lines,base-QPointF(0,2),base+QPointF(0,2));}
    } else if(d.kind==DimensionKind::Angular) {
        const QPointF u=g[0].b-g[0].a,v=g[1].b-g[1].a;
        const double det=cross(u,v);
        if(!g[0].line||!g[1].line||std::abs(det)<1.e-8) {out.error=QString::fromUtf8("Нужны две непараллельные прямые");return out;}
        const QPointF center=g[0].a+u*(cross(g[1].a-g[0].a,v)/det);
        QPointF ray1=unit(u),ray2=unit(v);
        if(dot(ray1,position-center)<0)ray1=-ray1;
        if(dot(ray2,position-center)<0)ray2=-ray2;
        double start=std::atan2(ray1.y(),ray1.x()),sweep=positive(std::atan2(ray2.y(),ray2.x())-start);
        const double pick=positive(std::atan2(position.y()-center.y(),position.x()-center.x())-start);
        if(pick>sweep) {start+=sweep;sweep=2*pi-sweep;std::swap(ray1,ray2);}
        const double r=std::max(2*d.arrow_size,length(position-center));
        QPointF a=center+QPointF(std::cos(start),std::sin(start))*r;
        out.lines.moveTo(a);
        for(int i=1;i<=96;++i) {const double t=start+sweep*i/96;out.lines.lineTo(center+QPointF(std::cos(t),std::sin(t))*r);}
        const QPointF b=center+QPointF(std::cos(start+sweep),std::sin(start+sweep))*r;
        line(out.lines,center,center+ray1*(r+2));line(out.lines,center,center+ray2*(r+2));
        arrow(out,d,a,QPointF(-std::sin(start),std::cos(start))*(d.outside?-1:1));
        arrow(out,d,b,QPointF(std::sin(start+sweep),-std::cos(start+sweep))*(d.outside?-1:1));
        out.label=center+QPointF(std::cos(start+sweep/2),std::sin(start+sweep/2))*(r+d.height);
        out.value=sweep*180/pi;
    } else {
        QPointF a=g[0].point,b=g[1].point;
        if(d.kind==DimensionKind::Perpendicular) {
            if(!g[1].line) {out.error=QString::fromUtf8("Вторая опора должна быть прямой");return out;}
            const QPointF u=unit(g[1].b-g[1].a); b=g[1].a+u*dot(a-g[1].a,u);
        }
        QPointF u=unit(b-a);
        if(d.kind==DimensionKind::Horizontal)u={1,0};
        if(d.kind==DimensionKind::Vertical)u={0,1};
        const QPointF n(-u.y(),u.x());
        const double h=dot(position-a,n);const QPointF da=a+n*h,db=da+u*dot(b-a,u);
        out.value=std::abs(dot(b-a,u))*g[0].scale;
        if(out.value<1.e-8) {out.error=QString::fromUtf8("Нулевой размер — выберите другие опоры");return out;}
        const auto extension=[&](QPointF source,QPointF dest) {const QPointF v=unit(dest-source);line(out.lines,source+v*.7,dest+v*2);};
        extension(a,da);extension(b,db);line(out.lines,da,db);
        const QPointF along=unit(db-da);const bool outside=d.outside||length(db-da)<d.arrow_size*4+d.height*3;
        arrow(out,d,da,along*(outside?-1:1));arrow(out,d,db,along*(outside?1:-1));
        if(outside) {line(out.lines,da-along*d.arrow_size*1.5,da);line(out.lines,db,db+along*d.arrow_size*1.5);}
        out.label=(da+db)/2-n*d.height*.75;
        if(d.text_position==2) {out.label=position;line(out.lines,(da+db)/2,position);}
        else if(d.text_position==0) {
            out.angle=std::atan2(u.y(),u.x())*180/pi;
            if(out.angle>=90)out.angle-=180; if(out.angle< -90)out.angle+=180;
        }
    }
    QString symbol=d.symbol;
    if(symbol.isEmpty()) {if(d.kind==DimensionKind::Radius)symbol="R";if(d.kind==DimensionKind::Diameter)symbol=QString::fromUtf8("⌀");}
    out.text=d.prefix+symbol+(d.override_text.isEmpty()?number(out.value,d.precision):d.override_text)+d.suffix;
    if(d.kind==DimensionKind::Angular&&d.symbol.isEmpty())out.text+=QString::fromUtf8("°");
    out.valid=std::isfinite(out.value);return out;
}
}
