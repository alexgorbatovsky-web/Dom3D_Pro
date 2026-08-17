/******************************** Circle3D.h *****************************/
#ifndef _LIB_AG_CIRCLE_3D_H
#define _LIB_AG_CIRCLE_3D_H
#include "CAlfaObject.h"
#include "Point3d.h"

class CAlfaDoc;
class  CView3d;
class CLine;
class CLineFig;
class CArc;

class CCircle3D : public CAlfaObject
{

protected:
	CPoint3d m_pc;
	Coord m_rad;

    CCircle3D();
	void Alloc();
public:
static unsigned long col_set;

	CCircle3D(CPoint3d* p, Coord r);
	CCircle3D(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3);
	CCircle3D(CPoint3d* p1, CPoint3d* p2);

    CCircle3D(const CCircle3D* src);

    ~CCircle3D();

    virtual CFigure* copy(BOOL id_copy=0);
	BOOL Copy(const CFigure* fig);
	virtual BOOL ControlBAD();

    virtual void Draw( CView3d* view);

    virtual unsigned int Get_size(){return 0;}

    virtual void Move(CPoint3d* ,CPoint3d* );
    virtual void Move(CVector* ,double );

    virtual void Zoom(CPoint3d* ,double ,double ,double );
    virtual void Mirror();

    virtual void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    virtual void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    virtual void mod_coord_am(CSystemCoord* sc);
    virtual void mod_coord_ma(CSystemCoord* sc);

    virtual void print(FILE* strm=stderr);
    virtual void print(LPCTSTR text, FILE* strm=stderr);

    virtual void Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size);

	virtual double FindPoint(CPoint3d* pm, CView3d* view, CSelectPrim* prim);
	double FindPoint(CPoint3d* pm, CSelectPrim* prim);
	double FindPoint(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0, double k, CSelectPrim* prim);

	BOOL GetPoint(CSelectPrim* prim, CPoint3d* p);
	double Get_dist_Min(CPoint3d* p, CView3d* view);
    double Get_dist_Min(CPoint3d* pm);
	double Get_dist_Min(CPoint3d* pm,  CSystemCoord* sc, CSystemCoord* vw, CPoint3d* p0,  double k);
    BOOL InRect(CPoint3d* p1, CPoint3d* p2 , CView3d* view);

	void DrawInfo(CView3d* view);

	int CrossLine(CPoint3d *p1,CPoint3d *p2, CPoint3d *pc1, CPoint3d *pc2);
	BOOL write_file_dxf(FILE *fil);
	int CrossingCLine(CLine* line, CPoint3d **pc);
	int CrossLine(CLineFig* line, CPoint3d **pc);
	int CrossLine(LINE_2P* line ,CPoint3d** pc);
	int CrossCircle(CCircle3D* cr2, CPoint3d** pc);
	void GetMidlePoint(CPoint3d* pm);

	int GetPointTanto(CPoint3d* p, int dir, CPoint3d* pt);
	CPoint3d* Pc(){return &m_pc;}
	double Rad(){return m_rad;}
	CLine* MakeLineTanto(CPoint3d* p, int dir);
	void GetPoints(CPoint3d p[4]);

	virtual int Crossing(CContour* line2, CPoint3d** pc);
	virtual CContour** Trimming(CContour** trim_line,int num_trim, int*  nl);
	void GetPointOrtho(CPoint3d* pm,CPoint3d* pc);
	virtual CLine* MakeLine(double delta);

	double FindRadius(CPoint3d* , CView3d* , CSelectPrim* );
	double FindRadius(CPoint3d* , CSelectPrim* );
	double FindRadius(CPoint3d* , CSystemCoord*, CSystemCoord*, CPoint3d*,  double ,CSelectPrim* );
	BOOL GetRadius(CSelectPrim* ,CCircle2D*);
	BOOL EditPoint(CSelectPrim* prim, CPoint3d* p, bool KnotOnly);

	void AddKnotsRect(CPoint3d* , CPoint3d* , CView3d* , CMoveKnotsBox* );
	void RemoveKnotsRect(CPoint3d* p1, CPoint3d* p2, CView3d* view, CMoveKnotsBox* box);
	Coord GetLength();

	Coord GetParametr(CPoint3d* p);
	CContour* Trimming(CSelectBox* SelectBox, CPoint3d* pm, CDraft* dr=NULL);
	void Edit();
	CCircle3D* MakeOffset(Coord offset);
	CArc* Trimming(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc);
	BOOL SetRadius(double rad){ if(rad<=0) return BAD; m_rad=rad; return OK;}
	BOOL IsPointInArc(CPoint3d* p, CSelectPrim* rad);
	void DrawKnots(CView2d* view, CDC* pDC, double dx, double dy, double k);
    BOOL InRect(CPoint3d* p1, CPoint3d* p2);
	void GetCenterPoint(CPoint3d* p);
	BOOL GetExtremumX(CArray<CPoint7d, CPoint7d>* );
	BOOL GetExtremumY(CArray<CPoint7d, CPoint7d>* );

protected:
	int InitPoints(Coord delta, CPoint3d p[]);
	int CrossCircleG(CCircle3D* cr2, CPoint3d** pc);


};



#endif /* _LIB_AG_CIRCLE_3D_H */

