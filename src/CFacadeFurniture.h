#pragma once

#include "CAssembled.h"

#include <cstdint>
#include <string>
#include <vector>

class TopoDS_Shape;
class CSmartLine;

enum class KitchenCabinetFacadeStyle : std::uint8_t {
    Plain = 0,
    Frame = 1,
    Screen = 2,
    Milled = 3,
    Milano = 4
};

enum class KitchenCabinetShowcaseFill : std::uint8_t {
    Glass = 0,
    Lattice = 1,
    MuntinBars = 2,
    StainedGlass = 3,
    None = 4
};

// Parametric furniture facade.  The class owns facade-specific geometry;
// cabinets and other furniture only place, trim and animate the result.
class CFacadeFurniture : public CAssembled {
public:
    explicit CFacadeFurniture(std::string name = "Furniture Facade");
    CFacadeFurniture(std::string name,
                     std::vector<unsigned long> element_ids);

    static bool IsValidStyle(KitchenCabinetFacadeStyle style);

    // Builds a facade in the XZ plane.  The outward/front surface is at y.
    // Frame returns a compound containing exactly two technological solids:
    // the outer frame and the separately milled centre panel.
    static TopoDS_Shape BuildPlanarShape(
        KitchenCabinetFacadeStyle style,
        double x,
        double y,
        double z,
        double width,
        double thickness,
        double height,
        bool round_screen_front = true,
        KitchenCabinetShowcaseFill showcase_fill =
            KitchenCabinetShowcaseFill::None,
        CSmartLine* source_cutter_debug = nullptr);

    // Builds a Frame facade from two user-defined XY section sketches.
    // The first sketch is swept around the outside of the door; the second
    // forms the routed centre panel and its central plateau.
    static TopoDS_Shape BuildPlanarFrameFromProfiles(
        const CSmartLine& frame_profile,
        const CSmartLine& panel_profile,
        double x,
        double y,
        double z,
        double width,
        double thickness,
        double height);

    // Builds the same Milled facade as BuildPlanarShape, using catalog
    // sketches for the cutter section and one or more routing guides. Cutter
    // physical cutters use their tip at Y=0; temporary working copies are
    // oriented automatically and advanced to the requested milling depth.
    static TopoDS_Shape BuildPlanarMilledFromProfiles(
        const CSmartLine& cutter_profile,
        const std::vector<const CSmartLine*>& guides,
        double x,
        double y,
        double z,
        double width,
        double thickness,
        double height,
        CSmartLine* source_cutter_debug = nullptr,
        bool require_evolved = false,
        TopoDS_Shape* cutter_volume_debug = nullptr,
        double cutting_depth = 9.0);
};
