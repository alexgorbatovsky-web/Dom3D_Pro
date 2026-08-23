#pragma once

#include "CAssembled.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class TopoDS_Shape;
class CSmartLine;
class CSplineCurve;
class CBSpline;
class CVector;

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

    // Creates the reusable classic Dom-3D Milano frame section in the XY
    // plane. The caller owns the sketch and may place it on any sweep guide.
    static std::unique_ptr<CSmartLine> CreateMilanoProfile(
        double frame_width);

    // Converts the legacy cubic spline exactly to one cubic NURBS curve.
    // The returned object can be added to the document and used by Solid
    // Swept without sampling or refitting the source geometry.
    static std::unique_ptr<CBSpline> CreateNurbsGuide(
        const CSplineCurve* guide,
        std::string* error = nullptr);

    // Sweeps a closed sketch profile along a legacy spline. The profile is
    // placed at the guide start and rotated around its tangent by angle.
    // Returns a null shape on failure and optionally describes the error.
    static TopoDS_Shape BuildSweptProfile(
        const CSmartLine* profile,
        const CSplineCurve* guide,
        double angle,
        double dx,
        double dy,
        std::string* error = nullptr);

    // Extrudes a closed sketch by dist along dir. The direction is
    // normalized; dist may be signed. Returns a null shape on failure.
    static TopoDS_Shape BuildExtrude(
        const CSmartLine* profile,
        const CVector& dir,
        double dist,
        std::string* error = nullptr);

    // Returns the section positioned at the exact start and tangent of the
    // legacy guide. This is the same placement used by BuildSweptProfile and
    // may be added to the document for visual inspection before sweeping.
    static std::unique_ptr<CSmartLine> PlaceSweptProfile(
        const CSmartLine* profile,
        const CSplineCurve* guide,
        double angle,
        double dx,
        double dy,
        std::string* error = nullptr);

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
