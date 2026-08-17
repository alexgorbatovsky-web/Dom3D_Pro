/******************************** Circle2D.h *****************************/
#ifndef _LIB_AG_CIRCLE_2D_H
#define _LIB_AG_CIRCLE_2D_H

class CPoint3d;
class CView3d;
struct LINE_2P;
class CLine;

struct CCircle2D
{
CPoint3d m_pc;
Coord m_rad;

	CCircle2D()
		:m_pc(0,0,0)
	{m_rad=10;}
	CCircle2D(CPoint3d* p, Coord r){m_pc=*p;m_rad=r;}
	CCircle2D(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3);

	int InitPoints(Coord delta, CPoint3d p[]);
	void Draw(CView3d* pview, Pixel col);

	CPoint3d* Pc(){return &m_pc;}
	double Rad(){return m_rad;}
	int CrossLine(CPoint3d *p1,CPoint3d *p2, CPoint3d *pc1,CPoint3d *pc2);
	int CrossCircleG(CCircle2D* cr2, CPoint3d* pc1, CPoint3d* pc2);
	int CrossCircle(CCircle2D* cr2, CPoint3d* pc1, CPoint3d* pc2);
	int PointIn(CPoint3d* p);
	int GetPointTanto(CPoint3d* p, CPoint3d* pt1, CPoint3d* pt2);
	int GetCircleTanto(CPoint3d* p, double rad, CPoint3d* pt1, CPoint3d* pt2);
	CCircle2D* MakeCircle(LINE_2P* line, int dir, Coord rad, int dir1, int dir2);
	CLine* MakeLineTanto(CPoint3d* p, int dir);
	int GetPointTanto(CPoint3d* p, int dir, CPoint3d* pt);

};




#endif /* _LIB_AG_CIRCLE_2D_H */

