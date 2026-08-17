#include "Fillet.h"

#include "LinkLine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kPiValue = 3.14159265358979323846;
constexpr double kFilletEpsilon = 1.0e-9;

double length_2d(double x, double y) {
    return std::sqrt(x * x + y * y);
}

CPoint3d normalized_tangent(CPoint3d tangent) {
    const double length = length_2d(tangent.x, tangent.y);
    if (length <= kFilletEpsilon) {
        return {};
    }
    tangent.x /= length;
    tangent.y /= length;
    tangent.z = 0.0;
    return tangent;
}

CPoint3d circle_center_on_curve(const CLinkLine& line,
                                double parameter,
                                double radius,
                                double normal_sign) {
    const CPoint3d point = line.GetPoint(parameter);
    const CPoint3d tangent = normalized_tangent(line.GetTangent(parameter));
    return {
        point.x - tangent.y * radius * normal_sign,
        point.y + tangent.x * radius * normal_sign,
        point.z
    };
}

double point_distance_2d(const CPoint3d& first, const CPoint3d& second) {
    return length_2d(first.x - second.x, first.y - second.y);
}

FilletGeometry calculate_curved_fillet(const CLinkLine& first,
                                       const CLinkLine& second,
                                       double radius) {
    FilletGeometry best;
    const CPoint3d corner = first.GetEnd();
    if (point_distance_2d(corner, second.GetStart()) > 1.0e-6) {
        return best;
    }

    CPoint3d first_forward = normalized_tangent(first.GetTangent(1.0));
    CPoint3d second_forward = normalized_tangent(second.GetTangent(0.0));
    if (length_2d(first_forward.x, first_forward.y) <= kFilletEpsilon
        || length_2d(second_forward.x, second_forward.y) <= kFilletEpsilon) {
        return best;
    }
    const CPoint3d first_away(-first_forward.x, -first_forward.y, 0.0);
    const CPoint3d second_away(second_forward.x, second_forward.y, 0.0);
    const double cosine = std::clamp(
        first_away.x * second_away.x + first_away.y * second_away.y,
        -1.0,
        1.0);
    const double angle = std::acos(cosine);
    if (angle <= 1.0e-6 || std::abs(kPiValue - angle) <= 1.0e-6) {
        return best;
    }

    const double tangent_distance = radius / std::tan(angle * 0.5);
    const double first_length = first.GetLength();
    const double second_length = second.GetLength();
    if (tangent_distance <= kFilletEpsilon
        || tangent_distance >= first_length
        || tangent_distance >= second_length) {
        return best;
    }
    const double bisector_x = first_away.x + second_away.x;
    const double bisector_y = first_away.y + second_away.y;
    const double bisector_length = length_2d(bisector_x, bisector_y);
    if (bisector_length <= kFilletEpsilon) {
        return best;
    }
    const double center_distance = radius / std::sin(angle * 0.5);
    const CPoint3d expected_center(
        corner.x + bisector_x / bisector_length * center_distance,
        corner.y + bisector_y / bisector_length * center_distance,
        corner.z);

    const double initial_first = std::clamp(
        1.0 - tangent_distance / first_length,
        1.0e-5,
        1.0 - 1.0e-5);
    const double initial_second = std::clamp(
        tangent_distance / second_length,
        1.0e-5,
        1.0 - 1.0e-5);
    double best_score = std::numeric_limits<double>::max();

    for (double first_sign : {-1.0, 1.0}) {
        for (double second_sign : {-1.0, 1.0}) {
            double first_parameter = initial_first;
            double second_parameter = initial_second;
            bool solved = false;
            for (int iteration = 0; iteration < 30; ++iteration) {
                const CPoint3d first_center =
                    circle_center_on_curve(first, first_parameter, radius, first_sign);
                const CPoint3d second_center =
                    circle_center_on_curve(second, second_parameter, radius, second_sign);
                const double fx = first_center.x - second_center.x;
                const double fy = first_center.y - second_center.y;
                if (length_2d(fx, fy) <= std::max(1.0e-7, radius * 1.0e-7)) {
                    solved = true;
                    break;
                }

                constexpr double step = 1.0e-5;
                const double first_before = std::max(1.0e-6, first_parameter - step);
                const double first_after = std::min(1.0 - 1.0e-6, first_parameter + step);
                const double second_before = std::max(1.0e-6, second_parameter - step);
                const double second_after = std::min(1.0 - 1.0e-6, second_parameter + step);
                const CPoint3d first_center_before =
                    circle_center_on_curve(first, first_before, radius, first_sign);
                const CPoint3d first_center_after =
                    circle_center_on_curve(first, first_after, radius, first_sign);
                const CPoint3d second_center_before =
                    circle_center_on_curve(second, second_before, radius, second_sign);
                const CPoint3d second_center_after =
                    circle_center_on_curve(second, second_after, radius, second_sign);
                const double first_span = first_after - first_before;
                const double second_span = second_after - second_before;
                const double j00 = (first_center_after.x - first_center_before.x) / first_span;
                const double j10 = (first_center_after.y - first_center_before.y) / first_span;
                const double j01 = -(second_center_after.x - second_center_before.x) / second_span;
                const double j11 = -(second_center_after.y - second_center_before.y) / second_span;
                const double determinant = j00 * j11 - j01 * j10;
                if (std::abs(determinant) <= 1.0e-12) {
                    break;
                }
                const double first_delta = (-fx * j11 + j01 * fy) / determinant;
                const double second_delta = (-j00 * fy + fx * j10) / determinant;
                first_parameter = std::clamp(
                    first_parameter + first_delta,
                    1.0e-6,
                    1.0 - 1.0e-6);
                second_parameter = std::clamp(
                    second_parameter + second_delta,
                    1.0e-6,
                    1.0 - 1.0e-6);
            }
            if (!solved) {
                continue;
            }

            const CPoint3d tangent_on_first = first.GetPoint(first_parameter);
            const CPoint3d tangent_on_second = second.GetPoint(second_parameter);
            const CPoint3d first_center =
                circle_center_on_curve(first, first_parameter, radius, first_sign);
            const CPoint3d second_center =
                circle_center_on_curve(second, second_parameter, radius, second_sign);
            const CPoint3d center(
                (first_center.x + second_center.x) * 0.5,
                (first_center.y + second_center.y) * 0.5,
                corner.z);
            if (point_distance_2d(first_center, second_center)
                > std::max(1.0e-5, radius * 1.0e-5)) {
                continue;
            }

            const double start_x = tangent_on_first.x - center.x;
            const double start_y = tangent_on_first.y - center.y;
            const double end_x = tangent_on_second.x - center.x;
            const double end_y = tangent_on_second.y - center.y;
            const double raw_angle = std::atan2(
                start_x * end_y - start_y * end_x,
                start_x * end_x + start_y * end_y);
            const double angle_options[] = {
                raw_angle,
                raw_angle > 0.0 ? raw_angle - 2.0 * kPiValue : raw_angle + 2.0 * kPiValue
            };
            for (double signed_angle : angle_options) {
                if (std::abs(signed_angle) <= 1.0e-6
                    || std::abs(signed_angle) >= kPiValue + 1.0e-5) {
                    continue;
                }
                const double direction_sign = signed_angle > 0.0 ? 1.0 : -1.0;
                const CPoint3d arc_start_tangent = normalized_tangent(CPoint3d(
                    -start_y * direction_sign,
                    start_x * direction_sign,
                    0.0));
                const CPoint3d arc_end_tangent = normalized_tangent(CPoint3d(
                    -end_y * direction_sign,
                    end_x * direction_sign,
                    0.0));
                const CPoint3d first_curve_tangent =
                    normalized_tangent(first.GetTangent(first_parameter));
                const CPoint3d second_curve_tangent =
                    normalized_tangent(second.GetTangent(second_parameter));
                const double start_alignment =
                    arc_start_tangent.x * first_curve_tangent.x
                    + arc_start_tangent.y * first_curve_tangent.y;
                const double end_alignment =
                    arc_end_tangent.x * second_curve_tangent.x
                    + arc_end_tangent.y * second_curve_tangent.y;
                if (start_alignment < 0.8 || end_alignment < 0.8) {
                    continue;
                }

                const double score =
                    point_distance_2d(center, expected_center)
                    + radius * ((1.0 - first_parameter) + second_parameter) * 0.01;
                if (score < best_score) {
                    best_score = score;
                    best.tangent_on_first = tangent_on_first;
                    best.tangent_on_second = tangent_on_second;
                    best.center = center;
                    best.first_parameter = first_parameter;
                    best.second_parameter = second_parameter;
                    best.signed_angle = signed_angle;
                    best.valid = true;
                }
            }
        }
    }
    return best;
}
}

CFillet::CFillet(std::size_t first_line_index, double radius)
    : first_line_index_(first_line_index),
      second_line_index_(first_line_index + 1),
      radius_(radius) {
}

CFillet::CFillet(std::size_t first_line_index, std::size_t second_line_index, double radius)
    : first_line_index_(first_line_index),
      second_line_index_(second_line_index),
      radius_(radius) {
}

std::size_t CFillet::GetFirstLineIndex() const {
    return first_line_index_;
}

std::size_t CFillet::GetSecondLineIndex() const {
    return second_line_index_;
}

double CFillet::GetRadius() const {
    return radius_;
}

bool CFillet::SetRadius(double radius) {
    if (radius <= kFilletEpsilon) {
        return false;
    }
    radius_ = radius;
    return true;
}

FilletGeometry CFillet::Calculate(const CLinkLine& first, const CLinkLine& second) const {
    FilletGeometry result;
    if (radius_ <= kFilletEpsilon) {
        return result;
    }
    if (first.GetType() == LinkLineType::Bezier
        || second.GetType() == LinkLineType::Bezier
        || first.GetType() == LinkLineType::Arc
        || second.GetType() == LinkLineType::Arc) {
        return calculate_curved_fillet(first, second, radius_);
    }

    const CPoint3d& corner = first.GetEnd();
    const CPoint3d& second_start = second.GetStart();
    const double join_dx = corner.x - second_start.x;
    const double join_dy = corner.y - second_start.y;
    const double join_dz = corner.z - second_start.z;
    if (std::sqrt(join_dx * join_dx + join_dy * join_dy + join_dz * join_dz) > 1.0e-6) {
        return result;
    }

    const double first_x = first.GetStart().x - corner.x;
    const double first_y = first.GetStart().y - corner.y;
    const double second_x = second.GetEnd().x - corner.x;
    const double second_y = second.GetEnd().y - corner.y;
    const double first_length = length_2d(first_x, first_y);
    const double second_length = length_2d(second_x, second_y);
    if (first_length <= kFilletEpsilon || second_length <= kFilletEpsilon) {
        return result;
    }

    const double first_dir_x = first_x / first_length;
    const double first_dir_y = first_y / first_length;
    const double second_dir_x = second_x / second_length;
    const double second_dir_y = second_y / second_length;
    const double cosine = std::clamp(
        first_dir_x * second_dir_x + first_dir_y * second_dir_y,
        -1.0,
        1.0);
    const double angle = std::acos(cosine);
    if (angle <= 1.0e-6 || std::abs(kPiValue - angle) <= 1.0e-6) {
        return result;
    }

    const double tangent_distance = radius_ / std::tan(angle * 0.5);
    if (tangent_distance <= kFilletEpsilon ||
        tangent_distance >= first_length ||
        tangent_distance >= second_length) {
        return result;
    }

    const double bisector_x = first_dir_x + second_dir_x;
    const double bisector_y = first_dir_y + second_dir_y;
    const double bisector_length = length_2d(bisector_x, bisector_y);
    if (bisector_length <= kFilletEpsilon) {
        return result;
    }

    result.tangent_on_first = CPoint3d(
        corner.x + first_dir_x * tangent_distance,
        corner.y + first_dir_y * tangent_distance,
        corner.z);
    result.tangent_on_second = CPoint3d(
        corner.x + second_dir_x * tangent_distance,
        corner.y + second_dir_y * tangent_distance,
        corner.z);
    const double center_distance = radius_ / std::sin(angle * 0.5);
    result.center = CPoint3d(
        corner.x + bisector_x / bisector_length * center_distance,
        corner.y + bisector_y / bisector_length * center_distance,
        corner.z);

    const double start_x = result.tangent_on_first.x - result.center.x;
    const double start_y = result.tangent_on_first.y - result.center.y;
    const double end_x = result.tangent_on_second.x - result.center.x;
    const double end_y = result.tangent_on_second.y - result.center.y;
    result.signed_angle = std::atan2(
        start_x * end_y - start_y * end_x,
        start_x * end_x + start_y * end_y);
    result.valid = std::abs(result.signed_angle) > 1.0e-6;
    return result;
}

std::vector<CPoint3d> CFillet::Sample(const CLinkLine& first,
                                      const CLinkLine& second,
                                      std::size_t minimum_segments) const {
    const FilletGeometry geometry = Calculate(first, second);
    if (!geometry.valid) {
        return {};
    }

    const std::size_t angle_segments = static_cast<std::size_t>(
        std::ceil(std::abs(geometry.signed_angle) / (kPiValue / 18.0)));
    const std::size_t segment_count = std::max(minimum_segments, angle_segments);
    std::vector<CPoint3d> points;
    points.reserve(segment_count + 1);

    const double start_x = geometry.tangent_on_first.x - geometry.center.x;
    const double start_y = geometry.tangent_on_first.y - geometry.center.y;
    for (std::size_t index = 0; index <= segment_count; ++index) {
        const double parameter = static_cast<double>(index) / static_cast<double>(segment_count);
        const double angle = geometry.signed_angle * parameter;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        points.emplace_back(
            geometry.center.x + start_x * cosine - start_y * sine,
            geometry.center.y + start_x * sine + start_y * cosine,
            geometry.center.z);
    }
    return points;
}
