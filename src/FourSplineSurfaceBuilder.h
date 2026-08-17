#pragma once

#include "TwoRailSweepSurfaceBuilder.h"

class TopoDS_Shape;

TopoDS_Shape BuildFourSplineSurfaceShape(
    const SweepCurveSamples& first,
    const SweepCurveSamples& second,
    const SweepCurveSamples& third,
    const SweepCurveSamples& fourth);
