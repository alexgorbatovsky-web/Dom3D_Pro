///********************************Point3d.h*****************************/
#ifndef _LIB_PLANE_AG_H
#define _LIB_PLANE_AG_H

#include "ageom.h"

class CPoint3d;
class CVector;

class CPlane
{
public:
	double a,b,c,d;

// Constructors
	CPlane(){a=1.0;b=0.0;c=0.0;d=0.0;}
	CPlane(double ai,double bi,double ci,double di){a=ai;b=bi;c=ci;d=di;}
	CPlane(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3);
	CPlane(CPoint3d* p, CVector* vect);//Плоскоть проходящая через точку перп. вектору
	CPlane(CPoint3d* p, CVector* vect,CPlane *pl);//Плоскоть проходящая через Прямую перп. Плоск.
	CPlane(CPoint3d* p, CPlane *pl);//Плоскоть проходящая через Точку парал. Плоск.

	void Get_3Point(CPoint3d* p1,CPoint3d* p2,CPoint3d* p3);
    void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void print(FILE* strm=stderr);
    void print(const char* text, FILE* strm=stderr);

	double dist_Point(CPoint3d* p);
	BOOL cross_Line(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc);
	BOOL cross_Line(CPoint3d* p, CVector* v, CPoint3d* pc);
//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CPlane& pl);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CPlane& pl);
	void Read(char* str);
	BOOL CrossNormal(CPoint3d* p1, CPoint3d* pc);
	double GetAngle(CVector* v);
	double GetAngle(CPlane* pl2);
	void Move(CVector* vect, double dist);
	void Rotate(CPoint3d* p0, CVector* vect,double alfa);
	BOOL Crossing3Planes(CPlane *pl2, CPlane *pl3, CPoint3d* pc);
	BOOL CroossCPlane(CPlane* pl2, CPoint3d* p1, CPoint3d* p2);

};

extern BOOL  ABCD(double p1[],double p2[],double p3[],double *a,double *b,double *c,double *d);
extern BOOL crossing_3plat(CPlane *pl1,CPlane *pl2,CPlane *pl3,double pc[]);

#endif // _LIB_PLANE_AG_H 
