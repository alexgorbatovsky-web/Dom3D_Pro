#include "solid/FacadeFrameShapeBuilder.h"

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int SolidCount(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID);
         explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}
}

int main() {
    constexpr double x = -220.0;
    constexpr double y = -18.0;
    constexpr double z = 12.0;
    constexpr double width = 440.0;
    constexpr double thickness = 18.0;
    constexpr double height = 720.0;
    constexpr double frame_width = 60.0;

    const TopoDS_Shape frame = BuildFastFacadeFrameShape(
        x, y, z, width, thickness, height, frame_width);
    Require(!frame.IsNull(), "Fast facade frame is null.");
    Require(BRepCheck_Analyzer(frame).IsValid(),
            "Fast facade frame is invalid.");
    Require(SolidCount(frame) == 4,
            "Fast facade frame must contain four rails.");

    GProp_GProps properties;
    BRepGProp::VolumeProperties(frame, properties);
    Require(std::fabs(properties.Mass()) > 1.0,
            "Fast facade frame has no volume.");

    Bnd_Box bounds;
    BRepBndLib::Add(frame, bounds);
    double x_min = 0.0;
    double y_min = 0.0;
    double z_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;
    bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
    Require(std::fabs(x_min - x) < 1.0e-5
            && std::fabs(x_max - (x + width)) < 1.0e-5
            && std::fabs(z_min - z) < 1.0e-5
            && std::fabs(z_max - (z + height)) < 1.0e-5,
            "Fast facade frame bounds are incorrect.");

    const auto started = std::chrono::steady_clock::now();
    std::vector<TopoDS_Shape> candidates;
    candidates.reserve(18);
    for (int index = 0; index < 18; ++index) {
        candidates.push_back(BuildFastFacadeFrameShape(
            0.0, -thickness, 0.0,
            360.0 + index * 3.0,
            thickness,
            620.0 + index * 5.0,
            frame_width));
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    for (const TopoDS_Shape& candidate : candidates) {
        Require(!candidate.IsNull() && BRepCheck_Analyzer(candidate).IsValid(),
                "Batch facade frame is invalid.");
    }
    std::cout << "18 fast facade frames: " << elapsed.count() << " ms\n";
    Require(elapsed < std::chrono::seconds(5),
            "Fast facade frame batch exceeded performance budget.");
    return 0;
}
