// Arc.h: interface for the CArc class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ARC_H__93FC1AE1_1EDF_11D4_82FE_CC9423E1D45D__INCLUDED_)
#define AFX_ARC_H__93FC1AE1_1EDF_11D4_82FE_CC9423E1D45D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "LinkLine.h"
#include "SystemCoord.h"

class CArc : public CLinkLine  
{
protected:
	BOOL m_clock;
	Coord m_rad;
	CSystemCoord m_sc;
	CPoint3d m_PntIn;


	CArc();
public:
	CArc(CPoint3d* pc, CPoint3d* p1, CPoint3d* p2);
	CArc(double x0, double y0, double x1, double y1,double x2, double y2);

	virtual ~CArc();
	CArc(const CArc* src);
	CLinkLine* copy();

	int	GetType(){return 3;}
	void Draw(CView3d* pview);

    virtual void Move(CPoint3d* ,CPoint3d* );
    virtual void Move(CVector* ,double );
	virtual void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    virtual void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	virtual void mod_coord_am(CSystemCoord* sc);
	virtual void mod_coord_ma(CSystemCoord* sc);
	CLine* MakeLine(double dopusk);
	int InitPoints(Coord delta, CPoint3d p[]);
	double Get_dist_Min(CPoint3d* pm, CView3d* view);
	double Get_dist_Min(CPoint3d* pm);
	BOOL EditPoint(int np, CPoint3d* p, int constr);
	CPoint3d* Pc(){return P(1);}
	double Rad(){return m_rad;}
	CCircle2D* MakeCircle(LINE_2P* line, BOOL dir,Coord rad, BOOL in, BOOL first);
	BOOL UpdateFillet(CLinkLine* line, CFillet* fil, Coord rad);
	void ChangeDir();
	void Mirror();
	void MirrorM();

	int CrossLine2D(LINE_2P* line,CPoint3d* pc1, CPoint3d* pc2);
	int CrossLine(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc1, CPoint3d* pc2);
	int CrossLine(LINE_2P* line, CPoint3d** pc);
	int CrossInfinityLine(LINE_2P* line, CPoint3d** pc);
	int CrossCLine(CLine* line,CPoint3d** pc);
	int CrossCircle(CCircle3D* cr,CPoint3d** pc);
	int CrossLine(CLinkLine* link,CPoint3d** pc);
	int CrossArc(CArc* arc,CPoint3d** pc);
	int CrossFillet(CFillet* fil,CPoint3d** pc);
	int CrossPlane(CPlane* pl, CPoint3d** pc);
	
	void SetClock(BOOL dir){m_clock=dir;}
	BOOL PontIn(CPoint3d* p);
	void Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size);
	double GetDistMin2D(Coord xm,Coord ym);
	BOOL EditLastPoint(CPoint3d* p, int constr);
	Coord GetLength();
	virtual void Revers();
    virtual void Zoom(CPoint3d* ,double ,double ,double );
	BOOL Offset(Coord offset);
	void GetPointOrtho(CPoint3d* pm,CPoint3d* pc);
	CCubSpline* MakeSpline(Coord delta);
	virtual BOOL ExtendBeginTo(CContour* boundry, CPoint3d* pm);
	virtual BOOL ExtendEndTo(CContour* boundry, CPoint3d* pm);
	double FindPoint(CPoint3d* pm, CView3d* view, int* i_min);
	void GetPoints(CPoint3d p[4]);
	BOOL GetPoint(int num, CPoint3d* p);

	CLinkLine* InsertPoint(CPoint3d* pc);
	int MakeKnot(CLinkLine* line2);
	int MakeLastKnot();
	int MakeKnotArc(CArc* arc);
	int MakeLastKnotArc(CArc* arc);
	virtual CSpline* MakeSpline();

	virtual int UpdateConstraint(CSystemCoord* msc);
	virtual int UpdateConstraint0(CSystemCoord* msc);
	virtual BOOL IsBasePoint(CSelectPrim* pr);
	BOOL SetRad(double rad);
	BOOL EditKnot(CLinkLine* line2);
	BOOL UpdatePoint(int np, LINE_2P* line);

	CConstraint* MakeNodeSmooth(CSelectPrim* prim);
	CConstraint* MakeNodeSmoothArc(CSelectPrim* prim);
	BOOL UpdateNodeSmooth(int np, int var);
	BOOL UpdateNodeSmoothArc(int np, int var);
	BOOL ControlNodeSmooth(int np);
	BOOL ControlNodeSmoothArc(int np);
	void GetDir(CVector*);
	void GetLastDir(CVector*);
	void Extend(double , int ){}
	CConstraint* MakeConstrTanto2();
	BOOL UpdateConstrTanto2(int dir1, int dir2, int dir);
	BOOL ControlConstrTanto2();
	CConstraint* MakeConstrTantoLine();
	BOOL UpdateConstrTantoLine(int dir1, int dir2, int dir);
	BOOL ControlConstrTantoLineCr();
	double FindPoint(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0, double k, int* i_min);
	double Get_dist_Min(CPoint3d* pm,  CSystemCoord* sc, CSystemCoord* vsc, CPoint3d* p0,  double k);
	virtual CSpline* MakeProection(CPlane* pl, CVector* dir);
	bool IsStraight(){return false;}
	bool EditRadius(CPoint3d* p);
	void RestoreDirArc();
	void SaveDirArc();
	BOOL UpdateFilletBezierSpline(CBezierSpline* line2, CFillet* fil, Coord rad);
	BOOL ChangeRadius(Coord offset);
	BOOL UpdateFilletArc(CArc* line2, CFillet* fil, Coord rad);
	void EditCurvature(CPoint3d* p1,CPoint3d* p2);
	void GetPoint(Coord t, CPoint3d * pt);// t=0-1;
	Coord GetOrthPoint(CPoint3d* p);
	void GetPointLength(double len,CPoint3d* p);
	bool IsRadiusConstraint();

static	bool (* m_wrk_function)(CSelectPrim& Prim, CVector* dir,double dist);

};



#endif // !defined(AFX_ARC_H__93FC1AE1_1EDF_11D4_82FE_CC9423E1D45D__INCLUDED_)
