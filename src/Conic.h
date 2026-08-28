#pragma once

#include "Point3d.h"
#include "Vector.h"

class CSplineCurve;
class CPolyline;

class CConic
{
public:
	CConic();
	CConic(CPoint3d* pa, CPoint3d* pb, CPoint3d* pc, double f);
	virtual ~CConic() {}

protected:
	CPoint3d m_pa;
	CPoint3d m_pb;
	CPoint3d m_pc;
	double m_f;


public:
	void GetPoint(double t, CPoint3d* pt);
	void GetPointVector(double s, CPoint3d* p, CVector* v);
	CSplineCurve* MakeSpline(int Qty);
	CPolyline* MakeLine(double dopusk);

};

