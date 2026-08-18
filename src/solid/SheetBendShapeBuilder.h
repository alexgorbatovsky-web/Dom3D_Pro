#pragma once

#include <TopoDS_Shape.hxx>

#include <string>

struct SheetBendParameters {
    double line_start_x = 0.0;
    double line_start_y = 0.0;
    double line_start_z = 0.0;
    double line_end_x = 0.0;
    double line_end_y = 0.0;
    double line_end_z = 0.0;
    double inner_radius = 2.0;
    double angle_degrees = 90.0;
    bool clockwise = true;
};

// Bends a constant-thickness sheet across a directed line. Clockwise is
// evaluated while looking from the line start towards the line end.
bool BuildSheetBendShape(const TopoDS_Shape& source,
                         const SheetBendParameters& parameters,
                         TopoDS_Shape& result,
                         std::string& error_message);
