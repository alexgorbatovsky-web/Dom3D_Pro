#include "IgesShapeCollector.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <cmath>
void CollectIgesSurfaceShapes(
    const TopoDS_Shape& shape,
    std::vector<TopoDS_Shape>& surface_shapes) {
    const std::size_t initial_count = surface_shapes.size();

    for (TopExp_Explorer explorer(shape, TopAbs_SOLID);
         explorer.More(); explorer.Next()) {
        surface_shapes.push_back(explorer.Current());
    }
    if (surface_shapes.size() != initial_count) {
        return;
    }

    for (TopExp_Explorer explorer(shape, TopAbs_SHELL);
         explorer.More(); explorer.Next()) {
        surface_shapes.push_back(explorer.Current());
    }
    if (surface_shapes.size() != initial_count) {
        return;
    }

    // A compound/compsolid without a shell can contain several independent
    // IGES surface entities. Keep every face as a separate scene object;
    // grouping them here makes unrelated imported surfaces inseparable.
    for (TopExp_Explorer explorer(shape, TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        surface_shapes.push_back(explorer.Current());
    }
}

float ComputeIgesMeshDeflection(const TopoDS_Shape& shape) {
    constexpr double kMinimumDeflection = 0.1;
    constexpr double kMaximumDeflection = 1000.0;
    constexpr double kRelativeDeflection = 0.01;

    if (shape.IsNull()) {
        return static_cast<float>(kMinimumDeflection);
    }

    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (bounds.IsVoid()) {
        return static_cast<float>(kMinimumDeflection);
    }

    Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
    bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const double dx = xmax - xmin;
    const double dy = ymax - ymin;
    const double dz = zmax - zmin;
    const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(diagonal)) {
        return static_cast<float>(kMinimumDeflection);
    }
    return static_cast<float>(std::clamp(
        diagonal * kRelativeDeflection,
        kMinimumDeflection,
        kMaximumDeflection));
}
