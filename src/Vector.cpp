// Vector.cpp: implementation of the Vector class.
//
//	This is a part of the CAD/CAM/CAE "Alpha".
//	Copyright (C) 1994-2000 A.Gorbatovsky 
//	All rights reserved.
//	Ukraine, Kiev
//////////////////////////////////////////////////////////////////////


#include "ageom.h"
#include "Point3d.h"
#include "Vector.h"
#include "SystemCoord.h"
#include "Plane.h"

//#include "service.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CVector::CVector(CPoint3d* p1, CPoint3d* p2)
{
    long double ax=p2->x-p1->x;	
    long double ay=p2->y-p1->y;
    long double az=p2->z-p1->z;
    long double q=qxyz(ax,ay,az);
    if(q < DDELTA){
		l=0;
		m=0;
		n=0;
		return ;
		}
    l=(double)ax/q;
    m=(double)ay/q;
    n=(double)az/q;

}

CVector::CVector(CVector* v1, CVector* v2)
{
	MultVectors(v1, v2);
}

CVector::CVector(CPoint3d& p1, CPoint3d& p2)
{
	long double ax = p2.x - p1.x;
	long double ay = p2.y - p1.y;
	long double az = p2.z - p1.z;
	long double q = qxyz(ax, ay, az);
	if (q < DDELTA) {
		l = 0;
		m = 0;
		n = 0;
		return;
	}
	l = (double)ax / q;
	m = (double)ay / q;
	n = (double)az / q;

}


CVector::CVector(CVector* v1, CVector* v2, CVector* v3, CVector* v4)
{
	l=(v1->l+v2->l+v3->l+v4->l)/4.0;
	m=(v1->m+v2->m+v3->m+v4->m)/4.0;
	n=(v1->n+v2->n+v3->n+v4->n)/4.0;
}

CVector::CVector(double axy,double az)
{
	l=cos(az)*cos(axy);
	m=cos(az)*sin(axy);
	n=sin(az);
	Normalize();
}

void CVector::Rotate(CPoint3d* p0,CPoint3d* p1,double alfa)
{
	if(alfa==0)
		return;
	if(dist_POINT(p0, p1)==0)
		return;
CVector vx(p0, p1);
CSystemCoord system(p0, &vx);
    mod_coord_am(&system);

CPoint3d p0m;
CVector cxm,cym,czm;
    cxm.l=cos(alfa);
    cxm.m=sin(alfa);
    cxm.n=0;
    cym.l=-sin(alfa);
    cym.m=cos(alfa);
    cym.n=0;
    czm.l=czm.m=0;
    czm.n=1;
    mod_coord_am(&cxm,&cym,&czm);
    mod_coord_ma(&system);
}

void CVector::mod_coord_ma(CVector* cx, CVector* cy, CVector* cz)
{
   double lt=l*cx->l+m*cy->l+n*cz->l;
   double mt=l*cx->m+m*cy->m+n*cz->m;
   double nt=l*cx->n+m*cy->n+n*cz->n;
    l=lt;
    m=mt;
    n=nt;
	Normalize();
}

void CVector::mod_coord_am(CVector* cx, CVector* cy, CVector* cz)
{
   double lt=l*cx->l+m*cx->m+n*cx->n;
   double mt=l*cy->l+m*cy->m+n*cy->n;
   double nt=l*cz->l+m*cz->m+n*cz->n;
	l=lt;
    m=mt;
    n=nt;
	Normalize();	
}
void CVector::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->cx, &sc->cy, &sc->cz);
}

void CVector::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->cx, &sc->cy, &sc->cz);
}

void CVector::Mirror()
{
	CPoint3d p1;
	CPoint3d p2;
	p2.Move(this, 100);
	p2.Mirror();
	calc(&p1, &p2);
}

double CVector::calc(CPoint3d* p1, CPoint3d* p2)
{
	if(!p1 || !p2){
		l=0;
		m=0;
		n=0;
		return 0;
	}
	double ax=p2->x-p1->x;	
	double ay=p2->y-p1->y;
	double az=p2->z-p1->z;
	long double  q=qxyz(ax,ay,az);
    if(q < DDELTA){
		l=0;
		m=0;
		n=0;
		return 0;
		}
    l=ax/q;
    m=ay/q;
    n=az/q;
    return q;
}
////////////////////////////////////////////////////////////////
/*
CArchive& AFXAPI operator<<(CArchive& ar, const CVector& v)
{	ar<<v.l<<v.m<<v.n;
	return ar;
}
CArchive& AFXAPI operator>>(CArchive& ar, CVector& v)
{	ar>>v.l>>v.m>>v.n;
	return ar;
}
*/
BOOL CVector::Mult(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3, CPoint3d* p4)
{
CVector v1;
	if(v1.calc(p1, p2)==0)
		return BAD;
CVector v2;
	if(v2.calc(p3, p4)==0)
		return BAD;

	return MultVectors(&v1, &v2);
}


BOOL CVector::MultVectors(CVector* v1, CVector* v2)
{
CVector vx=*v1;
CVector vy=*v2;

    l=vx.m*vy.n-vx.n*vy.m;
    m=vx.n*vy.l-vx.l*vy.n;
    n=vx.l*vy.m-vx.m*vy.l;

    return Normalize();
}

void CVector::GetOrth(CVector *v)
{
	if(!v)
		return;
	CVector vz(0,0, 1);   
     if(fabs(n)>0.95){
		vz.l=1;
		vz.m=vz.n=0;
	}  
	 v->MultVectors(this, &vz);
}


BOOL CVector::Normalize()
{
	long double q=qxyz(l, m, n);
    if(q < DDELTA){
		l=m=n=0;
//#ifdef _DEBUG
//		MessageBox(AfxGetApp()->m_pMainWnd->GetSafeHwnd(), "CVector::Normalize()\nl=m=n=0;", "The Error!", MB_OK|MB_ICONERROR);
//#endif
		return BAD;
		}
    l=l/q;
    m=m/q;
    n=n/q;
    return OK_AG;
}

void CVector::Read(char* str)
{
	if(str==NULL)
		return;
	char* str1=NULL;
	str1=strtok(str," 	");
	if(str1==NULL){
		l=0;
		m=0;	
		n=0;	
		return;
		}
	l=atof(str1);
	str1=strtok(NULL," 	");
	if(str1==NULL){
		m=0;	
		n=0;	
		return;
		}
	m=atof(str1);
	str1=strtok(NULL," 	");
	if(str1==NULL){
		n=0;	
		return;
		}
	n=atof(str1);
}

BOOL CVector::IsCongruent(CVector* v2)
{
	CPoint3d p0;
	CPoint3d p1;
	CPoint3d p2;
	CPoint3d p3;

    p0.Shift(this, 100,&p1);
    p0.Shift(v2, 100,&p2);
    p0.Shift(v2, -100,&p3);

    return (dist_POINT(&p1, &p2) < dist_POINT(&p1,&p3));
}

void CVector::Revers()
{
    l=-l;
    m=-m;
    n=-n;
}


BOOL CVector::IsCollinear(CVector* v2, double delta)
{
CVector v_tmp=*v2;

    if(!IsCongruent(v2)){
		v_tmp.l=-v_tmp.l;
		v_tmp.m=-v_tmp.m;
		v_tmp.n=-v_tmp.n;
	}
    
	double cos_alfa=l*v_tmp.l+m*v_tmp.m+n*v_tmp.n;
    if(fabs(cos_alfa)>1)
		cos_alfa=1.0;
	double alfa=acos(cos_alfa);
    return (alfa<delta);
}

double CVector::GetAngle(CVector* vect2)
{
	if(fabs(l)<DELTA && fabs(m)<DELTA &&fabs(n)<DELTA)
		return 0;
	double cos_alfa=l*vect2->l+m*vect2->m+n*vect2->n;
    if(fabs(cos_alfa)>1)
		cos_alfa=1.0;
	return acos(cos_alfa);
}

double CVector::GetAngle(CPlane* pl)
{
	long double q1=qxyz(pl->a,pl->b,pl->c);
	long double q2=qxyz(l,m,n);
	return asin(fabs(pl->a*l+pl->b*m+pl->c*n)/(q1*q2));
}



//short get_rotate_Vector_angle(LMN *v1,LMN *v2,LMN *vect,double *beta)
double CVector::GetAngleRotateVector(CVector* v1,CVector* v2)
{
/////Функция считает this вектор и угол поворота для совмещения v1 и v2
	CPoint3d p0;
	CPoint3d p1;
	CPoint3d p2;
    p1.Move(v1,100);
    p2.Move(v2,100);


	CSystemCoord sc;
	if(sc.Set(&p0, &p1, &p2)){
		if(v1->IsCollinear(v2, 0.0001))
			if(v1->IsCongruent(v2))
				return 0;
		CVector vect_orth;
		v1->GetOrth(&vect_orth);
		CPoint3d p3;
		p3.Move(&vect_orth,100);
		sc.Set(&p0, &p1, &p3);
	}

    p1.mod_coord_am(&sc);
    p2.mod_coord_am(&sc);

    CVector v1t(&p0, &p1);
    CVector v2t(&p0, &p2);
	double beta=v1t.GetAngle(&v2t);

    int lef=left_or_right(p0.x,p0.y,p1.x,p1.y,p2.x,p2.y);
    if(!lef)
		beta=-beta;
	CPoint3d pz(0,0,100);
    pz.mod_coord_ma(&sc);
 
	calc(&p0, &pz);
    return beta;
}

//void get_2angle_rotate_Vector(LMN *v1,LMN *v2,double *alfa,LMN *cy,double *beta)
void CVector::GetAnglesRotateVector(CVector* v1,CVector* v2, double* alfa, double* beta)
{
///Функция считает this вектор и 2 угла поворота для совмещения v1 и v2
//	Причем первый поворот делаем вокруг оси Cz на Угол alfa
//	Второй поворот делаем вокруг оси This Вектора на Угол beta
	CPoint3d p0;
	CPoint3d p1;
	CPoint3d p2;
    p1.Move(v1,100);
    p2.Move(v2,100);
	CPoint3d p1t=p1;
	CPoint3d p2t=p2;
    p1t.z=p2t.z=0;

	CPoint3d px(100,0,0);
	CPoint3d py(0,100,0);
	CPoint3d pz(0,0,100);
	CVector v1t(&p0, &p1t);
	CVector v2t(&p0, &p2t);

    *alfa=acos(v2t.l*v1t.l+v2t.m*v1t.m);
    short lef=left_or_right(p0.x,p0.y,p1.x,p1.y,p2.x,p2.y);
    if(!lef)
		*alfa=-*alfa;
    p1.Rotate(&p0,&pz,*alfa);
    p1t.x=p1.x;
    p1t.y=p1.y;
	p1t.Rotate(&p0,&pz,PI/2.0);

    calc(&p0, &p1t);

	double alfa_x=acos(v2t.l);
    lef=left_or_right(p0.x,p0.y,px.x,px.y,p2.x,p2.y);
    if(!lef)
		alfa_x=-alfa_x;

    px.Rotate(&p0, &pz, alfa_x);

	CSystemCoord sc;
	if(sc.Set(&p0, &px, &pz)){
		*beta=*alfa=0;
		return ;
	}
    p1.mod_coord_am(&sc);
    p2.mod_coord_am(&sc);
    v1t.calc(&p0, &p1);
    v2t.calc(&p0, &p2);
    *beta=acos(v2t.l*v1t.l+v2t.m*v1t.m);
    lef=left_or_right(p0.x,p0.y,p1.x,p1.y,p2.x,p2.y);
    if(lef)
		*beta=-*beta;
}

//short left_right_Vector(double cx[],double v2[],double v[])
int CVector::GetDir(CVector* cx, CVector* v2)
{

	CVector cz(cx, v2);
	CVector cy(&cz, cx);

	CPoint3d p0;
	CPoint3d pm;
	pm.Move(this, 100);
  
    pm.mod_coord_am(&p0, cx, &cy, &cz);
    if(pm.z>0)
		return RIGHT_DIR;
    if(pm.z<0)
		return LEFT_DIR;
    return MIDDLE_DIR;
}

void CVector::LMN_lm(double*li, double*mi)
{

    double q=qxy(l,m);
    if(q < DDELTA){
		*li=0;
		*mi=0;
		return ;
	}
    *li=l/q;
    *mi=m/q;
    return ;
}

int CVector::GetTwoAngleRotate(CVector* v2, double* alfa, double* beta)
{///Функция считает  2 угла поворота для с вектором v2
//	Причем первый поворот делаем вокруг оси Cx на Угол alfa
//	Второй поворот делаем вокруг оси Cz на Угол beta
	CVector vx(1,0,0);
	if(vx.IsCollinear(v2, DELTA))
		if(vx.IsCongruent(v2))
			return GetTwoAngleRotateVx(v2,alfa, beta);

////////////////Вначале  для плоскости YZ
	CPoint3d p1;
	p1.Move(this, 100);
	p1.x=0;
	CPoint3d p2;
	p2.Move(v2, 100);
	p2.x=0;
/////////////Определим угол для совмещения 2-х векторов В плоскости YZ
	CPoint3d p0;
	CVector cy(0,1,0);
	CVector c1(&p0, &p1);//1-й вектор в плоскости YZ
	CVector c2(&p0, &p2);//2-й вектор в плоскости YZ
	/////////Теперь проанализируем какой знак
	double a1=c1.GetAngle(&cy);
	if(c1.n<0)
		a1=-a1;
	double a2=c2.GetAngle(&cy);
	if(c2.n<0)
		a2=-a2;
	*alfa=a2-a1;
	
///////////////////////////Теперь повторим для плоскости XY
	CVector V1_rot=*this;
	CPoint3d px(100,0,0);
	V1_rot.Rotate(&p0, &px, *alfa);

	if(V1_rot.IsCollinear(v2, DELTA))
		if(V1_rot.IsCongruent(v2)){
			*beta=0;
			return 1;
		}
	p1=p0;
	p1.Move(&V1_rot, 100);
	p1.z=0;
	p2=p0;
	p2.Move(v2, 100);
	p2.z=0;

/////////////Определим угол для совмещения 2-х векторов для плоскости XY
	CVector cx(1,0,0);
	CVector c1z(&p0, &p1);//1-й вектор в плоскости XY
	CVector c2z(&p0, &p2);//2-й вектор в плоскости XY
	/////////Теперь проанализируем какой знак
	a1=c1z.GetAngle(&cx);
	if(c1z.m<0)
		a1=-a1;
	a2=c2z.GetAngle(&cx);
	if(c2z.m<0)
		a2=-a2;
	*beta=a2-a1;
	return 1;
}

int CVector::GetTwoAngleRotateVx(CVector* v2, double* alfa, double* beta)
{
//	Причем первый поворот делаем вокруг оси Cz на Угол alfa
//	Второй поворот делаем вокруг оси Cy на Угол beta
	CVector vz(0,0,1);
	if(vz.IsCollinear(v2, DELTA))
//		if(vz.IsCongruent(v2))
			return GetTwoAngleRotateVz(v2,alfa, beta);

////////////////Вначале  для плоскости XY
	CPoint3d p1;
	p1.Move(this, 100);
	p1.z=0;
	CPoint3d p2;
	p2.Move(v2, 100);
	p2.z=0;
/////////////Определим угол для совмещения 2-х векторов В плоскости YZ
	CPoint3d p0;
	CVector cy(1,0,0);
	CVector c1(&p0, &p1);//1-й вектор в плоскости XY
	CVector c2(&p0, &p2);//2-й вектор в плоскости XY
	/////////Теперь проанализируем какой знак
	double a1=c1.GetAngle(&cy);
	if(c1.m<0)
		a1=-a1;
	double a2=c2.GetAngle(&cy);
	if(c2.m<0)
		a2=-a2;
	*alfa=a2-a1;
	if(IsCollinear(&vz, DELTA))
//		if(IsCongruent(&vz))
			*alfa=PI/2.0;

	
///////////////////////////Теперь повторим для плоскости ZX

	CVector V1_rot=*this;
	CPoint3d pz(0,0,120);
	V1_rot.Rotate(&p0, &pz, *alfa);

	if(V1_rot.IsCollinear(v2, DELTA))
		if(V1_rot.IsCongruent(v2)){
			*beta=0;
			return 2;
		}
	p1=p0;
	p1.Move(&V1_rot, 100);
	p1.y=0;
	p2=p0;
	p2.Move(v2, 100);
	p2.y=0;

/////////////Определим угол для совмещения 2-х векторов для плоскости XY
	CVector cx(0,0,1);
	CVector c1z(&p0, &p1);//1-й вектор в плоскости ZX
	CVector c2z(&p0, &p2);//2-й вектор в плоскости ZX
	/////////Теперь проанализируем какой знак
	a1=c1z.GetAngle(&cx);
	if(c1z.l<0)
		a1=-a1;
	a2=c2z.GetAngle(&cx);
	if(c2z.l<0)
		a2=-a2;
	*beta=a2-a1;
	
	
	return 2;
}

int CVector::GetTwoAngleRotateVz(CVector* v2, double* alfa, double* beta)
{
	return 3;
}

void CVector::print(void)
{
	FILE* strm = NULL;
	fopen_s(&strm, "c:\\temp\\stdout.txt", "a+");
    if(strm==NULL)
		return;
    fprintf(strm," %10.6lf %10.6lf %10.6lf\n",l,m,n);
	fclose(strm);
}

void CVector::print(LPCTSTR text)
{
	FILE* strm = NULL;
	fopen_s(&strm, "c:\\temp\\stdout.txt", "a+");
    if(strm==NULL)
		return;
    fprintf(strm," %s  %10.6lf %10.6lf %10.6lf\n", text, l, m, n);
	fclose(strm);
}

void CVector::GetMidle(const CVector& v1, const CVector& v2)
{
	CVector dir1 = v1;
	CVector dir2 = v2;
	CPoint3d p0;
	CPoint3d p2;
	double dist = 10;
	p2.Move(&dir1, dist);
	p2.Move(&dir2, dist);
	CVector dirm(&p0, &p2);
	l = dirm.l;
	m = dirm.m;
	n = dirm.n;

}

double	CVector::DotP(const CPoint3d& p2)
{
	return (l * p2.x + m * p2.y + n * p2.z);
}


