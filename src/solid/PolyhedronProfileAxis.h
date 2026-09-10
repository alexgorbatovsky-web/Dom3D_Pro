#pragma once

#include "../SmartLine.h"
#include <algorithm>
#include <cmath>

// A rotational profile must lie in a meridian half-plane. Sewing alone
// can accept overlapping shells when the profile crosses the axis.
inline bool IsPolyhedronProfileAxisValid(const CSmartLine& profile, Vec3 axis) {
    const auto& system = profile.GetCoordinateSystem();
    const auto& n = system.normal;
    const double normalLength = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
    if (normalLength < 1e-12 || std::abs(n.x*axis.x + n.y*axis.y + n.z*axis.z) > normalLength*1e-5)
        return false;
    // The world axis must be contained in the sketch plane, not merely parallel to it.
    const double planeOffset = system.origin.x*n.x + system.origin.y*n.y + system.origin.z*n.z;
    if (std::abs(planeOffset) > normalLength*1e-4) return false;
    const double rx = (axis.y*n.z - axis.z*n.y) / normalLength;
    const double ry = (axis.z*n.x - axis.x*n.z) / normalLength;
    const double rz = (axis.x*n.y - axis.y*n.x) / normalLength;
    double low = 0.0, high = 0.0;
    for (const auto& p : profile.GetProfilePointsWorld()) {
        const double radius = p.x*rx + p.y*ry + p.z*rz;
        low = std::min(low, radius);
        high = std::max(high, radius);
    }
    const double tolerance = std::max(1e-5, std::max(-low, high)*1e-7);
    return !(low < -tolerance && high > tolerance)
        && std::max(-low, high) > tolerance;
}
