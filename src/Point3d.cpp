//Point3d	: implementation of the Points class
//

#include "ageom.h"
#include "SizeBlock.h"
#include "CView3d.h"
#include "Point3d.h"
#include "SystemCoord.h"

#ifndef String
#define String char*
#endif
#ifndef XtPointer
#define XtPointer void*
#endif
#ifndef Coord
#define Coord double
#endif
#ifndef Pixel
#define Pixel unsigned long
#endif
#ifndef LPCTSTR
#define LPCTSTR const char*
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
//Пробуем сделать константами оси координат, чтобы не создавать их каждый раз при повороте

const CPoint3d CPoint3d::AxisX(1.0f, 0.0f, 0.0f);
const CPoint3d CPoint3d::AxisY(0.0f, 1.0f, 0.0f);
const CPoint3d CPoint3d::AxisZ(0.0f, 0.0f, 1.0f);
const CPoint3d CPoint3d::AxisNegX(-1.0f, 0.0f, 0.0f);
const CPoint3d CPoint3d::AxisNegY(0.0f, -1.0f, 0.0f);
const CPoint3d CPoint3d::AxisNegZ(0.0f, 0.0f, -1.0f);

const CPoint3d CPoint3d::Cross(const CPoint3d& u, const CPoint3d& v) {
	return CPoint3d(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
}

CPoint3d::CPoint3d(CPoint8d* p)
{
	x = p->x;
	y = p->y;
	z = p->z;
}
CPoint3d::CPoint3d(CPoint7d* p)
{
	x = p->x;
	y = p->y;
	z = p->z;
}
CPoint3d::CPoint3d(CPoint3d* p)
{
	x = p->x;
	y = p->y;
	z = p->z;
}

void CPoint3d::print(FILE* strm)
{
	int str_null = 0;
	if (!strm) {
		str_null = 1;
		fopen_s(&strm, "c:\\temp\\stdout.txt", "a+");
	}
	if (strm == NULL)
		return;
	fprintf(strm, " %10.4f %10.4f %10.4f\n", x, y, z);
	if (str_null)
		fclose(strm);
}

void CPoint3d::print(const char* text, FILE* strm)
{
	int str_null = 0;
	if (!strm) {
		str_null = 1;
		fopen_s(&strm, "c:\\temp\\stdout.txt", "a+");
	}
	if (strm == NULL)
		return;
	fprintf(strm, "%s %10.4f %10.4f %10.4f\n", text, x, y, z);
	if (str_null)
		fclose(strm);

}

void CPoint3d::Move(CVector* dir, double dist)
{

	x += dir->l * dist;
	y += dir->m * dist;
	z += dir->n * dist;
}

void CPoint3d::Move(CPoint3d* p1, CPoint3d* p2)
{
	CVector vect;
	double dist = vect.calc(p1, p2);

	Move(&vect, dist);
}

void CPoint3d::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
	CVector vect;
	double dist = vect.calc(p0, p1);

	if (dist < DDELTA)
		return;
	Rotate(p0, &vect, alfa);
}

void CPoint3d::Rotate(CPoint3d* p0, CVector* dir, double alfa)
{
	if (alfa == 0)
		return;

	CSystemCoord system(p0, dir);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm(cos(alfa), sin(alfa), 0);
	CVector cym(-sin(alfa), cos(alfa), 0);
	CVector czm(0, 0, 1);

	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}



void CPoint3d::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx = pf->x - Sx * pf->x;
	double Cy = pf->y - Sy * pf->y;
	double Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}

void CPoint3d::Mirror()
{
	z = -z;
}

void CPoint3d::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = xa;
	y = ya;
	z = za;
}

void CPoint3d::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = xm;
	y = ym;
	z = zm;
}

void CPoint3d::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint3d::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

/*
CArchive& AFXAPI operator<<(CArchive& ar, const CPoint3d& p)
{	ar << p.x << p.y << p.z;
	return ar;
}

CArchive& AFXAPI operator>>(CArchive& ar, CPoint3d& p)
{	ar >> p.x >> p.y >> p.z;
	return ar;
}
*/
void CPoint3d::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
/*
	if (pv == NULL) {
		Message_err("CView3d==NULL!");
		return;
	}
	if (!pv->IsKindOf(RUNTIME_CLASS(CView3d)))	//pr==1 если view==CView3d
		return;

	if (pv->m_ProjType != 0) {
		pv->Project(this, pgr);
		return;
	}


	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
	*/
}

void CPoint3d::SetGrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;
/*
	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_ma(&p0, &cx, &cy, &cz);
	*/
}

void CPoint3d::GetAgrPoz(CView3d* pv, CPoint3d* pa)
{
	CVector cx, cy, cz;
	CPoint3d p0;
/*
	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pa->x = x;
	pa->y = y;
	pa->z = z;
	pa->mod_coord_am(&p0, &cx, &cy, &cz);
	*/
}
void CPoint3d::SetAgrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;
/*
	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_am(&p0, &cx, &cy, &cz);
	*/
}
/*
void CPoint3d::Read(char* str)
{
	if (str == NULL)
		return;
	char buf[BUFSIZE];
	strncpy(buf, str, BUFSIZE);
	String str1 = NULL;
	str1 = strtok(buf, " 	");
	if (str1 == NULL) {
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	CAlfaDoc* pDoc = GetAlfaDoc();
	if (pDoc)
		x = pDoc->GetValue(str1);
	else
		x = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		y = 0;
		z = 0;
		return;
	}
	if (pDoc)
		y = pDoc->GetValue(str1);
	else
		y = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		z = 0;
		return;
	}
	if (pDoc)
		z = pDoc->GetValue(str1);
	else
		z = atof(str1);
}
*/
void CPoint3d::Shift(CVector* vect, double dist, CPoint3d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
}
void CPoint3d::Shift(CVector* vect, double dist, CPoint8d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
}
BOOL CPoint3d::InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);
	CPoint3d pgr;
	GetGrPoz(view, &pgr);
	if (pgr.x >= X_min && pgr.x <= X_max && pgr.y >= Y_min && pgr.y <= Y_max)
		return 1;
	return 0;
}

BOOL CPoint3d::InRect(CPoint3d* p1, CPoint3d* p2)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);

	if (x >= X_min && x <= X_max && y >= Y_min && y <= Y_max)
		return 1;
	return 0;
}

bool CPoint3d::IsPointIn(CSizeBlock* block)
{
	if ((x <= block->X_max && x >= block->X_min) && \
		(y <= block->Y_max && y >= block->Y_min) && \
		z <= block->Z_max && z >= block->Z_min)
		return true;
	return false;
}

BOOL CPoint3d::Cross2DLine(CPoint3d* p11, CPoint3d* p12, CPoint3d* p21, CPoint3d* p22)
{
	double a1, b1, c1;
	double a2, b2, c2;

	XY_XY_ABC(p11->x, p11->y, p12->x, p12->y, &a1, &b1, &c1);
	XY_XY_ABC(p21->x, p21->y, p22->x, p22->y, &a2, &b2, &c2);
	x = p12->x;
	y = p12->y;
	return cross2_line_2D(a1, b1, c1, a2, b2, c2, &x, &y);

}

BOOL CPoint3d::Offset(double offset, CPoint3d* p1, CPoint3d* p2)
{
	double a, b, c;

	if (XY_XY_ABC(p1->x, p1->y, p2->x, p2->y, &a, &b, &c))
		return BAD;

	x += a * offset;
	y += b * offset;
	return OK_AG;
}
/*
void CPoint3d::GetOffsetPoint(LINE_2P* line, double offset, CPoint3d* p)
{
	p->x = x + line->a * offset;
	p->y = y + line->b * offset;
	p->z = z;
}*/

double CPoint3d::GetDistLine(CPoint3d* p1, CPoint3d* p2)
{
	CVector line(p1, p2);
	long double	q = qxyz(line.l, line.m, line.n);
	if (q < DDELTA)
		return dist_POINT(this, p1);

	long double	q1 = line.m * (p1->z - z) - line.n * (p1->y - y);
	long double	q2 = line.n * (p1->x - x) - line.l * (p1->z - z);
	long double	q3 = line.l * (p1->y - y) - line.m * (p1->x - x);
	return qxyz(q1, q2, q3) / q;
}

double CPoint3d::GetDistLine(CPoint3d* p1, CVector* line)
{
	long double	q = qxyz(line->l, line->m, line->n);
	if (q < DDELTA)
		return dist_POINT(this, p1);

	long double	q1 = line->m * (p1->z - z) - line->n * (p1->y - y);
	long double	q2 = line->n * (p1->x - x) - line->l * (p1->z - z);
	long double	q3 = line->l * (p1->y - y) - line->m * (p1->x - x);
	return qxyz(q1, q2, q3) / q;
}


double CPoint3d::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

double CPoint3d::GetDistLineSegment(CPoint3d* p1, CPoint3d* p2)
{
	if (p1->DistTo(p2) < 0.001)
		return DistTo(p1);

	if (!In_P1_P2(p1, p2)) {
		double dist1 = DistTo(p1);
		double dist2 = DistTo(p2);
		if (dist1 < dist2)
			return dist1;
		return dist2;
	}

	return GetDistLine(p1, p2);
}
double CPoint3d::GetDistLineSegment(CVertex5f& p1, CVertex5f& p2)
{
	CPoint3d p1d(p1.x, p1.y, p1.z);

	if (p1.DistTo(p2) < 0.001)
		return DistTo(&p1d);
	CPoint3d p2d(p2.x, p2.y, p2.z);

	if (!In_P1_P2(&p1d, &p2d)) {

		double dist1 = DistTo(&p1d);
		double dist2 = DistTo(&p2d);
		if (dist1 < dist2)
			return dist1;
		return dist2;
	}

	return GetDistLine(&p1d, &p2d);
}


//short POINT_IN_POINT1_POINT2(POINT *p,POINT *p1,POINT *p2)
int CPoint3d::In_P1_P2(CPoint3d* p1, CPoint3d* p2)
{
	/*
		double delta= 0.001;
		CVector v(p1, p2);
		CPlane pl(this, &v);
		double de1=p1->x*pl.a+p1->y*pl.b+p1->z*pl.c+pl.d;
		double de2=p2->x*pl.a+p2->y*pl.b+p2->z*pl.c+pl.d;
		if(fabs(de1)<delta || fabs(de2)<delta)
			return 1;
		if((de1>delta  && de2<-delta)||(de1<-delta && de2>delta))
			return 1;
		return 0;
	*/
	double delta = 0.0001;
	CVector v(p2, p1);
	v.Normalize();
	CPoint3d p0(x, y, z);
	CPlane pl(&p0, &v);

	double de1 = p1->x * pl.a + p1->y * pl.b + p1->z * pl.c + pl.d;
	double de2 = p2->x * pl.a + p2->y * pl.b + p2->z * pl.c + pl.d;
	if (fabs(de1) < delta || fabs(de2) < delta)
		return true;
	if ((de1 > delta && de2 < -delta) || (de1<-delta && de2>delta))
		return true;
	return false;

}
bool CPoint3d::IsPointInSegment(CPoint3d& p1, CPoint3d& p2)
{
	double delta = 0.0001;
	CVector v(p1, p2);
	v.Normalize();
	CPoint3d p0(x, y, z);
	CPlane pl(&p0, &v);

	double de1 = p1.x * pl.a + p1.y * pl.b + p1.z * pl.c + pl.d;
	double de2 = p2.x * pl.a + p2.y * pl.b + p2.z * pl.c + pl.d;
	if (fabs(de1) < delta || fabs(de2) < delta)
		return true;
	if ((de1 > delta && de2 < -delta) || (de1<-delta && de2>delta))
		return true;
	return false;
}



///////////////////////////////////////////////////////////////////////////
//////////////////////////CPoint8d////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void CPoint8d::Move(CVector* dir, double dist)
{

	x += dir->l * dist;
	y += dir->m * dist;
	z += dir->n * dist;
}

void CPoint8d::Move(CPoint3d* p1, CPoint3d* p2)
{
	CVector vect;
	double dist = vect.calc(p1, p2);

	Move(&vect, dist);
}

void CPoint8d::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
	if (alfa == 0)
		return;
	if (dist_POINT(p0, p1) == 0)
		return;
	CVector vx(p0, p1);
	CSystemCoord system(p0, &vx);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm(cos(alfa), sin(alfa), 0);
	CVector cym(-sin(alfa), cos(alfa), 0);
	CVector czm(0, 0, 1);

	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}

void CPoint8d::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx = pf->x - Sx * pf->x;
	double Cy = pf->y - Sy * pf->y;
	double Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}

void CPoint8d::Mirror()
{
	z = -z;
	n = -n;
}

void CPoint8d::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = xa;
	y = ya;
	z = za;
	double la = l * cx->l + m * cy->l + n * cz->l;
	double ma = l * cx->m + m * cy->m + n * cz->m;
	double na = l * cx->n + m * cy->n + n * cz->n;
	l = la;
	m = ma;
	n = na;
}

void CPoint8d::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = xm;
	y = ym;
	z = zm;

	double lm = l * cx->l + m * cx->m + n * cx->n;
	double mm = l * cy->l + m * cy->m + n * cy->n;
	double nm = l * cz->l + m * cz->m + n * cz->n;
	l = lm;
	m = mm;
	n = nm;
}

void CPoint8d::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint8d::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint8d::Read(LPCTSTR str)
{
	if (str == NULL)
		return;
	char buf[BUFSIZE];
	strncpy(buf, str, BUFSIZE);
	String str1 = NULL;
	str1 = strtok(buf, " 	");
	if (str1 == NULL) {
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	x = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		y = 0;
		z = 0;
		return;
	}
	y = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		z = 0;
		return;
	}
	z = atof(str1);
}

void CPoint8d::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
	if (pv->m_ProjType != 0) {
		pv->Project((CPoint3d*)this, pgr);
		return;
	}

	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
}
void CPoint8d::GetAgrPoz(CView3d* pv, CPoint3d* pa)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pa->x = x;
	pa->y = y;
	pa->z = z;
	pa->mod_coord_am(&p0, &cx, &cy, &cz);
}

void CPoint8d::SetAgrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_am(&p0, &cx, &cy, &cz);
}
void CPoint8d::SetGrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_ma(&p0, &cx, &cy, &cz);
}

void CPoint8d::print(FILE* strm)
{
	fprintf(strm, " %10.4f %10.4f %10.4f  %7.5f  %7.5f  %7.5f  %7.5f  %7.5f\n", x, y, z, l, m, n, s, t);
}

void CPoint8d::print(LPCTSTR text, FILE* strm)
{
	fprintf(strm, "%s %10.4f %10.4f %10.4f\n", text, x, y, z);
}

BOOL CPoint8d::InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);
	CPoint3d pgr;
	GetGrPoz(view, &pgr);
	if (pgr.x >= X_min && pgr.x <= X_max && pgr.y >= Y_min && pgr.y <= Y_max)
		return 1;
	return 0;
}

BOOL CPoint8d::InRect(CPoint3d* p1, CPoint3d* p2)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);

	if (x >= X_min && x <= X_max && y >= Y_min && y <= Y_max)
		return 1;
	return 0;
}


void CPoint8d::Shift(CVector* vect, double dist, CPoint8d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
	p->l = l;
	p->m = m;
	p->n = n;
	p->s = s;
	p->t = t;
}



void CPoint8d::Shift(CVector* vect, double dist, CPoint3d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
}

void CPoint8d::Shift(double dist, CPoint3d* p)
{
	p->x = x + l * dist;
	p->y = y + m * dist;
	p->z = z + n * dist;
}

/*
CArchive& AFXAPI operator<<(CArchive& ar, const CPoint8d& p)
{
	ar << p.x << p.y << p.z << p.l << p.m << p.n << p.s << p.t;
	return ar;
}

CArchive& AFXAPI operator>>(CArchive& ar, CPoint8d& p)
{
	ar >> p.x >> p.y >> p.z >> p.l >> p.m >> p.n >> p.s >> p.t;
	return ar;
}
*/
void CPoint8d::Revers()
{
	l = -l;
	m = -m;
	n = -n;
}

double CPoint8d::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

int CPoint8d::In_P1_P2(CPoint3d* p1, CPoint3d* p2)
{
	double delta = 0.01;

	CVector v(p1, p2);
	CPlane pl((CPoint3d*)&x, &v);

	double de1 = p1->x * pl.a + p1->y * pl.b + p1->z * pl.c + pl.d;
	double de2 = p2->x * pl.a + p2->y * pl.b + p2->z * pl.c + pl.d;
	if (fabs(de1) < delta || fabs(de2) < delta)
		return 1;
	if ((de1 > delta && de2 < -delta) || (de1<-delta && de2>delta))
		return 1;
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//////////////////////////  CPoint8f ////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void CPoint8f::Move(CVector* dir, double dist)
{

	x += dir->l * dist;
	y += dir->m * dist;
	z += dir->n * dist;
}

void CPoint8f::Move(CPoint3d* p1, CPoint3d* p2)
{
	CVector vect;
	double dist = vect.calc(p1, p2);

	Move(&vect, dist);
}

void CPoint8f::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
	if (alfa == 0)
		return;
	if (dist_POINT(p0, p1) == 0)
		return;
	CVector vx(p0, p1);
	CSystemCoord system(p0, &vx);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm, cym, czm;
	cxm.l = cos(alfa);
	cxm.m = sin(alfa);
	cxm.n = 0;
	cym.l = -sin(alfa);
	cym.m = cos(alfa);
	cym.n = 0;
	czm.l = czm.m = 0;
	czm.n = 1;
	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}

void CPoint8f::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx = pf->x - Sx * pf->x;
	double Cy = pf->y - Sy * pf->y;
	double Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}

void CPoint8f::Mirror()
{
	z = -z;
	n = -n;
}

void CPoint8f::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = xa;
	y = ya;
	z = za;
	double la = l * cx->l + m * cy->l + n * cz->l;
	double ma = l * cx->m + m * cy->m + n * cz->m;
	double na = l * cx->n + m * cy->n + n * cz->n;
	l = la;
	m = ma;
	n = na;
}

void CPoint8f::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = xm;
	y = ym;
	z = zm;

	double lm = l * cx->l + m * cx->m + n * cx->n;
	double mm = l * cy->l + m * cy->m + n * cy->n;
	double nm = l * cz->l + m * cz->m + n * cz->n;
	l = lm;
	m = mm;
	n = nm;
}

void CPoint8f::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint8f::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint8f::Read(LPCTSTR str)
{
	if (str == NULL)
		return;
	char buf[BUFSIZE];
	strncpy(buf, str, BUFSIZE);
	String str1 = NULL;
	str1 = strtok(buf, " 	");
	if (str1 == NULL) {
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	x = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		y = 0;
		z = 0;
		return;
	}
	y = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		z = 0;
		return;
	}
	z = atof(str1);
}

void CPoint8f::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
	if (pv->m_ProjType != 0) {
		pv->Project((CPoint3d*)this, pgr);
		return;
	}

	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
}
void CPoint8f::GetAgrPoz(CView3d* pv, CPoint3d* pa)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pa->x = x;
	pa->y = y;
	pa->z = z;
	pa->mod_coord_am(&p0, &cx, &cy, &cz);
}

void CPoint8f::SetAgrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_am(&p0, &cx, &cy, &cz);
}
void CPoint8f::SetGrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_ma(&p0, &cx, &cy, &cz);
}

void CPoint8f::print(FILE* strm)
{
	fprintf(strm, " %10.4f %10.4f %10.4f\n", x, y, z);
}

void CPoint8f::print(LPCTSTR text, FILE* strm)
{
	fprintf(strm, "%s %10.4f %10.4f %10.4f\n", text, x, y, z);
}

BOOL CPoint8f::InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);
	CPoint3d pgr;
	GetGrPoz(view, &pgr);
	if (pgr.x >= X_min && pgr.x <= X_max && pgr.y >= Y_min && pgr.y <= Y_max)
		return 1;
	return 0;
}

BOOL CPoint8f::InRect(CPoint3d* p1, CPoint3d* p2)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);

	if (x >= X_min && x <= X_max && y >= Y_min && y <= Y_max)
		return 1;
	return 0;
}


void CPoint8f::Shift(CVector* vect, double dist, CPoint8d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
	p->l = l;
	p->m = m;
	p->n = n;
	p->s = s;
	p->t = t;
}



void CPoint8f::Shift(CVector* vect, double dist, CPoint3d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
}

void CPoint8f::Shift(double dist, CPoint3d* p)
{
	p->x = x + l * dist;
	p->y = y + m * dist;
	p->z = z + n * dist;
}

/*
CArchive& AFXAPI operator<<(CArchive& ar, const CPoint8f& p)
{
	ar << p.x << p.y << p.z << p.l << p.m << p.n << p.s << p.t;
	return ar;
}

CArchive& AFXAPI operator>>(CArchive& ar, CPoint8f& p)
{
	ar >> p.x >> p.y >> p.z >> p.l >> p.m >> p.n >> p.s >> p.t;
	return ar;
}
*/
void CPoint8f::Revers()
{
	l = -l;
	m = -m;
	n = -n;
}

double CPoint8f::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

int CPoint8f::In_P1_P2(CPoint3d* p1, CPoint3d* p2)
{
	double delta = 0.01;

	CVector v(p1, p2);
	CPlane pl((CPoint3d*)&x, &v);

	double de1 = p1->x * pl.a + p1->y * pl.b + p1->z * pl.c + pl.d;
	double de2 = p2->x * pl.a + p2->y * pl.b + p2->z * pl.c + pl.d;
	if (fabs(de1) < delta || fabs(de2) < delta)
		return 1;
	if ((de1 > delta && de2 < -delta) || (de1<-delta && de2>delta))
		return 1;
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//////////////////////////CPoint4d////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void CPoint4d::Move(CVector* dir, double dist)
{

	x += dir->l * dist;
	y += dir->m * dist;
	z += dir->n * dist;
}

void CPoint4d::Move(CPoint3d* p1, CPoint3d* p2)
{
	CVector vect;
	double dist = vect.calc(p1, p2);

	Move(&vect, dist);

}


void CPoint4d::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
	if (alfa == 0)
		return;
	if (dist_POINT(p0, p1) == 0)
		return;
	CVector vx(p0, p1);
	CSystemCoord system(p0, &vx);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm, cym, czm;
	cxm.l = cos(alfa);
	cxm.m = sin(alfa);
	cxm.n = 0;
	cym.l = -sin(alfa);
	cym.m = cos(alfa);
	cym.n = 0;
	czm.l = czm.m = 0;
	czm.n = 1;
	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}

void CPoint4d::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx;
	double Cy;
	double Cz;

	Cx = pf->x - Sx * pf->x;
	Cy = pf->y - Sy * pf->y;
	Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}

void CPoint4d::Mirror()
{
	z = -z;
}

void CPoint4d::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = xa;
	y = ya;
	z = za;
}

void CPoint4d::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = xm;
	y = ym;
	z = zm;
}

void CPoint4d::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint4d::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint4d::Read(LPCTSTR str)
{
	if (str == NULL)
		return;
	char buf[BUFSIZE];
	strncpy(buf, str, BUFSIZE);
	String str1 = NULL;
	str1 = strtok(buf, " 	");
	if (str1 == NULL) {
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	x = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		y = 0;
		z = 0;
		v = 0;
		return;
	}
	y = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		z = 0;
		v = 0;
		return;
	}
	z = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL)
		v = 0;
	else
		v = atof(str1);
}

void CPoint4d::print(FILE* strm)
{
	fprintf(strm, " %10.4f %10.4f %10.4f\n", x, y, z);
}

void CPoint4d::print(LPCTSTR text, FILE* strm)
{
	fprintf(strm, "%s %10.4f %10.4f %10.4f\n", text, x, y, z);
}

BOOL CPoint4d::InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);
	CPoint3d pgr;
	GetGrPoz(view, &pgr);
	if (pgr.x >= X_min && pgr.x <= X_max && pgr.y >= Y_min && pgr.y <= Y_max)
		return 1;
	return 0;
}

void CPoint4d::Shift(CVector* vect, double dist, CPoint3d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;

}

void CPoint4d::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
	if (pv->m_ProjType != 0) {
		pv->Project((CPoint3d*)this, pgr);
		return;
	}

	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
}

void CPoint4d::SetGrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_ma(&p0, &cx, &cy, &cz);
}

void CPoint4d::GetAgrPoz(CView3d* pv, CPoint3d* pa)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pa->x = x;
	pa->y = y;
	pa->z = z;
	pa->mod_coord_am(&p0, &cx, &cy, &cz);
}
void CPoint4d::SetAgrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_am(&p0, &cx, &cy, &cz);
}

BOOL CPoint4d::Offset(double offset, CPoint3d* p1, CPoint3d* p2)
{
	double a, b, c;

	if (XY_XY_ABC(p1->x, p1->y, p2->x, p2->y, &a, &b, &c))
		return BAD;

	x += a * offset;
	y += b * offset;
	return OK_AG;
}

double CPoint4d::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

///////////////////////////////////////////////////////////////////////////
//////////////////////////CPoint7d////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void CPoint7d::Move(CVector* dir, double dist)
{

	x += dir->l * dist;
	y += dir->m * dist;
	z += dir->n * dist;
}

void CPoint7d::Move(CPoint3d* p1, CPoint3d* p2)
{
	CVector vect;
	double dist = vect.calc(p1, p2);

	Move(&vect, dist);

}

void CPoint7d::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
	if (alfa == 0)
		return;
	if (dist_POINT(p0, p1) == 0)
		return;
	CVector vx(p0, p1);
	CSystemCoord system(p0, &vx);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm(cos(alfa), sin(alfa), 0);
	CVector cym(-sin(alfa), cos(alfa), 0);
	CVector czm(0, 0, 1);

	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}

void CPoint7d::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx;
	double Cy;
	double Cz;

	Cx = pf->x - Sx * pf->x;
	Cy = pf->y - Sy * pf->y;
	Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}

void CPoint7d::Mirror()
{
	z = -z;
	n = -n;
}

void CPoint7d::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = xa;
	y = ya;
	z = za;
	double la = l * cx->l + m * cy->l + n * cz->l;
	double ma = l * cx->m + m * cy->m + n * cz->m;
	double na = l * cx->n + m * cy->n + n * cz->n;
	l = la;
	m = ma;
	n = na;
}

void CPoint7d::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = xm;
	y = ym;
	z = zm;

	double lm = l * cx->l + m * cx->m + n * cx->n;
	double mm = l * cy->l + m * cy->m + n * cy->n;
	double nm = l * cz->l + m * cz->m + n * cz->n;
	l = lm;
	m = mm;
	n = nm;
}

void CPoint7d::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint7d::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint7d::Read(LPCTSTR str)
{
	if (str == NULL)
		return;
	char buf[BUFSIZE];
	strncpy(buf, str, BUFSIZE);
	String str1 = NULL;
	str1 = strtok(buf, " 	");
	if (str1 == NULL) {
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	x = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		y = 0;
		z = 0;
		return;
	}
	y = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		z = 0;
		return;
	}
	z = atof(str1);
}

void CPoint7d::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
	if (pv->m_ProjType != 0) {
		pv->Project((CPoint3d*)this, pgr);
		return;
	}

	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
}
void CPoint7d::GetAgrPoz(CView3d* pv, CPoint3d* pa)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pa->x = x;
	pa->y = y;
	pa->z = z;
	pa->mod_coord_am(&p0, &cx, &cy, &cz);
}

void CPoint7d::SetAgrPoz(CView3d* pv)
{
/*
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_am(&p0, &cx, &cy, &cz);
*/
}
void CPoint7d::SetGrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;
/*
	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_ma(&p0, &cx, &cy, &cz);
*/
}

void CPoint7d::print(FILE* strm)
{
	fprintf(strm, " %10.4f %10.4f %10.4f\n", x, y, z);
}

/*
void CPoint7d::print(CMemFile* file)
{
	if (file == NULL)
		return;
	char buf[180];
	char nl[3] = "";
	nl[0] = 13;
	nl[1] = 10;
	nl[2] = 0;
	sprintf(buf, " %10.4f %10.4f %10.4f%s", x, y, z, nl);
	file->Write(buf, strlen(buf));

}
*/

void CPoint7d::print(LPCTSTR text, FILE* strm)
{
	fprintf(strm, "%s %10.2f %10.2f %10.2f %6.5f  %6.5f  %6.5f %5.2f\n", text, x, y, z, l, m, n, s);
}

BOOL CPoint7d::InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);
	CPoint3d pgr;
	GetGrPoz(view, &pgr);
	if (pgr.x >= X_min && pgr.x <= X_max && pgr.y >= Y_min && pgr.y <= Y_max)
		return 1;
	return 0;
}

BOOL CPoint7d::InRect(CPoint3d* p1, CPoint3d* p2)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);

	if (x >= X_min && x <= X_max && y >= Y_min && y <= Y_max)
		return 1;
	return 0;
}

void CPoint7d::Shift(CVector* vect, double dist, CPoint7d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
	p->l = l;
	p->m = m;
	p->n = n;
	p->s = s;
}

void CPoint7d::Shift(CVector* vect, double dist, CPoint3d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
}
/*
CArchive& AFXAPI operator<<(CArchive& ar, const CPoint7d& p)
{
	ar << p.x << p.y << p.z << p.l << p.m << p.n << p.s;
	return ar;
}

CArchive& AFXAPI operator>>(CArchive& ar, CPoint7d& p)
{
	ar >> p.x >> p.y >> p.z >> p.l >> p.m >> p.n >> p.s;
	return ar;
}*/

void CPoint7d::Revers()
{
	l = -l;
	m = -m;
	n = -n;
}
double CPoint7d::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

double CPoint7d::GetDistLine(CPoint3d* p1, CPoint3d* p2)
{
	CVector line(p1, p2);
	long double	q = qxyz(line.l, line.m, line.n);
	if (q < DDELTA)
		return dist_POINT((CPoint3d*)this, p1);

	long double	q1 = line.m * (p1->z - z) - line.n * (p1->y - y);
	long double	q2 = line.n * (p1->x - x) - line.l * (p1->z - z);
	long double	q3 = line.l * (p1->y - y) - line.m * (p1->x - x);
	return qxyz(q1, q2, q3) / q;
}

double CPoint7d::GetDistLine(CPoint3d* p1, CVector* line)
{
	long double	q = qxyz(line->l, line->m, line->n);
	if (q < DDELTA)
		return dist_POINT((CPoint3d*)this, p1);

	long double	q1 = line->m * (p1->z - z) - line->n * (p1->y - y);
	long double	q2 = line->n * (p1->x - x) - line->l * (p1->z - z);
	long double	q3 = line->l * (p1->y - y) - line->m * (p1->x - x);
	return qxyz(q1, q2, q3) / q;
}


///////////////////////////////////////////////////////////////////////////
//////////////////////////CPoint14d////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void CPoint14d::Move(CVector* dir, double dist)
{

	x += dir->l * dist;
	y += dir->m * dist;
	z += dir->n * dist;
}

void CPoint14d::Move(CPoint3d* p1, CPoint3d* p2)
{
	CVector vect;
	double dist = vect.calc(p1, p2);

	Move(&vect, dist);

}

void CPoint14d::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
	if (alfa == 0)
		return;
	if (dist_POINT(p0, p1) == 0)
		return;
	CVector vx(p0, p1);
	CSystemCoord system(p0, &vx);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm, cym, czm;
	cxm.l = cos(alfa);
	cxm.m = sin(alfa);
	cxm.n = 0;
	cym.l = -sin(alfa);
	cym.m = cos(alfa);
	cym.n = 0;
	czm.l = czm.m = 0;
	czm.n = 1;
	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}

void CPoint14d::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx;
	double Cy;
	double Cz;

	Cx = pf->x - Sx * pf->x;
	Cy = pf->y - Sy * pf->y;
	Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}



void CPoint14d::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = xa;
	y = ya;
	z = za;
	double la = lu * cx->l + mu * cy->l + nu * cz->l;
	double ma = lu * cx->m + mu * cy->m + nu * cz->m;
	double na = lu * cx->n + mu * cy->n + nu * cz->n;
	lu = la;
	mu = ma;
	nu = na;

	la = lv * cx->l + mv * cy->l + nv * cz->l;
	ma = lv * cx->m + mv * cy->m + nv * cz->m;
	na = lv * cx->n + mv * cy->n + nv * cz->n;
	lv = la;
	mv = ma;
	nv = na;

	la = luv * cx->l + muv * cy->l + nuv * cz->l;
	ma = luv * cx->m + muv * cy->m + nuv * cz->m;
	na = luv * cx->n + muv * cy->n + nuv * cz->n;
	luv = la;
	muv = ma;
	nuv = na;

}

void CPoint14d::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{

	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = xm;
	y = ym;
	z = zm;

	double lm = lu * cx->l + mu * cx->m + nu * cx->n;
	double mm = lu * cy->l + mu * cy->m + nu * cy->n;
	double nm = lu * cz->l + mu * cz->m + nu * cz->n;
	lu = lm;
	mu = mm;
	nu = nm;

	lm = lv * cx->l + mv * cx->m + nv * cx->n;
	mm = lv * cy->l + mv * cy->m + nv * cy->n;
	nm = lv * cz->l + mv * cz->m + nv * cz->n;
	lv = lm;
	mv = mm;
	nv = nm;

	lm = luv * cx->l + muv * cx->m + nuv * cx->n;
	mm = luv * cy->l + muv * cy->m + nuv * cy->n;
	nm = luv * cz->l + muv * cz->m + nuv * cz->n;
	luv = lm;
	muv = mm;
	nuv = nm;
}

void CPoint14d::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint14d::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CPoint14d::Read(String str)
{
	if (str == NULL)
		return;
	String str1 = NULL;
	str1 = strtok(str, " 	");
	if (str1 == NULL) {
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	x = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		y = 0;
		z = 0;
		return;
	}
	y = atof(str1);
	str1 = strtok(NULL, " 	");
	if (str1 == NULL) {
		z = 0;
		return;
	}
	z = atof(str1);
}

void CPoint14d::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
	if (pv->m_ProjType != 0) {
		pv->Project((CPoint3d*)this, pgr);
		return;
	}

	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
}
void CPoint14d::GetAgrPoz(CView3d* pv, CPoint3d* pa)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pa->x = x;
	pa->y = y;
	pa->z = z;
	pa->mod_coord_am(&p0, &cx, &cy, &cz);
}

void CPoint14d::SetAgrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_am(&p0, &cx, &cy, &cz);
}
void CPoint14d::SetGrPoz(CView3d* pv)
{
	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	mod_coord_ma(&p0, &cx, &cy, &cz);
}

void CPoint14d::print(FILE* strm)
{
	fprintf(strm, " %10.4f %10.4f %10.4f\n", x, y, z);
}

void CPoint14d::print(LPCTSTR text, FILE* strm)
{
	fprintf(strm, "%s %10.4f %10.4f %10.4f\n", text, x, y, z);
}

BOOL CPoint14d::InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);
	CPoint3d pgr;
	GetGrPoz(view, &pgr);
	if (pgr.x >= X_min && pgr.x <= X_max && pgr.y >= Y_min && pgr.y <= Y_max)
		return 1;
	return 0;
}
BOOL CPoint14d::InRect(CPoint3d* p1, CPoint3d* p2)
{
	Coord X_min = MIN(p1->x, p2->x);
	Coord X_max = MAX(p1->x, p2->x);
	Coord Y_min = MIN(p1->y, p2->y);
	Coord Y_max = MAX(p1->y, p2->y);

	if (x >= X_min && x <= X_max && y >= Y_min && y <= Y_max)
		return 1;
	return 0;
}

void CPoint14d::Mirror()
{
	z = -z;
	nu = -nu;
	nv = -nv;
	nuv = -nuv;
}

void CPoint14d::Shift(CVector* vect, double dist, CPoint3d* p)
{
	p->x = x + vect->l * dist;
	p->y = y + vect->m * dist;
	p->z = z + vect->n * dist;
}

double CPoint14d::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}


//=================================

CVertex5f::CVertex5f(float xi, float yi, float zi)
{
	x = xi;
	y = yi;
	z = zi;
	s = t = 0;
	FaceList.clear();
}

CVertex5f::CVertex5f(const CPoint3d* p)
{
	x = float(p->x);
	y = float(p->y);
	z = float(p->z);
	s = t = 0;
}


CVertex5f::CVertex5f(const CVertex5f& v)
{
	x = v.x;								// X Component
	y = v.y;								// Y Component
	z = v.z;								// Z Component
	s = v.s;
	t = v.t;
	FaceList = v.FaceList;
}


CVertex5f& CVertex5f::operator =(const CVertex5f& v)
{
	if (&v == this)
		return *this;

	x = v.x;
	y = v.y;
	z = v.z;
	s = v.s;
	t = v.t;
	FaceList = v.FaceList;
	return *this;
}
bool CVertex5f::operator >(const CVertex5f& src)
{

	return true;
}

void CVertex5f::GetGrPoz(CView3d* pv, CPoint3d* pgr)
{
	if (pv->m_ProjType != 0) {
		CPoint3d p3d(x, y, z);
		pv->Project(&p3d, pgr);
		return;
	}

	CVector cx, cy, cz;
	CPoint3d p0;

	cx.l = pv->m_Objmat[0][0]; cx.m = pv->m_Objmat[0][1]; cx.n = pv->m_Objmat[0][2];
	cy.l = pv->m_Objmat[1][0]; cy.m = pv->m_Objmat[1][1]; cy.n = pv->m_Objmat[1][2];
	cz.l = pv->m_Objmat[2][0]; cz.m = pv->m_Objmat[2][1]; cz.n = pv->m_Objmat[2][2];
	pgr->x = x;
	pgr->y = y;
	pgr->z = z;
	pgr->mod_coord_ma(&p0, &cx, &cy, &cz);
}

void CVertex5f::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	double xa = cx->l * x + cy->l * y + cz->l * z + p0->x;
	double ya = cx->m * x + cy->m * y + cz->m * z + p0->y;
	double za = cx->n * x + cy->n * y + cz->n * z + p0->z;
	x = (float)xa;
	y = (float)ya;
	z = (float)za;
}

void CVertex5f::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	double xm = cx->l * (x - p0->x) + cx->m * (y - p0->y) + cx->n * (z - p0->z);
	double ym = cy->l * (x - p0->x) + cy->m * (y - p0->y) + cy->n * (z - p0->z);
	double zm = cz->l * (x - p0->x) + cz->m * (y - p0->y) + cz->n * (z - p0->z);
	x = (float)xm;
	y = (float)ym;
	z = (float)zm;
}

void CVertex5f::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CVertex5f::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CVertex5f::Move(CVector* dir, double dist)
{
	x += (float)(dir->l * dist);
	y += (float)(dir->m * dist);
	z += (float)(dir->n * dist);
}

void CVertex5f::Rotate(CPoint3d* p0, CVector* dir, double alfa)
{
	if (alfa == 0)
		return;

	CSystemCoord system(p0, dir);
	mod_coord_am(&system);

	CPoint3d p0m;
	CVector cxm(cos(alfa), sin(alfa), 0);
	CVector cym(-sin(alfa), cos(alfa), 0);
	CVector czm(0, 0, 1);

	mod_coord_am(&p0m, &cxm, &cym, &czm);
	mod_coord_ma(&system);
}

void CVertex5f::Zoom(CPoint3d* pf, double Sx, double Sy, double Sz)
{
	double Cx = pf->x - Sx * pf->x;
	double Cy = pf->y - Sy * pf->y;
	double Cz = pf->z - Sz * pf->z;

	x = Sx * x + Cx;
	y = Sy * y + Cy;
	z = Sz * z + Cz;
}

double CVertex5f::GetDistLineSegment(CPoint3d* p1, CPoint3d* p2)
{
	if (!In_P1_P2(p1, p2)) {
		double dist1 = DistTo(p1);
		double dist2 = DistTo(p2);
		if (dist1 < dist2)
			return dist1;
		return dist2;
	}

	return GetDistLine(p1, p2);
}

double CVertex5f::DistTo(CPoint3d* p2)
{
	if (this == NULL)
		return 0;
	if (p2 == NULL)
		return 0;
	double ax = p2->x - x;
	double ay = p2->y - y;
	double az = p2->z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

double CVertex5f::DistTo(CVertex5f& p2)
{
	if (this == NULL)
		return 0;
	double ax = p2.x - x;
	double ay = p2.y - y;
	double az = p2.z - z;
	return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

int CVertex5f::In_P1_P2(CPoint3d* p1, CPoint3d* p2)
{
	double delta = 0.001;
	CVector v(p1, p2);
	CPoint3d p0(x, y, z);
	CPlane pl(&p0, &v);

	double de1 = p1->x * pl.a + p1->y * pl.b + p1->z * pl.c + pl.d;
	double de2 = p2->x * pl.a + p2->y * pl.b + p2->z * pl.c + pl.d;
	if (fabs(de1) < delta || fabs(de2) < delta)
		return 1;
	if ((de1 > delta && de2 < -delta) || (de1<-delta && de2>delta))
		return 1;
	return 0;
}
double CVertex5f::GetDistLine(CPoint3d* p1, CPoint3d* p2)
{
	CVector line(p1, p2);
	long double	q = qxyz(line.l, line.m, line.n);
	if (q < DDELTA)
		return DistTo(p1);

	long double	q1 = line.m * (p1->z - z) - line.n * (p1->y - y);
	long double	q2 = line.n * (p1->x - x) - line.l * (p1->z - z);
	long double	q3 = line.l * (p1->y - y) - line.m * (p1->x - x);
	return (double)qxyz(q1, q2, q3) / q;
}

void CVertex5f::CalcST(CVertex5f& p1, CVertex5f& p2)
{
	double dp1p2 = p1.DistTo(p2);
	if (dp1p2 < DELTA) {
		s = p1.s;
		t = p1.t;
		return;
	}

	double dp1p = DistTo(p1);

	double Ds = p2.s - p1.s;
	double ds = Ds * dp1p / dp1p2;
	s = p1.s + ds;

	double Dt = p2.t - p1.t;
	double dt = Dt * dp1p / dp1p2;
	t = p1.t + dt;

}
