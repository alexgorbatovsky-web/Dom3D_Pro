#include "LinkLine.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kGeometryEpsilon = 1.0e-12;

CPoint3d transform_point(const CPoint3d& point,
                         const CPoint3d& origin,
                         const CPoint3d& x_axis,
                         const CPoint3d& y_axis,
                         const CPoint3d& z_axis) {
    return CPoint3d(
        origin.x + point.x * x_axis.x + point.y * y_axis.x + point.z * z_axis.x,
        origin.y + point.x * x_axis.y + point.y * y_axis.y + point.z * z_axis.y,
        origin.z + point.x * x_axis.z + point.y * y_axis.z + point.z * z_axis.z);
}
}

CLinkLine::CLinkLine(CPoint3d start, CPoint3d end)
    : points_{start, end} {
}

CLinkLine::CLinkLine(CPoint3d* start, CPoint3d* end)
    : CLinkLine(start ? *start : CPoint3d(), end ? *end : CPoint3d()) {
}

LinkLineType CLinkLine::GetType() const {
    return LinkLineType::Segment;
}

std::unique_ptr<CLinkLine> CLinkLine::Clone() const {
    auto result = std::make_unique<CLinkLine>(points_[0], points_[1]);
    result->SetID(id_);
    return result;
}

const CPoint3d& CLinkLine::GetStart() const {
    return points_[0];
}

const CPoint3d& CLinkLine::GetEnd() const {
    return points_[1];
}

CPoint3d* CLinkLine::P(int index) {
    return index == 0 ? &points_[0] : index == 1 ? &points_[1] : nullptr;
}

const CPoint3d* CLinkLine::P(int index) const {
    return index == 0 ? &points_[0] : index == 1 ? &points_[1] : nullptr;
}

CPoint3d* CLinkLine::PLast() {
    return &points_[1];
}

const CPoint3d* CLinkLine::PLast() const {
    return &points_[1];
}

int CLinkLine::np() const {
    return 2;
}

void CLinkLine::SetStart(CPoint3d point) {
    points_[0] = point;
    EnforceGeometry();
}

void CLinkLine::SetEnd(CPoint3d point) {
    points_[1] = point;
    EnforceGeometry();
}

void CLinkLine::EnforceGeometry() {
}

double CLinkLine::GetLength() const {
    const double dx = points_[1].x - points_[0].x;
    const double dy = points_[1].y - points_[0].y;
    const double dz = points_[1].z - points_[0].z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

CPoint3d CLinkLine::GetPoint(double parameter) const {
    const double t = std::clamp(parameter, 0.0, 1.0);
    return CPoint3d(
        points_[0].x + (points_[1].x - points_[0].x) * t,
        points_[0].y + (points_[1].y - points_[0].y) * t,
        points_[0].z + (points_[1].z - points_[0].z) * t);
}

CPoint3d CLinkLine::GetTangent(double) const {
    return CPoint3d(
        points_[1].x - points_[0].x,
        points_[1].y - points_[0].y,
        points_[1].z - points_[0].z);
}

std::vector<CPoint3d> CLinkLine::Sample(std::size_t) const {
    return {points_[0], points_[1]};
}

double CLinkLine::DistanceToPoint2D(const CPoint3d& point) const {
    const double dx = points_[1].x - points_[0].x;
    const double dy = points_[1].y - points_[0].y;
    const double length_squared = dx * dx + dy * dy;
    if (length_squared <= kGeometryEpsilon) {
        const double px = point.x - points_[0].x;
        const double py = point.y - points_[0].y;
        return std::sqrt(px * px + py * py);
    }
    const double t = std::clamp(
        ((point.x - points_[0].x) * dx + (point.y - points_[0].y) * dy) / length_squared,
        0.0,
        1.0);
    const double nearest_x = points_[0].x + dx * t;
    const double nearest_y = points_[0].y + dy * t;
    const double px = point.x - nearest_x;
    const double py = point.y - nearest_y;
    return std::sqrt(px * px + py * py);
}

void CLinkLine::Translate(const CPoint3d& delta) {
    points_[0] += delta;
    points_[1] += delta;
}

void CLinkLine::TransformLocal(const CPoint3d& origin,
                               const CPoint3d& x_axis,
                               const CPoint3d& y_axis,
                               const CPoint3d& z_axis) {
    points_[0] = transform_point(points_[0], origin, x_axis, y_axis, z_axis);
    points_[1] = transform_point(points_[1], origin, x_axis, y_axis, z_axis);
}

std::size_t CLinkLine::GetID() const {
    return id_;
}

void CLinkLine::SetID(std::size_t id) {
    id_ = id;
}
