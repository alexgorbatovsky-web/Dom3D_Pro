#pragma once

#include "LinkLine.h"

class CBezierSpline final : public CLinkLine {
public:
    CBezierSpline(CPoint3d start,
                  CPoint3d control1,
                  CPoint3d control2,
                  CPoint3d end);

    LinkLineType GetType() const override;
    std::unique_ptr<CLinkLine> Clone() const override;
    double GetLength() const override;
    CPoint3d GetPoint(double parameter) const override;
    CPoint3d GetTangent(double parameter) const override;
    double GetNearestParameter(const CPoint3d& point) const;
    std::vector<CPoint3d> Sample(std::size_t segments = 32) const override;

    const CPoint3d& GetControl1() const;
    const CPoint3d& GetControl2() const;
    void SetControl1(CPoint3d point);
    void SetControl2(CPoint3d point);
    void SetStart(CPoint3d point) override;
    void SetEnd(CPoint3d point) override;

private:
    CPoint3d control1_;
    CPoint3d control2_;
};
