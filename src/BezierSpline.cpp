#include "BezierSpline.h"

#include <algorithm>
#include <cmath>

CBezierSpline::CBezierSpline(CPoint3d start,
                             CPoint3d control1,
                             CPoint3d control2,
                             CPoint3d end)
    : CLinkLine(start, end),
      control1_(control1-start),
      control2_(control2-end) {
}

LinkLineType CBezierSpline::GetType() const {
    return LinkLineType::Bezier;
}

std::unique_ptr<CLinkLine> CBezierSpline::Clone() const {
    auto result = std::make_unique<CBezierSpline>(
        GetStart(), GetControl1(), GetControl2(), GetEnd());
    result->SetID(GetID());
    return result;
}

double CBezierSpline::GetLength() const {
    const std::vector<CPoint3d> points = Sample(64);
    double length = 0.0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        const double dx = points[index].x - points[index - 1].x;
        const double dy = points[index].y - points[index - 1].y;
        const double dz = points[index].z - points[index - 1].z;
        length += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return length;
}

CPoint3d CBezierSpline::GetPoint(double parameter) const {
    const double t = std::clamp(parameter, 0.0, 1.0);
    const double u = 1.0 - t;
    const double b0 = u * u * u;
    const double b1 = 3.0 * u * u * t;
    const double b2 = 3.0 * u * t * t;
    const double b3 = t * t * t;
    const CPoint3d& start = GetStart();
    const CPoint3d& end = GetEnd();
    return {
        b0 * start.x + b1 * GetControl1().x + b2 * GetControl2().x + b3 * end.x,
        b0 * start.y + b1 * GetControl1().y + b2 * GetControl2().y + b3 * end.y,
        b0 * start.z + b1 * 0.0 + b2 * 0.0 + b3 * end.z
    };
}

CPoint3d CBezierSpline::GetTangent(double parameter) const {
    const double t = std::clamp(parameter, 0.0, 1.0);
    const double u = 1.0 - t;
    const CPoint3d& start = GetStart();
    const CPoint3d& end = GetEnd();
    return {
        3.0 * u * u * (GetControl1().x - start.x)
            + 6.0 * u * t * (GetControl2().x - GetControl1().x)
            + 3.0 * t * t * (end.x - GetControl2().x),
        3.0 * u * u * (GetControl1().y - start.y)
            + 6.0 * u * t * (GetControl2().y - GetControl1().y)
            + 3.0 * t * t * (end.y - GetControl2().y),
        3.0 * u * u * (0.0 - start.z)
            + 6.0 * u * t * (0.0 - 0.0)
            + 3.0 * t * t * (end.z - 0.0)
    };
}

double CBezierSpline::GetNearestParameter(const CPoint3d& point) const {
    const auto distance_squared = [this, &point](double parameter) {
        const CPoint3d candidate = GetPoint(parameter);
        const double dx = candidate.x - point.x;
        const double dy = candidate.y - point.y;
        const double dz = candidate.z - point.z;
        return dx * dx + dy * dy + dz * dz;
    };

    constexpr int coarse_steps = 32;
    double best = 0.0;
    double best_distance = distance_squared(0.0);
    for (int step = 1; step <= coarse_steps; ++step) {
        const double parameter =
            static_cast<double>(step) / coarse_steps;
        const double distance = distance_squared(parameter);
        if (distance < best_distance) {
            best_distance = distance;
            best = parameter;
        }
    }
    double left = std::max(0.0, best - 1.0 / coarse_steps);
    double right = std::min(1.0, best + 1.0 / coarse_steps);
    for (int iteration = 0; iteration < 24; ++iteration) {
        const double first = left + (right - left) / 3.0;
        const double second = right - (right - left) / 3.0;
        if (distance_squared(first) <= distance_squared(second)) {
            right = second;
        } else {
            left = first;
        }
    }
    return (left + right) * 0.5;
}

std::vector<CPoint3d> CBezierSpline::Sample(std::size_t segments) const {
    segments = std::max<std::size_t>(segments, 4);
    std::vector<CPoint3d> points;
    points.reserve(segments + 1);
    for (std::size_t index = 0; index <= segments; ++index) {
        points.push_back(GetPoint(
            static_cast<double>(index) / static_cast<double>(segments)));
    }
    return points;
}

CPoint3d CBezierSpline::GetControl1() const {
    return GetStart() + static_cast<CPoint3d>(control1_);
}

CPoint3d CBezierSpline::GetControl2() const {
    return GetEnd() + static_cast<CPoint3d>(control2_);
}

void CBezierSpline::SetControl1(CPoint3d point) {
    const SketchPoint value(point-GetStart());
    if (value.u == control1_.u && value.v == control1_.v) return;
    GeometryChanged(); control1_ = value;
}

void CBezierSpline::SetControl2(CPoint3d point) {
    const SketchPoint value(point-GetEnd());
    if (value.u == control2_.u && value.v == control2_.v) return;
    GeometryChanged(); control2_ = value;
}

void CBezierSpline::SetStart(CPoint3d point) { CLinkLine::SetStart(point); }
void CBezierSpline::SetEnd(CPoint3d point) { CLinkLine::SetEnd(point); }
