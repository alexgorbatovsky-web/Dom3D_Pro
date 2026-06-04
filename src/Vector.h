/******************************** Vector.h *****************************/
#ifndef _LIB_CVECTOR_AG_H
#define _LIB_CVECTOR_AG_H

#include "legacy_geometry_defs.h"

class CPoint3d;
class CPlane;
class CSystemCoord;

class CVector
{
public:
    double l,m,n;

public:
// Constructors
	CVector(){l=1.0;m=n=0;}

	CVector(double li,double mi,double ni){l=li;m=mi;n=ni;}
	CVector(CPoint3d* p1, CPoint3d* p2);
	CVector(CVector* v1, CVector* v2);
	CVector(CPoint3d& p1, CPoint3d& p2);
	CVector(CVector* v1, CVector* v2, CVector* v3, CVector* v4);
	CVector(double axy, double az);//2 полярных угла
	double	DotP(const CPoint3d& p2);

	inline double	Dot( const CVector & p2)
	{
		return (l*p2.l + m*p2.m + n*p2.n);
	}
	double & operator [] (const int Index);

	void operator += (const CVector &);

    BOOL Normalize();
	void Read(char* str);

    void mod_coord_ma(CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);
	void Rotate(CPoint3d* ,CPoint3d* ,double );
	void Mirror();

    double calc(CPoint3d* p1, CPoint3d* p2);
    void print(void);
    void print(LPCTSTR text);

//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CVector& v);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CVector& v);
	void GetOrth(CVector *v);
	BOOL IsCongruent(CVector *v);
	BOOL MultVectors(CVector* v1, CVector* v2);
	BOOL Mult(CPoint3d* p1, CPoint3d* p2, CPoint3d* p3, CPoint3d* p4);
	void Revers();
	BOOL IsCollinear(CVector* v2, double delta);
	double GetAngle(CVector* vect2);
	double GetAngleRotateVector(CVector* v1,CVector* v2);
	void GetAnglesRotateVector(CVector* v1,CVector* v2, double* alfa, double* beta);
	int GetDir(CVector* cx, CVector* v2);
	double GetAngle(CPlane* pl);
	void LMN_lm(double*li, double*mi);
	int GetTwoAngleRotate(CVector* v2, double* alfa, double* beta);
	int GetTwoAngleRotateVx(CVector* v2, double* alfa, double* beta);
	int GetTwoAngleRotateVz(CVector* v2, double* alfa, double* beta);
	void Set(double li,double mi,double ni){l=li;m=mi;n=ni;}
	void GetMidle(const CVector& v1, const CVector& v2);//This vector make midle between v1 & v2

	void Cross(const CVector &u, const CVector &v) {
		*this = CVector(u.m * v.n - u.n * v.m, u.n * v.l - u.l * v.n, u.l * v.m - u.m * v.l);
	}

};

inline double & CVector::operator [] (const int Index) {
	ASSERT(Index >= 0 && Index < 3);
	return (&l)[Index];
}

// cVec3::operator += : void (const CVector &)
inline void CVector::operator += (const CVector &u) {
	l += u.l;
	m += u.m;
	n += u.n;
	Normalize();
}

#endif /* _LIB_CVECTOR_AG_H */

