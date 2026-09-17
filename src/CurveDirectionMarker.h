#pragma once

#include "OpenGLCompat.h"
#include "Point3d.h"
#include <cmath>

// Screen-facing direction marker shared by polylines and splines. Its size
// does not depend on curve length, camera zoom or rotation.
inline void DrawCurveStartArrow(const CPoint3d& start,
                                const CPoint3d& next,
                                double curve_span) {
    if (curve_span <= 1.e-12) return;
    GLdouble model[16], projection[16]; GLint viewport[4], matrix_mode;
    glGetDoublev(GL_MODELVIEW_MATRIX,model);
    glGetDoublev(GL_PROJECTION_MATRIX,projection);
    glGetIntegerv(GL_VIEWPORT,viewport);
    glGetIntegerv(GL_MATRIX_MODE,&matrix_mode);
    if(viewport[2]<=0 || viewport[3]<=0) return;
    auto project=[&](const CPoint3d& p,double& x,double& y) {
        const double v[4]={p.x,p.y,p.z,1};double eye[4]={},clip[4]={};
        for(int row=0;row<4;++row) for(int col=0;col<4;++col) eye[row]+=model[col*4+row]*v[col];
        for(int row=0;row<4;++row) for(int col=0;col<4;++col) clip[row]+=projection[col*4+row]*eye[col];
        if(clip[3]<=1.e-12 || clip[2]<-clip[3] || clip[2]>clip[3]) return false;
        x=(clip[0]/clip[3]+1)*viewport[2]*0.5;
        y=(clip[1]/clip[3]+1)*viewport[3]*0.5;
        return std::isfinite(x) && std::isfinite(y);
    };
    double x,y,nx,ny;
    if(!project(start,x,y) || !project(next,nx,ny)) return;
    double dx=nx-x,dy=ny-y;
    const double length=std::hypot(dx,dy);
    if(length<1.e-6) return;
    dx/=length;dy/=length;
    const double tip_x=x+dx*22,tip_y=y+dy*22;
    const double base_x=tip_x-dx*10,base_y=tip_y-dy*10;
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LINE_BIT|GL_POLYGON_BIT);
    glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glOrtho(0,viewport[2],0,viewport[3],-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glColor3f(1.0f,0.82f,0.05f);
    glBegin(GL_TRIANGLES);
    glVertex2d(tip_x,tip_y);
    glVertex2d(base_x-dy*4,base_y+dx*4);
    glVertex2d(base_x+dy*4,base_y-dx*4);
    glEnd();
    glLineWidth(2.0f);glBegin(GL_LINES);
    glVertex2d(x,y);glVertex2d(base_x,base_y);glEnd();
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
    glMatrixMode(matrix_mode);glPopAttrib();
}
