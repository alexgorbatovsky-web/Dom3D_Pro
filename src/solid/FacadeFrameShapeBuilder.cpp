#include "FacadeFrameShapeBuilder.h"

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Mat.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <array>
#include <utility>
#include <vector>

namespace {
TopoDS_Shape compound_shape(const std::vector<TopoDS_Shape>& shapes) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        if (shape.IsNull()) {
            return {};
        }
        builder.Add(compound, shape);
    }
    return compound;
}

TopoDS_Shape mitered_horizontal_rail(const TopoDS_Wire& section,
                                     double z,
                                     double length) {
    try {
        gp_GTrsf start_transform;
        start_transform.SetVectorialPart(gp_Mat(
            1.0, 0.0, 1.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0));
        start_transform.SetTranslationPart(gp_XYZ(-z, 0.0, 0.0));

        gp_GTrsf end_transform;
        end_transform.SetVectorialPart(gp_Mat(
            1.0, 0.0, -1.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0));
        end_transform.SetTranslationPart(gp_XYZ(length + z, 0.0, 0.0));

        BRepBuilderAPI_GTransform start_builder(
            section, start_transform, true);
        BRepBuilderAPI_GTransform end_builder(
            section, end_transform, true);
        start_builder.Build();
        end_builder.Build();
        if (!start_builder.IsDone() || !end_builder.IsDone()) return {};

        BRepOffsetAPI_ThruSections loft(true, true);
        loft.CheckCompatibility(false);
        loft.AddWire(TopoDS::Wire(start_builder.Shape()));
        loft.AddWire(TopoDS::Wire(end_builder.Shape()));
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Shape transformed_copy(const TopoDS_Shape& shape,
                              const gp_Trsf& transform) {
    BRepBuilderAPI_Transform builder(shape, transform, true);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape{};
}

TopoDS_Wire frame_section(double x,
                          double y,
                          double z,
                          double thickness,
                          double frame_width) {
    const auto point = [=](double u, double v) {
        const double scale = frame_width / 60.0;
        return gp_Pnt(x, y + thickness - v * scale, z + u * scale);
    };
    BRepBuilderAPI_MakeWire wire;
    const auto add_line = [&](double u1, double v1, double u2, double v2) {
        BRepBuilderAPI_MakeEdge edge(point(u1, v1), point(u2, v2));
        if (edge.IsDone()) wire.Add(edge.Edge());
    };
    const auto add_bezier = [&](const std::array<std::pair<double, double>, 4>& poles) {
        TColgp_Array1OfPnt points(1, 4);
        for (int index = 0; index < 4; ++index) {
            points.SetValue(index + 1, point(
                poles[index].first, poles[index].second));
        }
        const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(points);
        BRepBuilderAPI_MakeEdge edge(curve);
        if (edge.IsDone()) wire.Add(edge.Edge());
    };

    add_line(0.0, 0.0, 0.0, 12.0);
    add_line(0.0, 12.0, 2.0, 12.0);
    add_line(2.0, 12.0, 4.0, 15.0);
    add_bezier({{{4.0, 15.0}, {13.9, 20.2226},
                 {41.881, 20.2226}, {51.3, 15.0}}});
    add_bezier({{{51.3, 15.0}, {53.0, 17.0},
                 {55.0, 17.0}, {57.0, 15.0}}});
    add_line(57.0, 15.0, 60.0, 15.0);
    add_line(60.0, 15.0, 60.0, 0.0);
    add_line(60.0, 0.0, 0.0, 0.0);
    return wire.IsDone() ? wire.Wire() : TopoDS_Wire{};
}
}

TopoDS_Shape BuildFastFacadeFrameShapeFromSection(
    const TopoDS_Wire& section,
    double x,
    double y,
    double z,
    double width,
    double height) {
    if (section.IsNull()) return {};
    const TopoDS_Shape horizontal = mitered_horizontal_rail(
        section, z, width);
    const TopoDS_Shape vertical = mitered_horizontal_rail(
        section, z, height);
    if (horizontal.IsNull() || vertical.IsNull()) return {};

    constexpr double pi = 3.14159265358979323846;
    const gp_Pnt origin(x, y, z);
    gp_Trsf top_transform;
    top_transform.SetRotation(gp_Ax1(origin, gp_Dir(0.0, 1.0, 0.0)), pi);
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(width, 0.0, height));
    top_transform.PreMultiply(translation);

    gp_Trsf right_transform;
    right_transform.SetRotation(
        gp_Ax1(origin, gp_Dir(0.0, 1.0, 0.0)), -pi * 0.5);
    translation.SetTranslation(gp_Vec(width, 0.0, 0.0));
    right_transform.PreMultiply(translation);

    gp_Trsf left_transform;
    left_transform.SetRotation(
        gp_Ax1(origin, gp_Dir(0.0, 1.0, 0.0)), pi * 0.5);
    translation.SetTranslation(gp_Vec(0.0, 0.0, height));
    left_transform.PreMultiply(translation);

    const TopoDS_Shape top_untrimmed = transformed_copy(horizontal, top_transform);
    const TopoDS_Shape right_untrimmed = transformed_copy(vertical, right_transform);
    const TopoDS_Shape left_untrimmed = transformed_copy(vertical, left_transform);
    if (top_untrimmed.IsNull() || right_untrimmed.IsNull()
        || left_untrimmed.IsNull()) return {};

    return compound_shape({
        horizontal, right_untrimmed, top_untrimmed, left_untrimmed});
}

TopoDS_Shape BuildFastFacadeFrameShape(double x,
                                       double y,
                                       double z,
                                       double width,
                                       double thickness,
                                       double height,
                                       double frame_width) {
    if (width <= 2.0 * frame_width
        || height <= 2.0 * frame_width
        || thickness <= 0.0
        || frame_width <= 0.0) {
        return {};
    }

    const TopoDS_Wire section = frame_section(
        x, y, z, thickness, frame_width);
    return BuildFastFacadeFrameShapeFromSection(
        section, x, y, z, width, height);
}
