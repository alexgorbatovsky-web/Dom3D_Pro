// Circle2D.cpp : implementation file
//

#include "stdafx.h"
#include "Alfa.h"
#include "AlfaDoc.h"
#include "View3d.h"
#include "Circle2D.h"
#include "ColorBox.h"
#include "Line_2P.h"
#include "Line.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCircle2D
/*  31.05.97	*/

CCircle2D::CCircle2D(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3)
{
	m_rad=0;
	LINE_2P line1(p1, p2);
	CPoint3d p0;
	p1->Shift((CVector*)&line1.m_l,0.5*dist_POINT(p1,p2), &p0);
	LINE_2P line1_orth;
	line1.MakeOrtho(&p0, &line1_orth);
	LINE_2P line2(p2, p3);
	p2->Shift((CVector*)&line2.m_l, 0.5*dist_POINT(p2,p3), &p0);
	LINE_2P line2_orth;
	line2.MakeOrtho(&p0, &line2_orth);
	if(cross2_line_2D(line1_orth.a, line1_orth.b, line1_orth.c, line2_orth.a, line2_orth.b, line2_orth.c, &m_pc.x, &m_pc.y))
		return ;//Parallel
	m_pc.z=0;

	m_rad=dist_XY_XY(p1->x,p1->y,m_pc.x, m_pc.y);
}

int CCircle2D::CrossLine(CPoint3d *p1,CPoint3d *p2, CPoint3d *pc1,CPoint3d *pc2)
{
CPoint3d pm1(0,0,0);
CPoint3d pm2(0,0,0);
double a1,b1,c1;
double a2,b2,c2=0;
Coord delta=DELTA;
Coord x0,y0;
Coord xc,yc;

    if(m_rad<delta){
		message_error_("Circle radius is zero!");
		return 0;
	}

    pm1.x=p1->x-m_pc.x;
    pm1.y=p1->y-m_pc.y;
    pm2.x=p2->x-m_pc.x;
    pm2.y=p2->y-m_pc.y;
    
    if(XY_XY_ABC(pm1.x,pm1.y,pm2.x,pm2.y,&a1,&b1,&c1))
		return 0;
    if(fabs(c1)-m_rad>delta)
		return 0;
    a2=b1;
    b2=-a1;
    cross2_line_2D(a1,b1,c1,a2,b2,c2,&x0,&y0);
    pc1->x=x0+m_pc.x;
    pc1->y=y0+m_pc.y;
    pc1->z=pc2->z=0;
    if(fabs(fabs(c1)-m_rad)<delta)
		return 1;

    XY_ab_r_X0Y0(x0,y0,a2,b2,-sqrt(pow(m_rad,2)-pow(c1,2)),&xc,&yc);
    pc1->x=xc+m_pc.x;
    pc1->y=yc+m_pc.y;

    XY_ab_r_X0Y0(x0,y0,a2,b2,sqrt(pow(m_rad,2)-pow(c1,2)),&xc,&yc);
    pc2->x=xc+m_pc.x;
    pc2->y=yc+m_pc.y;
		return 2;
}

int CCircle2D::InitPoints(Coord delta, CPoint3d p[])
{
	if(m_rad==0)
		return 0;
short  num_p=(short)ceil(2.0*PI/(2*acos(1-delta/m_rad)));
    if(num_p>1000)
		num_p=1000;
    if(num_p<33)
		num_p=33;
Coord da=2.0*PI/(Coord)(num_p-1);
    for(register int i=0;i<num_p;i++){
		p[i].x=m_rad*cos(da*i);
		p[i].y=m_rad*sin(da*i);
		p[i].z=0;
	}
CPoint3d p0;
    for(int i=0;i<num_p;i++)
		p[i].Move(&p0, &m_pc);
    return num_p;
}

void CCircle2D::Draw(CView3d* pview, Pixel col)
{
if(this==NULL)
    return;

	Set_Color(col);

CPoint3d p[1000];
Coord delta=pview->m_delta/10.0;
int num_p=InitPoints(delta, p);

	glDisable(GL_LIGHTING);
	glBegin(GL_LINE_STRIP);
		for(int i=0;i<num_p;i++)
			glVertex3d(p[i].x, p[i].y, p[i].z);
	glEnd();
	glEnable(GL_LIGHTING);
}


int CCircle2D::CrossCircleG(CCircle2D* cr2, CPoint3d* pc1, CPoint3d* pc2)
{
double delta=DDELTA;
short var=1;
double  d=dist_XY_XY(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y);
    if(d<m_rad)
		var=2;
	if(d<delta ||  ((var==1)&&d>(m_rad+cr2->m_rad+delta)) )
		return 0;
    if(var==2 && m_rad>(d+cr2->m_rad+delta) )
		return 0;
	pc1->z=0;
    if(var==1 && (fabs(d-(m_rad+cr2->m_rad))<=delta) ){
		XY_XY_R_XY(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,m_rad, &pc1->x,&pc1->y);
		return 1;
	}
    if(var==2 && (fabs(m_rad-(d+cr2->m_rad))<=delta) ){
		XY_XY_R_XY(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,d+cr2->m_rad, &pc1->x,&pc1->y);
		return 1;
	}
	pc2->z=0;
double x=(pow(cr2->m_rad,2)-pow(m_rad,2)+pow(d,2))/(2*d);
double y=sqrt(pow(cr2->m_rad,2)-pow(x,2));
    mood_coord2Dma(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,d-x,y, &pc1->x,&pc1->y);
    mood_coord2Dma(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,d-x,-y, &pc2->x,&pc2->y);
  return 2;
}

int CCircle2D::CrossCircle(CCircle2D* cr2, CPoint3d* pc1, CPoint3d* pc2)
{

    if(m_rad<cr2->m_rad)
		return CrossCircleG(cr2, pc1, pc2);
    return cr2->CrossCircleG(this, pc1, pc2);
}

int CCircle2D::PointIn(CPoint3d* p)
{
	if(dist_POINT(&m_pc, p)<m_rad+DDELTA)
		return 1;
	return 0;
}


int CCircle2D::GetPointTanto(CPoint3d* p, CPoint3d* pt1, CPoint3d* pt2)
{
double dist=dist_XY_XY(p->x,p->y, m_pc.x, m_pc.y);
	if(fabs(dist-m_rad)<DDELTA){
		*pt1=m_pc;
		return 1;
	}
    if(dist<m_rad)
		return 0;
	double alfa=acos(m_rad/dist);
	double zm=m_rad*sin(alfa);
	double xm=m_rad*cos(alfa);
	double x=dist-xm;
	CVector vect(p, &m_pc);
	p->Shift(&vect, x, pt1);
	p->Shift(&vect, x, pt2);

	pt1->Offset(zm, p, &m_pc);
	pt2->Offset(-zm, p, &m_pc);
	return 2;
}

int CCircle2D::GetCircleTanto(CPoint3d* p, double rad, CPoint3d* pt1, CPoint3d* pt2)
{
	CCircle2D cr1(p, rad);
	CCircle2D cr2(&m_pc, m_rad+rad);

	return cr1.CrossCircle(&cr2, pt1,pt2);
}


CCircle2D* CCircle2D::MakeCircle(LINE_2P* line, int dir, Coord rad, int dir1, int dir2)
{
	if(line->a==0 && line->b==0 && line->c==0){
		Message_err(IDS_BAD_LINE);
		return NULL;
		}
	Coord offset=rad;
    if(dir==LEFT_DIR)
		offset=-offset;
	double rad_tmp=m_rad+rad;
	if(dir1)
	    rad_tmp=m_rad-rad;
	CCircle2D crt(&m_pc, rad_tmp);
	CPoint3d pr1;
	CPoint3d pr2;
	line->m_p1.GetOffsetPoint(line, -offset, &pr1);
	line->m_p2.GetOffsetPoint(line, -offset, &pr2);
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=crt.CrossLine(&pr1, &pr2,  &pc1, &pc2);
	if(nc==0){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	if(nc==1)
		return new CCircle2D(&pc1, rad);
	if(dir2==0)
		return new CCircle2D(&pc1, rad);;
	return new CCircle2D(&pc2, rad);;
}


CLine* CCircle2D::MakeLineTanto(CPoint3d* p, int dir)
{
CPoint3d pt;
int nc=GetPointTanto(p, dir, &pt);
	if(nc<2){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
		}
	return new CLine(p, &pt);
}

int CCircle2D::GetPointTanto(CPoint3d* p, int dir, CPoint3d* pt)
{
double dist=dist_XY_XY(p->x,p->y, m_pc.x, m_pc.y);
	if(fabs(dist-m_rad)<DDELTA){
		*pt=m_pc;
		return 1;
	}

    if(dist<m_rad)
		return 0;

double alfa=acos(m_rad/dist);
double zm=m_rad*sin(alfa);
double xm=m_rad*cos(alfa);
double x=dist-xm;
CVector vect(p, &m_pc);
	p->Shift(&vect, x, pt);
	if(dir==1)
		pt->Offset(zm, p, &m_pc);
	else
		pt->Offset(-zm, p, &m_pc);
	return 2;
}

