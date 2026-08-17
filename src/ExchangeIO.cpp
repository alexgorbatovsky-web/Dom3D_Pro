#include "ExchangeIO.h"

#include "BezierSpline.h"
#include "CBSpline.h"
#include "CMesh3D.h"
#include "CPolyline.h"
#include "DrawingText.h"
#include "CadCurve3D.h"
#include "SketchArcLine.h"
#include "SmartLine.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace {
constexpr double kPiD = 3.1415926535897932384626433832795;
constexpr double kEpsPointToMillimeter = 25.4 / 72.0;
constexpr double kHpglUnitToMillimeter = 0.025;

struct Path3d {
    std::string name;
    std::string layer;
    Color color{};
    std::vector<Vec3> points;
    bool closed = false;
    double line_width = 0.5;
    std::string line_style = "CONTINUOUS";
};

struct DxfAnalyticSegment {
    Path3d style;
    Vec3 start{};
    Vec3 middle{};
    Vec3 end{};
    bool arc = false;
};

Vec3 to_vec3(const CPoint3d& point)
{
    return {static_cast<float>(point.x),
            static_cast<float>(point.y),
            static_cast<float>(point.z)};
}

CPoint3d to_point3d(Vec3 point)
{
    return {point.x, point.y, point.z};
}

std::string trim(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string safe_layer(std::string value)
{
    if (value.empty()) value = "0";
    for (char& ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '<' || ch == '>'
            || ch == '/' || ch == '\\' || ch == ':' || ch == ';') {
            ch = '_';
        }
    }
    return value;
}

unsigned long pack_rgb(Color color)
{
    const auto channel = [](float value) {
        return static_cast<unsigned long>(
            std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (channel(color.r) << 16) | (channel(color.g) << 8)
        | channel(color.b);
}

Color unpack_rgb(unsigned long value)
{
    return {static_cast<float>((value >> 16) & 255u) / 255.0f,
            static_cast<float>((value >> 8) & 255u) / 255.0f,
            static_cast<float>(value & 255u) / 255.0f};
}

void append_sampled_spline(const CBSpline& spline, Path3d& path)
{
    const int samples = std::max(32, static_cast<int>(spline.GetPointCount()) * 12);
    path.points.reserve(static_cast<size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        path.points.push_back(to_vec3(
            spline.Evaluate(static_cast<float>(i) / static_cast<float>(samples))));
    }
    path.closed = spline.IsClosed();
}

std::vector<Path3d> collect_curve_paths(
    const CAlfaDoc& document,
    bool include_smart_lines = true,
    bool include_bezier_splines = true)
{
    std::vector<Path3d> paths;
    for (const auto& object_ptr : document.GetObjects()) {
        const CAlfaObject* object = object_ptr.get();
        if (!object || !document.IsObjectVisible(*object)) continue;

        Path3d path;
        path.name = object->GetName();
        path.layer = object->GetGroupName();
        path.color = object->GetColor();
        path.line_width = object->GetLineWidth();
        path.line_style = object->GetLineStyle();
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
            for (const CPoint3d& point : polyline->GetRoundedPathPoints())
                path.points.push_back(to_vec3(point));
            path.closed = polyline->IsClosed();
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object);
                   spline && (include_bezier_splines || !spline->IsBezierChain())) {
            append_sampled_spline(*spline, path);
        } else if (const auto* curve = dynamic_cast<const CCadCurve3D*>(object)) {
            path.points = curve->GetPoints();
        } else if (include_smart_lines
                   && (dynamic_cast<const CSmartLine*>(object) != nullptr)) {
            const auto* sketch = static_cast<const CSmartLine*>(object);
            for (const CPoint3d& point : sketch->GetProfilePointsWorld())
                path.points.push_back(to_vec3(point));
            path.closed = sketch->IsClosed();
        }
        if (path.points.size() >= 2) paths.push_back(std::move(path));
    }
    return paths;
}

using TriangleVisitor = std::function<void(Vec3, Vec3, Vec3, Color, const std::string&)>;

void visit_mesh_triangles(const CMesh3D& mesh,
                          Color color,
                          const std::string& name,
                          const TriangleVisitor& visitor)
{
    const std::vector<Vec3>& vertices = mesh.GetVertices();
    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        if (face.deleted || face.corners.size() < 3) continue;
        const size_t first = face.corners[0].v;
        if (first >= vertices.size()) continue;
        for (size_t i = 1; i + 1 < face.corners.size(); ++i) {
            const size_t second = face.corners[i].v;
            const size_t third = face.corners[i + 1].v;
            if (second >= vertices.size() || third >= vertices.size()) continue;
            visitor(vertices[first], vertices[second], vertices[third], color, name);
        }
    }
}

void visit_document_triangles(const CAlfaDoc& document, const TriangleVisitor& visitor)
{
    for (const auto& object_ptr : document.GetObjects()) {
        const CAlfaObject* object = object_ptr.get();
        if (!object || !document.IsObjectVisible(*object)) continue;
        if (const auto* mesh = dynamic_cast<const CMesh3D*>(object)) {
            visit_mesh_triangles(*mesh, object->GetColor(), object->GetName(), visitor);
        } else if (const auto* solid = dynamic_cast<const CSolid*>(object)) {
            if (!solid->EnsureRenderMesh()) continue;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(i);
                if (surface && surface->pMesh3D) {
                    visit_mesh_triangles(*surface->pMesh3D, object->GetColor(),
                                         object->GetName(), visitor);
                }
            }
        }
    }
}

std::unique_ptr<CPolyline> make_polyline(const Path3d& path)
{
    if (path.points.size() < 2) return {};
    auto result = std::make_unique<CPolyline>(path.name.empty() ? "Imported curve" : path.name);
    for (Vec3 point : path.points) result->AddPoint(to_point3d(point));
    result->SetClosed(path.closed);
    result->SetColor(path.color);
    result->SetGroupName(path.layer);
    result->SetLineWidth(path.line_width);
    result->SetLineStyle(path.line_style);
    return result;
}

std::unique_ptr<CAlfaObject> make_planar_arc(
    const Path3d& style,
    Vec3 center,
    double radius,
    double start_angle,
    double end_angle,
    bool full_circle)
{
    if (!std::isfinite(radius) || radius <= 1.0e-9
        || !std::isfinite(center.x) || !std::isfinite(center.y)
        || !std::isfinite(center.z)) {
        return {};
    }

    auto sketch = std::make_unique<CSmartLine>(
        style.name.empty() ? "Imported arc" : style.name);
    if (!sketch->SetCoordinateSystem(
            to_point3d(center), {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})) {
        return {};
    }
    const auto point_at = [radius](double angle) {
        return CPoint3d(
            radius * std::cos(angle), radius * std::sin(angle), 0.0);
    };

    if (full_circle) {
        auto first = std::make_unique<CSketchArcLine>(
            point_at(0.0), point_at(kPiD * 0.5), point_at(kPiD));
        auto second = std::make_unique<CSketchArcLine>(
            point_at(kPiD), point_at(kPiD * 1.5), point_at(kPiD * 2.0));
        if (!sketch->AddLine(std::move(first), false)
            || !sketch->AddLine(std::move(second), true)
            || !sketch->SetClosed(true)) {
            return {};
        }
    } else {
        while (end_angle < start_angle) end_angle += 2.0 * kPiD;
        const double sweep = end_angle - start_angle;
        if (!std::isfinite(sweep) || sweep <= 1.0e-9) return {};
        auto arc = std::make_unique<CSketchArcLine>(
            point_at(start_angle),
            point_at(start_angle + sweep * 0.5),
            point_at(end_angle));
        if (!arc->IsValid() || !sketch->AddLine(std::move(arc), false)) {
            return {};
        }
    }

    sketch->SetColor(style.color);
    sketch->SetGroupName(style.layer);
    sketch->SetLineWidth(style.line_width);
    sketch->SetLineStyle(style.line_style);
    return sketch;
}

std::unique_ptr<CAlfaObject> fit_sampled_planar_arc(const Path3d& path)
{
    // Some older Dom-3D DXF files export an ARC as a densely sampled legacy
    // POLYLINE without bulge values.  Recover the analytic circle only when
    // a sufficiently long run is planar, circular and angularly monotonic.
    if (path.closed || path.points.size() < 8) return {};
    const Vec3 first = path.points.front();
    const Vec3 middle = path.points[path.points.size() / 2];
    const Vec3 last = path.points.back();
    const double determinant = 2.0 * (
        first.x * (middle.y - last.y)
        + middle.x * (last.y - first.y)
        + last.x * (first.y - middle.y));
    if (std::abs(determinant) <= 1.0e-9) return {};
    const double first_squared = first.x * first.x + first.y * first.y;
    const double middle_squared = middle.x * middle.x + middle.y * middle.y;
    const double last_squared = last.x * last.x + last.y * last.y;
    const double center_x = (
        first_squared * (middle.y - last.y)
        + middle_squared * (last.y - first.y)
        + last_squared * (first.y - middle.y)) / determinant;
    const double center_y = (
        first_squared * (last.x - middle.x)
        + middle_squared * (first.x - last.x)
        + last_squared * (middle.x - first.x)) / determinant;
    const double radius = std::hypot(first.x - center_x, first.y - center_y);
    if (!std::isfinite(radius) || radius <= 1.0e-8) return {};

    const double distance_tolerance = std::max(1.0e-5, radius * 1.0e-4);
    double previous_angle = std::atan2(
        first.y - center_y, first.x - center_x);
    double sweep = 0.0;
    int direction = 0;
    for (size_t index = 0; index < path.points.size(); ++index) {
        const Vec3 point = path.points[index];
        if (std::abs(point.z - first.z) > distance_tolerance
            || std::abs(std::hypot(point.x - center_x, point.y - center_y)
                        - radius) > distance_tolerance) {
            return {};
        }
        if (index == 0) continue;
        const double angle = std::atan2(
            point.y - center_y, point.x - center_x);
        double delta = angle - previous_angle;
        while (delta <= -kPiD) delta += 2.0 * kPiD;
        while (delta > kPiD) delta -= 2.0 * kPiD;
        if (std::abs(delta) <= 1.0e-10) {
            previous_angle = angle;
            continue;
        }
        const int step_direction = delta > 0.0 ? 1 : -1;
        if (direction != 0 && step_direction != direction) return {};
        direction = step_direction;
        sweep += delta;
        previous_angle = angle;
    }
    if (direction == 0 || std::abs(sweep) < kPiD / 36.0
        || std::abs(sweep) >= 2.0 * kPiD - 1.0e-4) {
        return {};
    }

    const CPoint3d origin(center_x, center_y, first.z);
    const auto local = [center_x, center_y](Vec3 point) {
        return CPoint3d(point.x - center_x, point.y - center_y, 0.0);
    };
    auto arc = std::make_unique<CSketchArcLine>(
        local(first), local(middle), local(last));
    if (!arc->IsValid()) return {};
    auto sketch = std::make_unique<CSmartLine>(
        path.name.empty() ? "Imported arc" : path.name);
    if (!sketch->SetCoordinateSystem(
            origin, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})
        || !sketch->AddLine(std::move(arc), false)) {
        return {};
    }
    sketch->SetColor(path.color);
    sketch->SetGroupName(path.layer);
    sketch->SetLineWidth(path.line_width);
    sketch->SetLineStyle(path.line_style);
    return sketch;
}

void append_imported_path(
    const Path3d& path,
    std::vector<std::unique_ptr<CAlfaObject>>& objects)
{
    if (auto arc = fit_sampled_planar_arc(path)) {
        objects.push_back(std::move(arc));
    } else if (auto polyline = make_polyline(path)) {
        objects.push_back(std::move(polyline));
    }
}

double dxf_connection_tolerance(Vec3 first, Vec3 second)
{
    const double coordinate_scale = std::max({
        1.0,
        std::abs(static_cast<double>(first.x)),
        std::abs(static_cast<double>(first.y)),
        std::abs(static_cast<double>(first.z)),
        std::abs(static_cast<double>(second.x)),
        std::abs(static_cast<double>(second.y)),
        std::abs(static_cast<double>(second.z))});
    return std::max(1.0e-5, coordinate_scale * 2.0e-7);
}

bool dxf_points_touch(Vec3 first, Vec3 second)
{
    const double dx = static_cast<double>(first.x) - second.x;
    const double dy = static_cast<double>(first.y) - second.y;
    const double dz = static_cast<double>(first.z) - second.z;
    const double tolerance = dxf_connection_tolerance(first, second);
    return dx * dx + dy * dy + dz * dz <= tolerance * tolerance;
}

struct ImportedChainSegment {
    Vec3 start{};
    Vec3 middle{};
    Vec3 end{};
    bool arc = false;
};

bool normalize_xy(double& x, double& y)
{
    const double length = std::hypot(x, y);
    if (length <= 1.0e-12) return false;
    x /= length;
    y /= length;
    return true;
}

bool recover_dxf_fillet(
    const ImportedChainSegment& previous,
    const ImportedChainSegment& arc_segment,
    const ImportedChainSegment& next,
    Vec3& corner,
    double& radius)
{
    if (previous.arc || !arc_segment.arc || next.arc) return false;
    CSketchArcLine arc(
        to_point3d(arc_segment.start),
        to_point3d(arc_segment.middle),
        to_point3d(arc_segment.end));
    if (!arc.IsValid()) return false;

    double previous_x = previous.end.x - previous.start.x;
    double previous_y = previous.end.y - previous.start.y;
    double next_x = next.end.x - next.start.x;
    double next_y = next.end.y - next.start.y;
    CPoint3d start_tangent = arc.GetTangent(0.0);
    CPoint3d end_tangent = arc.GetTangent(1.0);
    double start_x = start_tangent.x;
    double start_y = start_tangent.y;
    double end_x = end_tangent.x;
    double end_y = end_tangent.y;
    if (!normalize_xy(previous_x, previous_y)
        || !normalize_xy(next_x, next_y)
        || !normalize_xy(start_x, start_y)
        || !normalize_xy(end_x, end_y)
        || previous_x * start_x + previous_y * start_y < 0.9999
        || next_x * end_x + next_y * end_y < 0.9999) {
        return false;
    }

    const double determinant = previous_x * next_y - previous_y * next_x;
    if (std::abs(determinant) <= 1.0e-8) return false;
    const double offset_x = next.start.x - previous.end.x;
    const double offset_y = next.start.y - previous.end.y;
    const double previous_parameter =
        (offset_x * next_y - offset_y * next_x) / determinant;
    const double next_parameter =
        (offset_x * previous_y - offset_y * previous_x) / determinant;
    if (previous_parameter < -1.0e-5 || next_parameter > 1.0e-5) return false;
    corner = {
        static_cast<float>(previous.end.x + previous_parameter * previous_x),
        static_cast<float>(previous.end.y + previous_parameter * previous_y),
        previous.end.z};

    const double first_side = std::hypot(
        arc_segment.middle.x - arc_segment.start.x,
        arc_segment.middle.y - arc_segment.start.y);
    const double second_side = std::hypot(
        arc_segment.end.x - arc_segment.middle.x,
        arc_segment.end.y - arc_segment.middle.y);
    const double chord = std::hypot(
        arc_segment.end.x - arc_segment.start.x,
        arc_segment.end.y - arc_segment.start.y);
    const double twice_area = std::abs(
        (arc_segment.middle.x - arc_segment.start.x)
            * (arc_segment.end.y - arc_segment.start.y)
        - (arc_segment.middle.y - arc_segment.start.y)
            * (arc_segment.end.x - arc_segment.start.x));
    if (twice_area <= 1.0e-12) return false;
    radius = first_side * second_side * chord / (2.0 * twice_area);
    return std::isfinite(radius) && radius > 1.0e-8;
}

void append_analytic_dxf_sketches(
    const std::vector<DxfAnalyticSegment>& segments,
    std::vector<std::unique_ptr<CAlfaObject>>& objects)
{
    struct OrientedSegment {
        size_t index = 0;
        bool reversed = false;
    };
    std::vector<bool> used(segments.size(), false);

    const auto oriented_start = [&segments](const OrientedSegment& item) {
        return item.reversed ? segments[item.index].end
                             : segments[item.index].start;
    };
    const auto oriented_end = [&segments](const OrientedSegment& item) {
        return item.reversed ? segments[item.index].start
                             : segments[item.index].end;
    };

    for (size_t seed = 0; seed < segments.size(); ++seed) {
        if (used[seed]) continue;
        std::vector<OrientedSegment> chain{{seed, false}};
        used[seed] = true;

        bool extended = true;
        while (extended) {
            extended = false;
            const Vec3 chain_end = oriented_end(chain.back());
            for (size_t candidate = 0; candidate < segments.size(); ++candidate) {
                if (used[candidate]
                    || segments[candidate].style.layer != segments[seed].style.layer) {
                    continue;
                }
                if (dxf_points_touch(chain_end, segments[candidate].start)) {
                    chain.push_back({candidate, false});
                } else if (dxf_points_touch(chain_end, segments[candidate].end)) {
                    chain.push_back({candidate, true});
                } else {
                    continue;
                }
                used[candidate] = true;
                extended = true;
                break;
            }
        }

        extended = true;
        while (extended) {
            extended = false;
            const Vec3 chain_start = oriented_start(chain.front());
            for (size_t candidate = 0; candidate < segments.size(); ++candidate) {
                if (used[candidate]
                    || segments[candidate].style.layer != segments[seed].style.layer) {
                    continue;
                }
                OrientedSegment item;
                if (dxf_points_touch(segments[candidate].end, chain_start)) {
                    item = {candidate, false};
                } else if (dxf_points_touch(segments[candidate].start, chain_start)) {
                    item = {candidate, true};
                } else {
                    continue;
                }
                chain.insert(chain.begin(), item);
                used[candidate] = true;
                extended = true;
                break;
            }
        }

        const bool closed = chain.size() > 1
            && dxf_points_touch(oriented_end(chain.back()),
                                oriented_start(chain.front()));
        std::vector<ImportedChainSegment> imported;
        imported.reserve(chain.size());
        for (const OrientedSegment& item : chain) {
            const DxfAnalyticSegment& segment = segments[item.index];
            imported.push_back({
                item.reversed ? segment.end : segment.start,
                segment.middle,
                item.reversed ? segment.start : segment.end,
                segment.arc});
        }

        struct RecoveredFillet {
            std::size_t previous = 0;
            double radius = 0.0;
        };
        std::vector<bool> remove(imported.size(), false);
        std::vector<RecoveredFillet> recovered_fillets;
        for (std::size_t index = 0; index < imported.size(); ++index) {
            if ((!closed && (index == 0 || index + 1 >= imported.size()))
                || !imported[index].arc) {
                continue;
            }
            const std::size_t previous = index == 0 ? imported.size() - 1 : index - 1;
            const std::size_t next = (index + 1) % imported.size();
            Vec3 corner{};
            double radius = 0.0;
            if (!remove[previous] && !remove[next]
                && recover_dxf_fillet(
                    imported[previous], imported[index], imported[next],
                    corner, radius)) {
                imported[previous].end = corner;
                imported[next].start = corner;
                remove[index] = true;
                recovered_fillets.push_back({previous, radius});
            }
        }

        std::vector<std::size_t> base_index(imported.size(), imported.size());
        std::vector<ImportedChainSegment> base_segments;
        for (std::size_t index = 0; index < imported.size(); ++index) {
            if (remove[index]) continue;
            base_index[index] = base_segments.size();
            base_segments.push_back(imported[index]);
        }
        if (base_segments.empty()) continue;

        const Vec3 origin = base_segments.front().start;
        auto sketch = std::make_unique<CSmartLine>(
            chain.size() == 1 ? segments[seed].style.name : "DXF Sketch");
        if (!sketch->SetCoordinateSystem(
                to_point3d(origin), {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})) {
            continue;
        }
        bool valid = true;
        for (const ImportedChainSegment& segment : base_segments) {
            const auto local = [&origin](Vec3 point) {
                return CPoint3d(point.x - origin.x,
                                point.y - origin.y,
                                point.z - origin.z);
            };
            std::unique_ptr<CLinkLine> line;
            if (segment.arc) {
                line = std::make_unique<CSketchArcLine>(
                    local(segment.start), local(segment.middle), local(segment.end));
            } else {
                line = std::make_unique<CLinkLine>(
                    local(segment.start), local(segment.end));
            }
            if (!sketch->AddLine(std::move(line), sketch->GetNumLines() > 0)) {
                valid = false;
                break;
            }
        }
        if (!valid) continue;
        if (closed && !sketch->SetClosed(true)) continue;
        for (const RecoveredFillet& fillet : recovered_fillets) {
            if (base_index[fillet.previous] >= base_segments.size()
                || !sketch->AddFillet(
                    base_index[fillet.previous], fillet.radius)) {
                valid = false;
                break;
            }
        }
        if (!valid) continue;
        sketch->SetColor(segments[seed].style.color);
        sketch->SetGroupName(segments[seed].style.layer);
        sketch->SetLineWidth(segments[seed].style.line_width);
        sketch->SetLineStyle(segments[seed].style.line_style);
        objects.push_back(std::move(sketch));
    }
}

void append_arc(std::vector<Vec3>& points,
                Vec3 center,
                double radius,
                double start_angle,
                double end_angle,
                int minimum_segments = 12)
{
    while (end_angle < start_angle) end_angle += 2.0 * kPiD;
    const double sweep = end_angle - start_angle;
    const int segments = std::max(minimum_segments,
        static_cast<int>(std::ceil(std::fabs(sweep) / (kPiD / 24.0))));
    for (int i = points.empty() ? 0 : 1; i <= segments; ++i) {
        const double angle = start_angle + sweep * static_cast<double>(i) / segments;
        points.push_back({center.x + static_cast<float>(radius * std::cos(angle)),
                          center.y + static_cast<float>(radius * std::sin(angle)),
                          center.z});
    }
}

struct DxfPair { int code = 0; std::string value; };

bool read_dxf_pairs(const std::string& path, std::vector<DxfPair>& pairs)
{
    std::ifstream file(path);
    if (!file) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) lines.push_back(line);
    for (size_t index = 0; index < lines.size();) {
        const std::string code_text = trim(lines[index]);
        size_t parsed = 0;
        int code = 0;
        try {
            code = std::stoi(code_text, &parsed);
        } catch (...) {
            parsed = 0;
        }
        if (parsed != code_text.size()) {
            // Dom3D Pro briefly emitted literal newlines inside group 1 TEXT
            // values. Recover those files instead of losing the text.
            if (!pairs.empty() && (pairs.back().code == 1 || pairs.back().code == 3)) {
                pairs.back().value += '\n';
                pairs.back().value += trim(lines[index]);
                ++index;
                continue;
            }
            return false;
        }
        if (index + 1 >= lines.size()) return false;
        pairs.push_back({code, trim(lines[index + 1])});
        index += 2;
    }
    return !pairs.empty();
}

double dxf_double(const std::vector<DxfPair>& entity, int code, double fallback = 0.0)
{
    for (const DxfPair& pair : entity) {
        if (pair.code == code) {
            try { return std::stod(pair.value); } catch (...) { return fallback; }
        }
    }
    return fallback;
}

int dxf_int(const std::vector<DxfPair>& entity, int code, int fallback = 0)
{
    for (const DxfPair& pair : entity) {
        if (pair.code == code) {
            try { return std::stoi(pair.value); } catch (...) { return fallback; }
        }
    }
    return fallback;
}

std::string dxf_string(const std::vector<DxfPair>& entity, int code)
{
    for (const DxfPair& pair : entity) if (pair.code == code) return pair.value;
    return {};
}

void apply_dxf_style(const std::vector<DxfPair>& entity, Path3d& path)
{
    path.layer = dxf_string(entity, 8);
    const int true_color = dxf_int(entity, 420, -1);
    if (true_color >= 0) {
        path.color = unpack_rgb(static_cast<unsigned long>(true_color));
    } else {
        const int indexed_color = std::abs(dxf_int(entity, 62, 0));
        switch (indexed_color) {
        case 1: path.color = {1.0f, 0.0f, 0.0f}; break;
        case 2: path.color = {1.0f, 1.0f, 0.0f}; break;
        case 3: path.color = {0.0f, 1.0f, 0.0f}; break;
        case 4: path.color = {0.0f, 1.0f, 1.0f}; break;
        case 5: path.color = {0.0f, 0.0f, 1.0f}; break;
        case 6: path.color = {1.0f, 0.0f, 1.0f}; break;
        case 7: path.color = {0.75f, 0.75f, 0.75f}; break;
        default: break;
        }
    }
    const int line_weight = dxf_int(entity, 370, -1);
    if (line_weight > 0) path.line_width = static_cast<double>(line_weight) / 100.0;
    const std::string line_style = dxf_string(entity, 6);
    if (!line_style.empty()) path.line_style = upper(line_style);
}

template <typename T>
void write_dxf_pair(std::ostream& stream, int code, const T& value)
{
    stream << code << "\n" << value << "\n";
}

void write_dxf_entity_style(std::ostream& stream, const CAlfaObject& object)
{
    write_dxf_pair(stream, 8, safe_layer(object.GetGroupName()));
    write_dxf_pair(stream, 420, pack_rgb(object.GetColor()));
    write_dxf_pair(stream, 6, object.GetLineStyle());
    write_dxf_pair(stream, 370, static_cast<int>(
        std::lround(object.GetLineWidth() * 100.0)));
}

double positive_radians(double angle)
{
    while (angle < 0.0) angle += 2.0 * kPiD;
    while (angle >= 2.0 * kPiD) angle -= 2.0 * kPiD;
    return angle;
}

bool write_dxf_arc_from_points(
    std::ostream& stream,
    CPoint3d start,
    CPoint3d middle,
    CPoint3d end,
    const std::string& layer,
    Color color)
{
    const double determinant = 2.0 * (start.x * (middle.y - end.y)
        + middle.x * (end.y - start.y) + end.x * (start.y - middle.y));
    if (std::abs(determinant) <= 1.0e-10
        || std::abs(start.z - middle.z) > 1.0e-6
        || std::abs(start.z - end.z) > 1.0e-6) {
        return false;
    }
    const double aa = start.x * start.x + start.y * start.y;
    const double bb = middle.x * middle.x + middle.y * middle.y;
    const double cc = end.x * end.x + end.y * end.y;
    const double center_x = (aa * (middle.y - end.y)
        + bb * (end.y - start.y) + cc * (start.y - middle.y)) / determinant;
    const double center_y = (aa * (end.x - middle.x)
        + bb * (start.x - end.x) + cc * (middle.x - start.x)) / determinant;
    const double radius = std::hypot(start.x - center_x, start.y - center_y);
    if (radius <= 1.0e-9) return false;
    double start_angle = std::atan2(start.y - center_y, start.x - center_x);
    double middle_angle = std::atan2(middle.y - center_y, middle.x - center_x);
    double end_angle = std::atan2(end.y - center_y, end.x - center_x);
    const double ccw_end = positive_radians(end_angle - start_angle);
    const double ccw_middle = positive_radians(middle_angle - start_angle);
    if (ccw_middle > ccw_end) {
        std::swap(start_angle, end_angle);
    }
    constexpr double radians_to_degrees = 180.0 / kPiD;
    write_dxf_pair(stream, 0, "ARC");
    write_dxf_pair(stream, 8, safe_layer(layer));
    write_dxf_pair(stream, 420, pack_rgb(color));
    write_dxf_pair(stream, 10, center_x);
    write_dxf_pair(stream, 20, center_y);
    write_dxf_pair(stream, 30, start.z);
    write_dxf_pair(stream, 40, radius);
    write_dxf_pair(stream, 50, positive_radians(start_angle) * radians_to_degrees);
    write_dxf_pair(stream, 51, positive_radians(end_angle) * radians_to_degrees);
    return true;
}

bool dxf_arc_bulge(CPoint3d start, CPoint3d middle, CPoint3d end, double& bulge)
{
    const double determinant = 2.0 * (start.x * (middle.y - end.y)
        + middle.x * (end.y - start.y) + end.x * (start.y - middle.y));
    if (std::abs(determinant) <= 1.0e-10
        || std::abs(start.z - middle.z) > 1.0e-6
        || std::abs(start.z - end.z) > 1.0e-6) {
        return false;
    }
    const double aa = start.x * start.x + start.y * start.y;
    const double bb = middle.x * middle.x + middle.y * middle.y;
    const double cc = end.x * end.x + end.y * end.y;
    const double center_x = (aa * (middle.y - end.y)
        + bb * (end.y - start.y) + cc * (start.y - middle.y)) / determinant;
    const double center_y = (aa * (end.x - middle.x)
        + bb * (start.x - end.x) + cc * (middle.x - start.x)) / determinant;
    const double start_angle = std::atan2(start.y - center_y, start.x - center_x);
    const double middle_angle = std::atan2(middle.y - center_y, middle.x - center_x);
    const double end_angle = std::atan2(end.y - center_y, end.x - center_x);
    const double ccw_end = positive_radians(end_angle - start_angle);
    const double ccw_middle = positive_radians(middle_angle - start_angle);
    const double sweep = ccw_middle <= ccw_end
        ? ccw_end : ccw_end - 2.0 * kPiD;
    bulge = std::tan(sweep * 0.25);
    return std::isfinite(bulge) && std::abs(bulge) > 1.0e-12;
}

bool write_analytic_smart_line(
    std::ostream& stream,
    const CSmartLine& sketch,
    size_t& written)
{
    const std::size_t line_count = sketch.GetNumLines();
    if (line_count == 0) return false;
    const SketchCoordinateSystem& system = sketch.GetCoordinateSystem();
    if (std::abs(std::abs(system.normal.z) - 1.0) > 1.0e-5) return false;
    for (std::size_t index = 0; index < line_count; ++index) {
        const CLinkLine* line = sketch.GetLine(index);
        if (!line || line->GetType() == LinkLineType::Bezier) return false;
    }

    std::vector<double> starts(line_count, 0.0);
    std::vector<double> ends(line_count, 1.0);
    std::vector<CPoint3d> trimmed_starts;
    std::vector<CPoint3d> trimmed_ends;
    trimmed_starts.reserve(line_count);
    trimmed_ends.reserve(line_count);
    for (std::size_t index = 0; index < line_count; ++index) {
        trimmed_starts.push_back(sketch.GetLine(index)->GetStart());
        trimmed_ends.push_back(sketch.GetLine(index)->GetEnd());
    }
    std::vector<FilletGeometry> fillets;
    fillets.reserve(sketch.GetNumFillets());
    for (std::size_t index = 0; index < sketch.GetNumFillets(); ++index) {
        const CFillet* fillet = sketch.GetFillet(index);
        if (!fillet) return false;
        const std::size_t first_index = fillet->GetFirstLineIndex();
        const std::size_t second_index = fillet->GetSecondLineIndex();
        if (first_index >= line_count || second_index >= line_count) return false;
        const FilletGeometry geometry = fillet->Calculate(
            *sketch.GetLine(first_index), *sketch.GetLine(second_index));
        if (!geometry.valid) return false;
        ends[first_index] = geometry.first_parameter;
        starts[second_index] = geometry.second_parameter;
        trimmed_ends[first_index] = geometry.tangent_on_first;
        trimmed_starts[second_index] = geometry.tangent_on_second;
        fillets.push_back(geometry);
    }

    struct PolylineSegment {
        CPoint3d start;
        CPoint3d middle;
        CPoint3d end;
        bool arc = false;
    };
    std::vector<int> fillet_after(line_count, -1);
    for (std::size_t index = 0; index < fillets.size(); ++index) {
        const CFillet* fillet = sketch.GetFillet(index);
        const std::size_t first = fillet->GetFirstLineIndex();
        const std::size_t second = fillet->GetSecondLineIndex();
        if (second != (first + 1) % line_count
            || (!sketch.IsClosed() && first + 1 >= line_count)
            || fillet_after[first] >= 0) {
            return false;
        }
        fillet_after[first] = static_cast<int>(index);
    }

    std::vector<PolylineSegment> output;
    output.reserve(line_count + fillets.size());
    for (std::size_t index = 0; index < line_count; ++index) {
        const CLinkLine* line = sketch.GetLine(index);
        const bool curved = line->GetType() == LinkLineType::Arc;
        const CPoint3d local_start = curved
            ? line->GetPoint(starts[index]) : trimmed_starts[index];
        const CPoint3d local_end = curved
            ? line->GetPoint(ends[index]) : trimmed_ends[index];
        const CPoint3d world_start = sketch.LocalToWorld(local_start);
        const CPoint3d world_end = sketch.LocalToWorld(local_end);
        if (curved) {
            const CPoint3d world_middle = sketch.LocalToWorld(line->GetPoint(
                (starts[index] + ends[index]) * 0.5));
            output.push_back({world_start, world_middle, world_end, true});
        } else {
            output.push_back({world_start, {}, world_end, false});
        }
        if (fillet_after[index] >= 0) {
            const FilletGeometry& geometry = fillets[static_cast<size_t>(
                fillet_after[index])];
            const CPoint3d local_middle(
                geometry.center.x
                    + (geometry.tangent_on_first.x - geometry.center.x)
                        * std::cos(geometry.signed_angle * 0.5)
                    - (geometry.tangent_on_first.y - geometry.center.y)
                        * std::sin(geometry.signed_angle * 0.5),
                geometry.center.y
                    + (geometry.tangent_on_first.x - geometry.center.x)
                        * std::sin(geometry.signed_angle * 0.5)
                    + (geometry.tangent_on_first.y - geometry.center.y)
                        * std::cos(geometry.signed_angle * 0.5),
                geometry.center.z);
            output.push_back({
                sketch.LocalToWorld(geometry.tangent_on_first),
                sketch.LocalToWorld(local_middle),
                sketch.LocalToWorld(geometry.tangent_on_second),
                true});
        }
    }

    if (output.empty()) return false;
    for (std::size_t index = 0; index + 1 < output.size(); ++index) {
        if (std::hypot(output[index].end.x - output[index + 1].start.x,
                       output[index].end.y - output[index + 1].start.y) > 1.0e-5
            || std::abs(output[index].end.z - output[index + 1].start.z) > 1.0e-5) {
            return false;
        }
    }
    if (sketch.IsClosed()
        && (std::hypot(output.back().end.x - output.front().start.x,
                       output.back().end.y - output.front().start.y) > 1.0e-5
            || std::abs(output.back().end.z - output.front().start.z) > 1.0e-5)) {
        return false;
    }

    const std::size_t vertex_count = output.size() + (sketch.IsClosed() ? 0 : 1);
    write_dxf_pair(stream, 0, "LWPOLYLINE");
    write_dxf_entity_style(stream, sketch);
    write_dxf_pair(stream, 90, vertex_count);
    write_dxf_pair(stream, 70, sketch.IsClosed() ? 1 : 0);
    write_dxf_pair(stream, 38, output.front().start.z);
    for (const PolylineSegment& segment : output) {
        write_dxf_pair(stream, 10, segment.start.x);
        write_dxf_pair(stream, 20, segment.start.y);
        if (segment.arc) {
            double bulge = 0.0;
            if (!dxf_arc_bulge(segment.start, segment.middle, segment.end, bulge)) {
                return false;
            }
            write_dxf_pair(stream, 42, bulge);
        }
    }
    if (!sketch.IsClosed()) {
        write_dxf_pair(stream, 10, output.back().end.x);
        write_dxf_pair(stream, 20, output.back().end.y);
    }
    ++written;
    return true;
}

std::vector<double> parse_numbers(std::string value)
{
    for (char& ch : value) if (ch == ',') ch = ' ';
    std::istringstream stream(value);
    std::vector<double> values;
    double number = 0.0;
    while (stream >> number) values.push_back(number);
    return values;
}

void add_mesh_wire_paths(const CAlfaDoc& document, std::vector<Path3d>& paths)
{
    std::set<std::array<long long, 6>> edges;
    const auto quantize = [](float value) {
        return static_cast<long long>(std::llround(value * 10000.0));
    };
    visit_document_triangles(document, [&](Vec3 a, Vec3 b, Vec3 c, Color color,
                                           const std::string& name) {
        const std::array<std::pair<Vec3, Vec3>, 3> segments{{{a, b}, {b, c}, {c, a}}};
        for (const auto& segment : segments) {
            Vec3 first = segment.first;
            Vec3 second = segment.second;
            std::array<long long, 3> qa{quantize(first.x), quantize(first.y), quantize(first.z)};
            std::array<long long, 3> qb{quantize(second.x), quantize(second.y), quantize(second.z)};
            if (qb < qa) { std::swap(qa, qb); std::swap(first, second); }
            const std::array<long long, 6> key{qa[0], qa[1], qa[2], qb[0], qb[1], qb[2]};
            if (edges.insert(key).second)
                paths.push_back({name, {}, color, {first, second}, false});
        }
    });
}

bool finite_vec(Vec3 point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}
}

bool DxfIO::Import(const std::string& path,
                   std::vector<std::unique_ptr<CAlfaObject>>& objects,
                   std::string& error) const
{
    objects.clear();
    std::vector<DxfPair> pairs;
    if (!read_dxf_pairs(path, pairs)) {
        error = "Could not read ASCII DXF file.";
        return false;
    }

    std::vector<Vec3> face_vertices;
    std::vector<CMesh3D::Face> face_mesh;
    Path3d legacy;
    bool collecting_legacy = false;
    int curve_index = 1;
    std::vector<DxfAnalyticSegment> analytic_segments;

    for (size_t start = 0; start < pairs.size();) {
        if (pairs[start].code != 0) { ++start; continue; }
        const std::string type = upper(pairs[start].value);
        size_t end = start + 1;
        while (end < pairs.size() && pairs[end].code != 0) ++end;
        const std::vector<DxfPair> entity(pairs.begin() + static_cast<ptrdiff_t>(start + 1),
                                          pairs.begin() + static_cast<ptrdiff_t>(end));
        start = end;

        Path3d curve;
        curve.name = "DXF Curve " + std::to_string(curve_index++);
        apply_dxf_style(entity, curve);
        if (type == "LINE") {
            curve.points = {{static_cast<float>(dxf_double(entity, 10)),
                             static_cast<float>(dxf_double(entity, 20)),
                             static_cast<float>(dxf_double(entity, 30))},
                            {static_cast<float>(dxf_double(entity, 11)),
                             static_cast<float>(dxf_double(entity, 21)),
                             static_cast<float>(dxf_double(entity, 31))}};
            if (std::abs(static_cast<double>(curve.points[0].z)
                         - curve.points[1].z)
                <= dxf_connection_tolerance(curve.points[0], curve.points[1])) {
                analytic_segments.push_back(
                    {curve, curve.points[0], {}, curve.points[1], false});
            } else {
                append_imported_path(curve, objects);
            }
            continue;
        } else if (type == "LWPOLYLINE") {
            const double constant_width = dxf_double(entity, 43);
            const double start_width = dxf_double(entity, 40);
            if (constant_width > 0.0 || start_width > 0.0) {
                curve.line_width = std::max(constant_width, start_width);
            }
            const float elevation = static_cast<float>(dxf_double(entity, 38));
            Vec3 point{};
            bool has_point = false;
            double bulge = 0.0;
            std::vector<double> bulges;
            for (const DxfPair& pair : entity) {
                if (pair.code == 10) {
                    if (has_point) {
                        curve.points.push_back(point);
                        bulges.push_back(bulge);
                    }
                    point = {static_cast<float>(std::stod(pair.value)), 0.0f, elevation};
                    has_point = true;
                    bulge = 0.0;
                } else if (pair.code == 20 && has_point) {
                    point.y = static_cast<float>(std::stod(pair.value));
                } else if (pair.code == 30 && has_point) {
                    point.z = static_cast<float>(std::stod(pair.value));
                } else if (pair.code == 42 && has_point) {
                    bulge = std::stod(pair.value);
                }
            }
            if (has_point) {
                curve.points.push_back(point);
                bulges.push_back(bulge);
            }
            curve.closed = (dxf_int(entity, 70) & 1) != 0;
            const std::size_t segment_count = curve.points.size() >= 2
                ? curve.points.size() - (curve.closed ? 0 : 1) : 0;
            for (std::size_t index = 0; index < segment_count; ++index) {
                const Vec3 segment_start = curve.points[index];
                const Vec3 segment_end = curve.points[
                    (index + 1) % curve.points.size()];
                const double segment_bulge = bulges[index];
                if (std::isfinite(segment_bulge)
                    && std::abs(segment_bulge) > 1.0e-12) {
                    const Vec3 middle{
                        static_cast<float>((segment_start.x + segment_end.x) * 0.5
                            + (segment_end.y - segment_start.y) * segment_bulge * 0.5),
                        static_cast<float>((segment_start.y + segment_end.y) * 0.5
                            - (segment_end.x - segment_start.x) * segment_bulge * 0.5),
                        segment_start.z};
                    analytic_segments.push_back(
                        {curve, segment_start, middle, segment_end, true});
                } else {
                    analytic_segments.push_back(
                        {curve, segment_start, {}, segment_end, false});
                }
            }
            continue;
        } else if (type == "CIRCLE" || type == "ARC") {
            const Vec3 center{static_cast<float>(dxf_double(entity, 10)),
                              static_cast<float>(dxf_double(entity, 20)),
                              static_cast<float>(dxf_double(entity, 30))};
            const double radius = dxf_double(entity, 40);
            const double start_angle = type == "CIRCLE" ? 0.0
                : dxf_double(entity, 50) * kPiD / 180.0;
            const double end_angle = type == "CIRCLE" ? 2.0 * kPiD
                : dxf_double(entity, 51) * kPiD / 180.0;
            curve.name = type == "CIRCLE"
                ? "DXF Circle " + std::to_string(curve_index - 1)
                : "DXF Arc " + std::to_string(curve_index - 1);
            if (type == "CIRCLE") {
                if (auto analytic_curve = make_planar_arc(
                        curve, center, radius, start_angle, end_angle, true)) {
                    objects.push_back(std::move(analytic_curve));
                }
            } else if (std::isfinite(radius) && radius > 1.0e-9) {
                double adjusted_end = end_angle;
                while (adjusted_end < start_angle) adjusted_end += 2.0 * kPiD;
                const auto point_at = [center, radius](double angle) {
                    return Vec3{
                        static_cast<float>(center.x + radius * std::cos(angle)),
                        static_cast<float>(center.y + radius * std::sin(angle)),
                        center.z};
                };
                analytic_segments.push_back({
                    curve,
                    point_at(start_angle),
                    point_at(start_angle + (adjusted_end - start_angle) * 0.5),
                    point_at(adjusted_end),
                    true});
            }
            continue;
        } else if (type == "TEXT" || type == "MTEXT") {
            std::string text;
            for (const DxfPair& pair : entity) {
                if (pair.code == 1 || (type == "MTEXT" && pair.code == 3)) {
                    text += pair.value;
                }
            }
            for (std::size_t position = 0;
                 (position = text.find("\\P", position)) != std::string::npos;) {
                text.replace(position, 2, "\n");
                ++position;
            }
            auto drawing_text = std::make_unique<CDrawingText>(
                text,
                CPoint3d(dxf_double(entity, 10),
                         dxf_double(entity, 20),
                         dxf_double(entity, 30)),
                std::max(0.01, dxf_double(entity, 40, 1.0)),
                dxf_double(entity, 50),
                dxf_string(entity, 7).empty() ? "Arial" : dxf_string(entity, 7));
            drawing_text->SetColor(curve.color);
            drawing_text->SetGroupName(curve.layer);
            drawing_text->SetLineWidth(curve.line_width);
            drawing_text->SetLineStyle(curve.line_style);
            objects.push_back(std::move(drawing_text));
            continue;
        } else if (type == "POLYLINE") {
            const double start_width = dxf_double(entity, 40);
            const double end_width = dxf_double(entity, 41);
            if (start_width > 0.0 || end_width > 0.0) {
                curve.line_width = std::max(start_width, end_width);
            }
            legacy = curve;
            legacy.closed = (dxf_int(entity, 70) & 1) != 0;
            collecting_legacy = true;
            continue;
        } else if (type == "VERTEX" && collecting_legacy) {
            legacy.points.push_back({static_cast<float>(dxf_double(entity, 10)),
                                     static_cast<float>(dxf_double(entity, 20)),
                                     static_cast<float>(dxf_double(entity, 30))});
            continue;
        } else if (type == "SEQEND" && collecting_legacy) {
            append_imported_path(legacy, objects);
            collecting_legacy = false;
            continue;
        } else if (type == "3DFACE") {
            std::array<Vec3, 4> vertices{};
            int count = 4;
            for (int i = 0; i < 4; ++i) {
                vertices[static_cast<size_t>(i)] = {
                    static_cast<float>(dxf_double(entity, 10 + i)),
                    static_cast<float>(dxf_double(entity, 20 + i)),
                    static_cast<float>(dxf_double(entity, 30 + i))};
            }
            if (dot(vertices[3] - vertices[2], vertices[3] - vertices[2]) < 1.0e-12f) count = 3;
            CMesh3D::Face face;
            for (int i = 0; i < count; ++i) {
                face.corners.push_back({face_vertices.size(), 0, 0});
                face_vertices.push_back(vertices[static_cast<size_t>(i)]);
            }
            face_mesh.push_back(std::move(face));
            continue;
        } else {
            continue;
        }
    }
    if (collecting_legacy) {
        append_imported_path(legacy, objects);
    }
    append_analytic_dxf_sketches(analytic_segments, objects);
    if (!face_mesh.empty()) {
        auto mesh = std::make_unique<CMesh3D>("DXF 3D Faces");
        if (mesh->SetGeometry(std::move(face_vertices), std::move(face_mesh)))
            objects.push_back(std::move(mesh));
    }
    if (objects.empty()) {
        error = "DXF contains no supported LINE, POLYLINE, ARC, CIRCLE, TEXT, MTEXT, or 3DFACE entities.";
        return false;
    }
    return true;
}

bool DxfIO::Export(const std::string& path, const CAlfaDoc& document,
                   std::string& error) const
{
    std::ofstream file(path);
    if (!file) { error = "Could not create DXF file."; return false; }
    file << std::setprecision(12);
    write_dxf_pair(file, 0, "SECTION"); write_dxf_pair(file, 2, "HEADER");
    write_dxf_pair(file, 9, "$ACADVER"); write_dxf_pair(file, 1, "AC1015");
    write_dxf_pair(file, 0, "ENDSEC");
    write_dxf_pair(file, 0, "SECTION"); write_dxf_pair(file, 2, "ENTITIES");
    size_t written = 0;
    for (const auto& object : document.GetObjects()) {
        if (!object || !document.IsObjectVisible(*object)) continue;
        if (const auto* text = dynamic_cast<const CDrawingText*>(object.get())) {
            std::vector<std::string> lines(1);
            for (char character : text->GetText()) {
                if (character == '\n') {
                    lines.emplace_back();
                } else if (character != '\r') {
                    lines.back().push_back(character);
                }
            }
            const double angle = text->GetRotationDegrees() * kPiD / 180.0;
            const double advance = text->GetLineAdvance();
            for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
                // A DXF TEXT value is strictly one physical line. Separate
                // entities are understood by old Dom-3D versions as well as
                // by current CAD applications; keep blank lines as spaces so
                // their vertical advance is retained on a later import.
                const double offset = advance * static_cast<double>(line_index);
                const CPoint3d insertion{
                    text->GetInsertion().x + offset * std::sin(angle),
                    text->GetInsertion().y - offset * std::cos(angle),
                    text->GetInsertion().z};
                write_dxf_pair(file, 0, "TEXT");
                write_dxf_entity_style(file, *text);
                write_dxf_pair(file, 10, insertion.x);
                write_dxf_pair(file, 20, insertion.y);
                write_dxf_pair(file, 30, insertion.z);
                write_dxf_pair(file, 40, text->GetHeight());
                write_dxf_pair(file, 50, text->GetRotationDegrees());
                write_dxf_pair(file, 7, text->GetFontFamily());
                write_dxf_pair(file, 1, lines[line_index].empty() ? " " : lines[line_index]);
                ++written;
            }
            continue;
        }
        const auto* sketch = dynamic_cast<const CSmartLine*>(object.get());
        if (!sketch) continue;
        std::ostringstream analytic_entities;
        analytic_entities << std::setprecision(12);
        size_t analytic_written = 0;
        if (write_analytic_smart_line(
                analytic_entities, *sketch, analytic_written)) {
            file << analytic_entities.str();
            written += analytic_written;
            continue;
        }
        Path3d fallback;
        fallback.name = sketch->GetName();
        fallback.layer = sketch->GetGroupName();
        fallback.color = sketch->GetColor();
        fallback.line_width = sketch->GetLineWidth();
        fallback.line_style = sketch->GetLineStyle();
        fallback.closed = sketch->IsClosed();
        for (const CPoint3d& point : sketch->GetProfilePointsWorld()) {
            fallback.points.push_back(to_vec3(point));
        }
        if (fallback.points.size() >= 2) {
            write_dxf_pair(file, 0, "POLYLINE");
            write_dxf_pair(file, 8, safe_layer(fallback.layer));
            write_dxf_pair(file, 66, 1);
            write_dxf_pair(file, 70, fallback.closed ? 9 : 8);
            write_dxf_pair(file, 420, pack_rgb(fallback.color));
            write_dxf_pair(file, 6, fallback.line_style);
            write_dxf_pair(file, 370, static_cast<int>(std::lround(fallback.line_width * 100.0)));
            for (Vec3 point : fallback.points) {
                write_dxf_pair(file, 0, "VERTEX");
                write_dxf_pair(file, 8, safe_layer(fallback.layer));
                write_dxf_pair(file, 10, point.x);
                write_dxf_pair(file, 20, point.y);
                write_dxf_pair(file, 30, point.z);
                write_dxf_pair(file, 70, 32);
            }
            write_dxf_pair(file, 0, "SEQEND");
            ++written;
        }
    }
    for (const Path3d& curve : collect_curve_paths(document, false)) {
        write_dxf_pair(file, 0, "POLYLINE");
        write_dxf_pair(file, 8, safe_layer(curve.layer));
        write_dxf_pair(file, 66, 1); write_dxf_pair(file, 70, curve.closed ? 9 : 8);
        write_dxf_pair(file, 420, pack_rgb(curve.color));
        write_dxf_pair(file, 6, curve.line_style);
        write_dxf_pair(file, 370, static_cast<int>(std::lround(curve.line_width * 100.0)));
        for (Vec3 point : curve.points) {
            write_dxf_pair(file, 0, "VERTEX"); write_dxf_pair(file, 8, safe_layer(curve.layer));
            write_dxf_pair(file, 10, point.x); write_dxf_pair(file, 20, point.y);
            write_dxf_pair(file, 30, point.z); write_dxf_pair(file, 70, 32);
        }
        write_dxf_pair(file, 0, "SEQEND"); ++written;
    }
    visit_document_triangles(document, [&](Vec3 a, Vec3 b, Vec3 c, Color color,
                                           const std::string& name) {
        write_dxf_pair(file, 0, "3DFACE"); write_dxf_pair(file, 8, safe_layer(name));
        write_dxf_pair(file, 420, pack_rgb(color));
        const std::array<Vec3, 4> vertices{a, b, c, c};
        for (int i = 0; i < 4; ++i) {
            write_dxf_pair(file, 10 + i, vertices[static_cast<size_t>(i)].x);
            write_dxf_pair(file, 20 + i, vertices[static_cast<size_t>(i)].y);
            write_dxf_pair(file, 30 + i, vertices[static_cast<size_t>(i)].z);
        }
        ++written;
    });
    write_dxf_pair(file, 0, "ENDSEC"); write_dxf_pair(file, 0, "EOF");
    if (written == 0) { error = "There is no visible curve or mesh geometry to export."; return false; }
    return static_cast<bool>(file);
}

struct EpsPathSegment {
    Vec3 start{};
    Vec3 control1{};
    Vec3 control2{};
    Vec3 end{};
    bool bezier = false;
};

struct ImportedEpsPath {
    std::vector<EpsPathSegment> segments;
    Vec3 start{};
    Vec3 cursor{};
    bool has_start = false;
    bool closed = false;
};

bool eps_points_touch(Vec3 first, Vec3 second)
{
    const double dx = static_cast<double>(first.x) - second.x;
    const double dy = static_cast<double>(first.y) - second.y;
    const double dz = static_cast<double>(first.z) - second.z;
    return dx * dx + dy * dy + dz * dz <= 1.0e-10;
}

std::unique_ptr<CAlfaObject> make_eps_path_object(
    const ImportedEpsPath& path,
    const std::string& name)
{
    if (path.segments.empty()) return {};
    const bool has_bezier = std::any_of(
        path.segments.begin(), path.segments.end(),
        [](const EpsPathSegment& segment) { return segment.bezier; });
    if (!has_bezier) {
        Path3d polyline;
        polyline.name = name;
        polyline.closed = path.closed;
        polyline.points.push_back(path.segments.front().start);
        for (const EpsPathSegment& segment : path.segments) {
            polyline.points.push_back(segment.end);
        }
        if (polyline.closed && polyline.points.size() > 1
            && eps_points_touch(polyline.points.front(), polyline.points.back())) {
            polyline.points.pop_back();
        }
        return make_polyline(polyline);
    }

    const Vec3 origin = path.segments.front().start;
    auto sketch = std::make_unique<CSmartLine>(name);
    if (!sketch->SetCoordinateSystem(
            to_point3d(origin), {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})) {
        return {};
    }
    const auto local = [origin](Vec3 point) {
        return CPoint3d(point.x - origin.x,
                        point.y - origin.y,
                        point.z - origin.z);
    };
    for (const EpsPathSegment& segment : path.segments) {
        std::unique_ptr<CLinkLine> line;
        if (segment.bezier) {
            line = std::make_unique<CBezierSpline>(
                local(segment.start), local(segment.control1),
                local(segment.control2), local(segment.end));
        } else {
            line = std::make_unique<CLinkLine>(
                local(segment.start), local(segment.end));
        }
        if (!sketch->AddLine(
                std::move(line), sketch->GetNumLines() != 0)) {
            return {};
        }
    }
    if (path.closed && !sketch->SetClosed(true)) return {};
    return sketch;
}

void write_eps_point(std::ostream& stream,
                     CPoint3d point,
                     double min_x,
                     double min_y,
                     double scale)
{
    stream << (point.x - min_x) * scale + 10.0 << ' '
           << (point.y - min_y) * scale + 10.0;
}

void write_eps_polyline_path(std::ostream& stream,
                             const Path3d& path,
                             double min_x,
                             double min_y,
                             double scale)
{
    if (path.points.size() < 2) return;
    stream << path.color.r << ' ' << path.color.g << ' ' << path.color.b
           << " setrgbcolor\nnewpath\n";
    for (std::size_t index = 0; index < path.points.size(); ++index) {
        write_eps_point(stream, to_point3d(path.points[index]),
                        min_x, min_y, scale);
        stream << (index == 0 ? " moveto\n" : " lineto\n");
    }
    if (path.closed) stream << "closepath\n";
    stream << "stroke\n";
}

struct EpsCubic {
    CPoint3d start;
    CPoint3d control1;
    CPoint3d control2;
    CPoint3d end;
};

CPoint3d eps_lerp(CPoint3d first, CPoint3d second, double parameter)
{
    return first * (1.0 - parameter) + second * parameter;
}

std::pair<EpsCubic, EpsCubic> split_eps_cubic(
    const EpsCubic& curve,
    double parameter)
{
    const CPoint3d a = eps_lerp(curve.start, curve.control1, parameter);
    const CPoint3d b = eps_lerp(curve.control1, curve.control2, parameter);
    const CPoint3d c = eps_lerp(curve.control2, curve.end, parameter);
    const CPoint3d d = eps_lerp(a, b, parameter);
    const CPoint3d e = eps_lerp(b, c, parameter);
    const CPoint3d point = eps_lerp(d, e, parameter);
    return {{curve.start, a, d, point},
            {point, e, c, curve.end}};
}

EpsCubic trim_eps_bezier(const CBezierSpline& bezier,
                         double start_parameter,
                         double end_parameter)
{
    EpsCubic curve{
        bezier.GetStart(), bezier.GetControl1(),
        bezier.GetControl2(), bezier.GetEnd()};
    const double start = std::clamp(start_parameter, 0.0, 1.0);
    const double end = std::clamp(end_parameter, start, 1.0);
    if (end < 1.0) curve = split_eps_cubic(curve, end).first;
    if (start > 0.0 && end > 1.0e-12) {
        curve = split_eps_cubic(curve, start / end).second;
    }
    return curve;
}

void write_eps_cubic(std::ostream& stream,
                     const EpsCubic& curve,
                     const CSmartLine& sketch,
                     double min_x,
                     double min_y,
                     double scale)
{
    write_eps_point(stream, sketch.LocalToWorld(curve.control1),
                    min_x, min_y, scale);
    stream << ' ';
    write_eps_point(stream, sketch.LocalToWorld(curve.control2),
                    min_x, min_y, scale);
    stream << ' ';
    write_eps_point(stream, sketch.LocalToWorld(curve.end),
                    min_x, min_y, scale);
    stream << " curveto\n";
}

void write_eps_fillet(std::ostream& stream,
                      const FilletGeometry& fillet,
                      const CSmartLine& sketch,
                      double min_x,
                      double min_y,
                      double scale)
{
    const int pieces = std::max(1, static_cast<int>(std::ceil(
        std::abs(fillet.signed_angle) / (kPiD * 0.5))));
    const double start_angle = std::atan2(
        fillet.tangent_on_first.y - fillet.center.y,
        fillet.tangent_on_first.x - fillet.center.x);
    const double radius = std::hypot(
        fillet.tangent_on_first.x - fillet.center.x,
        fillet.tangent_on_first.y - fillet.center.y);
    for (int piece = 0; piece < pieces; ++piece) {
        const double first_angle = start_angle
            + fillet.signed_angle * piece / pieces;
        const double second_angle = start_angle
            + fillet.signed_angle * (piece + 1) / pieces;
        const double sweep = second_angle - first_angle;
        const double handle = 4.0 / 3.0 * std::tan(sweep * 0.25) * radius;
        const CPoint3d start(
            fillet.center.x + radius * std::cos(first_angle),
            fillet.center.y + radius * std::sin(first_angle),
            fillet.center.z);
        const CPoint3d end(
            fillet.center.x + radius * std::cos(second_angle),
            fillet.center.y + radius * std::sin(second_angle),
            fillet.center.z);
        const CPoint3d control1(
            start.x - std::sin(first_angle) * handle,
            start.y + std::cos(first_angle) * handle,
            start.z);
        const CPoint3d control2(
            end.x + std::sin(second_angle) * handle,
            end.y - std::cos(second_angle) * handle,
            end.z);
        write_eps_cubic(stream, {start, control1, control2, end}, sketch,
                        min_x, min_y, scale);
    }
}

bool write_eps_smart_line(std::ostream& stream,
                          const CSmartLine& sketch,
                          double min_x,
                          double min_y,
                          double scale)
{
    if (sketch.GetNumLines() == 0) return false;
    const std::size_t line_count = sketch.GetNumLines();
    std::vector<double> starts(line_count, 0.0);
    std::vector<double> ends(line_count, 1.0);
    std::vector<int> fillet_after(line_count, -1);
    std::vector<FilletGeometry> fillets;
    fillets.reserve(sketch.GetNumFillets());
    for (std::size_t index = 0; index < sketch.GetNumFillets(); ++index) {
        const CFillet* fillet = sketch.GetFillet(index);
        if (!fillet) return false;
        const std::size_t first = fillet->GetFirstLineIndex();
        const std::size_t second = fillet->GetSecondLineIndex();
        if (first >= line_count || second >= line_count
            || second != (first + 1) % line_count
            || fillet_after[first] >= 0) {
            return false;
        }
        const FilletGeometry geometry = fillet->Calculate(
            *sketch.GetLine(first), *sketch.GetLine(second));
        if (!geometry.valid) return false;
        ends[first] = geometry.first_parameter;
        starts[second] = geometry.second_parameter;
        fillet_after[first] = static_cast<int>(fillets.size());
        fillets.push_back(geometry);
    }
    stream << sketch.GetColor().r << ' ' << sketch.GetColor().g << ' '
           << sketch.GetColor().b << " setrgbcolor\nnewpath\n";
    CPoint3d cursor{};
    bool has_cursor = false;
    for (std::size_t index = 0; index < sketch.GetNumLines(); ++index) {
        const CLinkLine* line = sketch.GetLine(index);
        if (!line) return false;
        const CPoint3d local_start = line->GetPoint(starts[index]);
        const CPoint3d local_end = line->GetPoint(ends[index]);
        const CPoint3d start = sketch.LocalToWorld(local_start);
        if (!has_cursor
            || std::abs(cursor.x - start.x) > 1.0e-7
            || std::abs(cursor.y - start.y) > 1.0e-7) {
            write_eps_point(stream, start, min_x, min_y, scale);
            stream << " moveto\n";
        }
        if (line->GetType() == LinkLineType::Bezier) {
            const auto* bezier = dynamic_cast<const CBezierSpline*>(line);
            if (!bezier) return false;
            write_eps_cubic(stream,
                trim_eps_bezier(*bezier, starts[index], ends[index]),
                sketch, min_x, min_y, scale);
        } else if (line->GetType() == LinkLineType::Arc) {
            constexpr int samples = 32;
            for (int sample = 1; sample <= samples; ++sample) {
                const double parameter = starts[index]
                    + (ends[index] - starts[index]) * sample / samples;
                write_eps_point(stream, sketch.LocalToWorld(
                                    line->GetPoint(parameter)),
                                min_x, min_y, scale);
                stream << " lineto\n";
            }
        } else {
            write_eps_point(stream, sketch.LocalToWorld(local_end),
                            min_x, min_y, scale);
            stream << " lineto\n";
        }
        cursor = sketch.LocalToWorld(local_end);
        has_cursor = true;
        if (fillet_after[index] >= 0) {
            write_eps_fillet(stream,
                fillets[static_cast<std::size_t>(fillet_after[index])],
                sketch, min_x, min_y, scale);
            cursor = sketch.LocalToWorld(
                fillets[static_cast<std::size_t>(fillet_after[index])]
                    .tangent_on_second);
        }
    }
    if (sketch.IsClosed()) stream << "closepath\n";
    stream << "stroke\n";
    return true;
}

bool write_eps_bezier_spline(std::ostream& stream,
                             const CBSpline& spline,
                             double min_x,
                             double min_y,
                             double scale)
{
    if (!spline.IsBezierChain()) return false;
    const std::vector<CPoint3d>& points = spline.GetPoints();
    stream << spline.GetColor().r << ' ' << spline.GetColor().g << ' '
           << spline.GetColor().b << " setrgbcolor\nnewpath\n";
    write_eps_point(stream, points.front(), min_x, min_y, scale);
    stream << " moveto\n";
    for (std::size_t index = 1; index + 2 < points.size(); index += 3) {
        write_eps_point(stream, points[index], min_x, min_y, scale);
        stream << ' ';
        write_eps_point(stream, points[index + 1], min_x, min_y, scale);
        stream << ' ';
        write_eps_point(stream, points[index + 2], min_x, min_y, scale);
        stream << " curveto\n";
    }
    if (spline.IsClosed()) stream << "closepath\n";
    stream << "stroke\n";
    return true;
}

bool EpsIO::Import(const std::string& path,
                   std::vector<std::unique_ptr<CAlfaObject>>& objects,
                   std::string& error) const
{
    objects.clear();
    std::ifstream file(path);
    if (!file) { error = "Could not open EPS file."; return false; }
    std::ostringstream cleaned;
    std::string line;
    while (std::getline(file, line)) {
        const size_t comment = line.find('%');
        if (comment != std::string::npos) line.erase(comment);
        cleaned << line << ' ';
    }
    std::istringstream tokens(cleaned.str());
    std::vector<double> stack;
    std::vector<ImportedEpsPath> pending;
    ImportedEpsPath current;
    int index = 1;
    const auto finish_subpath = [&]() {
        if (!current.segments.empty()) pending.push_back(std::move(current));
        current = {};
    };
    const auto flush = [&]() {
        finish_subpath();
        for (const ImportedEpsPath& path_value : pending) {
            if (auto object = make_eps_path_object(
                    path_value, "EPS Path " + std::to_string(index++))) {
                objects.push_back(std::move(object));
            }
        }
        pending.clear();
    };
    const auto eps_point = [](double x, double y) {
        return Vec3{static_cast<float>(x * kEpsPointToMillimeter),
                    static_cast<float>(y * kEpsPointToMillimeter), 0.0f};
    };
    std::string token;
    while (tokens >> token) {
        char* end = nullptr;
        const double number = std::strtod(token.c_str(), &end);
        if (end && *end == '\0') { stack.push_back(number); continue; }
        const std::string op = upper(token);
        if ((op == "M" || op == "MOVETO") && stack.size() >= 2) {
            finish_subpath();
            current = {};
            current.start = eps_point(stack[stack.size() - 2], stack.back());
            current.cursor = current.start;
            current.has_start = true;
        } else if ((op == "L" || op == "LINETO") && stack.size() >= 2
                   && current.has_start) {
            const Vec3 destination = eps_point(
                stack[stack.size() - 2], stack.back());
            current.segments.push_back({
                current.cursor, {}, {}, destination, false});
            current.cursor = destination;
        } else if ((op == "C" || op == "CURVETO") && stack.size() >= 6
                   && current.has_start) {
            const Vec3 control1 = eps_point(
                stack[stack.size() - 6], stack[stack.size() - 5]);
            const Vec3 control2 = eps_point(
                stack[stack.size() - 4], stack[stack.size() - 3]);
            const Vec3 destination = eps_point(
                stack[stack.size() - 2], stack.back());
            current.segments.push_back({
                current.cursor, control1, control2, destination, true});
            current.cursor = destination;
        } else if ((op == "H" || op == "CLOSEPATH") && current.has_start) {
            if (!eps_points_touch(current.cursor, current.start)) {
                current.segments.push_back({
                    current.cursor, {}, {}, current.start, false});
                current.cursor = current.start;
            }
            current.closed = true;
        } else if (op == "S" || op == "STROKE" || op == "F" || op == "FILL") {
            flush();
        } else if (op == "N" || op == "NEWPATH") {
            flush();
        }
        stack.clear();
    }
    flush();
    if (objects.empty()) { error = "EPS contains no supported vector paths."; return false; }
    return true;
}

bool EpsIO::Export(const std::string& path, const CAlfaDoc& document,
                   std::string& error) const
{
    // The sampled paths are used for a safe bounding box.  They are not used
    // to write Beziers: doing so was the source of the visible polyline error.
    std::vector<Path3d> bounds_paths = collect_curve_paths(document);
    add_mesh_wire_paths(document, bounds_paths);
    std::vector<Path3d> paths = collect_curve_paths(document, false, false);
    add_mesh_wire_paths(document, paths);
    if (bounds_paths.empty()) {
        error = "There is no visible geometry to export.";
        return false;
    }
    float min_x = std::numeric_limits<float>::max(), min_y = min_x;
    float max_x = -min_x, max_y = -min_x;
    for (const Path3d& curve : bounds_paths) for (Vec3 point : curve.points) {
        min_x = std::min(min_x, point.x); min_y = std::min(min_y, point.y);
        max_x = std::max(max_x, point.x); max_y = std::max(max_y, point.y);
    }
    const double scale = 1.0 / kEpsPointToMillimeter;
    const int width = std::max(1, static_cast<int>(std::ceil((max_x-min_x)*scale + 20)));
    const int height = std::max(1, static_cast<int>(std::ceil((max_y-min_y)*scale + 20)));
    std::ofstream file(path);
    if (!file) { error = "Could not create EPS file."; return false; }
    file << "%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 " << width << ' ' << height
         << "\n%%Creator: Dom3D Pro\n1 setlinejoin 1 setlinecap\n";
    file << std::fixed << std::setprecision(4);
    for (const Path3d& curve : paths) {
        write_eps_polyline_path(file, curve, min_x, min_y, scale);
    }
    for (const auto& object : document.GetObjects()) {
        if (!object || !document.IsObjectVisible(*object)) continue;
        if (const auto* sketch = dynamic_cast<const CSmartLine*>(object.get())) {
            if (!write_eps_smart_line(file, *sketch, min_x, min_y, scale)) {
                Path3d fallback;
                fallback.color = sketch->GetColor();
                fallback.closed = sketch->IsClosed();
                for (const CPoint3d& point : sketch->GetProfilePointsWorld()) {
                    fallback.points.push_back(to_vec3(point));
                }
                write_eps_polyline_path(file, fallback, min_x, min_y, scale);
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object.get());
                   spline && spline->IsBezierChain()) {
            write_eps_bezier_spline(file, *spline, min_x, min_y, scale);
        }
    }
    file << "showpage\n%%EOF\n";
    return static_cast<bool>(file);
}

bool HpglIO::Import(const std::string& path,
                    std::vector<std::unique_ptr<CAlfaObject>>& objects,
                    std::string& error) const
{
    objects.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Could not open HPGL file."; return false; }
    const std::string data((std::istreambuf_iterator<char>(file)), {});
    Vec3 position{};
    bool absolute = true;
    bool pen_down = false;
    int pen = 1;
    Path3d current;
    int index = 1;
    const Color background = CSolid::GetHiddenLineBackgroundColor();
    const float background_luminance = 0.2126f * background.r
        + 0.7152f * background.g + 0.0722f * background.b;
    const Color contrasting_pen = background_luminance < 0.5f
        ? Color{0.92f, 0.92f, 0.92f}
        : Color{0.08f, 0.08f, 0.08f};
    const std::array<Color, 8> colors{{contrasting_pen,{1,0,0},{0,0.65f,0},{0,0,1},
                                       {1,0,1},{0,0.75f,0.75f},{0.85f,0.7f,0},{0.3f,0.3f,0.3f}}};
    const auto flush = [&]() {
        if (current.points.size() >= 2) {
            current.name = "HPGL Path " + std::to_string(index++);
            current.color = colors[static_cast<size_t>(std::clamp(pen-1, 0, 7))];
            if (auto polyline = make_polyline(current)) objects.push_back(std::move(polyline));
        }
        current = {};
    };
    size_t cursor = 0;
    while (cursor < data.size()) {
        const size_t semicolon = data.find(';', cursor);
        std::string command = trim(data.substr(cursor,
            semicolon == std::string::npos ? std::string::npos : semicolon-cursor));
        cursor = semicolon == std::string::npos ? data.size() : semicolon+1;
        if (command.size() < 2) continue;
        const std::string op = upper(command.substr(0,2));
        const std::vector<double> args = parse_numbers(command.substr(2));
        if (op == "IN") { flush(); position = {}; absolute = true; pen_down = false; }
        else if (op == "SP" && !args.empty()) { flush(); pen = static_cast<int>(args[0]); }
        else if (op == "PA") absolute = true;
        else if (op == "PR") absolute = false;
        if (op == "PU" || op == "PD" || op == "PA" || op == "PR") {
            const bool new_pen_down = op == "PD" ? true : op == "PU" ? false : pen_down;
            if (!new_pen_down && pen_down) flush();
            pen_down = new_pen_down;
            for (size_t i = 0; i + 1 < args.size(); i += 2) {
                Vec3 next{static_cast<float>(args[i]*kHpglUnitToMillimeter),
                          static_cast<float>(args[i+1]*kHpglUnitToMillimeter), 0.0f};
                if (!absolute) next = position + next;
                if (pen_down) {
                    if (current.points.empty()) current.points.push_back(position);
                    current.points.push_back(next);
                }
                position = next;
            }
        } else if (op == "CI" && !args.empty()) {
            Path3d circle;
            append_arc(circle.points, position, args[0]*kHpglUnitToMillimeter, 0, 2*kPiD, 36);
            circle.closed = true; circle.name = "HPGL Circle " + std::to_string(index++);
            circle.color = colors[static_cast<size_t>(std::clamp(pen-1,0,7))];
            if (auto polyline = make_polyline(circle)) objects.push_back(std::move(polyline));
        } else if (op == "AA" && args.size() >= 3) {
            const Vec3 center{static_cast<float>(args[0]*kHpglUnitToMillimeter),
                              static_cast<float>(args[1]*kHpglUnitToMillimeter), 0.0f};
            const double start_angle = std::atan2(position.y-center.y, position.x-center.x);
            const double radius = std::hypot(position.x-center.x, position.y-center.y);
            Path3d arc; arc.points.push_back(position);
            append_arc(arc.points, center, radius, start_angle,
                       start_angle + args[2]*kPiD/180.0, 12);
            position = arc.points.back(); arc.name = "HPGL Arc " + std::to_string(index++);
            arc.color = colors[static_cast<size_t>(std::clamp(pen-1,0,7))];
            if (auto polyline = make_polyline(arc)) objects.push_back(std::move(polyline));
        }
    }
    flush();
    if (objects.empty()) { error = "HPGL contains no supported pen paths."; return false; }
    return true;
}

bool HpglIO::Export(const std::string& path, const CAlfaDoc& document,
                    std::string& error) const
{
    std::vector<Path3d> paths = collect_curve_paths(document);
    add_mesh_wire_paths(document, paths);
    if (paths.empty()) { error = "There is no visible geometry to export."; return false; }
    std::ofstream file(path);
    if (!file) { error = "Could not create HPGL file."; return false; }
    file << "IN;SP1;PA;";
    for (const Path3d& curve : paths) {
        if (curve.points.size() < 2) continue;
        const auto coord = [](float value) { return std::llround(value / kHpglUnitToMillimeter); };
        file << "PU" << coord(curve.points[0].x) << ',' << coord(curve.points[0].y) << ";PD";
        for (size_t i = 1; i < curve.points.size(); ++i) {
            if (i > 1) file << ',';
            file << coord(curve.points[i].x) << ',' << coord(curve.points[i].y);
        }
        if (curve.closed) file << ',' << coord(curve.points[0].x) << ',' << coord(curve.points[0].y);
        file << ";PU;";
    }
    file << "SP0;";
    return static_cast<bool>(file);
}

bool StlIO::Import(const std::string& path,
                   std::vector<std::unique_ptr<CMesh3D>>& meshes,
                   std::string& error) const
{
    meshes.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Could not open STL file."; return false; }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0);
    std::vector<Vec3> vertices;
    std::vector<CMesh3D::Face> faces;
    bool binary = false;
    if (size >= 84) {
        std::array<char,80> header{};
        uint32_t count = 0;
        file.read(header.data(), header.size());
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        binary = file && size == static_cast<std::streamoff>(84ull + 50ull*count);
        if (binary) {
            vertices.reserve(static_cast<size_t>(count)*3);
            faces.reserve(count);
            for (uint32_t triangle = 0; triangle < count; ++triangle) {
                float values[12]{};
                uint16_t attribute = 0;
                file.read(reinterpret_cast<char*>(values), sizeof(values));
                file.read(reinterpret_cast<char*>(&attribute), sizeof(attribute));
                if (!file) { error = "Binary STL is truncated."; return false; }
                CMesh3D::Face face;
                for (int i = 0; i < 3; ++i) {
                    Vec3 point{values[3+i*3], values[4+i*3], values[5+i*3]};
                    if (!finite_vec(point)) { error = "STL contains invalid coordinates."; return false; }
                    face.corners.push_back({vertices.size(),0,0}); vertices.push_back(point);
                }
                faces.push_back(std::move(face));
            }
        }
    }
    if (!binary) {
        file.clear(); file.seekg(0);
        std::string word;
        std::vector<Vec3> triangle;
        while (file >> word) {
            if (upper(word) != "VERTEX") continue;
            Vec3 point{}; file >> point.x >> point.y >> point.z;
            if (!file || !finite_vec(point)) { error = "ASCII STL contains invalid vertex data."; return false; }
            triangle.push_back(point);
            if (triangle.size() == 3) {
                CMesh3D::Face face;
                for (Vec3 vertex : triangle) { face.corners.push_back({vertices.size(),0,0}); vertices.push_back(vertex); }
                faces.push_back(std::move(face)); triangle.clear();
            }
        }
    }
    if (faces.empty()) { error = "STL contains no triangles."; return false; }
    auto mesh = std::make_unique<CMesh3D>("Imported STL");
    if (!mesh->SetGeometry(std::move(vertices), std::move(faces))) {
        error = "STL mesh geometry is invalid."; return false;
    }
    meshes.push_back(std::move(mesh));
    return true;
}

bool StlIO::Export(const std::string& path, const CAlfaDoc& document,
                   std::string& error) const
{
    struct Triangle { Vec3 normal, a, b, c; };
    std::vector<Triangle> triangles;
    visit_document_triangles(document, [&](Vec3 a, Vec3 b, Vec3 c, Color,
                                           const std::string&) {
        const Vec3 normal = normalize(cross(b-a, c-a));
        if (finite_vec(a) && finite_vec(b) && finite_vec(c))
            triangles.push_back({normal,a,b,c});
    });
    if (triangles.empty()) { error = "There is no visible mesh or solid geometry to export."; return false; }
    if (triangles.size() > std::numeric_limits<uint32_t>::max()) {
        error = "STL contains too many triangles."; return false;
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) { error = "Could not create STL file."; return false; }
    std::array<char,80> header{};
    const char text[] = "Dom3D Pro Binary STL";
    std::memcpy(header.data(), text, sizeof(text)-1);
    file.write(header.data(), header.size());
    const uint32_t count = static_cast<uint32_t>(triangles.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const Triangle& triangle : triangles) {
        const float values[12]{triangle.normal.x,triangle.normal.y,triangle.normal.z,
            triangle.a.x,triangle.a.y,triangle.a.z, triangle.b.x,triangle.b.y,triangle.b.z,
            triangle.c.x,triangle.c.y,triangle.c.z};
        const uint16_t attribute = 0;
        file.write(reinterpret_cast<const char*>(values), sizeof(values));
        file.write(reinterpret_cast<const char*>(&attribute), sizeof(attribute));
    }
    return static_cast<bool>(file);
}
