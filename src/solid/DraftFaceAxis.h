#pragma once

#include "SurfaceFace.h"
#include <BRepAdaptor_Curve.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>

// Draft's signed angle depends on edge direction. Use the CAD curve's
// increasing parameter direction for both live creation and history replay.
// Display splines follow the oriented wire and may run the opposite way.
inline bool DraftFaceEdgeEndpoints(const CSurfaceFace& surface, int index,
                                  Vec3& start, Vec3& end)
{
    const auto* edge = surface.GetTopoEdge(index);
    if (!edge || edge->IsNull()) return surface.GetEdgeEndpoints(index,start,end);
    try {
        BRepAdaptor_Curve curve(*edge);
        const double first = curve.FirstParameter(), last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last) || last <= first) return false;
        const auto a = curve.Value(first), b = curve.Value(last);
        start = {float(a.X()),float(a.Y()),float(a.Z())};
        end = {float(b.X()),float(b.Y()),float(b.Z())};
        const Vec3 chord = end-start;
        const float length = std::sqrt(dot(chord,chord));
        if (length <= .000001f) return false;
        const auto direction = chord * (1.f/length);
        const auto tolerance = std::max(.001f,length*.001f);
        for (int i=1; i<6; ++i) {
            const auto p = curve.Value(first+(last-first)*i/6.);
            const Vec3 point{float(p.X()),float(p.Y()),float(p.Z())};
            const auto closest = start+direction*dot(point-start,direction);
            const auto delta = point-closest;
            if (std::sqrt(dot(delta,delta)) > tolerance) return false;
        }
        return true;
    } catch (const Standard_Failure&) { return false; }
}
