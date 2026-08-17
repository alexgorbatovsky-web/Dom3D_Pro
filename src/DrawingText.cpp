#include "DrawingText.h"

#ifdef LPCTSTR
#undef LPCTSTR
#endif
#ifdef String
#undef String
#endif
#ifdef Coord
#undef Coord
#endif
#ifdef Pixel
#undef Pixel
#endif
#include "OpenGLCompat.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <ostream>

namespace {
constexpr double kDrawingPi = 3.14159265358979323846;
}

CDrawingText::CDrawingText(
    std::string text, CPoint3d insertion, double height, double rotation_degrees,
    std::string font_family)
    : CAlfaObject(text.empty() ? "DXF Text" : text),
      text_(std::move(text)), insertion_(insertion),
      height_(std::max(0.01, height)), rotation_degrees_(rotation_degrees),
      font_family_(font_family.empty() ? "Arial" : std::move(font_family)) {
}

const std::string& CDrawingText::GetText() const { return text_; }
double CDrawingText::GetHeight() const { return height_; }
double CDrawingText::GetRotationDegrees() const { return rotation_degrees_; }
const CPoint3d& CDrawingText::GetInsertion() const { return insertion_; }
const std::string& CDrawingText::GetFontFamily() const { return font_family_; }

double CDrawingText::GetLineAdvance() const {
    // Arial at the 1000 px construction size used below has a 716 px cap
    // height. Keep this calculation GUI-independent because DXF export is
    // also exercised by command-line conversion and tests.
    return height_ * (1200.0 / 716.0);
}

void CDrawingText::SetText(std::string text) {
    text_ = std::move(text);
    SetName(text_.empty() ? "Text" : text_);
    geometry_dirty_ = true;
}

void CDrawingText::SetHeight(double height) {
    height_ = std::max(0.01, height);
    geometry_dirty_ = true;
}

void CDrawingText::SetRotationDegrees(double rotation_degrees) {
    rotation_degrees_ = rotation_degrees;
    geometry_dirty_ = true;
}

void CDrawingText::SetInsertion(CPoint3d insertion) {
    insertion_ = insertion;
    geometry_dirty_ = true;
}

void CDrawingText::SetFontFamily(std::string font_family) {
    font_family_ = font_family.empty() ? "Arial" : std::move(font_family);
    geometry_dirty_ = true;
}

CPoint3d CDrawingText::LocalToWorld(double x, double y) const {
    const double angle = rotation_degrees_ * kDrawingPi / 180.0;
    return {
        insertion_.x + x * std::cos(angle) - y * std::sin(angle),
        insertion_.y + x * std::sin(angle) + y * std::cos(angle),
        insertion_.z};
}

void CDrawingText::EnsureGeometry() const {
    if (!geometry_dirty_) return;
    geometry_dirty_ = false;
    contours_.clear();
    if (text_.empty()) return;

    QFont font(QString::fromUtf8(font_family_.c_str()));
    font.setPixelSize(1000);
    QPainterPath path;
    const QStringList lines = QString::fromUtf8(text_.c_str()).split('\n');
    for (int index = 0; index < lines.size(); ++index) {
        path.addText(0.0, static_cast<double>(index) * 1200.0, font, lines[index]);
    }
    const double cap_height = std::max(1.0, QFontMetricsF(font).capHeight());
    const double scale = height_ / cap_height;
    for (const QPolygonF& polygon : path.toSubpathPolygons()) {
        if (polygon.size() < 2) continue;
        std::vector<CPoint3d> contour;
        contour.reserve(static_cast<size_t>(polygon.size()));
        for (const QPointF& point : polygon) {
            contour.push_back(LocalToWorld(point.x() * scale, -point.y() * scale));
        }
        contours_.push_back(std::move(contour));
    }
}

void CDrawingText::Render3d(bool selected) const {
    EnsureGeometry();
    const Color color = GetColor();
    glDisable(GL_DEPTH_TEST);
    ApplyLineAppearance(selected, 3.5f, 1.5f);
    glColor3f(selected ? 0.72f : color.r,
              selected ? 0.12f : color.g,
              selected ? 1.0f : color.b);
    for (const auto& contour : contours_) {
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : contour) glVertex3d(point.x, point.y, point.z);
        glEnd();
    }
    ResetLineAppearance();
    glEnable(GL_DEPTH_TEST);
}

void CDrawingText::Render2d(float center_x, float center_y, float scale) const {
    EnsureGeometry();
    const Color color = GetColor();
    ApplyLineAppearance(false, 3.5f, 1.5f);
    glColor3f(color.r, color.g, color.b);
    for (const auto& contour : contours_) {
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : contour) {
            glVertex2f(center_x + static_cast<float>(point.x) * scale,
                       center_y + static_cast<float>(point.z) * scale);
        }
        glEnd();
    }
    ResetLineAppearance();
}

bool CDrawingText::HitTest(CurvePoint point, float tolerance) const {
    Vec3 minimum{}, maximum{};
    if (!GetBounds(minimum, maximum)) return false;
    return point.x >= minimum.x - tolerance && point.x <= maximum.x + tolerance
        && point.z >= minimum.z - tolerance && point.z <= maximum.z + tolerance;
}

bool CDrawingText::HitTestScreen(
    DomPoint point,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    float tolerance) const {
    EnsureGeometry();
    const auto distance_to_segment = [point](DomPoint start, DomPoint end) {
        const double dx = static_cast<double>(end.x - start.x);
        const double dy = static_cast<double>(end.y - start.y);
        const double length_squared = dx * dx + dy * dy;
        if (length_squared <= 1.0e-12) {
            return std::hypot(
                static_cast<double>(point.x - start.x),
                static_cast<double>(point.y - start.y));
        }
        const double parameter = std::clamp(
            (static_cast<double>(point.x - start.x) * dx
             + static_cast<double>(point.y - start.y) * dy) / length_squared,
            0.0, 1.0);
        return std::hypot(
            static_cast<double>(point.x - start.x) - dx * parameter,
            static_cast<double>(point.y - start.y) - dy * parameter);
    };
    bool has_bounds = false;
    int minimum_x = 0;
    int maximum_x = 0;
    int minimum_y = 0;
    int maximum_y = 0;
    for (const auto& contour : contours_) {
        DomPoint previous{};
        bool has_previous = false;
        for (const CPoint3d& vertex : contour) {
            DomPoint current{};
            if (!world_to_screen(
                    {static_cast<float>(vertex.x), static_cast<float>(vertex.y),
                     static_cast<float>(vertex.z)}, current)) {
                has_previous = false;
                continue;
            }
            if (!has_bounds) {
                minimum_x = maximum_x = current.x;
                minimum_y = maximum_y = current.y;
                has_bounds = true;
            } else {
                minimum_x = std::min(minimum_x, current.x);
                maximum_x = std::max(maximum_x, current.x);
                minimum_y = std::min(minimum_y, current.y);
                maximum_y = std::max(maximum_y, current.y);
            }
            if (has_previous && distance_to_segment(previous, current) <= tolerance) {
                return true;
            }
            previous = current;
            has_previous = true;
        }
    }
    return has_bounds
        && static_cast<double>(point.x) >= minimum_x - tolerance
        && static_cast<double>(point.x) <= maximum_x + tolerance
        && static_cast<double>(point.y) >= minimum_y - tolerance
        && static_cast<double>(point.y) <= maximum_y + tolerance;
}

bool CDrawingText::Save(std::ostream& stream) const {
    stream << "TEXT " << insertion_.x << ' ' << insertion_.y << ' ' << insertion_.z
           << ' ' << height_ << ' ' << rotation_degrees_ << ' ' << text_ << '\n';
    return static_cast<bool>(stream);
}

std::unique_ptr<CAlfaObject> CDrawingText::Clone() const {
    auto copy = std::make_unique<CDrawingText>(
        text_, insertion_, height_, rotation_degrees_, font_family_);
    copy->SetColor(GetColor());
    copy->SetGroupName(GetGroupName());
    copy->SetLineWidth(GetLineWidth());
    copy->SetLineStyle(GetLineStyle());
    return copy;
}

void CDrawingText::Translate(Vec3 delta) {
    insertion_.x += delta.x;
    insertion_.y += delta.y;
    insertion_.z += delta.z;
    geometry_dirty_ = true;
}

void CDrawingText::Rotate(Vec3 center, Vec3 axis, float angle) {
    if (std::abs(axis.z) < 0.999f) return;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle) * (axis.z < 0.0f ? -1.0 : 1.0);
    const double x = insertion_.x - center.x;
    const double y = insertion_.y - center.y;
    insertion_.x = center.x + x * cosine - y * sine;
    insertion_.y = center.y + x * sine + y * cosine;
    rotation_degrees_ += angle * 180.0 / kDrawingPi * (axis.z < 0.0f ? -1.0 : 1.0);
    geometry_dirty_ = true;
}

void CDrawingText::Scale(Vec3 center, Vec3, float factor) {
    insertion_.x = center.x + (insertion_.x - center.x) * factor;
    insertion_.y = center.y + (insertion_.y - center.y) * factor;
    insertion_.z = center.z + (insertion_.z - center.z) * factor;
    height_ *= std::abs(factor);
    geometry_dirty_ = true;
}

bool CDrawingText::GetBounds(Vec3& min_point, Vec3& max_point) const {
    EnsureGeometry();
    bool initialized = false;
    for (const auto& contour : contours_) {
        for (const CPoint3d& point : contour) {
            const Vec3 value{static_cast<float>(point.x), static_cast<float>(point.y),
                             static_cast<float>(point.z)};
            if (!initialized) {
                min_point = max_point = value;
                initialized = true;
            } else {
                min_point.x = std::min(min_point.x, value.x);
                min_point.y = std::min(min_point.y, value.y);
                min_point.z = std::min(min_point.z, value.z);
                max_point.x = std::max(max_point.x, value.x);
                max_point.y = std::max(max_point.y, value.y);
                max_point.z = std::max(max_point.z, value.z);
            }
        }
    }
    return initialized;
}
