#include "SurfacePatchBuilder.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Point = SurfacePatchPoint;
using Polygon = std::vector<Point>;

double area(const Polygon& polygon)
{
    double result = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const Point& a = polygon[i];
        const Point& b = polygon[(i + 1) % polygon.size()];
        result += a.u * b.v - b.u * a.v;
    }
    return result * 0.5;
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

double patch_area(const std::vector<Polygon>& patches)
{
    double result = 0.0;
    for (const Polygon& patch : patches) {
        require(patch.size() >= 3, "Generated a patch with fewer than three points.");
        require(area(patch) > 0.0, "Generated patch winding is not counter-clockwise.");
        result += area(patch);
    }
    return result;
}

void require_even_patch_boundaries(const std::vector<Polygon>& patches)
{
    for (const Polygon& patch : patches) {
        require((patch.size() & 1U) == 0U,
                "An island has an odd boundary and cannot be fully quadrangulated.");
    }
}

void require_bridge_node_classification(
    const std::vector<Polygon>& contours,
    const std::vector<Polygon>& patches)
{
    constexpr double tolerance_sq = 1.0e-18;
    bool found_bridge_subdivision = false;
    for (const Polygon& patch : patches) {
        for (Point point : patch) {
            if (!point.boundary_node) {
                found_bridge_subdivision = true;
                continue;
            }
            bool found = false;
            for (const Polygon& contour : contours) {
                for (Point boundary_point : contour) {
                    const double du = point.u - boundary_point.u;
                    const double dv = point.v - boundary_point.v;
                    if (du * du + dv * dv <= tolerance_sq) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
            require(found,
                    "A bridge endpoint is not an original boundary node.");
        }
    }
    require(found_bridge_subdivision,
            "Long bridges were not subdivided according to contour density.");
}

void require_all_original_nodes_preserved(
    const std::vector<Polygon>& contours,
    const std::vector<Polygon>& patches)
{
    constexpr double tolerance_sq = 1.0e-18;
    for (const Polygon& contour : contours) {
        for (Point original : contour) {
            bool found = false;
            for (const Polygon& patch : patches) {
                for (Point point : patch) {
                    const double du = point.u - original.u;
                    const double dv = point.v - original.v;
                    if (point.boundary_node
                        && du * du + dv * dv <= tolerance_sq) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
            require(found,
                    "An original boundary node, including a collinear node, was removed.");
        }
    }
}

bool has_bridge_between(const std::vector<Polygon>& patches,
                        Point first, Point second)
{
    constexpr double tolerance_sq = 1.0e-18;
    const auto same_point = [&](Point a, Point b) {
        const double du = a.u - b.u;
        const double dv = a.v - b.v;
        return du * du + dv * dv <= tolerance_sq;
    };
    const auto directed_bridge = [&](const Polygon& patch,
                                     Point start, Point finish) {
        for (size_t i = 0; i < patch.size(); ++i) {
            if (!patch[i].boundary_node || !same_point(patch[i], start))
                continue;
            size_t current = (i + 1) % patch.size();
            while (current != i && !patch[current].boundary_node)
                current = (current + 1) % patch.size();
            if (current != i && same_point(patch[current], finish))
                return true;
        }
        return false;
    };
    for (const Polygon& patch : patches) {
        if (directed_bridge(patch, first, second)
            || directed_bridge(patch, second, first)) {
            return true;
        }
    }
    return false;
}

void test_without_holes()
{
    const Polygon outer{{0, 0}, {10, 0}, {10, 8}, {0, 8}, {0, 0}};
    std::vector<Polygon> patches;
    std::string error;
    require(BuildSurfacePatchesWithoutHoles({outer}, patches, &error), error);
    require(patches.size() == 1, "A contour without holes was unexpectedly split.");
    require(std::fabs(patch_area(patches) - 80.0) < 1.0e-7,
            "Area changed for a contour without holes.");
}

void test_one_hole()
{
    const Polygon outer{{0, 0}, {12, 0}, {12, 10}, {0, 10}, {0, 0}};
    const Polygon hole{{4, 3}, {8, 3}, {8, 7}, {4, 7}, {4, 3}};
    std::vector<Polygon> patches;
    std::string error;
    require(BuildSurfacePatchesWithoutHoles({outer, hole}, patches, &error), error);
    require(patches.size() >= 2, "A hole did not produce independent patches.");
    require(std::fabs(patch_area(patches) - 104.0) < 1.0e-7,
            "Patch areas do not equal outer area minus hole area.");
    require_even_patch_boundaries(patches);
}

void test_multiple_holes()
{
    const Polygon outer{{0, 0}, {20, 0}, {20, 12}, {0, 12}, {0, 0}};
    const Polygon first{{3, 3}, {7, 3}, {7, 8}, {3, 8}, {3, 3}};
    const Polygon second{{13, 2}, {17, 2}, {17, 6}, {13, 6}, {13, 2}};
    std::vector<Polygon> patches;
    std::string error;
    const std::vector<Polygon> contours{outer, first, second};
    require(BuildSurfacePatchesWithoutHoles(contours, patches, &error), error);
    require(patches.size() >= 3, "Multiple holes did not produce independent patches.");
    require(std::fabs(patch_area(patches) - 204.0) < 1.0e-7,
            "Multiple-hole patch areas are incorrect.");
    require_bridge_node_classification(contours, patches);
    require_all_original_nodes_preserved(contours, patches);
    require_even_patch_boundaries(patches);
}

void test_narrow_bridges_between_three_holes()
{
    const Polygon outer{{0, 0}, {100, 0}, {100, 80}, {0, 80}};
    const Polygon first{{20, 10}, {38, 10}, {38, 28}, {20, 28}};
    const Polygon second{{40, 10}, {58, 10}, {58, 28}, {40, 28}};
    const Polygon third{{30, 30}, {48, 30}, {48, 48}, {30, 48}};
    std::vector<Polygon> patches;
    std::string error;
    const std::vector<Polygon> contours{outer, first, second, third};
    const bool built = BuildSurfacePatchesWithoutHoles(contours, patches, &error);
    require(built, "Three close holes: " + error);
    require(patches.size() >= 4,
            "Three close holes did not produce independent islands.");
    require(std::fabs(patch_area(patches) - 7028.0) < 1.0e-7,
            "Narrow bridges between holes changed the filled area.");
    require_bridge_node_classification(contours, patches);
    require_all_original_nodes_preserved(contours, patches);
    require_even_patch_boundaries(patches);
}

void test_collinear_surface_boundary_nodes_are_preserved()
{
    const Polygon outer{{0, 0}, {5, 0}, {10, 0}, {15, 0}, {20, 0},
                        {20, 6}, {20, 12}, {15, 12}, {10, 12}, {5, 12},
                        {0, 12}, {0, 6}};
    const Polygon hole{{7, 4}, {10, 4}, {13, 4}, {13, 8},
                       {10, 8}, {7, 8}};
    const std::vector<Polygon> contours{outer, hole};
    std::vector<Polygon> patches;
    std::string error;
    require(BuildSurfacePatchesWithoutHoles(contours, patches, &error), error);
    require_all_original_nodes_preserved(contours, patches);
}

void test_curved_hole_close_to_outer_boundary()
{
    const Polygon outer{{0, 0}, {60, 0}, {60, 50}, {0, 50}};
    Polygon hole;
    constexpr int segments = 16;
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < segments; ++i) {
        const double angle = 2.0 * pi * i / segments;
        hole.push_back({14.0 + 12.0 * std::cos(angle),
                        25.0 + 12.0 * std::sin(angle)});
    }
    std::vector<Polygon> patches;
    std::string error;
    const bool built = BuildSurfacePatchesWithoutHoles(
        {outer, hole}, patches, &error);
    require(built, "Curved near-boundary hole: " + error);
    require(patches.size() >= 2,
            "A curved hole near the outer boundary did not produce islands.");
    const double expected = std::fabs(area(outer)) - std::fabs(area(hole));
    require(std::fabs(patch_area(patches) - expected) < 1.0e-7,
            "A narrow outer bridge changed the filled area.");
    require_even_patch_boundaries(patches);
}

void test_complex_multi_hole_islands_preserve_geometry()
{
    const Polygon outer{{0, 0}, {100, 0}, {100, 80}, {0, 80}};
    const auto circle = [](double center_u, double center_v,
                           double radius, int segments) {
        Polygon result;
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < segments; ++i) {
            const double angle = 2.0 * pi * i / segments;
            result.push_back({center_u + radius * std::cos(angle),
                              center_v + radius * std::sin(angle)});
        }
        return result;
    };
    const Polygon small_circle = circle(25.0, 55.0, 10.0, 16);
    const Polygon large_circle = circle(68.0, 55.0, 14.0, 20);
    const Polygon rectangular_hole{
        {28, 13}, {78, 13}, {78, 27}, {28, 27}};
    const std::vector<Polygon> contours{
        outer, small_circle, large_circle, rectangular_hole};
    std::vector<Polygon> patches;
    std::string error;
    require(BuildSurfacePatchesWithoutHoles(contours, patches, &error), error);
    const double expected = std::fabs(area(outer))
        - std::fabs(area(small_circle))
        - std::fabs(area(large_circle))
        - std::fabs(area(rectangular_hole));
    require(std::fabs(patch_area(patches) - expected) < 1.0e-7,
            "Complex-island refinement changed the filled area.");
    require_even_patch_boundaries(patches);
    require_all_original_nodes_preserved(contours, patches);
}

void test_narrow_gap_bridge_is_preferred()
{
    const Polygon outer{{0, 0}, {20, 0}, {40, 0}, {60, 0}, {80, 0},
                        {100, 0}, {100, 20}, {100, 40}, {100, 60},
                        {100, 80}, {100, 100}, {80, 100}, {60, 100},
                        {40, 100}, {20, 100}, {0, 100}, {0, 80},
                        {0, 60}, {0, 40}, {0, 20}};
    const auto circle = [](double center_u, double center_v,
                           double radius, int segments) {
        Polygon result;
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < segments; ++i) {
            const double angle = 2.0 * pi * i / segments;
            result.push_back({center_u + radius * std::cos(angle),
                              center_v + radius * std::sin(angle)});
        }
        return result;
    };
    const Polygon small_circle = circle(25.0, 55.0, 9.0, 20);
    const Polygon large_circle = circle(68.0, 55.0, 14.0, 20);
    const Polygon slot{{20, 13}, {40, 13}, {60, 13}, {80, 13},
                       {80, 27}, {68, 27}, {56, 27}, {44, 27},
                       {32, 27}, {20, 27}};
    std::vector<Polygon> patches;
    std::string error;
    require(BuildSurfacePatchesWithoutHoles(
                {outer, small_circle, large_circle, slot}, patches, &error),
            "Narrow-gap bridge: " + error);
    const Point circle_bottom{68.0, 41.0};
    const Point slot_top{68.0, 27.0};
    const bool found_narrow_bridge =
        has_bridge_between(patches, circle_bottom, slot_top);
    require(found_narrow_bridge,
            "The shortest circle-to-slot gap was not selected as a bridge.");
    require_even_patch_boundaries(patches);
}

} // namespace

int main()
{
    test_without_holes();
    test_one_hole();
    test_multiple_holes();
    test_narrow_bridges_between_three_holes();
    test_curved_hole_close_to_outer_boundary();
    test_collinear_surface_boundary_nodes_are_preserved();
    test_complex_multi_hole_islands_preserve_geometry();
    test_narrow_gap_bridge_is_preferred();
    return EXIT_SUCCESS;
}
