#include "Dimens.h"

#include <cmath>
#include <utility>

namespace {
CPoint3d add(CPoint3d first, CPoint3d second) {
    return {first.x + second.x, first.y + second.y, first.z + second.z};
}

CPoint3d multiply(CPoint3d point, double value) {
    return {point.x * value, point.y * value, point.z * value};
}

CPoint3d normalized(CPoint3d point) {
    const double length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
    if (length <= 1.0e-12) {
        return {0.0, 1.0, 0.0};
    }
    return multiply(point, 1.0 / length);
}
}

CDimens::CDimens(std::string parameter_id, std::string label, double value)
    : parameter_id_(std::move(parameter_id)),
      label_(std::move(label)),
      value_(value) {
}

const std::string& CDimens::GetParameterId() const {
    return parameter_id_;
}

const std::string& CDimens::GetLabel() const {
    return label_;
}

double CDimens::GetValue() const {
    return value_;
}

void CDimens::SetValue(double value) {
    value_ = value;
}

bool CDimens::IsVisible() const {
    return visible_;
}

void CDimens::SetVisible(bool visible) {
    visible_ = visible;
}

CDimens3D::CDimens3D(CPoint3d start,
                     CPoint3d end,
                     CPoint3d offset_direction,
                     double offset,
                     std::string parameter_id,
                     std::string label,
                     double value)
    : CDimens(std::move(parameter_id), std::move(label), value),
      start_(start),
      end_(end),
      offset_direction_(normalized(offset_direction)),
      offset_(offset) {
}

void CDimens3D::SetPoints(CPoint3d start, CPoint3d end) {
    start_ = start;
    end_ = end;
}

void CDimens3D::SetOffset(CPoint3d direction, double distance) {
    offset_direction_ = normalized(direction);
    offset_ = distance;
}

const CPoint3d& CDimens3D::GetStart() const {
    return start_;
}

const CPoint3d& CDimens3D::GetEnd() const {
    return end_;
}

const CPoint3d& CDimens3D::GetOffsetDirection() const {
    return offset_direction_;
}

double CDimens3D::GetOffsetDistance() const {
    return offset_;
}

void CDimens3D::SetActive(bool active) {
    active_ = active;
}

bool CDimens3D::IsActive() const {
    return active_;
}

void CDimens3D::SetSourceIndex(size_t source_index) {
    source_index_ = source_index;
}

size_t CDimens3D::GetSourceIndex() const {
    return source_index_;
}

DimensionGeometry3D CDimens3D::GetGeometry(double extension_overshoot) const {
    const CPoint3d offset_vector = multiply(offset_direction_, offset_);
    const CPoint3d overshoot_vector =
        multiply(offset_direction_, extension_overshoot * (offset_ < 0.0 ? -1.0 : 1.0));
    DimensionGeometry3D geometry;
    geometry.source_start = start_;
    geometry.source_end = end_;
    geometry.dimension_start = add(start_, offset_vector);
    geometry.dimension_end = add(end_, offset_vector);
    geometry.extension_start = add(geometry.dimension_start, overshoot_vector);
    geometry.extension_end = add(geometry.dimension_end, overshoot_vector);
    geometry.text_position = multiply(
        add(geometry.dimension_start, geometry.dimension_end),
        0.5);
    return geometry;
}

double CDimens3D::GetMeasuredLength() const {
    const double dx = end_.x - start_.x;
    const double dy = end_.y - start_.y;
    const double dz = end_.z - start_.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
