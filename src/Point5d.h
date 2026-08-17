// Point5d.h: interface for the CPoint5d class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_POINT5D_H__16A8664A_C1DF_41A0_A2B0_16E7A75251A7__INCLUDED_)
#define AFX_POINT5D_H__16A8664A_C1DF_41A0_A2B0_16E7A75251A7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "SystemCoord.h"

struct CPoint5d  
{
public:
	CPoint5d();
Coord x,y,z;
Coord s,t;

    void Move(CPoint3d* p1, CPoint3d* p2);
    void Move(CVector* dir,double dist);

    void Zoom(CPoint3d* pf,double kx,double ky,double kz);
    void Mirror();
    void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);
	BOOL Offset(double offset, CPoint3d* p1,CPoint3d* p2);

};

#endif // !defined(AFX_POINT5D_H__16A8664A_C1DF_41A0_A2B0_16E7A75251A7__INCLUDED_)
