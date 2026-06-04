#include "CPolyline.h"

#include "OpenGLCompat.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <sstream>
#include <utility>

CPolyline::CPolyline()
    : CAlfaObject("Polyline") {
}

CPolyline::CPolyline(std::string name)
    : CAlfaObject(std::move(name)) {
}

const std::vector<CurvePoint>& CPolyline::GetPoints() const {
    return points_;
}

std::vector<CurvePoint>& CPolyline::GetPoints() {
    return points_;
}

bool CPolyline::IsEmpty() const {
    return points_.empty();
}

size_t CPolyline::GetPointCount() const {
    return points_.size();
}

void CPolyline::Clear() {
    points_.clear();
}

void CPolyline::AddPoint(CurvePoint point) {
    points_.push_back(point);
}

bool CPolyline::InsertPoint(size_t index, CurvePoint point) {
    if (index > points_.size()) {
        return false;
    }

    points_.insert(points_.begin() + static_cast<std::vector<CurvePoint>::difference_type>(index), point);
    return true;
}

bool CPolyline::SetPoint(size_t index, CurvePoint point) {
    if (index >= points_.size()) {
        return false;
    }

    points_[index] = point;
    return true;
}

bool CPolyline::RemovePoint(size_t index) {
    if (index >= points_.size()) {
        return false;
    }

    points_.erase(points_.begin() + static_cast<std::vector<CurvePoint>::difference_type>(index));
    return true;
}

bool CPolyline::HitTestPoint(CurvePoint point, float tolerance, size_t& point_index) const {
    float best_distance = tolerance;
    bool found = false;

    for (size_t i = 0; i < points_.size(); ++i) {
        const float dx = point.x - points_[i].x;
        const float dz = point.z - points_[i].z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= best_distance) {
            best_distance = distance;
            point_index = i;
            found = true;
        }
    }

    return found;
}

void CPolyline::Render3d(bool selected) const {
    Render3d(selected, false, 0);
}

void CPolyline::Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const {
    if (IsEmpty()) {
        return;
    }

    const Color color = GetColor();
    glDisable(GL_DEPTH_TEST);
    glLineWidth(selected ? 7.0f : 4.0f);
    glColor3f(selected ? 0.35f : color.r, selected ? 0.86f : color.g, selected ? 1.0f : color.b);
    glBegin(GL_LINE_STRIP);
    for (const CurvePoint& point : points_) {
        glVertex3f(point.x, 0.08f, point.z);
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);

    for (size_t i = 0; i < points_.size(); ++i) {
        DrawPointBox(points_[i], selected, has_selected_point && i == selected_point_index);
    }
}

void CPolyline::Render2d(float center_x, float center_y, float scale) const {
    if (IsEmpty()) {
        return;
    }

    const Color color = GetColor();
    glLineWidth(2.0f);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_STRIP);
    for (const CurvePoint& point : points_) {
        glVertex2f(center_x + point.x * scale, center_y + point.z * scale);
    }
    glEnd();
}

bool CPolyline::HitTest(CurvePoint point, float tolerance) const {
    if (points_.empty()) {
        return false;
    }

    if (points_.size() == 1) {
        return DistanceToSegment(point, points_[0], points_[0]) <= tolerance;
    }

    for (size_t i = 1; i < points_.size(); ++i) {
        if (DistanceToSegment(point, points_[i - 1], points_[i]) <= tolerance) {
            return true;
        }
    }

    return false;
}

void CPolyline::Translate(Vec3 delta) {
    for (CurvePoint& point : points_) {
        point.x += delta.x;
        point.z += delta.z;
    }
}

void CPolyline::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (CurvePoint& point : points_) {
        const Vec3 local = {point.x - center.x, -center.y, point.z - center.z};
        const Vec3 rotated = rotate_around_axis(local, axis, angle) + center;
        point.x = rotated.x;
        point.z = rotated.z;
    }
}

void CPolyline::Scale(Vec3 center, Vec3 axis, float factor) {
    for (CurvePoint& point : points_) {
        const Vec3 local = {point.x - center.x, -center.y, point.z - center.z};
        const Vec3 scaled = scale_along_axis(local, axis, factor) + center;
        point.x = scaled.x;
        point.z = scaled.z;
    }
}

bool CPolyline::GetBounds(Vec3& min_point, Vec3& max_point) const {
    if (points_.empty()) {
        return false;
    }

    min_point = {points_[0].x, 0.0f, points_[0].z};
    max_point = min_point;
    for (const CurvePoint& point : points_) {
        min_point.x = std::min(min_point.x, point.x);
        min_point.z = std::min(min_point.z, point.z);
        max_point.x = std::max(max_point.x, point.x);
        max_point.z = std::max(max_point.z, point.z);
    }

    return true;
}

void CPolyline::Edit(NativeWindowHandle parent_window) {
    CAlfaObject::Edit(parent_window);
}

bool CPolyline::Save(std::ostream& stream) const {
    const Material material = GetMaterial();
    stream << "Polyline \"" << GetName() << "\" "
           << material.diffuse.r << " " << material.diffuse.g << " " << material.diffuse.b << " "
           << material.alpha << " " << material.specular << " " << material.shininess << " "
           << points_.size() << "\n";
    for (const CurvePoint& point : points_) {
        stream << point.x << " " << point.z << "\n";
    }

    return static_cast<bool>(stream);
}

bool CPolyline::Load(std::istream& stream) {
    std::string keyword;
    stream >> keyword;
    if (keyword != "Polyline") {
        return false;
    }

    stream >> std::ws;
    if (stream.peek() != '"') {
        return false;
    }

    stream.get();
    std::string name;
    std::getline(stream, name, '"');
    SetName(name);

    size_t count = 0;
    Material material{};
    stream >> material.diffuse.r >> material.diffuse.g >> material.diffuse.b;
    stream >> std::ws;
    std::string header_line;
    std::getline(stream, header_line);
    std::istringstream header_stream(header_line);
    std::vector<float> values;
    float value = 0.0f;
    while (header_stream >> value) {
        values.push_back(value);
    }
    if (values.size() == 1) {
        count = static_cast<size_t>(values[0]);
    } else if (values.size() == 4) {
        material.alpha = values[0];
        material.specular = values[1];
        material.shininess = values[2];
        count = static_cast<size_t>(values[3]);
    } else {
        return false;
    }
    if (!stream) {
        return false;
    }
    SetMaterial(material);

    std::vector<CurvePoint> loaded;
    loaded.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        CurvePoint point{};
        stream >> point.x >> point.z;
        if (!stream) {
            return false;
        }
        loaded.push_back(point);
    }

    points_ = std::move(loaded);
    return true;
}

float CPolyline::DistanceToSegment(CurvePoint point, CurvePoint start, CurvePoint end) const {
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

void CPolyline::DrawPointBox(CurvePoint point, bool selected, bool point_selected) const {
    const float half = point_selected ? 0.14f : (selected ? 0.10f : 0.08f);
    const float height = point_selected ? 0.26f : (selected ? 0.20f : 0.16f);
    const float x = point.x - half;
    const float y = 0.09f;
    const float z = point.z - half;
    const float x2 = point.x + half;
    const float y2 = y + height;
    const float z2 = point.z + half;

    const Color color = GetColor();
    const float r = point_selected ? 1.0f : (selected ? 0.35f : color.r);
    const float g = point_selected ? 0.28f : (selected ? 0.86f : color.g * 0.7f);
    const float b = point_selected ? 0.20f : (selected ? 1.0f : color.b * 0.8f);

    glBegin(GL_QUADS);
    glColor3f(r, g, b);
    glVertex3f(x, y, z);
    glVertex3f(x2, y, z);
    glVertex3f(x2, y2, z);
    glVertex3f(x, y2, z);

    glColor3f(r * 0.85f, g * 0.85f, b * 0.85f);
    glVertex3f(x2, y, z);
    glVertex3f(x2, y, z2);
    glVertex3f(x2, y2, z2);
    glVertex3f(x2, y2, z);

    glColor3f(r * 0.75f, g * 0.75f, b * 0.75f);
    glVertex3f(x, y, z2);
    glVertex3f(x, y, z);
    glVertex3f(x, y2, z);
    glVertex3f(x, y2, z2);

    glColor3f(r * 0.92f, g * 0.92f, b * 0.92f);
    glVertex3f(x, y2, z);
    glVertex3f(x2, y2, z);
    glVertex3f(x2, y2, z2);
    glVertex3f(x, y2, z2);
    glEnd();
}
