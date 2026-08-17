#include "ConstraintBezierTangent.h"

#include "BezierSpline.h"
#include "SmartLine.h"

#include <cmath>

namespace {
constexpr double kTangentEpsilon = 1.0e-9;

CPoint3d subtract(CPoint3d a, CPoint3d b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

CPoint3d add_scaled(CPoint3d point, CPoint3d direction, double scale) {
    return {point.x + direction.x * scale,
            point.y + direction.y * scale,
            point.z + direction.z * scale};
}

double length(CPoint3d vector) {
    return std::sqrt(vector.x * vector.x + vector.y * vector.y
                     + vector.z * vector.z);
}
}

CConstraintBezierTangent::CConstraintBezierTangent(
    std::size_t line_index, bool at_start)
    : CConstraint(line_index), at_start_(at_start) {
}

ConstraintType CConstraintBezierTangent::GetType() const {
    return at_start_ ? ConstraintType::TangentAtStart
                     : ConstraintType::TangentAtEnd;
}

bool CConstraintBezierTangent::Apply(CSmartLine& sketch) const {
    auto* bezier = dynamic_cast<CBezierSpline*>(sketch.GetLine(GetLineIndex()));
    if (!bezier || sketch.GetNumLines() < 2) {
        return false;
    }

    if (at_start_) {
        const std::size_t previous_index = GetLineIndex() > 0
            ? GetLineIndex() - 1
            : sketch.IsClosed() ? sketch.GetNumLines() - 1 : sketch.GetNumLines();
        const CLinkLine* previous = sketch.GetLine(previous_index);
        if (!previous) {
            return false;
        }
        CPoint3d direction = previous->GetTangent(1.0);
        const double direction_length = length(direction);
        const double handle_length = length(
            subtract(bezier->GetControl1(), bezier->GetStart()));
        if (direction_length <= kTangentEpsilon
            || handle_length <= kTangentEpsilon) {
            return false;
        }
        bezier->SetControl1(add_scaled(
            bezier->GetStart(), direction,
            handle_length / direction_length));
        return true;
    }

    const std::size_t next_index = GetLineIndex() + 1 < sketch.GetNumLines()
        ? GetLineIndex() + 1
        : sketch.IsClosed() ? 0 : sketch.GetNumLines();
    const CLinkLine* next = sketch.GetLine(next_index);
    if (!next) {
        return false;
    }
    CPoint3d direction = next->GetTangent(0.0);
    const double direction_length = length(direction);
    const double handle_length = length(
        subtract(bezier->GetEnd(), bezier->GetControl2()));
    if (direction_length <= kTangentEpsilon
        || handle_length <= kTangentEpsilon) {
        return false;
    }
    bezier->SetControl2(add_scaled(
        bezier->GetEnd(), direction,
        -handle_length / direction_length));
    return true;
}

std::unique_ptr<CConstraint> CConstraintBezierTangent::Clone() const {
    return std::make_unique<CConstraintBezierTangent>(
        GetLineIndex(), at_start_);
}
