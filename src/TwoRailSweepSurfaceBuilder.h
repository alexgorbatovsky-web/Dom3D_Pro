#pragma once

#include "Point3d.h"

#include <vector>

class TopoDS_Shape;
class TopoDS_Wire;

struct SweepCurveSamples {
    std::vector<CPoint3d> points;
    bool closed = false;
};

TopoDS_Shape BuildTwoRailSweepSurfaceShape(const SweepCurveSamples& profile,
                                           const SweepCurveSamples& first_rail,
                                           const SweepCurveSamples& second_rail);

TopoDS_Shape BuildTwoRailSweepSolidShape(const SweepCurveSamples& profile,
                                         const SweepCurveSamples& first_rail,
                                         const SweepCurveSamples& second_rail);
TopoDS_Shape BuildTwoRailSweepSolidShape(const SweepCurveSamples& profile,
                                         const SweepCurveSamples& first_rail,
                                         const SweepCurveSamples& second_rail,
                                         const TopoDS_Wire& exact_profile_wire);
