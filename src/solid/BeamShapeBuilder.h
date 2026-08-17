#pragma once

#include <TopoDS_Shape.hxx>

enum class BeamSectionType {
    LBeam = 0,
    Square = 1,
    Channel = 2,
    TBeam = 3,
    IBeam = 4,
    Tube = 5
};

bool BuildBeamShape(BeamSectionType type,
                    double width,
                    double height,
                    double length,
                    double thickness,
                    TopoDS_Shape& result);
