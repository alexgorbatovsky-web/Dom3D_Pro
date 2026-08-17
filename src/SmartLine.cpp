#include "SmartLine.h"

#include "BezierSpline.h"
#include "ConstraintBezierTangent.h"
#include "CPolyline.h"
#include "ConstraintHorLine.h"
#include "ConstraintVertLine.h"
#include "LinkLineHor.h"
#include "LinkLineVert.h"
#include "OpenGLCompat.h"
#include "SketchArcLine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <utility>

namespace {
constexpr double kSketchEpsilon = 1.0e-8;
constexpr double kAxisTolerance = 1.0e-6;

CPoint3d subtract(const CPoint3d& first, const CPoint3d& second) {
    return CPoint3d(first.x - second.x, first.y - second.y, first.z - second.z);
}

CPoint3d add(const CPoint3d& first, const CPoint3d& second) {
    return CPoint3d(first.x + second.x, first.y + second.y, first.z + second.z);
}

CPoint3d multiply(const CPoint3d& point, double value) {
    return CPoint3d(point.x * value, point.y * value, point.z * value);
}

double dot_product(const CPoint3d& first, const CPoint3d& second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

CPoint3d cross_product(const CPoint3d& first, const CPoint3d& second) {
    return CPoint3d(
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x);
}

double vector_length(const CPoint3d& point) {
    return std::sqrt(dot_product(point, point));
}

bool normalize_point(CPoint3d& point) {
    const double length = vector_length(point);
    if (length <= kSketchEpsilon) {
        return false;
    }
    point.x /= length;
    point.y /= length;
    point.z /= length;
    return true;
}

CPoint3d rotate_point(CPoint3d point, CPoint3d axis, double angle) {
    if (!normalize_point(axis)) {
        return point;
    }
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    return add(
        add(multiply(point, cosine), multiply(cross_product(axis, point), sine)),
        multiply(axis, dot_product(axis, point) * (1.0 - cosine)));
}

double distance_2d(CurvePoint point, const CPoint3d& start, const CPoint3d& end) {
    const double dx = end.x - start.x;
    const double dz = end.z - start.z;
    const double length_squared = dx * dx + dz * dz;
    if (length_squared <= kSketchEpsilon) {
        const double px = point.x - start.x;
        const double pz = point.z - start.z;
        return std::sqrt(px * px + pz * pz);
    }
    const double parameter = std::clamp(
        ((point.x - start.x) * dx + (point.z - start.z) * dz) / length_squared,
        0.0,
        1.0);
    const double px = point.x - (start.x + dx * parameter);
    const double pz = point.z - (start.z + dz * parameter);
    return std::sqrt(px * px + pz * pz);
}

float distance_to_screen_segment(DomPoint point, DomPoint start, DomPoint end) {
    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    const float length_squared = dx * dx + dy * dy;
    if (length_squared <= 0.000001f) {
        const float px = static_cast<float>(point.x - start.x);
        const float py = static_cast<float>(point.y - start.y);
        return std::sqrt(px * px + py * py);
    }
    const float parameter = std::clamp(
        (static_cast<float>(point.x - start.x) * dx
         + static_cast<float>(point.y - start.y) * dy) / length_squared,
        0.0f,
        1.0f);
    const float px = static_cast<float>(point.x - start.x) - dx * parameter;
    const float py = static_cast<float>(point.y - start.y) - dy * parameter;
    return std::sqrt(px * px + py * py);
}

const char* line_type_name(LinkLineType type) {
    switch (type) {
    case LinkLineType::Horizontal:
        return "HORIZONTAL";
    case LinkLineType::Vertical:
        return "VERTICAL";
    case LinkLineType::Bezier:
        return "BEZIER";
    case LinkLineType::Arc:
        return "ARC";
    default:
        return "SEGMENT";
    }
}
}

bool CSmartLine::AddBezierWorld(CPoint3d start,
                                CPoint3d control1,
                                CPoint3d control2,
                                CPoint3d end,
                                bool connect_to_previous) {
    CPoint3d local_start = WorldToLocal(start);
    CPoint3d local_control1 = WorldToLocal(control1);
    CPoint3d local_control2 = WorldToLocal(control2);
    CPoint3d local_end = WorldToLocal(end);
    local_start.z = 0.0;
    local_control1.z = 0.0;
    local_control2.z = 0.0;
    local_end.z = 0.0;
    return AddLine(
        std::make_unique<CBezierSpline>(
            local_start, local_control1, local_control2, local_end),
        connect_to_previous);
}

CSmartLine::CSmartLine()
    : CAlfaObject("Sketch") {
    SetColor(kDefaultCurveColor);
}

CSmartLine::CSmartLine(std::string name)
    : CAlfaObject(std::move(name)) {
    SetColor(kDefaultCurveColor);
}

CSmartLine::~CSmartLine() = default;

bool CSmartLine::Create(const CPolyline& polyline) {
    const std::vector<CPoint3d>& points = polyline.GetPoints();
    if (points.size() < 2) {
        return false;
    }

    lines_.clear();
    constraints_.clear();
    fillets_.clear();
    closed_ = false;

    CPoint3d coordinate_origin = points.front();
    CPoint3d x_axis;
    CPoint3d normal;
    const auto constant_coordinate = [&](char axis) {
        const double first = axis == 'x' ? points.front().x
            : axis == 'y' ? points.front().y : points.front().z;
        return std::all_of(points.begin(), points.end(), [&](const CPoint3d& point) {
            const double value = axis == 'x' ? point.x
                : axis == 'y' ? point.y : point.z;
            return std::abs(value - first) <= kAxisTolerance;
        });
    };
    if (constant_coordinate('z')) {
        // Catalog and DXF profiles drawn in the global XY plane use X for
        // cutter width and Y for cutter depth. Keep those authored axes and
        // their global origin; deriving X from the first (often vertical)
        // edge swaps width and depth and defeats the 180-degree placement.
        coordinate_origin = CPoint3d(0.0, 0.0, points.front().z);
        x_axis = CPoint3d(1.0, 0.0, 0.0);
        normal = CPoint3d(0.0, 0.0, 1.0);
    } else if (constant_coordinate('y')) {
        coordinate_origin = CPoint3d(0.0, points.front().y, 0.0);
        x_axis = CPoint3d(1.0, 0.0, 0.0);
        normal = CPoint3d(0.0, -1.0, 0.0);
    } else if (constant_coordinate('x')) {
        coordinate_origin = CPoint3d(points.front().x, 0.0, 0.0);
        x_axis = CPoint3d(0.0, 1.0, 0.0);
        normal = CPoint3d(1.0, 0.0, 0.0);
    } else {
        x_axis = subtract(points[1], points[0]);
        if (!normalize_point(x_axis)) {
            return false;
        }
        normal = CPoint3d(0.0, -1.0, 0.0);
        if (points.size() >= 3) {
            for (std::size_t index = 2; index < points.size(); ++index) {
                CPoint3d candidate = cross_product(
                    x_axis, subtract(points[index], points[0]));
                if (normalize_point(candidate)) {
                    normal = candidate;
                    break;
                }
            }
        }
        if (std::abs(dot_product(x_axis, normal)) > 1.0 - kAxisTolerance) {
            const CPoint3d reference = std::abs(x_axis.x) < 0.9
                ? CPoint3d(1.0, 0.0, 0.0)
                : CPoint3d(0.0, 0.0, 1.0);
            normal = cross_product(x_axis, reference);
            if (!normalize_point(normal)) {
                return false;
            }
        }
    }
    if (!SetCoordinateSystem(coordinate_origin, x_axis, normal)) {
        return false;
    }

    std::vector<CPoint3d> local_points;
    local_points.reserve(points.size());
    for (const CPoint3d& point : points) {
        CPoint3d local = WorldToLocal(point);
        local.z = 0.0;
        local_points.push_back(local);
    }

    const bool repeated_end =
        local_points.front().DistTo(&local_points.back()) <= kAxisTolerance;
    const std::size_t point_count = repeated_end ? local_points.size() - 1 : local_points.size();
    if (point_count < 2) {
        return false;
    }

    for (std::size_t index = 1; index < point_count; ++index) {
        const CPoint3d& start = local_points[index - 1];
        const CPoint3d& end = local_points[index];
        std::unique_ptr<CLinkLine> line;
        if (std::abs(start.y - end.y) <= kAxisTolerance) {
            line = std::make_unique<CLinkLineHor>(start, end);
        } else if (std::abs(start.x - end.x) <= kAxisTolerance) {
            line = std::make_unique<CLinkLineVert>(start, end);
        } else {
            line = std::make_unique<CLinkLine>(start, end);
        }
        if (!AddLine(std::move(line), true)) {
            return false;
        }
    }

    if (polyline.IsClosed() || repeated_end) {
        std::unique_ptr<CLinkLine> closing_line =
            std::make_unique<CLinkLine>(local_points[point_count - 1], local_points[0]);
        if (std::abs(local_points[point_count - 1].y - local_points[0].y) <= kAxisTolerance) {
            closing_line = std::make_unique<CLinkLineHor>(
                local_points[point_count - 1], local_points[0]);
        } else if (std::abs(local_points[point_count - 1].x - local_points[0].x) <= kAxisTolerance) {
            closing_line = std::make_unique<CLinkLineVert>(
                local_points[point_count - 1], local_points[0]);
        }
        if (!AddLine(std::move(closing_line), true)) {
            return false;
        }
        closed_ = true;
    }

    // Preserve rounded polyline vertices when a catalog/DXF curve is
    // promoted to a parametric sketch. A repeated final point is only a
    // closure marker, so its radius is intentionally ignored.
    for (std::size_t point_index = 0; point_index < point_count; ++point_index) {
        const double radius = polyline.GetVertexRadius(point_index);
        if (radius <= kSketchEpsilon) {
            continue;
        }
        if (!closed_ && (point_index == 0 || point_index + 1 == point_count)) {
            continue;
        }
        const std::size_t first_line_index = point_index == 0
            ? lines_.size() - 1
            : point_index - 1;
        if (!AddFillet(first_line_index, radius)) {
            return false;
        }
    }
    return true;
}

bool CSmartLine::CreateFromWorldPoints(const std::vector<CPoint3d>& points,
                                       bool closed,
                                       CPoint3d origin,
                                       CPoint3d x_axis,
                                       CPoint3d y_axis) {
    if (points.size() < 2 || (closed && points.size() < 3)) {
        return false;
    }

    CPoint3d normal = cross_product(x_axis, y_axis);
    if (!normalize_point(normal) || !SetCoordinateSystem(origin, x_axis, normal)) {
        return false;
    }

    lines_.clear();
    constraints_.clear();
    fillets_.clear();
    closed_ = false;

    std::vector<CPoint3d> local_points;
    local_points.reserve(points.size());
    for (const CPoint3d& point : points) {
        CPoint3d local = WorldToLocal(point);
        local.z = 0.0;
        local_points.push_back(local);
    }

    auto make_line = [](const CPoint3d& start, const CPoint3d& end) -> std::unique_ptr<CLinkLine> {
        if (std::abs(start.y - end.y) <= kAxisTolerance) {
            return std::make_unique<CLinkLineHor>(start, end);
        }
        if (std::abs(start.x - end.x) <= kAxisTolerance) {
            return std::make_unique<CLinkLineVert>(start, end);
        }
        return std::make_unique<CLinkLine>(start, end);
    };

    for (std::size_t index = 1; index < local_points.size(); ++index) {
        if (!AddLine(make_line(local_points[index - 1], local_points[index]), true)) {
            lines_.clear();
            return false;
        }
    }
    if (closed) {
        if (!AddLine(make_line(local_points.back(), local_points.front()), true)
            || !SetClosed(true)) {
            lines_.clear();
            return false;
        }
    }
    return true;
}

bool CSmartLine::AddLine(std::unique_ptr<CLinkLine> line, bool connect_to_previous) {
    if (!line || line->GetLength() <= kSketchEpsilon) {
        return false;
    }
    if (connect_to_previous && !lines_.empty()) {
        line->SetStart(lines_.back()->GetEnd());
    }
    if (line->GetLength() <= kSketchEpsilon) {
        return false;
    }
    line->SetID(lines_.size());
    lines_.push_back(std::move(line));
    if (closed_ && lines_.size() > 1) {
        lines_.back()->SetEnd(lines_.front()->GetStart());
    }
    return true;
}

bool CSmartLine::Add(CLinkLine* line, BOOL assign_id) {
    std::unique_ptr<CLinkLine> owned(line);
    if (!owned) {
        return false;
    }
    const bool result = AddLine(std::move(owned), true);
    if (result && !assign_id) {
        lines_.back()->SetID(lines_.size() - 1);
    }
    return result;
}

bool CSmartLine::RemoveLine(std::size_t index) {
    if (index >= lines_.size()) {
        return false;
    }
    constraints_.erase(
        std::remove_if(
            constraints_.begin(),
            constraints_.end(),
            [index](const std::unique_ptr<CConstraint>& constraint) {
                return constraint->GetLineIndex() >= index;
            }),
        constraints_.end());
    fillets_.erase(
        std::remove_if(
            fillets_.begin(),
            fillets_.end(),
            [index](const CFillet& fillet) {
                return fillet.GetFirstLineIndex() >= index ||
                       fillet.GetSecondLineIndex() >= index;
            }),
        fillets_.end());
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(index));
    closed_ = false;
    RenumberLines();
    RemoveInvalidConstraintsAndFillets();
    if (index > 0 && index < lines_.size()) {
        lines_[index]->SetStart(lines_[index - 1]->GetEnd());
    }
    return true;
}

bool CSmartLine::SplitLine(std::size_t index, CPoint3d local_point) {
    if (index >= lines_.size()) {
        return false;
    }
    const LinkLineType type = lines_[index]->GetType();
    if (type != LinkLineType::Segment
        && type != LinkLineType::Horizontal
        && type != LinkLineType::Vertical) {
        return false;
    }
    local_point.z = 0.0;
    const CPoint3d start = lines_[index]->GetStart();
    const CPoint3d end = lines_[index]->GetEnd();
    if (vector_length(subtract(local_point, start)) <= kSketchEpsilon
        || vector_length(subtract(end, local_point)) <= kSketchEpsilon) {
        return false;
    }

    // A newly inserted node must be free to move away from the original
    // horizontal/vertical axis.  Therefore both resulting pieces are plain
    // LinkLine segments.  Explicit sketch constraints, when present, are
    // still copied to both pieces by ReplaceLineWithSplitParts().
    const auto make_part = [](CPoint3d first, CPoint3d second) {
        return std::unique_ptr<CLinkLine>(
            std::make_unique<CLinkLine>(first, second));
    };
    return ReplaceLineWithSplitParts(
        index,
        make_part(start, local_point),
        make_part(local_point, end));
}

bool CSmartLine::SplitBezierLine(std::size_t index, double parameter) {
    if (index >= lines_.size()) {
        return false;
    }
    const auto* bezier = dynamic_cast<const CBezierSpline*>(lines_[index].get());
    const double t = std::clamp(parameter, 0.0, 1.0);
    if (!bezier || t <= 1.0e-4 || t >= 1.0 - 1.0e-4) {
        return false;
    }
    const auto lerp = [t](const CPoint3d& first, const CPoint3d& second) {
        return add(multiply(first, 1.0 - t), multiply(second, t));
    };
    const CPoint3d p0 = bezier->GetStart();
    const CPoint3d p1 = bezier->GetControl1();
    const CPoint3d p2 = bezier->GetControl2();
    const CPoint3d p3 = bezier->GetEnd();
    const CPoint3d p01 = lerp(p0, p1);
    const CPoint3d p12 = lerp(p1, p2);
    const CPoint3d p23 = lerp(p2, p3);
    const CPoint3d p012 = lerp(p01, p12);
    const CPoint3d p123 = lerp(p12, p23);
    const CPoint3d split = lerp(p012, p123);
    return ReplaceLineWithSplitParts(
        index,
        std::make_unique<CBezierSpline>(p0, p01, p012, split),
        std::make_unique<CBezierSpline>(split, p123, p23, p3));
}

bool CSmartLine::ReplaceLineWithSplitParts(
    std::size_t index,
    std::unique_ptr<CLinkLine> first_part,
    std::unique_ptr<CLinkLine> second_part) {
    if (index >= lines_.size() || !first_part || !second_part) {
        return false;
    }
    struct ConstraintRecord {
        std::size_t line = 0;
        ConstraintType type = ConstraintType::Horizontal;
    };
    std::vector<ConstraintRecord> constraint_records;
    constraint_records.reserve(constraints_.size());
    for (const auto& constraint : constraints_) {
        constraint_records.push_back(
            {constraint->GetLineIndex(), constraint->GetType()});
    }
    struct FilletRecord {
        std::size_t first = 0;
        std::size_t second = 0;
        double radius = 0.0;
    };
    std::vector<FilletRecord> fillet_records;
    fillet_records.reserve(fillets_.size());
    for (const CFillet& fillet : fillets_) {
        fillet_records.push_back({
            fillet.GetFirstLineIndex(),
            fillet.GetSecondLineIndex(),
            fillet.GetRadius()});
    }

    lines_[index] = std::move(first_part);
    lines_.insert(
        lines_.begin() + static_cast<std::ptrdiff_t>(index + 1),
        std::move(second_part));
    RenumberLines();

    constraints_.clear();
    for (const ConstraintRecord& record : constraint_records) {
        const std::size_t shifted = record.line > index
            ? record.line + 1 : record.line;
        if (record.type == ConstraintType::Horizontal) {
            ConstrainHorizontal(shifted);
            if (record.line == index) {
                ConstrainHorizontal(index + 1);
            }
        } else if (record.type == ConstraintType::Vertical) {
            ConstrainVertical(shifted);
            if (record.line == index) {
                ConstrainVertical(index + 1);
            }
        } else if (record.type == ConstraintType::TangentAtStart) {
            ConstrainBezierTangentAtStart(shifted);
        } else if (record.type == ConstraintType::TangentAtEnd) {
            const std::size_t target = record.line == index
                ? index + 1 : shifted;
            ConstrainBezierTangentAtEnd(target);
        }
    }

    fillets_.clear();
    for (const FilletRecord& record : fillet_records) {
        std::size_t first = record.first;
        std::size_t second = record.second;
        if (first >= index) {
            ++first;
        }
        if (second > index) {
            ++second;
        }
        fillets_.emplace_back(first, second, record.radius);
    }
    RemoveInvalidConstraintsAndFillets();
    return true;
}

bool CSmartLine::ConvertLineToBezier(std::size_t index) {
    if (index >= lines_.size()
        || (lines_[index]->GetType() != LinkLineType::Segment
            && lines_[index]->GetType() != LinkLineType::Horizontal
            && lines_[index]->GetType() != LinkLineType::Vertical)) {
        return false;
    }
    const CPoint3d start = lines_[index]->GetStart();
    const CPoint3d end = lines_[index]->GetEnd();
    const CPoint3d delta = subtract(end, start);
    auto bezier = std::make_unique<CBezierSpline>(
        start,
        add(start, multiply(delta, 1.0 / 3.0)),
        add(start, multiply(delta, 2.0 / 3.0)),
        end);
    bezier->SetID(index);
    lines_[index] = std::move(bezier);
    constraints_.erase(
        std::remove_if(
            constraints_.begin(),
            constraints_.end(),
            [index](const std::unique_ptr<CConstraint>& constraint) {
                return constraint->GetLineIndex() == index;
            }),
        constraints_.end());
    RemoveInvalidConstraintsAndFillets();
    return true;
}

bool CSmartLine::ConvertLineToArc(std::size_t index, CPoint3d world_point) {
    if (index >= lines_.size()
        || (lines_[index]->GetType() != LinkLineType::Segment
            && lines_[index]->GetType() != LinkLineType::Horizontal
            && lines_[index]->GetType() != LinkLineType::Vertical)) {
        return false;
    }
    CPoint3d point_on_arc = WorldToLocal(world_point);
    point_on_arc.z = 0.0;
    auto arc = std::make_unique<CSketchArcLine>(
        lines_[index]->GetStart(), point_on_arc, lines_[index]->GetEnd());
    if (!arc->IsValid()) {
        return false;
    }
    arc->SetID(index);
    lines_[index] = std::move(arc);
    constraints_.erase(
        std::remove_if(
            constraints_.begin(),
            constraints_.end(),
            [index](const std::unique_ptr<CConstraint>& constraint) {
                return constraint->GetLineIndex() == index;
            }),
        constraints_.end());
    fillets_.erase(
        std::remove_if(
            fillets_.begin(),
            fillets_.end(),
            [index](const CFillet& fillet) {
                return fillet.GetFirstLineIndex() == index
                    || fillet.GetSecondLineIndex() == index;
            }),
        fillets_.end());
    RemoveInvalidConstraintsAndFillets();
    return true;
}

std::size_t CSmartLine::GetNumLines() const {
    return lines_.size();
}

CLinkLine* CSmartLine::GetLine(std::size_t index) {
    return index < lines_.size() ? lines_[index].get() : nullptr;
}

const CLinkLine* CSmartLine::GetLine(std::size_t index) const {
    return index < lines_.size() ? lines_[index].get() : nullptr;
}

CLinkLine* CSmartLine::Get_Link_by_index(int index, bool) {
    return index >= 0 ? GetLine(static_cast<std::size_t>(index)) : nullptr;
}

CLinkLine* CSmartLine::GetLastLine() {
    return lines_.empty() ? nullptr : lines_.back().get();
}

bool CSmartLine::MovePoint(std::size_t line_index, int endpoint, CPoint3d local_point) {
    CLinkLine* line = GetLine(line_index);
    if (!line || (endpoint != 0 && endpoint != 1)) {
        return false;
    }
    local_point.z = 0.0;
    if (endpoint == 0) {
        line->SetStart(local_point);
        if (line_index > 0) {
            lines_[line_index - 1]->SetEnd(line->GetStart());
        } else if (closed_ && lines_.size() > 1) {
            lines_.back()->SetEnd(line->GetStart());
        }
    } else {
        line->SetEnd(local_point);
        ConnectAdjacentLines(line_index);
    }
    return ApplyConstraints();
}

std::size_t CSmartLine::GetNodeCount() const {
    if (lines_.empty()) {
        return 0;
    }
    return closed_ ? lines_.size() : lines_.size() + 1;
}

CPoint3d CSmartLine::GetNodeWorld(std::size_t node_index) const {
    if (node_index >= GetNodeCount()) {
        return {};
    }
    if (!closed_ && node_index == 0) {
        return LocalToWorld(lines_.front()->GetStart());
    }
    const std::size_t line_index = closed_ ? node_index : node_index - 1;
    return LocalToWorld(lines_[line_index]->GetEnd());
}

bool CSmartLine::MoveNodeWorld(std::size_t node_index, CPoint3d world_point) {
    if (node_index >= GetNodeCount()) {
        return false;
    }

    std::vector<CPoint3d> old_starts;
    std::vector<CPoint3d> old_ends;
    old_starts.reserve(lines_.size());
    old_ends.reserve(lines_.size());
    for (const auto& line : lines_) {
        old_starts.push_back(line->GetStart());
        old_ends.push_back(line->GetEnd());
    }

    std::vector<CPoint3d> nodes(GetNodeCount());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        if (!closed_ && index == 0) {
            nodes[index] = lines_.front()->GetStart();
        } else {
            const std::size_t line_index = closed_ ? index : index - 1;
            nodes[index] = lines_[line_index]->GetEnd();
        }
    }

    CPoint3d local_point = WorldToLocal(world_point);
    local_point.z = 0.0;
    nodes[node_index] = local_point;

    std::vector<bool> propagated_x(nodes.size(), false);
    std::vector<bool> propagated_y(nodes.size(), false);
    propagated_x[node_index] = true;
    propagated_y[node_index] = true;
    for (std::size_t pass = 0; pass < nodes.size(); ++pass) {
        bool changed = false;
        for (std::size_t line_index = 0; line_index < lines_.size(); ++line_index) {
            const std::size_t start_node = closed_
                ? (line_index == 0 ? nodes.size() - 1 : line_index - 1)
                : line_index;
            const std::size_t end_node = closed_ ? line_index : line_index + 1;

            LinkLineType type = lines_[line_index]->GetType();
            for (const auto& constraint : constraints_) {
                if (constraint->GetLineIndex() == line_index) {
                    if (constraint->GetType() == ConstraintType::Horizontal) {
                        type = LinkLineType::Horizontal;
                    } else if (constraint->GetType() == ConstraintType::Vertical) {
                        type = LinkLineType::Vertical;
                    }
                }
            }

            if (type == LinkLineType::Horizontal) {
                if (propagated_y[start_node] && !propagated_y[end_node]) {
                    nodes[end_node].y = nodes[start_node].y;
                    propagated_y[end_node] = true;
                    changed = true;
                } else if (propagated_y[end_node] && !propagated_y[start_node]) {
                    nodes[start_node].y = nodes[end_node].y;
                    propagated_y[start_node] = true;
                    changed = true;
                }
            } else if (type == LinkLineType::Vertical) {
                if (propagated_x[start_node] && !propagated_x[end_node]) {
                    nodes[end_node].x = nodes[start_node].x;
                    propagated_x[end_node] = true;
                    changed = true;
                } else if (propagated_x[end_node] && !propagated_x[start_node]) {
                    nodes[start_node].x = nodes[end_node].x;
                    propagated_x[start_node] = true;
                    changed = true;
                }
            }
        }
        if (!changed) {
            break;
        }
    }

    for (std::size_t line_index = 0; line_index < lines_.size(); ++line_index) {
        const std::size_t start_node = closed_
            ? (line_index == 0 ? nodes.size() - 1 : line_index - 1)
            : line_index;
        const std::size_t end_node = closed_ ? line_index : line_index + 1;
        lines_[line_index]->SetStart(nodes[start_node]);
        lines_[line_index]->SetEnd(nodes[end_node]);
    }

    bool valid = std::all_of(
        lines_.begin(),
        lines_.end(),
        [](const std::unique_ptr<CLinkLine>& line) {
            return line->GetLength() > kSketchEpsilon;
        });
    for (const CFillet& fillet : fillets_) {
        const CLinkLine* first = GetLine(fillet.GetFirstLineIndex());
        const CLinkLine* second = GetLine(fillet.GetSecondLineIndex());
        valid = valid && first && second && fillet.Calculate(*first, *second).valid;
    }
    if (valid && ApplyConstraints()) {
        return true;
    }

    for (std::size_t index = 0; index < lines_.size(); ++index) {
        lines_[index]->SetStart(old_starts[index]);
        lines_[index]->SetEnd(old_ends[index]);
    }
    return false;
}

std::size_t CSmartLine::GetBezierControlPointCount() const {
    return static_cast<std::size_t>(std::count_if(
        lines_.begin(), lines_.end(), [](const std::unique_ptr<CLinkLine>& line) {
            return line->GetType() == LinkLineType::Bezier;
        })) * 2;
}

CPoint3d CSmartLine::GetBezierControlPointWorld(std::size_t control_index) const {
    for (const auto& line : lines_) {
        const auto* bezier = dynamic_cast<const CBezierSpline*>(line.get());
        if (!bezier) {
            continue;
        }
        if (control_index < 2) {
            return LocalToWorld(
                control_index == 0 ? bezier->GetControl1() : bezier->GetControl2());
        }
        control_index -= 2;
    }
    return {};
}

bool CSmartLine::MoveBezierControlPointWorld(std::size_t control_index,
                                             CPoint3d world_point) {
    CPoint3d local = WorldToLocal(world_point);
    local.z = 0.0;
    for (std::size_t line_index = 0; line_index < lines_.size(); ++line_index) {
        auto* bezier = dynamic_cast<CBezierSpline*>(lines_[line_index].get());
        if (!bezier) {
            continue;
        }
        if (control_index < 2) {
            if (control_index == 0) {
                bezier->SetControl1(local);
            } else {
                bezier->SetControl2(local);
            }

            // At a Bezier-Bezier smooth joint either handle may be the
            // driving handle.  When the constrained handle itself is moved,
            // rotate the opposite handle while preserving its length.  The
            // normal ApplyConstraints pass below then confirms exact
            // collinearity.  A straight neighbour remains fixed, so its
            // Bezier handle is intentionally restricted to that tangent.
            for (const auto& constraint : constraints_) {
                if (constraint->GetLineIndex() != line_index) {
                    continue;
                }
                if (control_index == 0
                    && constraint->GetType() == ConstraintType::TangentAtStart) {
                    const std::size_t previous_index = line_index > 0
                        ? line_index - 1
                        : closed_ ? lines_.size() - 1 : lines_.size();
                    auto* previous = previous_index < lines_.size()
                        ? dynamic_cast<CBezierSpline*>(lines_[previous_index].get())
                        : nullptr;
                    if (previous) {
                        const CPoint3d joint = bezier->GetStart();
                        CPoint3d direction = subtract(local, joint);
                        const double opposite_length = vector_length(
                            subtract(joint, previous->GetControl2()));
                        if (normalize_point(direction)
                            && opposite_length > kSketchEpsilon) {
                            previous->SetControl2(add(
                                joint, multiply(direction, -opposite_length)));
                        }
                    }
                } else if (control_index == 1
                           && constraint->GetType()
                               == ConstraintType::TangentAtEnd) {
                    const std::size_t next_index = line_index + 1 < lines_.size()
                        ? line_index + 1
                        : closed_ ? 0 : lines_.size();
                    auto* next = next_index < lines_.size()
                        ? dynamic_cast<CBezierSpline*>(lines_[next_index].get())
                        : nullptr;
                    if (next) {
                        const CPoint3d joint = bezier->GetEnd();
                        CPoint3d direction = subtract(joint, local);
                        const double opposite_length = vector_length(
                            subtract(next->GetControl1(), joint));
                        if (normalize_point(direction)
                            && opposite_length > kSketchEpsilon) {
                            next->SetControl1(add(
                                joint, multiply(direction, opposite_length)));
                        }
                    }
                }
            }
            return ApplyConstraints();
        }
        control_index -= 2;
    }
    return false;
}

std::size_t CSmartLine::GetArcGripCount() const {
    return static_cast<std::size_t>(std::count_if(
        lines_.begin(), lines_.end(), [](const std::unique_ptr<CLinkLine>& line) {
            return line->GetType() == LinkLineType::Arc;
        }));
}

CPoint3d CSmartLine::GetArcGripWorld(std::size_t grip_index) const {
    for (const auto& line : lines_) {
        const auto* arc = dynamic_cast<const CSketchArcLine*>(line.get());
        if (!arc) {
            continue;
        }
        if (grip_index == 0) {
            return LocalToWorld(arc->GetPoint(0.5));
        }
        --grip_index;
    }
    return {};
}

bool CSmartLine::MoveArcGripWorld(std::size_t grip_index, CPoint3d world_point) {
    CPoint3d local = WorldToLocal(world_point);
    local.z = 0.0;
    for (auto& line : lines_) {
        auto* arc = dynamic_cast<CSketchArcLine*>(line.get());
        if (!arc) {
            continue;
        }
        if (grip_index == 0) {
            CSketchArcLine candidate(arc->GetStart(), local, arc->GetEnd());
            if (!candidate.IsValid()) {
                return false;
            }
            arc->SetPointOnArc(local);
            return true;
        }
        --grip_index;
    }
    return false;
}

void CSmartLine::ConnectAdjacentLines(std::size_t changed_line_index) {
    if (changed_line_index >= lines_.size()) {
        return;
    }
    if (changed_line_index + 1 < lines_.size()) {
        lines_[changed_line_index + 1]->SetStart(lines_[changed_line_index]->GetEnd());
    } else if (closed_ && lines_.size() > 1) {
        lines_.front()->SetStart(lines_.back()->GetEnd());
    }
}

bool CSmartLine::IsClosed() const {
    return closed_ && lines_.size() >= 2;
}

bool CSmartLine::SetClosed(bool closed) {
    if (closed && lines_.size() < 2) {
        return false;
    }
    closed_ = closed;
    if (closed_) {
        lines_.back()->SetEnd(lines_.front()->GetStart());
    }
    return true;
}

bool CSmartLine::AddConstraint(std::unique_ptr<CConstraint> constraint) {
    if (!constraint || constraint->GetLineIndex() >= lines_.size()) {
        return false;
    }
    for (const auto& existing : constraints_) {
        if (existing->GetLineIndex() == constraint->GetLineIndex() &&
            existing->GetType() == constraint->GetType()) {
            return true;
        }
    }
    if (!constraint->Apply(*this)) {
        return false;
    }
    constraints_.push_back(std::move(constraint));
    return true;
}

bool CSmartLine::ConstrainHorizontal(std::size_t line_index) {
    return AddConstraint(std::make_unique<CConstraintHorLine>(line_index));
}

bool CSmartLine::ConstrainVertical(std::size_t line_index) {
    return AddConstraint(std::make_unique<CConstraintVertLine>(line_index));
}

bool CSmartLine::ConstrainBezierTangentAtStart(std::size_t line_index) {
    return AddConstraint(
        std::make_unique<CConstraintBezierTangent>(line_index, true));
}

bool CSmartLine::ConstrainBezierTangentAtEnd(std::size_t line_index) {
    return AddConstraint(
        std::make_unique<CConstraintBezierTangent>(line_index, false));
}

bool CSmartLine::ApplyConstraints() {
    bool success = true;
    for (const auto& constraint : constraints_) {
        success = constraint->Apply(*this) && success;
    }
    return success;
}

std::size_t CSmartLine::GetNumConstraints() const {
    return constraints_.size();
}

const CConstraint* CSmartLine::GetConstraint(std::size_t index) const {
    return index < constraints_.size() ? constraints_[index].get() : nullptr;
}

bool CSmartLine::AddFillet(std::size_t first_line_index, double radius) {
    if (first_line_index >= lines_.size() || radius <= kSketchEpsilon) {
        return false;
    }
    const std::size_t second_line_index =
        first_line_index + 1 < lines_.size() ? first_line_index + 1 : closed_ ? 0 : lines_.size();
    if (second_line_index >= lines_.size()) {
        return false;
    }
    CFillet candidate(first_line_index, second_line_index, radius);
    if (!candidate.Calculate(*lines_[first_line_index], *lines_[second_line_index]).valid) {
        return false;
    }
    for (CFillet& fillet : fillets_) {
        if (fillet.GetFirstLineIndex() == first_line_index) {
            return fillet.SetRadius(radius);
        }
    }
    fillets_.push_back(candidate);
    return true;
}

bool CSmartLine::RemoveFillet(std::size_t first_line_index) {
    const auto iterator = std::find_if(
        fillets_.begin(),
        fillets_.end(),
        [first_line_index](const CFillet& fillet) {
            return fillet.GetFirstLineIndex() == first_line_index;
        });
    if (iterator == fillets_.end()) {
        return false;
    }
    fillets_.erase(iterator);
    return true;
}

std::size_t CSmartLine::GetNumFillets() const {
    return fillets_.size();
}

CFillet* CSmartLine::GetFillet(std::size_t index) {
    return index < fillets_.size() ? &fillets_[index] : nullptr;
}

const CFillet* CSmartLine::GetFillet(std::size_t index) const {
    return index < fillets_.size() ? &fillets_[index] : nullptr;
}

CPoint3d CSmartLine::GetFilletGripWorld(std::size_t fillet_index) const {
    const CFillet* fillet = GetFillet(fillet_index);
    if (!fillet) {
        return {};
    }
    const CLinkLine* first = GetLine(fillet->GetFirstLineIndex());
    const CLinkLine* second = GetLine(fillet->GetSecondLineIndex());
    if (!first || !second) {
        return {};
    }
    const FilletGeometry geometry = fillet->Calculate(*first, *second);
    return geometry.valid ? LocalToWorld(geometry.center) : CPoint3d{};
}

bool CSmartLine::SetFilletRadiusFromWorld(std::size_t fillet_index, CPoint3d world_point) {
    CFillet* fillet = GetFillet(fillet_index);
    if (!fillet) {
        return false;
    }
    const CLinkLine* first = GetLine(fillet->GetFirstLineIndex());
    const CLinkLine* second = GetLine(fillet->GetSecondLineIndex());
    if (!first || !second) {
        return false;
    }

    const CPoint3d corner = first->GetEnd();
    CPoint3d first_vector = multiply(first->GetTangent(1.0), -1.0);
    CPoint3d second_vector = second->GetTangent(0.0);
    const double first_length = std::hypot(first_vector.x, first_vector.y);
    const double second_length = std::hypot(second_vector.x, second_vector.y);
    if (first_length <= kSketchEpsilon || second_length <= kSketchEpsilon) {
        return false;
    }

    const double cosine = std::clamp(
        (first_vector.x * second_vector.x + first_vector.y * second_vector.y) /
            (first_length * second_length),
        -1.0,
        1.0);
    const double sine = std::sin(std::acos(cosine) * 0.5);
    if (sine <= kSketchEpsilon) {
        return false;
    }

    const CPoint3d local_point = WorldToLocal(world_point);
    const double center_distance = std::hypot(local_point.x - corner.x, local_point.y - corner.y);
    const double radius = center_distance * sine;
    CFillet candidate(fillet->GetFirstLineIndex(), fillet->GetSecondLineIndex(), radius);
    if (!candidate.Calculate(*first, *second).valid) {
        return false;
    }
    return fillet->SetRadius(radius);
}

const SketchCoordinateSystem& CSmartLine::GetCoordinateSystem() const {
    return coordinate_system_;
}

bool CSmartLine::SetCoordinateSystem(CPoint3d origin, CPoint3d x_axis, CPoint3d normal) {
    if (!normalize_point(x_axis) || !normalize_point(normal)) {
        return false;
    }
    x_axis = subtract(x_axis, multiply(normal, dot_product(x_axis, normal)));
    if (!normalize_point(x_axis)) {
        return false;
    }
    CPoint3d y_axis = cross_product(normal, x_axis);
    if (!normalize_point(y_axis)) {
        return false;
    }
    coordinate_system_.origin = origin;
    coordinate_system_.x_axis = x_axis;
    coordinate_system_.y_axis = y_axis;
    coordinate_system_.normal = normal;
    return true;
}

void CSmartLine::SetFaceAttachment(unsigned long body_id, int face_index) {
    face_attachment_.body_id = body_id;
    face_attachment_.face_index = body_id != 0 ? face_index : -1;
}

void CSmartLine::ClearFaceAttachment() {
    face_attachment_ = {};
}

bool CSmartLine::HasFaceAttachment() const {
    return face_attachment_.body_id != 0 && face_attachment_.face_index >= 0;
}

const SketchFaceAttachment& CSmartLine::GetFaceAttachment() const {
    return face_attachment_;
}

CPoint3d CSmartLine::LocalToWorld(const CPoint3d& point) const {
    return add(
        coordinate_system_.origin,
        add(
            multiply(coordinate_system_.x_axis, point.x),
            add(
                multiply(coordinate_system_.y_axis, point.y),
                multiply(coordinate_system_.normal, point.z))));
}

CPoint3d CSmartLine::WorldToLocal(const CPoint3d& point) const {
    const CPoint3d relative = subtract(point, coordinate_system_.origin);
    return CPoint3d(
        dot_product(relative, coordinate_system_.x_axis),
        dot_product(relative, coordinate_system_.y_axis),
        dot_product(relative, coordinate_system_.normal));
}

std::vector<CPoint3d> CSmartLine::GetProfilePointsWorld() const {
    std::vector<CPoint3d> result;
    if (lines_.empty()) {
        return result;
    }

    const DisplayGeometry geometry = BuildDisplayGeometry();
    result.push_back(LocalToWorld(geometry.line_starts.front()));
    for (std::size_t line_index = 0; line_index < lines_.size(); ++line_index) {
        if (lines_[line_index]->GetType() == LinkLineType::Bezier
            || lines_[line_index]->GetType() == LinkLineType::Arc) {
            const std::vector<CPoint3d>& samples = geometry.curves[line_index];
            for (std::size_t point_index = 1; point_index < samples.size(); ++point_index) {
                result.push_back(LocalToWorld(samples[point_index]));
            }
        } else {
            result.push_back(LocalToWorld(geometry.line_ends[line_index]));
        }
        const auto fillet = std::find_if(
            fillets_.begin(),
            fillets_.end(),
            [line_index](const CFillet& candidate) {
                return candidate.GetFirstLineIndex() == line_index;
            });
        if (fillet == fillets_.end()) {
            continue;
        }
        const std::vector<CPoint3d> arc =
            fillet->Sample(*lines_[fillet->GetFirstLineIndex()], *lines_[fillet->GetSecondLineIndex()]);
        for (std::size_t point_index = 1; point_index < arc.size(); ++point_index) {
            result.push_back(LocalToWorld(arc[point_index]));
        }
    }

    if (IsClosed() && result.size() > 1) {
        const CPoint3d& first = result.front();
        const CPoint3d& last = result.back();
        const double dx = first.x - last.x;
        const double dy = first.y - last.y;
        const double dz = first.z - last.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= kAxisTolerance) {
            result.pop_back();
        }
    }
    return result;
}

CSmartLine::DisplayGeometry CSmartLine::BuildDisplayGeometry() const {
    DisplayGeometry result;
    result.line_starts.reserve(lines_.size());
    result.line_ends.reserve(lines_.size());
    result.curves.resize(lines_.size());
    std::vector<double> start_parameters(lines_.size(), 0.0);
    std::vector<double> end_parameters(lines_.size(), 1.0);
    for (const auto& line : lines_) {
        result.line_starts.push_back(line->GetStart());
        result.line_ends.push_back(line->GetEnd());
    }
    for (const CFillet& fillet : fillets_) {
        const std::size_t first = fillet.GetFirstLineIndex();
        const std::size_t second = fillet.GetSecondLineIndex();
        if (second >= lines_.size()) {
            continue;
        }
        const FilletGeometry geometry = fillet.Calculate(*lines_[first], *lines_[second]);
        if (!geometry.valid) {
            continue;
        }
        result.line_ends[first] = geometry.tangent_on_first;
        result.line_starts[second] = geometry.tangent_on_second;
        end_parameters[first] = geometry.first_parameter;
        start_parameters[second] = geometry.second_parameter;
        result.arcs.push_back(fillet.Sample(*lines_[first], *lines_[second]));
    }
    for (std::size_t line_index = 0; line_index < lines_.size(); ++line_index) {
        if (lines_[line_index]->GetType() != LinkLineType::Bezier
            && lines_[line_index]->GetType() != LinkLineType::Arc) {
            continue;
        }
        constexpr std::size_t segment_count = 32;
        std::vector<CPoint3d>& curve = result.curves[line_index];
        curve.reserve(segment_count + 1);
        for (std::size_t sample_index = 0; sample_index <= segment_count; ++sample_index) {
            const double fraction =
                static_cast<double>(sample_index) / static_cast<double>(segment_count);
            const double parameter =
                start_parameters[line_index]
                + (end_parameters[line_index] - start_parameters[line_index]) * fraction;
            curve.push_back(lines_[line_index]->GetPoint(parameter));
        }
    }
    return result;
}

void CSmartLine::Render3d(bool selected) const {
    const DisplayGeometry geometry = BuildDisplayGeometry();
    const Color color = GetColor();
    glDisable(GL_DEPTH_TEST);
    ApplyLineAppearance(selected);
    glColor3f(selected ? 0.72f : color.r, selected ? 0.12f : color.g, selected ? 1.0f : color.b);
    glBegin(GL_LINES);
    for (std::size_t index = 0; index < geometry.line_starts.size(); ++index) {
        if (lines_[index]->GetType() == LinkLineType::Bezier
            || lines_[index]->GetType() == LinkLineType::Arc) {
            continue;
        }
        const CPoint3d start = LocalToWorld(geometry.line_starts[index]);
        const CPoint3d end = LocalToWorld(geometry.line_ends[index]);
        glVertex3d(start.x, start.y, start.z);
        glVertex3d(end.x, end.y, end.z);
    }
    glEnd();
    for (const auto& curve : geometry.curves) {
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : curve) {
            const CPoint3d world = LocalToWorld(point);
            glVertex3d(world.x, world.y, world.z);
        }
        glEnd();
    }
    for (const auto& arc : geometry.arcs) {
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : arc) {
            const CPoint3d world = LocalToWorld(point);
            glVertex3d(world.x, world.y, world.z);
        }
        glEnd();
    }
    ResetLineAppearance();
    glEnable(GL_DEPTH_TEST);
}

void CSmartLine::Render2d(float center_x, float center_y, float scale) const {
    const DisplayGeometry geometry = BuildDisplayGeometry();
    const Color color = GetColor();
    ApplyLineAppearance(false);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
    for (std::size_t index = 0; index < geometry.line_starts.size(); ++index) {
        if (lines_[index]->GetType() == LinkLineType::Bezier
            || lines_[index]->GetType() == LinkLineType::Arc) {
            continue;
        }
        const CPoint3d start = LocalToWorld(geometry.line_starts[index]);
        const CPoint3d end = LocalToWorld(geometry.line_ends[index]);
        glVertex2f(center_x + static_cast<float>(start.x) * scale,
                   center_y + static_cast<float>(start.z) * scale);
        glVertex2f(center_x + static_cast<float>(end.x) * scale,
                   center_y + static_cast<float>(end.z) * scale);
    }
    glEnd();
    for (const auto& curve : geometry.curves) {
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : curve) {
            const CPoint3d world = LocalToWorld(point);
            glVertex2f(center_x + static_cast<float>(world.x) * scale,
                       center_y + static_cast<float>(world.z) * scale);
        }
        glEnd();
    }
    for (const auto& arc : geometry.arcs) {
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : arc) {
            const CPoint3d world = LocalToWorld(point);
            glVertex2f(center_x + static_cast<float>(world.x) * scale,
                       center_y + static_cast<float>(world.z) * scale);
        }
        glEnd();
    }
    ResetLineAppearance();
}

bool CSmartLine::HitTest(CurvePoint point, float tolerance) const {
    const DisplayGeometry geometry = BuildDisplayGeometry();
    for (std::size_t index = 0; index < geometry.line_starts.size(); ++index) {
        if (lines_[index]->GetType() == LinkLineType::Bezier
            || lines_[index]->GetType() == LinkLineType::Arc) {
            continue;
        }
        if (distance_2d(
                point,
                LocalToWorld(geometry.line_starts[index]),
                LocalToWorld(geometry.line_ends[index])) <= tolerance) {
            return true;
        }
    }
    for (const auto& curve : geometry.curves) {
        for (std::size_t index = 1; index < curve.size(); ++index) {
            if (distance_2d(
                    point,
                    LocalToWorld(curve[index - 1]),
                    LocalToWorld(curve[index])) <= tolerance) {
                return true;
            }
        }
    }
    for (const auto& arc : geometry.arcs) {
        for (std::size_t index = 1; index < arc.size(); ++index) {
            if (distance_2d(
                    point,
                    LocalToWorld(arc[index - 1]),
                    LocalToWorld(arc[index])) <= tolerance) {
                return true;
            }
        }
    }
    return false;
}

bool CSmartLine::HitTestScreen(
    DomPoint point,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    float tolerance) const
{
    const DisplayGeometry geometry = BuildDisplayGeometry();
    const auto test_segment = [&](const CPoint3d& local_start,
                                  const CPoint3d& local_end) {
        const CPoint3d world_start = LocalToWorld(local_start);
        const CPoint3d world_end = LocalToWorld(local_end);
        DomPoint screen_start{};
        DomPoint screen_end{};
        return world_to_screen(
                   {static_cast<float>(world_start.x),
                    static_cast<float>(world_start.y),
                    static_cast<float>(world_start.z)},
                   screen_start)
            && world_to_screen(
                   {static_cast<float>(world_end.x),
                    static_cast<float>(world_end.y),
                    static_cast<float>(world_end.z)},
                   screen_end)
            && distance_to_screen_segment(
                   point, screen_start, screen_end) <= tolerance;
    };

    for (std::size_t index = 0; index < geometry.line_starts.size(); ++index) {
        if (lines_[index]->GetType() == LinkLineType::Bezier
            || lines_[index]->GetType() == LinkLineType::Arc) {
            continue;
        }
        if (test_segment(
                geometry.line_starts[index], geometry.line_ends[index])) {
            return true;
        }
    }

    const auto test_polyline = [&](const std::vector<CPoint3d>& polyline) {
        for (std::size_t index = 1; index < polyline.size(); ++index) {
            if (test_segment(polyline[index - 1], polyline[index])) {
                return true;
            }
        }
        return false;
    };
    for (const std::vector<CPoint3d>& curve : geometry.curves) {
        if (test_polyline(curve)) {
            return true;
        }
    }
    for (const std::vector<CPoint3d>& arc : geometry.arcs) {
        if (test_polyline(arc)) {
            return true;
        }
    }
    return false;
}

bool CSmartLine::Save(std::ostream& stream) const {
    stream << "SMARTLINE 1\n";
    stream << GetName() << '\n';
    const SketchCoordinateSystem& system = coordinate_system_;
    stream << system.origin.x << ' ' << system.origin.y << ' ' << system.origin.z << '\n';
    stream << system.x_axis.x << ' ' << system.x_axis.y << ' ' << system.x_axis.z << '\n';
    stream << system.normal.x << ' ' << system.normal.y << ' ' << system.normal.z << '\n';
    stream << (closed_ ? 1 : 0) << ' ' << lines_.size() << '\n';
    for (const auto& line : lines_) {
        stream << line_type_name(line->GetType()) << ' '
               << line->GetStart().x << ' ' << line->GetStart().y << ' '
               << line->GetEnd().x << ' ' << line->GetEnd().y;
        if (const auto* bezier = dynamic_cast<const CBezierSpline*>(line.get())) {
            stream << ' ' << bezier->GetControl1().x << ' ' << bezier->GetControl1().y
                   << ' ' << bezier->GetControl2().x << ' ' << bezier->GetControl2().y;
        } else if (const auto* arc = dynamic_cast<const CSketchArcLine*>(line.get())) {
            stream << ' ' << arc->GetPointOnArc().x << ' ' << arc->GetPointOnArc().y;
        }
        stream << '\n';
    }
    stream << constraints_.size() << '\n';
    for (const auto& constraint : constraints_) {
        const char* type_name = "VERTICAL";
        switch (constraint->GetType()) {
        case ConstraintType::Horizontal:
            type_name = "HORIZONTAL";
            break;
        case ConstraintType::Vertical:
            type_name = "VERTICAL";
            break;
        case ConstraintType::TangentAtStart:
            type_name = "TANGENT_START";
            break;
        case ConstraintType::TangentAtEnd:
            type_name = "TANGENT_END";
            break;
        }
        stream << type_name << ' ' << constraint->GetLineIndex() << '\n';
    }
    stream << fillets_.size() << '\n';
    for (const CFillet& fillet : fillets_) {
        stream << fillet.GetFirstLineIndex() << ' '
               << fillet.GetSecondLineIndex() << ' '
               << fillet.GetRadius() << '\n';
    }
    return static_cast<bool>(stream);
}

std::unique_ptr<CAlfaObject> CSmartLine::Clone() const {
    return std::make_unique<CSmartLine>(MakeCopy());
}

CSmartLine CSmartLine::MakeCopy() const {
    CSmartLine result(GetName() + " Copy");
    result.coordinate_system_ = coordinate_system_;
    result.face_attachment_ = face_attachment_;
    result.closed_ = closed_;
    for (const auto& line : lines_) {
        result.lines_.push_back(line->Clone());
    }
    for (const auto& constraint : constraints_) {
        result.constraints_.push_back(constraint->Clone());
    }
    result.fillets_ = fillets_;
    result.SetGroupName(GetGroupName());
    result.SetVisible(IsVisible());
    result.SetColor(GetColor());
    result.SetLineWidth(GetLineWidth());
    result.SetLineStyle(GetLineStyle());
    result.SetMaterial(GetMaterial());
    result.SetMaterialId(GetMaterialId());
    result.SetParametricDefinition(
        GetParametricToolId(), GetParametricParameters());
    return result;
}

void CSmartLine::Translate(Vec3 delta) {
    coordinate_system_.origin.x += delta.x;
    coordinate_system_.origin.y += delta.y;
    coordinate_system_.origin.z += delta.z;
}

void CSmartLine::Rotate(Vec3 center, Vec3 axis, float angle) {
    CPoint3d center_point(center.x, center.y, center.z);
    CPoint3d axis_point(axis.x, axis.y, axis.z);
    coordinate_system_.origin = add(
        center_point,
        rotate_point(subtract(coordinate_system_.origin, center_point), axis_point, angle));
    coordinate_system_.x_axis = rotate_point(coordinate_system_.x_axis, axis_point, angle);
    coordinate_system_.y_axis = rotate_point(coordinate_system_.y_axis, axis_point, angle);
    coordinate_system_.normal = rotate_point(coordinate_system_.normal, axis_point, angle);
}

void CSmartLine::Scale(Vec3 center, Vec3, float factor) {
    if (std::abs(factor) <= kSketchEpsilon) {
        return;
    }
    coordinate_system_.origin.x = center.x + (coordinate_system_.origin.x - center.x) * factor;
    coordinate_system_.origin.y = center.y + (coordinate_system_.origin.y - center.y) * factor;
    coordinate_system_.origin.z = center.z + (coordinate_system_.origin.z - center.z) * factor;
    for (auto& line : lines_) {
        const CPoint3d scaled_start = multiply(line->GetStart(), factor);
        const CPoint3d scaled_end = multiply(line->GetEnd(), factor);
        CBezierSpline* bezier = dynamic_cast<CBezierSpline*>(line.get());
        CSketchArcLine* arc = dynamic_cast<CSketchArcLine*>(line.get());
        const CPoint3d scaled_control1 = bezier
            ? multiply(bezier->GetControl1(), factor)
            : CPoint3d();
        const CPoint3d scaled_control2 = bezier
            ? multiply(bezier->GetControl2(), factor)
            : CPoint3d();
        const CPoint3d scaled_arc_point = arc
            ? multiply(arc->GetPointOnArc(), factor)
            : CPoint3d();
        line->SetStart(scaled_start);
        line->SetEnd(scaled_end);
        if (bezier) {
            bezier->SetControl1(scaled_control1);
            bezier->SetControl2(scaled_control2);
        }
        if (arc) {
            arc->SetPointOnArc(scaled_arc_point);
        }
    }
    for (CFillet& fillet : fillets_) {
        fillet.SetRadius(std::abs(fillet.GetRadius() * factor));
    }
}

void CSmartLine::ScaleLocal(double x_factor, double y_factor) {
    if (std::abs(x_factor) <= kSketchEpsilon
        || std::abs(y_factor) <= kSketchEpsilon) {
        return;
    }
    const auto scaled = [x_factor, y_factor](CPoint3d point) {
        point.x *= x_factor;
        point.y *= y_factor;
        return point;
    };
    for (auto& line : lines_) {
        CBezierSpline* bezier = dynamic_cast<CBezierSpline*>(line.get());
        CSketchArcLine* arc = dynamic_cast<CSketchArcLine*>(line.get());
        const CPoint3d scaled_control1 = bezier
            ? scaled(bezier->GetControl1()) : CPoint3d();
        const CPoint3d scaled_control2 = bezier
            ? scaled(bezier->GetControl2()) : CPoint3d();
        const CPoint3d scaled_arc_point = arc
            ? scaled(arc->GetPointOnArc()) : CPoint3d();
        line->SetStart(scaled(line->GetStart()));
        line->SetEnd(scaled(line->GetEnd()));
        if (bezier) {
            bezier->SetControl1(scaled_control1);
            bezier->SetControl2(scaled_control2);
        }
        if (arc) {
            arc->SetPointOnArc(scaled_arc_point);
        }
    }
    const double fillet_scale = std::min(
        std::abs(x_factor), std::abs(y_factor));
    for (CFillet& fillet : fillets_) {
        fillet.SetRadius(fillet.GetRadius() * fillet_scale);
    }
}

bool CSmartLine::GetBounds(Vec3& min_point, Vec3& max_point) const {
    if (lines_.empty()) {
        return false;
    }
    min_point = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    max_point = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    const DisplayGeometry geometry = BuildDisplayGeometry();
    auto include_point = [&](const CPoint3d& local) {
        const CPoint3d world = LocalToWorld(local);
        min_point.x = std::min(min_point.x, static_cast<float>(world.x));
        min_point.y = std::min(min_point.y, static_cast<float>(world.y));
        min_point.z = std::min(min_point.z, static_cast<float>(world.z));
        max_point.x = std::max(max_point.x, static_cast<float>(world.x));
        max_point.y = std::max(max_point.y, static_cast<float>(world.y));
        max_point.z = std::max(max_point.z, static_cast<float>(world.z));
    };
    for (std::size_t index = 0; index < geometry.line_starts.size(); ++index) {
        include_point(geometry.line_starts[index]);
        include_point(geometry.line_ends[index]);
    }
    for (const auto& arc : geometry.arcs) {
        for (const CPoint3d& point : arc) {
            include_point(point);
        }
    }
    for (const auto& curve : geometry.curves) {
        for (const CPoint3d& point : curve) {
            include_point(point);
        }
    }
    return true;
}

void CSmartLine::RenumberLines() {
    for (std::size_t index = 0; index < lines_.size(); ++index) {
        lines_[index]->SetID(index);
    }
}

void CSmartLine::RemoveInvalidConstraintsAndFillets() {
    constraints_.erase(
        std::remove_if(
            constraints_.begin(),
            constraints_.end(),
            [this](const std::unique_ptr<CConstraint>& constraint) {
                return constraint->GetLineIndex() >= lines_.size();
            }),
        constraints_.end());
    fillets_.erase(
        std::remove_if(
            fillets_.begin(),
            fillets_.end(),
            [this](const CFillet& fillet) {
                return fillet.GetSecondLineIndex() >= lines_.size();
            }),
        fillets_.end());
}
