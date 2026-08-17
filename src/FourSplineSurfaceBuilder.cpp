#include "FourSplineSurfaceBuilder.h"

#ifdef Coord
#undef Coord
#endif
#ifdef String
#undef String
#endif
#ifdef LPCTSTR
#undef LPCTSTR
#endif
#ifdef Pixel
#undef Pixel
#endif
#ifdef XtPointer
#undef XtPointer
#endif

#include <BRepBuilderAPI_MakeFace.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomFill_BSplineCurves.hxx>
#include <GeomFill_FillingStyle.hxx>
#include <GeomAbs_Shape.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Approx_ParametrizationType.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {
constexpr double kPointTolerance = 1.0e-8;

double squared_distance(const CPoint3d& first, const CPoint3d& second)
{
    const double dx = first.x - second.x;
    const double dy = first.y - second.y;
    const double dz = first.z - second.z;
    return dx * dx + dy * dy + dz * dz;
}

std::vector<CPoint3d> clean_open_points(const SweepCurveSamples& curve)
{
    if (curve.closed) {
        return {};
    }
    std::vector<CPoint3d> result;
    result.reserve(curve.points.size());
    for (const CPoint3d& point : curve.points) {
        if (result.empty()
            || squared_distance(result.back(), point)
                > kPointTolerance * kPointTolerance) {
            result.push_back(point);
        }
    }
    if (result.size() >= 5) {
        return result;
    }

    std::vector<double> lengths(result.size(), 0.0);
    for (size_t index = 1; index < result.size(); ++index) {
        lengths[index] = lengths[index - 1]
            + std::sqrt(squared_distance(result[index - 1], result[index]));
    }
    if (lengths.empty() || lengths.back() <= kPointTolerance) {
        return {};
    }

    std::vector<CPoint3d> densified;
    constexpr int sample_count = 5;
    densified.reserve(sample_count);
    for (int sample = 0; sample < sample_count; ++sample) {
        const double target = lengths.back() * static_cast<double>(sample)
            / static_cast<double>(sample_count - 1);
        size_t segment = 1;
        while (segment + 1 < lengths.size() && lengths[segment] < target) {
            ++segment;
        }
        const double segment_length = lengths[segment] - lengths[segment - 1];
        const double alpha = segment_length > kPointTolerance
            ? (target - lengths[segment - 1]) / segment_length : 0.0;
        const CPoint3d& a = result[segment - 1];
        const CPoint3d& b = result[segment];
        densified.emplace_back(
            a.x + (b.x - a.x) * alpha,
            a.y + (b.y - a.y) * alpha,
            a.z + (b.z - a.z) * alpha);
    }
    return densified;
}

const CPoint3d& oriented_start(const std::vector<CPoint3d>& points,
                               bool reversed)
{
    return reversed ? points.back() : points.front();
}

const CPoint3d& oriented_end(const std::vector<CPoint3d>& points,
                             bool reversed)
{
    return reversed ? points.front() : points.back();
}

Handle(Geom_BSplineCurve) approximate_curve(const std::vector<CPoint3d>& points,
                                            double tolerance)
{
    if (points.size() < 2) {
        return {};
    }
    TColgp_Array1OfPnt array(1, static_cast<int>(points.size()));
    for (size_t index = 0; index < points.size(); ++index) {
        array.SetValue(static_cast<int>(index + 1),
            gp_Pnt(points[index].x, points[index].y, points[index].z));
    }
    GeomAPI_PointsToBSpline approximation(
        array, Approx_ChordLength, 3, 3, GeomAbs_C2, tolerance);
    if (!approximation.IsDone()) {
        return {};
    }
    Handle(Geom_BSplineCurve) curve = approximation.Curve();
    if (curve.IsNull()) {
        return {};
    }
    // The four curves were already joined at their averaged corners.  Keep
    // these endpoints exact after approximation so GeomFill receives a
    // strictly contiguous boundary loop.
    curve->SetPole(1, array.First());
    curve->SetPole(curve->NbPoles(), array.Last());
    return curve;
}
}

TopoDS_Shape BuildFourSplineSurfaceShape(
    const SweepCurveSamples& first,
    const SweepCurveSamples& second,
    const SweepCurveSamples& third,
    const SweepCurveSamples& fourth)
{
    try {
        const std::array<SweepCurveSamples, 4> source{
            first, second, third, fourth};
        std::array<std::vector<CPoint3d>, 4> points;
        CPoint3d minimum{};
        CPoint3d maximum{};
        bool have_bounds = false;
        for (size_t index = 0; index < source.size(); ++index) {
            points[index] = clean_open_points(source[index]);
            if (points[index].size() < 2) {
                return {};
            }
            for (const CPoint3d& point : points[index]) {
                if (!have_bounds) {
                    minimum = maximum = point;
                    have_bounds = true;
                } else {
                    minimum.x = std::min(minimum.x, point.x);
                    minimum.y = std::min(minimum.y, point.y);
                    minimum.z = std::min(minimum.z, point.z);
                    maximum.x = std::max(maximum.x, point.x);
                    maximum.y = std::max(maximum.y, point.y);
                    maximum.z = std::max(maximum.z, point.z);
                }
            }
        }

        std::array<int, 4> best_order{0, 1, 2, 3};
        std::array<bool, 4> best_reverse{};
        double best_score = std::numeric_limits<double>::max();
        double best_maximum_gap = best_score;
        std::array<int, 4> order{0, 1, 2, 3};
        do {
            for (unsigned mask = 0; mask < 16; ++mask) {
                double score = 0.0;
                double maximum_gap = 0.0;
                for (size_t edge = 0; edge < order.size(); ++edge) {
                    const size_t next = (edge + 1) % order.size();
                    const bool reverse_edge = (mask & (1u << edge)) != 0;
                    const bool reverse_next = (mask & (1u << next)) != 0;
                    const double gap = squared_distance(
                        oriented_end(points[static_cast<size_t>(order[edge])], reverse_edge),
                        oriented_start(points[static_cast<size_t>(order[next])], reverse_next));
                    score += gap;
                    maximum_gap = std::max(maximum_gap, gap);
                }
                if (score < best_score) {
                    best_score = score;
                    best_maximum_gap = maximum_gap;
                    best_order = order;
                    for (size_t edge = 0; edge < best_reverse.size(); ++edge) {
                        best_reverse[edge] = (mask & (1u << edge)) != 0;
                    }
                }
            }
        } while (std::next_permutation(order.begin(), order.end()));

        const double dx = maximum.x - minimum.x;
        const double dy = maximum.y - minimum.y;
        const double dz = maximum.z - minimum.z;
        const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double join_tolerance = std::max(1.0e-5, diagonal * 0.01);
        // Imported IGES curves are currently represented by 97 sampled
        // float points.  Fitting them more tightly than about 0.02% of the
        // patch size merely reproduces sampling noise and explodes the
        // compatible Coons knot vectors (for example, 192 x 192).
        const double fit_tolerance = std::max(1.0e-3, diagonal * 2.0e-4);
        if (std::sqrt(best_maximum_gap) > join_tolerance) {
            return {};
        }

        std::array<std::vector<CPoint3d>, 4> boundary;
        for (size_t edge = 0; edge < boundary.size(); ++edge) {
            boundary[edge] = points[static_cast<size_t>(best_order[edge])];
            if (best_reverse[edge]) {
                std::reverse(boundary[edge].begin(), boundary[edge].end());
            }
        }

        // GeomFill requires strictly contiguous curves. Average each pair of
        // already-near endpoints so all four corners are exactly shared.
        for (size_t edge = 0; edge < boundary.size(); ++edge) {
            const size_t next = (edge + 1) % boundary.size();
            const CPoint3d& end = boundary[edge].back();
            const CPoint3d& start = boundary[next].front();
            const CPoint3d corner(
                (end.x + start.x) * 0.5,
                (end.y + start.y) * 0.5,
                (end.z + start.z) * 0.5);
            boundary[edge].back() = corner;
            boundary[next].front() = corner;
        }

        std::array<Handle(Geom_BSplineCurve), 4> curves;
        for (size_t edge = 0; edge < curves.size(); ++edge) {
            curves[edge] = approximate_curve(boundary[edge], fit_tolerance);
            if (curves[edge].IsNull()) {
                return {};
            }
        }

        GeomFill_BSplineCurves fill(
            curves[0], curves[1], curves[2], curves[3], GeomFill_CoonsStyle);
        const Handle(Geom_BSplineSurface) surface = fill.Surface();
        if (surface.IsNull()) {
            return {};
        }
        BRepBuilderAPI_MakeFace face(surface, 1.0e-7);
        return face.IsDone() ? TopoDS_Shape(face.Face()) : TopoDS_Shape{};
    } catch (const Standard_Failure&) {
        return {};
    }
}
