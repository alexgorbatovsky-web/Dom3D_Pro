// OutLine.h: interface for the COutLine class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OUTLINE_H__DFE2D716_467F_4A16_AA63_9B7CE88ACD21__INCLUDED_)
#define AFX_OUTLINE_H__DFE2D716_467F_4A16_AA63_9B7CE88ACD21__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class COutLine  
{
public:
	COutLine();
	COutLine(int np);
	COutLine(CPoint3d& p1, CPoint3d& p2);

	virtual ~COutLine();

protected:
	CPoint3d* m_p;
    int m_n;

public:

	BOOL Alloc(int num_p);
	BOOL Realloc(int num_p);
    BOOL AddPoint(CPoint3d* p);
	BOOL InsertPoint(int np,CPoint3d *p);

	CPoint3d* P(int num_p);
	int np(){return m_n;}

    void Draw( );

    void Move(CVector* dir,double dist);
    void Move(CPoint3d* ,CPoint3d* );

    void Zoom(CPoint3d* ,double ,double ,double );
    void Mirror();
    void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);

    void Get_Min_Max(CVector* cx, CVector* cy, CVector* cz, CSizeBlock* size);
    double Get_dist_Min(CPoint3d* p, CView3d* view);
	double Get_dist_Min2D(CPoint3d* pm);
	double Get_dist_Min(CPoint3d* pm, CSystemCoord* sc, CSystemCoord* vw, double k, CPoint3d* p0);
	double Get_dist_MinAndPointMin(CPoint3d* p, CPoint3d* pmin);

};

#endif // !defined(AFX_OUTLINE_H__DFE2D716_467F_4A16_AA63_9B7CE88ACD21__INCLUDED_)
