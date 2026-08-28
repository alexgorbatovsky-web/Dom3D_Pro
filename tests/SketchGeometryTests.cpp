#include "BezierSpline.h"
#include "Dimens.h"
#include "Fillet.h"
#include "LinkLine.h"
#include "LinkLineHor.h"
#include "LinkLineVert.h"
#include "SketchArcLine.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
bool nearly_equal(double first, double second, double tolerance = 1.0e-8) {
    return std::abs(first - second) <= tolerance;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    CSketchArcLine arc(
        CPoint3d(0.0, 0.0, 0.0),
        CPoint3d(5.0, 5.0, 0.0),
        CPoint3d(10.0, 0.0, 0.0));
    require(arc.IsValid(), "Three non-collinear points did not create an arc.");
    require(nearly_equal(arc.GetPoint(0.5).x, 5.0), "Arc midpoint X is incorrect.");
    require(nearly_equal(arc.GetPoint(0.5).y, 5.0), "Arc midpoint Y is incorrect.");
    require(nearly_equal(arc.GetLength(), 5.0 * std::acos(-1.0)), "Arc length is incorrect.");
    require(arc.Clone()->GetType() == LinkLineType::Arc, "Arc clone lost its type.");
    CSketchArcLine invalid_arc(
        CPoint3d(0.0, 0.0, 0.0),
        CPoint3d(5.0, 0.0, 0.0),
        CPoint3d(10.0, 0.0, 0.0));
    require(!invalid_arc.IsValid(), "Collinear points created an arc.");

    CLinkLine segment(CPoint3d(0.0, 0.0, 0.0), CPoint3d(3.0, 4.0, 0.0));
    require(nearly_equal(segment.GetLength(), 5.0), "Segment length is incorrect.");

    CLinkLineHor horizontal(CPoint3d(1.0, 2.0, 0.0), CPoint3d(5.0, 9.0, 0.0));
    require(nearly_equal(horizontal.GetEnd().y, 2.0), "Horizontal line did not enforce Y.");

    CLinkLineVert vertical(CPoint3d(3.0, 1.0, 0.0), CPoint3d(8.0, 6.0, 0.0));
    require(nearly_equal(vertical.GetEnd().x, 3.0), "Vertical line did not enforce X.");

    CBezierSpline bezier(
        CPoint3d(0.0, 0.0, 0.0),
        CPoint3d(0.0, 2.0, 0.0),
        CPoint3d(2.0, 2.0, 0.0),
        CPoint3d(2.0, 0.0, 0.0));
    const CPoint3d bezier_middle = bezier.GetPoint(0.5);
    require(nearly_equal(bezier_middle.x, 1.0), "Bezier midpoint X is incorrect.");
    require(nearly_equal(bezier_middle.y, 1.5), "Bezier midpoint Y is incorrect.");
    require(bezier.Sample(8).size() == 9, "Bezier sampling count is incorrect.");
    bezier.SetStart(CPoint3d(1.0, 1.0, 0.0));
    require(nearly_equal(bezier.GetControl1().x, 1.0), "Bezier start handle did not follow its endpoint.");
    require(nearly_equal(bezier.GetControl1().y, 3.0), "Bezier start handle Y did not follow its endpoint.");
    const std::unique_ptr<CLinkLine> bezier_clone = bezier.Clone();
    require(bezier_clone->GetType() == LinkLineType::Bezier, "Bezier clone lost its curve type.");
    require(
        nearly_equal(bezier_clone->GetPoint(0.5).x, bezier.GetPoint(0.5).x),
        "Bezier clone geometry is incorrect.");

    CDimens3D dimension(
        CPoint3d(0.0, 0.0, 0.0),
        CPoint3d(100.0, 0.0, 0.0),
        CPoint3d(0.0, -1.0, 0.0),
        12.0,
        "width",
        "Length",
        100.0);
    require(nearly_equal(dimension.GetMeasuredLength(), 100.0), "3D dimension length is incorrect.");
    const DimensionGeometry3D dimension_geometry = dimension.GetGeometry(2.0);
    require(nearly_equal(dimension_geometry.dimension_start.y, -12.0), "3D dimension offset is incorrect.");
    require(nearly_equal(dimension_geometry.extension_start.y, -14.0), "3D dimension extension is incorrect.");
    require(nearly_equal(dimension_geometry.text_position.x, 50.0), "3D dimension text position is incorrect.");

    CLinkLine first(CPoint3d(0.0, 0.0, 0.0), CPoint3d(10.0, 0.0, 0.0));
    CLinkLine second(CPoint3d(10.0, 0.0, 0.0), CPoint3d(10.0, 10.0, 0.0));
    CFillet fillet(0, 2.0);
    const FilletGeometry geometry = fillet.Calculate(first, second);
    require(geometry.valid, "Valid right-angle fillet was rejected.");
    require(nearly_equal(geometry.tangent_on_first.x, 8.0), "First tangent X is incorrect.");
    require(nearly_equal(geometry.tangent_on_first.y, 0.0), "First tangent Y is incorrect.");
    require(nearly_equal(geometry.tangent_on_second.x, 10.0), "Second tangent X is incorrect.");
    require(nearly_equal(geometry.tangent_on_second.y, 2.0), "Second tangent Y is incorrect.");
    require(nearly_equal(geometry.center.x, 8.0), "Fillet center X is incorrect.");
    require(nearly_equal(geometry.center.y, 2.0), "Fillet center Y is incorrect.");
    require(fillet.Sample(first, second).size() >= 7, "Fillet sampling is too coarse.");

    CFillet oversized(0, 20.0);
    require(!oversized.Calculate(first, second).valid, "Oversized fillet was accepted.");

    CBezierSpline straight_bezier(
        CPoint3d(0.0, 0.0, 0.0),
        CPoint3d(10.0 / 3.0, 0.0, 0.0),
        CPoint3d(20.0 / 3.0, 0.0, 0.0),
        CPoint3d(10.0, 0.0, 0.0));
    CFillet bezier_fillet(0, 2.0);
    const FilletGeometry bezier_fillet_geometry =
        bezier_fillet.Calculate(straight_bezier, second);
    require(bezier_fillet_geometry.valid, "Bezier-to-line fillet was rejected.");
    require(
        nearly_equal(bezier_fillet_geometry.tangent_on_first.x, 8.0, 1.0e-5),
        "Bezier fillet first tangent is incorrect.");
    require(
        nearly_equal(bezier_fillet_geometry.tangent_on_second.y, 2.0, 1.0e-5),
        "Bezier fillet second tangent is incorrect.");
    require(
        bezier_fillet_geometry.first_parameter > 0.0
            && bezier_fillet_geometry.first_parameter < 1.0,
        "Bezier fillet did not trim the first curve.");

    CBezierSpline curved_bezier(
        CPoint3d(0.0, 0.0, 0.0),
        CPoint3d(3.0, 0.0, 0.0),
        CPoint3d(8.0, -2.0, 0.0),
        CPoint3d(10.0, 0.0, 0.0));
    CFillet curved_bezier_fillet(0, 0.75);
    const FilletGeometry curved_bezier_geometry =
        curved_bezier_fillet.Calculate(curved_bezier, second);
    require(curved_bezier_geometry.valid, "Curved Bezier-to-line fillet was rejected.");
    const double curved_radius_x =
        curved_bezier_geometry.tangent_on_first.x - curved_bezier_geometry.center.x;
    const double curved_radius_y =
        curved_bezier_geometry.tangent_on_first.y - curved_bezier_geometry.center.y;
    require(
        nearly_equal(
            std::hypot(curved_radius_x, curved_radius_y),
            0.75,
            1.0e-5),
        "Curved Bezier fillet radius is incorrect.");

    CLinkLine closing(CPoint3d(0.0, 10.0, 0.0), CPoint3d(0.0, 0.0, 0.0));
    CLinkLine first_closed(CPoint3d(0.0, 0.0, 0.0), CPoint3d(10.0, 0.0, 0.0));
    CFillet closing_fillet(3, 0, 1.0);
    require(
        closing_fillet.Calculate(closing, first_closed).valid,
        "Fillet at the closing sketch node was rejected.");
    return EXIT_SUCCESS;
}
