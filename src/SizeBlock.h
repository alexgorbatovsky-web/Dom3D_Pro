#pragma once
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

