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
    : points_{std::make_shared<SketchNode>(SketchNode{SketchPoint(start)}), std::make_shared<SketchNode>(SketchNode{SketchPoint(end)})} {
}

CLinkLine::CLinkLine(CPoint3d* start, CPoint3d* end)
    : CLinkLine(start ? *start : CPoint3d(), end ? *end : CPoint3d()) {
}

LinkLineType CLinkLine::GetType() const {
    return LinkLineType::Segment;
}

std::unique_ptr<CLinkLine> CLinkLine::Clone() const {
    auto result = std::make_unique<CLinkLine>(GetStart(), GetEnd());
    result->SetID(id_);
    return result;
}

CPoint3d CLinkLine::GetStart() const {
    return points_[0]->point;
}

CPoint3d CLinkLine::GetEnd() const {
    return points_[1]->point;
}

const CPoint3d* CLinkLine::P(int index) const {
    if (index < 0 || index > 1) return nullptr;
    read_cache_[index] = points_[index]->point;
    return &read_cache_[index];
}
const CPoint3d* CLinkLine::PLast() const { return P(1); }
void CLinkLine::GeometryChanged() {
    if (auto state=changes_.lock()) {
        if (!state->depth) throw std::logic_error("Edit sketch geometry through CSmartLine::BeginEdit/CommitEdit");
        state->Mark(SketchGeometryChanged);
    }
}
void CLinkLine::SetEndpointRaw(int index, CPoint3d p) {
    const SketchPoint value(p);
    auto& old = points_[index]->point;
    if (old.u == value.u && old.v == value.v) return;
    GeometryChanged(); old = value;
}
int CLinkLine::np() const {
    return 2;
}

void CLinkLine::SetStart(CPoint3d point) {
    SetEndpointRaw(0,point);
    EnforceGeometry();
}

void CLinkLine::SetEnd(CPoint3d point) {
    SetEndpointRaw(1,point);
    EnforceGeometry();
}

void CLinkLine::EnforceGeometry() {
}

double CLinkLine::GetLength() const {
    const double dx = GetEnd().x - GetStart().x;
    const double dy = GetEnd().y - GetStart().y;
    const double dz = GetEnd().z - GetStart().z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

CPoint3d CLinkLine::GetPoint(double parameter) const {
    const double t = std::clamp(parameter, 0.0, 1.0);
    return CPoint3d(
        GetStart().x + (GetEnd().x - GetStart().x) * t,
        GetStart().y + (GetEnd().y - GetStart().y) * t,
        GetStart().z + (GetEnd().z - GetStart().z) * t);
}

CPoint3d CLinkLine::GetTangent(double) const {
    return CPoint3d(
        GetEnd().x - GetStart().x,
        GetEnd().y - GetStart().y,
        GetEnd().z - GetStart().z);
}

std::vector<CPoint3d> CLinkLine::Sample(std::size_t) const {
    return {GetStart(), GetEnd()};
}

double CLinkLine::DistanceToPoint2D(const CPoint3d& point) const {
    const double dx = GetEnd().x - GetStart().x;
    const double dy = GetEnd().y - GetStart().y;
    const double length_squared = dx * dx + dy * dy;
    if (length_squared <= kGeometryEpsilon) {
        const double px = point.x - GetStart().x;
        const double py = point.y - GetStart().y;
        return std::sqrt(px * px + py * py);
    }
    const double t = std::clamp(
        ((point.x - GetStart().x) * dx + (point.y - GetStart().y) * dy) / length_squared,
        0.0,
        1.0);
    const double nearest_x = GetStart().x + dx * t;
    const double nearest_y = GetStart().y + dy * t;
    const double px = point.x - nearest_x;
    const double py = point.y - nearest_y;
    return std::sqrt(px * px + py * py);
}

void CLinkLine::Translate(const CPoint3d& delta) {
    SetStart(GetStart()+delta);
    SetEnd(GetEnd()+delta);
}

void CLinkLine::TransformLocal(const CPoint3d& origin,
                               const CPoint3d& x_axis,
                               const CPoint3d& y_axis,
                               const CPoint3d& z_axis) {
    SetStart(transform_point(GetStart(), origin, x_axis, y_axis, z_axis));
    SetEnd(transform_point(GetEnd(), origin, x_axis, y_axis, z_axis));
}

std::size_t CLinkLine::GetID() const {
    return id_;
}

void CLinkLine::SetID(std::size_t id) {
    if (!changes_.expired()) throw std::logic_error("Attached line identity is immutable");
    id_ = id;
}
void CLinkLine::SetEndpointIds(std::size_t start,std::size_t end) {
    if (!changes_.expired()) throw std::logic_error("Attached node identity is immutable");
    points_[0]->id=start; points_[1]->id=end;
}
