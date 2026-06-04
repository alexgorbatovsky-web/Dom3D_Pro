////////////////////Plane.c++
////
#include "Plane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


int  ABCD(double p1[],double p2[],double p3[],double *a,double *b,double *c,double *d)
{

    *a=p1[1]*(p2[2]-p3[2])-p1[2]*(p2[1]-p3[1])+p2[1]*p3[2]-p3[1]*p2[2];
    *b=p1[2]*(p2[0]-p3[0])-p1[0]*(p2[2]-p3[2])+p2[2]*p3[0]-p3[2]*p2[0];
    *c=p1[0]*(p2[1]-p3[1])-p1[1]*(p2[0]-p3[0])+p2[0]*p3[1]-p3[0]*p2[1];
    *d=p1[0]*(p2[1]*p3[2]-p3[1]*p2[2])-p1[1]*(p2[0]*p3[2]-p3[0]*p2[2])\
		+p1[2]*(p2[0]*p3[1]-p3[0]*p2[1]);
	long double    e=qxyz(*a,*b,*c);
    if(e < DDELTA)
		return BAD;

    *a=*a/e;
    *b=*b/e;
    *c=*c/e;
    *d=-*d/e;
    return OK_AG;
}

int crossing_3plat(CPlane *pl1,CPlane *pl2,CPlane *pl3,double p[])
{
double delta,deltax,deltay,deltaz;


     delta=pl1->a*(pl2->b*pl3->c-pl2->c*pl3->b)-pl2->a*(pl1->b*pl3->c-pl1->c*pl3->b)+pl3->a*(pl1->b*pl2->c-pl1->c*pl2->b);
    if(fabs(delta)<DDELTA)
		return BAD;
    deltax=-pl1->d*(pl2->b*pl3->c-pl2->c*pl3->b)+pl2->d*(pl1->b*pl3->c-pl1->c*pl3->b)-pl3->d*(pl1->b*pl2->c-pl1->c*pl2->b);
    deltay=pl1->a*(pl2->c*pl3->d-pl2->d*pl3->c)-pl2->a*(pl1->c*pl3->d-pl1->d*pl3->c)+pl3->a*(pl1->c*pl2->d-pl1->d*pl2->c);
    deltaz=pl1->a*(pl2->d*pl3->b-pl2->b*pl3->d)-pl2->a*(pl1->d*pl3->b-pl1->b*pl3->d)+pl3->a*(pl1->d*pl2->b-pl1->b*pl2->d);
    p[0]=deltax/delta;
    p[1]=deltay/delta;
    p[2]=deltaz/delta;
    return OK_AG;
}

//////////////////////	 CPlane	//////////////////

CPlane::CPlane(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3)
{
    a=p1->y*(p2->z-p3->z)-p1->z*(p2->y-p3->y)+p2->y*p3->z-p3->y*p2->z;
    b=p1->z*(p2->x-p3->x)-p1->x*(p2->z-p3->z)+p2->z*p3->x-p3->z*p2->x;
    c=p1->x*(p2->y-p3->y)-p1->y*(p2->x-p3->x)+p2->x*p3->y-p3->x*p2->y;
    d=p1->x*(p2->y*p3->z-p3->y*p2->z)-p1->y*(p2->x*p3->z-p3->x*p2->z)\
		+p1->z*(p2->x*p3->y-p3->x*p2->y);
	long double e=qxyz(a, b, c);
    if(e < DDELTA){
		a=b=c=d=0;
		return ;
	}
    a=a/e;
    b=b/e;
    c=c/e;
    d=-d/e;
}

CPlane::CPlane(CPoint3d* p1, CVector* vect,CPlane *pl)
{//Плоскоть проходящая через Прямую перп. Плоск.
	CPoint3d p2=*p1;
	p2.Move(vect,100);
	CPoint3d p3=*p1;
	p3.Move((CVector*)&pl->a,100);	
	if(!ABCD(&p1->x, &p2.x, &p3.x, &a, &b, &c, &d))
		return;
///Ошибка Прямая перпенд плоскости
//Найдем вектор перпенд зад. вектору
	CVector vect_orth;
	vect->GetOrth(&vect_orth);
	p3.Move(&vect_orth,100);	
	ABCD(&p1->x, &p2.x, &p3.x, &a, &b, &c, &d);
	
}

CPlane::CPlane(CPoint3d* p, CPlane *pl)
{//Плоскоть проходящая через Точку парал. Плоск.

	a=pl->a;
	b=pl->b;
	c=pl->c;
	d=-pl->a*p->x-pl->b*p->y-pl->c*p->z;
}

void CPlane::Get_3Point(CPoint3d* p1,CPoint3d* p2,CPoint3d* p3)
{
CPoint3d p0;
CVector cx(1,0,0);
CVector cy(0,1,0);
CVector cz(0,0,1);


    if( fabs(a)<DELTA || fabs(b)<DELTA || fabs(c)<DELTA || fabs(d)<DELTA ){
		if(fabs(a)>0.1){
			cross_Line(&p0, &cx , p1);
			p0.z=1000;
			cross_Line(&p0, &cx, p2);
			p0.z=0;p0.y=1000;
			cross_Line(&p0, &cx, p3);
			return;
			}
		if(fabs(b)>0.1){
			cross_Line(&p0, &cy,p1);
			p0.x=1000;
			cross_Line(&p0, &cy, p2);
			p0.x=0;p0.z=1000;
			cross_Line(&p0, &cy, p3);
			return;
			}
		cross_Line(&p0, &cz, p1);
		p0.x=1000;
		cross_Line(&p0, &cz, p2);
		p0.x=0;p0.y=1000;
		cross_Line(&p0, &cz, p3);
		return;
		}
	cross_Line(&p0, &cx, p1);
    cross_Line(&p0, &cy, p2);
    cross_Line(&p0, &cz, p3);
}

void CPlane::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
CPoint3d p1,p2,p3;

    if((fabs(a)<DELTA && fabs(b)<DELTA && fabs(c)<DELTA))
		return;
    Get_3Point(&p1 ,&p2 ,&p3);
    p1.mod_coord_ma(p0,cx,cy,cz);
    p2.mod_coord_ma(p0,cx,cy,cz);
    p3.mod_coord_ma(p0,cx,cy,cz);
    CPlane pl(&p1 ,&p2 ,&p3 );
	a=pl.a;
	b=pl.b;
	c=pl.c;
	d=pl.d;
}

void CPlane::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
CPoint3d p1,p2,p3;

    if((fabs(a)<DELTA && fabs(b)<DELTA && fabs(c)<DELTA))
		return;
    Get_3Point(&p1 ,&p2 ,&p3);
    p1.mod_coord_am(p0,cx,cy,cz);
    p2.mod_coord_am(p0,cx,cy,cz);
    p3.mod_coord_am(p0,cx,cy,cz);
    CPlane pl(&p1 ,&p2 ,&p3 );
	a=pl.a;
	b=pl.b;
	c=pl.c;
	d=pl.d;
}

void CPlane::print(FILE* fil)
{
	int file_null=0;
	if(!fil){
		fopen_s(&fil, "c:\\stdout.txt","a+");
		file_null=1;
	}
    if(fil==NULL)
		return ;
    fprintf(fil," %10.8f %10.8f %10.8f %10.4f\n",a, b, c, d);
	if(file_null)
		fclose(fil);
}
void CPlane::print(const char* text, FILE* fil)
{
	int file_null=0;
	if(!fil){
		fopen_s(&fil, "c:\\stdout.txt","a+");
		file_null=1;
	}
    if(fil==NULL)
		return ;

    fprintf(fil,"%s %10.8f %10.8f %10.8f %10.4f\n",text, a, b, c, d);

	if(file_null)
		fclose(fil);
}

double CPlane::dist_Point(CPoint3d* p)
{
    return fabs(p->x*a+p->y*b+p->z*c+d);
}

BOOL CPlane::cross_Line(CPoint3d* p, CVector* vect, CPoint3d* pc)
{
    if(fabs(a)<DELTA && fabs(b)<DELTA && fabs(c)<DELTA)	// неправильная плоскость
		return BAD;
    if(fabs(vect->l)<DELTA && fabs(vect->m)<DELTA && fabs(vect->n)<DELTA)	// неправильная прямая
		return BAD;

	long double line=a*vect->l+b*vect->m+c*vect->n;
    if(fabs(line) < DDELTA)	//	прямая паралельна плоскости
		return BAD;

	long double t=-(a*p->x+b*p->y+c*p->z+d)/line;
    pc->x=t*vect->l+p->x;
    pc->y=t*vect->m+p->y;
    pc->z=t*vect->n+p->z;
	return OK_AG;
}

BOOL CPlane::CrossNormal(CPoint3d* p, CPoint3d* pc)
{
	long double line=a*a+b*b+c*c;
    if(fabs(line) < DDELTA)	// неправильная плоскость
		return BAD;

	long double t=-(a*p->x+b*p->y+c*p->z+d)/line;
    pc->x=t*a+p->x;
    pc->y=t*b+p->y;
    pc->z=t*c+p->z;
	return OK_AG;
}



BOOL CPlane::cross_Line(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc)
{
CVector vect(p1, p2);

	return cross_Line(p1, &vect, pc);

}

CPlane::CPlane(CPoint3d* p, CVector* vect)
{//Плоскоть проходящая через точку перп. вектору

	d=-vect->l*p->x-vect->m*p->y-vect->n*p->z;
	a=vect->l;
	b=vect->m;
	c=vect->n;
}
/*
CArchive& AFXAPI operator<<(CArchive& ar, const CPlane& pl)
{
	ar<<pl.a<<pl.b<<pl.c<<pl.d;
	return ar;
}
CArchive& AFXAPI operator>>(CArchive& ar, CPlane& pl)
{
	ar>>pl.a>>pl.b>>pl.c>>pl.d;
	return ar;
}
*/
void CPlane::Read(char* str)
{
	char* str1=NULL;
	str1=strtok(str," 	");
	if(str1==NULL){
		a=1;
		b=0;	
		c=0;	
		d=0;	
		return;
		}
	a=atof(str1);
	str1=strtok(NULL," 	");
	if(str1==NULL){
		b=0;	
		c=0;	
		d=0;	
		return;
		}
	b=atof(str1);
	str1=strtok(NULL," 	");
	if(str1==NULL){
		c=0;	
		d=0;	
		return;
		}
	c=atof(str1);
	str1=strtok(NULL," 	");
	if(str1==NULL){
		d=0;	
		return;
		}
	d=atof(str1);
}

double CPlane::GetAngle(CVector* v)
{
	long double q1=qxyz(a,b,c);
	long double q2=qxyz(v->l,v->m,v->n);
	return asin(fabs(a*v->l+b*v->m+c*v->n)/(q1*q2));
}

double CPlane::GetAngle(CPlane* pl2)
{
	long double q1=qxyz(a,b,c);
	long double q2=qxyz(pl2->a,pl2->b,pl2->c);

    double alfa=(a*pl2->a+b*pl2->b+c*pl2->c)/(q1*q2);
    return acos(alfa);
}

void CPlane::Move(CVector* vect, double dist)
{

	CPoint3d p[3];
	Get_3Point(&p[0], &p[1], &p[2]);
	for(int i=0;i<3; i++)
		p[i].Move( vect, dist);
	CPlane plm(&p[0], &p[1], &p[2]);
	a=plm.a;
	b=plm.b;
	c=plm.c;
	d=plm.d;
}

void CPlane::Rotate(CPoint3d* p0, CVector* vect,double alfa)
{
	CPoint3d p[3];
	Get_3Point(&p[0], &p[1], &p[2]);
	for(int i=0;i<3; i++)
		p[i].Rotate(p0, vect, alfa);
	CPlane plm(&p[0], &p[1], &p[2]);
	a=plm.a;
	b=plm.b;
	c=plm.c;
	d=plm.d;

}

BOOL CPlane::Crossing3Planes(CPlane *pl2, CPlane *pl3, CPoint3d* pc)
{
    double delta=a*(pl2->b*pl3->c-pl2->c*pl3->b)-pl2->a*(b*pl3->c-c*pl3->b)+pl3->a*(b*pl2->c-c*pl2->b);
    if(fabs(delta)<DDELTA)
		return BAD;
    double deltax = -d*(pl2->b*pl3->c-pl2->c*pl3->b)+pl2->d*(b*pl3->c-c*pl3->b)-pl3->d*(b*pl2->c-c*pl2->b);
    double deltay = a*(pl2->c*pl3->d-pl2->d*pl3->c)-pl2->a*(c*pl3->d-d*pl3->c)+pl3->a*(c*pl2->d-d*pl2->c);
    double deltaz = a*(pl2->d*pl3->b-pl2->b*pl3->d)-pl2->a*(d*pl3->b-b*pl3->d)+pl3->a*(d*pl2->b-b*pl2->d);
	pc->x = deltax/delta;
    pc->y = deltay/delta;
    pc->z = deltaz/delta;
    return OK_AG;
}
BOOL CPlane::CroossCPlane(CPlane* pl2, CPoint3d* p1, CPoint3d* p2)
{
	CVector v1(a, b, c);
	CVector v2(pl2->a, pl2->b, pl2->c);
	if (v1.IsCollinear(&v2, DDELTA))
		return BAD;
	CPoint3d pc;
	CVector Norm(&v1, &v2);
	CPlane pl3(&pc, &Norm);//Плоскоть проходящая через точку перп. вектору
	if (crossing_3plat(this, pl2, &pl3, &p1->x))
		return BAD;
	*p2 = *p1;
	double length = 100;
	p1->Move(&Norm, -length);
	p2->Move(&Norm, length);
	return OK_AG;
}