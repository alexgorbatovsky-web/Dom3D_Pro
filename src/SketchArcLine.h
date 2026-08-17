#pragma once

#include "LinkLine.h"

class CSketchArcLine final : public CLinkLine {
public:
    CSketchArcLine(CPoint3d start, CPoint3d point_on_arc, CPoint3d end);

    LinkLineType GetType() const override;
    std::unique_ptr<CLinkLine> Clone() const override;
    double GetLength() const override;
    CPoint3d GetPoint(double parameter) const override;
    CPoint3d GetTangent(double parameter) const override;
    std::vector<CPoint3d> Sample(std::size_t segments = 32) const override;

    const CPoint3d& GetPointOnArc() const;
    void SetPointOnArc(CPoint3d point);
    bool IsValid() const;

private:
    bool Circle(double& center_x, double& center_y,
                double& radius, double& start_angle, double& sweep) const;

    CPoint3d point_on_arc_;
};
