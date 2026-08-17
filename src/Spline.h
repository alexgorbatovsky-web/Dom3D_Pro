// Spline.h: interface for the CSpline class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SPLINE_H__D47A21E1_3902_11D6_9520_BE8A13750473__INCLUDED_)
#define AFX_SPLINE_H__D47A21E1_3902_11D6_9520_BE8A13750473__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <vector>
#include "LinkLine.h"

class COutLine;
class CCubSpline;
class CSurface;
class CLine;


class CSpline : public CLinkLine  
{
protected:
	CSpline();

	CPoint7d* m_p7;
	int m_n;

	COutLine* m_outline;
	BYTE m_InitOutLine;
	std::vector <CPoint3d> m_Knots;
	std::vector <double> m_Params;


public:
	CString m_Name;

public:
	CSpline(int np);
	CSpline(CCubSpline* csr);
	CSpline(CPoint7d* pc, double rad);
	CSpline(CPoint3d* ap,int num_p);
	CSpline(CPoint7d* ap,int num_p);
	CSpline(const CSpline* src);
	CSpline(CPoint3d* pc, Coord rad, Coord delta);
	CSpline(CPoint3d* p1, CPoint3d* p2);

	virtual ~CSpline();
	BOOL Alloc(int num_p);
	BOOL Realloc(int num_p);
	CLinkLine* copy();

	int	GetType(){return 5;}

	CPoint3d* P(int num);
	CPoint7d* P7(int num);
	CPoint3d* PLast(void){return (CPoint3d*)&m_p7[m_n-1];}
	CPoint7d* P7Last(void){return &m_p7[m_n-1];}
	BOOL Update();
	BOOL Build();
	int np(){return m_n;}


	void mod_coord_ma(CSystemCoord* sc);
	void mod_coord_am(CSystemCoord* sc);
	virtual void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    virtual void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void Rotate(CPoint3d* p0,CPoint3d* p1,double alfa);
    virtual void Move(CPoint3d* ,CPoint3d* );
    virtual void Move(CVector* ,double );
	virtual void Revers();

	BOOL JoinG(CSpline *line2);
	bool JoinSL(CSpline *line2);
	bool JoinX(CSpline *line2);

	BOOL GetPoint(double s,CPoint7d *p,short extra=3);
	BOOL InsertPoint(Coord s);
	BOOL InsertPoint(int N, CPoint7d* p);
	CLinkLine* InsertPoint(CPoint3d* pc);
	void Serialize(CArchive& ar);

	BOOL Join(CSpline* line2, double delta);
	BOOL SetPoints(CSpline *exam);
	BOOL GetPointLength(double length, CPoint7d *p, short extra=3);

	BOOL parametrL_to_s(int num,Coord Length,Coord *s);
	BOOL DeletePoint(int N);
	BOOL ControlPoints(double delta);
	BOOL AddPointExample(CSpline *exam);
	BOOL AddPoint(CPoint3d* p, bool rebuild=true);
	BOOL ConversionStep(CSpline *exam1, CSpline *exam2, Coord step);
	BOOL ConversionStep2(CSpline *exam1, CSpline *exam2, Coord step);
	void Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size);
	double GetLength();
    void Zoom(CPoint3d* ,double ,double ,double );
	void Draw(CView3d* pview);
	bool UpdateOutLine(double dopusk);
	bool ConrolDelta(int num,Coord dopusk,Coord ds,int *pr);
	BOOL Offset(Coord offset);
	CLine* MakeLine(double delta);
	CLine* MakeLineAllLength(double delta);

	Coord Localized_point(Coord s,Coord ds,CPlane *pl,CPoint7d *p);
	void Extend(double dist, int var, bool insert_point=true);
	void ExtendSL(double dist, int var, bool insert_point=true);
	Coord GetOrthPoint(CPoint3d* p);
	CSpline* Simplify(int num_p);
	double FindPoint(CPoint3d* pm, int* i_min);

	int MakeKnot(CLinkLine* line2);
	int MakeFirstKnot(CLinkLine* line2);
	int MakeLastKnot();

	int CrossLine(LINE_2P* line,CPoint3d** pc);
	int CrossLine2D(LINE_2P* line2,CPoint3d* pc1, CPoint3d* pc2);
	int CrossPlane(CPlane* pl, CPoint3d** pc);

	BOOL EditFirstPoint(CPoint3d* p);
	BOOL EditLastPoint(CPoint3d* p, int constr);
	BOOL EditPoint(int np, CPoint3d* p, int constr);

	double FindPoint(CPoint3d* pm, CView3d* view, int* num);
	BOOL GetPoint(int num, CPoint3d* p);
	double Get_dist_Min(CPoint3d* p, CView3d* view);
	void print(CMemFile* file);
	void print(FILE* fil);
	void print();

	CSpline* MakeSpline();
	bool IsStraight(){return 0;}
	int CrossLine(CLinkLine* line,CPoint3d** pc);
	void GetDir(CVector* v);
	void GetLastDir(CVector* v);
	void Mirror();
	void MirrorX();
	void AddKnotsRect(CSmartLine* sl, CPoint3d* p1, CPoint3d* p2,\
		CView3d* view, CMoveKnotsBox* box);
	void RemoveKnotsRect(CSmartLine* sl, CPoint3d* p1, CPoint3d* p2,\
							 CView3d* view, CMoveKnotsBox* box);

	BOOL InRect(CPoint3d* p1, CPoint3d* p2);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2 , CView3d* view);
	BOOL ExtendBeginTo(CContour* , CPoint3d* ){return OK;}
	BOOL ExtendEndTo(CContour* , CPoint3d* ){return OK;}

	void Draw(CDC* pDC, double dx,double dy, double kx, double ky);
	void GetMidlePoint(CPoint3d* pm);
	void GetCenterPoint(CPoint3d* pm);

	BOOL write_file_dxf(FILE *fil);
	void Set_MSK();
	bool UpdatePoints();
	void UpdateST();
	int MakeKnotSpline(CSpline* line2);
	int MakeFirstKnotSpline(CSpline* line2);
	BOOL UpdateFillet(CLinkLine* line2, CFillet* fil, Coord rad);
	BOOL DeleteFillet(CSmartLine* sl);
	virtual void GetPointOrtho(CPoint3d* pm, CPoint3d* pc);
	bool GetPlaneSC(CSystemCoord* msc, double angle);
	bool SetPlaneSC(double angle, bool IsX0Center, bool ChangeBeg=false);
	bool SetPlaneSC(CVector& cx, CVector& cy, bool IntitSC, double angle, bool IsX0Center, bool IsChangeBeg=false);
	bool SetPlaneSC_G(CSystemCoord& msc, double angle, bool IsX0Center, bool IsChangeBeg);
	BOOL ChangeBegin(CPoint3d* Pb);
	bool IsClosed();
	bool InsertKnot(CPoint3d* p);
	double GetDistMin(CPoint3d* p);
	bool Intersection(CSpline* kr2, double* p1, double* p2);
	bool MoveBeginPoint();
	Coord GetLengthByP(Coord p);
	void MakeShear(CPoint3d* p0, CPoint3d* px, CPoint3d* py,  double dy0);
	int Trimming(CPlane *pl,CPoint3d *pk);
	bool MakePolyLine(std::vector<CPoint3d>& pnts, float tolerance);
	void RotateByTwoVector(CPoint3d* p0, CVector* v1, CVector* v2);
	void GetPlaneSC(CSystemCoord* msc);
	void AddKnot(CPoint3d* pnt);
	CPoint3d GetKnot(int ind);
	int GetQtyKnots(){return m_Knots.size();}
	void AddParams(double p){m_Params.push_back(p);}
	double GetParam(int ind);
	int GetQtyParam(){return m_Params.size();}
	bool ControlDirections(CSpline* spl2);
	bool ChangeBeginByTemplate(CSpline* example);
	bool ReParametrizationByTemplate(CSpline* example);
	bool ReParametrizationByClosedTemplate(CSpline* example);
	bool ChangeBeginByCross(CSpline* splCr);
	CSpline* SimplifyByCross(CSpline* splCr);
	int FindNearestPoint(CPoint3d* p);


static	bool SetPlaneSC(CTypedPtrArray<CObArray, CSpline*>& splines, double angle);
static	bool GetPlaneSC(CTypedPtrArray<CObArray, CSpline*>& splines, CSystemCoord* msc);


	/*
	virtual int CrossCircle(CCircle3D* cr,CPoint3d** pc);
	virtual int CrossLine(CLinkLine* line,CPoint3d** pc);
	virtual int CrossArc(CArc* arc,CPoint3d** pc);
	virtual int CrossLine( CSmartLine* sl2,CPoint3d** pc);
	virtual int CrossFillet(CFillet* fil,CPoint3d** pc);
	
    virtual void print(FILE* strm=stderr);
    virtual void print(LPCTSTR text, FILE* strm=stderr);

	virtual double FindExtremePoint(CPoint3d* pm, CView3d* view, int* num);


	virtual BOOL UpdateFilletArc(CArc* arc, CFillet* fil, Coord rad);
	CVector* GetDir(){return &m_dir;}

	virtual void GetPointOrtho(CPoint3d* pm,CPoint3d* pc);
	virtual int CrossPlane(CPlane* pl, CPoint3d** pc);

	double FindExtremePoint(CPoint3d* pm, int* i_min);

	virtual CLinkLine* Trimming(CRectangle* rect);
	void Trimming1(CRectangle* rect, LINE_2P* line, CPoint3d* pc);
	CLinkLine* Trimming2(CRectangle* rect, LINE_2P* line, CPoint3d* pc1, CPoint3d* pc2);

	virtual CLinkLine* Trimming(CCircle2D* c2d);
	void Trimming1(CCircle2D* c2d, LINE_2P* line, CPoint3d* pc);
	CLinkLine* Trimming2(CCircle2D* c2d, LINE_2P* line, CPoint3d* pc1, CPoint3d* pc2);
*/
protected:
	Coord Iteration(CPoint3d* p, Coord ds, Coord s_min);
	double IterationPm(CPoint3d* p1, CPoint3d* p2, double dist1);
};

struct ChainSpline{

	CArray<CSpline*, CSpline*> chain;
	void Add(CSpline* ptr){if(ptr) chain.Add(ptr); }

	ChainSpline & operator =(const ChainSpline& p);
};

inline ChainSpline& ChainSpline::operator = (const ChainSpline &ch)
{
	if (&ch == this)
		return *this;
	for (int i = 0; i < ch.chain.GetSize(); i++)
		chain.Add(ch.chain[i]);

	return *this;
}


#endif // !defined(AFX_SPLINE_H__D47A21E1_3902_11D6_9520_BE8A13750473__INCLUDED_)
