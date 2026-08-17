// Circle3D.cpp : implementation file
//
//	This is a part of the CAD/CAM/CAE "Alpha".
//	Copyright (C) 1994-2000 A.Gorbatovsky 
//	All rights reserved.
//	Ukraine, Kiev
//////////////////////////////////////////////////////////////////////

#include "Circle2D.h"
#include "Circle3D.h"
#include "ColorBox.h"
#include "CAlfaObject.h"
#include "CView3d.h"
#include "Line_2P.h"
#include "LineFig.h"
#include "Line_2P.h"
#include "LineStyleBox.h"
#include "SmartLine.h"
#include "Conic.h"
#include "Arc.h"
#include "MoveKnotsBox.h"
#include "APoint3d.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

unsigned long CCircle3D::col_set=RGB_CIRCLE;


//////////////////////////////////////     CCircle3d		//////////////////////////////


CCircle3D::CCircle3D(): CContour(TYPE_CIRCLE,col_set), m_pc(0,0,0)
{	
	Alloc();
}

CCircle3D::CCircle3D(CPoint3d* p, Coord r): CContour(TYPE_CIRCLE,col_set)
{
	Alloc();
	m_pc=*p;
	m_rad=r;
	m_name.Format("Circle R%4.1f",r);
}

void CCircle3D::Alloc()
{
	m_name.Format("Circle %d",m_ID);
    m_rad=10;
    m_width=0;
}


CCircle3D::CCircle3D(CPoint3d* pc, CPoint3d* p2)
: CContour(TYPE_CIRCLE,col_set)
{
	Alloc();

	m_pc=*pc;
	m_rad=dist_POINT(pc, p2);;

}

CCircle3D::CCircle3D(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3)
: CContour(TYPE_CIRCLE,col_set)
{
	Alloc();

LINE_2P line1(p1, p2);
CPoint3d p0;
	p1->Shift((CVector*)&line1.m_l,0.5*dist_POINT(p1,p2), &p0);
LINE_2P line1_orth;
	line1.MakeOrtho(&p0, &line1_orth);
LINE_2P line2(p2, p3);
	p2->Shift((CVector*)&line2.m_l, 0.5*dist_POINT(p2,p3), &p0);
LINE_2P line2_orth;
	line2.MakeOrtho(&p0, &line2_orth);
	m_rad=0;
	if(cross2_line_2D(line1_orth.a, line1_orth.b, line1_orth.c, line2_orth.a, line2_orth.b, line2_orth.c, &m_pc.x, &m_pc.y))
		return ;//Parallel
	m_pc.z=0;

	m_rad=dist_XY_XY(p1->x,p1->y,m_pc.x, m_pc.y);
	m_name.Format("Circle R%4.1f",m_rad);

}


CCircle3D::CCircle3D(const CCircle3D* src)
    : CContour(TYPE_CIRCLE,src->m_col)
{
	CContour::Copy(src);
	m_pc=src->m_pc;
	m_rad=src->m_rad;
    m_style=src->m_style;
    m_width=src->m_width;
	
}


CCircle3D::~CCircle3D()
{

}

BOOL CCircle3D::ControlBAD()
{
	if(m_rad<DDELTA)
		return BAD;
	return OK;
}

CFigure* CCircle3D::copy(BOOL id_copy)
{
if(this==NULL)
    return NULL;
CCircle3D* circle=new CCircle3D(this);
if(circle==NULL){
	Message_err(IDS_BAD_ALLOC_MEMORY);
	return NULL;
	}
	if(id_copy)
		circle->m_ID=m_ID;
	else
		circle->SetUngrouped();
	return circle;
}

BOOL CCircle3D::Copy(const CFigure* fig)
{
	if(!fig)
		return BAD;
	if(!fig->IsKindOf(RUNTIME_CLASS(CCircle3D))){
		Message_err("Bad src to Copy!");
		return BAD;
	}
	CCircle3D* cr=(CCircle3D*)fig;
	m_pc=cr->m_pc;
	m_rad=cr->m_rad;
	return OK;
}

void CCircle3D::Draw(CView3d* pview)
{
	if(!DrawingGlass)
		return;
	if(this==NULL)
		return;
    if(Selected())
		Set_Color(~m_col);
    else
		Set_Color(m_col);

CPoint3d p[1000];
Coord delta=pview->m_delta/10.0;
int num_p=InitPoints(delta, p);

	glLineWidth((float)m_width+1);
	glDisable(GL_LIGHTING);

	GLint factor=CLineStyleBox::GetFactor(m_style);
	GLushort pattern=m_style;
	if(m_style!=0xFFFF){
		glEnable(GL_LINE_STIPPLE);
		glLineStipple( factor, pattern);
	}

	
	glBegin(GL_LINE_STRIP);
		for(int i=0;i<num_p;i++)
			glVertex3d(p[i].x, p[i].y, p[i].z);
	glEnd();

	if(pview->draw_knots){
		pview->DrawKnot(&m_pc, RGB_POINT);
		GetPoints(p);
		for(int i=0;i<4;i++)
			pview->DrawKnot(&p[i], RGB_POINT);
	}

	glLineWidth(1.0);
	glDisable (GL_LINE_STIPPLE);
	glEnable(GL_LIGHTING);
}

int CCircle3D::InitPoints(Coord delta, CPoint3d p[])
{
	if(m_rad==0)
		return 0;
	if(delta<DELTA)
		delta=0.02;
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
	CSystemCoord sc;
	m_pDoc->GetSystemCoord(&sc);
	CPoint3d p0;
	for(int i=0;i<num_p;i++){
		p[i].mod_coord_ma(&p0, &m_Cx, &m_Cy, &m_Cz);
		p[i].mod_coord_am(&p0, &sc.cx, &sc.cy, &sc.cz);
		p[i].Move(&p0, &m_pc);
	}
    return num_p;
}

void CCircle3D::Draw(CView2d* view, CDC* pDC, double dx,double dy, double kx, double )
{
if(this==NULL)
    return;

CPoint3d p[1000];
Coord delta=DopuskModelling/kx;
int num_p=InitPoints(delta, p);

	CPen Pen;
	CLineStyleBox::CreatePen(pDC, &Pen, m_style, m_width, m_col, Selected());
	CPen* OldPen=pDC->SelectObject(&Pen);

	for (int i=0; i<num_p-1; i++){
		pDC->MoveTo(int((p[i].x+dx)*kx), int((p[i].y+dy)*kx));
		pDC->LineTo(int((p[i+1].x+dx)*kx), int((p[i+1].y+dy)*kx));
	}

	pDC->SelectObject(OldPen);
	Pen.DeleteObject();

	DrawKnots(view, pDC, dx, dy, kx);
}

void CCircle3D::DrawKnots(CView2d* view, CDC* pDC, double dx, double dy, double k)
{
	if(!view || !view->IsDrawKnots())
		return;

	view->DrawKnots(pDC, &m_pc, dx, dy, k, m_rad/3, m_col);
	CPoint3d p[4];
	GetPoints(p);
	for(int i=0;i<4;i++)
		view->DrawKnots(pDC, &p[i], dx, dy, k, m_rad/3, m_col);
}

void CCircle3D::Move(CPoint3d* p1,CPoint3d* p2)
{
if(this==NULL)
    return;
CVector vect;
double dist=vect.calc(p1, p2);
    Move(&vect,dist);
}

void CCircle3D::Move(CVector* vect,double dist)
{
if(this==NULL)
    return;
	CFigure::Move(vect,dist);
	m_pc.Move(vect,dist);
}
void CCircle3D::Mirror()
{
if(this==NULL)
    return;
	CFigure::Mirror();
	m_pc.Mirror();
}

void CCircle3D::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
if(this==NULL)
    return;
	CFigure::mod_coord_ma(p0,cx,cy,cz);
	m_pc.mod_coord_ma(p0,cx,cy,cz);
}

void CCircle3D::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
if(this==NULL)
    return;
	CFigure::mod_coord_am(p0,cx,cy,cz);
	m_pc.mod_coord_am(p0,cx,cy,cz);
}

void CCircle3D::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}
void CCircle3D::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CCircle3D::print(FILE* strm)
{
if(this==NULL)
    return;
	int file_null=0;
	if(!strm){
		fopen_s(&strm, "c:\\stdout.txt","a+");
		file_null=1;
	}
    if(strm==NULL)
		return ;


	CString str;
	str.LoadString(IDS_CIRCLE);
    fprintf(strm, "%s.\n",str);
    CFigure::print(strm);
    fprintf(strm, "Radius= %-5.2f\n",m_rad);
    fprintf(strm, "Center x=%-6.2f, y=%-6.2f, z=%-6.2f\n", m_pc.x, m_pc.y, m_pc.z);
	str.LoadString(IDS_LENGTH_LINE);
    fprintf(strm, "%s= %5.2f\n",str, GetLength());
	String NameStyle=CLineStyleBox::GetLineStyleName(m_style);
    fprintf(strm, "Name Style Line%s\n", NameStyle);

	if(file_null)
		fclose(strm);
}

void CCircle3D::print(LPCTSTR text, FILE* strm)
{
if(this==NULL)
    return;
	CString str;
	str.LoadString(IDS_CIRCLE);
    fprintf(strm, "%s.\n",str);
    CFigure::print(text, strm);
    fprintf(strm, "Radius= %-5.2f\n",m_rad);
    fprintf(strm, "Center x=%-6.2f, y=%-6.2f, z=%-6.2f\n", m_pc.x, m_pc.y, m_pc.z);
}


void CCircle3D::Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size)
{
CPoint3d p0;
CPoint3d pm;
	size->X_min=size->Y_min=size->Z_min=1e15;
	size->X_max=size->Y_max=size->Z_max=-1e15;

	pm=m_pc;
	pm.mod_coord_ma(&p0, cx, cy, cz);	
	size->X_min=MIN(size->X_min,pm.x-m_rad);
	size->X_max=MAX(size->X_max,pm.x+m_rad);
	size->Y_min=MIN(size->Y_min,pm.y-m_rad);
	size->Y_max=MAX(size->Y_max,pm.y+m_rad);
	size->Z_min=MIN(size->Z_min,pm.z);
	size->Z_max=MAX(size->Z_max,pm.z);
}



void CCircle3D::Zoom(CPoint3d* p0,double Kx,double Ky,double Kz)
{
if(this==NULL)
    return;
	CFigure::Zoom(p0, Kx, Ky, Kz);
	m_pc.Zoom(p0, Kx, Ky, Kz);
	m_rad*=Kx;
}

double CCircle3D::Get_dist_Min(CPoint3d* pm,  CView3d* view)
{
if(this==NULL)
    return 1e15;
CPoint3d p[1000];
Coord delta=view->m_delta/10.0;
int num_p=InitPoints(delta, p);

register double dist_min=1e15;
CPoint3d pgr1;
CPoint3d pgr2;

    for(int i=0;i<num_p-1;i++){
		p[i].GetGrPoz(view, &pgr1);
		p[i+1].GetGrPoz(view, &pgr2);
		register double dist=get_dist_XY_line2D(pm->x, pm->y, pgr1.x, pgr1.y, pgr2.x, pgr2.y);
		if(dist_min>dist)
			dist_min=dist;
		}
   return dist_min;
}

double CCircle3D::Get_dist_Min(CPoint3d* pm)
{
if(this==NULL)
    return 1e15;
CPoint3d p[1000];
Coord delta=0.03;
int num_p=InitPoints(delta, p);
register double dist_min=1e15;

    for(int i=0;i<num_p-1;i++){
		register double dist=get_dist_XY_line2D(pm->x, pm->y, p[i].x, p[i].y, p[i+1].x, p[i+1].y);
		if(dist_min>dist)
			dist_min=dist;
		}
   return dist_min;
}

double CCircle3D::Get_dist_Min(CPoint3d* pm,  CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0,  double k)
{
if(this==NULL)
    return 1e15;
	double dist_min=1e15;
	CPoint3d p[1000];
	Coord delta=0.05;
	int num_p=InitPoints(delta, p);
	CPoint3d pf;
    for(int i=0;i<num_p-1;i++){
		CPoint3d pgr1=p[i];
		pgr1.mod_coord_am(sc);
		pgr1.mod_coord_ma(vw);
		pgr1.Move(&pf, p0);
		pgr1.Zoom(&pf,k, k, k);
		CPoint3d pgr2=p[i+1];
		pgr2.mod_coord_am(sc);
		pgr2.mod_coord_ma(vw);
		pgr2.Move(&pf, p0);
		pgr2.Zoom(&pf,k, k, k);
		double dist=get_dist_XY_line2D(pm->x, pm->y, pgr1.x, pgr1.y, pgr2.x, pgr2.y);
		if(dist_min>dist)
			dist_min=dist;
		}
	return dist_min;
}


void CCircle3D::GetPoints(CPoint3d p[4])
{
	CSystemCoord sc;
	m_pDoc->GetSystemCoord(&sc);
	CPoint3d p0=m_pc;
	p0.mod_coord_ma(&sc.p0, &sc.cx, &sc.cy, &sc.cz);
	p0.mod_coord_am(&m_P0, &m_Cx, &m_Cy, &m_Cz);

	for(int i=0;i<4; i++)
		p[i]=p0;
	p[0].y+=m_rad;
	p[1].x+=m_rad;
	p[2].y-=m_rad;
	p[3].x-=m_rad;


	for(int i=0;i<4;i++){
		p[i].mod_coord_ma(&m_P0, &m_Cx, &m_Cy, &m_Cz);
		p[i].mod_coord_am(&sc.p0, &sc.cx, &sc.cy, &sc.cz);
	}

}

double CCircle3D::FindPoint(CPoint3d* pm, CView3d* view, CSelectPrim* prim)
{
CPoint3d pgr;

	m_pc.GetGrPoz(view, &pgr);
double	dist_min=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);

	prim->m_part=m_part;
	prim->ID=m_ID;
	prim->num_pnt=0;
	if(!view->draw_knots)
		return dist_min;
	CPoint3d p[4];
	GetPoints(p);
	for(int i=0;i<4; i++){
		p[i].GetGrPoz(view, &pgr);
		double	dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
		if(dist_min>dist){
			dist_min=dist;
			prim->num_pnt=i+1;
		}
	}
		return dist_min;
}

double CCircle3D::FindPoint(CPoint3d* pm, CSelectPrim* prim)
{
	double	dist_min=dist_XY_XY(pm->x, pm->y, m_pc.x, m_pc.y);
	prim->m_part=m_part;
	prim->ID=m_ID;
	prim->num_pnt=0;
	CPoint3d p[4];
	GetPoints(p);
	for(int i=0;i<4; i++){
		double	dist=dist_XY_XY(pm->x, pm->y, p[i].x, p[i].y);
		if(dist_min>dist){
			dist_min=dist;
			prim->num_pnt=i+1;
		}
	}
	return dist_min;
}


double CCircle3D::FindPoint(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0, double k, CSelectPrim* prim)
{
	prim->m_part=m_part;
	prim->ID=m_ID;
	prim->num_pnt=0;
	CPoint3d pf;
	CPoint3d ptp=m_pc;

	ptp.mod_coord_am(sc);
	ptp.mod_coord_ma(vw);
	ptp.Move(&pf, p0);
	ptp.Zoom(&pf,k, k, k);

	double dist_min=dist_XY_XY(pm->x, pm->y, ptp.x, ptp.y);

	CPoint3d p[4];
	GetPoints(p);
	for(int i=0;i<4;i++){
		ptp=p[i];
		ptp.mod_coord_am(sc);
		ptp.mod_coord_ma(vw);
		ptp.Move(&pf, p0);
		ptp.Zoom(&pf,k, k, k);

		double dist=dist_XY_XY(pm->x, pm->y, ptp.x, ptp.y);
		if(dist_min>dist){
		    dist_min=dist;
		    prim->num_pnt=i+1;
		    }
	}
	return dist_min;
}



BOOL CCircle3D::GetPoint(CSelectPrim* prim, CPoint3d* p)
{
	if(prim->num_pnt>4){
		Message_err("BAD index of point!");
		return BAD;
	}
	CPoint3d ps[4];
	GetPoints(ps);
	switch (prim->num_pnt){
	case 0:
		*p=m_pc;
		break;
	case 1:
		*p=ps[0];
		break;
	case 2:
		*p=ps[1];
		break;
	case 3:
		*p=ps[2];
		break;
	case 4:
		*p=ps[3];
		break;
	}
	return OK;
}

BOOL CCircle3D::InRect(CPoint3d* p1, CPoint3d* p2 , CView3d* view)
{
if(this==NULL)
    return 0;

	if(m_pc.InRect(p1,p2,view))
	    return 1;
    return 0;
}

BOOL CCircle3D::InRect(CPoint3d* p1, CPoint3d* p2)
{
if(this==NULL)
    return 0;

	if(m_pc.InRect(p1,p2))
	    return 1;
    return 0;
}


void CCircle3D::DrawInfo(CView3d* view)
{
	if(!view)
		return;

}

int CCircle3D::CrossLine(CPoint3d *p1,CPoint3d *p2, CPoint3d *pc1,CPoint3d *pc2)
{
CPoint3d pm1(0,0,0);
CPoint3d pm2(0,0,0);
double a1,b1,c1;
double a2,b2,c2=0;
Coord delta=DELTA;
Coord x0,y0;
Coord xc,yc;

    if(m_rad<delta){
		message_error_(IDS_RADIUS_ZERRO);
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

int CCircle3D::CrossLine(LINE_2P* line ,CPoint3d** pc)
{
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=line->CrossCircle(this, &pc1, &pc2);
	if(nc==0)
		return nc;
	int i=0;
	*pc=(CPoint3d *)realloc((char*)*pc,nc*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	(*pc+i)->x=pc1.x;
	(*pc+i)->y=pc1.y;
	(*pc+i)->z=0;
	i++;
	if(nc==1)
		return nc;
	
	(*pc+i)->x=pc2.x;
	(*pc+i)->y=pc2.y;
	(*pc+i)->z=0;

	return nc;
}

int CCircle3D::CrossingCLine(CLine* line, CPoint3d **pc)
{
	int num_p=0;
	CPoint3d pc1;
	CPoint3d pc2;
	for(int i=0;i<line->np()-1;i++){
		LINE_2P line2p(line->P(i), line->P(i+1));
		int nc=line2p.CrossCircle(this, &pc1, &pc2);
		if(nc==1){
			*pc=(CPoint3d *)realloc((char*)*pc,(num_p+1)*sizeof(CPoint3d));
			if(*pc==NULL){
				Message_err(IDS_BAD_ALLOC_MEMORY);
				return 0;
			}
			(*pc+num_p)->x=pc1.x;
			(*pc+num_p)->y=pc1.y;
			(*pc+num_p)->z=0;
			num_p++;
		}
		if(nc==2){
			*pc=(CPoint3d *)realloc((char*)*pc,(num_p+2)*sizeof(CPoint3d));
			if(*pc==NULL){
				Message_err(IDS_BAD_ALLOC_MEMORY);
				return 0;
			}
			(*pc+num_p)->x=pc1.x;
			(*pc+num_p)->y=pc1.y;
			(*pc+num_p)->z=0;
			num_p++;
			(*pc+num_p)->x=pc2.x;
			(*pc+num_p)->y=pc2.y;
			(*pc+num_p)->z=0;
			num_p++;
		}
	}

	return num_p;
}


int CCircle3D::CrossLine(CLineFig* line, CPoint3d **pc)
{
int nc;
int num_p=0;
CPoint3d pc1;
CPoint3d pc2;
	for(int i=0;i<line->np()-1;i++){
		LINE_2P line2p(line->P1(i), line->P2(i));
		nc=line2p.CrossCircle(this, &pc1, &pc2);
		if(nc==1){
			*pc=(CPoint3d *)realloc((char*)*pc,(num_p+1)*sizeof(CPoint3d));
			if(*pc==NULL){
				Message_err(IDS_BAD_ALLOC_MEMORY);
				return 0;
			}
			(*pc+num_p)->x=pc1.x;
			(*pc+num_p)->y=pc1.y;
			(*pc+num_p)->z=0;
			num_p++;
		}
		if(nc==2){
			*pc=(CPoint3d *)realloc((char*)*pc,(num_p+2)*sizeof(CPoint3d));
			if(*pc==NULL){
				Message_err(IDS_BAD_ALLOC_MEMORY);
				return 0;
			}
			(*pc+num_p)->x=pc1.x;
			(*pc+num_p)->y=pc1.y;
			(*pc+num_p)->z=0;
			num_p++;
			(*pc+num_p)->x=pc2.x;
			(*pc+num_p)->y=pc2.y;
			(*pc+num_p)->z=0;
			num_p++;
		}
	}
	CPoint3d pcf[2];
	for(int i=0; i<line->m_fillets.GetSize(); i++){
		int np=line->m_fillets[i].CrossCircle(this, pcf);
		if(np>0){
			*pc=(CPoint3d *)realloc((char*)*pc,(num_p+np)*sizeof(CPoint3d));
			if(*pc==NULL){
				Message_err(IDS_BAD_ALLOC_MEMORY);
				return 0;
			}
			for(int ii=0; ii<np; ii++){
				(*pc+num_p)->x=pcf[ii].x;
				(*pc+num_p)->y=pcf[ii].y;
				(*pc+num_p)->z=0;
				num_p++;
			}
		}
	}
	return num_p;
}

int CCircle3D::CrossCircleG(CCircle3D* cr2, CPoint3d** pc)
{

double delta=DDELTA;
short var;

double  d=dist_XY_XY(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y);
    if(d<m_rad)
		var=2;
    else
		var=1;

    if(d<delta ||  ((var==1)&&d>(m_rad+cr2->m_rad+delta)) )
		return 0;

    if(var==2 && m_rad>(d+cr2->m_rad+delta) )
		return 0;
	*pc=(CPoint3d *)realloc((char*)*pc, 2*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	(*pc)->z=0;
    if(var==1 && (fabs(d-(m_rad+cr2->m_rad))<=delta) ){
		XY_XY_R_XY(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,m_rad, &(*pc)->x,&(*pc)->y);
		return 1;
	}
    if(var==2 && (fabs(m_rad-(d+cr2->m_rad))<=delta) ){
		XY_XY_R_XY(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,d+cr2->m_rad, &(*pc)->x,&(*pc)->y);
		return 1;
	}
	(*pc+1)->z=0;
double x=(pow(cr2->m_rad,2)-pow(m_rad,2)+pow(d,2))/(2*d);
double y=sqrt(pow(cr2->m_rad,2)-pow(x,2));

    mood_coord2Dma(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,d-x,y, &(*pc)->x,&(*pc)->y);
    mood_coord2Dma(m_pc.x,m_pc.y,cr2->m_pc.x,cr2->m_pc.y,d-x,-y, &(*pc+1)->x,&(*pc+1)->y);

  return 2;

}

int CCircle3D::CrossCircle(CCircle3D* cr2, CPoint3d** pc)
{

    if(m_rad<cr2->m_rad)
		return CrossCircleG(cr2, pc);
    return cr2->CrossCircleG(this, pc);
}

int CCircle3D::GetPointTanto(CPoint3d* p, int dir, CPoint3d* pt)
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

CLine* CCircle3D::MakeLineTanto(CPoint3d* p, int dir)
{
CPoint3d pt;
int nc=GetPointTanto(p, dir, &pt);
	if(nc==0){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
		}
	if(nc==1){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
		}
	return new CLine(p, &pt);
}


BOOL CCircle3D::write_file_dxf(FILE *fil)
{
    if(Invisible())
		return OK;
	    
   fprintf(fil,"0\n");
   fprintf(fil,"CIRCLE\n");

     fprintf(fil,"  8\n");
    fprintf(fil,"%d\n",m_layer);


   fprintf(fil,"  6\n");
   fprintf(fil,"CONTINUOUS\n");
//   fprintf(fil,"%s\n",Line_style[style].name);


   fprintf(fil,"  62\n");
//    if(width>0)
	    fprintf(fil," 7\n");
//    else
//		fprintf(fil,"%d\n",get_color_from_RGB(m_col));

   fprintf(fil," 10\n");
   fprintf(fil,"%10.5f\n",m_pc.x);

   fprintf(fil," 20\n");
   fprintf(fil,"%10.5f\n",m_pc.y);

   fprintf(fil," 30\n");
   fprintf(fil,"%10.5f\n",m_pc.z);

   fprintf(fil," 40\n");
   fprintf(fil,"%10.4f\n",m_rad);

	return OK;
}



CLine* CCircle3D::MakeLine(double delta)
{
	double angle1=0;
	double angle2=2*PI;

	
	if(m_rad<=0){
		Message_err("rad<=0!");
		return NULL;
	}
	if(delta>=m_rad)
		delta=m_rad/2.0;

	double angle=angle2-angle1;
	if(angle==0)
		angle=2.0*PI;
	Coord val=angle/(2*acos(1-delta/m_rad));
	int num_point=(int)ceil(val);
    if(num_point<9)
		num_point=9;    
	double alfa=angle/(Coord)(num_point-1);
	CLine* line=new CLine(num_point);
	if(line==NULL)
		return NULL;
	if(line->np()!=num_point){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return NULL;
		}
	CSystemCoord sc;
	m_pDoc->GetSystemCoord(&sc);
	CPoint3d p0;
   for(int i=0;i<num_point;i++){
		line->P(i)->x=m_rad*cos(alfa*i);
		line->P(i)->y=m_rad*sin(alfa*i);
		line->P(i)->z=0;
		line->P(i)->mod_coord_ma(&p0, &m_Cx, &m_Cy, &m_Cz);
		line->P(i)->mod_coord_am(&p0, &sc.cx, &sc.cy, &sc.cz);
		line->P(i)->Move(&p0, &m_pc);
	}
	return line;
}


int CCircle3D::Crossing(CContour* line2, CPoint3d** pc)
{
	CLine* line=NULL;
	int np=0;
	switch (line2->GetType()){
	case TYPE_LINE:
		return CrossLine((CLineFig*)line2, pc);
	case TYPE_CIRCLE:
		return CrossCircle((CCircle3D*)line2, pc);
	case TYPE_SPLINE:
		line=((CCubSpline*)line2)->MakeLine(0.003);
		np=CrossingCLine(line, pc);
		delete line;
		return np;
	case TYPE_CONIC:
		line=((CConic*)line2)->MakeLine(0.003);
		np=CrossingCLine(line, pc);
		delete line;
		return np;
	case TYPE_SKETCH:
		return ((CSmartLine*)line2)->CrossCircle(this, pc);
	case TYPE_MSK:
		return ((CSystemCoordF*)line2)->Crossing(this, pc);
	}
	Message_err("CCircle3D::Crossing\nBad Type of CContour.");
	return 0;
}

int cmp_pnt_angle(const void *ptr1,const void *ptr2)
{
	CPoint3d* p1=(CPoint3d*)ptr1;
	CPoint3d* p2=(CPoint3d*)ptr2;
	CPoint3d p0;
	CPoint3d px(100,0,0);

	LINE_2P lineX(&p0, &px);
	LINE_2P line1(&p0, p1);
	LINE_2P line2(&p0, p2);

	Coord angle1=lineX.GetAngle(&line1);
	if(p1->y<0)
		angle1=-angle1;
	Coord angle2=lineX.GetAngle(&line2);
	if(p2->y<0)
		angle2=-angle2;

    if(angle1<angle2)
		return 1;
    if(angle1>angle2)
		return -1;
    return 0;
}


CContour** CCircle3D::Trimming(CContour** trim_line,int num_trim, int*  nl)
{
	*nl=0;
	CPoint3d* pc=NULL;
	CPoint3d* p=NULL;
	int nc=0;
	for(int i=0;i<num_trim;i++){
		if(trim_line[i]==this)
			continue;
		int np=Crossing(trim_line[i], &p);
		if(np==0)
			continue;
		pc=(CPoint3d*)realloc((char*)pc,(nc+np)*sizeof(CPoint3d));
		if(pc==NULL){
			message_error_(IDS_BAD_ALLOC_MEMORY);
			return NULL;
		}
		for(int ii=0;ii<np;ii++)
			pc[nc++]=p[ii];
	}
	if(nc<2)
		return NULL;
	CVector cx(1,0,0);
	CVector cy(0,1,0);
	CVector cz(0,0,1);
	for(int i=0;i<nc;i++)
		pc[i].mod_coord_am(&m_pc, &cx, &cy, &cz);
    qsort((void*)pc,nc,sizeof(CPoint3d),cmp_pnt_angle);
	for(int i=0;i<nc;i++)
		pc[i].mod_coord_ma(&m_pc, &cx, &cy, &cz);

	CSmartLine** lines=(CSmartLine**)calloc(nc, sizeof(CSmartLine*));
	if(lines==NULL){
		message_error_(IDS_BAD_ALLOC_MEMORY);
		return NULL;
	}
	int i=0;
	for(i=0;i<nc-1;i++){
		CArc* arc=new CArc(&m_pc, &pc[i], &pc[i+1]);
		lines[i]=new CSmartLine;
		lines[i]->Add(arc);
	}
	CArc* arc=new CArc(&m_pc, &pc[i], &pc[0]);
	lines[i]=new CSmartLine;
	lines[i]->Add(arc);
	*nl=nc;
	m_rad=0;
	return (CContour**)lines;

}

void CCircle3D::GetPointOrtho(CPoint3d* pm,CPoint3d* pc)
{
	CVector vector3d(Pc(), pm);
	Pc()->Shift(&vector3d, m_rad, pc);
}

double CCircle3D::FindRadius(CPoint3d* pm, CView3d* view, CSelectPrim* rad)
{
	rad->type=0;
	rad->m_part=m_part;
	rad->ID=m_ID;
	return Get_dist_Min(pm, view);
}

double CCircle3D::FindRadius(CPoint3d* pm, CSelectPrim* rad)
{
	rad->type=0;
	rad->m_part=m_part;
	rad->ID=m_ID;
	return Get_dist_Min(pm);
}

double CCircle3D::FindRadius(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0,  double k,CSelectPrim* rad)
{
	rad->type=0;
	rad->m_part=m_part;
	rad->ID=m_ID;
	return Get_dist_Min(pm, sc, vw, p0, k);
}


BOOL CCircle3D::IsPointInArc(CPoint3d* , CSelectPrim* rad)
{
	if(rad->ID!=m_ID )
		return 0;
	if(rad->m_part==m_part)
		return 1;
	return 0;
}

BOOL CCircle3D::GetRadius(CSelectPrim* rad, CCircle2D* cr)
{
	if(rad->ID!=m_ID){
		Message_err("rad->ID!=m_ID!");
		return BAD;
	}
	cr->m_pc=m_pc;
	cr->m_rad=m_rad;	
	return OK;
}

BOOL CCircle3D::EditPoint(CSelectPrim* prim, CPoint3d* pc, bool /*KnotOnly*/)
{
	if(prim->num_pnt==0){
		m_pc=*pc;
		return OK;
	}

	m_rad=dist_POINT(&m_pc, pc);
	return OK;
}

void CCircle3D::AddKnotsRect(CPoint3d* p1, CPoint3d* p2, CView3d* view, CMoveKnotsBox* box)
{

	CSelectPrim knot;
	knot.type=TYPE_KNOT;
	knot.m_part=m_part;
	knot.ID=m_ID;
	knot.num_pnt=0;
		
	if(m_pc.InRect(p1,p2,view))
		if(box->Add(knot)==OK){
			CKnot* pnt=new CKnot(&knot, 0, RGB_POINT);
			view->AddFigure(pnt);
		}

}

void CCircle3D::RemoveKnotsRect(CPoint3d* p1, CPoint3d* p2, CView3d* view, CMoveKnotsBox* box)
{

	CSelectPrim knot;
	knot.type=TYPE_KNOT;
	knot.m_part=m_part;
	knot.ID=m_ID;
	knot.num_pnt=0;
	if(m_pc.InRect(p1,p2,view))
		if(box->RemoveKnot(&knot)==OK)
			view->RemoveKnot(&knot);
}

void CCircle3D::GetMidlePoint(CPoint3d* pm)
{
	CLine* line=MakeLine(0.1);
	if(line==NULL)
		return ;
	line->GetMidlePoint(pm);
	delete line;

}

Coord CCircle3D::GetLength()
{
	return 2*PI*m_rad;
}


Coord CCircle3D::GetParametr(CPoint3d* p)
{
	CVector cx(1,0,0);
	CVector cy(0,1,0);
	CVector cz(0,0,1);
	p->mod_coord_am(&m_pc, &cx, &cy, &cz);

	CPoint3d p0;
	CPoint3d px(100,0,0);

	LINE_2P lineX(&p0, &px);
	LINE_2P line1(&p0, p);

	Coord angle1=lineX.GetAngle(&line1);
	if(p->y<0)
		angle1=-angle1;

	p->mod_coord_ma(&m_pc, &cx, &cy, &cz);
	return angle1;
}
extern int cmp_pnt_v(const void *ptr1,const void *ptr2);

CContour* CCircle3D::Trimming(CSelectBox* SelectBox, CPoint3d* pm, CDraft* dr)
{
	CPoint4d* pc=NULL;
	CPoint3d* p=NULL;
	int np=0;
	int size=SelectBox->GetSize();
	for(int i=0;i<size;i++){
		CContour* line2=(CContour*)SelectBox->GetFigure(i);
		if(line2==NULL)
			continue;
		line2->SetUnselected();
		if(line2==this)
			continue;
		int nc= Crossing(line2, &p);
		for(int k=0;k<nc;k++){
			pc=(CPoint4d*)realloc(pc, (np+1)*sizeof(CPoint4d));
			pc[np].x=p[k].x;
			pc[np].y=p[k].y;
			pc[np].z=p[k].z;
			pc[np++].v=GetParametr(&p[k]);
		}
	}
	if(np<2)
		return NULL;
    qsort((void*)pc,np,sizeof(CPoint4d),cmp_pnt_v);
/*	for(int j=0;j<np;j++){
		CAPoint3d* pnt=new CAPoint3d((CPoint3d*)&pc[j]);
		SelectBox->GetDoc()->AddFigure(pnt);
	}
*/
	double v=GetParametr(pm);
	int n2=0;
	while(v>pc[n2].v && n2<np)
		n2++;
	int n1=n2-1;
	if(n2==0)
		n1=np-1;
	if(n2==np){
		n1=0;
		n2--;
	}

CArc*  arc=new CArc(&m_pc, (CPoint3d*)&pc[n2], (CPoint3d*)&pc[n1]);
CSmartLine* line=new CSmartLine;
	line->Add(arc);
	if(arc->PontIn(pm))
		arc->ChangeDir();
	SelectBox->GetDoc()->AddFigureUndo(copy(1));
	m_rad=0;
	return line;
}


void CCircle3D::Edit()
{
	Message_user(IDS_EDIT_PARAMETERS);
	char string[80];
	sprintf(string,"%6.2f",m_rad);
CRadiusBox TextBox(string);
	int pr=TextBox.DoModal();
	if(pr!=IDOK)
		return;
	m_rad=atof(TextBox.m_Radius);

}


CCircle3D* CCircle3D::MakeOffset(Coord offset)
{

    if((m_rad-offset)<=0){
		message_error_(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	
	CCircle3D* cr=(CCircle3D*)copy();
    cr->m_rad=m_rad-offset;
	cr->m_name.Format("Offset %-5.2f",offset);
	return cr;
}

CArc* CCircle3D::Trimming(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc)
{
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=CrossLine(p1, p2, &pc1, &pc2);
	if(nc<2)
		return NULL;
	CArc* arc=new CArc(&m_pc, &pc1, &pc2);

	CLine* line=arc->MakeLine(0.05);
	CPoint3d pm;
	line->GetMidlePoint(&pm);
	delete line;
	int dir1=left_or_right(p1->x,p1->y,p2->x,p2->y, pc->x,pc->y);
	int dir2=left_or_right(p1->x,p1->y,p2->x,p2->y, pm.x,pm.y);
	if(dir1!=dir2)	
		arc->ChangeDir();
	return arc;
}

void CCircle3D::GetCenterPoint(CPoint3d* p)
{
	*p=m_pc;
}

BOOL CCircle3D::GetExtremumX(CArray<CPoint7d, CPoint7d>* pnts)
{
	CPoint3d ps[4];
	GetPoints(ps);
	CPoint7d p1(ps[1].x, ps[1].y,ps[1].z);
	pnts->Add(p1);

	CPoint7d p2(ps[3].x, ps[3].y,ps[3].z);
	pnts->Add(p2);
	
	return OK;
}


BOOL CCircle3D::GetExtremumY(CArray<CPoint7d, CPoint7d>* pnts)
{
	CPoint3d ps[4];
	GetPoints(ps);
	CPoint7d p1(ps[0].x, ps[0].y,ps[0].z);
	pnts->Add(p1);

	CPoint7d p2(ps[2].x, ps[2].y,ps[2].z);
	pnts->Add(p2);

	return OK;
}
