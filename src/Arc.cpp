// Arc.cpp: implementation of the CArc class.
//
//////////////////////////////////////////////////////////////////////

#include "CAlfaDoc.h"
#include "Arc.h"
#include "Circle2D.h"
#include "Circle3D.h"
#include "Line_2P.h"
#include "View3d.h"
#include "Line.h"
#include "SmartLine.h"
#include "CubSpline.h"
#include "Surface.h"
#include "Spline.h"
#include "BezierSpline.h"



#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


bool (__cdecl* CArc::m_wrk_function)(class CSelectPrim &,class CVector *,double)=NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CArc::CArc()
{
	m_clock=TRUE;
	m_rad=0;

	CVector cx(1,0,0);
	CVector cy(0,1,0);
	CVector cz(0,0,1);
	CPoint3d p0;
	m_sc.p0=p0;
	m_sc.cx=cx;
	m_sc.cy=cy;
	m_sc.cz=cz;
	if(Alloc(3))
		return;
}

CArc::CArc(CPoint3d* pc, CPoint3d* p1, CPoint3d* p2)
{
	m_clock=TRUE;
	m_rad=dist_POINT(pc, p1);
	if(Alloc(3))
		return;
	*P(0)=*p1;
	*P(1)=*pc;
	*P(2)=*p2;
	CVector vect2(Pc(), P(2));
	Pc()->Shift(&vect2, m_rad, P(2));

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}

	CVector cx(1,0,0);
	CVector cy(0,1,0);
	CVector cz(0,0,1);
	CPoint3d p0;
	m_sc.p0=p0;
	m_sc.cx=cx;
	m_sc.cy=cy;
	m_sc.cz=cz;
}

CArc::CArc(double x0, double y0, double x1, double y1,double x2, double y2)
{
	m_clock=TRUE;
	if(Alloc(3))
		return;
	P(0)->x = x0;
	P(0)->y = y0;

	P(1)->x = x1;
	P(1)->y = y1;

	P(2)->x = x2;
	P(2)->y = y2;

	m_rad=dist_POINT(P(0), P(1));

	CVector vect2(Pc(), P(2));
	Pc()->Shift(&vect2, m_rad, P(2));

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}

	CVector cx(1,0,0);
	CVector cy(0,1,0);
	CVector cz(0,0,1);
	CPoint3d p0;
	m_sc.p0=p0;
	m_sc.cx=cx;
	m_sc.cy=cy;
	m_sc.cz=cz;
}



CArc::~CArc()
{

}


CArc::CArc(const CArc* src)
:CLinkLine(src)
{
	m_clock=src->m_clock;
	m_rad=src->m_rad;
	m_sc=src->m_sc;
}

CLinkLine* CArc::copy()
{
if(this==NULL)
    return NULL;
CArc* conic=new CArc(this);
if(conic==NULL){
	Message_err(IDS_BAD_ALLOC_MEMORY);
	return NULL;
	}

	return conic;
}


void CArc::Move(CPoint3d* p1,CPoint3d* p2)
{
	CLinkLine::Move(p1, p2);
	m_sc.p0.Move(p1, p2);

}

void CArc::Move(CVector* dir ,double dist)
{
	CLinkLine::Move(dir, dist);
	m_sc.p0.Move(dir, dist);

}


void CArc::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	CLinkLine::mod_coord_ma(p0, cx, cy, cz);
	
	m_sc.p0.mod_coord_ma(p0, cx, cy, cz);
	m_sc.cx.mod_coord_ma(cx, cy, cz);
	m_sc.cy.mod_coord_ma(cx, cy, cz);
	m_sc.cz.mod_coord_ma(cx, cy, cz);
	m_PntIn.mod_coord_ma(p0,cx, cy, cz);
}

void CArc::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	CLinkLine::mod_coord_am(p0, cx, cy, cz);
	m_sc.p0.mod_coord_am(p0, cx, cy, cz);
	m_sc.cx.mod_coord_am(cx, cy, cz);
	m_sc.cy.mod_coord_am(cx, cy, cz);
	m_sc.cz.mod_coord_am(cx, cy, cz);
	m_PntIn.mod_coord_am(p0,cx, cy, cz);
}

void CArc::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CArc::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CArc::GetDir(CVector* dir)
{
	if(!m_sl)
		return;
	CSystemCoord dsc;
	m_sl->m_pDoc->GetSystemCoord(&dsc);
	CSystemCoord msc;
	m_sl->GetSystemCoord(&msc);
	CPoint3d p=*P(0);
	p.mod_coord_ma(&dsc);
	p.mod_coord_am(&msc);

	double val=-100;
	if(!m_clock)
		val=100;
	CPoint3d pc=*Pc();
	CPoint3d p0=*P(0);
	pc.mod_coord_ma(&dsc);
	pc.mod_coord_am(&msc);
	p0.mod_coord_ma(&dsc);
	p0.mod_coord_am(&msc);

	
	p.Offset(val, &pc, &p0);
	dir->calc(&p0, &p);
	dir->mod_coord_ma(&msc);
	dir->mod_coord_am(&dsc);
}
void CArc::GetLastDir(CVector* dir)
{
	if(!m_sl)
		return;
	CSystemCoord dsc;
	m_sl->m_pDoc->GetSystemCoord(&dsc);
	CSystemCoord msc;
	m_sl->GetSystemCoord(&msc);
	CPoint3d p=*P(2);
	p.mod_coord_ma(&dsc);
	p.mod_coord_am(&msc);
	double val=-100;
	if(!m_clock)
		val=100;
	CPoint3d pc=*Pc();
	CPoint3d p2=*P(2);
	pc.mod_coord_ma(&dsc);
	pc.mod_coord_am(&msc);
	p2.mod_coord_ma(&dsc);
	p2.mod_coord_am(&msc);
	p.Offset(val, &pc, &p2);
	dir->calc(&p2, &p);
	dir->mod_coord_ma(&msc);
	dir->mod_coord_am(&dsc);
}

void CArc::Draw(CView3d * pview)
{
if(this==NULL)
    return;
	if(!IsKindOf( RUNTIME_CLASS( CArc ) )){
		return ;
	}
    if(m_selected)
		Set_Color(~m_col);
    else
		Set_Color(m_col);



CPoint3d p[1000];
Coord delta=pview->m_delta/10.0;
int num_p=InitPoints( delta, p);

	glBegin(GL_LINE_STRIP);
		for(int i=0;i<num_p;i++)
			glVertex3d(p[i].x, p[i].y, p[i].z);
	glEnd();
	if(pview->draw_knots){
		pview->DrawKnot(P(1), RGB_POINT);
		pview->DrawKnot(P(0), RGB_RED);
		pview->DrawKnot(P(2), RGB_POINT);

		CPoint3d ps[4];
		GetPoints(ps);
		for(int i=0;i<4; i++){
			if(!PontIn(&ps[i]))
				continue;
		pview->DrawKnot(&ps[i], RGB_POINT);
		}
	}

/*/////////////////////////////
	if(pview->draw_knots){
		CPoint3d px=m_sc.p0;
		px.Move(&m_sc.cx, 100);
		Set_Color(RGB_RED);
		glBegin(GL_LINES);
			glVertex3d(m_sc.p0.x, m_sc.p0.y, m_sc.p0.z);
			glVertex3d(px.x, px.y, px.z);
		glEnd();

		CPoint3d py=m_sc.p0;
		py.Move(&m_sc.cy, 100);
		Set_Color(RGB_GREEN);
		glBegin(GL_LINES);
			glVertex3d(m_sc.p0.x, m_sc.p0.y, m_sc.p0.z);
			glVertex3d(py.x, py.y, py.z);
		glEnd();

		CPoint3d pz=m_sc.p0;
		pz.Move(&m_sc.cz, 100);
		Set_Color(RGB_BLUE);
		glBegin(GL_LINES);
			glVertex3d(m_sc.p0.x, m_sc.p0.y, m_sc.p0.z);
			glVertex3d(pz.x, pz.y, pz.z);
		glEnd();
	}
*/////////////////////////////////	
///////////////////////////////////	
}

int CArc::InitPoints(Coord delta, CPoint3d p[])
{
    if(m_rad==0)
		return 0;
	if(m_sl==NULL)
		return 0;	
	CPoint3d p0;
	CPoint3d p1=*m_sl->P1(this);
	CPoint3d p2=*m_sl->P2(this);
	m_sl->GetST(&p1, &p1.x, &p1.y);
	m_sl->GetST(&p2, &p2.x, &p2.y);
	p1.z=p2.z=0;
	CPoint3d pc(P5(1)->s, P5(1)->t, 0);
	p1.Move(&pc,&p0);
	p2.Move(&pc,&p0);
	CVector vx(1, 0, 0);
	CVector v1(&p0, &p1);
	CVector v2(&p0, &p2);
	double angle_1=acos(vx.l*v1.l+vx.m*v1.m+vx.n*v1.n);
	double angle_2=acos(vx.l*v2.l+vx.m*v2.m+vx.n*v2.n);
    if(p1.y<0)
		angle_1=2.0*PI-angle_1;
    if(p2.y<0)
		angle_2=2.0*PI-angle_2;
	double angle=angle_2-angle_1;
    if(angle_1 > angle_2)
		angle+=2*PI;
    if(m_clock==TRUE)
		angle-=2*PI;
    if(dist_POINT(&p1, &p2)<0.01)
		angle=2*PI;
    if(delta>m_rad)
		delta=m_rad/2.0;
	double da=acos((m_rad-delta)/m_rad);
	int num_p=int(fabs(angle/da));
    if(num_p>999)
		num_p=999;
    if(num_p<4)
		num_p=4;
    da=angle/(Coord)num_p;
    num_p++;
	CPoint3d pz(0, 0, 100);
    for(register int i=0;i<num_p;i++){
		p[i]=p1;
		p[i].Rotate(&p0, &pz, i*da);
		p[i].Move(&p0,&pc);
		m_sl->RestorePointASK(p[i].x, p[i].y, &p[i]);
	}
	return num_p;
}

CLine* CArc::MakeLine(double dopusk)
{
	CPoint3d p[1000];
	int num_p=InitPoints(dopusk, p);
	CLine* line=new CLine(p, num_p);
	return line;
}

double CArc::Get_dist_Min(CPoint3d* pm, CView3d* view)
{
if(this==NULL)
    return 1e15;
	double dist_min=1e15;
	CPoint3d pgr1;
	CPoint3d pgr2;
	CPoint3d p[1000];
	Coord delta=view->m_delta/3.0;
	int num_p=InitPoints(delta, p);

    for(int i=0;i<num_p-1;i++){
		p[i].GetGrPoz(view, &pgr1);
		p[i+1].GetGrPoz(view, &pgr2);
		double dist=get_dist_XY_line2D(pm->x, pm->y, pgr1.x, pgr1.y, pgr2.x, pgr2.y);
		if(dist_min>dist)
			dist_min=dist;
		}
	return dist_min;
}

double CArc::Get_dist_Min(CPoint3d* pm)
{
	return GetDistMin2D(pm->x, pm->y);
}
double CArc::GetDistMin2D(Coord xm,Coord ym)
{
if(this==NULL)
    return 1e15;
	double dist_min=1e15;
CPoint3d p[1000];
Coord delta=0.05;
int num_p=InitPoints(delta, p);
    for(int i=0;i<num_p-1;i++){
		double dist=get_dist_XY_line2D(xm, ym, p[i].x, p[i].y, p[i+1].x, p[i+1].y);
		if(dist_min>dist)
			dist_min=dist;
		}
	return dist_min;
}

double CArc::Get_dist_Min(CPoint3d* pm,  CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0,  double k)
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


/*
BOOL CArc::EditPoint(int np, CPoint3d* p)
{
	if(np>=m_num){
		Message_err("ip>=m_num!");
		return BAD;
		}
	*P(np)=*p;
	CVector vect1(Pc(), P(0));
	CVector vect2(Pc(), P(2));
	if(np==0 || np==1){
		m_rad=dist_POINT(Pc(), P(0));
		Pc()->Shift(&vect2, m_rad, P(2));
	}
	if(np==2){
		m_rad=dist_POINT(Pc(), P(2));
		Pc()->Shift(&vect1, m_rad, P(0));
	}
	return OK;
}*/

BOOL CArc::EditPoint(int np, CPoint3d* p, int constr)
{
	if(!m_sl )
		return OK;
	if(np>2)
		return BAD;

	int nc=m_sl->GetNumConstraint(this, 1);
	if(!IsRadiusConstraint()){
		double length=GetLength();
		double k=0.05;
		if(np==0)
			k=0.95;
		CPoint3d midle_p;
		GetPointLength(length*k, &midle_p);
		*P(np)=*p;
		if(np==2)
			m_rad=Pc()->DistTo(P(2));
		else
			m_rad=Pc()->DistTo(P(0));

		if(np!=1){
			EditCurvature(NULL, &midle_p);
		}
		else{
			CVector vect2(Pc(), P(2));
			Pc()->Shift(&vect2, m_rad, P(2));
		}
	}
	else{
		if(np==2 || np==0)
			*P(np)=*p;
		CVector vect1(Pc(), P(0));
		CVector vect2(Pc(), P(2));
		if(np==1)
			*P(np)=*p;
		Pc()->z=0;
		Pc()->Shift(&vect1, m_rad, P(0));
		Pc()->Shift(&vect2, m_rad, P(2));

	}

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}	
	if(!constr)
		return OK;
	int num_line=m_sl->GetIndexLine(this);
	if(num_line==0  && np==0 && m_sl->WasClosed())
		return m_sl->GetLastLine()->EditLastPoint(P(0), constr);
	if( num_line>0)
		m_sl->GetPreviousLink(this)->EditLastPoint(P(0), constr);

	if( num_line<m_sl->GetNumLines()-1)
		return m_sl->GetNextLink(this)->EditPoint(0, P(2), constr);
	return OK;
}

BOOL CArc::SetRad(double rad)
{
	if(rad<=0){
		Message_err("Rad=0!");
		return BAD;
	}
	m_rad=rad;
	CVector vect1(Pc(), P(0));
	CVector vect2(Pc(), P(2));
	Pc()->Shift(&vect1, m_rad, P(0));
	Pc()->Shift(&vect2, m_rad, P(2));

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
	
	return OK;
}


BOOL CArc::EditLastPoint(CPoint3d* p, int )
{
	CVector vect(Pc(), p);
	Pc()->Shift(&vect, m_rad, P(2));

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
	
	return OK;
}


int CArc::CrossLine(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc1, CPoint3d* pc2)
{
CCircle2D Circle2D(Pc(), m_rad);

	return Circle2D.CrossLine(p1, p2, pc1, pc2);

}

CCircle2D* CArc::MakeCircle(LINE_2P* line, BOOL dir,Coord rad, BOOL in, BOOL first)
{
	if(line->a==0 && line->b==0 && line->c==0){
		Message_err(IDS_BAD_LINE);
		return NULL;
		}
Coord offset=rad;
    if(dir==LEFT_DIR)
		offset=-offset;
double rad_tmp=Rad()+rad;
	if(in)
	    rad_tmp=Rad()-rad;
CCircle2D crt(Pc(), rad_tmp);
CPoint3d pr1;
CPoint3d pr2;
	line->m_p1.GetOffsetPoint(line, -offset, &pr1);
	line->m_p2.GetOffsetPoint(line, -offset, &pr2);
CPoint3d pc1;
CPoint3d pc2;
	int nc=crt.CrossLine(&pr1, &pr2,  &pc1, &pc2);
	if(nc==0){
//		Message_err(IDS_NO_INTERSECTION);
		return NULL;
	}
	if(nc==1 || first)
		return new CCircle2D(&pc1, rad);
	return new CCircle2D(&pc2, rad);;
}

void CArc::ChangeDir()
{
    if(m_clock==TRUE)
		m_clock=FALSE;
	else
		m_clock=TRUE;
}

void CArc::Mirror()
{
	CLinkLine::Mirror();
	m_PntIn.Mirror();

}

void CArc::RestoreDirArc()
{
	if(!PontIn(&m_PntIn))
		ChangeDir();
}

void CArc::SaveDirArc()
{
	CLine* line=MakeLine(1);
	if(!line)
		return;
	line->GetMidlePoint(&m_PntIn);
	delete line;
}

void CArc::MirrorM()
{
	CLinkLine::Mirror();
	ChangeDir();

}

int CArc::CrossLine(LINE_2P* line, CPoint3d** pc)
{
	CCircle2D cr(Pc(), m_rad);
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=line->CrossCircle(&cr, &pc1, &pc2);


	if(nc==0)
		return 0;
	*pc=(CPoint3d *)realloc((char*)*pc,2*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	if(nc==1){
		if(PontIn(&pc1)==TRUE){
			(*pc)->x=pc1.x;
			(*pc)->y=pc1.y;
			(*pc)->z=0;
			return 1;
		}
		return 0;
	}
	nc=0;
	if(PontIn( &pc1)==TRUE){
		(*pc)->x=pc1.x;
		(*pc)->y=pc1.y;
		(*pc)->z=0;
		nc++;
	}
	if(PontIn(&pc2)==TRUE){
		(*pc+nc)->x=pc2.x;
		(*pc+nc)->y=pc2.y;
		(*pc+nc)->z=0;
		nc++;
	}

	return nc;
}


int CArc::CrossInfinityLine(LINE_2P* line, CPoint3d** pc)
{

	CCircle2D cr(Pc(), m_rad);
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=cr.CrossLine(&line->m_p1, &line->m_p2, &pc1, &pc2);

	if(nc==0)
		return 0;
	*pc=(CPoint3d *)realloc((char*)*pc,2*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	if(nc==1){
		if(PontIn(&pc1)==TRUE){
			(*pc)->x=pc1.x;
			(*pc)->y=pc1.y;
			(*pc)->z=0;
			return 1;
		}
		return 0;
	}
	nc=0;
	if(PontIn( &pc1)==TRUE){
		(*pc)->x=pc1.x;
		(*pc)->y=pc1.y;
		(*pc)->z=0;
		nc++;
	}
	if(PontIn(&pc2)==TRUE){
		(*pc+nc)->x=pc2.x;
		(*pc+nc)->y=pc2.y;
		(*pc+nc)->z=0;
		nc++;
	}

	return nc;
}

///////////////////////////////////

BOOL CArc::PontIn(CPoint3d* pi)
{
    if(m_rad==0)
		return 0;
	if(m_sl==NULL)
		return 1;
	
	CPoint3d p0;
	CPoint3d p1=*m_sl->P1(this);
	CPoint3d p2=*m_sl->P2(this);
	CPoint3d pc(P5(1)->s, P5(1)->t, 0);

	m_sl->GetST(&p1, &p1.x, &p1.y);
	m_sl->GetST(&p2, &p2.x, &p2.y);
	p1.z=0;
	p2.z=0;
	p1.Move(&pc,&p0);
	p2.Move(&pc,&p0);

	CPoint3d pk;
	m_sl->GetST(pi, &pk.x, &pk.y);
	pk.z=0;
	pk.Move(&pc,&p0);


	CVector vx(1, 0, 0);
	CVector v(&p0, &pk);
	double angle=acos(vx.l*v.l+vx.m*v.m+vx.n*v.n);
    if(pk.y<0)
		angle=2.0*PI-angle;
	CVector v1(&p0, &p1);
	CVector v2(&p0, &p2);
	double angle1=acos(vx.l*v1.l+vx.m*v1.m+vx.n*v1.n);
	double angle2=acos(vx.l*v2.l+vx.m*v2.m+vx.n*v2.n);
    if(p1.y<0)
		angle1=2.0*PI-angle1;
    if(p2.y<0)
		angle2=2.0*PI-angle2;
	if(fabs(angle-angle1)<DDELTA)
		return TRUE;
	if(fabs(angle-angle2)<DDELTA)
		return TRUE;

    if(m_clock==FALSE){
		if(angle1<angle2){
			if(angle>=angle1 && angle<=angle2)
				return TRUE;
			return FALSE;
		}
		if(angle>angle2 && angle<angle1)
			return FALSE;
		return TRUE;
	}
	if(angle1<angle2){
		if(angle>angle1 && angle<angle2)
			return FALSE;
		return TRUE;
	}
	if(angle>=angle2 && angle<=angle1)
		return TRUE;
	return FALSE;
}

int CArc::CrossCircle(CCircle3D* cr,CPoint3d** pc)
{
	CCircle3D crt(Pc(), m_rad);
	CPoint3d* p=NULL;
	int nc=crt.CrossCircle(cr, &p);
	if(nc==0)
		return 0;
	*pc=(CPoint3d *)realloc((char*)*pc,2*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	if(nc==1){
		if(PontIn(p)==TRUE){
			(*pc)->x=p->x;
			(*pc)->y=p->y;
			(*pc)->z=0;
			return 1;
		}
		return 0;
	}
	nc=0;
	if(PontIn(p)==TRUE){
		(*pc)->x=p[0].x;
		(*pc)->y=p[0].y;
		(*pc)->z=0;
		nc++;
	}
	if(PontIn(&p[1])==TRUE){
		(*pc+nc)->x=p[1].x;
		(*pc+nc)->y=p[1].y;
		(*pc+nc)->z=0;
		nc++;
	}

	return nc;
}

int CArc::CrossLine( CLinkLine* link,CPoint3d** pc)
{
	if(this==NULL)
		return 0;
	if(!IsKindOf( RUNTIME_CLASS( CArc ) )){
		return 0;
	}

	if(link==NULL)
		return 0;
	if(link->IsKindOf( RUNTIME_CLASS( CArc ) )){
		CArc* arc=(CArc*)link;
		return CrossArc(arc, pc);
	}

	CLine* line=link->MakeLine(0.003);
	int np=CrossCLine(line, pc);
	delete line;
	return np;
}

int CArc::CrossCLine(CLine* line,CPoint3d** pc)
{
	int nc=0;
	for(int i=0;i<line->np()-1; i++){
		LINE_2P line2(line->P(i),line->P(i+1));
		CPoint3d* p=NULL;
		int np=CrossLine(&line2, &p);
		if(!np)
			continue;
		*pc=(CPoint3d *)realloc((char*)*pc,(nc+np)*sizeof(CPoint3d));
		if(*pc==NULL){
			Message_err(IDS_BAD_ALLOC_MEMORY);
			return 0;
		}
		for(int k=0;k<np;k++){
			(*pc+nc)->x=p[k].x;
			(*pc+nc)->y=p[k].y;
			(*pc+nc)->z=0;
			nc++;
		}
	}
	return DeleteEqualPoints(*pc, nc);
}

int CArc::CrossArc(CArc* arc,CPoint3d** pc)
{
	CCircle3D crt(Pc(), m_rad);
	CPoint3d* p=NULL;
	int nc=arc->CrossCircle(&crt, &p);
	if(nc==0)
		return 0;
	*pc=(CPoint3d *)realloc((char*)*pc,2*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	if(nc==1){
		if(PontIn(p)==TRUE){
			(*pc)->x=p->x;
			(*pc)->y=p->y;
			(*pc)->z=0;
			return 1;
		}
		return 0;
	}
	nc=0;
	if(PontIn(p)==TRUE){
		(*pc)->x=p[0].x;
		(*pc)->y=p[0].y;
		(*pc)->z=0;
		nc++;
	}
	if(PontIn( &p[1])==TRUE){
		(*pc+nc)->x=p[1].x;
		(*pc+nc)->y=p[1].y;
		(*pc+nc)->z=0;
		nc++;
	}
	if(p)
		free(p);
	return nc;
}

void CArc::Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size)
{
CPoint3d p0;
CPoint3d pm;
	size->X_min=size->Y_min=size->Z_min=1e15;
	size->X_max=size->Y_max=size->Z_max=-1e15;
CPoint3d p[1000];
Coord delta=0.1;
int num_p=InitPoints(delta, p);

	for(int i=0;i<num_p;i++){	
		pm=p[i];
		pm.mod_coord_ma(&p0, cx, cy, cz);
		size->X_min=MIN(size->X_min,pm.x);
		size->X_max=MAX(size->X_max,pm.x);
		size->Y_min=MIN(size->Y_min,pm.y);
		size->Y_max=MAX(size->Y_max,pm.y);
		size->Z_min=MIN(size->Z_min,pm.z);
		size->Z_max=MAX(size->Z_max,pm.z);
		}
}

int CArc::CrossFillet(CFillet* fil,CPoint3d** pc)
{
	CCircle3D cr(&fil->m_pc, fil->Rad());
	CPoint3d* p=NULL;
	int nc=CrossCircle( &cr, &p);
	if(nc==0)
		return nc;
	*pc=(CPoint3d *)realloc((char*)*pc,nc*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	int np=0;
	for(int i=0;i<nc;i++)
		if(fil->PontIn(&p[i])){
			(*pc+np)->x=p[i].x;
			(*pc+np)->y=p[i].y;
			(*pc+np)->z=0;
			np++;
		}
	return np;
}

Coord CArc::GetLength()
{
Coord Length=0;
CPoint3d p[1000];
Coord delta=0.01;
int num_p=InitPoints( delta, p);

	for(int i=0;i<num_p-1;i++)
		Length+=dist_POINT(&p[i], &p[i+1]);
	return Length;
}


void CArc::Revers()
{

	CLinkLine::Revers();
	ChangeDir();
}

void CArc::Zoom(CPoint3d* p0,double Kx,double Ky,double Kz)
{
	for(int i=0;i<m_num;i++)
		m_p[i].Zoom(p0, Kx, Ky, Kz);
	m_rad=dist_POINT(P(0), Pc());
}


BOOL CArc::Offset(Coord offset)
{
    if(m_clock==TRUE)
		offset=-offset;
    if((m_rad-offset)<=0){
		message_error_("Offset exceeds the radius!");
		return BAD;
	}
    m_rad=m_rad-offset;

	CVector vect1(Pc(), P(0));
	CVector vect2(Pc(), P(2));
	Pc()->Shift(&vect1, m_rad, P(0));
	Pc()->Shift(&vect2, m_rad, P(2));


	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}	
	
	
	return OK;
}

BOOL CArc::ChangeRadius(Coord offset)
{
    if((m_rad+offset)<=0){
		message_error_("Arc Radius<0!");
		return BAD;
	}
    m_rad=m_rad+offset;
	EditPoint(0,P(0), 0);
	return OK;
}


CCubSpline* CArc::MakeSpline(Coord delta)
{
	delta=delta*20.0;
	CPoint3d p[1000];
	int np=InitPoints( delta, p);

	return new CCubSpline(p, np);
}


int CArc::CrossPlane(CPlane* pl, CPoint3d** pc)
{
	if(!m_sl)
		return 0;
	CPoint3d cp=*Pc();
	cp.mod_coord_am(&m_sc.p0,&m_sc.cx,&m_sc.cy,&m_sc.cz);
	CCircle2D Circle2D(&cp, m_rad);
	

	CPlane pl2(Pc(), P(0), P(2));
	CVector cz((CVector*)pl, (CVector*)&pl2);
	CPoint3d p1(-100000,-100000,-100000);
	CPlane pl3(&p1, &cz);
	CPoint3d pc1;

	if(crossing_3plat(pl, &pl2, &pl3, &p1.x))
		return 0;
	
	CPoint3d p2(100000,100000,100000);
	CPlane pl4(&p2, &cz);
	CPoint3d pc2;
	if(crossing_3plat(pl, &pl2, &pl4, &p2.x))
		return 0;
	p1.mod_coord_am(&m_sc.p0,&m_sc.cx,&m_sc.cy,&m_sc.cz);
	p2.mod_coord_am(&m_sc.p0,&m_sc.cx,&m_sc.cy,&m_sc.cz);
	
	int nc=Circle2D.CrossLine(&p1, &p2, &pc1, &pc2);
	if(nc==0)
		return 0;
	pc1.mod_coord_ma(&m_sc.p0,&m_sc.cx,&m_sc.cy,&m_sc.cz);
	pc2.mod_coord_ma(&m_sc.p0,&m_sc.cx,&m_sc.cy,&m_sc.cz);

	*pc=(CPoint3d *)realloc((char*)*pc,2*sizeof(CPoint3d));
	if(*pc==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return 0;
	}
	if(nc==1){
		if(PontIn(&pc1)==TRUE){
			(*pc)->x=pc1.x;
			(*pc)->y=pc1.y;
			(*pc)->z=pc1.z;
			return 1;
		}
		return 0;
	}
	nc=0;
	if(PontIn( &pc1)==TRUE){
		(*pc)->x=pc1.x;
		(*pc)->y=pc1.y;
		(*pc)->z=pc1.z;
		nc++;
	}
	if(PontIn(&pc2)==TRUE){
		(*pc+nc)->x=pc2.x;
		(*pc+nc)->y=pc2.y;
		(*pc+nc)->z=pc2.z;
		nc++;
	}
	return nc;
}

BOOL CArc::ExtendBeginTo(CContour* boundry, CPoint3d* pm)
{
	 CPoint3d* pc=NULL;
	CCircle3D cr(Pc(), m_rad);
	int np=boundry->Crossing(&cr, &pc);
	if(np==0){
//		Message_err(IDS_NO_CROSS_BOUNDARY);
		return BAD;
	}
	Coord dist_min=1e15;
	int i_min=0;
	for(int i=0;i<np;i++){
		Coord dist=dist_POINT(&pc[i], pm);
		if(dist_min>dist){
			dist_min=dist;
			i_min=i;
		}
	}
	*P(0)=pc[i_min];
	free(pc);
	return OK;
}
BOOL CArc::ExtendEndTo(CContour* boundry, CPoint3d* pm)
{
	CPoint3d* pc=NULL;
	CCircle3D cr(Pc(), m_rad);
	int np=boundry->Crossing(&cr, &pc);
	if(np==0){
//		Message_err(IDS_NO_CROSS_BOUNDARY);
		return BAD;
	}
	Coord dist_min=1e15;
	int i_min=0;
	for(int i=0;i<np;i++){
		Coord dist=dist_POINT(&pc[i], pm);
		if(dist_min>dist){
			dist_min=dist;
			i_min=i;
		}
	}
	*P(2)=pc[i_min];
	free(pc);
	return OK;
}



void CArc::GetPointOrtho(CPoint3d* pm,CPoint3d* pc)
{
	CVector vect(Pc(), pm);
	Pc()->Shift(&vect, m_rad, pc);
}


void CArc::GetPoints(CPoint3d p[4])
{
	
	CPoint3d pc=*Pc();
//	pc.mod_coord_am(&m_sc.p0, &m_sc.cx, &m_sc.cy, &m_sc.cz);	
	
	for(int i=0;i<4; i++)
		p[i]=pc;
	for(int i=0;i<3;i++)
		if(fabs(P(i)->z)>0.01)
			return;
	p[0].y+=m_rad;
	p[1].x+=m_rad;
	p[2].y-=m_rad;
	p[3].x-=m_rad;


//	for(i=0;i<4; i++)
//		p[i].mod_coord_ma(&m_sc.p0, &m_sc.cx, &m_sc.cy, &m_sc.cz);

}


double CArc::FindPoint(CPoint3d* pm, CView3d* view, int* i_min)
{
double dist_min=1e15;
CPoint3d pgr;
	for(int i=0;i<m_num;i++){
		P(i)->GetGrPoz(view, &pgr);
		double dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
		if(dist_min>dist){
		    dist_min=dist;
		    *i_min=i;
		    }
	}
	if(!view->draw_knots)
		return dist_min;
	CPoint3d p[4];
	GetPoints(p);
	for(int i=0;i<4; i++){
		if(!PontIn(&p[i]))
			continue;
		p[i].GetGrPoz(view, &pgr);
		double	dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
		if(dist_min>dist){
			dist_min=dist;
			*i_min=i+m_num;
		}
	}
	
	return dist_min;
}

BOOL CArc::GetPoint(int num, CPoint3d* p)
{
if(this==NULL){
	Message_err("CLinkLine=NULL !");
	return BAD;
	}
	if(num<m_num){
		*p=*P(num);
		return OK;
		}
	num-=m_num;
	if(num>3){
		Message_err("BAD Index of Points");
		return BAD;
	}
	CPoint3d ps[4];
	GetPoints(ps);
	*p=ps[num];

	return OK;
}


CLinkLine* CArc::InsertPoint(CPoint3d* pc)
{
	Message_err(IDS_IMPOSIBLE_GEOMETRY);
	return NULL;
	CPoint3d* p2=m_sl->P2(this);
	CVector vect(Pc(), pc);
	Pc()->Shift(&vect, m_rad, pc);
	
	CArc* line2=new CArc(Pc(), pc, p2);
	line2->m_clock=m_clock;
	
	EditPoint(2, pc, 1);
	
//	*m_sl->P2(this)=*pc;
	return line2;
}



int CArc::CrossLine2D(LINE_2P* line,CPoint3d* pc1, CPoint3d* pc2)
{
	CCircle2D cr2d(Pc(), m_rad);
	return cr2d.CrossLine(&line->m_p1, &line->m_p2, pc1, pc2);
}

int CArc::MakeKnot(CLinkLine* line2)
{
	if(!line2)
		return BAD;
	if(dist_POINT(PLast(), line2->P(0))<0.01)
		return OK;

	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) )){
		CArc* arc=(CArc*)line2;
		return MakeKnotArc(arc);
	}
	LINE_2P line(line2->P(0), line2->P(1));
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=CrossLine2D(&line, &pc1, &pc2);
    if(nc==0){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}

    if(nc==1){
		EditPoint(2, &pc1, 0);
		return  line2->EditPoint(0, &pc1, 0);
	}
	Coord dist1=dist_POINT(m_sl->P2(this),&pc1);
    Coord dist2=dist_POINT(m_sl->P2(this),&pc2);
	CPoint3d pc;
	if(dist1<=dist2)
		pc=pc1;
	else
		pc=pc2;
	EditPoint(2, &pc, 0);
	return  line2->EditPoint(0, &pc, 0);
}

int CArc::MakeKnotArc(CArc* arc)
{
	CCircle2D cr1(Pc(), m_rad);
	CCircle2D cr2(arc->Pc(), arc->m_rad);

	CPoint3d pc1;
	CPoint3d pc2;
	int nc=cr1.CrossCircle(&cr2, &pc1, &pc2);
    if(nc==0){
	//	Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
    if(nc==1){
		EditPoint(2, &pc1, 0);
		return  arc->EditPoint(0, &pc1, 0);
	}
    Coord dist1=dist_POINT(m_sl->P2(this),&pc1);
    Coord dist2=dist_POINT(m_sl->P2(this),&pc2);

	CPoint3d pc;
	if(dist1<=dist2)
		pc=pc1;
	else
		pc=pc2;
	EditPoint(2, &pc, 0);
	return  arc->EditPoint(0, &pc, 0);	
}

int CArc::MakeLastKnot()
{
	CLinkLine* line2=m_sl->GetLastLine();
	if(!line2)
		return BAD;
	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) )){
		CArc* arc=(CArc*)line2;
		return MakeLastKnotArc(arc);
	}
	LINE_2P link2(line2->P(0), line2->P(1));
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=CrossLine2D(&link2, &pc1, &pc2);
    if(nc==0){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
    if(nc==1){
		EditPoint(0, &pc1, 0);
		return  line2->EditLastPoint(&pc1, 0);
	}
    Coord dist1=dist_POINT(m_sl->P1(this),&pc1);
    Coord dist2=dist_POINT(m_sl->P1(this),&pc2);
	CPoint3d pc;
	if(dist1<=dist2)
		pc=pc1;
	else
		pc=pc2;
	EditPoint(0, &pc, 0);
	return  line2->EditLastPoint( &pc, 0);
}

int CArc::MakeLastKnotArc(CArc* arc)
{
	CCircle2D cr1(Pc(), m_rad);
	CCircle2D cr2(arc->Pc(), arc->m_rad);

	CPoint3d pc1;
	CPoint3d pc2;
	int nc=cr1.CrossCircle(&cr2, &pc1, &pc2);
    if(nc==0){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
    if(nc==1){
		EditPoint(0, &pc1, 0);
		return  arc->EditLastPoint(&pc1, 0);
	}
    Coord dist1=dist_POINT(m_sl->P1(this),&pc1);
    Coord dist2=dist_POINT(m_sl->P1(this),&pc2);
	CPoint3d pc;
	if(dist1<=dist2)
		pc=pc1;
	else
		pc=pc2;
	EditPoint(0, &pc, 0);
	return  arc->EditLastPoint( &pc, 0);

}

CSpline* CArc::MakeSpline()
{
	float delta=Rad()/30.0;
	CPoint3d p[1000];
	int np=InitPoints( delta, p);
	CSpline* spline=new CSpline(np);
	for(int i=0;i<np;i++)
		*spline->P(i)=p[i];
	spline->Build();
	return spline;
}

//////////////////////////5.08.2002  ////////////////////////
int CArc::UpdateConstraint(CSystemCoord* msc)
{
	int num_line=m_sl->GetIndexLine(this);
	if(num_line==0)
		return UpdateConstraint0( msc);

	CTypedPtrArray<CObArray, CConstraint*> constr;
	m_sl->GetConstraints(this, &constr );
	if(!constr.GetSize()){
		SetUpdateflag(1);
		return OK;
	}
	for(int i=0;i<constr.GetSize(); i++)
		if(constr[i]->DoIt(msc))
			return BAD;
	int updated=1;
	for(int i=0;i<constr.GetSize(); i++){
		if(constr[i]->IsDeterminated())
			constr[i]->SetUpdateflag(1);
		else
			updated=0;
	}
	if(updated)
		SetUpdateflag(1);
	CLinkLine* link2=m_sl->GetNextLink(this);

//	else MakeKnot
	if(link2)
		return MakeKnot(link2);
	return OK;
}

int CArc::UpdateConstraint0(CSystemCoord* msc)
{
	CTypedPtrArray<CObArray, CConstraint*> constr;
	m_sl->GetConstraints(this, &constr );
	if(!constr.GetSize()){
		SetUpdateflag(1);
		return OK;
	}
	for(int i=0;i<constr.GetSize(); i++)
		if(constr[i]->DoIt(msc))
			return BAD;

	CLinkLine* line_pr=m_sl->FindLinkedLine(this);
	if(line_pr && line_pr!=this)
		if(m_sl->UpdateConstraintLink(line_pr, msc))
			return BAD;
	int updated=1;
	for(int i=0;i<constr.GetSize(); i++){
		if(constr[i]->IsDeterminated())
			constr[i]->SetUpdateflag(1);
		else
			updated=0;
	}
	if(updated)
		SetUpdateflag(1);

	if(!m_sl->WasClosed())
		return OK;

	CLinkLine* line1=m_sl->GetLastLine();
	if(line1->EditLastPoint(P(0), 1))
		return BAD;

	if(m_sl->UpdateLinkLinesBack(1))
		return BAD;

	return OK;
}

BOOL CArc::IsBasePoint(CSelectPrim* pr)
{
	if(pr->num_pnt==1)
		return 1;
	return 0;
}

BOOL CArc::EditKnot(CLinkLine* line2)
{
	if(!line2)
		return BAD;
	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) )){
		CArc* arc=(CArc*)line2;
		return MakeKnotArc(arc);
	}
	return line2->EditPoint(0, PLast(), 1);
	
}

BOOL CArc::UpdatePoint(int np, LINE_2P* line)
{
	CPoint3d pc1;
	CPoint3d pc2;
	int nc=CrossLine2D(line, &pc1, &pc2);
    if(nc==0){
	//	Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
    if(nc==1){
		P(np)->x=pc1.x;
		P(np)->y=pc1.y;
		return OK;
	}
    Coord dist1=dist_POINT(P(np),&pc1);
    Coord dist2=dist_POINT(P(np),&pc2);
	if(dist1<=dist2){
		P(np)->x=pc1.x;
		P(np)->y=pc1.y;
		return OK;
	}
	P(np)->x=pc2.x;
	P(np)->y=pc2.y;
	return OK;
}

CConstraint* CArc::MakeNodeSmooth(CSelectPrim* prim)
{
	if(prim->num_pnt!=0 && prim->num_pnt!=2){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	int np0_link=0;
	int np1_link=1;
	CLinkLine* link=m_sl->GetPreviousLink(this);
	if(prim->num_pnt==2){
		link=m_sl->GetNextLink(this);
		np0_link=1;
		np1_link=0;
	}
	if(link==NULL)
		return NULL;
	if(link->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return MakeNodeSmoothArc(prim);

	CConstraintTantoCr* con= new CConstraintTantoCr(prim);
	CCircle2D cr(Pc(), Rad());
	CPoint3d pt1;
	CPoint3d pt2;
	int pr=cr.GetPointTanto(link->P(np0_link), &pt1, &pt2);
	if(pr!=2){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	double dist1=dist_POINT(link->P(np1_link), &pt1);
	double dist2=dist_POINT(link->P(np1_link), &pt2);
	if(dist1<=dist2)
		EditPoint(prim->num_pnt, &pt1, 1);
	else{
		EditPoint(prim->num_pnt, &pt2, 1);
		con->m_var=1;
	}
	if(link->EditPoint(np1_link,  P(prim->num_pnt), 1))
		return NULL;
	return con;	
}
BOOL CArc::UpdateNodeSmooth(int np, int var)
{
	if(np==1)
		return BAD;
	int np0_link=0;
	int np1_link=1;
	CLinkLine* link=m_sl->GetPreviousLink(this);
	if(np==2){
		link=m_sl->GetNextLink(this);
		np0_link=1;
		np1_link=0;
	}
	if(link==NULL)
		return BAD;
	if(link->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return UpdateNodeSmoothArc(np, var);
	
	CCircle2D cr(Pc(), Rad());
	CPoint3d pt1;
	CPoint3d pt2;
	int pr=cr.GetPointTanto(link->P(np0_link), &pt1, &pt2);
	if(pr!=2){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	if(var==0)
		EditPoint(np, &pt1, 1);
	else
		EditPoint(np, &pt2, 1);
	return link->EditPoint(np1_link,  P(np), 1);	
}


BOOL CArc::ControlNodeSmooth(int np)
{
	CLinkLine* link=NULL;
	if(np==2)
		link=m_sl->GetNextLink(this);
	if(np==0)
		link=m_sl->GetPreviousLink(this);
	if(link==NULL)
		return BAD;
	if(link->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return ControlNodeSmoothArc(np);
	
	double dist=Pc()->GetDistLine(link->P(0), link->P(1));
	if(fabs(dist-Rad())<DELTA)
		return OK;
	return 2;
}

CConstraint* CArc::MakeNodeSmoothArc(CSelectPrim* prim)
{
	CArc* arc=(CArc*)m_sl->GetPreviousLink(this);
	int np2_link=2;
	int np1=2;
	if(prim->num_pnt==2){
		arc=(CArc*)m_sl->GetNextLink(this);
		np2_link=0;
		np1=0;
	}
	if(arc==NULL)
		return NULL;

	CCircle2D cr(arc->Pc(), arc->Rad());
	CPoint3d pt1;
	CPoint3d pt2;
	int	pr=cr.GetCircleTanto(P(np1), Rad(), &pt1, &pt2);
	if(pr==0){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	CConstraintTantoCr* con= new CConstraintTantoCr(prim);
	double dist1=dist_POINT(Pc(), &pt1);
	double dist2=dist_POINT(Pc(), &pt2);
	if(dist1<=dist2)
		EditPoint(1, &pt1, 1);
	else{
		EditPoint(1, &pt2, 1);
		con->m_var=1;
	}
	
	CCircle2D cr0(Pc(), Rad());
	cr0.CrossCircle(&cr, &pt1, &pt2);
	
	EditPoint(prim->num_pnt, &pt1, 1);
	if(arc->EditPoint(np2_link, &pt1, 1))
		return NULL;
	return con;
}
BOOL CArc::UpdateNodeSmoothArc(int np, int var)
{
	CArc* arc=(CArc*)m_sl->GetPreviousLink(this);
	int np2_link=2;
	int np1=2;
	if(np==2){
		arc=(CArc*)m_sl->GetNextLink(this);
		np2_link=0;
		np1=0;
	}
	if(arc==NULL)
		return BAD;

	CCircle2D cr(arc->Pc(), arc->Rad());
	CPoint3d pt1;
	CPoint3d pt2;
	int	pr=cr.GetCircleTanto(P(np1), Rad(), &pt1, &pt2);
	if(pr==0){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	 
	if(var==0)
		EditPoint(1, &pt1, 1);
	else
		EditPoint(1, &pt2, 1);

	CCircle2D cr0(Pc(), Rad());
	cr0.CrossCircle(&cr, &pt1, &pt2);
	EditPoint(np, &pt1, 1);

	return arc->EditPoint(np2_link, &pt1, 1);
}

BOOL CArc::ControlNodeSmoothArc(int np)
{
	CArc* arc=NULL;
	if(np==2)
		arc=(CArc*)m_sl->GetNextLink(this);
	if(np==0)
		arc=(CArc*)m_sl->GetPreviousLink(this);
	if(arc==NULL)
		return BAD;
	double dist=dist_POINT(Pc(),arc->Pc());
	if(fabs(dist-(Rad()+arc->Rad()))<DELTA)
		return OK;
	return 2;
}

CConstraint* CArc::MakeConstrTanto2()
{
	CLinkLine* link1=m_sl->GetPreviousLink(this);
	if(link1==NULL)
		return NULL;
	if(!link1->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return MakeConstrTantoLine();
	CLinkLine* link2=m_sl->GetNextLink(this);
	if(link2==NULL)
		return NULL;
	if(!link2->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return MakeConstrTantoLine();
	
	CArc* arc1=(CArc*)link1;
	CArc* arc2=(CArc*)link2;
	
	BOOL dir1=0;
	Coord dist=dist_POINT(arc1->Pc(), Pc());
	if(dist<arc1->Rad())
		dir1=1;

	dist=dist_POINT(arc2->Pc(), Pc());
	BOOL dir2=0;
	if(dist<arc2->Rad())
		dir2=1;

	LINE_2P line(arc1->Pc(), arc2->Pc());
	int dir=line.GetDir(Pc());

	CCircle2D cr1(arc1->Pc(), arc1->Rad());
	CCircle2D cr2(arc2->Pc(), arc2->Rad());

	CCircle3D* circle=CCircle2DBox::MakeCircle(&cr1, &cr2, dir1, dir2, dir, Rad());
	if(circle==NULL)
		return NULL;
	EditPoint(1, circle->Pc(), 1);	

	CPoint3d p=*Pc();
	CVector vect1(Pc(), arc1->Pc());
	p.Move(&vect1, Rad());
	EditPoint(0, &p, 1);
	arc1->EditLastPoint(P(0), 1);

	
	p=*Pc();
	CVector vect2(Pc(), arc2->Pc());
	p.Move(&vect2, Rad());
	EditPoint(2, &p, 1);	
	arc2->EditPoint(0, P(2), 1);

	CSelectPrim prim;
	prim.ID=m_sl->GetID();
	prim.m_part=m_sl->GetPart();
	prim.num_line=m_sl->GetIndexLine(this);

	CConstraintTanto2* con=new CConstraintTanto2(&prim);
	con->m_var=(BYTE)dir1;
	con->m_var2=(BYTE)dir2;
	con->m_dir=(BYTE)dir;

	return con;
}

BOOL CArc::UpdateConstrTanto2(int dir1, int dir2, int dir)
{
	CLinkLine* link1=m_sl->GetPreviousLink(this);
	if(link1==NULL)
		return NULL;
	if(!link1->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return UpdateConstrTantoLine(dir1, dir2, dir);
	CLinkLine* link2=m_sl->GetNextLink(this);
	if(link2==NULL)
		return NULL;
	if(!link2->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return UpdateConstrTantoLine(dir1, dir2, dir);
	
	CArc* arc1=(CArc*)link1;
	CArc* arc2=(CArc*)link2;
	
	CCircle2D cr1(arc1->Pc(), arc1->Rad());
	CCircle2D cr2(arc2->Pc(), arc2->Rad());

	CCircle3D* circle=CCircle2DBox::MakeCircle(&cr1, &cr2, dir1, dir2, dir, Rad());
	if(circle==NULL)
		return BAD;
	EditPoint(1, circle->Pc(), 1);	

	CPoint3d p=*Pc();
	CVector vect1(Pc(), arc1->Pc());
	p.Move(&vect1, Rad());
	EditPoint(0, &p, 1);
	arc1->EditLastPoint(P(0), 1);

	p=*Pc();
	CVector vect2(Pc(), arc2->Pc());
	p.Move(&vect2, Rad());
	EditPoint(2, &p, 1);	
	arc2->EditPoint(0, P(2), 1);

	return OK;
}

BOOL CArc::ControlConstrTanto2()
{
	CArc* arc1=(CArc*)m_sl->GetPreviousLink(this);
	CArc* arc2=(CArc*)m_sl->GetNextLink(this);
	if(arc1==NULL || arc1==NULL)
		return BAD;

	if(!arc1->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return ControlConstrTantoLineCr();
	if(!arc2->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return ControlConstrTantoLineCr();
	
	double dist=dist_POINT(Pc(),arc1->Pc());
	double delta=dist-(Rad()+arc1->Rad());
	if(fabs(delta)>DELTA)
		return 2;

	dist=dist_POINT(Pc(),arc2->Pc());
	delta=dist-(Rad()+arc2->Rad());
	if(fabs(delta)>DELTA)
		return 2;

	return OK;
}

BOOL CArc::ControlConstrTantoLineCr()
{
	CLinkLine* link1=m_sl->GetPreviousLink(this);
	CLinkLine* link2=m_sl->GetNextLink(this);
	if(link1==NULL || link2==NULL){
		Message_err("Not found link of tanto!");
		return BAD;
	}
	CLinkLine* line1=NULL;
	CArc* arc=NULL;
	if(link1->IsKindOf( RUNTIME_CLASS( CLinkLine ) )){
		line1=link1;
		if(link2->IsKindOf( RUNTIME_CLASS( CArc ) ))
			arc=(CArc*)link2;
	}
	else
		if(link1->IsKindOf( RUNTIME_CLASS( CArc ) )){
			arc=(CArc*)link1;
		if(link2->IsKindOf( RUNTIME_CLASS( CLinkLine ) ))
			line1=link2;
		}

	LINE_2P line2p();
	double dist1=Pc()->GetDistLine(line1->P(0), line1->P(1));
	if(fabs(dist1-Rad())>DELTA)
		return 2;

	double dist=dist_POINT(Pc(),arc->Pc());
	double delta=dist-(Rad()+arc->Rad());
	if(fabs(delta)>DELTA)
		return 2;

	return OK;
}

CConstraint* CArc::MakeConstrTantoLine()
{
	CLinkLine* link1=m_sl->GetPreviousLink(this);
	CLinkLine* link2=m_sl->GetNextLink(this);
	if(link1==NULL || link2==NULL){
		Message_err("Not found link of tanto!");
		return NULL;
	}
	int nt_line=1;
	int nt_cr=0;

	int nt1=0;
	int nt2=2;
	int dir2=0;

	CLinkLine* line1=NULL;
	CArc* arc=NULL;
	if(link1->IsKindOf( RUNTIME_CLASS( CLinkLine ) )){
		line1=link1;
		if(link2->IsKindOf( RUNTIME_CLASS( CArc ) ))
			arc=(CArc*)link2;
	}
	else
		if(link1->IsKindOf( RUNTIME_CLASS( CArc ) )){
			arc=(CArc*)link1;
		if(link2->IsKindOf( RUNTIME_CLASS( CLinkLine ) ))
			line1=link2;
		nt_line=0;
		nt_cr=2;
		nt1=2;
		nt2=0;
		dir2=1;
		}

	if(line1==NULL || arc==NULL){
		Message_err("Not found link of tanto!");
		return NULL;
	}
	LINE_2P line2p(line1->P(0), line1->P(1));

	int dir=line2p.GetDir(Pc());
	CCircle2D cr(arc->Pc(), arc->Rad());
	int dir1=cr.PointIn(Pc());
	CCircle2D* crt=cr.MakeCircle( &line2p, dir, Rad(), dir1, dir2);
	if(!crt)
		return NULL;

	EditPoint(1, &crt->m_pc, 1);
	LINE_2P line_ortho;
	line2p.MakeOrtho(Pc(), &line_ortho);
	CPoint3d pt1;
	line_ortho.CrossLine2D(&line2p, &pt1);
	line1->EditPoint(nt_line, &pt1, 1);

	EditPoint(nt1, &pt1, 1);
	CPoint3d pt2=*Pc();
	CVector vect(Pc(), arc->Pc());
	pt2.Move(&vect, Rad());
	arc->EditPoint(nt_cr,  &pt2, 1);
	EditPoint(nt2, &pt2, 1);

	CSelectPrim prim;
	prim.ID=m_sl->GetID();
	prim.m_part=m_sl->GetPart();
	prim.num_line=m_sl->GetIndexLine(this);

	CConstraintTanto2* con=new CConstraintTanto2(&prim);
	con->m_var=(BYTE)dir1;
	con->m_var2=(BYTE)dir2;
	con->m_dir=(BYTE)dir;

	delete crt;
	return con;
}

BOOL CArc::UpdateConstrTantoLine(int dir1, int dir2, int dir)
{
	CLinkLine* link1=m_sl->GetPreviousLink(this);
	CLinkLine* link2=m_sl->GetNextLink(this);
	if(link1==NULL || link2==NULL){
		Message_err("Not found link of tanto!");
		return OK;
	}
	int nt_line=1;
	int nt_cr=0;

	int nt1=0;
	int nt2=2;

	CLinkLine* line1=NULL;
	CArc* arc=NULL;
	if(link1->IsKindOf( RUNTIME_CLASS( CLinkLine ) )){
		line1=link1;
		if(link2->IsKindOf( RUNTIME_CLASS( CArc ) ))
			arc=(CArc*)link2;
	}
	else
		if(link1->IsKindOf( RUNTIME_CLASS( CArc ) )){
			arc=(CArc*)link1;
		if(link2->IsKindOf( RUNTIME_CLASS( CLinkLine ) ))
			line1=link2;
		nt_line=0;
		nt_cr=2;
		nt1=2;
		nt2=0;
		dir2=1;
		}

	if(line1==NULL || arc==NULL){
		Message_err("Not found link of tanto!");
		return OK;
	}
	LINE_2P line2p(line1->P(0), line1->P(1));

	CCircle2D cr(arc->Pc(), arc->Rad());
	CCircle2D* crt=cr.MakeCircle( &line2p, dir, Rad(), dir1, dir2);
	if(!crt)
		return BAD;

	EditPoint(1, &crt->m_pc, 1);
	LINE_2P line_ortho;
	line2p.MakeOrtho(Pc(), &line_ortho);
	CPoint3d pt1;
	line_ortho.CrossLine2D(&line2p, &pt1);
	line1->EditPoint(nt_line, &pt1, 1);

	EditPoint(nt1, &pt1, 1);

	CPoint3d pt2=*Pc();
	CVector vect(Pc(), arc->Pc());
	pt2.Move(&vect, Rad());
	arc->EditPoint(nt_cr,  &pt2, 1);
	EditPoint(nt2, &pt2, 1);


	delete crt;
	return OK;
}

double CArc::FindPoint(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0, double k, int* i_min)
{
	CPoint3d pf;
	double dist_min=1e15;
	for(int i=0;i<m_num;i++){
		CPoint3d ptp=*P(i);
		ptp.mod_coord_am(sc);
		ptp.mod_coord_ma(vw);
		ptp.Move(&pf, p0);
		ptp.Zoom(&pf,k, k, k);
		double dist=dist_XY_XY(pm->x, pm->y, ptp.x, ptp.y);
		if(dist_min>dist){
		    dist_min=dist;
		    *i_min=i;
		    }
	}

	CPoint3d p[4];
	GetPoints(p);
	for(int i=0;i<4; i++){
		if(!PontIn(&p[i]))
			continue;
		CPoint3d ptp=p[i];
		ptp.mod_coord_am(sc);
		ptp.mod_coord_ma(vw);
		ptp.Move(&pf, p0);
		ptp.Zoom(&pf,k, k, k);

		double dist=dist_XY_XY(pm->x, pm->y, ptp.x, ptp.y);
		if(dist_min>dist){
			dist_min=dist;
			*i_min=i+m_num;
		}
	}
	
	return dist_min;
}

CSpline* CArc::MakeProection(CPlane*pl, CVector* dir)
{
	CPoint3d pc;
	if(pl->cross_Line(Pc(), dir, &pc)){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	CVector dir1(Pc(), &pc);
	CSpline* spl1=MakeSpline();
	if(!spl1)
		return NULL;
	CSpline* spl2=MakeSpline();
	if(!spl2)
		return NULL;
	spl1->Move(&dir1, dist_POINT(Pc(), &pc));
	spl2->Move(&dir1, dist_POINT(Pc(), &pc));

	spl1->Move(dir, Rad()*3);
	spl2->Move(dir, -Rad()*3);

	CArray<CSpline*> lines;
	lines.Add(spl1);
	lines.Add(spl2);

	CSurface mm;
	if(mm.Create(lines))
		return NULL;
	delete lines[0];
	delete lines[1];
	lines.RemoveAll();

	CArray<CTrimLine* ,CTrimLine* >CrLines;

	if(mm.SectionPlane(&CrLines, pl))
		return NULL;
	CSpline* SplineCr=CrLines[0]->MakeSpline();
	delete CrLines[0];
	CrLines.RemoveAll();

	return SplineCr;
}

BOOL CArc::UpdateFillet(CLinkLine* line2, CFillet* fil, Coord rad)
{
	if(!m_sl)
		return BAD;
	if(line2->IsKindOf( RUNTIME_CLASS( CBezierSpline ) ))
		return UpdateFilletBezierSpline((CBezierSpline*)line2, fil, rad);
	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return UpdateFilletArc((CArc*)line2, fil, rad);

	CPoint3d p0=*PLast();
	CVector LastDir;
	GetLastDir(&LastDir);
	CPoint3d py=p0;
	py.Move(&LastDir, -10);
	CPoint3d px=*line2->P(1);
	CVector c1,c2,c3;
	if(POINTs_SC(&p0, &px, &py, &c1, &c2, &c3)){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	} 
	CArc* arc=(CArc*)copy();
	arc->mod_coord_am(&p0, &c1, &c2, &c3);
	if(arc->Pc()->x<0)
		arc->ChangeRadius(rad);
	else
		arc->ChangeRadius(-rad);

	LINE_2P line(line2->P(0), line2->P(1));
	
    line.mod_coord_am(&p0, &c1, &c2, &c3);
	line.Offset(rad);
	CPoint3d* pc=NULL;
	int nc=arc->CrossLine(&line, &pc);
	delete arc;
	if(nc==0){
		Message_err(IDS_IMPOSIBLE_FILLET);
		fil->m_Ok=0;
		return BAD;
	}
	pc[0].mod_coord_ma(&p0, &c1, &c2, &c3);
	fil->m_pc=pc[0];
	if(nc>1){
		pc[1].mod_coord_ma(&p0, &c1, &c2, &c3);
		if(dist_POINT(&p0, &pc[0])>dist_POINT(&p0, &pc[1]))
			fil->m_pc=pc[1];
	}
	free(pc);
	GetPointOrtho(&fil->m_pc, &fil->m_p1);
	line2->GetPointOrtho(&fil->m_pc, &fil->m_p2);
	m_fillet2=line2->m_fillet1=1;
	fil->m_Ok=1;
	return OK;
}

BOOL CArc::UpdateFilletBezierSpline(CBezierSpline* line2, CFillet* fil, Coord rad)
{
	CPoint3d p0=*PLast();
	CVector LastDir;
	GetLastDir(&LastDir);
	CPoint3d py=*PLast();
	py.Move(&LastDir, -10);
	CPoint3d px=*line2->P(1);
	CVector c1,c2,c3;
	if(POINTs_SC(&p0, &px, &py, &c1, &c2, &c3)){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	} 
	CArc* arc=(CArc*)copy();
	arc->mod_coord_am(&p0, &c1, &c2, &c3);
	if(arc->Pc()->x<0)
		arc->ChangeRadius(rad);
	else
		arc->ChangeRadius(-rad);
	CSpline* cpline=line2->MakeSplineAllLegth();
	if(!cpline)
		return BAD;
    cpline->mod_coord_am(&p0, &c1, &c2, &c3);
	cpline->Offset(rad);
	CPoint3d* pc=NULL;
	int nc=arc->CrossLine(cpline, &pc);
	delete cpline;
	delete arc;
	if(nc==0){
		Message_err(IDS_IMPOSIBLE_FILLET);
		fil->m_Ok=0;
		return BAD;
	}
	pc[0].mod_coord_ma(&p0, &c1, &c2, &c3);
	fil->m_pc=pc[0];
	if(nc>1){
		pc[1].mod_coord_ma(&p0, &c1, &c2, &c3);
		if(dist_POINT(&p0, &pc[0])>dist_POINT(&p0, &pc[1]))
			fil->m_pc=pc[1];
	}
	free(pc);
	GetPointOrtho(&fil->m_pc, &fil->m_p1);
	double s=line2->GetOrthPoint(&fil->m_pc);
	line2->GetPoint(s, &fil->m_p2);
	m_fillet2=line2->m_fillet1=1;
	fil->m_Ok=1;
	return OK;
}

BOOL CArc::UpdateFilletArc(CArc* link2, CFillet* fil, Coord rad)
{
	if(!m_sl)
		return BAD;
	CLine* line1=MakeLine(Rad()/5.0);
	line1->Revers();
	CPoint3d p1;
	line1->GetPoint(0, Rad()/5.0, &p1);
	CPoint3d p2;
	CLine* line2=link2->MakeLine(link2->Rad()/5.0);
	line2->GetPoint(0, Rad()/5.0, &p2);
	CArc* arc1=(CArc*)copy();
	if(dist_POINT(Pc(), &p2)>Rad())
		arc1->ChangeRadius(rad);
	else
		arc1->ChangeRadius(-rad);
	CArc* arc2=(CArc*)link2->copy();
	if(dist_POINT(arc2->Pc(), &p1)>arc2->Rad())
		arc2->ChangeRadius(rad);
	else
		arc2->ChangeRadius(-rad);
	CSystemCoord dsc;
	m_sl->m_pDoc->GetSystemCoord(&dsc);
	CSystemCoord msc;
	m_sl->GetSystemCoord(&msc);
	arc1->mod_coord_ma(&dsc);
	arc1->mod_coord_am(&msc);
	arc2->mod_coord_ma(&dsc);
	arc2->mod_coord_am(&msc);
	CCircle2D cr1(arc1->Pc(), arc1->Rad());
	CCircle2D cr2(arc2->Pc(), arc2->Rad());
	CPoint3d pc1;
	CPoint3d pc2;

	int nc=cr1.CrossCircleG(&cr2, &pc1, &pc2);
	delete arc1;
	delete arc2;
	if(nc==0){
		Message_err(IDS_IMPOSIBLE_FILLET);
		fil->m_Ok=0;
		return BAD;
	}
	CPoint3d p0=*PLast();
	p0.mod_coord_ma(&dsc);
	p0.mod_coord_am(&msc);

	CPoint3d pc=pc1;
	if(dist_POINT(&p0, &pc1)>dist_POINT(&p0, &pc2))
		pc=pc2;
	pc.mod_coord_ma(&msc);
	pc.mod_coord_am(&dsc);
	fil->m_pc=pc;
	GetPointOrtho(&pc, &fil->m_p1);
	link2->GetPointOrtho(&pc, &fil->m_p2);
	m_fillet2=link2->m_fillet1=1;
	fil->m_Ok=1;
	return OK;
}

bool CArc::EditRadius(CPoint3d* p)
{
	CCircle2D c2d(P(0), P(2), p);
	if(c2d.m_rad==0){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	m_rad=c2d.m_rad;
	Pc()->x=c2d.m_pc.x;
	Pc()->y=c2d.m_pc.y;

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}	
	if(!PontIn(p))
		ChangeDir();
	return OK;
}


void CArc::EditCurvature(CPoint3d* ,CPoint3d* p2)
{
	CCircle2D c2d(P(0), P(2), p2);
	if(c2d.m_rad==0)
		return ;

	m_rad=c2d.m_rad;
	Pc()->x=c2d.m_pc.x;
	Pc()->y=c2d.m_pc.y;

	for(int i=0;i<3;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}	
	if(!PontIn(p2))
		ChangeDir();

}

void CArc::GetPoint(Coord t, CPoint3d * pt)// t=0-1;
{
	CPoint3d ps[1000];
	int np=InitPoints(0.01,  ps);
	CLine line(ps, np);

	line.GetPoint(0, line.GetLength()*t, pt);
}

Coord CArc::GetOrthPoint(CPoint3d* p)
{
	CPoint3d ps[1000];
	int np=InitPoints(0.01,  ps);
	CLine line(ps, np);

	return line.GetOrthPoint(p);
}

void CArc::GetPointLength(double len,CPoint3d* p)
{
	CPoint3d ps[1000];
	int np=InitPoints(0.01,  ps);
	CLine line(ps, np);

	line.GetPoint(0, len, p);

/*
CAlfaDoc* pDoc=GetAlfaDoc();
pDoc->AddFigure(new CAPoint3d(p));
pDoc->AddFigure(new CLineFig(&line));
*/
}


bool  m_wrk_functionCArc(CSelectPrim& Prim, CVector* dir,double dist)
{
	CAlfaDoc* pDoc=GetAlfaDoc();
	if(!pDoc)
		return false;

	CSmartLine* fig=(CSmartLine*)pDoc->GetFigure(Prim.m_part, Prim.ID);
	if(!fig)
		return false;

	CPoint3d p;
	fig->GetPoint(&Prim, &p);
	p.Move(dir, dist);
		
	if(fig->EditPoint(&Prim, &p, 1))
		return false;
	if(CArc::m_wrk_function)
		return (CArc::m_wrk_function)(Prim,  dir, dist);
	return true;
}

bool  m_wrk_functionCArc2(CSelectPrim& Prim, CVector* dir,double dist)
{
	CAlfaDoc* pDoc=GetAlfaDoc();
	if(!pDoc)
		return false;

	CSmartLine* fig=(CSmartLine*)pDoc->GetFigure(&Prim);
	if(!fig)
		return false;

	CArc* arc=(CArc*)fig->GetLink(Prim.num_line);
	ASSERT(arc!=NULL);

	CPoint3d midle_p;
	arc->GetPointLength(arc->GetLength()/2.0, &midle_p);
	CPoint3d p2=midle_p;
	p2.Move(dir, dist);

	arc->EditCurvature(&midle_p, &p2);

	if(CArc::m_wrk_function)
		return (CArc::m_wrk_function)(Prim,  dir, dist);
	return true;
}


bool CArc::IsRadiusConstraint()
{
	CTypedPtrArray<CObArray, CConstraint*> constr;
	m_sl->GetConstraints(this, &constr );
	if(!constr.GetSize())
		return false;

	for(int i=0;i<constr.GetSize(); i++)
		if(constr[i]->GetType()==CONSTR_RAD)
			return true;

	return false;
}
