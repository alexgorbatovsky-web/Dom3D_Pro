#include "FourSplineSurfaceBuilder.h"

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
#ifdef XtPointer
#undef XtPointer
#endif

#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <Geom_BSplineSurface.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <cstdlib>
#include <cmath>
#include <iostream>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

SweepCurveSamples curve(std::initializer_list<CPoint3d> points)
{
    SweepCurveSamples result;
    result.points.assign(points.begin(), points.end());
    return result;
}
}

int main()
{
    // Intentionally shuffled and inconsistently directed: the builder must
    // discover the contiguous loop and orient the four boundaries itself.
    const SweepCurveSamples generator_1 = curve({
        {0.0, 0.0, 0.0}, {5.0, 0.0, 2.0}, {10.0, 0.0, 0.0}});
    const SweepCurveSamples guide_2 = curve({
        {0.0, 10.0, 0.0}, {0.0, 5.0, -1.0}, {0.0, 0.0, 0.0}});
    const SweepCurveSamples generator_2 = curve({
        {10.0, 10.0, 0.0}, {5.0, 10.0, 3.0}, {0.0, 10.0, 0.0}});
    const SweepCurveSamples guide_1 = curve({
        {10.0, 0.0, 0.0}, {10.0, 5.0, 1.0}, {10.0, 10.0, 0.0}});

    const TopoDS_Shape shape = BuildFourSplineSurfaceShape(
        generator_2, guide_1, generator_1, guide_2);
    require(!shape.IsNull(), "Four-spline surface was not created.");
    require(BRepCheck_Analyzer(shape).IsValid(),
            "Four-spline surface is topologically invalid.");

    int face_count = 0;
    for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
        ++face_count;
    }
    require(face_count == 1, "Four-spline result must contain one face.");

    // IGES curves are imported as 97 display samples.  Building an exact
    // interpolant from all of them used to create roughly 192 x 192 surface
    // parameter lines after opposite boundary knot compatibility.
    SweepCurveSamples sampled[4];
    constexpr int sample_count = 97;
    for (int i = 0; i < sample_count; ++i) {
        const double t = static_cast<double>(i) / (sample_count - 1);
        const double x = 10.0 * t;
        const double y = 10.0 * t;
        sampled[0].points.emplace_back(x, 0.0, std::sin(3.141592653589793 * t));
        sampled[1].points.emplace_back(10.0, y, 0.5 * std::sin(3.141592653589793 * t));
        sampled[2].points.emplace_back(10.0 - x, 10.0, 1.5 * std::sin(3.141592653589793 * t));
        sampled[3].points.emplace_back(0.0, 10.0 - y, 0.25 * std::sin(3.141592653589793 * t));
    }
    const TopoDS_Shape compact_shape = BuildFourSplineSurfaceShape(
        sampled[0], sampled[1], sampled[2], sampled[3]);
    TopExp_Explorer compact_faces(compact_shape, TopAbs_FACE);
    require(compact_faces.More(), "Sampled four-spline surface was not created.");
    const Handle(Geom_BSplineSurface) compact_surface =
        Handle(Geom_BSplineSurface)::DownCast(
            BRep_Tool::Surface(TopoDS::Face(compact_faces.Current())));
    require(!compact_surface.IsNull(), "Four-spline result is not a BSpline surface.");
    require(compact_surface->NbUPoles() < 64 && compact_surface->NbVPoles() < 64,
            "Sampled boundaries produced an excessive surface parameter grid.");
    return EXIT_SUCCESS;
}
