#include "CBSpline.h"

#include "OpenGLCompat.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <memory>
#include <ostream>
#include <sstream>
#include <utility>

CBSpline::CBSpline()
    : CAlfaObject("B-Spline") {
    SetColor(kDefaultCurveColor);
}

CBSpline::CBSpline(std::string name)
    : CAlfaObject(std::move(name)) {
    SetColor(kDefaultCurveColor);
}

const std::vector<CPoint3d>& CBSpline::GetPoints() const {
    return points_;
}

std::vector<CPoint3d>& CBSpline::GetPoints() {
    return points_;
}

bool CBSpline::IsEmpty() const {
    return points_.empty();
}

bool CBSpline::IsClosed() const {
    return closed_ && CanClose();
}

bool CBSpline::CanClose() const {
    return points_.size() >= 3;
}

void CBSpline::SetClosed(bool closed) {
    closed_ = closed && CanClose();
}

bool CBSpline::Close() {
    if (!CanClose()) {
        closed_ = false;
        return false;
    }
    closed_ = true;
    return true;
}

void CBSpline::Open() {
    closed_ = false;
}

size_t CBSpline::GetPointCount() const {
    return points_.size();
}

void CBSpline::Clear() {
    points_.clear();
    weights_.clear();
    knots_.clear();
    closed_ = false;
}

void CBSpline::AddPoint(CPoint3d point) {
    closed_ = false;
    knots_.clear();
    points_.push_back(point);
    weights_.push_back(1.0);
}

void CBSpline::SetBezierInterpolationPoints(
    const std::vector<CPoint3d>& interpolation_points) {
    curve_type_ = SplineCurveType::Bezier;
    degree_ = 3;
    closed_ = false;
    knots_.clear();
    points_.clear();
    weights_.clear();
    if (interpolation_points.empty()) return;
    if (interpolation_points.size() == 1) {
        AddPoint(interpolation_points.front());
        return;
    }

    const size_t anchor_count = interpolation_points.size();
    std::vector<double> intervals(anchor_count - 1, 1.0);
    for (size_t index = 0; index + 1 < anchor_count; ++index) {
        const CPoint3d delta = interpolation_points[index + 1]
            - interpolation_points[index];
        intervals[index] = std::max(
            1.0e-6,
            std::sqrt(delta.x * delta.x + delta.y * delta.y
                      + delta.z * delta.z));
    }

    std::vector<CPoint3d> second(anchor_count);
    if (anchor_count > 2) {
        const size_t unknown_count = anchor_count - 2;
        std::vector<double> lower(unknown_count, 0.0);
        std::vector<double> diagonal(unknown_count, 0.0);
        std::vector<double> upper(unknown_count, 0.0);
        std::vector<CPoint3d> right(unknown_count);
        for (size_t row = 0; row < unknown_count; ++row) {
            const size_t index = row + 1;
            const double previous_h = intervals[index - 1];
            const double next_h = intervals[index];
            lower[row] = row == 0 ? 0.0 : previous_h;
            diagonal[row] = 2.0 * (previous_h + next_h);
            upper[row] = row + 1 == unknown_count ? 0.0 : next_h;
            right[row] = CPoint3d(
                6.0 * ((interpolation_points[index + 1].x
                         - interpolation_points[index].x) / next_h
                       - (interpolation_points[index].x
                          - interpolation_points[index - 1].x) / previous_h),
                6.0 * ((interpolation_points[index + 1].y
                         - interpolation_points[index].y) / next_h
                       - (interpolation_points[index].y
                          - interpolation_points[index - 1].y) / previous_h),
                6.0 * ((interpolation_points[index + 1].z
                         - interpolation_points[index].z) / next_h
                       - (interpolation_points[index].z
                          - interpolation_points[index - 1].z) / previous_h));
        }
        for (size_t row = 1; row < unknown_count; ++row) {
            const double factor = lower[row] / diagonal[row - 1];
            diagonal[row] -= factor * upper[row - 1];
            right[row] -= right[row - 1] * factor;
        }
        const auto divide_point = [](const CPoint3d& point, double divisor) {
            return CPoint3d(point.x / divisor,
                            point.y / divisor,
                            point.z / divisor);
        };
        second[anchor_count - 2] =
            divide_point(right.back(), diagonal.back());
        for (size_t row = unknown_count - 1; row-- > 0;) {
            second[row + 1] = divide_point(
                right[row] - second[row + 2] * upper[row], diagonal[row]);
        }
    }

    points_.reserve((anchor_count - 1) * 3 + 1);
    points_.push_back(interpolation_points.front());
    for (size_t segment = 0; segment + 1 < anchor_count; ++segment) {
        const double h = intervals[segment];
        const CPoint3d delta = interpolation_points[segment + 1]
            - interpolation_points[segment];
        const CPoint3d chord(delta.x / h, delta.y / h, delta.z / h);
        const CPoint3d start_derivative = chord
            - (second[segment] * 2.0 + second[segment + 1])
                * (h / 6.0);
        const CPoint3d end_derivative = chord
            + (second[segment]
               + second[segment + 1] * 2.0) * (h / 6.0);
        points_.push_back(
            interpolation_points[segment] + start_derivative * (h / 3.0));
        points_.push_back(
            interpolation_points[segment + 1] - end_derivative * (h / 3.0));
        points_.push_back(interpolation_points[segment + 1]);
    }
    weights_.assign(points_.size(), 1.0);
}

void CBSpline::SetClosedBezierInterpolationPoints(
    const std::vector<CPoint3d>& interpolation_points) {
    if (interpolation_points.size() < 3) {
        SetBezierInterpolationPoints(interpolation_points);
        return;
    }

    curve_type_ = SplineCurveType::Bezier;
    degree_ = 3;
    knots_.clear();
    points_.clear();
    weights_.clear();
    const size_t count = interpolation_points.size();
    std::vector<double> intervals(count, 1.0);
    for (size_t index = 0; index < count; ++index) {
        const CPoint3d& start = interpolation_points[index];
        const CPoint3d& end = interpolation_points[(index + 1) % count];
        const double dx = end.x - start.x;
        const double dy = end.y - start.y;
        const double dz = end.z - start.z;
        intervals[index] = std::max(
            1.0e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    // Periodic chord-length tangents give the first and last cubic fragments
    // the same tangent at their shared (duplicated) anchor.
    std::vector<CPoint3d> derivatives(count);
    for (size_t index = 0; index < count; ++index) {
        const size_t previous = (index + count - 1) % count;
        const size_t next = (index + 1) % count;
        const double span = intervals[previous] + intervals[index];
        derivatives[index] = CPoint3d(
            (interpolation_points[next].x - interpolation_points[previous].x) / span,
            (interpolation_points[next].y - interpolation_points[previous].y) / span,
            (interpolation_points[next].z - interpolation_points[previous].z) / span);
    }

    points_.reserve(count * 3 + 1);
    points_.push_back(interpolation_points.front());
    for (size_t segment = 0; segment < count; ++segment) {
        const size_t next = (segment + 1) % count;
        const double control_scale = intervals[segment] / 3.0;
        points_.push_back(CPoint3d(
            interpolation_points[segment].x + derivatives[segment].x * control_scale,
            interpolation_points[segment].y + derivatives[segment].y * control_scale,
            interpolation_points[segment].z + derivatives[segment].z * control_scale));
        points_.push_back(CPoint3d(
            interpolation_points[next].x - derivatives[next].x * control_scale,
            interpolation_points[next].y - derivatives[next].y * control_scale,
            interpolation_points[next].z - derivatives[next].z * control_scale));
        points_.push_back(interpolation_points[next]);
    }
    weights_.assign(points_.size(), 1.0);
    closed_ = true;
}

bool CBSpline::IsBezierChain() const {
    return curve_type_ == SplineCurveType::Bezier
        && points_.size() >= 4
        && (points_.size() - 1) % 3 == 0;
}

bool CBSpline::InsertPoint(size_t index, CPoint3d point, double weight) {
    if (index > points_.size()) {
        return false;
    }
    points_.insert(
        points_.begin() + static_cast<std::vector<CPoint3d>::difference_type>(index),
        point);
    weights_.insert(
        weights_.begin() + static_cast<std::vector<double>::difference_type>(
            std::min(index, weights_.size())),
        std::max(1.0e-6, weight));
    weights_.resize(points_.size(), 1.0);
    knots_.clear();
    closed_ = false;
    return true;
}

bool CBSpline::InsertShapePreservingPoint(double parameter) {
    if (points_.size() < 2) {
        return false;
    }
    const double normalized = std::clamp(parameter, 0.0, 1.0);
    if (normalized <= 1.0e-6 || normalized >= 1.0 - 1.0e-6) {
        return false;
    }

    if (curve_type_ == SplineCurveType::Bezier) {
        if (!IsBezierChain()) return false;
        const size_t segment_count = (points_.size() - 1) / 3;
        const double scaled = normalized * segment_count;
        const size_t segment = std::min(
            static_cast<size_t>(scaled), segment_count - 1);
        const double local = scaled - static_cast<double>(segment);
        if (local <= 1.0e-5 || local >= 1.0 - 1.0e-5) return false;
        const size_t first = segment * 3;
        const CPoint3d p0 = points_[first];
        const CPoint3d p1 = points_[first + 1];
        const CPoint3d p2 = points_[first + 2];
        const CPoint3d p3 = points_[first + 3];
        const CPoint3d a = p0 * (1.0 - local) + p1 * local;
        const CPoint3d b = p1 * (1.0 - local) + p2 * local;
        const CPoint3d c = p2 * (1.0 - local) + p3 * local;
        const CPoint3d d = a * (1.0 - local) + b * local;
        const CPoint3d e = b * (1.0 - local) + c * local;
        const CPoint3d split = d * (1.0 - local) + e * local;
        points_.erase(points_.begin() + static_cast<std::ptrdiff_t>(first + 1),
                      points_.begin() + static_cast<std::ptrdiff_t>(first + 3));
        points_.insert(points_.begin() + static_cast<std::ptrdiff_t>(first + 1),
                       {a, d, split, e, c});
        weights_.assign(points_.size(), 1.0);
        knots_.clear();
        degree_ = 3;
        return true;
    }

    if (IsClosed()) {
        const size_t count = points_.size();
        const size_t segment = std::min(
            static_cast<size_t>(normalized * count), count - 1);
        return InsertPoint(segment + 1, Evaluate(static_cast<float>(normalized)));
    }

    struct HomogeneousPoint {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double w = 1.0;
    };
    const int point_count = static_cast<int>(points_.size());
    const int degree = std::clamp(degree_, 1, point_count - 1);
    const int last_point = point_count - 1;
    const int last_knot = last_point + degree + 1;
    std::vector<double> knots = knots_;
    if (knots.size() != static_cast<size_t>(last_knot + 1)) {
        knots.assign(static_cast<size_t>(last_knot + 1), 0.0);
        const int interior_count = point_count - degree - 1;
        for (int index = 0; index <= last_knot; ++index) {
            if (index <= degree) knots[static_cast<size_t>(index)] = 0.0;
            else if (index >= point_count) knots[static_cast<size_t>(index)] = 1.0;
            else knots[static_cast<size_t>(index)] =
                static_cast<double>(index - degree)
                / static_cast<double>(interior_count + 1);
        }
    }

    const double domain_first = knots[static_cast<size_t>(degree)];
    const double domain_last = knots[static_cast<size_t>(point_count)];
    const double knot = domain_first + normalized * (domain_last - domain_first);
    int span = degree;
    for (int index = degree; index < point_count; ++index) {
        if (knot >= knots[static_cast<size_t>(index)]
            && knot < knots[static_cast<size_t>(index + 1)]) {
            span = index;
            break;
        }
    }
    int multiplicity = 0;
    for (double value : knots) {
        if (std::abs(value - knot) <= 1.0e-10) ++multiplicity;
    }
    if (multiplicity >= degree) {
        return false;
    }

    std::vector<HomogeneousPoint> source(points_.size());
    for (size_t index = 0; index < points_.size(); ++index) {
        const double weight = index < weights_.size()
            ? std::max(1.0e-6, weights_[index]) : 1.0;
        source[index] = {points_[index].x * weight,
                         points_[index].y * weight,
                         points_[index].z * weight,
                         weight};
    }
    std::vector<HomogeneousPoint> result(points_.size() + 1);
    for (int index = 0; index <= span - degree; ++index) {
        result[static_cast<size_t>(index)] = source[static_cast<size_t>(index)];
    }
    for (int index = span - multiplicity; index <= last_point; ++index) {
        result[static_cast<size_t>(index + 1)] = source[static_cast<size_t>(index)];
    }
    for (int index = span - degree + 1;
         index <= span - multiplicity; ++index) {
        const double denominator = knots[static_cast<size_t>(index + degree)]
            - knots[static_cast<size_t>(index)];
        const double alpha = std::abs(denominator) <= 1.0e-12
            ? 0.0 : (knot - knots[static_cast<size_t>(index)]) / denominator;
        const HomogeneousPoint& previous = source[static_cast<size_t>(index - 1)];
        const HomogeneousPoint& current = source[static_cast<size_t>(index)];
        result[static_cast<size_t>(index)] = {
            previous.x * (1.0 - alpha) + current.x * alpha,
            previous.y * (1.0 - alpha) + current.y * alpha,
            previous.z * (1.0 - alpha) + current.z * alpha,
            previous.w * (1.0 - alpha) + current.w * alpha};
    }

    std::vector<double> inserted_knots(knots.size() + 1);
    for (int index = 0; index <= span; ++index) {
        inserted_knots[static_cast<size_t>(index)] = knots[static_cast<size_t>(index)];
    }
    inserted_knots[static_cast<size_t>(span + 1)] = knot;
    for (int index = span + 1; index <= last_knot; ++index) {
        inserted_knots[static_cast<size_t>(index + 1)] = knots[static_cast<size_t>(index)];
    }

    points_.resize(result.size());
    weights_.resize(result.size());
    for (size_t index = 0; index < result.size(); ++index) {
        const double weight = std::max(1.0e-12, result[index].w);
        points_[index] = CPoint3d(result[index].x / weight,
                                  result[index].y / weight,
                                  result[index].z / weight);
        weights_[index] = weight;
    }
    knots_ = std::move(inserted_knots);
    return true;
}

bool CBSpline::SetPoint(size_t index, CPoint3d point) {
    if (index >= points_.size()) {
        return false;
    }
    if (IsBezierChain()) {
        if (index % 3 == 0) {
            const CPoint3d delta(
                point.x - points_[index].x,
                point.y - points_[index].y,
                point.z - points_[index].z);
            if (IsClosed() && (index == 0 || index + 1 == points_.size())) {
                points_.front() = point;
                points_.back() = point;
                points_[1] += delta;
                points_[points_.size() - 2] += delta;
                return true;
            }
            if (index > 0) points_[index - 1] += delta;
            if (index + 1 < points_.size()) points_[index + 1] += delta;
        } else {
            // Every internal anchor of an interpolated Bezier chain is a
            // smooth (G1) join.  Rotating either handle rotates the handle on
            // the other side while preserving its independently set length.
            const bool outgoing_control = index % 3 == 1;
            const size_t anchor = outgoing_control ? index - 1 : index + 1;
            const bool closed_seam_control = IsClosed()
                && ((outgoing_control && anchor == 0)
                    || (!outgoing_control && anchor + 1 == points_.size()));
            const bool has_opposite = closed_seam_control
                || (outgoing_control ? anchor > 0
                                     : anchor + 1 < points_.size());
            if (has_opposite) {
                const size_t opposite = closed_seam_control
                    ? (outgoing_control ? points_.size() - 2 : 1)
                    : (outgoing_control ? anchor - 1 : anchor + 1);
                const CPoint3d& anchor_point = points_[anchor];
                const double dx = point.x - anchor_point.x;
                const double dy = point.y - anchor_point.y;
                const double dz = point.z - anchor_point.z;
                const double moved_length = std::sqrt(dx * dx + dy * dy + dz * dz);
                const double ox = points_[opposite].x - anchor_point.x;
                const double oy = points_[opposite].y - anchor_point.y;
                const double oz = points_[opposite].z - anchor_point.z;
                const double opposite_length =
                    std::sqrt(ox * ox + oy * oy + oz * oz);
                if (moved_length > 1.0e-12 && opposite_length > 1.0e-12) {
                    const double scale = opposite_length / moved_length;
                    points_[opposite] = CPoint3d(
                        anchor_point.x - dx * scale,
                        anchor_point.y - dy * scale,
                        anchor_point.z - dz * scale);
                }
            }
        }
    }
    points_[index] = point;
    return true;
}

bool CBSpline::SetPointDirect(size_t index, CPoint3d point) {
    if (index >= points_.size()) return false;
    points_[index] = point;
    return true;
}

bool CBSpline::RemovePoint(size_t index) {
    if (index >= points_.size()) {
        return false;
    }
    points_.erase(points_.begin() + static_cast<std::vector<CPoint3d>::difference_type>(index));
    if (index < weights_.size()) {
        weights_.erase(weights_.begin() + static_cast<std::vector<double>::difference_type>(index));
    }
    knots_.clear();
    if (!CanClose()) {
        closed_ = false;
    }
    return true;
}

void CBSpline::Reverse() {
    std::reverse(points_.begin(), points_.end());
    std::reverse(weights_.begin(), weights_.end());
    if (!knots_.empty()) {
        const double first = knots_.front();
        const double last = knots_.back();
        std::reverse(knots_.begin(), knots_.end());
        for (double& knot : knots_) knot = first + last - knot;
    }
}

bool CBSpline::ExtendEndpoint(bool at_start, double distance) {
    if (closed_ || points_.size() < 2 || distance <= 0.0) {
        return false;
    }
    const size_t endpoint = at_start ? 0 : points_.size() - 1;
    const size_t neighbor = at_start ? 1 : points_.size() - 2;
    const double dx = points_[endpoint].x - points_[neighbor].x;
    const double dy = points_[endpoint].y - points_[neighbor].y;
    const double dz = points_[endpoint].z - points_[neighbor].z;
    const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length <= 1.0e-12) {
        return false;
    }
    points_[endpoint].x += dx * distance / length;
    points_[endpoint].y += dy * distance / length;
    points_[endpoint].z += dz * distance / length;
    return true;
}

CPoint3d CBSpline::Evaluate(float t) const {
    if (curve_type_ == SplineCurveType::Bezier) {
        return EvaluateBezier(t);
    }
    if (curve_type_ == SplineCurveType::Nurbs
        || (curve_type_ == SplineCurveType::BSpline && !knots_.empty())) {
        return EvaluateNurbs(t);
    }
    if (IsClosed()) {
        return EvaluateClosed(t);
    }

    return EvaluateOpen(points_, t);
}

SplineCurveType CBSpline::GetCurveType() const { return curve_type_; }

void CBSpline::SetCurveType(SplineCurveType type) {
    curve_type_ = type;
    if (curve_type_ != SplineCurveType::BSpline) {
        closed_ = false;
    }
}

int CBSpline::GetDegree() const {
    return points_.size() <= 1
        ? 1
        : std::clamp(degree_, 1, static_cast<int>(points_.size()) - 1);
}

void CBSpline::SetDegree(int degree) {
    degree = std::max(1, degree);
    if (degree_ != degree) knots_.clear();
    degree_ = degree;
}

const std::vector<double>& CBSpline::GetWeights() const { return weights_; }

void CBSpline::SetWeights(std::vector<double> weights) {
    weights.resize(points_.size(), 1.0);
    for (double& weight : weights) {
        weight = std::max(1.0e-6, weight);
    }
    weights_ = std::move(weights);
}

void CBSpline::SetWeight(size_t index, double weight) {
    if (weights_.size() < points_.size()) {
        weights_.resize(points_.size(), 1.0);
    }
    if (index < weights_.size()) {
        weights_[index] = std::max(1.0e-6, weight);
    }
}

const std::vector<double>& CBSpline::GetKnots() const { return knots_; }

bool CBSpline::SetKnots(std::vector<double> knots) {
    const size_t required = points_.size() + static_cast<size_t>(GetDegree()) + 1;
    if (knots.size() != required
        || !std::is_sorted(knots.begin(), knots.end())
        || knots[static_cast<size_t>(GetDegree())]
            >= knots[points_.size()]) {
        return false;
    }
    knots_ = std::move(knots);
    return true;
}

CPoint3d CBSpline::EvaluateBezier(float t) const {
    if (points_.empty()) {
        return {};
    }
    const double u = std::clamp(static_cast<double>(t), 0.0, 1.0);
    if (IsBezierChain()) {
        const size_t segment_count = (points_.size() - 1) / 3;
        const double scaled = u * static_cast<double>(segment_count);
        const size_t segment = std::min(
            static_cast<size_t>(scaled), segment_count - 1);
        const double local = segment + 1 == segment_count && u >= 1.0
            ? 1.0 : scaled - static_cast<double>(segment);
        const double one_minus = 1.0 - local;
        const size_t first = segment * 3;
        return points_[first] * (one_minus * one_minus * one_minus)
            + points_[first + 1]
                * (3.0 * one_minus * one_minus * local)
            + points_[first + 2]
                * (3.0 * one_minus * local * local)
            + points_[first + 3] * (local * local * local);
    }
    std::vector<CPoint3d> work = points_;
    for (size_t level = 1; level < work.size(); ++level) {
        for (size_t i = 0; i + level < work.size(); ++i) {
            work[i] = CPoint3d(
                work[i].x * (1.0 - u) + work[i + 1].x * u,
                work[i].y * (1.0 - u) + work[i + 1].y * u,
                work[i].z * (1.0 - u) + work[i + 1].z * u);
        }
    }
    return work.front();
}

CPoint3d CBSpline::EvaluateNurbs(float t) const {
    if (points_.empty()) {
        return {};
    }
    if (points_.size() == 1) {
        return points_.front();
    }

    if (curve_type_ == SplineCurveType::Nurbs && IsClosed()
        && knots_.empty()) {
        // A periodic NURBS of degree p repeats its first p poles internally.
        // Keep only the unique poles in the document so editing does not show
        // duplicate handles at the seam.  Older files may contain one copied
        // endpoint; ignore that redundant pole here.
        size_t unique_count = points_.size();
        const CPoint3d& first = points_.front();
        const CPoint3d& last = points_.back();
        const double seam_dx = first.x - last.x;
        const double seam_dy = first.y - last.y;
        const double seam_dz = first.z - last.z;
        if (unique_count > 3
            && seam_dx * seam_dx + seam_dy * seam_dy + seam_dz * seam_dz
                <= 1.0e-20) {
            --unique_count;
        }
        if (unique_count >= 3) {
            struct HomogeneousPoint {
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;
                double w = 1.0;
            };
            const int degree = std::clamp(
                degree_, 1, static_cast<int>(unique_count) - 1);
            double normalized = std::clamp(static_cast<double>(t), 0.0, 1.0);
            if (normalized >= 1.0) normalized = 0.0;
            const double parameter = static_cast<double>(degree)
                + normalized * static_cast<double>(unique_count);
            const int span = std::min(
                static_cast<int>(std::floor(parameter)),
                degree + static_cast<int>(unique_count) - 1);
            std::vector<HomogeneousPoint> work(
                static_cast<size_t>(degree + 1));
            for (int index = 0; index <= degree; ++index) {
                const size_t pole = static_cast<size_t>(span - degree + index)
                    % unique_count;
                const double weight = pole < weights_.size()
                    ? std::max(1.0e-6, weights_[pole]) : 1.0;
                work[static_cast<size_t>(index)] = {
                    points_[pole].x * weight,
                    points_[pole].y * weight,
                    points_[pole].z * weight,
                    weight};
            }
            for (int level = 1; level <= degree; ++level) {
                for (int index = degree; index >= level; --index) {
                    const int knot_left = span - degree + index;
                    const int knot_right = span + index - level + 1;
                    const double alpha = (parameter - knot_left)
                        / static_cast<double>(knot_right - knot_left);
                    HomogeneousPoint& current = work[static_cast<size_t>(index)];
                    const HomogeneousPoint& previous =
                        work[static_cast<size_t>(index - 1)];
                    current.x = previous.x * (1.0 - alpha) + current.x * alpha;
                    current.y = previous.y * (1.0 - alpha) + current.y * alpha;
                    current.z = previous.z * (1.0 - alpha) + current.z * alpha;
                    current.w = previous.w * (1.0 - alpha) + current.w * alpha;
                }
            }
            const HomogeneousPoint& result = work.back();
            const double weight = std::max(1.0e-12, result.w);
            return CPoint3d(
                result.x / weight, result.y / weight, result.z / weight);
        }
    }

    struct HomogeneousPoint {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double w = 1.0;
    };
    const int point_count = static_cast<int>(points_.size());
    const int degree = std::clamp(degree_, 1, point_count - 1);
    const int knot_count = point_count + degree + 1;
    std::vector<double> knots = knots_;
    if (knots.size() != static_cast<size_t>(knot_count)) {
        knots.assign(static_cast<size_t>(knot_count), 0.0);
        const int interior_count = point_count - degree - 1;
        for (int i = 0; i < knot_count; ++i) {
            if (i <= degree) knots[static_cast<size_t>(i)] = 0.0;
            else if (i >= point_count) knots[static_cast<size_t>(i)] = 1.0;
            else knots[static_cast<size_t>(i)] =
                static_cast<double>(i - degree) / static_cast<double>(interior_count + 1);
        }
    }

    const double domain_first = knots[static_cast<size_t>(degree)];
    const double domain_last = knots[static_cast<size_t>(point_count)];
    const double u = domain_first
        + std::clamp(static_cast<double>(t), 0.0, 1.0)
            * (domain_last - domain_first);
    if (u <= domain_first) return points_.front();
    if (u >= domain_last) return points_.back();
    int span = degree;
    for (int i = degree; i < point_count; ++i) {
        if (u >= knots[static_cast<size_t>(i)]
            && u < knots[static_cast<size_t>(i + 1)]) {
            span = i;
            break;
        }
    }

    std::vector<HomogeneousPoint> d(static_cast<size_t>(degree + 1));
    for (int j = 0; j <= degree; ++j) {
        const size_t index = static_cast<size_t>(span - degree + j);
        const double weight = index < weights_.size()
            ? std::max(1.0e-6, weights_[index]) : 1.0;
        d[static_cast<size_t>(j)] = {
            points_[index].x * weight,
            points_[index].y * weight,
            points_[index].z * weight,
            weight};
    }
    for (int r = 1; r <= degree; ++r) {
        for (int j = degree; j >= r; --j) {
            const int left = span - degree + j;
            const int right = span + j - r + 1;
            const double denominator = knots[static_cast<size_t>(right)]
                - knots[static_cast<size_t>(left)];
            const double alpha = std::abs(denominator) <= 1.0e-12
                ? 0.0 : (u - knots[static_cast<size_t>(left)]) / denominator;
            HomogeneousPoint& current = d[static_cast<size_t>(j)];
            const HomogeneousPoint& previous = d[static_cast<size_t>(j - 1)];
            current.x = previous.x * (1.0 - alpha) + current.x * alpha;
            current.y = previous.y * (1.0 - alpha) + current.y * alpha;
            current.z = previous.z * (1.0 - alpha) + current.z * alpha;
            current.w = previous.w * (1.0 - alpha) + current.w * alpha;
        }
    }
    const HomogeneousPoint& result = d[static_cast<size_t>(degree)];
    if (std::abs(result.w) <= 1.0e-12) {
        return {};
    }
    return CPoint3d(result.x / result.w, result.y / result.w, result.z / result.w);
}

CPoint3d CBSpline::EvaluateClosed(float t) const {
    if (points_.empty()) {
        return {};
    }
    if (points_.size() == 1) {
        return points_.front();
    }

    const float u_global = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(points_.size());
    const size_t segment = std::min(static_cast<size_t>(u_global), points_.size() - 1);
    const float u = u_global - static_cast<float>(segment);
    const size_t count = points_.size();
    const CPoint3d& p0 = points_[(segment + count - 1) % count];
    const CPoint3d& p1 = points_[segment % count];
    const CPoint3d& p2 = points_[(segment + 1) % count];
    const CPoint3d& p3 = points_[(segment + 2) % count];

    const float u2 = u * u;
    const float u3 = u2 * u;
    return CPoint3d(
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * u + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * u2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * u3),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * u + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * u2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * u3),
        0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * u + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * u2 + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * u3));
}

CPoint3d CBSpline::EvaluateOpen(const std::vector<CPoint3d>& points, float t) const {
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1) {
        return points.front();
    }
    if (points.size() < 3) {
        const float scaled = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(points.size() - 1);
        const size_t index = std::min(static_cast<size_t>(scaled), points.size() - 2);
        const float local = scaled - static_cast<float>(index);
        const CPoint3d& a = points[index];
        const CPoint3d& b = points[index + 1];
        return CPoint3d(a.x + (b.x - a.x) * local,
                        a.y + (b.y - a.y) * local,
                        a.z + (b.z - a.z) * local);
    }

    const int degree = std::clamp(
        degree_, 1, static_cast<int>(points.size()) - 1);
    const int point_count = static_cast<int>(points.size());
    const int knot_count = point_count + degree + 1;
    std::vector<float> knots(static_cast<size_t>(knot_count), 0.0f);
    const int interior_count = point_count - degree - 1;
    for (int i = 0; i < knot_count; ++i) {
        if (i <= degree) {
            knots[static_cast<size_t>(i)] = 0.0f;
        } else if (i >= point_count) {
            knots[static_cast<size_t>(i)] = 1.0f;
        } else {
            knots[static_cast<size_t>(i)] = static_cast<float>(i - degree) / static_cast<float>(interior_count + 1);
        }
    }

    const float u = std::clamp(t, 0.0f, 1.0f);
    if (u <= 0.0f) {
        return points.front();
    }
    if (u >= 1.0f) {
        return points.back();
    }

    int span = degree;
    for (int i = degree; i < point_count; ++i) {
        if (u >= knots[static_cast<size_t>(i)] && u < knots[static_cast<size_t>(i + 1)]) {
            span = i;
            break;
        }
    }

    std::vector<CPoint3d> d(static_cast<size_t>(degree + 1));
    for (int j = 0; j <= degree; ++j) {
        d[static_cast<size_t>(j)] = points[static_cast<size_t>(span - degree + j)];
    }

    for (int r = 1; r <= degree; ++r) {
        for (int j = degree; j >= r; --j) {
            const int knot_left = span - degree + j;
            const int knot_right = span + j - r + 1;
            const float denominator = knots[static_cast<size_t>(knot_right)] - knots[static_cast<size_t>(knot_left)];
            const float alpha = denominator <= 0.000001f
                ? 0.0f
                : (u - knots[static_cast<size_t>(knot_left)]) / denominator;
            CPoint3d& current = d[static_cast<size_t>(j)];
            const CPoint3d& previous = d[static_cast<size_t>(j - 1)];
            current = CPoint3d(previous.x * (1.0f - alpha) + current.x * alpha,
                               previous.y * (1.0f - alpha) + current.y * alpha,
                               previous.z * (1.0f - alpha) + current.z * alpha);
        }
    }

    return d[static_cast<size_t>(degree)];
}

void CBSpline::Render3d(bool selected) const {
    Render3d(selected, false, 0);
}

void CBSpline::Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const {
    if (points_.empty()) {
        return;
    }

    const Color color = GetColor();
    glDisable(GL_DEPTH_TEST);
    glLineWidth(selected ? 4.0f : 3.0f);
    glColor3f(selected ? 1.0f : color.r, selected ? 0.12f : color.g, selected ? 0.18f : color.b);
    glBegin(GL_LINE_STRIP);
    const int samples = std::max(2, static_cast<int>(points_.size()) * 24);
    for (int i = 0; i <= samples; ++i) {
        const CPoint3d point = Evaluate(static_cast<float>(i) / static_cast<float>(samples));
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    if (selected && has_selected_point) {
        glLineWidth(1.0f);
        glColor3f(0.65f, 0.65f, 0.65f);
        glBegin(GL_LINE_STRIP);
        for (const CPoint3d& point : points_) {
            glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
        }
        if (IsClosed()) {
            const CPoint3d& point = points_.front();
            glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
        }
        glEnd();
        for (size_t i = 0; i < points_.size(); ++i) {
            const bool bezier_control = IsBezierChain() && i % 3 != 0;
            DrawPointBox(points_[i], selected,
                         has_selected_point && i == selected_point_index,
                         bezier_control);
        }
    }
    glEnable(GL_DEPTH_TEST);
}

void CBSpline::Render2d(float center_x, float center_y, float scale) const {
    if (points_.empty()) {
        return;
    }
    const Color color = GetColor();
    glLineWidth(2.0f);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_STRIP);
    const int samples = std::max(2, static_cast<int>(points_.size()) * 24);
    for (int i = 0; i <= samples; ++i) {
        const CPoint3d point = Evaluate(static_cast<float>(i) / static_cast<float>(samples));
        glVertex2f(center_x + static_cast<float>(point.x) * scale, center_y + static_cast<float>(point.z) * scale);
    }
    glEnd();
}

bool CBSpline::HitTest(CurvePoint point, float tolerance) const {
    if (points_.empty()) {
        return false;
    }
    const int samples = std::max(2, static_cast<int>(points_.size()) * 24);
    CPoint3d previous = Evaluate(0.0f);
    for (int i = 1; i <= samples; ++i) {
        const CPoint3d current = Evaluate(static_cast<float>(i) / static_cast<float>(samples));
        if (DistanceToSegment(point, previous, current) <= tolerance) {
            return true;
        }
        previous = current;
    }
    return false;
}

std::unique_ptr<CAlfaObject> CBSpline::Clone() const {
    auto copy = std::make_unique<CBSpline>(GetName() + " Copy");
    copy->points_ = points_;
    copy->weights_ = weights_;
    copy->knots_ = knots_;
    copy->curve_type_ = curve_type_;
    copy->degree_ = degree_;
    copy->closed_ = closed_;
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->SetParametricDefinition(
        GetParametricToolId(), GetParametricParameters());
    copy->m_LayerID = m_LayerID;
    return copy;
}

void CBSpline::Translate(Vec3 delta) {
    for (CPoint3d& point : points_) {
        point.x += delta.x;
        point.y += delta.y;
        point.z += delta.z;
    }
}

void CBSpline::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (CPoint3d& point : points_) {
        const Vec3 local{static_cast<float>(point.x) - center.x,
                         static_cast<float>(point.y) - center.y,
                         static_cast<float>(point.z) - center.z};
        const Vec3 rotated = rotate_around_axis(local, axis, angle) + center;
        point.x = rotated.x;
        point.y = rotated.y;
        point.z = rotated.z;
    }
}

void CBSpline::Scale(Vec3 center, Vec3 axis, float factor) {
    for (CPoint3d& point : points_) {
        const Vec3 local{static_cast<float>(point.x) - center.x,
                         static_cast<float>(point.y) - center.y,
                         static_cast<float>(point.z) - center.z};
        const Vec3 scaled = (dot(axis, axis) <= 0.000001f ? scale_uniform(local, factor) : scale_along_axis(local, axis, factor)) + center;
        point.x = scaled.x;
        point.y = scaled.y;
        point.z = scaled.z;
    }
}

bool CBSpline::GetBounds(Vec3& min_point, Vec3& max_point) const {
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

void CBSpline::Edit(NativeWindowHandle parent_window) {
    CAlfaObject::Edit(parent_window);
}

bool CBSpline::Save(std::ostream& stream) const {
    const Material material = GetMaterial();
    stream << "BSpline \"" << GetName() << "\" "
           << material.diffuse.r << " " << material.diffuse.g << " " << material.diffuse.b << " "
           << material.alpha << " " << material.specular << " " << material.shininess << " "
           << (IsClosed() ? 1 : 0) << " "
           << static_cast<int>(curve_type_) << " " << GetDegree() << " "
           << points_.size() << "\n";
    for (size_t index = 0; index < points_.size(); ++index) {
        const CPoint3d& point = points_[index];
        const double weight = index < weights_.size() ? weights_[index] : 1.0;
        stream << point.x << " " << point.y << " " << point.z << " "
               << weight << "\n";
    }
    return static_cast<bool>(stream);
}

bool CBSpline::Load(std::istream& stream) {
    std::string keyword;
    stream >> keyword;
    if (keyword != "BSpline") {
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

    Material material{};
    size_t count = 0;
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
    bool has_weights = false;
    closed_ = false;
    curve_type_ = SplineCurveType::BSpline;
    degree_ = 3;
    if (values.size() == 4) {
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
    } else if (values.size() == 7) {
        material.alpha = values[0];
        material.specular = values[1];
        material.shininess = values[2];
        closed_ = values[3] != 0.0f;
        const int type = static_cast<int>(values[4]);
        if (type < static_cast<int>(SplineCurveType::BSpline)
            || type > static_cast<int>(SplineCurveType::Nurbs)) {
            return false;
        }
        curve_type_ = static_cast<SplineCurveType>(type);
        degree_ = std::max(1, static_cast<int>(values[5]));
        count = static_cast<size_t>(values[6]);
        has_weights = true;
    } else {
        return false;
    }
    if (!stream) {
        return false;
    }
    SetMaterial(material);

    std::vector<CPoint3d> loaded;
    std::vector<double> loaded_weights;
    loaded.reserve(count);
    loaded_weights.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        CPoint3d point{};
        stream >> point.x >> point.y >> point.z;
        double weight = 1.0;
        if (has_weights) {
            stream >> weight;
        }
        if (!stream) {
            return false;
        }
        loaded.push_back(point);
        loaded_weights.push_back(weight);
    }
    const bool loaded_closed = closed_;
    points_ = std::move(loaded);
    SetWeights(std::move(loaded_weights));
    SetCurveType(curve_type_);
    SetClosed(loaded_closed);
    return true;
}

float CBSpline::DistanceToSegment(CurvePoint point, const CPoint3d& start, const CPoint3d& end) const {
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

void CBSpline::DrawPointBox(const CPoint3d& point, bool selected,
                           bool point_selected, bool bezier_control) const {
    const float half = point_selected ? 0.056f : (selected ? 0.0385f : 0.0315f);
    const float height = point_selected ? 0.098f : (selected ? 0.0665f : 0.0595f);
    const float x = static_cast<float>(point.x) - half;
    const float y = static_cast<float>(point.y) - half;
    const float z = static_cast<float>(point.z) - half;
    const float x2 = static_cast<float>(point.x) + half;
    const float y2 = y + height;
    const float z2 = static_cast<float>(point.z) + half;

    const float r = point_selected ? 1.0f : (bezier_control ? 0.15f : 0.0f);
    const float g = point_selected ? 0.9f : (bezier_control ? 0.75f : 1.0f);
    const float b = point_selected ? 0.0f : (bezier_control ? 1.0f : 0.0f);

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
