#pragma once

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <gce_MakeCirc.hxx>
#include <gp_Vec.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>

// Recognize a full circle by its CAD geometry, independently of mesh density.
// Sweep can encode a circular section as a periodic rational B-spline.
inline bool IsCircularSplineBoundary(const TopoDS_Edge& edge)
{
    try {
        const BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_BSplineCurve)
            return false;
        const double first = curve.FirstParameter();
        const double span = curve.LastParameter() - first;
        if (!std::isfinite(first) || !std::isfinite(span) || span <= 0.0)
            return false;
        const gp_Pnt start = curve.Value(first);
        const gce_MakeCirc fit(start, curve.Value(first + span / 3.0),
                              curve.Value(first + span * 2.0 / 3.0));
        if (!fit.IsDone())
            return false;
        const gp_Circ circle = fit.Value();
        const double radius = circle.Radius();
        if (!std::isfinite(radius) || radius <= 1.0e-9)
            return false;
        // Do not allow a loose imported CAD tolerance to turn an arbitrary
        // closed spline into a circle. Check both radial and planar residuals.
        const double tolerance = std::max(radius * 1.0e-5,
            std::min(BRep_Tool::Tolerance(edge), radius * 1.0e-4));
        if (start.Distance(curve.Value(first + span)) > tolerance)
            return false;
        const gp_Vec normal(circle.Axis().Direction());
        gp_Vec previous(circle.Location(), start);
        double winding = 0.0;
        constexpr int samples = 192;
        for (int index = 1; index <= samples; ++index) {
            const gp_Vec radial(circle.Location(),
                curve.Value(first + span * index / samples));
            if (std::fabs(radial.Dot(normal)) > tolerance
                || std::fabs(radial.Magnitude() - radius) > tolerance)
                return false;
            const double angle = std::atan2(
                normal.Dot(previous.Crossed(radial)), previous.Dot(radial));
            // Reject folds, multiple turns and under-resolved parameter jumps.
            if (angle <= 0.0 || angle >= 1.5707963267948966)
                return false;
            winding += angle;
            previous = radial;
        }
        return std::fabs(winding - 6.2831853071795865) < 1.0e-5;
    } catch (const Standard_Failure&) {
        return false;
    }
}
