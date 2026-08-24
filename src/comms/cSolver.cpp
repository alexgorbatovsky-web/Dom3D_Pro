#include "comms.h"

namespace comms {

//----------------------------------------------------------------------------------
// cSolver::GetRoots2
//----------------------------------------------------------------------------------
int cSolver::GetRoots2(const float a, const float b, const float c, float *Roots2) {
	if(a == 0.0f) {
		if(b == 0.0f) {
			if(c == 0.0f) {
				return -3; // Infinite number of roots (a == b == c == 0)
			}
			return -2; // No roots (a == b == 0)
		}
		Roots2[0] = -c / b;
		return -1; // One real root (a == 0)
	}
	float d = b * b - 4.0f * a * c;
	if(0.0f == d) {
		Roots2[0] = -b / (2.0f * a);
		return 1; // One real root (d == 0)
	}
	float t;
	if(d < 0.0f) { // Two conjugate complex roots (d < 0)
		t = 0.5f / a;
		Roots2[0] = -b * t;
		Roots2[1] = cMath::Sqrt(-d) * t;
		return 0;
	}
	// Two real roots
	if(b >= 0.0f) {
		d = -0.5f * (b + cMath::Sqrt(d));
	} else {
		d = -0.5f * (b - cMath::Sqrt(d));
	}
	Roots2[0] = d / a;
	Roots2[1] = c / d;
	return 2;
} // cSolver::GetRoots2

//-------------------------------------------------------------------------------------------------------------------
// cSolver::GetLowestPositiveRoot2
//-------------------------------------------------------------------------------------------------------------------
bool cSolver::GetLowestPositiveRoot2(const float a, const float b, const float c, const float MaxRoot, float *Root) {
	float r[2];
	int l = GetRoots2(a, b, c, r);
	if(l <= 0) {
		return false;
	}
	if(1 == l) {
		r[1] = r[0];
	}

	if(r[0] > r[1]) {
		cMath::Swap(r[0], r[1]);
	}
	if(r[0] > 0.0f && r[0] < MaxRoot) {
		*Root = r[0];
		return true;
	}
	if(r[1] > 0.0f && r[1] < MaxRoot) {
		*Root = r[1];
		return true;
	}
	return false;
} // cSolver::GetLowestPositiveRoot2

//----------------------------------------------------------------------------------
// cSolver::GetRoots3
//----------------------------------------------------------------------------------
int cSolver::GetRoots3(const float a, const float b, const float c, float *Roots3) {
	float q = (a * a - 3.0f * b) / 9.0f;
	float r = (a * (2.0f * a * a - 9.0f * b) + 27.0f * c) / 54.0f;
	float r2 = r * r;
	float q3 = q * q * q;
	float aOver3 = a / 3.0f;
	float t, aa, bb, Im;
	if(r2 <= q3) {
		t = cMath::ACos(r / cMath::Sqrt(q3));
		q = -2.0f * cMath::Sqrt(q);
		Roots3[0] = q * cMath::Cos(t / 3.0f) - aOver3;
		Roots3[1] = q * cMath::Cos((t + cMath::TwoPi) / 3.0f) - aOver3;
		Roots3[2] = q * cMath::Cos((t - cMath::TwoPi) / 3.0f) - aOver3;
		return 3; // Three real roots
	} else {
		if(r < 0.0f) {
			r = -r;
		}
		aa = -cMath::Pow(r + cMath::Sqrt(r2 - q3), 1.0f / 3.0f);
		bb = 0.0f;
		if(aa != 0.0f) {
			bb = q / aa;
		}
		q = aa + bb;
		r = aa - bb;
		Roots3[0] = q - aOver3;
		Roots3[1] = -0.5f * q - aOver3;
		Im = cMath::Sqrt(3.0f) * 0.5f * cMath::Abs(r);
		if(0.0f == Im) {
			return 2; // One real root + one complex (imaginary part is zero, i.e. two real roots)
		}
		Roots3[2] = Im;
		return 1; // One real root + two complex
	}
} // cSolver::GetRoots3

//-----------------------------------------------------------------------------------
// cSolver::EigenSolve
//-----------------------------------------------------------------------------------
bool cSolver::EigenSolve(const cMat3 &S, float *EigenValues3, cVec3 *EigenVectors3) {
	int i;
	float q[3];
	cVec3 y[3], u;
	float l[3];
	float a = -(S[0][0] + S[1][1] + S[2][2]);
	float b = S[0][0] * S[1][1] + S[0][0] * S[2][2] + S[1][1] * S[2][2] -
				S[1][2] * S[2][1] - S[0][1] * S[1][0] - S[0][2] * S[2][0];
	float c = -(S[0][0] * S[1][1] * S[2][2] - S[0][0] * S[1][2] * S[2][1] -
				S[0][1] * S[1][0] * S[2][2] + S[0][1] * S[1][2] * S[2][0] +
				S[0][2] * S[1][0] * S[2][1] - S[0][2] * S[1][1] * S[2][0]);
	int r = GetRoots3(a, b, c, l);
	if(r != 3) { // We need three real roots
		return false;
	}
	if(EigenValues3 != nullptr) {
		EigenValues3[0] = l[0];
		EigenValues3[1] = l[1];
		EigenValues3[2] = l[2];
	}
	// Eigen Vectors
	y[0].Set(0.0f, 0.0f, 1.0f);
	y[1] = y[0] * S;
	y[2] = y[1] * S;
	for(i = 0; i < 3; i++) {
		// Horner's method
		q[0] = 1.0f;
		q[1] = l[i] * q[0] + a;
		q[2] = l[i] * q[1] + b;
		u = q[0] * y[2] + q[1] * y[1] + q[2] * y[0];
		u.Normalize();
		if(EigenVectors3 != nullptr) {
			EigenVectors3[i] = u;
		}
	}
	return true;
} // cSolver::EigenSolve

//-----------------------------------------------------------------------------------------------------------------------------
// cSolver::ApproximatePoints
//-----------------------------------------------------------------------------------------------------------------------------
bool cSolver::ApproximatePoints(const cVec3 *Points, const int Count, cVec3 *Center, cVec3 *Normal, const float SpaceEpsilon) {
	cAssert(Center != nullptr);
	cAssert(Normal != nullptr);

	Center->SetZero();
	Normal->SetZero();
	
	if(Count < 3) {
		return false;
	}

	int i, j, k;
	float s;
	
	// Center
	for(i = 0; i < Count; i++) {
		*Center += Points[i];
	}
	*Center /= float(Count);

	// Bounds
	cBounds BB = cBounds::FromPoints(Points, Count);
	cVec3 b = BB.GetMax() - BB.GetMin();
	if(cMath::IsZero(b.x, SpaceEpsilon)) { // Plane YZ
		*Normal = cVec3::AxisX;
		return true;
	} else if(cMath::IsZero(b.y, SpaceEpsilon)) { // Plane XZ
		*Normal = cVec3::AxisY;
		return true;
	} else if(cMath::IsZero(b.z, SpaceEpsilon)) { // Plane XY
		*Normal = cVec3::AxisZ;
		return true;
	}
	
	// M
	cMat3 M(cMat3::RowsCtor, *Center, cVec3::Zero, cVec3::Zero);
	M = (float)Count * M * cMat3::Transpose(M);
	for(i = 0; i < 3; i++) {
		for(j = 0; j < 3; j++) {
			s = 0.0f;
			for(k = 0; k < Count; k++) {
				s += Points[k][i] * Points[k][j];
			}
			M(i, j) -= s;
		}
	}
	
	cAssert(M.IsSymmetric());
	// Eigen values
	float EigenValues[3];
	cVec3 EigenVectors[3];
	if(EigenSolve(M, EigenValues, EigenVectors)) {
		*Normal = EigenVectors[cMath::MaxIndex(EigenValues[0], EigenValues[1], EigenValues[2])];
		return true;
	}
	return false;
} // cSolver::ApproximatePoints

// cSolver::PointInPolygon
bool cSolver::PointInPolygon(const cList<cVec2> &Polygon, const cVec2 &Point) {
	if(Polygon.Count() <= 1) {
		return false;
	}
	int IntersectionsCount = 0;
	int PrevIndex = Polygon.Count() - 1;
	bool PrevUnder = Polygon[PrevIndex].y < Point.y;
	bool CurUnder;
	cVec2 u, v;
	float t;
	int i;
	for(i = 0; i < Polygon.Count(); i++) {
		CurUnder = Polygon[i].y < Point.y;
		u = Polygon[i] - Polygon[PrevIndex];
		v = Point - Polygon[PrevIndex];
		t = cVec2::Ccw(u, v);
		if(PrevUnder && !CurUnder) {
			if(t > 0.0f) {
				IntersectionsCount++;
			}
		}
		if(!PrevUnder && CurUnder) {
			if(t < 0.0f) {
				IntersectionsCount++;
			}
		}
		PrevIndex = i;
		PrevUnder = CurUnder;
	}
	return (IntersectionsCount & 1) != 0;
}

} // comms
