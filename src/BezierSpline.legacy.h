// BezierSpline.h: interface for the CBezierSpline class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BEZIERSPLINE_H__3D6E10C4_08A0_11D4_9E04_AB59505B8925__INCLUDED_)
#define AFX_BEZIERSPLINE_H__3D6E10C4_08A0_11D4_9E04_AB59505B8925__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "LinkLine.h"
#include "Point3d.h"	// Added by ClassView
class CSmartLine;
class CArc;
class CSpline;

class CBezierSpline : public CLinkLine  
{
protected:

	CBezierSpline();

public:
	CBezierSpline(CPoint3d* p0, CPoint3d* p1, CPoint3d* p2, CPoint3d* p3);
	CBezierSpline(double x1, double y1,double x2, double y2,double x3, double y3,double x4, double y4);

	virtual ~CBezierSpline();

	int	GetType(){return 4;}
	void GetPoint(double t,CPoint3d* pt);
	void GetPointVector(double s,CPoint3d *p, CVector* v);

	void Draw(CView3d* pview);
	double Get_dist_Min(CPoint3d* pm, CView3d* view);
	BOOL EditPoint(int np, CPoint3d* p, int constr);
	void print(FILE* strm);

	double FindPoint(CPoint3d* pm, CView3d* view, int* i_min);
	virtual int MakeKnot(CLinkLine* line2);
	virtual int MakeLastKnot();
	BOOL Offset(double offset);
	CLinkLine* InsertPoint(CPoint3d* pc);

	double GetOrthPoint(CPoint3d* p);
	double  Iteration(CPoint3d* p, double ds, double s_min);
	BOOL UpdateFillet(CLinkLine* line2, CFillet* fil, double rad);
	CSpline* MakeSpline();
	CSpline* MakeSplineAllLegth();
	CConstraint* MakeNodeSmooth(CSelectPrim* prim);
	BOOL UpdateNodeSmooth(int np, int var);
	BOOL UpdateNodeSmoothBezier(int np);
	BOOL ControlNodeSmooth(int np);
	double GetLength();
	void GetPointLength(double len,CPoint3d* p);

	virtual int CrossLine(LINE_2P* line,CPoint3d** pc);
	virtual int CrossCircle(CCircle3D* cr,CPoint3d** pc);
	virtual int CrossLine(CLinkLine* line,CPoint3d** pc);
	virtual int CrossArc(CArc* arc,CPoint3d** pc);
	virtual int CrossLine2D(LINE_2P* line2,CPoint3d* pc1, CPoint3d* pc2);
	virtual int CrossFillet(CFillet* fil,CPoint3d** pc);
	virtual int CrossPlane(CPlane* pl, CPoint3d** pc);
	virtual CSpline* MakeProection(CPlane* pl, CVector* dir);
	virtual bool IsStraight(){return 0;}
	int UpdateConstraint(CSystemCoord* msc);
	virtual BOOL EditLastPoint(CPoint3d* p, int constr);
	virtual BOOL EditFirstPoint(CPoint3d* p);
	virtual void GetPointOrtho(CPoint3d* pm, CPoint3d* pc);

	CPolyline* MakeLineAllLength(double dopusk);
	BOOL UpdateFilletBezierSpline(CBezierSpline* line2, CFillet* fil, double rad);
	BOOL UpdateFilletArc(CArc* line2, CFillet* fil, double rad);

	void EditCurvature(CPoint3d* p1,CPoint3d* p2);

	CSpline* CreateSpline();
	void WriteKnots(float* pn);
	void WriteToEps(FILE*);
	CSpline* CreateSpline(int nds);
	void GetDir(CVector* dir);

};


#endif // !defined(AFX_BEZIERSPLINE_H__3D6E10C4_08A0_11D4_9E04_AB59505B8925__INCLUDED_)
