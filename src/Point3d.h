///********************************Point3d.h*****************************/
#ifndef _LIB_POINT3D_AG_H
#define _LIB_POINT3D_AG_H

#include <vector>

#include "legacy_geometry_defs.h"
#include "Vector.h"

class CVector;
class CPlane;
class CPoint7d;
class CPoint8d;
class CSystemCoord;
class CView3d;
struct LINE_2P;
struct CSizeBlock;
struct CVertex5f;
struct cVec2;

struct CPoint6d
{
	double x, y, z;
	double w, t, s;
};

class CPoint3d
{
public:
	double x, y, z;

	// Constructors
	CPoint3d() { x = y = z = 0; }
	CPoint3d(double xi, double yi, double zi) { x = xi; y = yi; z = zi; }
	CPoint3d(CPoint7d*);
	CPoint3d(CPoint3d*);
	CPoint3d(CPoint8d*);


//	void Read(const char* str);

	void Move(CPoint3d* p1, CPoint3d* p2);
	void Move(CVector* dir, double dist);

	void Rotate(CPoint3d*, CPoint3d*, double angle);
	void Rotate(CPoint3d* p0, CVector* v, double alfa);
	void Zoom(CPoint3d* pf, double kx, double ky, double kz);
	void Mirror();
	BOOL InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);

//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CPoint3d& p);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CPoint3d& p);

	void print(FILE* strm = stderr);
	void print(const char* text, FILE* strm);

	void GetGrPoz(CView3d* view, CPoint3d* p);
	void GetAgrPoz(CView3d* view, CPoint3d* p);
	void SetGrPoz(CView3d* view);
	void SetAgrPoz(CView3d* view);

	void Shift(CVector* vect, double dist, CPoint3d* p);
	void Shift(CVector* vect, double dist, CPoint8d* p);
	BOOL Cross2DLine(CPoint3d* p11, CPoint3d* p12, CPoint3d* p21, CPoint3d* p22);
	BOOL Offset(double offset, CPoint3d* p1, CPoint3d* p2);
//	void GetOffsetPoint(LINE_2P* line, double offset, CPoint3d* p);
	double GetDistLine(CPoint3d* p1, CPoint3d* p2);
	double GetDistLine(CPoint3d* p, CVector* line);
	double DistTo(CPoint3d* p2);
	int In_P1_P2(CPoint3d* p1, CPoint3d* p2);
	bool IsPointIn(CSizeBlock* block);
	double GetDistLineSegment(CPoint3d* p1, CPoint3d* p2);
	double GetDistLineSegment(CVertex5f& p1, CVertex5f& p2);
	float LengthSq() const;
	float Length() const;
	float Normalize();
	bool IsPointInSegment(CPoint3d& p1, CPoint3d& p2);

	float dot(const CPoint3d& u) const {
		return float(u.x * x + u.y * y + u.z * z);
	}

	const CPoint3d operator + (const CPoint3d&) const;
	const CPoint3d operator - (const CPoint3d&) const;
	const CPoint3d operator * (const CPoint3d&) const;
	const CPoint3d operator * (const float) const;

	void operator += (const CPoint3d&);
	void operator -= (const CPoint3d&);
	void operator *= (const CPoint3d&);
	void operator *= (const float);
	void operator /= (const CPoint3d&);
	void operator /= (const float);

	static const CPoint3d Cross(const CPoint3d&, const CPoint3d&);


	static const CPoint3d AxisX;
	static const CPoint3d AxisY;
	static const CPoint3d AxisZ;
	static const CPoint3d AxisNegX;
	static const CPoint3d AxisNegY;
	static const CPoint3d AxisNegZ;

};

inline float CPoint3d::Normalize() {
	const float l = Length();
	if (l > 0.0f) {
		const float il = 1.0f / l;
		x *= il;
		y *= il;
		z *= il;
	}
	return l;
}
inline float CPoint3d::Length() const {
	return float(sqrt(x * x + y * y + z * z));
}

inline float CPoint3d::LengthSq() const {
	return float(x * x + y * y + z * z);
}

inline const CPoint3d CPoint3d::operator + (const CPoint3d& u) const {
	return CPoint3d(x + u.x, y + u.y, z + u.z);
}

inline const CPoint3d CPoint3d::operator - (const CPoint3d& u) const {
	return CPoint3d(x - u.x, y - u.y, z - u.z);
}

inline const CPoint3d CPoint3d::operator * (const CPoint3d& u) const {
	return CPoint3d(x * u.x, y * u.y, z * u.z);
}

inline const CPoint3d CPoint3d::operator * (const float s) const {
	return CPoint3d(x * s, y * s, z * s);
}


inline void CPoint3d::operator += (const CPoint3d& u) {
	x += u.x;
	y += u.y;
	z += u.z;
}

// CPoint3d::operator -= : void (const CPoint3d &)
inline void CPoint3d::operator -= (const CPoint3d& u) {
	x -= u.x;
	y -= u.y;
	z -= u.z;
}

// CPoint3d::operator *= : void (const CPoint3d &)
inline void CPoint3d::operator *= (const CPoint3d& u) {
	x *= u.x;
	y *= u.y;
	z *= u.z;
}

// CPoint3d::operator *= : void (const float)
inline void CPoint3d::operator *= (const float s) {
	x *= s;
	y *= s;
	z *= s;
}

// CPoint3d::operator /= : void (const CPoint3d &)
inline void CPoint3d::operator /= (const CPoint3d& u) {
	x /= u.x;
	y /= u.y;
	z /= u.z;
}

// CPoint3d::operator /= : void (const float)
inline void CPoint3d::operator /= (const float s) {
	const float is = 1.0f / s;
	x *= is;
	y *= is;
	z *= is;
}



inline double
dot(const CPoint3d& p1, const CPoint3d& p2)
{
	return (p1.x * p2.x + p1.y * p2.y + p1.z * p2.z);
}

inline void
cross(CPoint3d& dest, const CPoint3d& p1, const CPoint3d& p2)
{
	dest.x = p1.y * p2.z - p1.z * p2.y;
	dest.y = p1.z * p2.x - p1.x * p2.z;
	dest.z = p1.x * p2.y - p1.y * p2.x;
}

inline void
sub(CPoint3d& dest, const CPoint3d& p1, const CPoint3d& p2)
{
	dest.x = p1.x - p2.x;
	dest.y = p1.y - p2.y;
	dest.z = p1.z - p2.z;
}

inline void
add(CPoint3d& dest, const CPoint3d& p1, const CPoint3d& p2)
{
	dest.x = p1.x + p2.x;
	dest.y = p1.y + p2.y;
	dest.z = p1.z + p2.z;
}

inline void
mult(CPoint3d& dest, const CPoint3d& p, const double val)
{
	dest.x = p.x * val;
	dest.y = p.y * val;
	dest.z = p.z * val;
}

inline double
dist(const CPoint3d& p1, const CPoint3d& p2, double& distSquare)
{
	double x = p2.x - p1.x;
	double y = p2.y - p1.y;
	double z = p2.z - p1.z;
	distSquare = (x * x + y * y + z * z);
	return sqrt(distSquare);
}

class CPoint8d
{
public:
	double x, y, z, l, m, n, s, t;

	// Constructors
	CPoint8d() { x = y = z = l = m = n = s = t = 0; }
	CPoint8d(double xi, double yi, double zi) { x = xi; y = yi; z = zi; l = m = n = s = t = 0; }

	void Move(CPoint3d* p1, CPoint3d* p2);
	void Move(CVector* dir, double dist);

	void Rotate(CPoint3d*, CPoint3d*, double angle);
	void Zoom(CPoint3d* pf, double kx, double ky, double kz);
	void Mirror();
	BOOL InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);
//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CPoint8d& p);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CPoint8d& p);

	void GetGrPoz(CView3d* view, CPoint3d* p);
	void GetAgrPoz(CView3d* view, CPoint3d* p);
	void SetGrPoz(CView3d* view);
	void SetAgrPoz(CView3d* view);


	void print(FILE* strm = stderr);
	void print(LPCTSTR text, FILE* strm);

	void Read(LPCTSTR str);
	void Shift(double dist, CPoint3d* p);
	void Shift(CVector* vect, double dist, CPoint3d* p);
	void Shift(CVector* vect, double dist, CPoint8d* p);
	void Revers();
	double DistTo(CPoint3d* p2);
	int In_P1_P2(CPoint3d* p1, CPoint3d* p2);

};

class CPoint8f
{
public:
	float x, y, z, l, m, n, s, t;

	// Constructors
	CPoint8f() { x = y = z = l = m = n = s = t = 0; }
	CPoint8f(float xi, float yi, float zi) { x = xi; y = yi; z = zi; l = m = n = s = t = 0; }

	void Move(CPoint3d* p1, CPoint3d* p2);
	void Move(CVector* dir, double dist);

	void Rotate(CPoint3d*, CPoint3d*, double angle);
	void Zoom(CPoint3d* pf, double kx, double ky, double kz);
	void Mirror();
	BOOL InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);
//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CPoint8f& p);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CPoint8f& p);

	void GetGrPoz(CView3d* view, CPoint3d* p);
	void GetAgrPoz(CView3d* view, CPoint3d* p);
	void SetGrPoz(CView3d* view);
	void SetAgrPoz(CView3d* view);


	void print(FILE* strm = stderr);
	void print(LPCTSTR text, FILE* strm);

	void Read(LPCTSTR str);
	void Shift(double dist, CPoint3d* p);
	void Shift(CVector* vect, double dist, CPoint3d* p);
	void Shift(CVector* vect, double dist, CPoint8d* p);
	void Revers();
	double DistTo(CPoint3d* p2);
	int In_P1_P2(CPoint3d* p1, CPoint3d* p2);

};

class CPoint4d
{
public:
	double x, y, z, v;
	char p;

	// Constructors
	CPoint4d() { x = y = z = v = 0; p = 0; }
	CPoint4d(double xi, double yi, double zi) { x = xi; y = yi; z = zi; v = 0; p = 0; }

	void Move(CPoint3d* p1, CPoint3d* p2);
	void Move(CVector* dir, double dist);

	void Rotate(CPoint3d*, CPoint3d*, double angle);
	void Zoom(CPoint3d* pf, double kx, double ky, double kz);
	void Mirror();
	BOOL InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);

	void print(FILE* strm = stderr);
	void print(LPCTSTR text, FILE* strm);

	void GetGrPoz(CView3d* view, CPoint3d* p);
	void GetAgrPoz(CView3d* view, CPoint3d* p);
	void SetGrPoz(CView3d* view);
	void SetAgrPoz(CView3d* view);

	void Read(LPCTSTR str);
	void Shift(CVector* vect, double dist, CPoint3d* p);
	BOOL Offset(double offset, CPoint3d* p1, CPoint3d* p2);
	double DistTo(CPoint3d* p2);

};

// CPoint4ds will be sorted by v
inline bool operator<(const CPoint4d& p1, const CPoint4d& p2)
{
	return p1.v < p2.v;
}

class CPoint7d
{
public:
	double x, y, z, l, m, n, s;

	// Constructors
	CPoint7d() { x = y = z = l = m = n = s = 0; }
	CPoint7d(double xi, double yi, double zi) { x = xi; y = yi; z = zi; l = m = n = s = 0; }

	void Move(CPoint3d* p1, CPoint3d* p2);
	void Move(CVector* dir, double dist);

	void Rotate(CPoint3d*, CPoint3d*, double angle);
	void Zoom(CPoint3d* pf, double kx, double ky, double kz);
	void Mirror();
	BOOL InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);
//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CPoint7d& p);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CPoint7d& p);

	void GetGrPoz(CView3d* view, CPoint3d* p);
	void GetAgrPoz(CView3d* view, CPoint3d* p);
	void SetGrPoz(CView3d* view);
	void SetAgrPoz(CView3d* view);

	//void print(CMemFile* file);
	void print(FILE* strm = stderr);
	void print(LPCTSTR text, FILE* strm);

	void Read(LPCTSTR str);
	void Shift(CVector* vect, double dist, CPoint3d* p);
	void Shift(CVector* vect, double dist, CPoint7d* p);
	void Revers();
	double DistTo(CPoint3d* p2);
	double GetDistLine(CPoint3d* p1, CPoint3d* p2);
	double GetDistLine(CPoint3d* p, CVector* line);

};

class CPoint14d
{
public:
	Coord x, y, z;
	Coord lu, mu, nu;
	Coord lv, mv, nv;
	Coord luv, muv, nuv;
	Coord s, t;

	// Constructors
	CPoint14d() { x = y = z = lu = mu = nu = lv = mv = nv = luv = muv = nuv = s = t = 0; }
	CPoint14d(double xi, double yi, double zi) { x = xi; y = yi; z = zi; lu = mu = nu = lv = mv = nv = luv = muv = nuv = s = t = 0; }

	void Move(CPoint3d* p1, CPoint3d* p2);
	void Move(CVector* dir, double dist);

	void Rotate(CPoint3d*, CPoint3d*, double angle);
	void Zoom(CPoint3d* pf, double kx, double ky, double kz);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2, CView3d* view);
	BOOL InRect(CPoint3d* p1, CPoint3d* p2);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);

	void GetGrPoz(CView3d* view, CPoint3d* p);
	void GetAgrPoz(CView3d* view, CPoint3d* p);
	void SetGrPoz(CView3d* view);
	void SetAgrPoz(CView3d* view);


	void print(FILE* strm = stderr);
	void print(LPCTSTR text, FILE* strm);

	void Read(String str);
	void Mirror();
	void Shift(CVector* vect, double dist, CPoint3d* p);
	double DistTo(CPoint3d* p2);

};

struct CFace3Pr
{
	int m_index;// index of CFace3ds
	CVector norm;// Нормаль к этому треугольнику
	unsigned int num_vert;

	CFace3Pr() { m_index = 0; num_vert = 0; }
	CFace3Pr(int i, CVector v, unsigned int  nv) { m_index = i; norm = v; num_vert = nv; }
};


struct CVertex5f
{
	float x, y, z;
	float s, t;
	std::vector<CFace3Pr> FaceList;//List of linked triangles

	CVertex5f() { x = y = z = s = t = 0; }
	CVertex5f(float x, float y, float z);
	CVertex5f(const CPoint3d* p);
	CVertex5f(const CVertex5f& v);

	CVertex5f& operator =(const CVertex5f& p);
	bool operator >(const CVertex5f& p);
	void GetGrPoz(CView3d* pv, CPoint3d* pgr);

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CSystemCoord* sc);
	void mod_coord_ma(CSystemCoord* sc);

	void Rotate(CPoint3d* p0, CVector* dir, double alfa);

	void Move(CVector* dir, double dist);
	double GetDistLineSegment(CPoint3d* p1, CPoint3d* p2);
	double DistTo(CPoint3d* p2);
	double DistTo(CVertex5f& p2);
	int In_P1_P2(CPoint3d* p1, CPoint3d* p2);
	double GetDistLine(CPoint3d* p1, CPoint3d* p2);
	void Zoom(CPoint3d* pf, double Sx, double Sy, double Sz);
	void CalcST(CVertex5f& p1, CVertex5f& p2);

//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CVertex5f& p);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CVertex5f& p);


	static bool Equals(const CVertex5f&, const CVertex5f&, const float Eps = 0.000001);
	bool operator == (const CVertex5f&) const;

};
inline bool CVertex5f::operator == (const CVertex5f& u) const {
	return CVertex5f::Equals(*this, u);
}

inline bool CVertex5f::Equals(const CVertex5f& u, const CVertex5f& v, const float Eps) {
	if (fabs(u.x - v.x) > Eps) {
		return false;
	}
	if (fabs(u.y - v.y) > Eps) {
		return false;
	}
	if (fabs(u.z - v.z) > Eps) {
		return false;
	}
	return true;
}



struct CVector3f
{
	float l, m, n;
	CVector3f() { l = m = n = 0; }
	CVector3f(float li, float mi, float ni) { l = li; m = mi; n = ni; }

	void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
	void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
//	friend CArchive& AFXAPI operator<<(CArchive& ar, const CVector3f& p);
//	friend CArchive& AFXAPI operator>>(CArchive& ar, CVector3f& p);

	static bool Equals(const CVector3f&, const CVector3f&, const float Eps = 0.000001);
	bool operator == (const CVector3f&) const;

};

inline bool CVector3f::operator == (const CVector3f& u) const {
	return CVector3f::Equals(*this, u);
}
inline bool CVector3f::Equals(const CVector3f& u, const CVector3f& v, const float Eps) {
	if (fabs(u.l - v.l) > Eps) {
		return false;
	}
	if (fabs(u.m - v.m) > Eps) {
		return false;
	}
	if (fabs(u.n - v.n) > Eps) {
		return false;
	}
	return true;
}


#endif /* _LIB_POINT3D_AG_H */

