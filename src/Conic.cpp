#include "iges/SplineCurve.h"
#include "CPolyline.h"

#include "Conic.h"

#include <cmath>

CConic::CConic()
{

	m_f = 0.5;
}

CConic::CConic(CPoint3d* pa, CPoint3d* pb, CPoint3d* pc, double f)
{
	m_pa = *pa;
	m_pb = *pb;
	m_pc = *pc;
	m_f = f;
}


void CConic::GetPoint(double t, CPoint3d* pt)
{
	double p = (1 - m_f) / m_f;
	double q = (2 * m_f - 1) / pow(m_f, 2);
	double y0 = 0.5;

	if (m_f == 0.5)
		y0 = 0.5 * (1 - (2 * t - 1) * (2 * t - 1));
	else
		y0 = m_f - p * (sqrt(1 + (q / pow(p, 2)) * pow(2 * t - 1, 2)) - 1) / q;
	double xd = (m_pa.x + m_pc.x) / 2.0;
	double yd = (m_pa.y + m_pc.y) / 2.0;
	double zd = (m_pa.z + m_pc.z) / 2.0;
	pt->x = m_pa.x + (m_pc.x - m_pa.x) * t + (m_pb.x - xd) * y0;
	pt->y = m_pa.y + (m_pc.y - m_pa.y) * t + (m_pb.y - yd) * y0;
	pt->z = m_pa.z + (m_pc.z - m_pa.z) * t + (m_pb.z - zd) * y0;

}

void CConic::GetPointVector(double s, CPoint3d* p, CVector* v)
{
	double delta = 0.00001;

	GetPoint(s, p);

	if (s < delta) {
		v->calc(&m_pa, &m_pb);
		return;
	}
	if ((1 - s) < delta) {
		v->calc(&m_pb, &m_pc);
		return;
	}

	CPoint3d p2;
	GetPoint(s + delta, &p2);

	v->calc(p, &p2);
}

CSplineCurve* CConic::MakeSpline(int num)
{
	if (num < 2)
		return NULL;
	double ds = 1 / double(num - 1);

	CSplineCurve* spline = new CSplineCurve;
	if (spline->Realloc(num))
		return NULL;
	for (int i = 0; i < num; i++)
		GetPointVector(ds * i, spline->Pnt(i), (CVector*)&spline->P(i)->l);
	if (!spline->Update())
		return NULL;
	return spline;
}

CPolyline* CConic::MakeLine(double dopusk)
{
	if (dopusk < 0.001)
		dopusk = 0.001;
	double  ds = 0.05;
	CPoint3d p1;
	CPoint3d p2;
	CVector vect;
	GetPointVector(0.5, &p1, &vect);
	GetPoint(0.5 + ds, &p2);
	double delta = p2.GetDistLine(&p1, &vect);
	//////////Вначале определим шаг по параметру -ds
	while (delta > dopusk && ds > 0.000001) {
		ds = ds / 2;
		GetPoint(0.5 + ds, &p2);
		delta = p2.GetDistLine(&p1, &vect);
	}
	int	np = (int)(1 / ds);	/*	number step	*/
	CPolyline* line = new CPolyline("Conic Polyline");
	for (int i = 0; i <= np; i++) {
		CPoint3d point;
		GetPoint(ds * i, &point);
		line->AddPoint(point);
	}
	return line;
}
