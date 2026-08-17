#include "CadCurve3D.h"

#include "OpenGLCompat.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ostream>
#include <utility>

CCadCurve3D::CCadCurve3D()
    : CAlfaObject("CAD Curve") {
}

CCadCurve3D::CCadCurve3D(std::string name)
    : CAlfaObject(std::move(name)) {
}

CCadCurve3D::CCadCurve3D(std::string name, std::vector<Vec3> points)
    : CAlfaObject(std::move(name)), points_(std::move(points)) {
}

const std::vector<Vec3>& CCadCurve3D::GetPoints() const {
    return points_;
}

std::vector<Vec3>& CCadCurve3D::GetPoints() {
    return points_;
}

void CCadCurve3D::SetPoints(std::vector<Vec3> points) {
    points_ = std::move(points);
}

bool CCadCurve3D::IsEmpty() const {
    return points_.empty();
}

void CCadCurve3D::Render3d(bool selected) const {
    if (points_.size() < 2) {
        return;
    }

    const Color color = GetColor();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(selected ? 5.0f : 3.0f);
    glColor3f(selected ? 0.35f : color.r, selected ? 0.86f : color.g, selected ? 1.0f : color.b);
    glBegin(GL_LINE_STRIP);
    for (const Vec3& point : points_) {
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

void CCadCurve3D::Render2d(float center_x, float center_y, float scale) const {
    if (points_.size() < 2) {
        return;
    }

    const Color color = GetColor();
    glLineWidth(2.0f);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_STRIP);
    for (const Vec3& point : points_) {
        glVertex2f(center_x + point.x * scale, center_y + point.z * scale);
    }
    glEnd();
}

bool CCadCurve3D::HitTest(CurvePoint point, float tolerance) const {
    if (points_.size() < 2) {
        return false;
    }

    for (size_t i = 1; i < points_.size(); ++i) {
        if (DistanceToSegmentXZ(point, points_[i - 1], points_[i]) <= tolerance) {
            return true;
        }
    }
    return false;
}

bool CCadCurve3D::Save(std::ostream& stream) const {
    stream << "CadCurve3D \"" << GetName() << "\" " << points_.size() << "\n";
    for (const Vec3& point : points_) {
        stream << point.x << " " << point.y << " " << point.z << "\n";
    }
    return static_cast<bool>(stream);
}

std::unique_ptr<CAlfaObject> CCadCurve3D::Clone() const {
    auto copy = std::make_unique<CCadCurve3D>(GetName() + " Copy", points_);
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    return copy;
}

void CCadCurve3D::Translate(Vec3 delta) {
    for (Vec3& point : points_) {
        point = point + delta;
    }
}

void CCadCurve3D::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (Vec3& point : points_) {
        point = rotate_around_axis(point - center, axis, angle) + center;
    }
}

void CCadCurve3D::Scale(Vec3 center, Vec3 axis, float factor) {
    for (Vec3& point : points_) {
        const Vec3 local = point - center;
        point = (dot(axis, axis) <= 0.000001f ? scale_uniform(local, factor) : scale_along_axis(local, axis, factor)) + center;
    }
}

bool CCadCurve3D::GetBounds(Vec3& min_point, Vec3& max_point) const {
    if (points_.empty()) {
        return false;
    }

    min_point = points_.front();
    max_point = points_.front();
    for (const Vec3& point : points_) {
        min_point.x = std::min(min_point.x, point.x);
        min_point.y = std::min(min_point.y, point.y);
        min_point.z = std::min(min_point.z, point.z);
        max_point.x = std::max(max_point.x, point.x);
        max_point.y = std::max(max_point.y, point.y);
        max_point.z = std::max(max_point.z, point.z);
    }
    return true;
}

float CCadCurve3D::DistanceToSegmentXZ(CurvePoint point, Vec3 start, Vec3 end) const {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float length_sq = dx * dx + dz * dz;
    if (length_sq <= 0.00001f) {
        const float px = point.x - start.x;
        const float pz = point.z - start.z;
        return std::sqrt(px * px + pz * pz);
    }

    const float t = std::clamp(((point.x - start.x) * dx + (point.z - start.z) * dz) / length_sq, 0.0f, 1.0f);
    const float closest_x = start.x + t * dx;
    const float closest_z = start.z + t * dz;
    const float px = point.x - closest_x;
    const float pz = point.z - closest_z;
    return std::sqrt(px * px + pz * pz);
}
