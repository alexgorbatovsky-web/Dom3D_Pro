#include "SketchArcLine.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kEpsilon = 1.0e-10;
constexpr double kTwoPi = 6.28318530717958647692;

double positive_angle(double angle) {
    while (angle < 0.0) angle += kTwoPi;
    while (angle >= kTwoPi) angle -= kTwoPi;
    return angle;
}
}

CSketchArcLine::CSketchArcLine(CPoint3d start, CPoint3d point_on_arc, CPoint3d end)
    : CLinkLine(start, end), point_on_arc_(point_on_arc) {
    point_on_arc_.z = 0.0;
}

LinkLineType CSketchArcLine::GetType() const { return LinkLineType::Arc; }

std::unique_ptr<CLinkLine> CSketchArcLine::Clone() const {
    auto result = std::make_unique<CSketchArcLine>(GetStart(), point_on_arc_, GetEnd());
    result->SetID(GetID());
    return result;
}

bool CSketchArcLine::Circle(double& cx, double& cy, double& radius,
                            double& start_angle, double& sweep) const {
    const CPoint3d& a = GetStart();
    const CPoint3d& b = point_on_arc_;
    const CPoint3d& c = GetEnd();
    const double determinant = 2.0 * (a.x * (b.y - c.y)
        + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (std::abs(determinant) <= kEpsilon) return false;
    const double aa = a.x * a.x + a.y * a.y;
    const double bb = b.x * b.x + b.y * b.y;
    const double cc = c.x * c.x + c.y * c.y;
    cx = (aa * (b.y - c.y) + bb * (c.y - a.y) + cc * (a.y - b.y)) / determinant;
    cy = (aa * (c.x - b.x) + bb * (a.x - c.x) + cc * (b.x - a.x)) / determinant;
    radius = std::hypot(a.x - cx, a.y - cy);
    if (radius <= kEpsilon) return false;
    start_angle = std::atan2(a.y - cy, a.x - cx);
    const double middle_angle = std::atan2(b.y - cy, b.x - cx);
    const double end_angle = std::atan2(c.y - cy, c.x - cx);
    const double ccw_end = positive_angle(end_angle - start_angle);
    const double ccw_middle = positive_angle(middle_angle - start_angle);
    sweep = ccw_middle <= ccw_end ? ccw_end : ccw_end - kTwoPi;
    return std::abs(sweep) > kEpsilon;
}

bool CSketchArcLine::IsValid() const {
    double cx, cy, radius, start, sweep;
    return Circle(cx, cy, radius, start, sweep);
}

double CSketchArcLine::GetLength() const {
    double cx, cy, radius, start, sweep;
    return Circle(cx, cy, radius, start, sweep)
        ? radius * std::abs(sweep) : CLinkLine::GetLength();
}

CPoint3d CSketchArcLine::GetPoint(double parameter) const {
    double cx, cy, radius, start, sweep;
    if (!Circle(cx, cy, radius, start, sweep)) return CLinkLine::GetPoint(parameter);
    const double angle = start + sweep * std::clamp(parameter, 0.0, 1.0);
    return {cx + radius * std::cos(angle), cy + radius * std::sin(angle), 0.0};
}

CPoint3d CSketchArcLine::GetTangent(double parameter) const {
    double cx, cy, radius, start, sweep;
    if (!Circle(cx, cy, radius, start, sweep)) return CLinkLine::GetTangent(parameter);
    const double angle = start + sweep * std::clamp(parameter, 0.0, 1.0);
    return {-radius * std::sin(angle) * sweep,
             radius * std::cos(angle) * sweep, 0.0};
}

std::vector<CPoint3d> CSketchArcLine::Sample(std::size_t segments) const {
    segments = std::max<std::size_t>(segments, 8);
    std::vector<CPoint3d> result;
    result.reserve(segments + 1);
    for (std::size_t i = 0; i <= segments; ++i) {
        result.push_back(GetPoint(static_cast<double>(i) / static_cast<double>(segments)));
    }
    return result;
}

const CPoint3d& CSketchArcLine::GetPointOnArc() const { return point_on_arc_; }
void CSketchArcLine::SetPointOnArc(CPoint3d point) {
    point.z = 0.0;
    point_on_arc_ = point;
}
