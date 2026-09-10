#pragma once

#include "SweptSolidBuilder.h"
#include "SmartLine.h"
#include "CPolyline.h"
#include "CBSpline.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <gp_Pln.hxx>

inline bool SolidRevolveAxisInProfilePlane(const TopoDS_Shape& profile, Vec3 axis) {
    if (profile.IsNull() || profile.ShapeType() != TopAbs_FACE) return false;
    const BRepAdaptor_Surface surface(TopoDS::Face(profile));
    if (surface.GetType() != GeomAbs_Plane) return false;
    return std::abs(surface.Plane().Axis().Direction().Dot(
        gp_Dir(axis.x, axis.y, axis.z))) < 1.0e-7;
}

// A surface revolves the authored path, never a filled profile or axis caps.
// Shared by preview and history replay so spline edits retain their shape.
inline bool BuildSurfaceRevolveProfile(const CAlfaObject& profile,
                                       TopoDS_Shape& shape, Vec3& origin) {
    TopoDS_Wire wire;
    const auto* spline = dynamic_cast<const CBSpline*>(&profile);
    if (spline && spline->IsClosed()) {
        if (spline->GetPointCount() < 3) return false;
        // Periodic interpolation omits the duplicate endpoint, preserving a
        // single closed seam instead of leaving a gap at the spline ends.
        const int count = 128;
        Handle(TColgp_HArray1OfPnt) points = new TColgp_HArray1OfPnt(1, count);
        for (int i = 0; i < count; ++i) {
            const auto point = spline->Evaluate(static_cast<float>(i) / count);
            points->SetValue(i + 1, gp_Pnt(point.x, point.y, point.z));
        }
        GeomAPI_Interpolate curve(points, true, 1.0e-7);
        curve.Perform();
        if (!curve.IsDone()) return false;
        BRepBuilderAPI_MakeEdge edge(curve.Curve());
        if (!edge.IsDone()) return false;
        BRepBuilderAPI_MakeWire builder(edge.Edge());
        if (!builder.IsDone()) return false;
        wire = builder.Wire();
    } else {
        wire = BuildSolidCenterPath(profile);
    }
    if (wire.IsNull()) return false;
    origin = {};
    if (const auto* sketch = dynamic_cast<const CSmartLine*>(&profile)) {
        const auto point = sketch->GetCoordinateSystem().origin;
        origin = {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
    } else if (const auto* line = dynamic_cast<const CPolyline*>(&profile)) {
        if (!line->GetPoints().empty()) origin.z = line->GetPoints().front().z;
    }
    shape = wire;
    return true;
}
