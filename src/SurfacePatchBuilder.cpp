#include "SurfacePatchBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace {

constexpr double kRelativeEpsilon = 1.0e-9;

double distance_sq(SurfacePatchPoint a, SurfacePatchPoint b)
{
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return du * du + dv * dv;
}

double cross(SurfacePatchPoint a, SurfacePatchPoint b, SurfacePatchPoint p)
{
    return (b.u - a.u) * (p.v - a.v) - (b.v - a.v) * (p.u - a.u);
}

double signed_area(const std::vector<SurfacePatchPoint>& polygon)
{
    double area = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const SurfacePatchPoint& a = polygon[i];
        const SurfacePatchPoint& b = polygon[(i + 1) % polygon.size()];
        area += a.u * b.v - b.u * a.v;
    }
    return area * 0.5;
}

double geometry_scale(const std::vector<std::vector<SurfacePatchPoint>>& contours)
{
    double min_u = std::numeric_limits<double>::max();
    double max_u = std::numeric_limits<double>::lowest();
    double min_v = std::numeric_limits<double>::max();
    double max_v = std::numeric_limits<double>::lowest();
    for (const auto& contour : contours) {
        for (SurfacePatchPoint point : contour) {
            min_u = std::min(min_u, point.u);
            max_u = std::max(max_u, point.u);
            min_v = std::min(min_v, point.v);
            max_v = std::max(max_v, point.v);
        }
    }
    if (min_u > max_u || min_v > max_v)
        return 1.0;
    return std::max({max_u - min_u, max_v - min_v, 1.0});
}

void normalize_polygon(std::vector<SurfacePatchPoint>& polygon, double epsilon)
{
    std::vector<SurfacePatchPoint> clean;
    clean.reserve(polygon.size());
    const double epsilon_sq = epsilon * epsilon;
    for (SurfacePatchPoint point : polygon) {
        if (clean.empty() || distance_sq(clean.back(), point) > epsilon_sq)
            clean.push_back(point);
    }
    if (clean.size() > 1 && distance_sq(clean.front(), clean.back()) <= epsilon_sq)
        clean.pop_back();
    polygon = std::move(clean);
}

bool point_on_segment(SurfacePatchPoint p,
                      SurfacePatchPoint a,
                      SurfacePatchPoint b,
                      double epsilon)
{
    if (std::fabs(cross(a, b, p)) > epsilon)
        return false;
    return p.u >= std::min(a.u, b.u) - epsilon
        && p.u <= std::max(a.u, b.u) + epsilon
        && p.v >= std::min(a.v, b.v) - epsilon
        && p.v <= std::max(a.v, b.v) + epsilon;
}

bool point_in_polygon(const std::vector<SurfacePatchPoint>& polygon,
                      SurfacePatchPoint point,
                      double epsilon,
                      bool include_boundary = true)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const SurfacePatchPoint& a = polygon[j];
        const SurfacePatchPoint& b = polygon[i];
        if (point_on_segment(point, a, b, epsilon))
            return include_boundary;
        const bool crosses = ((a.v > point.v) != (b.v > point.v))
            && point.u < (b.u - a.u) * (point.v - a.v) / (b.v - a.v) + a.u;
        if (crosses)
            inside = !inside;
    }
    return inside;
}

int orientation(SurfacePatchPoint a,
                SurfacePatchPoint b,
                SurfacePatchPoint c,
                double epsilon)
{
    const double value = cross(a, b, c);
    if (value > epsilon)
        return 1;
    if (value < -epsilon)
        return -1;
    return 0;
}

bool segments_intersect(SurfacePatchPoint a,
                        SurfacePatchPoint b,
                        SurfacePatchPoint c,
                        SurfacePatchPoint d,
                        double epsilon)
{
    const int o1 = orientation(a, b, c, epsilon);
    const int o2 = orientation(a, b, d, epsilon);
    const int o3 = orientation(c, d, a, epsilon);
    const int o4 = orientation(c, d, b, epsilon);
    if (o1 != o2 && o3 != o4)
        return true;
    return (o1 == 0 && point_on_segment(c, a, b, epsilon))
        || (o2 == 0 && point_on_segment(d, a, b, epsilon))
        || (o3 == 0 && point_on_segment(a, c, d, epsilon))
        || (o4 == 0 && point_on_segment(b, c, d, epsilon));
}

bool segment_crosses_polygon(const std::vector<SurfacePatchPoint>& polygon,
                             SurfacePatchPoint a,
                             SurfacePatchPoint b,
                             const SurfacePatchPoint* allowed_endpoint,
                             double epsilon)
{
    const double epsilon_sq = epsilon * epsilon;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const SurfacePatchPoint& c = polygon[i];
        const SurfacePatchPoint& d = polygon[(i + 1) % polygon.size()];
        if (!segments_intersect(a, b, c, d, epsilon))
            continue;
        if (allowed_endpoint
            && (distance_sq(c, *allowed_endpoint) <= epsilon_sq
                || distance_sq(d, *allowed_endpoint) <= epsilon_sq)) {
            continue;
        }
        return true;
    }
    return false;
}

bool bridge_is_visible(SurfacePatchPoint hole_point,
                       SurfacePatchPoint patch_point,
                       const std::vector<SurfacePatchPoint>& patch,
                       const std::vector<SurfacePatchPoint>& hole,
                       const std::vector<std::vector<SurfacePatchPoint>>& obstacles,
                       double epsilon)
{
    if (distance_sq(hole_point, patch_point) <= epsilon * epsilon)
        return false;
    if (segment_crosses_polygon(patch, hole_point, patch_point, &patch_point, epsilon)
        || segment_crosses_polygon(hole, hole_point, patch_point, &hole_point, epsilon)) {
        return false;
    }
    for (const auto& obstacle : obstacles) {
        if (segment_crosses_polygon(obstacle, hole_point, patch_point, nullptr, epsilon))
            return false;
    }
    for (int sample = 1; sample < 10; ++sample) {
        const double alpha = static_cast<double>(sample) / 10.0;
        const SurfacePatchPoint p{
            hole_point.u + (patch_point.u - hole_point.u) * alpha,
            hole_point.v + (patch_point.v - hole_point.v) * alpha};
        if (!point_in_polygon(patch, p, epsilon)
            || point_in_polygon(hole, p, epsilon, false)) {
            return false;
        }
        for (const auto& obstacle : obstacles) {
            if (point_in_polygon(obstacle, p, epsilon, false))
                return false;
        }
    }
    return true;
}

std::vector<size_t> bridge_anchor_indices(const std::vector<SurfacePatchPoint>& hole,
                                          size_t requested_count,
                                          size_t preferred_anchor)
{
    if (hole.empty())
        return {};
    std::array<size_t, 4> extrema{};
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].u < hole[extrema[0]].u)
            extrema[0] = i;
        if (hole[i].v > hole[extrema[1]].v)
            extrema[1] = i;
        if (hole[i].u > hole[extrema[2]].u)
            extrema[2] = i;
        if (hole[i].v < hole[extrema[3]].v)
            extrema[3] = i;
    }
    std::vector<size_t> indices(extrema.begin(), extrema.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (requested_count == 2 && preferred_anchor < hole.size()) {
        size_t opposite = preferred_anchor;
        double farthest_distance = -1.0;
        for (size_t i = 0; i < hole.size(); ++i) {
            const double candidate_distance =
                distance_sq(hole[preferred_anchor], hole[i]);
            if (candidate_distance > farthest_distance) {
                farthest_distance = candidate_distance;
                opposite = i;
            }
        }
        indices = {preferred_anchor, opposite};
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    } else if (requested_count == 2 && indices.size() > 2) {
        std::pair<size_t, size_t> farthest{indices[0], indices[1]};
        double farthest_distance = distance_sq(hole[farthest.first], hole[farthest.second]);
        for (size_t i = 0; i < indices.size(); ++i) {
            for (size_t j = i + 1; j < indices.size(); ++j) {
                const double candidate_distance = distance_sq(hole[indices[i]], hole[indices[j]]);
                if (candidate_distance > farthest_distance) {
                    farthest_distance = candidate_distance;
                    farthest = {indices[i], indices[j]};
                }
            }
        }
        indices = {farthest.first, farthest.second};
        std::sort(indices.begin(), indices.end());
    } else if (indices.size() > requested_count) {
        indices.resize(requested_count);
    }
    if (indices.size() >= 2)
        return indices;

    indices = {0, hole.size() / 2};
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

size_t closest_visible_bridge_anchor(
    const std::vector<SurfacePatchPoint>& patch,
    const std::vector<SurfacePatchPoint>& hole,
    const std::vector<std::vector<SurfacePatchPoint>>& obstacles,
    double epsilon)
{
    size_t best_anchor = hole.size();
    double best_distance = std::numeric_limits<double>::max();
    for (size_t hole_index = 0; hole_index < hole.size(); ++hole_index) {
        for (size_t patch_index = 0; patch_index < patch.size(); ++patch_index) {
            if (!patch[patch_index].boundary_node
                || !bridge_is_visible(hole[hole_index], patch[patch_index],
                                      patch, hole, obstacles, epsilon)) {
                continue;
            }
            const double candidate_distance =
                distance_sq(hole[hole_index], patch[patch_index]);
            if (candidate_distance < best_distance) {
                best_distance = candidate_distance;
                best_anchor = hole_index;
            }
        }
    }
    return best_anchor;
}

struct BridgeCandidate {
    size_t patch_index = 0;
    double cost = 0.0;
};

bool choose_ordered_bridges(
    const std::vector<SurfacePatchPoint>& patch,
    const std::vector<SurfacePatchPoint>& hole,
    const std::vector<size_t>& hole_indices,
    const std::vector<std::vector<SurfacePatchPoint>>& obstacles,
    double epsilon,
    std::vector<size_t>& patch_indices)
{
    std::vector<std::vector<BridgeCandidate>> candidates(hole_indices.size());
    SurfacePatchPoint hole_center{};
    for (SurfacePatchPoint point : hole) {
        hole_center.u += point.u;
        hole_center.v += point.v;
    }
    hole_center.u /= static_cast<double>(hole.size());
    hole_center.v /= static_cast<double>(hole.size());
    for (size_t anchor = 0; anchor < hole_indices.size(); ++anchor) {
        std::vector<BridgeCandidate> any_direction;
        const SurfacePatchPoint hole_point = hole[hole_indices[anchor]];
        const double radial_u = hole_point.u - hole_center.u;
        const double radial_v = hole_point.v - hole_center.v;
        const double radial_length = std::hypot(radial_u, radial_v);
        for (size_t i = 0; i < patch.size(); ++i) {
            if (!patch[i].boundary_node)
                continue;
            if (!bridge_is_visible(hole_point, patch[i],
                                   patch, hole, obstacles, epsilon)) {
                continue;
            }
            const double candidate_distance_sq = distance_sq(hole_point, patch[i]);
            BridgeCandidate candidate{i, candidate_distance_sq};
            any_direction.push_back(candidate);
            if (radial_length <= epsilon)
                continue;
            const double delta_u = patch[i].u - hole_point.u;
            const double delta_v = patch[i].v - hole_point.v;
            const double delta_length = std::hypot(delta_u, delta_v);
            if (delta_length <= epsilon)
                continue;
            const double alignment = (delta_u * radial_u + delta_v * radial_v)
                / (delta_length * radial_length);
            // A four-way island cut must leave the hole in the direction of
            // its corresponding extremum.  Distance-only scoring made all
            // four bridges converge on the nearest side for a tiny hole.
            if (alignment <= 0.05)
                continue;
            candidate.cost = candidate_distance_sq
                / std::max(alignment * alignment, 1.0e-6);
            candidates[anchor].push_back(candidate);
        }
        // Concave patches may genuinely have no visible boundary point in the
        // outward half-plane. Retain the old visibility/distance behaviour as
        // a local fallback instead of rejecting an otherwise splittable hole.
        if (candidates[anchor].empty())
            candidates[anchor] = std::move(any_direction);
        std::sort(candidates[anchor].begin(), candidates[anchor].end(),
                  [](const BridgeCandidate& a, const BridgeCandidate& b) {
                      return a.cost < b.cost;
                  });
        if (candidates[anchor].size() > 24)
            candidates[anchor].resize(24);
        if (candidates[anchor].empty())
            return false;
    }

    double best_cost = std::numeric_limits<double>::max();
    std::vector<size_t> current(hole_indices.size());
    for (const BridgeCandidate& first : candidates.front()) {
        current[0] = first.patch_index;
        const auto search = [&](const auto& self, size_t anchor, size_t previous,
                                double cost) -> void {
            if (cost >= best_cost)
                return;
            if (anchor == candidates.size()) {
                best_cost = cost;
                patch_indices = current;
                return;
            }
            for (const BridgeCandidate& candidate : candidates[anchor]) {
                size_t unwrapped = candidate.patch_index;
                while (unwrapped <= previous)
                    unwrapped += patch.size();
                if (unwrapped >= first.patch_index + patch.size())
                    continue;
                current[anchor] = candidate.patch_index;
                self(self, anchor + 1, unwrapped, cost + candidate.cost);
            }
        };
        search(search, 1, first.patch_index, first.cost);
    }
    return !patch_indices.empty();
}

void append_unique(std::vector<SurfacePatchPoint>& result,
                   SurfacePatchPoint point,
                   double epsilon)
{
    if (result.empty() || distance_sq(result.back(), point) > epsilon * epsilon)
        result.push_back(point);
}

void append_forward_arc(std::vector<SurfacePatchPoint>& result,
                        const std::vector<SurfacePatchPoint>& polygon,
                        size_t first,
                        size_t last,
                        double epsilon)
{
    size_t index = first;
    append_unique(result, polygon[index], epsilon);
    while (index != last) {
        index = (index + 1) % polygon.size();
        append_unique(result, polygon[index], epsilon);
    }
}

void append_reverse_arc(std::vector<SurfacePatchPoint>& result,
                        const std::vector<SurfacePatchPoint>& polygon,
                        size_t first,
                        size_t last,
                        double epsilon)
{
    size_t index = first;
    append_unique(result, polygon[index], epsilon);
    while (index != last) {
        index = (index + polygon.size() - 1) % polygon.size();
        append_unique(result, polygon[index], epsilon);
    }
}

size_t bridge_interval_count(SurfacePatchPoint start,
                             SurfacePatchPoint destination,
                             double step,
                             double epsilon)
{
    const double length = std::sqrt(distance_sq(start, destination));
    return step > epsilon
        ? std::max<size_t>(1, static_cast<size_t>(std::ceil(length / step)))
        : 1;
}

void append_bridge(std::vector<SurfacePatchPoint>& result,
                   SurfacePatchPoint destination,
                   size_t intervals,
                   double epsilon)
{
    if (result.empty()) {
        result.push_back(destination);
        return;
    }
    const SurfacePatchPoint start = result.back();
    intervals = std::max<size_t>(1, intervals);
    // Preserve density along the bridge, but mark its internal samples as
    // auxiliary. Only the original start and destination boundary nodes may
    // anchor bridges created while processing later holes.
    for (size_t i = 1; i < intervals; ++i) {
        const double alpha = static_cast<double>(i)
            / static_cast<double>(intervals);
        append_unique(result,
                      {start.u + (destination.u - start.u) * alpha,
                       start.v + (destination.v - start.v) * alpha,
                       false},
                      epsilon);
    }
    append_unique(result, destination, epsilon);
}

bool polygon_self_intersects(const std::vector<SurfacePatchPoint>& polygon,
                             double epsilon)
{
    for (size_t i = 0; i < polygon.size(); ++i) {
        const size_t i_next = (i + 1) % polygon.size();
        for (size_t j = i + 1; j < polygon.size(); ++j) {
            const size_t j_next = (j + 1) % polygon.size();
            if (i == j || i_next == j || j_next == i)
                continue;
            if (segments_intersect(polygon[i], polygon[i_next],
                                   polygon[j], polygon[j_next], epsilon)) {
                return true;
            }
        }
    }
    return false;
}

bool split_patch(const std::vector<SurfacePatchPoint>& patch,
                 const std::vector<SurfacePatchPoint>& hole,
                 const std::vector<std::vector<SurfacePatchPoint>>& obstacles,
                 double bridge_step,
                 double epsilon,
                 std::vector<std::vector<SurfacePatchPoint>>& result)
{
    // Preserve the original island construction: first try four cuts from
    // the hole (left/top/right/bottom extrema), producing four simple sectors.
    // The two-cut form is only a fallback when four visible ordered bridges
    // cannot be constructed.
    const std::array<size_t, 2> bridge_counts{4, 2};
    for (size_t requested : bridge_counts) {
        std::vector<SurfacePatchPoint> parity_patch = patch;
        if (((parity_patch.size() + hole.size()) & 1U) != 0U) {
            bool subdivided_auxiliary_bridge = false;
            for (size_t i = 0; i < parity_patch.size(); ++i) {
                const size_t next = (i + 1) % parity_patch.size();
                if (parity_patch[i].boundary_node
                    && parity_patch[next].boundary_node) {
                    continue;
                }
                const SurfacePatchPoint midpoint{
                    (parity_patch[i].u + parity_patch[next].u) * 0.5,
                    (parity_patch[i].v + parity_patch[next].v) * 0.5,
                    false};
                parity_patch.insert(
                    parity_patch.begin() + static_cast<std::ptrdiff_t>(next),
                    midpoint);
                subdivided_auxiliary_bridge = true;
                break;
            }
            if (!subdivided_auxiliary_bridge)
                continue;
        }
        const size_t preferred_anchor = requested == 2
            ? closest_visible_bridge_anchor(
                parity_patch, hole, obstacles, epsilon)
            : hole.size();
        const std::vector<size_t> hole_indices = bridge_anchor_indices(
            hole, requested, preferred_anchor);
        if (hole_indices.size() < 2)
            continue;
        std::vector<size_t> patch_indices;
        if (!choose_ordered_bridges(parity_patch, hole, hole_indices, obstacles,
                                    epsilon, patch_indices)) {
            continue;
        }

        // Every generated patch must have an even number of boundary edges
        // to admit an all-quad triangulation pairing. The two bridge parities
        // of a patch are the only values we may change without inserting or
        // removing nodes on real surface boundaries. Solve the parity cycle
        // once and use the same interval count on both sides of every bridge.
        std::vector<size_t> bridge_intervals(hole_indices.size(), 1);
        std::vector<int> base_parities(hole_indices.size(), 0);
        for (size_t i = 0; i < hole_indices.size(); ++i) {
            const size_t next = (i + 1) % hole_indices.size();
            bridge_intervals[i] = bridge_interval_count(
                parity_patch[patch_indices[i]], hole[hole_indices[i]],
                bridge_step, epsilon);
            const size_t patch_arc_edges =
                (patch_indices[next] + parity_patch.size() - patch_indices[i])
                % parity_patch.size();
            const size_t reverse_hole_arc_edges =
                (hole_indices[next] + hole.size() - hole_indices[i])
                % hole.size();
            base_parities[i] = static_cast<int>(
                (patch_arc_edges + reverse_hole_arc_edges) & 1U);
        }
        std::vector<int> best_parities;
        size_t best_changes = std::numeric_limits<size_t>::max();
        for (int first_parity = 0; first_parity <= 1; ++first_parity) {
            std::vector<int> parities(hole_indices.size(), 0);
            parities[0] = first_parity;
            bool consistent = true;
            for (size_t i = 0; i < hole_indices.size(); ++i) {
                const size_t next = (i + 1) % hole_indices.size();
                const int next_parity = parities[i] ^ base_parities[i];
                if (next == 0) {
                    consistent = next_parity == first_parity;
                } else {
                    parities[next] = next_parity;
                }
                if (!consistent)
                    break;
            }
            if (!consistent)
                continue;
            size_t changes = 0;
            for (size_t i = 0; i < parities.size(); ++i) {
                if (static_cast<int>(bridge_intervals[i] & 1U)
                    != parities[i]) {
                    ++changes;
                }
            }
            if (changes < best_changes) {
                best_changes = changes;
                best_parities = std::move(parities);
            }
        }
        if (best_parities.empty())
            continue;
        for (size_t i = 0; i < bridge_intervals.size(); ++i) {
            if (static_cast<int>(bridge_intervals[i] & 1U)
                != best_parities[i]) {
                ++bridge_intervals[i];
            }
        }

        std::vector<std::vector<SurfacePatchPoint>> candidate_patches;
        bool valid = true;
        for (size_t i = 0; i < hole_indices.size(); ++i) {
            const size_t next = (i + 1) % hole_indices.size();
            std::vector<SurfacePatchPoint> candidate;
            append_forward_arc(candidate, parity_patch,
                               patch_indices[i], patch_indices[next], epsilon);
            append_bridge(candidate, hole[hole_indices[next]],
                          bridge_intervals[next], epsilon);
            append_reverse_arc(candidate, hole, hole_indices[next], hole_indices[i], epsilon);
            append_bridge(candidate, parity_patch[patch_indices[i]],
                          bridge_intervals[i], epsilon);
            normalize_polygon(candidate, epsilon);
            if (candidate.size() < 3
                || std::fabs(signed_area(candidate)) <= epsilon * epsilon
                || polygon_self_intersects(candidate, epsilon)) {
                valid = false;
                break;
            }
            if (signed_area(candidate) < 0.0)
                std::reverse(candidate.begin(), candidate.end());
            candidate_patches.push_back(std::move(candidate));
        }
        if (valid) {
            result = std::move(candidate_patches);
            return true;
        }
    }
    return false;
}

SurfacePatchPoint interior_point(const std::vector<SurfacePatchPoint>& polygon)
{
    SurfacePatchPoint result{};
    for (SurfacePatchPoint point : polygon) {
        result.u += point.u;
        result.v += point.v;
    }
    if (!polygon.empty()) {
        result.u /= static_cast<double>(polygon.size());
        result.v /= static_cast<double>(polygon.size());
    }
    const double scale = geometry_scale({polygon});
    const double epsilon = scale * kRelativeEpsilon;
    if (point_in_polygon(polygon, result, epsilon, false))
        return result;

    // The vertex average of a concave loop may lie outside the loop.  A point
    // just inside any non-degenerate edge is a stable representative for
    // deciding which current patch owns this hole.
    const double orientation_sign = signed_area(polygon) >= 0.0 ? 1.0 : -1.0;
    for (double relative_offset : {1.0e-7, 1.0e-5, 1.0e-3}) {
        for (size_t i = 0; i < polygon.size(); ++i) {
            const SurfacePatchPoint& a = polygon[i];
            const SurfacePatchPoint& b = polygon[(i + 1) % polygon.size()];
            const double du = b.u - a.u;
            const double dv = b.v - a.v;
            const double length = std::hypot(du, dv);
            if (length <= epsilon)
                continue;
            const double offset = scale * relative_offset;
            const SurfacePatchPoint candidate{
                (a.u + b.u) * 0.5 - orientation_sign * dv / length * offset,
                (a.v + b.v) * 0.5 + orientation_sign * du / length * offset};
            if (point_in_polygon(polygon, candidate, epsilon, false))
                return candidate;
        }
    }
    return polygon.empty() ? result : polygon.front();
}

bool split_all_holes(
    std::vector<std::vector<SurfacePatchPoint>>& patches,
    const std::vector<std::vector<SurfacePatchPoint>>& holes,
    const std::vector<size_t>& remaining,
    double bridge_step,
    double epsilon,
    size_t& attempts)
{
    if (remaining.empty())
        return true;
    // Prevent pathological imported contours from causing unbounded
    // combinatorial search. Ordinary CAD faces have only a few holes.
    constexpr size_t max_attempts = 4096;
    if (attempts >= max_attempts)
        return false;

    for (size_t remaining_position = 0;
         remaining_position < remaining.size(); ++remaining_position) {
        const size_t hole_index = remaining[remaining_position];
        const SurfacePatchPoint hole_center = interior_point(holes[hole_index]);
        for (size_t patch_index = 0; patch_index < patches.size(); ++patch_index) {
            if (!point_in_polygon(patches[patch_index], hole_center, epsilon))
                continue;
            ++attempts;

            std::vector<std::vector<SurfacePatchPoint>> obstacles;
            for (size_t other_index : remaining) {
                if (other_index == hole_index)
                    continue;
                if (point_in_polygon(patches[patch_index],
                                     interior_point(holes[other_index]), epsilon)) {
                    obstacles.push_back(holes[other_index]);
                }
            }

            std::vector<std::vector<SurfacePatchPoint>> split;
            if (!split_patch(patches[patch_index], holes[hole_index],
                             obstacles, bridge_step, epsilon, split)) {
                continue;
            }

            std::vector<std::vector<SurfacePatchPoint>> next_patches = patches;
            next_patches.erase(next_patches.begin()
                + static_cast<std::ptrdiff_t>(patch_index));
            next_patches.insert(next_patches.end(), split.begin(), split.end());
            std::vector<size_t> next_remaining = remaining;
            next_remaining.erase(next_remaining.begin()
                + static_cast<std::ptrdiff_t>(remaining_position));
            if (split_all_holes(next_patches, holes, next_remaining,
                                bridge_step, epsilon, attempts)) {
                patches = std::move(next_patches);
                return true;
            }
        }
    }
    return false;
}

} // namespace

bool BuildSurfacePatchesWithoutHoles(
    const std::vector<std::vector<SurfacePatchPoint>>& contours,
    std::vector<std::vector<SurfacePatchPoint>>& patches,
    std::string* error)
{
    patches.clear();
    if (error)
        error->clear();
    const double scale = geometry_scale(contours);
    const double epsilon = scale * kRelativeEpsilon;

    std::vector<std::vector<SurfacePatchPoint>> loops = contours;
    loops.erase(std::remove_if(loops.begin(), loops.end(), [&](auto& loop) {
        normalize_polygon(loop, epsilon);
        return loop.size() < 3 || std::fabs(signed_area(loop)) <= epsilon * epsilon;
    }), loops.end());
    if (loops.empty()) {
        if (error)
            *error = "No valid UV contours.";
        return false;
    }
    const auto outer_it = std::max_element(loops.begin(), loops.end(),
        [](const auto& a, const auto& b) {
            return std::fabs(signed_area(a)) < std::fabs(signed_area(b));
        });
    std::vector<SurfacePatchPoint> outer = std::move(*outer_it);
    loops.erase(outer_it);
    if (signed_area(outer) < 0.0)
        std::reverse(outer.begin(), outer.end());

    std::vector<std::vector<SurfacePatchPoint>> holes;
    for (auto& loop : loops) {
        if (!point_in_polygon(outer, interior_point(loop), epsilon))
            continue;
        if (signed_area(loop) < 0.0)
            std::reverse(loop.begin(), loop.end());
        holes.push_back(std::move(loop));
    }
    if (holes.empty()) {
        patches.push_back(std::move(outer));
        return true;
    }

    std::sort(holes.begin(), holes.end(), [](const auto& a, const auto& b) {
        return std::fabs(signed_area(a)) > std::fabs(signed_area(b));
    });
    patches.push_back(std::move(outer));

    double perimeter = 0.0;
    size_t edge_count = 0;
    for (const auto& contour : contours) {
        for (size_t i = 0; i < contour.size(); ++i) {
            perimeter += std::sqrt(distance_sq(
                contour[i], contour[(i + 1) % contour.size()]));
            ++edge_count;
        }
    }
    const double bridge_step = edge_count > 0
        ? perimeter / static_cast<double>(edge_count) : scale;

    std::vector<size_t> remaining(holes.size());
    std::iota(remaining.begin(), remaining.end(), size_t{0});
    size_t attempts = 0;
    if (!split_all_holes(patches, holes, remaining,
                         bridge_step, epsilon, attempts)) {
        if (error)
            *error = "Could not construct non-intersecting bridges for all holes.";
        patches.clear();
        return false;
    }
    return !patches.empty();
}
