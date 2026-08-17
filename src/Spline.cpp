// Spline.cpp: implementation of the CSpline class.
//
//////////////////////////////////////////////////////////////////////


#include "Spline.h"
#include "OutLine.h"
#include "View3d.h"
#include "Surface.h"
#include "CubSpline.h"
#include "SystemCoord.h"
#include "CAlfaDoc.h"
#include "Line.h"
#include "Arc.h"
#include "SmartLine.h"
#include "MoveKnotsBox.h"
#include "BezierSpline.h"
#include "FreeSurface.h"
#include "service.h"
#include <algorithm>
#include "Solid.h"
#include <functional>


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

extern int INKM(double RES[39], int ISK[7], double T[3], double* A);
extern int STAFM2(int IPR, int N, int LR[], double D, double* A );
extern int STAFMM(int IPR, int N, int LR[], double D, double* A );
extern int ASCCM(int IS1[7], int IS2[7],double T[3], double S[3], double E, double* A1, double* A2 );


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSpline::CSpline()
{
	Alloc(0);
}

CSpline::CSpline(int num_p)
{
	Alloc(num_p);
}

CSpline::CSpline(CCubSpline* scr)
{
	if(Alloc(scr->np()))
		return;
	for(int i=0; i<scr->np();i++){
		*P7(i)=*scr->P(i);
		P5(i)->s=scr->P(i)->x;
		P5(i)->t=scr->P(i)->y;
	}
	m_Name = scr->GetName();
}

CSpline::CSpline(CPoint3d* ap,int num_p)
{
	if(Alloc( num_p))
		return;
	for(int i=0;i<num_p;i++){
		P7(i)->x=ap[i].x;
		P7(i)->y=ap[i].y;
		P7(i)->z=ap[i].z;
		P5(i)->s=ap[i].x;
		P5(i)->t=ap[i].y;
	}
	Build();
}

CSpline::CSpline(CPoint3d* p1, CPoint3d* p2)
{
	if(Alloc( 2))
		return;

	P7(0)->x=p1->x;
	P7(0)->y=p1->y;
	P7(0)->z=p1->z;
	P5(0)->s=p1->x;
	P5(0)->t=p1->y;

	P7(1)->x=p2->x;
	P7(1)->y=p2->y;
	P7(1)->z=p2->z;
	P5(1)->s=p2->x;
	P5(1)->t=p2->y;

	Build();
}

CSpline::CSpline(CPoint7d* ap,int num_p)
{
	if(ap==NULL){
		Alloc(0);
		return ;
	}
	if(Alloc(num_p))
		return;
	for(int i=0;i<num_p;i++){
		m_p7[i]=ap[i];
		P5(i)->s=ap[i].x;
		P5(i)->t=ap[i].y;
	}
	Update();
}

CSpline::CSpline(CPoint3d* pc, Coord rad, Coord delta)
{
	if(rad==0)
		rad=10;
	if(delta<0.001)
		delta=0.01;
	delta=delta*100.0;
    if(delta>rad)
		delta=rad/5.0;
	double da=acos((rad-delta)/rad);
	double angle=2*PI;
    int num_p=short(fabs(angle/da))+1;
	if(num_p<17)
		num_p=17;
	da=angle/(num_p-1);
	if(Alloc(num_p))
		return;
	CPoint7d p(rad, 0, 0);
	p.m=1.0;
	for(int i=0;i<num_p;i++)
		m_p7[i]=p;
	CPoint3d p0;
	CPoint3d pz(0,0,100);
    for(int i=0;i<num_p;i++)
		m_p7[i].Rotate(&p0, &pz, i*da);
	Move(&p0, pc);
	Update();
}

CSpline::~CSpline()
{
	if(m_outline)
		delete m_outline;
	m_outline=NULL;
	if(m_p7)
		free(m_p7);
	m_p7=NULL;
	m_num=0;
	m_n=0;
}

CSpline::CSpline(const CSpline* src)
{
	if(Alloc( src->m_n))
		return;
	for(int i=0;i<m_n;i++){
		m_p7[i]=src->m_p7[i];
		m_p[i]=src->m_p[i];
	}
	m_ID=src->m_ID;
	m_col=src->m_col;
	m_fillet1=src->m_fillet1;
	m_fillet2=src->m_fillet2;
	m_dir=src->m_dir;

}

CLinkLine* CSpline::copy()
{
if(this==NULL)
    return NULL;
CSpline* conic=new CSpline(this);
if(conic==NULL){
	Message_err(IDS_BAD_ALLOC_MEMORY);
	return NULL;
	}

	return conic;
}


BOOL CSpline::Alloc(int num_p)
{
	m_p=0;
	m_p7=0;
	m_outline=new COutLine;
	m_InitOutLine=0;
	m_num=0;
	m_n=0;
	if(num_p>=1024){
			Message_err("Number of Point >1024");
			return BAD;
	}
	if(num_p){
		m_p7=(CPoint7d*)calloc(num_p,sizeof(CPoint7d));
		m_p=(CPoint5d*)calloc(num_p,sizeof(CPoint5d));
		if(m_p7==NULL || m_p==NULL ){
			m_n=0;
			Message_err(IDS_BAD_ALLOC_MEMORY);
			return BAD;
			}
	}
	m_n=m_num=num_p;
	return OK;
}


BOOL CSpline::Realloc(int num_p)
{
	if(num_p==0){
		if(m_p)
			free(m_p);
		m_p=NULL;
		m_n=0;
		m_p7=NULL;
		m_num=0;
		return OK;
	}
	CPoint7d* ptr =(CPoint7d*)realloc((char*)m_p7,num_p*sizeof(CPoint7d));
    if(ptr==0){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return BAD;
		}
	m_p7=ptr;
 
	m_p=(CPoint5d*)realloc((char*)m_p,num_p*sizeof(CPoint5d));
    if(m_p==0){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return BAD;
		}
	m_n=m_num=num_p;
    return OK;
}



CSpline::CSpline(CPoint7d* pc, double rad)
{
CMainFrame* pFrame = (CMainFrame*) AfxGetApp()->m_pMainWnd;
CAlfaDoc* pDoc=(CAlfaDoc*)pFrame->GetActiveDocument();

	int num_p=9;
	if(Alloc(9))
		return;

	CPoint7d p(rad, 0, 0);
	p.m=1.0;
	for(int i=0;i<num_p;i++)
		m_p7[i]=p;
	CPoint3d p0;
	CPoint3d pz(0,0,100);
	double da=2*PI/(num_p-1);
	for(int i=0;i<num_p-1;i++){
		m_p7[i].Rotate(&p0, &pz, i*da);
		m_p[i].s=m_p7[i].x;
		m_p[i].t=m_p7[i].y;
	}
	
	Update();

	double alfa=PI/4.0;
	CPoint3d p1(0,0, 100);
	Rotate(&p0, &p1, alfa);


	CSystemCoord sc((CPoint3d*)pc, (CVector*) &pc->l);
	mod_coord_ma(&sc);

//pDoc->AddFigure(new CCubSpline(P7(0), np()));
}

void CSpline::Serialize(CArchive& ar)
{
	if (ar.IsStoring()){
		ar << m_ID;
		ar<<m_n;
		ar.Write(m_p7,m_n*sizeof(CPoint7d));
		ar << m_col;
		ar << m_fillet1;
		ar << m_fillet2;
		ar.Write(m_p,m_n*sizeof(CPoint5d));
	}
	else{
		ar >> m_ID;
		ar >> m_n;
		m_p7=(CPoint7d*)calloc(m_n,sizeof(CPoint7d));
		ar.Read(m_p7,m_n*sizeof(CPoint7d));
		ar >> m_col;
		ar >> m_fillet1;
		ar >> m_fillet2;
		m_p=(CPoint5d*)calloc(m_n,sizeof(CPoint5d));
		if(CSmartLine::m_VersionSL<2){
			for(int i=0;i<m_n;i++){
				m_p[i].s=m_p7[i].x;
				m_p[i].t=m_p7[i].y;
			}
		}
		else
			ar.Read(m_p,m_n*sizeof(CPoint5d));
		m_num=m_n;
	}
}

CPoint7d* CSpline::P7(int num_p)
{
	if(num_p>=m_n){
		#ifdef _DEBUG
			Message_err("Bad index Points");
		#endif
		return m_p7;
	}
	return &m_p7[num_p];
}

CPoint3d* CSpline::P(int num_p)
{
	if(num_p>=m_n){
		#ifdef _DEBUG
			Message_err("Bad index Points");
		#endif
		return (CPoint3d*)m_p7;
	}
	return (CPoint3d*)&m_p7[num_p];
}

BOOL CSpline::Update()
{
if(this==NULL)
    return BAD;
if(m_p7==NULL){
	message_error_("m_p7==NULL");
    return BAD;
}
int lr[4]={1,4,7,7};
double d=-1;		//	izloms no	0.1-yes	
int ipr=0;		//	parametr no		1-yes

	m_InitOutLine=0;
    for(int i=0;i<m_n;i++)
		P7(i)->s=0;
//	UpdateST();
    return STAFM2(ipr, m_n, lr, d, (double*)m_p7);
}

BOOL CSpline::Build()
{
if(this==NULL)
    return BAD;
	for(int i=0;i<m_n;i++){
		P7(i)->l=P7(i)->m=P7(i)->n=P7(i)->s=0;
//		P5(i)->s=P7(i)->x;
//		P5(i)->t=P7(i)->y;
	}
    return Update();
}

void CSpline::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	if(this==NULL)
		return;
	CLinkLine::mod_coord_ma(p0,cx,cy,cz);
    for(int i=0;i<m_n;i++)
		P7(i)->mod_coord_ma(p0,cx,cy,cz);
	m_outline->mod_coord_ma(p0,cx,cy,cz);
}

void CSpline::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
	if(this==NULL)
		return;
	CLinkLine::mod_coord_am(p0,cx,cy,cz);

	for(int i=0;i<m_n;i++)
		P7(i)->mod_coord_am(p0,cx,cy,cz);
	m_outline->mod_coord_am(p0,cx,cy,cz);
}

void CSpline::mod_coord_am(CSystemCoord* sc)
{
	mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}
void CSpline::mod_coord_ma(CSystemCoord* sc)
{
	mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}
void CSpline::Move(CPoint3d* p1,CPoint3d* p2)
{
if(this==NULL)
    return;
CVector vect;
double dist=vect.calc(p1, p2);
    Move(&vect,dist);
}

void CSpline::Move(CVector* vect,double dist)
{
if(this==NULL)
    return;

    for(int i=0;i<m_n;i++)
		P(i)->Move(vect,dist);

	m_outline->Move(vect,dist);

}


void CSpline::Rotate(CPoint3d* p0,CPoint3d* p1,double alfa)
{
	if(fabs(alfa)<DDELTA)
		return;
	if(dist_POINT(p0, p1)==0)
		return;
	CVector vx(p0, p1);
	CSystemCoord SysCoordByVect(p0, &vx);
    mod_coord_am(&SysCoordByVect);

/*char buf[120];
sprintf(buf,"cx= l= %6.5f m=%6.5f n=%6.5f  ", SysCoordByVect.cx.l, SysCoordByVect.cx.m, SysCoordByVect.cx.n);
Step(buf);
sprintf(buf,"cy= l= %6.5f m=%6.5f n=%6.5f  ", SysCoordByVect.cy.l, SysCoordByVect.cy.m, SysCoordByVect.cy.n);
Step(buf);
sprintf(buf,"cz= l= %6.5f m=%6.5f n=%6.5f  ", SysCoordByVect.cz.l, SysCoordByVect.cz.m, SysCoordByVect.cz.n);
Step(buf);
*/
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
    mod_coord_am(&p0m,&cxm,&cym,&czm);
    mod_coord_ma(&SysCoordByVect);
}

void CSpline::Revers()
{
register int i,j;

    for(i=0,j=m_n-1;i<j;i++,j--){
		CPoint7d ptm=m_p7[i];
		m_p7[i]=m_p7[j];
		m_p7[j]=ptm;
	}
    for(i=0,j=m_num-1;i<j;i++,j--){
		CPoint5d ptm=m_p[i];
		m_p[i]=m_p[j];
		m_p[j]=ptm;
	}
	for(i=0;i<m_n;i++){
		m_p7[i].l=-m_p7[i].l;
		m_p7[i].m=-m_p7[i].m;
		m_p7[i].n=-m_p7[i].n;
	}
	Update();
}

BOOL CSpline::JoinG(CSpline *line2)
{
	if(this==line2){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	if(m_n==0){
		Realloc(line2->np());
		 for(int j=0;j<line2->np();j++)
			*P7(j)=*line2->P7(j);
		return OK;
	}
	int num_beg=np();
    if(Realloc(np()+line2->np()-1))
		return BAD;
    for(register int i= num_beg,j=1;j<line2->np();i++,j++){
		*P7(i)=*line2->P7(j);
		*P5(i)=*line2->P5(j);
	}
	return Update();
}

bool CSpline::JoinSL(CSpline *line2)
{
	double delta =0.05;
	Coord angle=delta*5.0;
	CVector v1(P7Last()->l, P7Last()->m, P7Last()->n);
	CVector v2(line2->P7(0)->l, line2->P7(0)->m, line2->P7(0)->n);
	if(!v1.IsCollinear(&v2, angle))
		return JoinX(line2);

	JoinG(line2);
	return true;
}

bool CSpline::JoinX(CSpline *line2)
{
	double dd1=0.1;
	double dd2=0.25;
	double len = line2->P7Last()->s;
	double kf = len / 30.0;
	double kfMin = 0.06;
	if (kf < kfMin)
		kf = kfMin;
	if (kf < 1.0) {
		dd1 *= kf;
		dd2 *= kf;
	}

	int var = 1;
	bool insert_point=false;

	ExtendSL(-(dd1+dd2), var, insert_point);
	ExtendSL(dd2, var, true);
//	::AddPoint(PLast(), RGB_RED);
	var = 0;
	line2->ExtendSL(-(dd1+dd2), var,  false);
	line2->ExtendSL(dd2, var, true);
//	::AddPoint(line2->P(0), RGB_RED);

	AddKnot(PLast());
	AddKnot(line2->P(0));

	int num_beg=np();  
    if(Realloc(np()+line2->np()))
		return false;
	for(int i= num_beg,j=0;j<line2->np();i++,j++){
		*P7(i)=*line2->P7(j);
		*P5(i)=*line2->P5(j);
	}

	int pr = Update();
	return (pr == OK);
}

BOOL CSpline::AddPoint(CPoint3d* p, bool rebuild)
{
	if(!p)
		return BAD;
	CPoint3d pa = *p;

	if(Realloc(m_n+1))
		return BAD;
	PLast()->x= pa.x;
	PLast()->y= pa.y;
	PLast()->z= pa.z;
	if(m_n<2)
		return OK;
	if(!rebuild)
		return OK;
	return Build();
}

BOOL CSpline::GetPoint(double s,CPoint7d *p,short extra)
{
if(this==NULL)
    return BAD;
Coord res[39];
int isk[7]={1,12,7,2,1,30,0};
double t[3];

    isk[0]=extra;
    isk[3]=m_n;
    t[0]=double(s+1);
    t[1]=1;
    t[2]=m_n;

    if(INKM(res, isk, t, (double*)m_p7))
		return BAD;
    p->x=res[0];
    p->y=res[1];
    p->z=res[2];

    p->l=res[3];
    p->m=res[4];
    p->n=res[5];
    p->s=s;

	CVector* vect=(CVector*)&p->l;
    return vect->Normalize();
}


Coord CSpline::GetOrthPoint(CPoint3d* p)
{
Coord s_min=0;
int ns=10;
Coord ds=1/(Coord)ns;
CPoint7d pn;
double dist_min=1e15;
double dist;

    for(int j=0;j<m_n-1;j++)
		for(int i=0;i<ns;i++){
			GetPoint(j+i*ds, &pn);
			dist=dist_POINT(&pn.x, &p->x);
			if(dist_min>dist){
				dist_min=dist;
				s_min=j+i*ds;
			}
	    }
	int n_iter=6;
    ds=0.15;
    for(int i=0;i<n_iter;i++){
		 s_min=Iteration(p, ds, s_min);
		 ds*=0.15;
	 }
    return s_min;
}

Coord  CSpline::Iteration(CPoint3d* p, Coord ds, Coord s_min)
{
Coord sb=s_min-ds/2.0;
int ns=10;
Coord dds=ds/(Coord)ns;
CPoint7d pn;
double dist_min=1e15;
double dist;

    for(int i=0;i<ns;i++){
		GetPoint(sb+i*dds, &pn);
		dist=dist_POINT(&pn.x, &p->x);
		if(dist_min>dist){
			dist_min=dist;
			s_min=sb+i*dds;
			}
	 }
    return s_min;
}

BOOL CSpline::InsertPoint(Coord s)
{
    int N=(short)floor(s);
    if(s<0||s>m_n-1){
//		message_error_("Bad parametr s for insert_Point!");
		return BAD;
	}
	CPoint7d p;
    if(GetPoint(s,&p))
		return BAD;
    return InsertPoint(N, &p);
}

BOOL CSpline::InsertPoint(int N, CPoint7d* p)
{
    if(N<-1||N>m_n-1){
		message_error_("Bad parametr N for insert_Point!");
	return BAD;
	}
	int i_min=0;
	double dist = FindPoint((CPoint3d*)p, &i_min);
	if(dist<DELTA)
		return OK;

    if(Realloc(m_n+1))
		return BAD;
	int i=0;
    for(i=m_n-1;i>N+1;i--){
		m_p7[i]=m_p7[i-1];
		m_p[i]=m_p[i-1];
	}
    m_p7[i]=*p;
    m_p[i].s=p->x;
    m_p[i].t=p->y;

    return Update();
}

double CSpline::FindPoint(CPoint3d* pm, int* i_min)
{
double dist_min=1e15;
	for(int i=0;i<m_n;i++){
		double dist=dist_POINT(pm, P(i));
		if(dist_min>dist){
		    dist_min=dist;
		    *i_min=i;
		    }
	}
	return dist_min;
}

CLinkLine* CSpline::InsertPoint(CPoint3d* p)
{
	double s=GetOrthPoint(p);
	if(InsertPoint(s))
		return NULL;
	int Index=0;
	FindPoint(p, &Index);
	return Simplify(Index);
}

CSpline* CSpline::Simplify(int num_p)
{
	if(num_p<0 || num_p>=m_n){
		Message_err("BAD Index Point Simplify!");
		return NULL;
		}
	if(num_p==0 || num_p==m_n-1){
		Message_err(IDS_NO_SIMPLIFY_END_POINTS);
		return NULL;
		}
	CSpline* line2=new CSpline;
	if(line2==NULL){
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return NULL;
		}

	if(line2->Realloc(m_n-num_p))
		return NULL;

	for(int i=0,j=num_p;j<m_n;i++,j++){
		line2->m_p7[i]=m_p7[j];
		line2->m_p[i]=m_p[j];
	}
	m_n=m_num=num_p+1;
	Update();
	line2->Update();
	return line2;
}
BOOL CSpline::Join(CSpline* spline2, double delta)
{
	if(!spline2){
		Message_err("spline2==NULL!");
		return BAD;
		}
	if(!spline2->IsKindOf( RUNTIME_CLASS( CSpline ) )){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
		}
	if(this==spline2){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	
	if(m_n==0)
		return JoinG(spline2 );
double dist1=dist_POINT(PLast(), spline2->P(0));
    if(dist1<delta)
		return  JoinG( spline2);

double dist2=dist_POINT(PLast(), spline2->PLast());
    if(dist2<delta){
		spline2->Revers();
		return  JoinG( spline2 );
	}

double dist3=dist_POINT(P(0), spline2->P(0));
    if(dist3<delta){
		Revers();
		return  JoinG( spline2 );
	}
double dist4=dist_POINT(P(0), spline2->PLast());
    if(dist4>delta ){
		return 2;
	}
	Revers();
	spline2->Revers();
    return  JoinG(spline2 );
}

double CSpline::GetLength()
{
	return P7Last()->s;
}

BOOL CSpline::GetPointLength(double length, CPoint7d *p, short extra)
{
int num=0;
Coord s=0;

    if(m_p7[0].s>0){
		message_error_("BAD spline!\n p[0].s>0");
		return BAD;
	}
    if(length>P7(m_n-1)->s)
		num=m_n-1;
    else
		while(length>P7(num)->s)
			num++;
Coord ds=length-P7(num)->s;
    if(length==0)
		s=0;
    else{
		if(length>0){
			s=ds/(P7(num)->s-P7(num-1)->s);
			s+=num;
			}
		else
		  s=ds/(P7(1)->s-P7(0)->s);  
	}
    return GetPoint(s, p, extra);
}

BOOL CSpline::SetPoints(CSpline *exam)
{
	if(this==exam)
		return OK;
    if(exam->P7(exam->m_n-1)->s==0){
		message_error_("Bad example for setting point, s=0!!");
		return BAD;
		}
	long double koeff=P7(m_n-1)->s/exam->P7(exam->m_n-1)->s;
	CPoint7d* pnts=(CPoint7d*)calloc(exam->np(), sizeof(CPoint7d));
	if(pnts==NULL){
		message_error_(IDS_BAD_ALLOC_MEMORY);
		return BAD;
	}
	int i=0;
    for( i=1;i<exam->np()-1;i++)
		if(GetPointLength(exam->P7(i)->s*koeff, &pnts[i]))
			return BAD;
	pnts[i]=*P7(m_n-1);

    if(Realloc(exam->np()))
		return BAD;
    for(i=1;i<m_n;i++)
		m_p7[i]=pnts[i];
	free(pnts);
    return Update();
}

BOOL CSpline::parametrL_to_s(int num,Coord Length,Coord *s)
{
	double ds;
    if(num<0 || num>=m_n-1){
		message_error_("Bad number parametrL_to_s!!");
		return BAD;
	}
	double distPP=P7(num+1)->s-P7(num)->s;
    while(distPP<Length && num<m_n-2){
		Length-=distPP;
		num++;
		distPP=P7(num+1)->s-P7(num)->s;
	}
    if(distPP<Length-DDELTA){
		message_error_("Too large Length parametrL_to_s!!");
		return BAD;
	}
    if(distPP==0)
		ds=0;
    else
		ds=Length/distPP;
    *s=num+ds;
    return OK;
}

BOOL CSpline::DeletePoint(int N)
{
	if(m_n==2){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);	
		return BAD;
	}
	if(N>=m_n || N<0){
		Message_err("Bad Number point to delete!");	
		return BAD;
	}
    m_n--;
    for(int i=N;i<m_n;i++)
		m_p7[i]=m_p7[i+1];

    m_num--;
    for(int i=N;i<m_num;i++)
		m_p[i]=m_p[i+1];
	return Update();
}

BOOL CSpline::ControlPoints(double delta)
{

    if(m_n<2)
		return BAD;
    for(register int i=0;i<m_n-1;i++)
		while(i<m_n-1 && (dist_POINT(&m_p7[i].x,&m_p7[i+1].x)< delta) ) /* points sovpadajut */
			if(DeletePoint(i))
				return BAD;
    return OK;
}

BOOL CSpline::AddPointExample(CSpline *exam)
{
    if(exam->P7Last()->s==0){
		message_error_("Bad example for setting point!!");
		return BAD;
	}

	Coord s;
    for(int i=1;i<exam->np()-1;i++){
		long double koeff=P7Last()->s/exam->P7Last()->s;
		double Len = exam->P7(i)->s*koeff;
		if(parametrL_to_s(0, Len, &s))
		{
			return BAD;
		}
		double ostatok=s-floor(s);
//		if(ostatok>0.05 && ostatok<0.95)	
		if(ostatok>0.005 && ostatok<0.995)	
			if(InsertPoint(s))
				return BAD;
	}
    if(ControlPoints(0.02))
		return BAD;
    return OK;
}

BOOL CSpline::ConversionStep(CSpline *exam1, CSpline *exam2, Coord step)
{
	return ConversionStep2(exam1, exam2, step);

	CSpline exam1_tmp(exam1);
	CSpline exam2_tmp(exam2);

    if(exam1_tmp.AddPointExample(exam2))
		return BAD;
    if(exam2_tmp.SetPoints(&exam1_tmp))
		return BAD;
    if(Alloc(exam1_tmp.np()))
		return BAD;

    for(int i=0;i<m_n;i++){
		P(i)->x=exam1_tmp.P(i)->x+(exam2_tmp.P(i)->x-exam1_tmp.P(i)->x)*step;
		P(i)->y=exam1_tmp.P(i)->y+(exam2_tmp.P(i)->y-exam1_tmp.P(i)->y)*step;
		P(i)->z=exam1_tmp.P(i)->z+(exam2_tmp.P(i)->z-exam1_tmp.P(i)->z)*step;
	}
   return Build();
}



BOOL CSpline::ConversionStep2(CSpline *exam1, CSpline *exam2, Coord step)
{
	vector<double> pnts;
	pnts.resize(exam1->np()+ exam2->np()-2);
    for(int i=0;i<exam1->np();i++)
		pnts[i]= exam1->P7(i)->s;
	
	double k1= exam1->P7Last()->s/exam2->P7Last()->s;
    for(int i=exam1->np(), j=1 ; j <exam2->np()-1; i++, j++)
		pnts[i]= exam2->P7(j)->s * k1;

	std::sort(pnts.rbegin(), pnts.rend(), std::greater<double>());

	double deltas = DELTA;
	for(int i= pnts.size()-1; i>0 ; i--)
	{
		if(fabs(pnts[i] - pnts[i-1]) < deltas)
			pnts.erase(pnts.begin()+i);
	}
	
	if(Alloc(pnts.size() ))
		return BAD;
	double k2= exam2->P7Last()->s/exam1->P7Last()->s;

	for(int i=0; i< pnts.size(); i++)
	{
		CPoint7d p1;
		exam1->GetPointLength(pnts[i], &p1);
		CPoint7d p2;
		exam2->GetPointLength(pnts[i]*k2, &p2);
		P(i)->x =p1.x +(p2.x - p1.x)*step;
		P(i)->y =p1.y +(p2.y - p1.y)*step;
		P(i)->z =p1.z +(p2.z - p1.z)*step;
	}
   return Build();
}



void CSpline::Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size)
{
	if(!m_InitOutLine)
		UpdateOutLine(0.1);
	m_outline->Get_Min_Max(cx, cy, cz, size);

}

void CSpline::Zoom(CPoint3d* p0,double Kx,double Ky,double Kz)
{
	for(int i=0;i<m_n;i++)
		P(i)->Zoom(p0, Kx, Ky, Kz);
	if(Kx!=Ky || Kx!=Kz || Ky!=Kz)
		Build();
	else
		Update();
	m_outline->Zoom(p0, Kx, Ky, Kz);
}

bool CSpline::ConrolDelta(int num,Coord dopusk,Coord ds,int *pr)
{
    *pr=0;
	CPoint7d p2;
    if(GetPoint(num+ds,&p2))
		return BAD;
	if(dopusk>0.005)
		dopusk/=2.0;
	CVector vector3d((CPoint3d*)P(num), (CPoint3d*)&p2);
	Coord dds=ds/10.0;
	CPoint7d pm;
	for(int i=1;i<10;i++){
		if(GetPoint(num+dds*i,&pm))
			return BAD;
		Coord  delta=dist_POINT_LINE(&pm.x,&(P(num)->x),&vector3d.l);
		if(delta>dopusk)
			(*pr)++;
	}
	
   return OK; 
}
void Set_Color(Pixel col);
void CSpline::Draw(CView3d* pview)
{
if(this==NULL)
    return;
    if(m_selected)
		Set_Color(~m_col);
    else
		Set_Color(m_col);

	if(!m_InitOutLine)
		UpdateOutLine(pview->m_delta);
	if(m_outline)
		m_outline->Draw();
	if(pview->draw_knots)
		for (int i=0; i<m_n; i++)
			pview->DrawKnot(P(i), RGB_POINT);
}

bool CSpline::UpdateOutLine(double dopusk)
{
	if(dopusk>0.005)
		dopusk/=2.0;
	if(m_fillet1 || m_fillet2){
		CLine* line= MakeLine(dopusk);
		if(!line)
			return BAD;
		if(m_outline->Realloc(line->np()))
			return BAD;
		for(int i=0;i<line->np();i++)
			*m_outline->P(i)=*line->P(i);
		m_InitOutLine=1;
		return OK;
	}

	if(m_outline->Realloc(m_n))
		return BAD;
	for(int i=0;i<m_n;i++){
		m_outline->P(i)->x=P(i)->x;
		m_outline->P(i)->y=P(i)->y;
		m_outline->P(i)->z=P(i)->z;
	}
int np=-1;
CPoint7d pm;
int pr;
    for(int j=0;j<m_n-1;j++){
		Coord ds=1;
		np++;
		if(ConrolDelta(j,dopusk,ds,&pr))
			return BAD;
		while(pr!=OK && ds>0.00001){
			ds=ds/2.0;
			if(ConrolDelta(j,dopusk,ds,&pr))
				return BAD;
			}
		int n=(int)ceil(1/ds);
		for(int i=1;i<n;i++){
			if(GetPoint(j+ds*i,&pm))
				return BAD;
			CPoint3d p(&pm);
			if(m_outline->InsertPoint(np,&p)){
				m_InitOutLine=1;
				return OK;
			}
			np++;
			}
	}
	m_InitOutLine=1;
//	AllocExtremumPoints();	
	return OK;
}

BOOL CSpline::Offset(Coord offset)
{
	for(int i=0;i<m_n;i++){
		CPoint3d p1=*P(i);
		CPoint3d p2=p1;
		p2.Move((CVector*)&P7(i)->l,10);
		P(i)->Offset(offset, &p1, &p2);
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
	return Update();
}

CLine* CSpline::MakeLineAllLength(double dopusk)
{
	CLine* line=new CLine(m_n);
	if(line==NULL){
		message_error_(IDS_BAD_ALLOC_MEMORY);
		return NULL;
	}
    for(int i=0;i<m_n;i++){
	 line->P(i)->x=P(i)->x;
	 line->P(i)->y=P(i)->y;
	 line->P(i)->z=P(i)->z;
	}
	int np=-1;
	CPoint7d pm;
	int pr;	
    for(int j=0;j<m_n-1;j++){
		Coord ds=1;
		np++;
		if(ConrolDelta(j,dopusk,ds,&pr))
			return NULL;
		while(pr!=OK && ds>0.002){
			ds=ds/2.0;
			if(ConrolDelta(j,dopusk,ds,&pr))
				return NULL;
			}
		int n=(short)ceil(1/ds);
		for(int i=1;i<n;i++){
			if(GetPoint(j+ds*i,&pm))
				return NULL;
			CPoint3d p(&pm);
			if(line->InsertPoint(np,&p))
				return NULL;
			np++;
			}
	}
	return line;
}

CLine* CSpline::MakeLine(double dopusk)
{
	if(!m_fillet1 && !m_fillet2)
		return MakeLineAllLength(dopusk);


	Coord ds_min=1;
    for(int j=0;j<m_n-1;j++){
		Coord ds=1;
		int pr;
		if(ConrolDelta(j,dopusk,ds,&pr))
			return NULL;
		while(pr!=OK && ds>0.002){
			ds=ds/2.0;
			if(ds_min>ds)
				ds_min=ds;
			if(ConrolDelta(j,dopusk,ds,&pr))
				return NULL;
			}

	}
	double Sb=0;
	double Se=np()-1;
	if(m_fillet1){
		CPoint3d* Pb=m_sl->P1(this);
		Sb=GetOrthPoint(Pb);
	}
	if(m_fillet2){
		CPoint3d* Pe=m_sl->P2(this);
		Se=GetOrthPoint(Pe);
	}
	double Sb_Se=Se-Sb;
	int	np=(short)(Sb_Se/ds_min);	/*	number points	*/
	
	CLine* line=new CLine(np+1);

	for(register int i=0;i<=np;i++){
		CPoint7d p7;
		GetPoint(Sb+ds_min*i, &p7);
		line->P(i)->x=p7.x;
		line->P(i)->y=p7.y;
		line->P(i)->z=p7.z;
	}
	if(m_fillet1 || m_fillet2){
		CPoint7d P;
		GetPoint(Se, &P);
		line->AddPoint((CPoint3d*)&P);
	}
	line->ControlPoints(0.01);
	return line;
}

Coord CSpline::Localized_point(Coord s,Coord ds,CPlane *pl,CPoint7d *p)
{
CPoint7d p1;
CPoint7d pm;
register int i=0;
double delta=DELTA/10.0;


    ds=ds/2.0;
    GetPoint(s,&p1);
    GetPoint(s+ds,&pm);
	register double de1=pm.x*pl->a+pm.y*pl->b+pm.z*pl->c+pl->d;
	register double de2=p1.x*pl->a+p1.y*pl->b+p1.z*pl->c+pl->d;
    while(ds>DDELTA && fabs(de1)>delta && i<100){
		if((de1>0&&de2>0)||(de1<0&&de2<0))	
			s+=ds;
		ds=ds/2.0;
		GetPoint(s,&p1);
		GetPoint(s+ds,&pm);
		de1=pm.x*pl->a+pm.y*pl->b+pm.z*pl->c+pl->d;
		de2=p1.x*pl->a+p1.y*pl->b+p1.z*pl->c+pl->d;
		i++;
	}
    *p=pm;
    return s+ds;
}

int CSpline::CrossPlane(CPlane* pl, CPoint3d** p)
{
	int n = 0;//	num of point crosses
	register int i, j;
	register double de1, de2;
	register double ds;
	register double delta;
	register int np;
	CPoint7d pm;
	CPoint7d p2;

    delta=P(0)->x*pl->a+P(0)->y*pl->b+P(0)->z*pl->c+pl->d;
    if(fabs(delta)<DELTA){
		*p=(CPoint3d *)realloc((char*)*p,(n+1)*sizeof(CPoint3d));
		if(*p==NULL){
			message_error_("No memory!");
			return 0;
			}
		(*p)[n].x=P(0)->x;
		(*p)[n].y=P(0)->y;
		(*p)[n].z=P(0)->z;
		n++;
	}	
    for(j=0;j<m_n-1;j++){
		ds=1;
		delta=dist_POINT_LINE(&P(j+1)->x,&P(j)->x,&P7(j)->l);
		while(delta>DELTA&&ds>0.000001){
			ds=ds/2;
			GetPoint(j+ds,&pm,3);
			delta=dist_POINT_LINE(&pm.x,&(P(j)->x),&(P7(j)->l));
			}

		np=(short)(1/ds);	/*	number adding points	*/
		for(i=0;i<np;i++){
			GetPoint(j+ds*i,&pm,3);
			GetPoint(j+ds*(i+1),&p2,3);
			de1=pm.x*pl->a+pm.y*pl->b+pm.z*pl->c+pl->d;
			de2=p2.x*pl->a+p2.y*pl->b+p2.z*pl->c+pl->d;			
			if((de1>=0&&de2<0)||(de1<0&&de2>=0)){		/*	Cross	*/
				*p=(CPoint3d *)realloc((char*)*p,(n+1)*sizeof(CPoint3d));
				if(*p==NULL){
					message_error_("No memory!");
					return 0;
					}
				Localized_point(j+ds*i,ds,pl,&pm);
				(*p)[n].x=pm.x;
				(*p)[n].y=pm.y;
				(*p)[n].z=pm.z;
				n++;
			}
		}
	}

    delta=PLast()->x*pl->a+PLast()->y*pl->b+PLast()->z*pl->c+pl->d;
    if(fabs(delta)<DELTA){
		*p=(CPoint3d *)realloc((char*)*p,(n+1)*sizeof(CPoint3d));
		if(*p==NULL){
			message_error_("No memory!");
			return 0;
			}
		(*p)[n].x=PLast()->x;
		(*p)[n].y=PLast()->y;
		(*p)[n].z=PLast()->z;
		n++;
	}
    return n;
}

int CSpline::CrossLine(CLinkLine* line,CPoint3d** pc)
{
	CLine* line1=MakeLine(0.02);
	CLine* line2=line->MakeLine(0.02);
	if(!line1|| !line2)
		return 0;
	int np=line1->CrossLine(line2, pc);

	delete line1;
	delete line2;
	return np;
}


int CSpline::CrossLine(LINE_2P* line2 ,CPoint3d** pc)
{
	CLine* line=MakeLine(0.02);
	if(!line)
		return 0;
	int np=line->CrossLine(line2, pc);
	delete line;
	return np;
}

int CSpline::CrossLine2D(LINE_2P* line2,CPoint3d* pc1, CPoint3d* pc2)
{
	if(!line2)
		return 0;
	CPoint3d* pc=NULL;
	int np=CrossLine(line2, &pc);
	if(np)
		*pc1=pc[0];
	if(np>0)
		*pc2=pc[1];
	if(pc)
		free(pc);
	return np;
}

void CSpline::Extend(double dist, int var, bool insert_point)
{

	if(np()==2 || insert_point==false){
		int n=var==0?0:m_n-1;
		if(var==0)
			dist=-dist;
		P(n)->Move((CVector*)&P7(n)->l, dist);
		P5(n)->s=P(n)->x;
		P5(n)->t=P(n)->y;
		Update();
		return ;
	}

	CPoint7d p2;
	if(var==0){
		p2=*P7(0);
		p2.Move((CVector*)&P7(0)->l, -dist);
		InsertPoint(-1, &p2);
		return ;
	}

	p2=*P7Last();
	p2.Move((CVector*)&P7Last()->l, dist);
	InsertPoint(m_n-1, &p2);
}

void CSpline::ExtendSL(double dist, int var, bool insert_point)
{

	if( insert_point==false){
		int n=var==0?0:m_n-1;
		if(var==0)
			dist=-dist;
		P(n)->Move((CVector*)&P7(n)->l, dist);
		P5(n)->s=P(n)->x;
		P5(n)->t=P(n)->y;
		Update();
		return ;
	}

	CPoint7d p2;
	if(var==0){
		p2=*P7(0);
		p2.Move((CVector*)&P7(0)->l, -dist);
		InsertPoint(-1, &p2);
		return ;
	}

	p2=*P7Last();
	p2.Move((CVector*)&P7Last()->l, dist);
	InsertPoint(m_n-1, &p2);
}

int CSpline::MakeKnot(CLinkLine* line2)
{
	if(!line2)
		return BAD;
	if(dist_POINT(PLast(), line2->P(0))<0.01)
		return OK;
	if(line2->IsKindOf( RUNTIME_CLASS( CSpline ) ))
		return MakeKnotSpline((CSpline*)line2);
	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) )){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return OK;
	}
	CPoint3d* pc=NULL;
	int nc=CrossLine(line2, &pc);
    if(nc>0){
		double dist_min=1e15;
		int i_min=0;
		for(int i=0;i<nc;i++){
			double dist=dist_POINT(PLast(), &pc[i]);
			if(dist_min>dist){
				dist_min=dist;
				i_min=i;
			}
		}	
		CPoint3d pc1=pc[i_min];
		free(pc);
		CLinkLine* link= InsertPoint(&pc1);
		delete link;
		line2->P(0)->x=line2->P5(0)->s=pc1.x;
		line2->P(0)->y=line2->P5(0)->t=pc1.y;
	return OK;
	}


	CPoint3d p1=*PLast();
	CPoint3d p2=p1;
	p2.Move((CVector*)&P7Last()->l, 10);
	LINE_2P link1(&p1, &p2);
	LINE_2P link2(line2->P(0), line2->P(1));
	link1.CrossLine2D(&link2, &p1);

	EditLastPoint(&p1, 0);

	line2->P(0)->x=line2->P5(0)->s=p1.x;
	line2->P(0)->y=line2->P5(0)->t=p1.y;
	return OK;
}

int CSpline::MakeKnotSpline(CSpline* line2)
{
	return OK;
}

int CSpline::MakeFirstKnot(CLinkLine* line2)
{
	if(!line2)
		return BAD;
	if(dist_POINT(P(0), line2->PLast())<0.003)
		return OK;
	if(line2->IsKindOf( RUNTIME_CLASS( CSpline ) ))
		return MakeFirstKnotSpline((CSpline*)line2);

	if(!line2->IsKindOf( RUNTIME_CLASS( CLinkLine ) )){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}


	CPoint3d p1=*P(0);
	CPoint3d p2=p1;
	p2.Move((CVector*)&P7(0)->l, -10);
	LINE_2P link1(&p1, &p2);
	LINE_2P link2(line2->P(0), line2->P(1));
	link1.CrossLine2D(&link2, &p1);
	double dist=dist_POINT(P(0),&p1);
	Extend(dist*2.0, 0);
	line2->Extend(dist*2.0, 1);
	LINE_2P link3(line2->P(0), line2->P(1));

	CPoint3d* pc=NULL;
	int nc=CrossLine(&link3, &pc);
    if(nc==0){
		*line2->P(1)=*P(0);
		return OK;
	}
	double dist_min=1e15;
	int i_min=0;
	for(int i=0;i<nc;i++){
		double dist=dist_POINT(P(0), &pc[i]);
		if(dist_min>dist){
			dist_min=dist;
			i_min=i;
		}
	}	
	CPoint3d pc1=pc[i_min];
	CSpline* link= (CSpline*)InsertPoint(&pc1);
	if(!link)
		return BAD;
	if(Realloc(link->np()))
		return BAD;
	for(int i=0;i<m_n;i++){
		m_p7[i]=link->m_p7[i];
		m_p[i]=link->m_p[i];
	}
	delete link;
	line2->P(1)->x=line2->P5(1)->s=pc1.x;
	line2->P(1)->y=line2->P5(1)->t=pc1.y;

	return OK;
}

int CSpline::MakeFirstKnotSpline(CSpline* line2)
{
	return OK;

}

int CSpline::MakeLastKnot()
{
	CLinkLine* line2=m_sl->GetLastLine();
	if(!line2)
		return BAD;
	return MakeFirstKnot(line2);

}

BOOL CSpline::EditPoint(int np, CPoint3d* p, int )
{
	if(np>=m_n){
		Message_err("np>=m_num!");
		return BAD;
		}
	if(np<0){
		Message_err("np<0!");
		return BAD;
		}

	*P(np)=*p;
	P5(np)->s=p->x;
	P5(np)->t=p->y;

//	return Update();
	return Build();
}

BOOL CSpline::EditLastPoint(CPoint3d* p, int constr)
{

	return EditPoint(m_n-1, p, constr);
}

double CSpline::FindPoint(CPoint3d* pm, CView3d* view, int* i_min)
{
double dist_min=1e15;
double dist;
CPoint3d pgr;

	if(view->draw_knots)
		for(int i=0;i<m_n;i++){
			P(i)->GetGrPoz(view, &pgr);
			dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
			if(dist_min>dist){
				dist_min=dist;
				*i_min=i;
				}
		}
	else{
		P(0)->GetGrPoz(view, &pgr);
		dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
		if(dist_min>dist){
			dist_min=dist;
			*i_min=0;
			}
		PLast()->GetGrPoz(view, &pgr);
		dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
		if(dist_min>dist){
			dist_min=dist;
			*i_min=m_n-1;
			}
		}
	return dist_min;
}

BOOL CSpline::GetPoint(int num, CPoint3d* p)
{
if(this==NULL){
	Message_err("CLinkLine=NULL !");
	return BAD;
	}
	if(num>=m_n){
		#ifdef _DEBUG
			Message_err("Bad index Points");
		#endif
		return BAD;
		}
	*p=*P(num);
	return OK;
}

double CSpline::Get_dist_Min(CPoint3d* pm, CView3d* view)
{
if(this==NULL)
    return 1e15;
	if(!m_InitOutLine)
		UpdateOutLine(view->m_delta);
   return m_outline->Get_dist_Min(pm, view);
}


void CSpline::print(CMemFile* file)
{
	if(!m_sl)
		return;
	char buf[180];
	char nl[3]="";
	nl[0]=13;
	nl[1]=10;
	nl[2]=0;
    sprintf(buf, "Spline%s",nl);

	file->Write(buf,strlen(buf));

    sprintf(buf,"Index=%d, ID=%d%s",m_sl->GetIndexLine(this), m_ID, nl);
	file->Write(buf,strlen(buf));

	for(int i=0;i<m_n;i++){
		sprintf(buf, "%d ",i);
		file->Write(buf,strlen(buf));
		P7(i)->print(file);
	}
}

void CSpline::print(FILE* fil)
{
if(this==NULL)
    return;
	if(!fil)
		fopen_s(&fil,"c:\\stdout.txt","a+");
    if(fil==NULL)
		return ;
    fprintf(fil,"CSpline np=%d  \n",m_n );
	char buf[80];
	for(int i=0;i<m_n;i++){
		sprintf(buf,"%d",i);
		P7(i)->print(buf, fil);
	}
	fclose(fil);
}

void CSpline::print()
{
	FILE* strm = fopen("c:\\stdout.txt", "a+");;
	if (NULL == strm)
		return;

	fprintf(strm, "SPLINE\n");
	fprintf(strm, " %d\n", m_n);

	for (int i = 0; i<m_n; i++)
		fprintf(strm, "%10.2lf  %10.2lf %10.2lf\n", P7(i)->x, P7(i)->y, P7(i)->z);
	fclose(strm);
}

CSpline* CSpline::MakeSpline()
{
	return new CSpline(this);
}

void CSpline::GetDir(CVector* v)
{
	v->l=P7(0)->l;
	v->m=P7(0)->m;
	v->n=P7(0)->n;
}

void CSpline::GetLastDir(CVector* v)
{
	v->l=P7Last()->l;
	v->m=P7Last()->m;
	v->n=P7Last()->n;
}

void CSpline::Mirror()
{
if(this==NULL)
    return;
    for(int i=0;i<m_n;i++)
		P7(i)->Mirror();
	Update();

}

void CSpline::MirrorX()
{
	CPoint3d pm;
	GetCenterPoint(&pm);

	CPlane plXY(0,0,1,0);
	CVector dir(0,1,0);
	CPlane pl(&pm, &dir, &plXY);

	CSystemCoord SC;
	if(SC.GetMirrorSC(&pl)){
		return ;
	}

	mod_coord_am(&SC);
	Mirror();
	mod_coord_ma(&SC);
}

void CSpline::AddKnotsRect(CSmartLine* sl, CPoint3d* p1, CPoint3d* p2,\
		CView3d* view, CMoveKnotsBox* box)
{
	CSelectPrim knot;
	knot.type=TYPE_KNOT;
	knot.m_part=sl->m_part;
	knot.ID=sl->GetID();
//	knot.num_line=sl->GetIndexLine(this);
	knot.num_line=m_ID;
	for(int i=0;i<m_n;i++)		
		if(P7(i)->InRect(p1,p2,view)){
			knot.num_pnt=i;
			if(box->Add(knot)==OK){
				CKnot* pnt=new CKnot(&knot, 0, RGB_POINT);
				view->AddFigure(pnt);
			}
		}
}


void CSpline::RemoveKnotsRect(CSmartLine* sl, CPoint3d* p1, CPoint3d* p2,\
							 CView3d* view, CMoveKnotsBox* box)
{
	CSelectPrim knot;
	knot.type=TYPE_KNOT;
	knot.m_part=sl->m_part;
	knot.ID=sl->GetID();
	knot.num_line=sl->GetIndexLine(this);
	for(int i=0;i<m_n;i++)		
		if(P7(i)->InRect(p1,p2,view)){
			knot.num_pnt=i;
			if(box->RemoveKnot(&knot)==OK)
				view->RemoveKnot(&knot);
		}
}

BOOL CSpline::InRect(CPoint3d* p1, CPoint3d* p2 , CView3d* view)
{
	for(int i=0;i<m_n;i++)		
		if(P7(i)->InRect(p1,p2,view))
			return 1;
    return 0;
}

BOOL CSpline::InRect(CPoint3d* p1, CPoint3d* p2)
{
	for(int i=0;i<m_n;i++)		
		if(P7(i)->InRect(p1,p2))
			return 1;
    return 0;
}

void CSpline::Draw(CDC* pDC, double dx,double dy, double kx, double ky)
{
if(this==NULL)
    return;
    if(m_selected)
		Set_Color(CFigure::Selected_col);
    else
		Set_Color(m_col);

	if(!m_InitOutLine)
		UpdateOutLine(0.1);
	if(m_outline)
		m_outline->Draw(pDC,dx, dy, kx, ky);

	
}

void CSpline::GetMidlePoint(CPoint3d* pm)
{
	CLine* line=MakeLine(0.03);
	if(!line)
		return ;
	line->GetMidlePoint(pm);
	delete line;
}

void CSpline::GetCenterPoint(CPoint3d* pm)
{
	CLine* line=MakeLine(0.03);
	if(!line)
		return ;
	line->GetCenterPoint(pm);
	delete line;
}

BOOL CSpline::write_file_dxf(FILE *)
{
/*	CLine* line=MakeLine(0.03);
	if(!line)
		return BAD;
	line->write_file_dxf(fil, )
		delete line;
*/
	return OK;
}

BOOL CSpline::EditFirstPoint(CPoint3d* p)
{
	return EditPoint(0, p, 0);
}

void CSpline::Set_MSK()
{
	for(int i=0;i<m_num;i++){
		P(i)->x=P5(i)->s;
		P(i)->y=P5(i)->t;
		P(i)->z=0;

		P7(i)->x=P5(i)->s;
		P7(i)->y=P5(i)->t;
		P7(i)->z=0;

	}

/*
	if(!m_sl)
		return ;
	CSystemCoord msc;
	m_sl->GetSystemCoord(&msc);
	CSystemCoord dmsc;
	m_sl->m_pDoc->GetSystemCoord(&dmsc);

	for(int i=0;i<m_num;i++){
		P7(i)->x=P5(i)->s;
		P7(i)->y=P5(i)->t;
		P7(i)->z=0;

		CPoint7d p;
		p.x=P5(i)->s;
		p.y=P5(i)->t;
		p.z=0;
		p.l=P7(i)->l;
		p.m=P7(i)->m;
		p.n=P7(i)->n;
		p.mod_coord_ma(&dmsc);
		p.mod_coord_am(&msc);
		P7(i)->l=p.l;
		P7(i)->m=p.m;
		P7(i)->n=p.n;

	}
	Update();
*/


}

bool CSpline::UpdatePoints()
{
	if(!m_sl)
		return BAD;
	CSystemCoord msc;
	m_sl->GetSystemCoord(&msc);
	CSystemCoord dmsc;
	m_sl->m_pDoc->GetSystemCoord(&dmsc);
	for(int i=0;i<m_num;i++){
		CPoint7d p;
		p.x=P5(i)->s;
		p.y=P5(i)->t;
		p.z=0;
		p.l=P7(i)->l;
		p.m=P7(i)->m;
		p.n=P7(i)->n;

		p.mod_coord_ma(&msc);
		p.mod_coord_am(&dmsc);
		
		P7(i)->x=p.x;
		P7(i)->y=p.y;
		P7(i)->z=p.z;
		P7(i)->l=p.l;
		P7(i)->m=p.m;
		P7(i)->n=p.n;

	}
	return 	Update();
}

void CSpline::UpdateST()
{
	if(!m_sl)
		return ;
	CSystemCoord msc;
	m_sl->GetSystemCoord(&msc);
	CSystemCoord dmsc;
	m_sl->m_pDoc->GetSystemCoord(&dmsc);
	for(int i=0;i<m_num;i++){
		CPoint3d p;
		p.x=P(i)->x;
		p.y=P(i)->y;
		p.z=P(i)->z;
		p.mod_coord_ma(&dmsc);
		p.mod_coord_am(&msc);
		P5(i)->s=p.x;
		P5(i)->t=p.y;
	}
}

BOOL CSpline::UpdateFillet(CLinkLine* line2, CFillet* fil, Coord rad)
{
	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) )){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	if(line2->IsKindOf( RUNTIME_CLASS( CBezierSpline ) )){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	if(line2->IsKindOf( RUNTIME_CLASS( CSpline ) )){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}

	CPoint3d p0=*PLast();
	CPoint3d px=*line2->PLast();
	CPoint3d py=*P(2);
	CVector c1,c2,c3;
	if(POINTs_SC(&p0, &px, &py, &c1, &c2, &c3)){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}

	LINE_2P line1(  line2->P(0), line2->PLast());
    line1.mod_coord_am(&p0, &c1, &c2, &c3);
	line1.Offset(rad);

	CSpline* cpline=MakeSpline();
	if(!cpline)
		return BAD;
    cpline->mod_coord_am(&p0, &c1, &c2, &c3);
	cpline->Offset(rad);
	CPoint3d* pc=NULL;
	int nc=cpline->CrossLine(&line1, &pc);
	delete cpline;
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
	line2->GetPointOrtho(&fil->m_pc, &fil->m_p2);
	double s=GetOrthPoint(&fil->m_pc);
	CPoint7d p7;

	GetPoint(s, &p7);
	fil->m_p1.x=p7.x;
	fil->m_p1.y=p7.y;
	fil->m_p1.z=p7.z;

	m_fillet2=line2->m_fillet1=1;
	fil->m_Ok=1;
	m_InitOutLine=0;
	return OK;
}

BOOL CSpline::DeleteFillet(CSmartLine* sl)
{
	if(!this)
		return BAD;
	m_fillet2=0;
	int index=sl->GetIndexLine(this);
	if(index<0)
		return BAD;
	if(index<sl->GetNumLines()-1)
		sl->Get_Link_by_index(index+1)->m_fillet1=0;
	else
		sl->Get_Link_by_index(0)->m_fillet1=0;
	m_InitOutLine=0;
	return OK;
}

void CSpline::GetPointOrtho(CPoint3d* p, CPoint3d* pc)
{
	if(!p)
		return;
	if(!pc)
		return;
	*pc=*p;

	double s = GetOrthPoint(p);
	CPoint7d pn;
	GetPoint(s, &pn);

	pc->x= pn.x;
	pc->y= pn.y;
	pc->z= pn.z;
}

void CSpline::GetPlaneSC(CSystemCoord* msc)
{
	double dist_max = 0;
	int i_max = 0;

	CPoint3d pm = *P(0);

	CPoint3d px = pm;
	CVector cx(P7(0)->l, P7(0)->m, P7(0)->n);
	px.Move(&cx, 100);

	for (int i = 0; i<np(); i++){
		double dist = P(i)->GetDistLine(&pm, &px);
		if (dist_max<dist){
			dist_max = dist;
			i_max = i;
		}
	}
	CPoint3d py = *P(i_max);

	if (dist_max<DDELTA){
		CVector cy;
		py = pm;
		cx.GetOrth(&cy);
		py.Move(&cy, 100);
	}

	msc->Set(&pm, &px, &py);
}

bool CSpline::GetPlaneSC(CSystemCoord* msc, double angle)
{
	double dist_max=0;
	int i_max=0;
	CPoint3d pm=*P(0);

	CLine* pline= MakeLine(0.02);
	if(!pline)
		return false;
	CPoint3d px=pm;

	for(int i=0;i< pline->np();i++){
		double dist=  pline->P(i)->DistTo(&pm);
		if(dist_max<dist){
			px = *pline->P(i);
			dist_max=dist;
		}
	}
	dist_max=0;
	CVector cx(&pm, &px);

	CPoint3d py;
	for(int i=0;i< pline->np();i++){
		double dist=  pline->P(i)->GetDistLine(&pm, &px);
		if(dist_max<dist){
			py = *pline->P(i);
			dist_max=dist;
		}
	}

	if(dist_max<DELTA){
		CVector cy;
		py=pm;
		cx.GetOrth(&cy);
		py.Move(&cy, 100);
	}

	msc->Set(&pm, &px, &py);
	pline->mod_coord_am(msc);

	CPoint3d p0;
	CPoint3d p0z(0,0,100);
	pline->RotateG(&p0, &p0z, angle);


	CSizeBlock size;
	pline->GetBoundaryBox(&size);
	CPoint3d p0x(size.X_min, 0, 0);
	p0x.mod_coord_ma(msc);

	msc->Set(&p0x, &px, &py);
	delete pline;
	return true;
}

bool CSpline::SetPlaneSC(CVector& cx, CVector& cy, bool InitSC, double angle, bool IsX0Center, bool IsChangeBeg)
{
	CSystemCoord msc; 
	if (!InitSC)
	{
		GetPlaneSC(&msc);
		cx.Set(msc.cx.l, msc.cx.m, msc.cx.n);
		cy.Set(msc.cy.l, msc.cy.m, msc.cy.n);
		return SetPlaneSC_G(msc, angle, IsX0Center, IsChangeBeg);

	}
	msc.p0 = *P(0);
	msc.cx = cx;
	msc.cy = cy;
	CVector cz(&msc.cx, &msc.cy);
	msc.cz = cz;

	return SetPlaneSC_G(msc, angle, IsX0Center, IsChangeBeg);

}

bool CSpline::SetPlaneSC_G(CSystemCoord& msc1, double angle, bool IsX0Center, bool IsChangeBeg)
{
	bool IsOnXY = true;
	double dopusk=0.02;
	for(int i=0;i < np(); i++)
		if( fabs(P(i)->z) >dopusk)
			IsOnXY = false;

	CLine* pline= MakeLine(0.02);
	if(!pline)
		return false;
	CPoint3d px(100,0,0);
	CPoint3d py(0, 100, 0);

//	CSystemCoord msc1;
//	pline->GetPlaneSC(&msc1);
	if(!IsOnXY)
	{
		pline->mod_coord_am(&msc1);
		mod_coord_am(&msc1);
	}
	CPoint3d pm;
	CVector vz(0,0,1);
	pline->GetCenterPoint(&pm);
	CPoint3d pmz=pm;
	pmz.Move(&vz, 100);

	pline->RotateG(&pm, &pmz, angle);
	Rotate(&pm, &pmz, angle);

	pline->GetCenterPoint(&pm);
	CPoint3d p0;
	pline->Move(&pm, &p0);
	Move(&pm, &p0);


	CAlfaDoc* pDoc = GetAlfaDoc();
//	pDoc->AddFigure(new CLineFig (pline));


	CSizeBlock size;
	pline->GetBoundaryBox(&size);

	CVector cx(1,0,0);
	if(IsX0Center)
		Move(&cx, -size.X_min);

	CVector cy(0, 1,0);
	double dy =  size.Y_min + (size.Y_max-size.Y_min)/2.0;
	Move(&cy, -dy);

	if(!pline->IsClock())
		Revers();
//	pDoc->AddFigure(new CLineFig (pline));

	if(IsChangeBeg && IsClosed())
	{
		CPoint3d p1c(0, size.Y_min-10, 0);
		CPoint3d p2c (0, size.Y_max+10, 0);
		LINE_2P line2(&p1c, &p2c) ;
		CPoint3d* pc=NULL;
		int nc = pline->CrossLine(&line2, &pc);
		if(!nc)
			return true;
		CPoint3d pcdwn;
		double y_min=1e15;
		for(int i=0;  i< nc; i++)
		{
			if(y_min > pc[i].y)
			{
				pcdwn=pc[i];
				y_min = pc[i].y;
			}
		}
		InsertKnot(&pcdwn);
		ChangeBegin(&pcdwn);
	}
	delete pline;
	return true;
}



bool CSpline::SetPlaneSC(double angle, bool IsX0Center, bool IsChangeBeg)
{
	CSystemCoord msc;
	GetPlaneSC(&msc);

	SetPlaneSC_G(msc, angle,  IsX0Center,  IsChangeBeg );
	return true;
}

bool CSpline::MoveBeginPoint()
{

	CLine* pline= MakeLine(0.02);
	if(!pline)
		return false;
	if(IsClosed())
		if(!pline->IsClock())
			Revers();

	CSizeBlock size1;
	pline->GetBoundaryBox(&size1);

	CVector cx(1,0,0);
	Move(&cx, -size1.X_min);
	pline->Move(&cx, -size1.X_min);


	if(!IsClosed())
		return true;

	CPoint3d pm;
	pline->GetCenterPoint(&pm);
	CPoint3d pmz=pm;
	CVector vz(0,0,1);
	pmz.Move(&vz, 100);
	CPoint3d p0;
	pline->Move(&pm, &p0);
	Move(&pm, &p0);

	CSizeBlock size;
	pline->GetBoundaryBox(&size);

	CPoint3d p1c(0, size.Y_min-10, 0);
	CPoint3d p2c (0, size.Y_max+10, 0);
	LINE_2P line2(&p1c, &p2c) ;
	CPoint3d* pc=NULL;
	int nc = pline->CrossLine(&line2, &pc);
	if(!nc)
		return true;
	CPoint3d pcdwn;
	double y_min=1e15;
	for(int i=0;  i< nc; i++)
	{
		if(y_min > pc[i].y)
		{
			pcdwn=pc[i];
			y_min = pc[i].y;
		}
	}
	InsertKnot(&pcdwn);
	ChangeBegin(&pcdwn);
	delete pline;
	Move(&p0, &pm);
	return true;
}


bool CSpline::IsClosed()
{
	if(m_n<3)
		return 0;
	if(dist_POINT(&P(0)->x, &PLast()->x)< DELTA)
		return 1;
	return 0;
}


BOOL CSpline::ChangeBegin(CPoint3d* Pb)
{
	if(!IsClosed()){
		Message_inf(IDS_NO_CHANGE_BEGIN);
		return BAD;
	}

	int i_min=0;
	FindPoint(Pb, &i_min);
	if(i_min==0 || i_min==m_n-1)
		return OK;
	CSpline ln_tmp;
	ln_tmp.Alloc(i_min+1);
	if(ln_tmp.m_n==0)
		return BAD;
    for(int i=0;i<=i_min;i++)
		ln_tmp.m_p7[i]=m_p7[i];

	int i=0;
    for( int  j=i_min; j<m_n; i++,j++)

    for(int j=1;j<ln_tmp.m_n;i++,j++)
    return Update();
}

bool CSpline::InsertKnot(CPoint3d* p)
{
	double s=GetOrthPoint(p);
	if(InsertPoint(s))
	{
/*CAlfaDoc* pDoc= GetAlfaDoc();
pDoc->AddFigure(new CCubSpline (P7(0), np()));
::AddPoint(p, RGB_RED);

	CPoint7d p2;
    GetPoint(s, &p2);
::AddPoint((CPoint3d*)&p2, RGB_GREEN);
*/

		return false;
	}
	return true;
}

double CSpline::GetDistMin(CPoint3d* p)
{
	if(!p)
		return 1e15;
	CPoint3d pc;
	GetPointOrtho(p, &pc);
	return pc.DistTo(p);
}


double CSpline::IterationPm(CPoint3d* p1, CPoint3d* p2, double dist1)
{
	double dopusk =0.01;
	CPoint7d p7;
	double s1 = GetOrthPoint(p1);
	CPoint3d pm =*p1; 

	while(p1->DistTo(p2)> dopusk)
	{
		pm =*p1; 
		CVector dir(p1, p2);
		pm.Move(&dir, p1->DistTo(p2)/2.0);
		s1 = GetOrthPoint( &pm);

		BOOL pr1= GetPoint( s1, &p7);
		double dist2= p7.DistTo(&pm);
		if(dist2< dist1)
		{
			*p2= *p1;
			*p1 = pm;
			dist1 = dist2;
		}
		else
			*p2 = pm;
	}
	p1->x = p7.x;
	p1->y = p7.y;
	p1->z = p7.z;
	*p2=pm;
	return s1;
}

bool CSpline::Intersection(CSpline* kr2, double* p1, double* p2)
{
	double delta=0.01;
	CLine* pline= kr2->MakeLine(delta);
	if(!pline)
		return false;
	double dist_min=1e15;
	int i_min=0;
	for(int i=0;i< pline->np(); i++)
	{
		double s= GetOrthPoint( pline->P(i));
		CPoint7d p7;
		BOOL pr1= GetPoint( s, &p7);
		double dist= pline->P(i)->DistTo((CPoint3d*) &p7);
		if(dist < dist_min)
		{
			dist_min = dist;
			i_min=i;
		}
	}

	CPoint3d p1i;
	CPoint3d p2i;
	p1i = *pline->P(i_min);
	int i2_min= i_min+1;
	if(i_min == pline->np()-1)
		i2_min= i_min-1;
	p2i = *pline->P(i2_min);

	*p1 = IterationPm(&p1i, &p2i,  dist_min);
	*p2 = kr2->GetOrthPoint(&p2i);

	delete pline;
	return true;
}

Coord CSpline::GetLengthByP(Coord p)
{
	if(np()<1)
		return 0;
	if(p<0)
		return p*P7(1)->s;
	if(p>=np()-1)
		return P7Last()->s;

	int i=(int)floor(p);
	Coord ds=p-i;

	return P7(i)->s+(P7(i+1)->s-P7(i)->s)*ds;
}

void CSpline::MakeShear(CPoint3d* p0, CPoint3d* px, CPoint3d* py,  double dy0)
{
	CSystemCoord msk;
	msk.Set(p0, px, py);
	px->mod_coord_am(&msk);
	py->mod_coord_am(&msk);
	if( fabs(py->x) < DDELTA)
		return;

	mod_coord_am(&msk);
	double dy = dy0/py->x;
	for(int i=0; i< m_n; i++)
	{
		if(P(i)->x>0 && P(i)->x <= px->x)
		{
			P(i)->y = P(i)->y+ P(i)->x*dy;
			P5(i)->y = P5(i)->y + P5(i)->x*dy;
		}
	}
	px->mod_coord_ma(&msk);
	py->mod_coord_ma(&msk);
	mod_coord_ma(&msk);

	UpdateST();
	Build();
}

int CSpline::Trimming(CPlane *pl,CPoint3d *pk)
{

	register int i,nn=0;
	CPoint7d p0;
	CPoint7d* pt=NULL;
	int num_p=0;
	double	delta=pk->x*pl->a+pk->y*pl->b+pk->z*pl->c+pl->d;
	if(fabs(delta)<DDELTA){
		message_error_("Control point on the plat!!!");
		return BAD;
		}
	double	de2=0;
    for(i=0;i<np()-1;i++){	/***********  Main cycle *********************/

		double	de1=P(i)->x*pl->a+P(i)->y*pl->b+P(i)->z*pl->c+pl->d;
		de2=P(i+1)->x*pl->a+P(i+1)->y*pl->b+P(i+1)->z*pl->c+pl->d;
	if( (de1>0&&de2<0) || (de1<0&&de2>0) ){		/*	crossing	*/
	    if( fabs(de1)< DDELTA || fabs(de2)< DDELTA ){ 	/* plat follow point */
		if(fabs(de1)< DDELTA){
		    pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
		    if(pt==NULL){
				message_error_(IDS_BAD_ALLOC_MEMORY);
				return BAD;
			}
		    pt[nn++]=*P7(i);
		}
		else{
		    if( (de1>0&&delta>0) || (de1<0&&delta<0) ){
			pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
			if(pt==NULL){
			    message_error_(IDS_BAD_ALLOC_MEMORY);
			    return BAD;
			    }
			pt[nn++]=*P7(i);
			}
		}
		}			
	    else{		/* plat no follow point */
		if( (de1>0&&delta>0) || (de1<0&&delta<0) ){
		    pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
		    if(pt==NULL){
			message_error_(IDS_BAD_ALLOC_MEMORY);
			return BAD;
			}
		    pt[nn++]=*P7(i);
		    }
		Localized_point(i,1,pl, &p0);
		pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
		if(pt==NULL){
		    message_error_(IDS_BAD_ALLOC_MEMORY);
		    return BAD;
		    }
		pt[nn++]=p0;
		}
	    }			
	else				/*	no crossing	*/
	    if( (de1>0&&delta>0) || (de1<0&&delta<0) ){	/*	1-st point	*/
			pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
			if(pt==NULL){
				message_error_(IDS_BAD_ALLOC_MEMORY);
				return BAD;
				}
			pt[nn++]=*P7(i);
		}
	    else
		if(fabs(de1)< DDELTA){         	/* plat follow 1-st point */
		    pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
		    if(pt==NULL){
			message_error_(IDS_BAD_ALLOC_MEMORY);
			return BAD;
			}
		    pt[nn++]=*P7(i);
		    }
	}
/*********** end main cycle *********************/
	
    if( (de2>0&&delta>0) || (de2<0&&delta<0) ){	/*	2-nd point	*/
		pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
		pt[nn++]=*P7(i);
	}
    else
	if(fabs(de2)< DDELTA){         	/* plat follow 2-nd point */
	    pt=(CPoint7d *)realloc((char*)pt,(++num_p)*sizeof(CPoint7d));
	    if(pt==NULL){
		message_error_(IDS_BAD_ALLOC_MEMORY);
		return BAD;
		}
	    pt[nn++]=*P7(i);
	    }
    if(nn<2){
		message_error_("num point <2!");
		return BAD;
	}
	if(Realloc(num_p))
		return BAD;
 
	for(i=0;i<num_p;i++)
		*P7(i)=pt[i];
	free(pt);
    return Update();
}



bool CSpline::MakePolyLine(std::vector<CPoint3d>& pnts, float tolerance)
{
	for (int i = 0; i<m_n; i++){
		CPoint3d pnt(P(i)->x, P(i)->y, P(i)->z);
		if (pnts.size())
		{
			if (i == 0)
			{
				double distPP = pnts[pnts.size()-1].DistTo(&pnt);
				if (distPP > DDELTA)
					pnts.push_back(pnt);
			}
			else
				pnts.push_back(pnt);
		}
		else
			pnts.push_back(pnt);
	}
	int np = -1;
	CPoint3d pm;
	int pr;
	for (int j = 0; j<m_n - 1; j++){
		double ds = 1;
		np++;
		if (ConrolDelta(j, tolerance, ds, &pr))
			return false;
		while (pr != OK && ds>0.002){
			ds = ds / 2.0;
			if (ConrolDelta(j, tolerance, ds, &pr))
				return false;
		}
		int n = (short)ceil(1 / ds);
		for (int i = 1; i<n; i++){
			if (!GetPoint(j + ds*i, &pm))
				return false;
			CPoint3d p(pm.x, pm.y, pm.z);
			pnts.insert(pnts.begin() + np + 1, p);
			np++;
		}
	}
	return true;
}

bool CSpline::GetPlaneSC(CTypedPtrArray<CObArray, CSpline*>& splines, CSystemCoord* msc)
{
	if (splines.GetCount()==0)
		return false;
	
	double dist_max = 0;
	int i_max = 0;
	CPoint3d pm = *splines[0]->P(0);
	std::vector<CPoint3d> pnts;
	float tolerance = 0.02;
	for (int j = 0; j < splines.GetCount(); j++){
		splines[j]->MakePolyLine(pnts, tolerance);
	}
	CPoint3d px = pm;
	CPoint3d p3d0(pm.x, pm.y, pm.z);
	CPoint3d p3dx = p3d0;
	for (int i = 0; i< pnts.size(); i++){
		double dist = pnts[i].DistTo(&p3d0);
		if (dist_max<dist){
			p3dx = pnts[i];
			dist_max = dist;
		}
	}
	dist_max = 0;
	px.x = p3dx.x;
	px.y = p3dx.y;
	px.z = p3dx.z;

	CVector cx(&pm, &px);

	CPoint3d py;
	for (int i = 0; i< pnts.size(); i++){
		CPoint3d pi(pnts[i].x, pnts[i].y,  pnts[i].z);
		double dist = pi.GetDistLine(&pm, &px);
		if (dist_max<dist){
			py = pnts[i];
			dist_max = dist;
		}
	}
	CPoint3d py2(py.x, py.y, py.z);

	if (dist_max<DDELTA){
		CVector cy;
		py2 = pm;
		cx.GetOrth(&cy);
		py2.Move(&cy, 100);
	}
	msc->Set(&pm, &px, &py2);
	return true;
}


bool CSpline::SetPlaneSC(CTypedPtrArray<CObArray, CSpline*>& splines, double angle)
{
	CSystemCoord msc;
	if (!GetPlaneSC(splines, &msc))
		return false;

	bool IsOnXY = true;
	for (int j = 0; j < splines.GetCount(); j++){
		for (int i = 0; i < splines[j]->np(); i++)
			if (fabs(splines[j]->P(i)->z) >DDELTA)
				IsOnXY = false;
	}

	std::vector<CPoint3d> pnts;
	double tolerance  = 0.02;
// if Curve is not on XY put it on Plane XY
	for (int j = 0; j < splines.GetCount(); j++){
		if (!IsOnXY)
			splines[j]->mod_coord_am(&msc);
		splines[j]->MakePolyLine(pnts, tolerance);
	}

	CSizeBlock bbox;
	for (int i = 0; i < pnts.size(); i++)
		bbox.AddPoint(&pnts[i]);
	CPoint3d pc =	bbox.m_pc;

	CPoint3d pm(pc.x, pc.y, pc.z);
	CPoint3d pmz = pm;
	CVector vz(0, 0, 1);
	pmz.Move(&vz, 100);
	for (int j = 0; j < splines.GetCount(); j++)
		splines[j]->Rotate(&pm, &pmz, angle);

	std::vector<CPoint3d> pnts2;

	for (int j = 0; j < splines.GetCount(); j++)
		splines[j]->MakePolyLine(pnts2, tolerance);


	bbox.SetEmpty();
	for (int i = 0; i < pnts2.size(); i++)
		bbox.AddPoint(&pnts2[i]);

	CVector cx(1, 0, 0);
	for (int j = 0; j < splines.GetCount(); j++)
		splines[j]->Move(&cx, - bbox.GetMin().x);

	CVector cy(0, 1, 0);
	double dy = bbox.GetMin().y + (bbox.GetMax().y - bbox.GetMin().y) / 2.0;
	for (int j = 0; j < splines.GetCount(); j++)
		splines[j]->Move(&cy, -dy);



	return true;
}

void CSpline::RotateByTwoVector(CPoint3d* p0, CVector* v1, CVector* v2)
{
	CVector vy;
	double alfa1=vy.GetAngleRotateVector(v1, v2);	
	CPoint3d px1;
	p0->Shift(&vy, 100, &px1);
	Rotate(p0, &px1, alfa1);
}


void CSpline::AddKnot(CPoint3d* pnt)
{
	if(pnt)
		m_Knots.push_back(*pnt);
}

CPoint3d CSpline::GetKnot(int ind)
{
	CPoint3d p0;
	if(ind < 0 || ind > m_Knots.size())
		return p0;
	return m_Knots[ind];
}


double CSpline::GetParam(int ind)
{
	if(ind < 0 || ind > m_Params.size())
		return 0;
	return m_Params[ind];
}

bool CSpline::ControlDirections(CSpline* spl2)
{
	double dopusk = 0.2;
	CLine* pline1 = MakeLine(dopusk);
	if (!pline1)
		return false;
	CLine* pline2 = spl2->MakeLine(dopusk);
	if (!pline2)
		return false;
	pline1->SetPointsExampleSL(pline2);
	double dist_summ = 0;
	for (int i = 0; i < pline1->np(); i++){

		double dist = pline1->P(i)->DistTo(pline2->P(i));
		dist_summ += dist;
	}
	pline2->Revers();
	double dist_summ2 = 0;
	for (int i = 0; i < pline1->np(); i++){

		double dist = pline1->P(i)->DistTo(pline2->P(i));
		dist_summ2 += dist;
	}
	if (dist_summ2 < dist_summ)
		spl2->Revers();
	return true;
}

bool CSpline::ChangeBeginByTemplate(CSpline* example)
{
	if (!IsClosed() || !example->IsClosed())
		return true;
	CLine* pline1 = MakeLine(0.02);
	if (!pline1)
		return false;
	CLine* pline2 = example->MakeLine(0.02);
	if (!pline2)
		return false;
	pline1->SetPointsExampleSL(pline2);
	double SumMin = 1e15;
	CPoint3d pMin;

	//	CAlfaDoc* pDoc = GetCAlfaDoc();
	//	CLayer* llMin=NULL;
	for (int dd = 0; dd<pline1->np(); dd++)
	{
		//		CString nameL;
		//		nameL.Format("dd=%d", dd);
		//	CLayer* layer = pDoc->AddLayer(nameL);
		double Sum = 0;
		for (int i = 0; i<pline1->np(); i++)
		{
			int k = i + dd;
			if (k> pline1->np())
				k = k - pline1->np();
			Sum += pline1->P(i)->DistTo(pline2->P(k));
		}
		if (Sum < SumMin)
		{
			SumMin = Sum;
			pMin = *pline1->P(dd);
			//		llMin = layer;
		}

	}
	//llMin->m_name="SumMin";
	delete pline1;
	delete pline2;

	//	AddFigure(new CCubSpline (P7(0), np()), RGB_BLUE);
	InsertKnot(&pMin);
	ChangeBegin(&pMin);
	//	AddFigure(new CCubSpline (P7(0), np()), RGB_RED);
	return true;
}

bool CSpline::ReParametrizationByTemplate(CSpline* example)
{
	if (IsClosed() && example->IsClosed())
		return ReParametrizationByClosedTemplate(example);

	CLine* pline1 = MakeLine(0.2);
	if (!pline1)
		return false;
	CLine* pline2 = example->MakeLine(0.2);
	if (!pline2)
		return false;
	pline1->SetPointsExampleSL(pline2);

	double summ1 = 0;
	for (int j = 0, i = 0; j < pline1->np(); j++, i++){
		if (i < pline2->np()){
			double dist = pline1->P(j)->DistTo(pline2->P(i));
			summ1 += dist;
		}
	}
	pline1->Revers();
	double summ2 = 0;
	for (int j = 0, i = 0; j < pline1->np(); j++, i++){
		if (i < pline2->np()){
			double dist = pline1->P(j)->DistTo(pline2->P(i));
			summ2 += dist;
		}
	}
	if (summ1 > summ2)
		Revers();
	delete pline1;
	delete pline2;
	SetPoints(example);
	return true;
}


bool CSpline::ReParametrizationByClosedTemplate(CSpline* example)
{
	if (!IsClosed() || !example->IsClosed())
		return true;
	double delta = 3.5;
	CLine* pLinThis = MakeLine(delta);
	if (!pLinThis)
		return false;
	CLine* pExample = example->MakeLine(delta);
	if (!pExample)
		return false;
	pLinThis->SetPointsExampleSL(pExample);

	double SumMin1 = 1e15;
	CPoint3d pMin;
//	::AddPoint(pExample->P(0), RGB_GREEN);
	for (int dd = 0; dd<pExample->np()-1; dd++)
	{
		double Sum = 0;
		for (int i = 0; i<pLinThis->np(); i++)
		{
			int k = i + dd;
			if (k >= pLinThis->np())
				k = k - pLinThis->np();
			Sum += pLinThis->P(k)->DistTo(pExample->P(i));
		}
		if (Sum < SumMin1)
		{
			SumMin1 = Sum;
			pMin = *pLinThis->P(dd);
		}
	}

//	::AddPoint(&pMin, RGB_RED);
	double SumMin2 = 1e15;
	pLinThis->Revers();
	CPoint3d pMin2;
	for (int dd = 0; dd<pExample->np() - 1; dd++)
	{
		double Sum = 0;
		for (int i = 0; i<pLinThis->np(); i++)
		{
			int k = i + dd;
			if (k >= pLinThis->np())
				k = k - pLinThis->np();
			Sum += pLinThis->P(k)->DistTo(pExample->P(i));
		}
		if (Sum < SumMin2)
		{
			SumMin2 = Sum;
			pMin2 = *pLinThis->P(dd);
		}
	}

	if (SumMin1 < SumMin2){
		InsertKnot(&pMin);
		ChangeBegin(&pMin);
	}
	else{
		Revers();
		InsertKnot(&pMin2);
		ChangeBegin(&pMin2);
	}

	delete pLinThis;
	delete pExample;

//	::AddPoint(&pMin, RGB_RED);

	GetPointOrtho(example->P(0), &pMin);
	InsertKnot(&pMin);
	ChangeBegin(&pMin);


	SetPoints(example);

	return true;
}

bool CSpline::ChangeBeginByCross(CSpline* example)
{
	if (!IsClosed())
		return false;

	if (!example)
		return false;
	double p1;
	double p2;
	Intersection(example, &p1, &p2);
	CPoint7d p7;
	GetPoint(p1, &p7);

	CPoint3d pMin(p7.x, p7.y, p7.z);
	InsertKnot(&pMin);
	ChangeBegin(&pMin);

	return true;
}

int CSpline::FindNearestPoint(CPoint3d* p)
{
	double dist_min = 1e15;
	int i_min = 0;
	for (int i = 0; i < np(); i++){
		double dist = p->DistTo(P(i));
		if (dist_min > dist){
			dist_min = dist;
			i_min = i;
		}
	}
	return i_min;
}

CSpline* CSpline::SimplifyByCross(CSpline* splCr)
{
	double p1;
	double p2;
	Intersection(splCr, &p1, &p2);
	CPoint7d p7;
	GetPoint(p1, &p7);
	CPoint3d pMin(p7.x, p7.y, p7.z);
	InsertKnot(&pMin);
	int num_p = FindNearestPoint(&pMin);
	return Simplify(num_p);
}