#pragma once

#include "Common.h"
#include "Point3d.h"

#include <cstddef>
#include <vector>

class CMesh3D;
class CSurfaceFace;

class CNet {
public:
    int Build(CSurfaceFace* mm, double delta);
    int BuildNetByTwoQty(CSurfaceFace* mm, int QtyS, int QtyT);
    bool BuildMesh3D(CMesh3D* m);

    CPoint8d* P(int s, int t);
    const CPoint8d* P(int s, int t) const;
    const std::vector<CPoint8d>& GetPoints() const { return points_; }
    int GetQtyS() const { return qty_s_; }
    int GetQtyT() const { return qty_t_; }
    void ReversPoints();

private:
    size_t Index(int s, int t) const;

    std::vector<CPoint8d> points_;
    int qty_s_ = 0;
    int qty_t_ = 0;
};
