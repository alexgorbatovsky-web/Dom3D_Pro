#include "TwoSketchSolidBuilder.h"

#include "../SketchProfileBuilder.h"
#include "../SmartLine.h"

#ifdef Coord
#undef Coord
#endif
#ifdef String
#undef String
#endif
#ifdef LPCTSTR
#undef LPCTSTR
#endif
#ifdef Pixel
#undef Pixel
#endif

#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>

bool BuildSolidBetweenSketches(const CSmartLine& first,
                               const CSmartLine& second,
                               TopoDS_Shape& result,
                               std::string* error) {
    result.Nullify();
    TopoDS_Face first_face;
    TopoDS_Face second_face;
    Vec3 normal{};
    if (!BuildSketchProfileFace(first, first_face, normal)
        || !BuildSketchProfileFace(second, second_face, normal)) {
        if (error) *error = "Both sketches must be closed valid profiles.";
        return false;
    }
    const TopoDS_Wire first_wire = BRepTools::OuterWire(first_face);
    const TopoDS_Wire second_wire = BRepTools::OuterWire(second_face);
    if (first_wire.IsNull() || second_wire.IsNull()) {
        if (error) *error = "Sketch profile has no outer wire.";
        return false;
    }
    BRepOffsetAPI_ThruSections loft(true, false, 1.0e-6);
    loft.CheckCompatibility(true);
    loft.AddWire(first_wire);
    loft.AddWire(second_wire);
    loft.Build();
    if (!loft.IsDone() || loft.Shape().IsNull()) {
        if (error) *error = "Loft between sketches could not be built.";
        return false;
    }
    result = loft.Shape();
    return true;
}
