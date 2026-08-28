#include "../src/solid/BeamShapeBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
constexpr double kPi = 3.14159265358979323846;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

double Volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
}

int main() {
    constexpr double width = 20.0;
    constexpr double height = 30.0;
    constexpr double length = 100.0;
    constexpr double thickness = 2.0;

    const std::array<double, 6> expected_areas = {
        thickness * (width + height - thickness),
        width * height - (width - 2.0 * thickness) * (height - 2.0 * thickness),
        2.0 * width * thickness + (height - 2.0 * thickness) * thickness,
        width * thickness + (height - thickness) * thickness,
        2.0 * width * thickness + (height - 2.0 * thickness) * thickness,
        kPi * (100.0 - 64.0)
    };

    for (int index = 0; index < 6; ++index) {
        TopoDS_Shape shape;
        Require(BuildBeamShape(
                    static_cast<BeamSectionType>(index),
                    width,
                    height,
                    length,
                    thickness,
                    shape),
                "Beam profile could not be built.");
        Require(!shape.IsNull(), "Beam profile returned a null shape.");
        Require(shape.ShapeType() == TopAbs_SOLID, "Beam profile is not a solid.");
        Require(BRepCheck_Analyzer(shape).IsValid(), "Beam profile is invalid.");

        const double expected_volume = expected_areas[static_cast<size_t>(index)] * length;
        Require(std::fabs(Volume(shape) - expected_volume) <= expected_volume * 1.0e-8,
                "Beam profile volume is incorrect.");
    }

    TopoDS_Shape invalid_shape;
    Require(!BuildBeamShape(
                BeamSectionType::Square, 20.0, 20.0, 100.0, 10.0, invalid_shape),
            "A hollow profile with no opening must be rejected.");
    Require(!BuildBeamShape(
                BeamSectionType::Tube, 20.0, 20.0, 100.0, 10.0, invalid_shape),
            "A tube with no opening must be rejected.");

    return 0;
}
