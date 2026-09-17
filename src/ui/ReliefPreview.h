#pragma once
#include "../CMesh3D.h"
#include "LanguageManager.h"
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

// Software rendering keeps the bounded preview available without OpenGL.
class ReliefPreview : public QWidget {
public:
    explicit ReliefPreview(QWidget* parent):QWidget(parent){setMinimumSize(420,420);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);}
    void SetMesh(std::unique_ptr<CMesh3D> mesh,int axis){mesh_=std::move(mesh);axis_=axis;update();}
protected:
    void mousePressEvent(QMouseEvent* e) override {last_=e->position();}
    void mouseMoveEvent(QMouseEvent* e) override {
        if(e->buttons()&Qt::LeftButton){yaw_+=(e->position().x()-last_.x())*.01;tilt_=std::clamp(tilt_+(e->position().y()-last_.y())*.01,-1.4,1.4);last_=e->position();update();}
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.fillRect(rect(),QColor(36,40,45));
        if(!mesh_){painter.setPen(Qt::white);painter.drawText(rect().adjusted(20,20,-20,-20),Qt::AlignCenter|Qt::TextWordWrap,DomTranslate("Build a quick preview to inspect the ornament. Drag to rotate."));return;}
        const auto& vs=mesh_->GetVertices();if(vs.empty())return;
        Vec3 lo=vs[0],hi=lo;
        for(const auto& v:vs){lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
        double centre[3]={(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};
        double diagonal=std::sqrt(std::pow(hi.x-lo.x,2)+std::pow(hi.y-lo.y,2)+std::pow(hi.z-lo.z,2));
        double scale=.92*std::min(width(),height())/std::max(diagonal,1e-6);
        struct Point{double x,y,depth;};std::vector<Point> points;points.reserve(vs.size());
        for(const auto& v:vs){double p[3]={v.x-centre[0],v.y-centre[1],v.z-centre[2]};double x=p[(axis_+1)%3],y=p[(axis_+2)%3],z=p[axis_];double xx=x*std::cos(yaw_)-y*std::sin(yaw_),yy=x*std::sin(yaw_)+y*std::cos(yaw_);points.push_back({xx,z*std::cos(tilt_)-yy*std::sin(tilt_),yy*std::cos(tilt_)+z*std::sin(tilt_)});}
        struct Face{size_t a,b,c;double depth;};std::vector<Face> fs;fs.reserve(mesh_->GetFaces().size());
        for(const auto& f:mesh_->GetFaces())if(f.corners.size()==3){size_t a=f.corners[0].v,b=f.corners[1].v,c=f.corners[2].v;fs.push_back({a,b,c,(points[a].depth+points[b].depth+points[c].depth)/3});}
        std::sort(fs.begin(),fs.end(),[](const Face&a,const Face&b){return a.depth<b.depth;});painter.setPen(Qt::NoPen);
        for(auto f:fs){auto a=points[f.a],b=points[f.b],c=points[f.c];double ux=b.x-a.x,uy=b.y-a.y,uz=b.depth-a.depth,vx=c.x-a.x,vy=c.y-a.y,vz=c.depth-a.depth;double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;double n=std::sqrt(nx*nx+ny*ny+nz*nz);double light=.35+.65*std::abs((-.35*nx+.55*ny+.76*nz)/std::max(n,1e-15));painter.setBrush(QColor::fromRgbF(.78*light,.71*light,.54*light));QPolygonF poly;for(auto p:{a,b,c})poly<<QPointF(width()/2+p.x*scale,height()/2-p.y*scale);painter.drawPolygon(poly);}
    }
private:
    std::unique_ptr<CMesh3D> mesh_;int axis_=2;double yaw_=.4,tilt_=.3;QPointF last_;
};
