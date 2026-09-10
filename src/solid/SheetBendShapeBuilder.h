#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

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
    // False keeps the legacy right-hand side of the directed line, as seen
    // from outside the supporting face. True bends the opposite side.
    bool reverse_side = false;
};

// Bends a constant-thickness sheet across a directed line. Clockwise is
// evaluated while looking from the line start towards the line end.
bool BuildSheetBendShape(const TopoDS_Shape& source,
                         const SheetBendParameters& parameters,
                         TopoDS_Shape& result,
                         std::string& error_message);

// Shared geometry for construction, picking a moving side, and the direction guide.
struct SheetBendFrame {
    gp_Pnt origin;
    gp_Dir axis, normal;
    double thickness = 0, line_length = 0, diagonal = 0;
};
bool ResolveSheetBendFrame(const TopoDS_Shape& source,
                          const SheetBendParameters& parameters,
                          SheetBendFrame& frame, std::string& error_message);
