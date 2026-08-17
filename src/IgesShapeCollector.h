#pragma once

#include <TopoDS_Shape.hxx>

#include <vector>

void CollectIgesSurfaceShapes(
    const TopoDS_Shape& shape,
    std::vector<TopoDS_Shape>& surface_shapes);

float ComputeIgesMeshDeflection(const TopoDS_Shape& shape);
