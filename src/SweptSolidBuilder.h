#pragma once

#include <string>
#include <vector>

class CAlfaObject;
class CAlfaDoc;
class CSmartLine;
class CPolyline;
class TopoDS_Shape;
class TopoDS_Wire;
struct Vec3;

// Physical cutter sketches use the tool-tip datum at Y=0. The profile may
// extend toward either +Y or -Y; its working copy is oriented so the shank
// remains outside while the tip advances to the requested milling depth.
struct MillingCutterPlacement {
    double delta_y = 0.0;
    double angle_degrees = 0.0;
    bool rotated_legacy_profile = false;
    bool tip_at_cutting_depth = false;
};

MillingCutterPlacement ResolveMillingCutterPlacement(
    const CSmartLine& cutter);

// Validates the catalog authoring contract for newly selected facade cutters.
// Existing project resources continue to use the compatibility placement
// logic above and are therefore not invalidated retroactively.
bool ValidateMillingCutterCatalogProfile(
    const CSmartLine& cutter,
    std::string& error);

// Finds the single usable native cutter sketch in a catalog document.
// Empty placeholder curves/sketches are ignored, while multiple real
// profiles and non-empty legacy polylines remain ambiguous and are rejected.
const CSmartLine* ResolveMillingCutterCatalogProfile(
    const CAlfaDoc& catalog_document,
    std::string& error);

// Restricts a planar facade cutter to the requested depth measured from the
// facade's front (minimum-Y) face. The untrimmed cutter can still be retained
// separately for diagnostics.
TopoDS_Shape LimitMillingCutterToFacadeDepth(
    const TopoDS_Shape& cutter,
    const TopoDS_Shape& facade,
    double cutting_depth);

// Common milling sweep core. Both built-in and catalog Milled facades use
// this exact PipeShell construction; only their section and guide differ.
TopoDS_Shape BuildMillingSweepShape(const TopoDS_Wire& section_wire,
                                    const TopoDS_Wire& guide_wire,
                                    const Vec3& guide_normal,
                                    int transition_mode = 2,
                                    bool require_evolved = false,
                                    bool preserve_section_orientation = false);

TopoDS_Shape BuildSweptSolidShape(const CSmartLine& section,
                                  const CAlfaObject& guide,
                                  int transition_mode,
                                  double delta_x = 0.0,
                                  double delta_y = 0.0,
                                  double angle_degrees = 0.0,
                                  const std::vector<double>& width_scales = {},
                                  const std::vector<double>& height_scales = {},
                                  bool require_evolved = false,
                                  bool force_authored_section = false);

// Creates the exact section copy used at the start of BuildSweptSolidShape.
// Intended for visual diagnostics and profile-orientation verification.
bool BuildPlacedSweptSectionSketch(const CSmartLine& section,
                                   const CAlfaObject& guide,
                                   CSmartLine& placed_section,
                                   double delta_x = 0.0,
                                   double delta_y = 0.0,
                                   double angle_degrees = 0.0);

TopoDS_Shape BuildFrameSolidShape(const CSmartLine& profile,
                                  double width,
                                  double height);

TopoDS_Shape BuildWireSolidShape(const CPolyline& path, double radius);
TopoDS_Shape BuildWireSolidShape(const CAlfaObject& path, double radius);
