#pragma once

//-----------------------------------------------------------------------------
// cSolver
//-----------------------------------------------------------------------------
class cSolver {
public:
	// GetRoots2: a * x ^ 2 + b * x + c = 0
	// Returns:
	// -3 - Infinite number of roots (a == b == c == 0)
	// -2 - No roots (a == b == 0)
	// -1 - One real root (a == 0)
	//  0 - Two conjugate complex roots
	//  1 - One real root (d == 0)
	//  2 - Two real roots
	static int GetRoots2(const float a, const float b, const float c, float *Roots2);
	static bool GetLowestPositiveRoot2(const float a, const float b, const float c, const float MaxRoot, float *Root);
	
	// GetRoots3: x ^ 3 + a * x ^ 2 + b * x + c = 0
	// Returns:
	// 3 - Three real roots
	// 2 - One real root + one complex (imaginary part is zero, i.e. two real roots)
	// 1 - One real root + two complex
	static int GetRoots3(const float a, const float b, const float c, float *Roots3);
	
	// Eigen values and eigen vectors of symmetric matrix
	static bool EigenSolve(const cMat3 &S, float *EigenValues3, cVec3 *EigenVectors3);
	
	static bool ApproximatePoints(const cVec3 *Points, const int Count, cVec3 *Center, cVec3 *Normal, const float SpaceEpsilon = 1.0f);
	
	static bool PointInPolygon(const cList<cVec2> &Polygon, const cVec2 &Point);
}; // cSolver
