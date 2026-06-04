// Triangle.h: interface for the CTriangle class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TRIANGLE_H__3581364D_F601_491C_B765_2777DABFB7F8__INCLUDED_)
#define AFX_TRIANGLE_H__3581364D_F601_491C_B765_2777DABFB7F8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Point3d.h"
#include "Plane.h"
#include "Vector.h"

struct CSizeBlock
{
	double X_min, X_max;
	double Y_min, Y_max;
	double Z_min, Z_max;
	CPoint3d m_p[8];//Угловые точки
	double m_Rad;//Радиус ограничивающей  Сферы
	CPoint3d m_pc;//Центр ограничивающей сферы
	CPlane m_plane[6];//Граничные плоскости

	CSizeBlock() { X_min = Y_min = Z_min = 1e15;	X_max = Y_max = Z_max = -1e15; m_Rad = 0; }

	void Update();
	int CrossLine(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc, CPlane* pl);
	bool IsPointIn(CSizeBlock* Block2, CPoint3d* p_In);
	void Move(CVector* dir, double dist);
	void Move(CPoint3d* p1, CPoint3d* p2);
	void print();
	void SetEmpty() { X_min = Y_min = Z_min = 1e15;	X_max = Y_max = Z_max = -1e15; m_Rad = 0; }
	bool IsEmpty() { return X_min > X_max || Y_min > Y_max; }
	void AddPoint(CPoint3d* p);
	CPoint3d GetMin();
	CPoint3d GetMax();
	double GetSizeX() { return X_max - X_min; }
	double GetSizeY() { return Y_max - Y_min; }
	double GetSizeZ() { return Z_max - Z_min; }

};


class CVertex  
{
public:
	CPoint3d m_point;
//	CVector m_normal;
//	double u,v;
	CVertex(){}
	virtual ~CVertex(){}

};


class CTriangle;
	struct Hit {
		double dist;
		const CTriangle * tri;
		CPoint3d m_point;
		double u;
		double v;
	};

class CTriangle
{
public:
	CTriangle();
	virtual ~CTriangle();


		// defined
//		CVertex vtx[3];
		CPoint3d vtx[3];
		
		// calculated
		CVector n;//Это вектор перпендикулярный треугольнику
		CPoint3d u;
		CPoint3d v;
		double  d;
		int    pln;
		double  ux;
		double  uy;
		double  vx;
		double  vy;


	inline bool
	calcTriangle(CTriangle & tri)
	{
		sub(tri.u, tri.vtx[1], tri.vtx[0]);
		sub(tri.v, tri.vtx[2], tri.vtx[0]);

		tri.n.l = tri.u.y*tri.v.z - tri.u.z*tri.v.y;
		tri.n.m = tri.u.z*tri.v.x - tri.u.x*tri.v.z;
		tri.n.n = tri.u.x*tri.v.y - tri.u.y*tri.v.x;
		
		tri.n.Normalize();
		if ((fabs(tri.n.l) < DDELTA) && (fabs(tri.n.m) < DDELTA) && (fabs(tri.n.n) < DDELTA))
			return false;

		tri.d =tri.n.l*tri.vtx[0].x + tri.n.m*tri.vtx[0].y + tri.n.n*tri.vtx[0].z;

		double xy = tri.u.x*tri.v.y - tri.v.x*tri.u.y, axy = fabs(xy);
		double yz = tri.u.y*tri.v.z - tri.v.y*tri.u.z, ayz = fabs(yz);
		double zx = tri.u.z*tri.v.x - tri.v.z*tri.u.x, azx = fabs(zx);

		if (axy > ayz && axy > azx) {
			tri.pln = 1;
			tri.ux =  tri.v.y/xy;
			tri.uy = -tri.v.x/xy;
			tri.vx = -tri.u.y/xy;
			tri.vy =  tri.u.x/xy;
		}
		else if (ayz > azx) {
			tri.pln = 2;
			tri.ux =  tri.v.z/yz;
			tri.uy = -tri.v.y/yz;
			tri.vx = -tri.u.z/yz;
			tri.vy =  tri.u.y/yz;
		}
		else if (azx > 0) {
			tri.pln = 3;
			tri.ux =  tri.v.x/zx;
			tri.uy = -tri.v.z/zx;
			tri.vx = -tri.u.x/zx;
			tri.vy =  tri.u.z/zx;
		}

		return true;
	}

	inline bool
	intersect( CTriangle & tri,const CPoint3d & ray_orig,const CVector& ray_dir, Hit & hit)
	{
		const double a = tri.n.Dot( ray_dir);
		if (fabs(a) < DDELTA)
			return false;

		const double b = tri.n.DotP(ray_orig);
		const double dist = (tri.d - b) / a;

//		if (!(dist > (MIN_DIST*1000) && hit.dist > dist))
//			return false;

		CPoint3d hitPnt;
		hitPnt.x = ray_orig.x + dist * ray_dir.l;
		hitPnt.y = ray_orig.y + dist * ray_dir.m;
		hitPnt.z = ray_orig.z + dist * ray_dir.n;

		double hitu;
		double hitv;

		switch (tri.pln) {
		case 1:
			hitu = tri.ux*(hitPnt.x - tri.vtx[0].x) + tri.uy*(hitPnt.y - tri.vtx[0].y);
			hitv = tri.vx*(hitPnt.x - tri.vtx[0].x) + tri.vy*(hitPnt.y - tri.vtx[0].y);
			break;
		case 2:
			hitu = tri.ux*(hitPnt.y - tri.vtx[0].y) + tri.uy*(hitPnt.z - tri.vtx[0].z);
			hitv = tri.vx*(hitPnt.y - tri.vtx[0].y) + tri.vy*(hitPnt.z - tri.vtx[0].z);
			break;
		case 3:
			hitu = tri.ux*(hitPnt.z - tri.vtx[0].z) + tri.uy*(hitPnt.x - tri.vtx[0].x);
			hitv = tri.vx*(hitPnt.z - tri.vtx[0].z) + tri.vy*(hitPnt.x - tri.vtx[0].x);
			break;
		default:
			return false;
		}

		if ((hitu >= 0) && (hitv >= 0) && (hitv + hitu <= 1)) {
			hit.dist = dist;
			hit.tri = &tri;
			hit.m_point = hitPnt;
			hit.u = hitu;
			hit.v = hitv;
			return true;
		}
		
		return false;
	}

	CTriangle& operator =(const CTriangle& p)
	{
		if (&p == this)
			return *this;

		vtx[0] = p.vtx[0];
		vtx[1] = p.vtx[1];
		vtx[2] = p.vtx[2];
		n = p.n;
		u = p.u;
		v = p.v;
		d = p.d;
		pln = p.pln;
		ux = p.ux;
		uy = p.uy;
		vx = p.vx;
		vy = p.vy;
		return *this;
	}


};


#endif // !defined(AFX_TRIANGLE_H__3581364D_F601_491C_B765_2777DABFB7F8__INCLUDED_)
