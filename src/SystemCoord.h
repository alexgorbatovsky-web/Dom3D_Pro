#if !defined(AFX_SYSTEMCOORD_H__BB051661_6CF0_11D3_9E03_F6939C116F5D__INCLUDED_)
#define AFX_SYSTEMCOORD_H__BB051661_6CF0_11D3_9E03_F6939C116F5D__INCLUDED_

#include "Point3d.h"	// Added by ClassView
#include "Vector.h"	// Added by ClassView
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// SystemCoord.h : header file
//
class CAlfaDoc;
class CPlane;


/////////////////////////////////////////////////////////////////////////////
// CSystemCoord 

class CSystemCoord
{
// Construction
public:
	CSystemCoord();
	CSystemCoord(CPoint3d *p0,CVector* cx,CVector* cy,CVector* cz);
	CSystemCoord(CPoint3d *p0, CPoint3d *px,  CPoint3d *py);
	CSystemCoord(CPoint3d *p0,CVector *v0);//Система Корд. с пл. XY перпенд вектору


// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSystemCoord)
	public:
	//}}AFX_VIRTUAL

// Implementation
public:
	CVector cx;
	CVector cy;
	CVector cz;
	CPoint3d p0;

	virtual ~CSystemCoord();

	BOOL GetMirrorSC(CPlane* pl);
	BOOL Set(CPoint3d* p0, CPoint3d* px, CPoint3d* py);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);
	bool IsValid();
	void print();
	void GetEulerAngles(double& alfa, double& beta, double& gamma, CPoint3d& pn1, CPoint3d& pn2);

};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.


extern void ModifMsc(short init, CAlfaDoc* pDoci, void* );
extern void ModifPointOriginSc(short init, CAlfaDoc* pDoci, void* );


#endif // !defined(AFX_SYSTEMCOORD_H__BB051661_6CF0_11D3_9E03_F6939C116F5D__INCLUDED_)
