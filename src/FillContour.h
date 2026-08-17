#pragma once

#include "CMesh3D.h"

#include <vector>

using Vector3D = Vec3;

struct ContourPoint {
    Vec3 Pos{};
    Vec3 Normal{};
};

class ContourToFill {
public:
    bool ForceSnaping = false;
    bool InvertOrder = false;
    bool LockContourOrder = false;
    bool LockDirection = false;
    bool WasFlipped = false;
    int MaxAllowedTime = 20000;

    virtual ~ContourToFill() = default;

    virtual bool PlacePoint(Vec3& pt, Vec3& n);
    void FillByQuads(CMesh3D& mesh);
    void FillByTriangles(CMesh3D& mesh, int nSubd = 0);
    void Clear();
    void AddPoint(const Vec3& pos, const Vec3& normal);

private:
    std::vector<ContourPoint> points_;
};

class ContourQuadrangulator {
public:
    void CreateFromMesh(const CMesh3D* mesh);
    void Clear();
    void Quadrangulate(CMesh3D* res);

private:
    std::vector<Vec3> vertices_;
    std::vector<CMesh3D::Face> faces_;
};

bool FillContorByTriangles(CMesh3D* mesh, const std::vector<Vec3>& contour, Vec3 normal);
bool FillContourByTriangles(CMesh3D* mesh, const std::vector<Vec3>& contour, Vec3 normal);
