#pragma once
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <string>

struct PanelContourResult {
    TopoDS_Shape panel;
    TopoDS_Shape remainder;
};

// Deform a single panel while retaining its boundary and boundary tangents.
// Profile 0 preserves legacy filling; 0.25..0.75 controls shoulder fullness.
bool BuildBulgedPanelFace(const TopoDS_Face& source, double height,
    TopoDS_Shape& result, std::string& error, double profile = 0);

// Positive thickness extends inward from the oriented panel face.
bool BuildThickenedPanel(const TopoDS_Shape& panel, double thickness,
    TopoDS_Shape& result, std::string& error);

// The contour is projected along direction. Gap is measured in that projection
// plane; positive depth offsets the panel opposite the oriented face normal.
bool BuildPanelContourShape(const TopoDS_Shape& body, const TopoDS_Face& face,
    const TopoDS_Wire& contour, const gp_Dir& direction, bool larger_region,
    double gap, double depth, PanelContourResult& result, std::string& error,
    double thickness = 0.0);
