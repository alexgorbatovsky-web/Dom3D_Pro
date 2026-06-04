// SystemCoord.cpp : implementation file
//


#include "SystemCoord.h"
//#include "Point3dBox.h"
//#include "Line_2P.h"
//#include "Line.h"
#include "CAlfaDoc.h"
#include "CView3d.h"
//#include "service.h"
#include "Plane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSystemCoord

CSystemCoord::CSystemCoord()
:p0(0,0,0), cx(1,0,0), cy(0,1,0), cz(0,0,1)
{

}

CSystemCoord::CSystemCoord(CPoint3d *p,CVector* cxi,CVector* cyi,CVector* czi)
{
	p0=*p;
	cx=*cxi;
	cy=*cyi;
	cz=*czi;
}


CSystemCoord::CSystemCoord(CPoint3d *p0i,CVector *v0)
:p0(0,0,0), cx(1,0,0), cy(0,1,0), cz(0,0,1)
{
	p0=*p0i;
    v0->GetOrth(&cx);
    if(cy.MultVectors( &cx, v0))
		return ;
    cz.MultVectors(&cx, &cy);
}

CSystemCoord::CSystemCoord(CPoint3d *p0i, CPoint3d *px,  CPoint3d *py)
{
	p0=*p0i;

	CVector cxt(p0i, px);
	cx=cxt;

	CVector cyt( p0i, py);
	CVector czt( &cx, &cyt);
	cz = czt;

	CVector cyt2( &cz, &cx);
	cy=cyt2;

}



CSystemCoord::~CSystemCoord()
{
}

void CSystemCoord::mod_coord_am(CSystemCoord* sc)
{
	p0.mod_coord_am(sc);
	cx.mod_coord_am(sc);
	cy.mod_coord_am(sc);
	cz.mod_coord_am(sc);
}

void CSystemCoord::mod_coord_ma(CSystemCoord* sc)
{
	p0.mod_coord_ma(sc);
	cx.mod_coord_ma(sc);
	cy.mod_coord_ma(sc);
	cz.mod_coord_ma(sc);
}

/////////////////////////////////////////////////////////////////////////////
// CSystemCoord message handlers

BOOL CSystemCoord::GetMirrorSC(CPlane* pl)
{
CPoint3d pn;
    pl->CrossNormal(&pn, &p0);//опускаем нормаль на плоскость

    cz.l=pl->a;
    cz.m=pl->b;
    cz.n=pl->c;
    cz.GetOrth(&cx);

	return cy.MultVectors(&cz, &cx);
}
/*
void ModifMsc(short init, CAlfaDoc* pDoci, void* )
{
static char step=0;
static CPoint3d p0;
static CPoint3d px;
static CPoint3d py;
static CAlfaDoc* pDoc;

	if(init){
		step=1;
		pDoc=pDoci;
		GetPoint(&p0, pDoc, ModifMsc);
		Message_user(IDS_POINT_ORIGIN_SC);
		return;
		}

	if(step==1){
		step=2;
		GetPoint(&px, pDoc, ModifMsc);
		Message_user(IDS_POINT_AXIS_X);
		return;
		}
	if(step==2){
		step=3;
		GetPoint(&py, pDoc, ModifMsc);
		Message_user(IDS_POINT_PL_XY);
		return;
		}

CPoint3d P0;
CVector cxt,cyt,czt;
	pDoc->GetSystemCoord(&P0, &cxt, &cyt, &czt);
	pDoc->WriteUndoMSK(&P0, &cxt, &cyt, &czt);////////Сохраняем старую СК

    p0.mod_coord_ma(&P0, &cxt, &cyt, &czt);
    px.mod_coord_ma(&P0, &cxt, &cyt, &czt);
    py.mod_coord_ma(&P0, &cxt, &cyt, &czt);

    if(POINTs_SC(&p0, &px, &py, &cxt, &cyt, &czt)){
		Message_err(IDS_ZERRO_POINTS);	
		pDoc->PopUndo();
		return ;
		}

	pDoc->SetSystemCoord(&p0, &cxt, &cyt, &czt);
}
*/
/*
void ModifPointOriginSc(short init, CAlfaDoc* pDoci, void* )
{
static CPoint3d p0;
static CAlfaDoc* pDoc;

	if(init){
		pDoc=pDoci;
		GetPoint(&p0, pDoc, ModifPointOriginSc);
		Message_user(IDS_POINT_ORIGIN_SC);
		return;
		}

	CPoint3d P0;
	CVector cxt,cyt,czt;
	pDoc->GetSystemCoord(&P0, &cxt, &cyt, &czt);
	pDoc->WriteUndoMSK(&P0, &cxt, &cyt, &czt);////////Сохраняем старую СК

    p0.mod_coord_ma(&P0, &cxt, &cyt, &czt);
	pDoc->SetSystemCoord(&p0, &cxt, &cyt, &czt);
}
*/
BOOL CSystemCoord::Set(CPoint3d* p0i, CPoint3d* px, CPoint3d* py)
{
	p0=*p0i;

	return POINTs_SC(p0i, px, py, &cx, &cy, &cz);
}

bool CSystemCoord::IsValid()
{

	CPoint3d px=p0;
	CPoint3d py=p0;
	px.Move(&cx, 100);
	py.Move(&cy, 100);
	if(POINTs_SC(&p0, &px, &py, &cx, &cy, &cz))
		return false;
	return true;
}
	
void CSystemCoord::print()
{
//	Step("========CSystemCoord==========");
	FILE* strm =NULL;
	p0.print("p0", strm);

	cx.print("cx");
	cy.print("cy");
	cz.print("cz");
}

void CSystemCoord::GetEulerAngles(double& alfa, double& beta, double& gamma, CPoint3d& p1, CPoint3d& p2)
{
	CPlane pl1(0, 0, 1, 0); // Plane XY
	CPlane pl2(&p0, &cz); // Plane xy this System Coordinate
	pl1.CroossCPlane(&pl2, &p1, &p2);
//	AddLine(&p1, &p2, RGB_RED);
	CVector N(&p1, &p2);
	CVector acx(1,0,0);
	alfa = acx.GetAngle(&N);
	CVector acz(0, 0, 1);
	CVector cz2(&acx, &N);
	if (!acz.IsCongruent(&cz2))
		alfa = -alfa;

	beta = cz.GetAngle(&acz);
	CVector cz3(&acz, &cz);
	if (!N.IsCongruent(&cz3))
		beta = -beta;
	gamma = N.GetAngle(&cx);
	CVector cz4(&N, &cx);
	if (!cz.IsCongruent(&cz4))
		gamma = -gamma;
}