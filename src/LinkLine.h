#pragma once

#include "Point3d.h"

#include <cstddef>
#include <memory>
#include <vector>

enum class LinkLineType {
    Segment,
    Horizontal,
    Vertical,
    Bezier,
    Arc
};

class CLinkLine {
public:
    CLinkLine(CPoint3d start, CPoint3d end);
    CLinkLine(CPoint3d* start, CPoint3d* end);
    virtual ~CLinkLine() = default;

    virtual LinkLineType GetType() const;
    virtual std::unique_ptr<CLinkLine> Clone() const;

    const CPoint3d& GetStart() const;
    const CPoint3d& GetEnd() const;
    CPoint3d* P(int index);
    const CPoint3d* P(int index) const;
    CPoint3d* PLast();
    const CPoint3d* PLast() const;
    int np() const;

    virtual void SetStart(CPoint3d point);
    virtual void SetEnd(CPoint3d point);
    virtual void EnforceGeometry();

    virtual double GetLength() const;
    virtual CPoint3d GetPoint(double parameter) const;
    virtual CPoint3d GetTangent(double parameter) const;
    virtual std::vector<CPoint3d> Sample(std::size_t segments = 1) const;
    double DistanceToPoint2D(const CPoint3d& point) const;
    void Translate(const CPoint3d& delta);
    void TransformLocal(const CPoint3d& origin,
                        const CPoint3d& x_axis,
                        const CPoint3d& y_axis,
                        const CPoint3d& z_axis);

    std::size_t GetID() const;
    void SetID(std::size_t id);

private:
    CPoint3d points_[2];
    std::size_t id_ = 0;
};
