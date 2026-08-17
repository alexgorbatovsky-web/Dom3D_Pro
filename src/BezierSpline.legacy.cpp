// BezierSpline.cpp: implementation of the CBezierSpline class.
//
//////////////////////////////////////////////////////////////////////

#include "BezierSpline.h"
#include "View3d.h"
#include "CAlfaDoc.h"
#include "Spline.h"
#include "SmartLine.h"
#include "Line_2P.h"
#include "Circle3D.h"
#include "Arc.h"



#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBezierSpline::CBezierSpline()
{
	if(Alloc(4))
		return;

}

CBezierSpline::CBezierSpline(CPoint3d* p0, CPoint3d* p1, CPoint3d* p2, CPoint3d* p3)
{
	if(Alloc(4))
		return;

	*P(0)=*p0;
	*P(1)=*p1;
	*P(2)=*p2;
	*P(3)=*p3;
	for(int i=0;i<m_num;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
}

CBezierSpline::CBezierSpline(double x1, double y1,double x2, double y2,double x3, double y3,double x4, double y4)
{
	if(Alloc(4))
		return;

	P(0)->x=x1;
	P(0)->y=y1;

	P(1)->x=x2;
	P(1)->y=y2;

	P(2)->x=x3;
	P(2)->y=y3;

	P(3)->x=x4;
	P(3)->y=y4;

	for(int i=0;i<m_num;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
}

CBezierSpline::~CBezierSpline()
{

}

void Set_Color(unsigned long col);
void CBezierSpline::Draw(CView3d * pview)
{
if(this==NULL)
    return;

	m_dir.calc(P(0), P(1));
	register double  ds=0.05;
	CPoint3d p1;
	CPoint3d p2;
	CVector vect;
    GetPointVector(0.25, &p1, &vect);	
    GetPoint(0.25+ds, &p2);
	register double delta=dist_POINT_LINE(&p2.x, &p1.x, &vect.l);
    while(delta>pview->m_delta && ds>0.000001){
		ds=ds/2;
		GetPoint(0.25+ds, &p2);
		delta=dist_POINT_LINE(&p2.x, &p1.x, &vect.l);
		}
    if(m_selected)
		Set_Color(~m_col);
    else
		Set_Color(m_col);
	double Sb=0;
	double Se=1;
	if(m_fillet1){
		CPoint3d* Pb=m_sl->P1(this);
		Sb=GetOrthPoint(Pb);
	}
	if(m_fillet2){
		CPoint3d* Pe=m_sl->P2(this);
		Se=GetOrthPoint(Pe);
	}
	double Sb_Se=Se-Sb;
	int	np=(short)(Sb_Se/ds);	/*	number points	*/
	glBegin(GL_LINE_STRIP);
	for(int i=0;i<=np;i++){
		GetPoint(Sb+ds*i, &p1);	
		glVertex3d(p1.x, p1.y, p1.z);
	}
	if(m_fillet1 || m_fillet2){
		GetPoint(Se, &p1);	
		glVertex3d(p1.x, p1.y, p1.z);
	}
	glEnd();

	bool Edited=false;
	if(m_sl)
	{
		Edited = m_sl->m_IsEditing;
	}
	if(Edited || pview->draw_knots){
		glLineWidth((float)1);
		Set_Color(RGB_CURVE2P);
		glBegin(GL_LINES);
			glVertex3d(P(0)->x, P(0)->y, P(0)->z);
			glVertex3d(P(1)->x, P(1)->y, P(1)->z);
		glEnd();
	
		glBegin(GL_LINES);
			glVertex3d(P(2)->x, P(2)->y, P(2)->z);
			glVertex3d(P(3)->x, P(3)->y, P(3)->z);
		glEnd();

		pview->DrawKnot(P(0), RGB_POINT);
		pview->DrawKnot(P(1), RGB_POINT);
		pview->DrawKnot(P(2), RGB_POINT);
		pview->DrawKnot(P(3), RGB_POINT);
	}
}


void CBezierSpline::GetPointVector(double s,CPoint3d *p, CVector* v)
{
double delta=0.00001;

    GetPoint(s, p);

    if(s<delta){
		v->calc(P(0), P(1));
	    return;
	}
    if((1-s)<delta){
		v->calc(P(2), P(3));
	    return ;
	}
CPoint3d p2;
    GetPoint(s+delta,&p2);
	v->calc(p, &p2);
}

void CBezierSpline::GetPoint(double t, CPoint3d * pt)
{
//	P= (1-t)*(1-t)*(1-t)*P1+3*t*(1-t)*(1-t)*P2+3*t*t*(1-t)*P3+t*t*t*P4.
	pt->x=pow((1-t),3)*P(0)->x+3*t*pow((1-t),2)*P(1)->x+3*t*t*(1-t)*P(2)->x+t*t*t*P(3)->x;
	pt->y=pow((1-t),3)*P(0)->y+3*t*pow((1-t),2)*P(1)->y+3*t*t*(1-t)*P(2)->y+t*t*t*P(3)->y;
	pt->z=pow((1-t),3)*P(0)->z+3*t*pow((1-t),2)*P(1)->z+3*t*t*(1-t)*P(2)->z+t*t*t*P(3)->z;
}

CLinkLine* CBezierSpline::InsertPoint(CPoint3d* pc)
{
	double t=GetOrthPoint(pc);

	GetPoint(t, pc);
	CPoint3d p2;
	CPoint3d p3;
	CPoint3d p4;
	CPoint3d p5;
	CPoint3d p6;
	
	p2.x=P(0)->x+(P(1)->x-P(0)->x)*t;
	p2.y=P(0)->y+(P(1)->y-P(0)->y)*t;
	p2.z=P(0)->z+(P(1)->z-P(0)->z)*t;

	CPoint3d Pp;
	Pp.x=-3*(1-t)*(1-t)*P(0)->x+3*(1-t)*(1-3*t)*P(1)->x+3*t*(2-3*t)*P(2)->x+3*t*t*P(3)->x;
	Pp.y=-3*(1-t)*(1-t)*P(0)->y+3*(1-t)*(1-3*t)*P(1)->y+3*t*(2-3*t)*P(2)->y+3*t*t*P(3)->y;
	Pp.z=-3*(1-t)*(1-t)*P(0)->z+3*(1-t)*(1-3*t)*P(1)->z+3*t*(2-3*t)*P(2)->z+3*t*t*P(3)->z;

	p3.x=pc->x-Pp.x*t/3;
	p3.y=pc->y-Pp.y*t/3;
	p3.z=pc->z-Pp.z*t/3;

//	P4=*pc;
	p5.x=pc->x+Pp.x*(1-t)/3;
	p5.y=pc->y+Pp.y*(1-t)/3;
	p5.z=pc->z+Pp.z*(1-t)/3;

	p6.x=P(3)->x+(P(2)->x-P(3)->x)*(1-t);
	p6.y=P(3)->y+(P(2)->y-P(3)->y)*(1-t);
	p6.z=P(3)->z+(P(2)->z-P(3)->z)*(1-t);
	
	CBezierSpline* line2=new CBezierSpline(pc, &p5, &p6, P(3));

	
	*P(1)=p2;	
	*P(2)=p3;	
	*P(3)=*pc;	
	for(int i=0;i<m_num;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
	
	return line2;
}

CPolyline* CBezierSpline::MakeLine(double dopusk)
{
	register double  ds=0.2;
	CPoint3d p1;
	CPoint3d p2;
	CVector vect;
    GetPointVector(0.25, &p1, &vect);
    GetPoint(0.25+ds, &p2);
	register double delta=dist_POINT_LINE(&p2.x, &p1.x, &vect.l);
    while(delta>dopusk && ds>0.00001){
		ds=ds/2;
		GetPoint(0.25+ds, &p2);
		delta=dist_POINT_LINE(&p2.x, &p1.x, &vect.l);
		}
	double Sb=0;
	double Se=1;
	if(m_fillet1){
		CPoint3d* Pb=m_sl->P1(this);
		Sb=GetOrthPoint(Pb);
	}
	if(m_fillet2){
		CPoint3d* Pe=m_sl->P2(this);
		Se=GetOrthPoint(Pe);
	}
	double Sb_Se=Se-Sb;
	int	np=(short)(Sb_Se/ds);	/*	number points	*/
	
	CPolyline* line=new CPolyline(np+1);
	if(!line)
		return NULL;
	for (register int i=0;i<=np;i++){
		GetPoint(Sb+ds*i, line->P(i));
	}
	if(m_fillet1 || m_fillet2){
		CPoint3d P;
		GetPoint(Se, &P);
		line->AddPoint(&P);
	}
	line->ControlPoints(0.01);
	return line;
}

double CBezierSpline::Get_dist_Min(CPoint3d* pm, CView3d* view)
{
if(this==NULL)
    return 1e15;
	double delta=view->m_delta/3.0;
	CPolyline* line=MakeLine(delta);
	double dist_min=line->Get_dist_Min(pm, view);
	delete line;

	bool Edited=false;
	if(m_sl)
	{
		Edited = m_sl->m_IsEditing;
	}


	if(!Edited && !view->draw_knots)
		return dist_min;
	CPoint3d pgr1;
	CPoint3d pgr2;
	
	P(0)->GetGrPoz(view, &pgr1);
	P(1)->GetGrPoz(view, &pgr2);
	double dist=get_dist_XY_line2D(pm->x, pm->y, pgr1.x, pgr1.y, pgr2.x, pgr2.y);
	if(dist_min>dist)
		dist_min=dist;
	P(2)->GetGrPoz(view, &pgr1);
	P(3)->GetGrPoz(view, &pgr2);
	dist=get_dist_XY_line2D(pm->x, pm->y, pgr1.x, pgr1.y, pgr2.x, pgr2.y);
	if(dist_min>dist)
		dist_min=dist;
	
	return dist_min;
}


double CBezierSpline::FindPoint(CPoint3d* pm, CView3d* view, int* i_min)
{
double dist_min=1e15;
CPoint3d pgr;

	P(0)->GetGrPoz(view, &pgr);
	double dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
	if(dist_min>dist){
		dist_min=dist;
		*i_min=0;
		}
	P(3)->GetGrPoz(view, &pgr);
	dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
	if(dist_min>dist){
		dist_min=dist;
		*i_min=3;
		}

	bool Edited=false;
	if(m_sl)
	{
		Edited = m_sl->m_IsEditing;
	}

	if(!Edited && !view->draw_knots)
		return dist_min;

	P(1)->GetGrPoz(view, &pgr);
	dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
	if(dist_min>dist){
		dist_min=dist;
		*i_min=1;
		}
	
	P(2)->GetGrPoz(view, &pgr);
	dist=dist_XY_XY(pm->x, pm->y, pgr.x, pgr.y);
	if(dist_min>dist){
		dist_min=dist;
		*i_min=2;
		}
	return dist_min;
}

BOOL CBezierSpline::EditPoint(int np, CPoint3d* p, int constr)
{
	if(np>=m_num){
		Message_err("np>=m_num!");
		return BAD;
		}
	CVector dir(P(np), p);
	double dist=dist_POINT(P(np), p);
	*P(np)=*p;
	switch (np) {
	case 0:
		P(1)->Move(&dir, dist);
		break;
	case 1:
		if(m_sl->IsNodeSmooth(this, 0)){
			UpdateNodeSmooth(0,0);
		}
		break;
	case 2:
		if(m_sl->IsNodeSmooth(this, 3)){
			UpdateNodeSmooth(3,0);
		}
		break;

	case 3:
		P(2)->Move(&dir, dist);
		break;
	}
	for(int i=0;i<m_num;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
	
	if(np==0){
		CLinkLine* link_pr=m_sl->GetPreviousLink(this);
		if(link_pr)
			if(link_pr->EditLastPoint(P(0), constr))
				return BAD;
	}
	if(np==3){
		CLinkLine* link_next=m_sl->GetNextLink(this);
		if(link_next)
			if(link_next->EditFirstPoint( P(3)))
				return BAD;
	}		
	return OK;
}

BOOL CBezierSpline::EditLastPoint(CPoint3d* p, int constr)
{
	*P(3)=*p;
	P5(3)->s=p->x;
	P5(3)->t=p->y;
	return OK;
}

BOOL CBezierSpline::EditFirstPoint(CPoint3d* p)
{
	CVector dir(P(0), p);
	double dist=dist_POINT(P(0), p);
	*P(0)=*p;
	P5(0)->s=p->x;
	P5(0)->t=p->y;
	P(1)->Move(&dir, dist);
	for(int i=0;i<m_num;i++){
		P5(i)->s=P(i)->x;
		P5(i)->t=P(i)->y;
	}
	return OK;
}

void CBezierSpline::print(FILE* strm)
{
	int file_null=0;
	if(!strm){
		fopen_s(&strm, "c:\\stdout.txt","a+");
		file_null=1;
	}
    if(strm==NULL)
		return ;

	CString str;
	str.LoadString(IDS_BEZIER_CURVE);
    fprintf(strm, "%s.\n",str);
    fprintf(strm, "ID=%d\n",m_ID);

	if(file_null)
		fclose(strm);
}


int CBezierSpline::MakeKnot(CLinkLine* line2)
{
	if(!line2)
		return BAD;

	P(3)->x=line2->P(0)->x;
	P(3)->y=line2->P(0)->y;

	P5(3)->s=P(3)->x;
	P5(3)->t=P(3)->y;
	
	return OK;
}


int CBezierSpline::MakeLastKnot()
{
	CLinkLine* line2=m_sl->GetLastLine();
	if(!line2)
		return BAD;

	return  line2->EditLastPoint(P(0), 0);

}

BOOL CBezierSpline::Offset(Coord offset)
{

	Message_err(IDS_IMPOSIBLE_GEOMETRY);
	return BAD;
	CPoint3d ptm=*P(0);
	P(0)->Offset(offset, P(0), P(1));
	P(1)->Offset(offset, &ptm, P(1));

	ptm=*P(2);

	P(2)->Offset(offset, &ptm, P(3));
	P(3)->Offset(-offset, P(3), &ptm);
	return OK;
}

Coord CBezierSpline::GetOrthPoint(CPoint3d* p)
{
Coord s_min=0;
int ns=10;
Coord ds=1/(Coord)ns;
CPoint3d pn;
double dist_min=1e15;
double dist;

	for(int i=0;i<ns;i++){
		GetPoint(i*ds, &pn);
		dist=dist_POINT(&pn.x, &p->x);
		if(dist_min>dist){
			dist_min=dist;
			s_min=i*ds;
		}
	}
	int n_iter=6;

    ds=0.15;
    for(int i=0;i<n_iter;i++){
		 s_min=Iteration(p, ds, s_min);
		 ds*=0.45;
	 }
    return s_min;
}

Coord  CBezierSpline::Iteration(CPoint3d* p, Coord ds, Coord s_min)
{
Coord sb=s_min-ds/2.0;
int ns=10;
Coord dds=ds/(Coord)ns;
CPoint3d pn;
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


CSpline* CBezierSpline::MakeSplineAllLegth()
{
	float delta=10;
	CLine* line=MakeLineAllLength(delta);
	if(!line)
		return NULL;	
	CSpline* spline=new CSpline(line->np());
	for(int i=0;i<line->np();i++)
		*spline->P(i)=*line->P(i);
	spline->Build();
	delete line;
	CVector dir1(P(0),P(1));	
	spline->P7(0)->l=dir1.l;
	spline->P7(0)->m=dir1.m;
	spline->P7(0)->n=dir1.n;
	CVector dir2(P(2),P(3));	
	spline->P7Last()->l=dir2.l;
	spline->P7Last()->m=dir2.m;
	spline->P7Last()->n=dir2.n;
	spline->Update();
	return spline;
}

BOOL CBezierSpline::UpdateNodeSmoothBezier(int np)
{
	CLinkLine* link=m_sl->GetNextLink(this);
	if(np==0)
		link=m_sl->GetPreviousLink(this);
	if(link==NULL)
		return BAD;
	CPoint3d* p0=P(3);
	CPoint3d* p1=P(2);
	CPoint3d* p2=link->P(1);
	CPoint5d* p5=link->P5(1);

	if(np==0){
		p0=P(0);
		p1=P(1);
		p2=link->P(2);
		p5=link->P5(2);
	}
	CVector cx;
	CVector cy;
	CVector cz;
	if(POINTs_SC(p0, p1, p2, &cx, &cy, &cz))
		return OK;

	p1->mod_coord_am(p0, &cx, &cy, &cz);
	p2->mod_coord_am(p0, &cx, &cy, &cz);
	CPoint3d pc;
	CPoint3d pz(0,0,100);
	CVector Norm1(&pc, p1);
	CVector Norm2(&pc, p2);
	double angle=acos(Norm1.l*Norm2.l+Norm1.m*Norm2.m+Norm1.n*Norm2.n);
	double angle2=PI-angle;
	p2->Rotate(&pc, &pz, angle2);
	p1->mod_coord_ma(p0, &cx, &cy, &cz);
	p2->mod_coord_ma(p0, &cx, &cy, &cz);

	p5->s=p2->x;
	p5->t=p2->y;
	
	return OK;
}

BOOL CBezierSpline::ControlNodeSmooth(int np)
{
	return OK;
}

CConstraint* CBezierSpline::MakeNodeSmooth(CSelectPrim* prim)
{
	if(UpdateNodeSmooth(prim->num_pnt, 0))
		return NULL;
	CConstraintNodeSmooth* con= new CConstraintNodeSmooth(prim);
	return con;
}

BOOL CBezierSpline::UpdateNodeSmooth(int np, int )
{
	CLinkLine* link=m_sl->GetNextLink(this);
	if(np==0)
		link=m_sl->GetPreviousLink(this);
	if(link==NULL)
		return BAD;
	if(link->IsKindOf( RUNTIME_CLASS( CBezierSpline ) ))
		return UpdateNodeSmoothBezier(np);

	CVector dir;
	if(np==0)
		link->GetLastDir(&dir);
	else
		link->GetDir(&dir);
	int np2=2;
	int np3=3;
	if(np==0){
		np2=1;
		np3=0;
	}
	CPoint3d pd=*P(np3);
	pd.Move(&dir, 100);
	LINE_2P line(&pd, P(np3));
	LINE_2P line_orth;
	line.MakeOrtho(P(np2), &line_orth);
	line.CrossLine(&line_orth, &pd);
	*P(np2)=pd;
	P5(np2)->s=pd.x;
	P5(np2)->t=pd.y;
	return OK;
}

double CBezierSpline::GetLength()
{
	int np=100;
	int ns = np-1;
	double ds=1.0/(float)ns;

	double delta =0.01;
	double dist_max=1;
	Coord length=0;
	int iter=1;
	while(dist_max > delta && iter< 10)
	{
		length =0;
		for (int i=0; i< np-1; i++)
		{
			dist_max = 0;

			CPoint3d pm;
			GetPoint(ds*i+ds/2.0, &pm);// midle Point

			CPoint3d p1;
			GetPoint(ds*i, &p1);

			CPoint3d p2;
			GetPoint(ds*(i+1), &p2);

			double dist= pm.GetDistLine(&p1, &p2);
			if(dist > dist_max)
				dist_max = dist;
			length+=p1.DistTo(&p2);
		}
		iter++;
		np = np*2;
		ns=np-1;
		ds=1.0/(float)ns;
	}
	return	 length;
}

int CBezierSpline::CrossLine(LINE_2P* line2,CPoint3d** pc)
{
	CLine* line=MakeLine(0.003);
	int np=line->CrossLine(line2, pc);
	delete line;
	return np;
}

int CBezierSpline::CrossCircle(CCircle3D* cr,CPoint3d** pc)
{
	CLine* line=MakeLine(0.003);
	int np=cr->CrossingCLine(line, pc);
	delete line;
	return np;
}

int CBezierSpline::CrossLine2D(LINE_2P* line2,CPoint3d* pc1, CPoint3d* pc2)
{
	CLine* line=MakeLine(0.003);
	CPoint3d* pc=NULL;
	int np=line->CrossLine(line2, &pc);
	delete line;
	if(np)
		*pc1=pc[0];
	if(np>1)
		*pc2=pc[1];
	if(pc)
		free(pc);
	return np;
}

int CBezierSpline::CrossFillet(CFillet* fil,CPoint3d** pc)
{
	CLine* line=MakeLine(0.003);
	int np=fil->CrossLine(line, pc);
	delete line;
	return np;
}

int CBezierSpline::CrossPlane(CPlane* pl, CPoint3d** pc)
{
	CLine* line=MakeLine(0.003);
	int np=line->CrossPlane(pl, pc);
	delete line;
	return np;
}

int CBezierSpline::CrossLine(CLinkLine* link,CPoint3d** pc)
{
	if(link->IsKindOf( RUNTIME_CLASS( CArc ) )){
		CArc* arc=(CArc*)link;
		return CrossArc(arc, pc);
	}

	CLine* line1=MakeLine(0.003);
	CLine* line2=link->MakeLine(0.003);
	int np=line1->CrossLine(line2, pc);
	delete line1;
	delete line2;
	return np;
}

int CBezierSpline::CrossArc(CArc* arc,CPoint3d** pc)
{
	CLine* line=MakeLine(0.003);
	int np=arc->CrossCLine(line, pc);
	delete line;
	return np;
}

CSpline* CBezierSpline::MakeProection(CPlane*pl, CVector* dir)
{
	CPoint3d pc;
	if(pl->cross_Line(P(0), dir, &pc)){
		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return NULL;
	}
	CVector dir1(P(0), &pc);
	CSpline* spl1=MakeSpline();
	if(!spl1)
		return NULL;
	CSpline* spl2=MakeSpline();
	if(!spl2)
		return NULL;
	spl1->Move(&dir1, dist_POINT(P(0), &pc));
	spl2->Move(&dir1, dist_POINT(P(0), &pc));

	spl1->Move(dir, GetLength()*3);
	spl2->Move(dir, -GetLength()*3);

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

int CBezierSpline::UpdateConstraint(CSystemCoord* msc)
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

	int updated=1;
	for(int i=0;i<constr.GetSize(); i++){
		if(constr[i]->IsDeterminated())
			constr[i]->SetUpdateflag(1);
		else
			updated=0;
	}
	if(updated)
		SetUpdateflag(1);

	return OK;
}

CLine* CBezierSpline::MakeLineAllLength(double dopusk)
{
	register Coord  ds=0.2;
	CPoint3d p1;
	CPoint3d p2;
	CVector vect;
    GetPointVector(0.25, &p1, &vect);
    GetPoint(0.25+ds, &p2);
	register Coord delta=dist_POINT_LINE(&p2.x, &p1.x, &vect.l);
    while(delta>dopusk && ds>0.000001){
		ds=ds/2;
		GetPoint(0.25+ds, &p2);
		delta=dist_POINT_LINE(&p2.x, &p1.x, &vect.l);
		}
	int	np=(short)ceil(1/ds);	//	number points
	CLine* line=new CLine(np+1);

	for(register int i=0;i<=np;i++){
		GetPoint(ds*i, line->P(i));
	}
	return line;
}

BOOL CBezierSpline::UpdateFillet(CLinkLine* line2, CFillet* fil, Coord rad)
{
	if(line2->IsKindOf( RUNTIME_CLASS( CBezierSpline ) ))
		return UpdateFilletBezierSpline((CBezierSpline*)line2, fil, rad);
	if(line2->IsKindOf( RUNTIME_CLASS( CArc ) ))
		return UpdateFilletArc((CArc*)line2, fil, rad);

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

	CSpline* cpline=MakeSplineAllLegth();
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
	GetPoint(s, &fil->m_p1);	
	m_fillet2=line2->m_fillet1=1;
	fil->m_Ok=1;
	return OK;
}

BOOL CBezierSpline::UpdateFilletBezierSpline(CBezierSpline* line2, CFillet* fil, Coord rad)
{
	CPoint3d p0=*PLast();
	CPoint3d px=*line2->P(1);
	CPoint3d py=*P(2);
	CVector c1,c2,c3;
	if(POINTs_SC(&p0, &px, &py, &c1, &c2, &c3)){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	CSpline* cpline=MakeSplineAllLegth();
	if(!cpline)
		return BAD;
    cpline->mod_coord_am(&p0, &c1, &c2, &c3);

	cpline->Offset(rad);
	CSpline* cpline2=line2->MakeSplineAllLegth();
	if(!cpline2)
		return BAD;
    cpline2->mod_coord_am(&p0, &c1, &c2, &c3);
	cpline2->Offset(rad);
	CPoint3d* pc=NULL;
	int nc=cpline->CrossLine(cpline2, &pc);
	delete cpline;
	delete cpline2;
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
	double s1=line2->GetOrthPoint(&fil->m_pc);
	CPoint3d pc1;
	line2->GetPoint(s1, &fil->m_p2);
	double s2=GetOrthPoint(&fil->m_pc);
	GetPoint(s2, &fil->m_p1);
	fil->m_pc=fil->m_pc;
	m_fillet2=line2->m_fillet1=1;
	fil->m_Ok=1;
	return OK;
}

BOOL CBezierSpline::UpdateFilletArc(CArc* line2, CFillet* fil, Coord rad)
{
	CPoint3d p0=*PLast();
	CVector Dir;
	line2->GetDir(&Dir);
	CPoint3d py=p0; 
	py.Move(&Dir, 10);
	CPoint3d px=*P(2);
	CVector c1,c2,c3;
	if(POINTs_SC(&p0, &px, &py, &c1, &c2, &c3)){
//		Message_err(IDS_IMPOSIBLE_GEOMETRY);
		return BAD;
	}
	CArc* arc=(CArc*)line2->copy();
	arc->mod_coord_am(&p0, &c1, &c2, &c3);
	if(arc->Pc()->x<0)
		arc->ChangeRadius(rad);
	else
		arc->ChangeRadius(-rad);
	CSpline* cpline=MakeSplineAllLegth();
	if(!cpline)
		return BAD;
    cpline->mod_coord_am(&p0, &c1, &c2, &c3);
	cpline->Offset(-rad);
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
	double s=GetOrthPoint(&fil->m_pc);
	line2->GetPointOrtho(&fil->m_pc, &fil->m_p2);
	GetPoint(s, &fil->m_p1);
	m_fillet2=line2->m_fillet1=1;
	fil->m_Ok=1;
	return OK;
}

void CBezierSpline::EditCurvature(CPoint3d* p1,CPoint3d* p2)
{
	double dist=p1->DistTo(p2);
	CVector dir(p1,p2);

	double kf=1.5;
	double d1=P(1)->DistTo(P(0))*dist/p1->DistTo(P(0));
	P(1)->Move(&dir, d1*kf);
	double d2=P(2)->DistTo(P(3))*dist/p1->DistTo(P(3));
	P(2)->Move(&dir, d2*kf);

}

void CBezierSpline::GetPointLength(double len,CPoint3d* p)
{
	double t=len/GetLength();
	GetPoint(t, p);
}


void CBezierSpline::WriteKnots(float* pn)
{

	pn[0]= P(0)->x;
	pn[1]= P(0)->y;
	pn[2]= P(0)->z;

	pn[3]= P(1)->x;
	pn[4]= P(1)->y;
	pn[5]= P(1)->z;

	pn[6]= P(2)->x;
	pn[7]= P(2)->y;
	pn[8]= P(2)->z;


	pn[9]= P(3)->x;
	pn[10]= P(3)->y;
	pn[11]= P(3)->z;
}

void CBezierSpline::WriteToEps(FILE* fil)
{
	if(!fil)
		return;

	if(m_num<2)
		return;

   fprintf(fil,"newpath\n");
   fprintf(fil,"%8.3f %8.3f moveto\n",P(0)->x, P(0)->y);

	for(int i=1; i< m_num; i++)
		fprintf(fil,"%8.3f %8.3f ",P(i)->x, P(i)->y);

   fprintf(fil,"curveto\n");
   fprintf(fil,"stroke\n");
}

void CBezierSpline::GetPointOrtho(CPoint3d* p, CPoint3d* pc)
{

	*pc=*p;
	CLine* line=MakeLine(0.01);
	if(line==NULL)
		return;
	line->GetPointOrtho(p,pc);
	delete line;
}

CSpline* CBezierSpline::MakeSpline()
{

//	return CreateSpline();

	float delta=10;
	CLine* line=MakeLine(delta);
	if(!line)
		return NULL;	
	CSpline* spline=new CSpline(line->np());
	if(!spline)
	{
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return NULL;
	}	

	for(int i=0;i<line->np();i++)
		*spline->P(i)=*line->P(i);
	spline->Build();
	delete line;
	if(!m_fillet1){
		CVector dir1(P(0),P(1));
		spline->P7(0)->l=dir1.l;
		spline->P7(0)->m=dir1.m;
		spline->P7(0)->n=dir1.n;
	}
	if(!m_fillet2){
		CVector dir2(P(2),P(3));	
		spline->P7Last()->l=dir2.l;
		spline->P7Last()->m=dir2.m;
		spline->P7Last()->n=dir2.n;
	}
	spline->Update();
	return spline;
}

CSpline* CBezierSpline::CreateSpline()
{
	int	QtyDs = 4;

	CSpline* spline = CreateSpline(QtyDs);


/*
	CSpline* spline = new CSpline();
	if(!spline)
		return NULL;
	spline->Alloc(4);
	double len= this->GetLength();

	*spline->P(0) = *P(0);
	*spline->P(3) = *P(3);

	Coord s=0.2;
	CPoint3d pm;
	CVector vm;
	GetPointVector( s, &pm, &vm);
	*spline->P(1) = pm;
	spline->P7(1)->l= vm.l;
	spline->P7(1)->m= vm.m;
	spline->P7(1)->n= vm.n;
	spline->P7(1)->s= len*s;

	s=0.8;
	GetPointVector( s, &pm, &vm);
	*spline->P(2) = pm;
	spline->P7(2)->l= vm.l;
	spline->P7(2)->m= vm.m;
	spline->P7(2)->n= vm.n;
	spline->P7(2)->s= len*s;



	CVector dir1(P(0),P(1));	
	spline->P7(0)->l=dir1.l;
	spline->P7(0)->m=dir1.m;
	spline->P7(0)->n=dir1.n;
	spline->P7(0)->s=0;

	CVector dir2(P(2), P(3));	
	spline->P7Last()->l=dir2.l;
	spline->P7Last()->m=dir2.m;
	spline->P7Last()->n=dir2.n;
	spline->P7Last()->s =  len;

	return spline;
*/
//	double tolerance = DopuskModelling*5.0;
	double tolerance = 1.0;

	double ds = 1.0/(float)QtyDs;
	CPoint3d pc;
	double dist_max=0;
	while (QtyDs<100)
	{
		dist_max=0;
		for(int i=0; i< QtyDs; i++)
		{
			GetPoint(ds*i+ds/2.0, &pc);
			CPoint3d pc2;
			spline->GetPointOrtho(&pc, &pc2);
			double dist = pc.DistTo(&pc2);
			if(dist_max < dist)
			{
				dist_max = dist;
			}
		}
		if(dist_max< tolerance)
			return spline;
		QtyDs = (int)QtyDs*1.5;
		delete spline;
		spline = CreateSpline(QtyDs);
	if(!spline)
		return NULL;
		ds = 1.0/(float)QtyDs;
	}
	delete spline;

	return CreateSpline(QtyDs);

}

CSpline* CBezierSpline::CreateSpline(int QtyDs)
{

	register double  ds= 1.0/(float)QtyDs ;

	int	np = QtyDs +1;
	CPoint3d* pnts = (CPoint3d*)calloc(np, sizeof(CPoint3d));
	if(!pnts)
	{
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return NULL;
	}

	for(register int i=0; i< np; i++){
		GetPoint(ds*i, pnts+i);
	}

//==========================
	CSpline* spline=new CSpline(np);
	if(!spline)
	{
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return NULL;
	}
	if(spline->np()!=np)
	{
		Message_err(IDS_BAD_ALLOC_MEMORY);
		return NULL;
	}
	for(int i=0;i< np;i++)
		*spline->P(i)=*(pnts+i);

	spline->Build();
	CVector dir1(P(0),P(1));	
	spline->P7(0)->l=dir1.l;
	spline->P7(0)->m=dir1.m;
	spline->P7(0)->n=dir1.n;

	CVector dir2(P(2), P(3));	
	spline->P7Last()->l=dir2.l;
	spline->P7Last()->m=dir2.m;
	spline->P7Last()->n=dir2.n;
	spline->Update();

	free(pnts);
	return spline;
}

void CBezierSpline::GetDir(CVector* dir)
{
	if(P(0)->DistTo(P(1))> 0.1)
		dir->calc(P(0), P(1));
	else
		dir->calc(P(0), P(2));
}