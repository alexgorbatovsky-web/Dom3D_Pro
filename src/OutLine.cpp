// OutLine.cpp: implementation of the COutLine class.
//
//////////////////////////////////////////////////////////////////////

#include "OutLine.h"
#include "LineStyleBox.h"
#include "CAlfaDoc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

COutLine::COutLine()
{
	Alloc(0);

}

COutLine::COutLine(int np)
{
	Alloc(np);
}

COutLine::COutLine(CPoint3d& p1, CPoint3d& p2)
{
	if(Alloc(2))
		return;
	m_p[0]=p1;
	m_p[1]=p2;
}

COutLine::~COutLine()
{
    if(m_p)
		free(m_p);
    m_p=NULL;
    m_n=0;
}

BOOL COutLine::Alloc(int num_p)
{
	m_p=NULL;
	if(num_p){
		m_p=(CPoint3d*)calloc(num_p,sizeof(CPoint3d));
		if(m_p==0){
			m_n=0;
			Message_err(IDS_BAD_ALLOC_MEMORY);
			return BAD;
			}
	}
	m_n=num_p;
	return OK;
}

BOOL COutLine::Realloc(int num_p)
{

CPoint3d* ptr =(CPoint3d*)realloc((char*)m_p,num_p*sizeof(CPoint3d));
    if(ptr==0){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return BAD;
		}
	m_p=ptr;
    m_n=num_p;
    return OK;
}

BOOL COutLine::AddPoint(CPoint3d* add_p)
{
	if(Realloc(m_n+1))
		return BAD;
    m_p[m_n-1]=*add_p;
    return 0;
}

BOOL COutLine::InsertPoint(int np,CPoint3d *p)
{
    if(np>=m_n|| np<0){
//		Message_err("BAD Number of Point to insert!!");
		return BAD;
	}

Coord dist=dist_POINT(&m_p[np],p);
    if(dist<DDELTA)
		return OK;

CPoint3d* ptr=(CPoint3d *)realloc((char*)m_p,(m_n+1)*sizeof(CPoint3d));
    if(ptr==NULL){
		message_error_(IDS_BAD_ALLOC_MEMORY);
		return BAD;
	}
	m_p=ptr;
    for(register int i=m_n;i>np;i--)
		m_p[i]=m_p[i-1];
    m_p[np+1]=*p;
    m_n++;

	return OK;
}

CPoint3d* COutLine::P(int num_p)
{

	if(num_p>=m_n){
		#ifdef _DEBUG
			Message_err("Bad index Points");
		#endif
		return m_p;
	}
	return &m_p[num_p];
}

void COutLine::Draw()
{
if(this==NULL)
    return;

	glBegin(GL_LINE_STRIP);
		for (int i=0; i<m_n; i++)
			glVertex3d(P(i)->x,P(i)->y, P(i)->z);
	glEnd();

}

void COutLine::Move(CVector* vect,double dist)
{
if(this==NULL)
    return;
    for(int i=0;i<m_n;i++)
		P(i)->Move(vect,dist);
}

void COutLine::Move(CPoint3d* p1,CPoint3d* p2)
{
if(this==NULL)
    return;
CVector vect;
double dist=vect.calc(p1, p2);

    Move(&vect,dist);
}

void COutLine::Mirror()
{
if(this==NULL)
    return;
    for(int i=0;i<m_n;i++)
		P(i)->Mirror();
}
void COutLine::Zoom(CPoint3d* p0,double Kx,double Ky,double Kz)
{
if(this==NULL)
    return;
    for(int i=0;i<m_n;i++)
		P(i)->Zoom(p0, Kx, Ky, Kz);
}

void COutLine::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
if(this==NULL)
    return;
    for(int i=0;i<m_n;i++)
		P(i)->mod_coord_ma(p0,cx,cy,cz);
}

void COutLine::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
if(this==NULL)
    return;
    for(int i=0;i<m_n;i++)
		P(i)->mod_coord_am(p0,cx,cy,cz);
}

void COutLine::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}
void COutLine::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void COutLine::Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size)
{
	CPoint3d p0;
	CPoint3d pm;
	size->X_min=size->Y_min=size->Z_min=1e15;
	size->X_max=size->Y_max=size->Z_max=-1e15;

	for(int i=0;i<m_n;i++){
		pm=*P(i);
		pm.mod_coord_ma(&p0, cx, cy, cz);
		size->X_min=MIN(size->X_min,pm.x);
		size->X_max=MAX(size->X_max,pm.x);
		size->Y_min=MIN(size->Y_min,pm.y);
		size->Y_max=MAX(size->Y_max,pm.y);
		size->Z_min=MIN(size->Z_min,pm.z);
		size->Z_max=MAX(size->Z_max,pm.z);
		}

}

double COutLine::Get_dist_Min(CPoint3d* pm,  CView3d* view)
{
	if(this==NULL)
		return 1e15;
	register double dist_min=1e15;
	CPoint3d pgr1;
	CPoint3d pgr2;
    if(m_n<2)
		return dist_min;
    for(int i=0;i<m_n-1;i++){
		P(i)->GetGrPoz(view, &pgr1);
		P(i+1)->GetGrPoz(view, &pgr2);
		register double dist=get_dist_XY_line2D(pm->x, pm->y, pgr1.x, pgr1.y, pgr2.x, pgr2.y);
		if(dist_min>dist)
			dist_min=dist;
		}
 
   return dist_min;
}

double COutLine::Get_dist_Min2D(CPoint3d* pm)
{
	if(this==NULL)
		return 1e15;
	register double dist_min=1e15;
    if(m_n<2)
		return dist_min;
    for(int i=0;i<m_n-1;i++){
		register double dist=get_dist_XY_line2D(pm->x, pm->y, P(i)->x, P(i)->y, P(i+1)->x, P(i+1)->y);
		if(dist_min>dist)
			dist_min=dist;
		}
   return dist_min;
}

double COutLine::Get_dist_MinAndPointMin(CPoint3d* p, CPoint3d* pmin)
{
	if (this == NULL)
		return 1e15;
	double dist_min = 1e15;
	for (int i = 0; i < m_n ; i++) {
		double dist = p->DistTo(P(i));
		if (dist_min > dist) {
			*pmin = *P(i);
			dist_min = dist;
		}
	}
	return dist_min;
}

double COutLine::Get_dist_Min(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, double k, CPoint3d* p0)
{
	CPoint3d pf;
	double dist_min=1e15;
    for(int i=0;i<m_n-1;i++){
		CPoint3d pt1=*P(i);
		CPoint3d pt2=*P(i+1);

		pt1.mod_coord_am(sc);
		pt1.mod_coord_ma(vw);
		pt1.Move(&pf, p0);
		pt1.Zoom(&pf,k, k, k);

		pt2.mod_coord_am(sc);
		pt2.mod_coord_ma(vw);
		pt2.Move(&pf, p0);
		pt2.Zoom(&pf,k, k, k);
		
		register double dist=get_dist_XY_line2D(pm->x, pm->y, pt1.x, pt1.y, pt2.x, pt2.y);
		if(dist_min>dist)
			dist_min=dist;
		}

	
	return dist_min;
}


