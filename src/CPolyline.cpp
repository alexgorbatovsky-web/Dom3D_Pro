#include "CPolyline.h"

#include "OpenGLCompat.h"
#include "SurfaceUVMapping.h"
#include "solid/SurfaceFace.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <utility>

namespace {
Vec3 to_vec3(const CPoint3d& point) {
    return {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
}

CPoint3d to_point3d(Vec3 point) {
    return CPoint3d(point.x, point.y, point.z);
}

bool plane_from_polyline_points(const std::vector<CPoint3d>& points, Vec3& plane_point, Vec3& plane_normal) {
    if (points.size() < 3) {
        return false;
    }

    Vec3 normal{};
    for (size_t i = 0; i < points.size(); ++i) {
        const CPoint3d& current = points[i];
        const CPoint3d& next = points[(i + 1) % points.size()];
        normal.x += static_cast<float>((current.y - next.y) * (current.z + next.z));
        normal.y += static_cast<float>((current.z - next.z) * (current.x + next.x));
        normal.z += static_cast<float>((current.x - next.x) * (current.y + next.y));
    }

    normal = normalize(normal);
    if (dot(normal, normal) <= 0.000001f) {
        return false;
    }

    plane_point = to_vec3(points.front());
    plane_normal = normal;
    return true;
}

std::filesystem::path obj_output_path(const std::string& name) {
    std::filesystem::path path(name);
    if (!path.has_extension()) {
        path += ".obj";
    }
    return path;
}
}

CPolyline::CPolyline()
    : CAlfaObject("Polyline") {
    IsReversed = false;
}

CPolyline::CPolyline(std::string name)
    : CAlfaObject(std::move(name)) {
}

const std::vector<CPoint3d>& CPolyline::GetPoints() const {
    return points_;
}

std::vector<CPoint3d>& CPolyline::GetPoints() {
    return points_;
}

bool CPolyline::IsEmpty() const {
    return points_.empty();
}

bool CPolyline::IsClosed() const {
    return closed_ && CanClose();
}

bool CPolyline::CanClose() const {
    return points_.size() >= 3;
}

void CPolyline::SetClosed(bool closed) {
    closed_ = closed && CanClose();
}

bool CPolyline::Close() {
    if (!CanClose()) {
        closed_ = false;
        return false;
    }
    closed_ = true;
    return true;
}

void CPolyline::Open() {
    closed_ = false;
}

size_t CPolyline::GetPointCount() const {
    return points_.size();
}
size_t CPolyline::np() const {
    return points_.size();
}

void CPolyline::Clear() {
    points_.clear();
    closed_ = false;
}

void CPolyline::AddPoint(CPoint3d point) {
    closed_ = false;
    points_.push_back(point);
}

void CPolyline::AddPoint(CurvePoint point) {
    AddPoint(ToPoint3d(point));
}

bool CPolyline::InsertPoint(size_t index, CPoint3d point) {
    if (index > points_.size()) {
        return false;
    }

    points_.insert(points_.begin() + static_cast<std::vector<CPoint3d>::difference_type>(index), point);
    return true;
}

bool CPolyline::InsertPoint(size_t index, CurvePoint point) {
    return InsertPoint(index, ToPoint3d(point));
}

bool CPolyline::SetPoint(size_t index, CPoint3d point) {
    if (index >= points_.size()) {
        return false;
    }

    points_[index] = point;
    return true;
}

bool CPolyline::SetPoint(size_t index, CurvePoint point) {
    return SetPoint(index, ToPoint3d(point));
}

bool CPolyline::PutOnSurface(CSurfaceFace* surface) {
    if (!surface || points_.empty()) {
        return false;
    }

    SurfaceUVMapping mapping(surface);
    if (!mapping.IsValid()) {
        return false;
    }

    std::vector<CPoint3d> uv_points;
    uv_points.reserve(points_.size());
    SurfaceUVPoint previous{};
    bool has_previous = false;
    for (const CPoint3d& point : points_) {
        SurfaceUVPoint uv{};
        if (!mapping.Project(to_vec3(point), uv)) {
            return false;
        }
        if (has_previous) {
            uv = mapping.UnwrapNear(uv, previous);
        }
        uv_points.emplace_back(uv.u, uv.v, 0.0);
        previous = uv;
        has_previous = true;
    }

    points_ = std::move(uv_points);
    SetLockedPlane({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    return true;
}
bool CPolyline::RestoreTo3DFromUVSurface(CSurfaceFace* mm)
{
    if (!mm) {
        return false;
    }

    for (CPoint3d& point : points_) {
        CPoint8d ps;
        if (!mm->GetPoint(point.x, point.y, &ps)) {
            return false;
        }
        point.x = ps.x;
        point.y = ps.y;
        point.z = ps.z;
    }
    ClearLockedPlane();

    return true;
}


bool CPolyline::ExportToObj(const std::string& name) const {
    if (name.empty() || points_.empty()) {
        return false;
    }

    std::ofstream stream(obj_output_path(name), std::ios::out | std::ios::trunc);
    if (!stream) {
        return false;
    }

    stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    stream << "# Dom3D Pro CPolyline export\n";
    stream << "o " << (GetName().empty() ? "Polyline" : GetName()) << "\n";
    for (const CPoint3d& point : points_) {
        stream << "v " << point.x << " " << point.y << " " << point.z << "\n";
    }

    if (points_.size() == 1) {
        stream << "p 1\n";
    } else {
        stream << "l";
        for (size_t i = 0; i < points_.size(); ++i) {
            stream << " " << i + 1;
        }
        if (IsClosed()) {
            stream << " 1";
        }
        stream << "\n";
    }

    return static_cast<bool>(stream);
}

bool CPolyline::printToFile(const std::string& name) const {
    if (name.empty() || points_.empty()) {
        return false;
    }

    std::filesystem::path path(name);
    if (!path.has_parent_path()) {
        path = std::filesystem::path("C:\\temp") / path;
    }
    if (!path.has_extension()) {
        path += ".txt";
    }

    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }

    std::ofstream stream(path, std::ios::out | std::ios::app);
    if (!stream) {
        return false;
    }

    stream << "POLYLINE\n";
    stream << points_.size() << "\n";
    stream << std::fixed << std::setprecision(9);
    for (const CPoint3d& point : points_) {
        stream << point.x << " " << point.y << " " << point.z << "\n";
    }

    return static_cast<bool>(stream);
}

bool CPolyline::RemovePoint(size_t index) {
    if (index >= points_.size()) {
        return false;
    }

    points_.erase(points_.begin() + static_cast<std::vector<CPoint3d>::difference_type>(index));
    if (!CanClose()) {
        closed_ = false;
    }
    return true;
}

void CPolyline::SetLockedPlane(Vec3 plane_point, Vec3 plane_normal) {
    const Vec3 normal = normalize(plane_normal);
    if (dot(normal, normal) <= 0.000001f) {
        ClearLockedPlane();
        return;
    }
    has_locked_plane_ = true;
    locked_plane_point_ = plane_point;
    locked_plane_normal_ = normal;
}

void CPolyline::ClearLockedPlane() {
    has_locked_plane_ = false;
    locked_plane_point_ = {};
    locked_plane_normal_ = {0.0f, 1.0f, 0.0f};
}

bool CPolyline::GetLockedPlane(Vec3& plane_point, Vec3& plane_normal) const {
    if (!has_locked_plane_) {
        return false;
    }
    plane_point = locked_plane_point_;
    plane_normal = locked_plane_normal_;
    return true;
}

bool CPolyline::ApplyFillet(size_t point_index, double radius) {
    if (radius <= 0.000001 || points_.size() < 3 || point_index >= points_.size()) {
        return false;
    }
    if (!IsClosed() && (point_index == 0 || point_index + 1 >= points_.size())) {
        return false;
    }

    const size_t count = points_.size();
    const size_t previous_index = point_index == 0 ? count - 1 : point_index - 1;
    const size_t next_index = point_index + 1 == count ? 0 : point_index + 1;

    const Vec3 previous = to_vec3(points_[previous_index]);
    const Vec3 corner = to_vec3(points_[point_index]);
    const Vec3 next = to_vec3(points_[next_index]);
    const Vec3 to_previous = previous - corner;
    const Vec3 to_next = next - corner;
    const float previous_length = std::sqrt(dot(to_previous, to_previous));
    const float next_length = std::sqrt(dot(to_next, to_next));
    if (previous_length <= 0.000001f || next_length <= 0.000001f) {
        return false;
    }

    const Vec3 previous_dir = to_previous / previous_length;
    const Vec3 next_dir = to_next / next_length;
    const float cos_angle = std::clamp(dot(previous_dir, next_dir), -1.0f, 1.0f);
    const float angle = std::acos(cos_angle);
    if (angle <= 0.001f || std::fabs(kPi - angle) <= 0.001f) {
        return false;
    }

    const float tangent_distance = static_cast<float>(radius / std::tan(angle * 0.5f));
    if (tangent_distance <= 0.000001f || tangent_distance >= previous_length || tangent_distance >= next_length) {
        return false;
    }

    Vec3 plane_point{};
    Vec3 plane_normal{};
    if (!GetLockedPlane(plane_point, plane_normal) && !plane_from_polyline_points(points_, plane_point, plane_normal)) {
        return false;
    }
    plane_normal = normalize(plane_normal);

    const Vec3 tangent_previous = corner + previous_dir * tangent_distance;
    const Vec3 tangent_next = corner + next_dir * tangent_distance;
    const Vec3 bisector = normalize(previous_dir + next_dir);
    if (dot(bisector, bisector) <= 0.000001f) {
        return false;
    }

    const Vec3 center = corner + bisector * static_cast<float>(radius / std::sin(angle * 0.5f));
    const Vec3 start_radius = tangent_previous - center;
    const Vec3 end_radius = tangent_next - center;
    float signed_angle = std::atan2(dot(cross(start_radius, end_radius), plane_normal), dot(start_radius, end_radius));
    if (signed_angle > kPi) {
        signed_angle -= 2.0f * kPi;
    } else if (signed_angle < -kPi) {
        signed_angle += 2.0f * kPi;
    }
    if (std::fabs(signed_angle) <= 0.0001f) {
        return false;
    }

    const int segment_count = std::max(4, static_cast<int>(std::ceil(std::fabs(signed_angle) / (kPi / 18.0f))));
    std::vector<CPoint3d> arc_points;
    arc_points.reserve(static_cast<size_t>(segment_count) + 1);
    for (int i = 0; i <= segment_count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segment_count);
        arc_points.push_back(to_point3d(center + rotate_around_axis(start_radius, plane_normal, signed_angle * t)));
    }

    points_.erase(points_.begin() + static_cast<std::vector<CPoint3d>::difference_type>(point_index));
    points_.insert(points_.begin() + static_cast<std::vector<CPoint3d>::difference_type>(point_index),
                   arc_points.begin(),
                   arc_points.end());
    return true;
}

bool CPolyline::HitTestPoint(CurvePoint point, float tolerance, size_t& point_index) const {
    float best_distance = tolerance;
    bool found = false;

    for (size_t i = 0; i < points_.size(); ++i) {
        const float dx = point.x - static_cast<float>(points_[i].x);
        const float dz = point.z - static_cast<float>(points_[i].z);
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
    glLineWidth(selected ? 5.0f : 4.0f);
    glColor3f(selected ? 0.72f : color.r, selected ? 0.12f : color.g, selected ? 1.0f : color.b);
    glBegin(GL_LINE_STRIP);
    for (const CPoint3d& point : points_) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    if (IsClosed()) {
        const CPoint3d& point = points_.front();
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);

    if (selected) {
        for (size_t i = 0; i < points_.size(); ++i) {
            DrawPointBox(points_[i], selected, has_selected_point && i == selected_point_index);
        }
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
    for (const CPoint3d& point : points_) {
        glVertex2f(center_x + static_cast<float>(point.x) * scale, center_y + static_cast<float>(point.z) * scale);
    }
    if (IsClosed()) {
        const CPoint3d& point = points_.front();
        glVertex2f(center_x + static_cast<float>(point.x) * scale, center_y + static_cast<float>(point.z) * scale);
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
    if (IsClosed() && DistanceToSegment(point, points_.back(), points_.front()) <= tolerance) {
        return true;
    }

    return false;
}

std::unique_ptr<CAlfaObject> CPolyline::Clone() const {
    auto copy = std::make_unique<CPolyline>(GetName() + " Copy");
    copy->points_ = points_;
    copy->closed_ = closed_;
    copy->has_locked_plane_ = has_locked_plane_;
    copy->locked_plane_point_ = locked_plane_point_;
    copy->locked_plane_normal_ = locked_plane_normal_;
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    return copy;
}

void CPolyline::Translate(Vec3 delta) {
    for (CPoint3d& point : points_) {
        point.x += delta.x;
        point.y += delta.y;
        point.z += delta.z;
    }
    if (has_locked_plane_) {
        locked_plane_point_ = locked_plane_point_ + delta;
    }
}

void CPolyline::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (CPoint3d& point : points_) {
        const Vec3 local = {
            static_cast<float>(point.x) - center.x,
            static_cast<float>(point.y) - center.y,
            static_cast<float>(point.z) - center.z
        };
        const Vec3 rotated = rotate_around_axis(local, axis, angle) + center;
        point.x = rotated.x;
        point.y = rotated.y;
        point.z = rotated.z;
    }
    if (has_locked_plane_) {
        locked_plane_point_ = rotate_around_axis(locked_plane_point_ - center, axis, angle) + center;
        locked_plane_normal_ = normalize(rotate_around_axis(locked_plane_normal_, axis, angle));
    }
}

void CPolyline::Scale(Vec3 center, Vec3 axis, float factor) {
    for (CPoint3d& point : points_) {
        const Vec3 local = {
            static_cast<float>(point.x) - center.x,
            static_cast<float>(point.y) - center.y,
            static_cast<float>(point.z) - center.z
        };
        const Vec3 scaled = (dot(axis, axis) <= 0.000001f ? scale_uniform(local, factor) : scale_along_axis(local, axis, factor)) + center;
        point.x = scaled.x;
        point.y = scaled.y;
        point.z = scaled.z;
    }
    if (has_locked_plane_) {
        const Vec3 local = locked_plane_point_ - center;
        locked_plane_point_ = (dot(axis, axis) <= 0.000001f ? scale_uniform(local, factor) : scale_along_axis(local, axis, factor)) + center;
        if (dot(axis, axis) > 0.000001f) {
            ClearLockedPlane();
        }
    }
}

bool CPolyline::GetBounds(Vec3& min_point, Vec3& max_point) const {
    if (points_.empty()) {
        return false;
    }

    min_point = {static_cast<float>(points_[0].x), static_cast<float>(points_[0].y), static_cast<float>(points_[0].z)};
    max_point = min_point;
    for (const CPoint3d& point : points_) {
        min_point.x = std::min(min_point.x, static_cast<float>(point.x));
        min_point.y = std::min(min_point.y, static_cast<float>(point.y));
        min_point.z = std::min(min_point.z, static_cast<float>(point.z));
        max_point.x = std::max(max_point.x, static_cast<float>(point.x));
        max_point.y = std::max(max_point.y, static_cast<float>(point.y));
        max_point.z = std::max(max_point.z, static_cast<float>(point.z));
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
           << (IsClosed() ? 1 : 0) << " " << points_.size() << "\n";
    for (const CPoint3d& point : points_) {
        stream << point.x << " " << point.y << " " << point.z << "\n";
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
    } else if (values.size() == 5) {
        material.alpha = values[0];
        material.specular = values[1];
        material.shininess = values[2];
        closed_ = values[3] != 0.0f;
        count = static_cast<size_t>(values[4]);
    } else {
        return false;
    }
    if (!stream) {
        return false;
    }
    SetMaterial(material);

    std::vector<CPoint3d> loaded;
    loaded.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::string point_line;
        stream >> std::ws;
        std::getline(stream, point_line);
        std::istringstream point_stream(point_line);
        std::vector<double> coords;
        double coord = 0.0;
        while (point_stream >> coord) {
            coords.push_back(coord);
        }
        if (coords.size() == 2) {
            loaded.emplace_back(coords[0], 0.08, coords[1]);
        } else if (coords.size() >= 3) {
            loaded.emplace_back(coords[0], coords[1], coords[2]);
        } else {
            return false;
        }
    }

    points_ = std::move(loaded);
    SetClosed(closed_);
    return true;
}

float CPolyline::DistanceToSegment(CurvePoint point, const CPoint3d& start, const CPoint3d& end) const {
    const float start_x = static_cast<float>(start.x);
    const float start_z = static_cast<float>(start.z);
    const float end_x = static_cast<float>(end.x);
    const float end_z = static_cast<float>(end.z);
    const float dx = end_x - start_x;
    const float dz = end_z - start_z;
    const float length_sq = dx * dx + dz * dz;
    if (length_sq <= 0.00001f) {
        const float px = point.x - start_x;
        const float pz = point.z - start_z;
        return std::sqrt(px * px + pz * pz);
    }

    const float t = std::clamp(((point.x - start_x) * dx + (point.z - start_z) * dz) / length_sq, 0.0f, 1.0f);
    const float closest_x = start_x + t * dx;
    const float closest_z = start_z + t * dz;
    const float px = point.x - closest_x;
    const float pz = point.z - closest_z;
    return std::sqrt(px * px + pz * pz);
}

void CPolyline::DrawPointBox(const CPoint3d& point, bool selected, bool point_selected) const {
    const float half = point_selected ? 0.056f : (selected ? 0.0385f : 0.0315f);
    const float height = point_selected ? 0.098f : (selected ? 0.0665f : 0.0595f);
    const float x = static_cast<float>(point.x) - half;
    const float y = static_cast<float>(point.y) - half;
    const float z = static_cast<float>(point.z) - half;
    const float x2 = static_cast<float>(point.x) + half;
    const float y2 = y + height;
    const float z2 = static_cast<float>(point.z) + half;

    const Color color = GetColor();
    const float r = point_selected ? 0.0f : (selected ? 0.0f : color.r);
    const float g = point_selected ? 1.0f : (selected ? 1.0f : color.g * 0.7f);
    const float b = point_selected ? 0.0f : (selected ? 0.0f : color.b * 0.8f);

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

CPoint3d CPolyline::ToPoint3d(CurvePoint point) {
    return CPoint3d(point.x, 0.08, point.z);
}

CurvePoint CPolyline::ToCurvePoint(const CPoint3d& point) {
    return {static_cast<float>(point.x), static_cast<float>(point.z)};
}

bool CPolyline::CreatePolygone(float Length, int qty)
{
    if (qty < 3)
        return false;
	points_.resize(qty + 1);
    float alfa = PI / (float)qty;
    CPoint3d p;
    p.x = -Length / 2.0;
    p.y = -Length / tan(alfa) / 2.0;

    points_[0] = p;
    CPoint3d p0;
    CPoint3d pz(0, 0, 100);
    for (int i = 0; i < qty; i++) {
        p.Rotate(&p0, &pz, alfa * 2.0);
        points_[i + 1] = p;
    }
    return true;
}
CPoint3d* CPolyline::P(int n)
{
    if (n >= GetPointCount())
        return NULL;
    return &points_[n];

}
CPoint3d* CPolyline::PLast()
{
    return &points_[points_.size()-1];
}
void CPolyline::Revers()
{
    CPoint3d ptm;
    int i, j;
    for (i = 0, j = np() - 1; i < j; i++, j--) {
        ptm = points_[i];
        points_[i] = points_[j];
        points_[j] = ptm;
    }
    IsReversed = !IsReversed;
}
double CPolyline::GetLength()
{
    double Length = 0;
    for (int i = 0; i < np() - 1; i++)
        Length += P(i)->DistTo(P(i + 1));

    return Length;
}
bool CPolyline::IsConcavePolygonOnXY()
{
    int n = np();
    if (n < 3) return false;

    // Oriented area (projection onto XY)
    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        CPoint3d* p = P(i);
        CPoint3d* q = P((i + 1) % n);
        area += p->x * q->y - q->x * p->y;
    }
    // degenerate/collinear case - considered non-concave
    const double AREA_EPS = 1e-12;
    if (fabs(area) < AREA_EPS) return false;

    int polySign = (area > 0.0) ? 1 : -1; // +1 � CCW, -1 � CW

    const double eps = 1e-10;
    // Walk along the peaks and check the sign cross(prev->cur, cur->next)
    for (int i = 0; i < n; ++i) {
        CPoint3d* prev = P((i - 1 + n) % n);
        CPoint3d* cur = P(i);
        CPoint3d* next = P((i + 1) % n);

        double vx1 = cur->x - prev->x;
        double vy1 = cur->y - prev->y;
        double vx2 = next->x - cur->x;
        double vy2 = next->y - cur->y;

        double crossZ = vx1 * vy2 - vy1 * vx2;

        // Ignoring nearly collinear triples
        if (fabs(crossZ) <= eps) continue;

        int sign = (crossZ > 0.0) ? 1 : -1;
        if (sign != polySign) {
            // a concave angle was found
            return true;
        }
    }


    return false;
}

bool CPolyline::JoinG(CPolyline* line2)
{
    if (!line2 || this == line2 || line2->IsEmpty()) {
        return false;
    }

    if (IsEmpty()) {
        points_ = line2->GetPoints();
        closed_ = false;
        TmpLen = static_cast<float>(GetLength());
        return true;
    }

    const std::vector<CPoint3d>& line2_points = line2->GetPoints();
    if (line2_points.size() > 1) {
        points_.insert(points_.end(), line2_points.begin() + 1, line2_points.end());
    }
    closed_ = false;
    TmpLen = static_cast<float>(GetLength());
    return true;
}


bool CPolyline::JoinLine(CPolyline* line2, double delta)
{
    if (!line2 || this == line2 || line2->IsEmpty()) {
        return false;
    }
    if (IsEmpty())
        return JoinG(line2);
    double dist1 = PLast()->DistTo(line2->P(0));
    if (dist1 < delta)
        return JoinG(line2);

    double dist2 = PLast()->DistTo(line2->PLast());
    if (dist2 < delta) {
        line2->Revers();
        return JoinG(line2);
    }

    double dist3 = P(0)->DistTo(line2->P(0));
    if (dist3 < delta) {
        Revers();
        return JoinG(line2);
    }
    double dist4 = P(0)->DistTo(line2->PLast());
    if (dist4 > delta) {
        return false;
    }
    Revers();
    line2->Revers();
    return JoinG(line2);
}
bool CPolyline::JoinMultuLines(std::vector<CPolyline*>* lines, std::vector<CPolyline*>* LinesJoin, double delta)
{
    if (!lines || !LinesJoin)
        return false;

    while (!lines->empty()) {
        CPolyline* line1 = lines->front();
        if (!line1)
            return false;
        lines->erase(lines->begin());
        LinesJoin->push_back(line1);

        bool joined = true;
        while (joined && !lines->empty()) {
            joined = false;
            for (size_t i = 0; i < lines->size(); ++i) {
                CPolyline* line2 = (*lines)[i];
                if (!line2 || line2->IsEmpty())
                    continue;

                if (line1->P(0)->DistTo(line2->P(0)) < delta
                    || line1->P(0)->DistTo(line2->PLast()) < delta
                    || line1->PLast()->DistTo(line2->P(0)) < delta
                    || line1->PLast()->DistTo(line2->PLast()) < delta) {
                    if (!line1->JoinLine(line2, delta))
                        return false;
                    lines->erase(lines->begin() + static_cast<std::vector<CPolyline*>::difference_type>(i));
                    joined = true;
                    break;
                }
            }
        }
    }
    return true;
}
