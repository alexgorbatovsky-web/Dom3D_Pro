// Point5d.cpp: implementation of the CPoint5d class.
//
//////////////////////////////////////////////////////////////////////
#include "ageom.h"
#include "Point5d.h"
#include "Vector.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPoint5d::CPoint5d()
{

	x=y=z=s=t=0;
}

void CPoint5d::Move(CVector* dir,double dist)
{

    x+=dir->l*dist;
    y+=dir->m*dist;
    z+=dir->n*dist;
}

void CPoint5d::Move(CPoint3d* p1, CPoint3d* p2)
{
CVector vect;
double dist=vect.calc(p1,p2);

    Move(&vect, dist);
}

void CPoint5d::Zoom(CPoint3d* pf,double Sx,double Sy,double Sz )
{
	double Cx=pf->x-Sx*pf->x;
	double Cy=pf->y-Sy*pf->y;
	double Cz=pf->z-Sz*pf->z;

    x=Sx*x+Cx;
    y=Sy*y+Cy;
    z=Sz*z+Cz;

    s=Sx*s+Cx;
    t=Sy*t+Cy;

}

void CPoint5d::Mirror()
{
	z=-z;
}


void CPoint5d::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

double xa=cx->l*x+cy->l*y+cz->l*z+p0->x;
double ya=cx->m*x+cy->m*y+cz->m*z+p0->y;
double za=cx->n*x+cy->n*y+cz->n*z+p0->z;
    x=xa;
    y=ya;
    z=za;    
}

void CPoint5d::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
 
double xm=cx->l*(x-p0->x)+cx->m*(y-p0->y)+cx->n*(z-p0->z);
double ym=cy->l*(x-p0->x)+cy->m*(y-p0->y)+cy->n*(z-p0->z);
double zm=cz->l*(x-p0->x)+cz->m*(y-p0->y)+cz->n*(z-p0->z);
    x=xm;
    y=ym;
    z=zm;   
}

void CPoint5d::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint5d::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}


BOOL CPoint5d::Offset(double offset, CPoint3d* p1,CPoint3d* p2)
{
double a, b, c;

	if(XY_XY_ABC(p1->x, p1->y, p2->x, p2->y, &a, &b, &c))
		return BAD;

    x+=a*offset;
    y+=b*offset;
	s=x;
	t=y;
	return OK;
}
