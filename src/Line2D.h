#pragma once

#include <vector>

static const double EPS2D = 0.001;

struct cVec2 {
    double x = 0.0;
    double y = 0.0;

    cVec2() = default;
    cVec2(double x_value, double y_value) : x(x_value), y(y_value) {}

    cVec2 operator+(const cVec2& other) const { return {x + other.x, y + other.y}; }
    cVec2 operator-(const cVec2& other) const { return {x - other.x, y - other.y}; }
    cVec2 operator*(double scale) const { return {x * scale, y * scale}; }
};

inline double Length2(const cVec2& value)
{
    return value.x * value.x + value.y * value.y;
}

inline bool EqualPoint2(const cVec2& a, const cVec2& b, double eps)
{
    return Length2(a - b) <= eps * eps;
}

struct Face2D {
    std::vector<cVec2> verts;
};

enum PointFacePos {
    PFP_OUTSIDE = 0,
    PFP_BOUNDARY = 1,
    PFP_INSIDE = 2
};

enum FaceCutCase {
    FACECUT_NONE = 0,
    FACECUT_SPLIT_2 = 2,
    FACECUT_SPLIT_3 = 3,
    FACECUT_COMPLEX = 100
};

struct CellCutHit {
    int edgeIndex = -1;
    int vertexIndex = -1;
    int segIndex = -1;
    double t = 0.0;
    double u = 0.0;
    cVec2 pt;
};

struct CellCutInfo {
    std::vector<CellCutHit> hits;
    std::vector<int> touchedFaceVertices;
    bool hasBorderOverlap = false;
    bool hasInteriorSegment = false;
    int boundaryContactCount = 0;
    int interiorNodeCount = 0;
    int branchInteriorNodeCount = 0;
    int VariantCut = 0;
    int NumCutEdge = 0;
    int edgeIndex1 = -1;
    int edgeIndex2 = -1;
    int NumCutVertex = -1;
    int vertexToMove = -1;
    cVec2 moveTarget;
    bool hasMoveTarget = false;
    FaceCutCase cutCase = FACECUT_NONE;
    std::vector<int> PntInFace;
    std::vector<int> PntTouchedFaceVertx; // index of the Cut nodes coinciding with the face vertices

//	int edgeIndex = -1;
};

void InitCellCutInfo(CellCutInfo& info);
PointFacePos ClassifyPointInFace2(const Face2D& face, const cVec2& point, double eps = EPS2D);
bool AnalyzeFaceCut(const Face2D& face, const std::vector<cVec2>& cutPolyInput, CellCutInfo& info, double eps = EPS2D);
void ClassifyFaceCut(const Face2D& face, const std::vector<cVec2>& cut, CellCutInfo& info, double eps = EPS2D);
