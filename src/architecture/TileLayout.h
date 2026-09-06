#pragma once

#include "../CMesh3D.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace TileLayout {
struct Point { double x, y; };
struct Settings {
    double length=400, width=400, thickness=6, gap=2;
    double surface_length=3000, surface_width=2000, bevel=3, insert_size=100;
    int pattern=0, quantity=2, uv_mode=1, seed=1;
};
struct Tile { std::unique_ptr<CMesh3D> mesh; int material=0; };
struct Result { std::vector<Tile> tiles; std::string error; };
inline double cross2(Point a, Point b) { return a.x*b.y-a.y*b.x; }
inline Point sub(Point a, Point b) { return {a.x-b.x,a.y-b.y}; }
inline double area(const std::vector<Point>& p) {
    double a=0; for(size_t i=0;i<p.size();++i) a+=cross2(p[i],p[(i+1)%p.size()]); return a/2;
}
inline std::vector<Point> clip(std::vector<Point> p, Point n, double d) {
    std::vector<Point> out;
    if(p.empty()) return out;
    for(size_t i=0;i<p.size();++i) {
        Point a=p[i], b=p[(i+1)%p.size()];
        double da=a.x*n.x+a.y*n.y-d, db=b.x*n.x+b.y*n.y-d;
        if(da>=-1e-8) out.push_back(a);
        if((da>0 && db<0)||(da<0 && db>0)) {
            double t=da/(da-db);out.push_back({a.x+t*(b.x-a.x),a.y+t*(b.y-a.y)});
        }
    }
    std::vector<Point> clean;
    for(auto q:out) if(clean.empty()||std::hypot(q.x-clean.back().x,q.y-clean.back().y)>1e-7) clean.push_back(q);
    if(clean.size()>1 && std::hypot(clean.front().x-clean.back().x,clean.front().y-clean.back().y)<1e-7) clean.pop_back();
    return clean;
}
inline std::unique_ptr<CMesh3D> mesh(const std::vector<Point>& base, Point origin,
    Point u, Point v, double length, double width, const Settings& s, double uv_angle) {
    const size_t count=base.size();
    // Offset the convex footprint; reduce the bevel for narrow boundary cuts.
    double bevel=std::min({s.bevel,s.thickness*.5,length*.2,width*.2});
    std::vector<Point> top=base;
    for(int attempt=0;attempt<24;++attempt) {
        bool valid=true;
        for(size_t i=0;i<count;++i) {
            Point a=sub(base[i],base[(i+count-1)%count]),b=sub(base[(i+1)%count],base[i]);
            double al=std::hypot(a.x,a.y),bl=std::hypot(b.x,b.y);
            Point na{-a.y/al,a.x/al},nb{-b.y/bl,b.x/bl};double det=cross2(na,nb);
            if(std::abs(det)<1e-10) top[i]={base[i].x+na.x*bevel,base[i].y+na.y*bevel};
            else top[i]={base[i].x+bevel*(nb.y-na.y)/det,base[i].y+bevel*(na.x-nb.x)/det};
        }
        for(auto p:top) for(size_t j=0;j<count;++j) {
            Point edge=sub(base[(j+1)%count],base[j]);
            if(cross2(edge,sub(p,base[j])) < bevel*std::hypot(edge.x,edge.y)-1e-6) valid=false;
        }
        if(valid && area(top)>1e-8) break;
        bevel*=.5;top=base;
    }
    std::vector<Vec3> vertices;std::vector<UV> uvs;std::vector<Vec3> normals;
    const double c=std::cos(uv_angle),sn=std::sin(uv_angle);
    for(int layer=0;layer<2;++layer) for(auto p:layer?top:base) {
        vertices.push_back({float(p.x),float(p.y),float(layer?s.thickness:0)});
        Point delta=sub(p,origin);double x=(delta.x*u.x+delta.y*u.y)/length-.5;
        double y=(delta.x*v.x+delta.y*v.y)/width-.5;
        uvs.push_back({float(.5+c*x-sn*y),float(.5+sn*x+c*y)});
    }
    std::vector<CMesh3D::Face> faces;
    auto face=[&](std::vector<size_t> indices,bool smooth) {
        CMesh3D::Face f;
        Vec3 n=normalize(cross(vertices[indices[1]]-vertices[indices[0]],vertices[indices[2]]-vertices[indices[0]]));
        for(auto index:indices) {
            Vec3 normal=smooth&&index>=count?Vec3{0,0,1}:n;
            normals.push_back(normal);f.corners.push_back({index,normals.size()-1,index});
        }
        faces.push_back(std::move(f));
    };
    std::vector<size_t> upper,lower;
    for(size_t i=0;i<count;++i){upper.push_back(count+i);lower.push_back(count-1-i);}
    face(upper,false);face(lower,false);
    for(size_t i=0;i<count;++i){size_t j=(i+1)%count;face({i,j,j+count,i+count},bevel>0);}
    auto result=std::make_unique<CMesh3D>("Tile");
    if(!result->SetGeometry(std::move(vertices),std::move(faces),std::move(uvs),std::move(normals))) return {};
    return result;
}
inline Result Build(const Settings& s) {
    Result result;
    for(double value:{s.length,s.width,s.thickness,s.surface_length,s.surface_width})
        if(!std::isfinite(value)||value<=0){result.error="Tile dimensions must be positive.";return result;}
    if(!std::isfinite(s.gap)||s.gap<0||!std::isfinite(s.bevel)||s.bevel<0){result.error="Gap and bevel must be nonnegative.";return result;}
    const double L=s.length+s.gap,W=s.width+s.gap;
    const double sx=s.quantity==0?s.length:s.quantity==1?2*L-s.gap:s.surface_length;
    const double sy=s.quantity==0?s.width:s.quantity==1?2*W-s.gap:s.surface_width;
    const int pattern=s.quantity==2?s.pattern:0;
    if(pattern==7 && (!std::isfinite(s.insert_size)||s.insert_size<=0||s.insert_size>=s.length)){result.error="Insert Size must be positive and smaller than Tile Length (large square side).";return result;}
    const double angle=(pattern==4||pattern==6)?3.14159265358979323846/4:0;
    Point ru{std::cos(angle),std::sin(angle)},rv{-ru.y,ru.x};
    size_t candidates=0;
    auto add_tile=[&](double x,double y,bool vertical,int i,int j,int half,double tile_length=0,double tile_width=0) {
        if(tile_length<=0)tile_length=s.length;
        if(tile_width<=0)tile_width=s.width;
        if(++candidates>200000||result.tiles.size()>=20000){result.error="Too many tiles. Increase tile size or reduce the surface (maximum 20000 tiles).";return;}
        Point origin{x+s.gap*.5,y+s.gap*.5};
        Point u=vertical?Point{0,1}:Point{1,0},v=vertical?Point{-1,0}:Point{0,1};
        if(vertical) origin.x+=tile_width;
        auto rotate=[&](Point p){return Point{ru.x*p.x+rv.x*p.y,ru.y*p.x+rv.y*p.y};};
        origin=rotate(origin);u=rotate(u);v=rotate(v);
        // Keep the outside boundary at zero, with the gap only between tiles.
        origin.x-=s.gap*.5;origin.y-=s.gap*.5;
        std::vector<Point> p;
        for(auto q:std::vector<Point>{{0,0},{tile_length,0},{tile_length,tile_width},{0,tile_width}})
            p.push_back({origin.x+u.x*q.x+v.x*q.y,origin.y+u.y*q.x+v.y*q.y});
        p=clip(p,{1,0},0);p=clip(p,{-1,0},-sx);p=clip(p,{0,1},0);p=clip(p,{0,-1},-sy);
        if(p.size()<3||area(p)<1e-6)return;
        uint32_t hash=uint32_t(i)*73856093u ^ uint32_t(j)*19349663u ^ uint32_t(half)*83492791u ^ uint32_t(s.seed)*2654435761u;
        hash^=hash>>16;hash*=2246822519u;hash^=hash>>13;
        double uv=s.uv_mode==0?0:s.uv_mode==1?double(hash%4)*1.5707963267948966:double(hash%360)*.017453292519943295;
        auto tile=mesh(p,origin,u,v,tile_length,tile_width,s,uv);
        if(tile) result.tiles.push_back({std::move(tile),pattern==7?half:s.pattern==1?((i+j)&1):0});
    };
    if(pattern==7) {
        // Two square sizes tile the lattice (B,S),(-S,B), area B*B+S*S.
        const double B=L,S=s.insert_size+s.gap,det=B*B+S*S;
        double amin=1e30,amax=-1e30,bmin=1e30,bmax=-1e30;
        for(auto p:std::vector<Point>{{0,0},{sx,0},{sx,sy},{0,sy}}){
            double a=(B*p.x+S*p.y)/det,b=(-S*p.x+B*p.y)/det;
            amin=std::min(amin,a);amax=std::max(amax,a);bmin=std::min(bmin,b);bmax=std::max(bmax,b);
        }
        if((amax-amin+8)*(bmax-bmin+8)>100000){result.error="Surface requires too many tiles.";return result;}
        for(int i=int(std::floor(amin))-3;i<=int(std::ceil(amax))+3&&result.error.empty();++i)
            for(int j=int(std::floor(bmin))-3;j<=int(std::ceil(bmax))+3&&result.error.empty();++j){
                double x=i*B-j*S,y=i*S+j*B;
                add_tile(x,y,false,i,j,0,s.length,s.length);
                add_tile(x+B,y,false,i,j,1,s.insert_size,s.insert_size);
            }
    } else if(pattern==5||pattern==6) {
        // A horizontal and vertical rectangle form a fundamental domain of
        // the lattice (L,-L), (W,W). It works for any positive aspect ratio.
        double minA=1e30,maxA=-1e30,minB=1e30,maxB=-1e30;
        for(auto p:std::vector<Point>{{0,0},{sx,0},{sx,sy},{0,sy}}){double x=p.x*ru.x+p.y*ru.y,y=p.x*rv.x+p.y*rv.y;double a=(x-y)/(2*L),b=(x+y)/(2*W);minA=std::min(minA,a);maxA=std::max(maxA,a);minB=std::min(minB,b);maxB=std::max(maxB,b);}
        if((maxA-minA+8)*(maxB-minB+8)>100000){result.error="Surface requires too many tiles.";return result;}
        for(int i=int(std::floor(minA))-3;i<=int(std::ceil(maxA))+3 && result.error.empty();++i)
            for(int j=int(std::floor(minB))-3;j<=int(std::ceil(maxB))+3 && result.error.empty();++j){double x=i*L+j*W,y=-i*L+j*W;add_tile(x,y,false,i,j,0);add_tile(x,y+W,true,i,j,1);}
    } else {
        double xmin=0,xmax=sx,ymin=0,ymax=sy;
        if(pattern==4){xmin=0;xmax=(sx+sy)/std::sqrt(2.0);ymin=-sx/std::sqrt(2.0);ymax=sy/std::sqrt(2.0);}
        if(((xmax-xmin)/L+5)*((ymax-ymin)/W+5)>200000){result.error="Surface requires too many tiles.";return result;}
        for(int j=int(std::floor(ymin/W))-1;j<=int(std::ceil(ymax/W)) && result.error.empty();++j) {
            double shift=pattern==2?(j&1)*L*.5:pattern==3?((j%3+3)%3)*L/3:0;
            for(int i=int(std::floor(xmin/L))-2;i<=int(std::ceil(xmax/L)) && result.error.empty();++i)add_tile(i*L+shift,j*W,false,i,j,0);
        }
    }
    if(!result.error.empty())result.tiles.clear();
    return result;
}
}
