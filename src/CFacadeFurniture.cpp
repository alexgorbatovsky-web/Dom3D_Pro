#include "CFacadeFurniture.h"
#include "BezierSpline.h"
#include "SketchProfileBuilder.h"
#include "SketchArcLine.h"
#include "SmartLine.h"
#include "SweptSolidBuilder.h"
#include "CAlfaDoc.h"
#include "solid/FacadeFrameShapeBuilder.h"

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

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_TransitionMode.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <gp_Trsf.hxx>
#include <Geom_BezierCurve.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {
TopoDS_Shape box_shape(double x, double y, double z,
                       double width, double depth, double height) {
    BRepPrimAPI_MakeBox builder(gp_Pnt(x, y, z), width, depth, height);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape fillet_front_edges(const TopoDS_Shape& shape,
                                double front_y,
                                double radius) {
    if (shape.IsNull() || radius <= 0.0) {
        return shape;
    }
    BRepFilletAPI_MakeFillet fillet(shape);
    int edge_count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        TopoDS_Vertex first_vertex;
        TopoDS_Vertex last_vertex;
        TopExp::Vertices(edge, first_vertex, last_vertex);
        if (first_vertex.IsNull() || last_vertex.IsNull()) {
            continue;
        }
        const gp_Pnt first = BRep_Tool::Pnt(first_vertex);
        const gp_Pnt last = BRep_Tool::Pnt(last_vertex);
        if (std::abs(first.Y() - front_y) <= 1.0e-7
            && std::abs(last.Y() - front_y) <= 1.0e-7) {
            fillet.Add(radius, edge);
            ++edge_count;
        }
    }
    if (edge_count == 0) {
        return shape;
    }
    fillet.Build();
    return fillet.IsDone() ? fillet.Shape() : shape;
}

TopoDS_Shape compound_shape(const std::vector<TopoDS_Shape>& shapes) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    bool has_shape = false;
    for (const TopoDS_Shape& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
            has_shape = true;
        }
    }
    return has_shape ? TopoDS_Shape(compound) : TopoDS_Shape();
}

TopoDS_Shape translated_shape(const TopoDS_Shape& shape,
                              double x,
                              double y,
                              double z,
                              bool copy_geometry = false) {
    if (shape.IsNull()) {
        return {};
    }
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(x, y, z));
    BRepBuilderAPI_Transform builder(shape, transform, copy_geometry);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape{};
}

TopoDS_Shape rectangular_ring_shape(double x,
                                    double y,
                                    double z,
                                    double width,
                                    double depth,
                                    double height,
                                    double border_width) {
    if (width <= 2.0 * border_width || height <= 2.0 * border_width) {
        return {};
    }
    const TopoDS_Shape outer = box_shape(x, y, z, width, depth, height);
    const TopoDS_Shape opening = box_shape(
        x + border_width,
        y - 0.1,
        z + border_width,
        width - 2.0 * border_width,
        depth + 0.2,
        height - 2.0 * border_width);
    BRepAlgoAPI_Cut cut(outer, opening);
    cut.Build();
    return cut.IsDone() ? cut.Shape() : TopoDS_Shape();
}

TopoDS_Shape frame_shape(double x,
                         double y,
                         double z,
                         double width,
                         double thickness,
                         double height,
                         double frame_width) {
    const TopoDS_Shape fast_frame = BuildFastFacadeFrameShape(
        x, y, z, width, thickness, height, frame_width);
    if (!fast_frame.IsNull()) {
        return fast_frame;
    }

    // Exact CreateFilenka5 ("Milano") section from the classic Dom-3D.
    // U runs from the outer edge towards the opening; V is the section
    // height measured forward from the rear facade plane.
    const auto profile_point = [=](double u, double v) {
        const double scale = frame_width / 60.0;
        return gp_Pnt(
            x,
            y + thickness - v * scale,
            z + u * scale);
    };
    BRepBuilderAPI_MakeWire section_builder;
    const auto add_line = [&](double u1, double v1, double u2, double v2) {
        BRepBuilderAPI_MakeEdge edge(
            profile_point(u1, v1), profile_point(u2, v2));
        if (edge.IsDone()) {
            section_builder.Add(edge.Edge());
        }
    };
    const auto add_bezier = [&](const std::array<std::pair<double, double>, 4>& poles) {
        TColgp_Array1OfPnt points(1, 4);
        for (int index = 0; index < 4; ++index) {
            points.SetValue(
                index + 1,
                profile_point(poles[index].first, poles[index].second));
        }
        const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(points);
        BRepBuilderAPI_MakeEdge edge(curve);
        if (edge.IsDone()) {
            section_builder.Add(edge.Edge());
        }
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

    BRepBuilderAPI_MakePolygon guide_builder;
    guide_builder.Add(gp_Pnt(x, y + thickness, z));
    guide_builder.Add(gp_Pnt(x + width, y + thickness, z));
    guide_builder.Add(gp_Pnt(x + width, y + thickness, z + height));
    guide_builder.Add(gp_Pnt(x, y + thickness, z + height));
    guide_builder.Close();

    if (section_builder.IsDone() && guide_builder.IsDone()) {
        try {
            BRepOffsetAPI_MakePipeShell sweep(guide_builder.Wire());
            sweep.SetMode(gp_Dir(0.0, -1.0, 0.0));
            sweep.SetTransitionMode(BRepBuilderAPI_RightCorner);
            sweep.Add(section_builder.Wire(), false, false);
            if (sweep.IsReady()) {
                sweep.Build();
                if (sweep.IsDone() && sweep.MakeSolid()) {
                    return sweep.Shape();
                }
            }
        } catch (const Standard_Failure&) {
            // Keep the robust fallback below for unusually small facades.
        }
    }

    TopoDS_Shape frame = rectangular_ring_shape(
        x, y, z, width, thickness, height, frame_width);
    if (frame.IsNull()) {
        return {};
    }
    frame = fillet_front_edges(frame, y, 2.0);

    const double molding_width = std::clamp(frame_width * 0.22, 7.0, 14.0);
    const double molding_raise = std::clamp(thickness * 0.18, 2.0, 4.0);
    TopoDS_Shape molding = rectangular_ring_shape(
        x + frame_width - molding_width,
        y - molding_raise,
        z + frame_width - molding_width,
        width - 2.0 * (frame_width - molding_width),
        molding_raise + 0.3,
        height - 2.0 * (frame_width - molding_width),
        molding_width);
    if (molding.IsNull()) {
        return frame;
    }
    molding = fillet_front_edges(molding, y - molding_raise, 2.0);
    BRepAlgoAPI_Fuse fuse(frame, molding);
    fuse.Build();
    return fuse.IsDone() ? fuse.Shape() : frame;
}

struct LegacyProfileSelection {
    std::size_t num_line = 0;
    int num_pnt = 1;
};

bool apply_legacy_profile_fillet(CSmartLine& profile,
                                 LegacyProfileSelection selection,
                                 double radius) {
    const std::size_t line_count = profile.GetNumLines();
    if (line_count == 0 || selection.num_line >= line_count) {
        return false;
    }
    // Classic CSelectPrim uses point 1 for the end of the selected line.
    // Point 0 denotes the beginning, hence the previous line junction.
    const std::size_t first_line = selection.num_pnt == 0
        ? (selection.num_line + line_count - 1) % line_count
        : selection.num_line;
    return profile.AddFillet(first_line, radius);
}

TopoDS_Shape milano_frame_shape(double x,
                                double y,
                                double z,
                                double width,
                                double thickness,
                                double height,
                                double frame_width) {
    const double scale = frame_width / 60.0;
    const auto point = [scale](double u, double v) {
        return CPoint3d(u * scale, v * scale, 0.0);
    };
    CSmartLine profile("Milano_4");
    profile.AddLine(std::make_unique<CLinkLine>(
        point(0.0, 0.0), point(0.0, 5.0)));
    profile.AddLine(std::make_unique<CLinkLine>(
        point(0.0, 5.0), point(2.0, 5.0)));
    profile.AddLine(std::make_unique<CLinkLine>(
        point(2.0, 5.0), point(4.0, 8.0)));
    profile.AddLine(std::make_unique<CBezierSpline>(
        point(4.0, 8.0), point(13.9, 13.2226),
        point(41.881, 13.2226), point(51.3, 8.0)));
    profile.AddLine(std::make_unique<CBezierSpline>(
        point(51.3, 8.0), point(53.0, 10.0),
        point(55.0, 10.0), point(57.0, 8.0)));
    profile.AddLine(std::make_unique<CLinkLine>(
        point(57.0, 8.0), point(60.0, 8.0)));
    profile.AddLine(std::make_unique<CLinkLine>(
        point(60.0, 8.0), point(60.0, 0.0)));
    profile.AddLine(std::make_unique<CLinkLine>(
        point(60.0, 0.0), point(0.0, 0.0)));
    // Facade profiles are authored in the global XY plane.  Their placement
    // on a particular facade is a separate operation performed below.  This
    // keeps a debug copy of the sketch natural and reusable.
    if (!profile.SetClosed(true)
        || !profile.SetCoordinateSystem(
            CPoint3d(0.0, 0.0, 0.0),
            CPoint3d(1.0, 0.0, 0.0),
            CPoint3d(0.0, 0.0, 1.0))) {
        return {};
    }

    LegacyProfileSelection selection;
    selection.num_pnt = 1;
    if (!apply_legacy_profile_fillet(profile, selection, 1.0 * scale)) {
        return {};
    }
    selection.num_line = 2;
    if (!apply_legacy_profile_fillet(profile, selection, 2.0 * scale)) {
        return {};
    }
    selection.num_line = 5;
    if (!apply_legacy_profile_fillet(profile, selection, 1.0 * scale)) {
        return {};
    }
    [[maybe_unused]] CSmartLine profile_cpy = profile.MakeCopy();
    // Debug example:
 //   CAlfaDoc* pDoc = GetAlfaDoc();
  //  pDoc->AddObject(std::make_unique<CSmartLine>(std::move(profile_cpy)));

    TopoDS_Face profile_face;
    Vec3 profile_normal{};
    if (!BuildSketchProfileFace(profile, profile_face, profile_normal)) {
        return {};
    }
    // Map local profile coordinates (U,V,0) to the first frame corner:
    // U -> +Z, V -> -Y.  The sketch itself remains on global XY.
    gp_Trsf profile_placement;
    profile_placement.SetValues(
        0.0,  0.0, 1.0, x,
        0.0, -1.0, 0.0, y + thickness,
        1.0,  0.0, 0.0, z);
    BRepBuilderAPI_Transform placed_profile(
        profile_face, profile_placement, true);
    placed_profile.Build();
    if (!placed_profile.IsDone()) {
        return {};
    }
    const TopoDS_Wire section_wire = BRepTools::OuterWire(
        TopoDS::Face(placed_profile.Shape()));
    if (section_wire.IsNull()) {
        return {};
    }

    // Keep the exact classic Milano section, but assemble four straight
    // mitered rails instead of running one expensive PipeShell through four
    // right-angle transitions.  The visible profile is identical and the
    // resulting independent rails are also cheaper to rebuild and render.
    const TopoDS_Shape fast_frame = BuildFastFacadeFrameShapeFromSection(
        section_wire, x, y, z, width, height);
    if (!fast_frame.IsNull()) {
        return fast_frame;
    }

    BRepBuilderAPI_MakePolygon guide_builder;
    guide_builder.Add(gp_Pnt(x, y + thickness, z));
    guide_builder.Add(gp_Pnt(x + width, y + thickness, z));
    guide_builder.Add(gp_Pnt(x + width, y + thickness, z + height));
    guide_builder.Add(gp_Pnt(x, y + thickness, z + height));
    guide_builder.Close();
    if (!guide_builder.IsDone()) {
        return {};
    }

    try {
        BRepOffsetAPI_MakePipeShell sweep(guide_builder.Wire());
        sweep.SetMode(gp_Dir(0.0, -1.0, 0.0));
        sweep.SetTransitionMode(BRepBuilderAPI_RightCorner);
        sweep.Add(section_wire, false, false);
        if (!sweep.IsReady()) {
            return {};
        }
        sweep.Build();
        if (!sweep.IsDone() || !sweep.MakeSolid()) {
            return {};
        }
        return sweep.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Shape flat_center_panel_shape(double x,
                                     double y,
                                     double z,
                                     double width,
                                     double thickness,
                                     double height,
                                     double frame_width) {
    const double overlap = std::clamp(frame_width * 0.12, 5.0, 8.0);
    const double inset = frame_width - overlap;
    const double panel_depth = std::clamp(thickness * 0.55, 5.0, thickness);
    return box_shape(
        x + inset,
        y + thickness - panel_depth,
        z + inset,
        width - 2.0 * inset,
        panel_depth,
        height - 2.0 * inset);
}

TopoDS_Wire rectangle_wire(double x,
                           double y,
                           double z,
                           double width,
                           double height) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(x, y, z));
    polygon.Add(gp_Pnt(x + width, y, z));
    polygon.Add(gp_Pnt(x + width, y, z + height));
    polygon.Add(gp_Pnt(x, y, z + height));
    polygon.Close();
    return polygon.IsDone() ? polygon.Wire() : TopoDS_Wire{};
}

TopoDS_Shape frame_center_panel_shape(double x,
                                      double y,
                                      double z,
                                      double width,
                                      double thickness,
                                      double height,
                                      double frame_width) {
    const double overlap = std::clamp(frame_width * 0.16, 5.0, 10.0);
    const double inset = frame_width - overlap;
    const double panel_width = width - 2.0 * inset;
    const double panel_height = height - 2.0 * inset;
    if (panel_width <= 2.0 || panel_height <= 2.0) {
        return {};
    }

    constexpr double requested_milling_width = 32.6;
    const double maximum_milling_width = std::max(
        2.0, std::min(panel_width, panel_height) * 0.5 - overlap - 1.0);
    const double milling_width = std::min(
        requested_milling_width, maximum_milling_width);
    const double loft_inset = overlap + milling_width;
    const double base_front = y + thickness * 0.64;
    const double plateau_front = y + thickness * 0.30;

    const TopoDS_Shape base = box_shape(
        x + inset, base_front, z + inset,
        panel_width, y + thickness - base_front, panel_height);
    const double plateau_width = panel_width - 2.0 * loft_inset;
    const double plateau_height = panel_height - 2.0 * loft_inset;
    if (plateau_width <= 1.0 || plateau_height <= 1.0) {
        return fillet_front_edges(base, base_front, 2.0);
    }

    const TopoDS_Wire outer_wire = rectangle_wire(
        x + inset, base_front, z + inset, panel_width, panel_height);
    const TopoDS_Wire inner_wire = rectangle_wire(
        x + inset + loft_inset,
        plateau_front,
        z + inset + loft_inset,
        plateau_width,
        plateau_height);
    if (outer_wire.IsNull() || inner_wire.IsNull()) {
        return base;
    }

    BRepOffsetAPI_ThruSections routed_band(true, false);
    routed_band.AddWire(outer_wire);
    routed_band.AddWire(inner_wire);
    routed_band.Build();
    if (!routed_band.IsDone()) {
        return base;
    }

    TopoDS_Shape plateau = box_shape(
        x + inset + loft_inset,
        plateau_front,
        z + inset + loft_inset,
        plateau_width,
        y + thickness - plateau_front,
        plateau_height);
    plateau = fillet_front_edges(plateau, plateau_front, 2.0);

    BRepAlgoAPI_Fuse base_and_band(base, routed_band.Shape());
    base_and_band.Build();
    const TopoDS_Shape first = base_and_band.IsDone()
        ? base_and_band.Shape()
        : compound_shape({base, routed_band.Shape()});
    BRepAlgoAPI_Fuse completed(first, plateau);
    completed.Build();
    return completed.IsDone()
        ? completed.Shape()
        : compound_shape({first, plateau});
}

void add_rectangular_border(std::vector<TopoDS_Shape>& shapes,
                            double x,
                            double y,
                            double z,
                            double width,
                            double depth,
                            double height,
                            double border_width,
                            double front_fillet_radius = 0.0) {
    const double horizontal_width = std::max(1.0, width - 2.0 * border_width);
    const auto add_board = [&](TopoDS_Shape board) {
        shapes.push_back(fillet_front_edges(
            board, y, front_fillet_radius));
    };
    add_board(box_shape(x, y, z, border_width, depth, height));
    add_board(box_shape(
        x + width - border_width, y, z, border_width, depth, height));
    add_board(box_shape(
        x + border_width, y, z,
        horizontal_width, depth, border_width));
    add_board(box_shape(
        x + border_width, y, z + height - border_width,
        horizontal_width, depth, border_width));
}

TopoDS_Shape showcase_bar(double center_x,
                          double y,
                          double center_z,
                          double length,
                          double depth,
                          double bar_width,
                          double angle_radians,
                          double opening_x,
                          double opening_z,
                          double opening_width,
                          double opening_height) {
    struct Point2 {
        double x = 0.0;
        double z = 0.0;
    };
    const double direction_x = std::cos(angle_radians);
    const double direction_z = -std::sin(angle_radians);
    const double normal_x = -direction_z;
    const double normal_z = direction_x;
    const double half_length = length * 0.5;
    const double half_width = bar_width * 0.5;
    std::vector<Point2> polygon = {
        {center_x - direction_x * half_length - normal_x * half_width,
         center_z - direction_z * half_length - normal_z * half_width},
        {center_x + direction_x * half_length - normal_x * half_width,
         center_z + direction_z * half_length - normal_z * half_width},
        {center_x + direction_x * half_length + normal_x * half_width,
         center_z + direction_z * half_length + normal_z * half_width},
        {center_x - direction_x * half_length + normal_x * half_width,
         center_z - direction_z * half_length + normal_z * half_width}
    };

    const auto clip_by_plane = [](std::vector<Point2> input,
                                  bool x_plane,
                                  double coordinate,
                                  bool keep_greater) {
        std::vector<Point2> output;
        if (input.empty()) {
            return output;
        }
        const auto value = [x_plane](const Point2& point) {
            return x_plane ? point.x : point.z;
        };
        const auto inside = [&](const Point2& point) {
            return keep_greater
                ? value(point) >= coordinate - 1.0e-9
                : value(point) <= coordinate + 1.0e-9;
        };
        Point2 previous = input.back();
        bool previous_inside = inside(previous);
        for (const Point2& current : input) {
            const bool current_inside = inside(current);
            if (previous_inside != current_inside) {
                const double denominator = value(current) - value(previous);
                const double ratio = std::abs(denominator) <= 1.0e-12
                    ? 0.0
                    : (coordinate - value(previous)) / denominator;
                output.push_back({
                    previous.x + (current.x - previous.x) * ratio,
                    previous.z + (current.z - previous.z) * ratio});
            }
            if (current_inside) {
                output.push_back(current);
            }
            previous = current;
            previous_inside = current_inside;
        }
        return output;
    };

    polygon = clip_by_plane(std::move(polygon), true, opening_x, true);
    polygon = clip_by_plane(
        std::move(polygon), true, opening_x + opening_width, false);
    polygon = clip_by_plane(std::move(polygon), false, opening_z, true);
    polygon = clip_by_plane(
        std::move(polygon), false, opening_z + opening_height, false);
    if (polygon.size() < 3) {
        return {};
    }

    BRepBuilderAPI_MakePolygon outline;
    for (const Point2& point : polygon) {
        outline.Add(gp_Pnt(point.x, y, point.z));
    }
    outline.Close();
    if (!outline.IsDone()) {
        return {};
    }
    BRepBuilderAPI_MakeFace face(outline.Wire());
    if (!face.IsDone()) {
        return {};
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, depth, 0.0));
    prism.Build();
    return prism.IsDone() ? prism.Shape() : TopoDS_Shape{};
}

std::vector<TopoDS_Shape> stained_louisiana_wires(
    double opening_x,
    double front_y,
    double opening_z,
    double opening_width,
    double opening_height) {
    constexpr double source_width = 480.0;
    constexpr double source_height = 680.0;
    const double kx = opening_width / source_width;
    const double ky = opening_height / source_height;
    const auto point = [=](double x, double y,
                           bool mirror_x = false,
                           bool mirror_y = false) {
        if (mirror_x) {
            x = source_width - x;
        }
        if (mirror_y) {
            y = source_height - y;
        }
        return CPoint3d(x * kx, y * ky, 0.0);
    };

    std::vector<std::unique_ptr<CSmartLine>> guides;
    const auto line_guide = [&](double x1, double y1,
                                double x2, double y2,
                                bool mirror_x = false,
                                bool mirror_y = false) {
        auto guide = std::make_unique<CSmartLine>("Stained Louisiana Line");
        guide->SetCoordinateSystem(
            CPoint3d(0.0, 0.0, 0.0),
            CPoint3d(1.0, 0.0, 0.0),
            CPoint3d(0.0, 0.0, 1.0));
        guide->AddLine(std::make_unique<CLinkLine>(
            point(x1, y1, mirror_x, mirror_y),
            point(x2, y2, mirror_x, mirror_y)));
        guides.push_back(std::move(guide));
    };
    const auto curve_guide = [&](const std::array<CPoint3d, 4>& first,
                                 const std::array<CPoint3d, 4>* second,
                                 const CPoint3d* line_end,
                                 bool mirror_x = false) {
        const auto p = [&](const CPoint3d& source) {
            return point(source.x, source.y, mirror_x, false);
        };
        auto guide = std::make_unique<CSmartLine>("Stained Louisiana Curve");
        guide->SetCoordinateSystem(
            CPoint3d(0.0, 0.0, 0.0),
            CPoint3d(1.0, 0.0, 0.0),
            CPoint3d(0.0, 0.0, 1.0));
        guide->AddLine(std::make_unique<CBezierSpline>(
            p(first[0]), p(first[1]), p(first[2]), p(first[3])));
        if (second) {
            guide->AddLine(std::make_unique<CBezierSpline>(
                p((*second)[0]), p((*second)[1]),
                p((*second)[2]), p((*second)[3])));
        }
        if (line_end) {
            guide->AddLine(std::make_unique<CLinkLine>(
                p(first[3]), p(*line_end)));
        }
        guides.push_back(std::move(guide));
    };

    // Louisiana 1: the two outer vertical muntins.
    line_guide(50.0, 0.0, 50.0, 680.0);
    line_guide(50.0, 0.0, 50.0, 680.0, true);

    // Louisiana 2 and its X mirror.
    const std::array<CPoint3d, 4> curve2 = {{
        {240.0, 580.1564, 0.0}, {303.2725, 515.7550, 0.0},
        {269.2728, 333.1109, 0.0}, {120.9966, 340.0, 0.0}}};
    const CPoint3d curve2_end(0.0, 340.0, 0.0);
    curve_guide(curve2, nullptr, &curve2_end);
    curve_guide(curve2, nullptr, &curve2_end, true);

    // Louisiana 3 and 4: centre stems.
    line_guide(240.0, 580.1564, 240.0, 680.0);
    line_guide(240.0, 99.8436, 240.0, 0.0);

    // Louisiana 5 and its X mirror.
    const std::array<CPoint3d, 4> curve5 = {{
        {120.9966, 340.0, 0.0}, {269.2728, 346.8891, 0.0},
        {303.2725, 164.2450, 0.0}, {240.0, 99.8436, 0.0}}};
    curve_guide(curve5, nullptr, nullptr);
    curve_guide(curve5, nullptr, nullptr, true);

    // Louisiana 6 and its Y mirror.
    line_guide(0.0, 645.0, 480.0, 645.0);
    line_guide(0.0, 645.0, 480.0, 645.0, false, true);

    // Louisiana 7: the central S-shaped pair and its X mirror.
    const std::array<CPoint3d, 4> curve7a = {{
        {210.0676, 519.3486, 0.0}, {137.8411, 477.5208, 0.0},
        {176.8148, 393.8651, 0.0}, {240.0, 340.0, 0.0}}};
    const std::array<CPoint3d, 4> curve7b = {{
        {240.0, 340.0, 0.0}, {303.1852, 286.1349, 0.0},
        {342.1589, 202.4792, 0.0}, {269.9324, 160.6514, 0.0}}};
    curve_guide(curve7a, &curve7b, nullptr);
    curve_guide(curve7a, &curve7b, nullptr, true);

    gp_Trsf placement;
    placement.SetValues(
        1.0, 0.0, 0.0, opening_x,
        0.0, 0.0, -1.0, front_y,
        0.0, 1.0, 0.0, opening_z);
    const double wire_radius = std::clamp(
        std::min(opening_width, opening_height) * 0.007, 2.0, 5.0);
    std::vector<TopoDS_Shape> result;
    result.reserve(guides.size());
    for (const std::unique_ptr<CSmartLine>& guide : guides) {
        TopoDS_Wire sketch_wire;
        if (!guide || !BuildOpenSketchProfileWire(*guide, sketch_wire)) {
            continue;
        }
        BRepBuilderAPI_Transform placed(sketch_wire, placement, true);
        placed.Build();
        if (!placed.IsDone()) {
            continue;
        }
        const TopoDS_Wire path = TopoDS::Wire(placed.Shape());
        TopExp_Explorer edge_explorer(path, TopAbs_EDGE);
        if (!edge_explorer.More()) {
            continue;
        }
        const TopoDS_Edge first_edge = TopoDS::Edge(edge_explorer.Current());
        BRepAdaptor_Curve curve(first_edge);
        gp_Pnt start;
        gp_Vec tangent;
        curve.D1(curve.FirstParameter(), start, tangent);
        if (tangent.SquareMagnitude() <= 1.0e-12) {
            continue;
        }
        BRepBuilderAPI_MakeEdge circle_edge(
            gp_Circ(gp_Ax2(start, gp_Dir(tangent)), wire_radius));
        if (!circle_edge.IsDone()) {
            continue;
        }
        BRepBuilderAPI_MakeWire section(circle_edge.Edge());
        if (!section.IsDone()) {
            continue;
        }
        try {
            BRepOffsetAPI_MakePipe pipe(path, section.Wire());
            pipe.Build();
            if (pipe.IsDone()) {
                result.push_back(pipe.Shape());
            }
        } catch (const Standard_Failure&) {
            // A failed decorative guide must not invalidate the whole door.
        }
    }
    return result;
}

TopoDS_Shape cut_milled_panel(
        const TopoDS_Shape& panel,
        const std::vector<TopoDS_Shape>& cutter_sweeps) {
    if (panel.IsNull() || cutter_sweeps.empty()) {
        return panel;
    }
    std::vector<TopoDS_Shape> cutter_solids;
    for (const TopoDS_Shape& cutter : cutter_sweeps) {
        if (cutter.IsNull()) continue;
        for (TopExp_Explorer solid(cutter, TopAbs_SOLID);
             solid.More(); solid.Next()) {
            cutter_solids.push_back(solid.Current());
        }
    }
    if (cutter_solids.empty()) return panel;

    GProp_GProps source_properties;
    BRepGProp::VolumeProperties(panel, source_properties);
    TopoDS_Shape result = panel;
    for (const TopoDS_Shape& cutter : cutter_solids) {
        try {
            // Neighbouring Swept and Revolve bodies overlap by design. A
            // single Boolean against their Compound can classify the shared
            // faces inconsistently. Subtract the physical tool positions one
            // after another; this is equivalent to the real machining pass
            // and keeps each BOP small and robust.
            BRepAlgoAPI_Cut cut(result, cutter);
            cut.SetFuzzyValue(0.02);
            cut.SetRunParallel(false);
            cut.Build();
            if (!cut.IsDone() || cut.Shape().IsNull()
                || !TopExp_Explorer(cut.Shape(), TopAbs_SOLID).More()) {
                continue;
            }
            GProp_GProps candidate_properties;
            BRepGProp::VolumeProperties(
                cut.Shape(), candidate_properties);
            if (source_properties.Mass() > 1.0e-9
                && candidate_properties.Mass()
                    < source_properties.Mass() * 0.5) {
                continue;
            }
            result = cut.Shape();
        } catch (const Standard_Failure&) {
            // One failed tool position must not discard the already machined
            // valid portions of the facade.
        }
    }
    return result;
}

TopoDS_Shape milled_facade_shape(double x,
                                 double y,
                                 double z,
                                 double width,
                                 double thickness,
                                 double height,
                                 CSmartLine* source_cutter_debug) {
    TopoDS_Shape panel = fillet_front_edges(
        box_shape(x, y, z, width, thickness, height), y, 7.0);
    if (panel.IsNull()) {
        return {};
    }

    const double minimum_side = std::min(width, height);
    const double track_inset = std::clamp(
        minimum_side * 0.16, 45.0, 82.0);
    if (width <= 2.0 * (track_inset + 15.0)
        || height <= 2.0 * (track_inset + 15.0)) {
        return panel;
    }

    // Standard inputs are sketches too. They are fed through precisely the
    // same placement and Swept builder as catalog inputs.
    CSmartLine cutter_profile("Built-in Milled Cutter");
    cutter_profile.SetCoordinateSystem(
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
    constexpr double outside = -1.0;
    const auto point = [](double u, double depth) {
        return CPoint3d{u, -depth, 0.0};
    };
    cutter_profile.Add(new CLinkLine(
        point(-15.0, outside), point(-15.0, 9.0)));
    cutter_profile.Add(new CLinkLine(
        point(-15.0, 9.0), point(-14.0, 9.0)));
    cutter_profile.Add(new CLinkLine(
        point(-14.0, 9.0), point(-12.0, 7.0)));
    cutter_profile.Add(new CSketchArcLine(
        point(-12.0, 7.0), point(-9.94974747, 2.05025253),
        point(-5.0, 0.0)));
    cutter_profile.Add(new CLinkLine(
        point(-5.0, 0.0), point(5.0, 0.0)));
    cutter_profile.Add(new CSketchArcLine(
        point(5.0, 0.0), point(9.94974747, 2.05025253),
        point(12.0, 7.0)));
    cutter_profile.Add(new CLinkLine(
        point(12.0, 7.0), point(14.0, 9.0)));
    cutter_profile.Add(new CLinkLine(
        point(14.0, 9.0), point(15.0, 9.0)));
    cutter_profile.Add(new CLinkLine(
        point(15.0, 9.0), point(15.0, outside)));
    cutter_profile.Add(new CLinkLine(
        point(15.0, outside), point(-15.0, outside)));
    cutter_profile.SetClosed(true);

    CSmartLine guide("Built-in Milled Guide");
    guide.SetCoordinateSystem(
        {x + track_inset, y, z + track_inset},
        {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0});
    const double guide_width = width - 2.0 * track_inset;
    const double guide_height = height - 2.0 * track_inset;
    guide.Add(new CLinkLine({0.0, 0.0, 0.0},
                            {guide_width, 0.0, 0.0}));
    guide.Add(new CLinkLine({guide_width, 0.0, 0.0},
                            {guide_width, guide_height, 0.0}));
    guide.Add(new CLinkLine({guide_width, guide_height, 0.0},
                            {0.0, guide_height, 0.0}));
    guide.Add(new CLinkLine({0.0, guide_height, 0.0},
                            {0.0, 0.0, 0.0}));
    guide.SetClosed(true);
    if (source_cutter_debug) {
        // Show the source exactly as authored in its own local XY plane.
        // BuildSweptSolidShape will create and place a separate working copy.
        *source_cutter_debug = cutter_profile.MakeCopy();
        source_cutter_debug->SetName(
            "Built-in Milled Cutter XY Source (temporary)");
    }
    const TopoDS_Shape cutter = BuildSweptSolidShape(
        cutter_profile, guide, 2);
    if (cutter.IsNull()) {
        return panel;
    }
    return cut_milled_panel(panel, {cutter});
}

struct ProfiledFacadeCache {
    double width = 0.0;
    double thickness = 0.0;
    double height = 0.0;
    double frame_width = 0.0;
    TopoDS_Shape shape;

    bool Matches(double candidate_width,
                 double candidate_thickness,
                 double candidate_height,
                 double candidate_frame_width) const {
        constexpr double tolerance = 1.0e-9;
        return !shape.IsNull()
            && std::abs(width - candidate_width) <= tolerance
            && std::abs(thickness - candidate_thickness) <= tolerance
            && std::abs(height - candidate_height) <= tolerance
            && std::abs(frame_width - candidate_frame_width) <= tolerance;
    }
};

TopoDS_Shape cached_profiled_facade(bool milano,
                                    double width,
                                    double thickness,
                                    double height,
                                    double frame_width) {
    // A double-door cabinet used to run the same expensive pipe operation
    // twice.  Store one facade at a neutral position (front plane Y=0) and
    // place lightweight located copies for individual doors and rebuilds.
    static ProfiledFacadeCache frame_cache;
    static ProfiledFacadeCache milano_cache;
    ProfiledFacadeCache& cache = milano ? milano_cache : frame_cache;
    if (!cache.Matches(width, thickness, height, frame_width)) {
        const double neutral_y = -thickness;
        const TopoDS_Shape frame = milano
            ? milano_frame_shape(
                0.0, neutral_y, 0.0,
                width, thickness, height, frame_width)
            : frame_shape(
                0.0, neutral_y, 0.0,
                width, thickness, height, frame_width);
        if (frame.IsNull()) {
            return {};
        }
        const TopoDS_Shape panel = milano
            ? flat_center_panel_shape(
                0.0, neutral_y, 0.0,
                width, thickness, height, frame_width)
            : frame_center_panel_shape(
                0.0, neutral_y, 0.0,
                width, thickness, height, frame_width);
        cache.width = width;
        cache.thickness = thickness;
        cache.height = height;
        cache.frame_width = frame_width;
        cache.shape = compound_shape({frame, panel});
    }
    return cache.shape;
}

bool profile_face_and_bounds(const CSmartLine& profile,
                             TopoDS_Face& face,
                             double& min_x,
                             double& min_y,
                             double& max_x,
                             double& max_y) {
    Vec3 normal{};
    if (!profile.IsClosed()
        || !BuildSketchProfileFace(profile, face, normal)) {
        return false;
    }
    Bnd_Box bounds;
    BRepBndLib::Add(face, bounds);
    double min_z = 0.0;
    double max_z = 0.0;
    bounds.Get(min_x, min_y, min_z, max_x, max_y, max_z);
    return std::isfinite(min_x) && std::isfinite(min_y)
        && std::isfinite(max_x) && std::isfinite(max_y)
        && max_x - min_x > 1.0e-6 && max_y - min_y > 1.0e-6;
}

TopoDS_Shape rectangular_profile_rails(const TopoDS_Face& source,
                                       double source_min_x,
                                       double source_min_y,
                                       double scale,
                                       double rail_width,
                                       double x,
                                       double rear_y,
                                       double z,
                                       double width,
                                       double height) {
    std::vector<TopoDS_Shape> rails;
    rails.reserve(4);
    const auto miter_mask = [&](int side) {
        const double low_y = rear_y - 10000.0;
        BRepBuilderAPI_MakePolygon polygon;
        if (side == 0) { // bottom
            polygon.Add(gp_Pnt(x, low_y, z));
            polygon.Add(gp_Pnt(x + width, low_y, z));
            polygon.Add(gp_Pnt(x + width - rail_width, low_y, z + rail_width));
            polygon.Add(gp_Pnt(x + rail_width, low_y, z + rail_width));
        } else if (side == 1) { // top
            polygon.Add(gp_Pnt(x, low_y, z + height));
            polygon.Add(gp_Pnt(x + rail_width, low_y, z + height - rail_width));
            polygon.Add(gp_Pnt(x + width - rail_width, low_y, z + height - rail_width));
            polygon.Add(gp_Pnt(x + width, low_y, z + height));
        } else if (side == 2) { // left
            polygon.Add(gp_Pnt(x, low_y, z));
            polygon.Add(gp_Pnt(x + rail_width, low_y, z + rail_width));
            polygon.Add(gp_Pnt(x + rail_width, low_y, z + height - rail_width));
            polygon.Add(gp_Pnt(x, low_y, z + height));
        } else { // right
            polygon.Add(gp_Pnt(x + width, low_y, z));
            polygon.Add(gp_Pnt(x + width, low_y, z + height));
            polygon.Add(gp_Pnt(x + width - rail_width, low_y, z + height - rail_width));
            polygon.Add(gp_Pnt(x + width - rail_width, low_y, z + rail_width));
        }
        polygon.Close();
        if (!polygon.IsDone()) {
            return TopoDS_Shape{};
        }
        BRepBuilderAPI_MakeFace face(polygon.Wire(), true);
        if (!face.IsDone()) {
            return TopoDS_Shape{};
        }
        BRepPrimAPI_MakePrism prism(
            face.Face(), gp_Vec(0.0, 20000.0, 0.0), true);
        prism.Build();
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape{};
    };
    const auto add_rail = [&](const gp_Trsf& transform,
                              const gp_Vec& vector,
                              int side) {
        BRepBuilderAPI_Transform placed(source, transform, true);
        placed.Build();
        if (!placed.IsDone() || placed.Shape().ShapeType() != TopAbs_FACE) {
            return;
        }
        BRepPrimAPI_MakePrism prism(
            TopoDS::Face(placed.Shape()), vector, true);
        prism.Build();
        if (!prism.IsDone()) {
            return;
        }
        const TopoDS_Shape mask = miter_mask(side);
        if (mask.IsNull()) {
            return;
        }
        BRepAlgoAPI_Common trim(prism.Shape(), mask);
        trim.Build();
        if (trim.IsDone() && !trim.Shape().IsNull()) {
            rails.push_back(trim.Shape());
        }
    };

    // Bottom: U -> +Z, extrusion -> +X.
    gp_Trsf transform;
    transform.SetValues(
        0.0, 0.0, scale, x,
        0.0, -scale, 0.0, rear_y + scale * source_min_y,
        scale, 0.0, 0.0, z - scale * source_min_x);
    add_rail(transform, gp_Vec(width, 0.0, 0.0), 0);

    // Top: U -> -Z, extrusion -> +X.
    transform.SetValues(
        0.0, 0.0, -scale, x,
        0.0, -scale, 0.0, rear_y + scale * source_min_y,
        -scale, 0.0, 0.0, z + height + scale * source_min_x);
    add_rail(transform, gp_Vec(width, 0.0, 0.0), 1);

    // Left: U -> +X, extrusion -> +Z.
    transform.SetValues(
        scale, 0.0, 0.0, x - scale * source_min_x,
        0.0, -scale, 0.0, rear_y + scale * source_min_y,
        0.0, 0.0, -scale, z);
    add_rail(transform, gp_Vec(0.0, 0.0, height), 2);

    // Right: U -> -X, extrusion -> +Z.
    transform.SetValues(
        -scale, 0.0, 0.0, x + width + scale * source_min_x,
        0.0, -scale, 0.0, rear_y + scale * source_min_y,
        0.0, 0.0, scale, z);
    add_rail(transform, gp_Vec(0.0, 0.0, height), 3);

    return rails.size() == 4 ? compound_shape(rails) : TopoDS_Shape{};
}
}

CFacadeFurniture::CFacadeFurniture(std::string name)
    : CAssembled(std::move(name)) {}

CFacadeFurniture::CFacadeFurniture(
    std::string name,
    std::vector<unsigned long> element_ids)
    : CAssembled(std::move(name), std::move(element_ids)) {}

bool CFacadeFurniture::IsValidStyle(KitchenCabinetFacadeStyle style) {
    return style == KitchenCabinetFacadeStyle::Plain
        || style == KitchenCabinetFacadeStyle::Frame
        || style == KitchenCabinetFacadeStyle::Screen
        || style == KitchenCabinetFacadeStyle::Milled
        || style == KitchenCabinetFacadeStyle::Milano;
}

TopoDS_Shape CFacadeFurniture::BuildPlanarShape(
    KitchenCabinetFacadeStyle style,
    double x,
    double y,
    double z,
    double width,
    double thickness,
    double height,
    bool round_screen_front,
    KitchenCabinetShowcaseFill showcase_fill,
    CSmartLine* source_cutter_debug) {
    if (style == KitchenCabinetFacadeStyle::Plain) {
        return box_shape(x, y, z, width, thickness, height);
    }

    const double minimum_side = std::min(width, height);
    double frame_width = std::clamp(minimum_side * 0.13, 32.0, 72.0);
    if (width <= 2.0 * frame_width + 2.0
        || height <= 2.0 * frame_width + 2.0) {
        return box_shape(x, y, z, width, thickness, height);
    }

    if (style == KitchenCabinetFacadeStyle::Milled) {
        return milled_facade_shape(
            x, y, z, width, thickness, height,
            source_cutter_debug);
    }

    TopoDS_Shape profiled_showcase_frame;
    if (style == KitchenCabinetFacadeStyle::Frame) {
        const double profile_frame_width = std::min(
            60.0, std::max(20.0, minimum_side * 0.5 - 2.0));
        const TopoDS_Shape profiled = cached_profiled_facade(
            false, width, thickness, height, profile_frame_width);
        if (showcase_fill == KitchenCabinetShowcaseFill::None) {
            return translated_shape(profiled, x, y + thickness, z);
        }
        // A showcase Frame uses the same profiled rails as a regular Frame,
        // but deliberately omits its wooden centre panel.  The glass or
        // decorative showcase fill is added below as a separate part.
        if (profiled.ShapeType() == TopAbs_COMPOUND) {
            TopoDS_Iterator iterator(profiled);
            if (iterator.More()) {
                profiled_showcase_frame = translated_shape(
                    iterator.Value(), x, y + thickness, z);
            }
        }
        if (profiled_showcase_frame.IsNull()) {
            return {};
        }
        frame_width = profile_frame_width;
    }

    if (style == KitchenCabinetFacadeStyle::Milano) {
        const double profile_frame_width = std::min(
            60.0, std::max(20.0, minimum_side * 0.5 - 2.0));
        const TopoDS_Shape facade = cached_profiled_facade(
            true, width, thickness, height, profile_frame_width);
        if (facade.IsNull()) {
            return box_shape(x, y, z, width, thickness, height);
        }
        return translated_shape(facade, x, y + thickness, z);
    }

    std::vector<TopoDS_Shape> shapes;
    const double panel_depth = thickness * 0.30;
    const double opening_x = x + frame_width;
    const double opening_z = z + frame_width;
    const double opening_width = width - 2.0 * frame_width;
    const double opening_height = height - 2.0 * frame_width;
    const double fill_y = y + thickness - panel_depth;
    if (showcase_fill != KitchenCabinetShowcaseFill::Lattice
        && showcase_fill != KitchenCabinetShowcaseFill::None) {
        shapes.push_back(box_shape(
            opening_x, fill_y, opening_z,
            opening_width, panel_depth, opening_height));
    }

    const double cx = opening_x + opening_width * 0.5;
    const double cz = opening_z + opening_height * 0.5;
    const double bar_depth = std::max(2.0, thickness * 0.45);
    const double bar_width = std::clamp(minimum_side * 0.025, 8.0, 18.0);
    if (showcase_fill == KitchenCabinetShowcaseFill::MuntinBars) {
        // Three internal bars in each direction divide the glass into a
        // regular 4 x 4 field, matching classic furniture muntin layouts.
        for (int divider = 1; divider <= 3; ++divider) {
            const double fraction = static_cast<double>(divider) / 4.0;
            shapes.push_back(showcase_bar(
                cx, y,
                opening_z + opening_height * fraction,
                opening_width, bar_depth, bar_width, 0.0,
                opening_x, opening_z, opening_width, opening_height));
            shapes.push_back(showcase_bar(
                opening_x + opening_width * fraction,
                y, cz,
                opening_height, bar_depth, bar_width,
                3.14159265358979323846 * 0.5,
                opening_x, opening_z, opening_width, opening_height));
        }
    } else if (showcase_fill == KitchenCabinetShowcaseFill::Lattice) {
        const double diagonal = std::hypot(opening_width, opening_height) * 1.4;
        // A double-door leaf is much narrower than a single door.  Extend
        // both diagonal families by another three members on either side so
        // the unchanged lattice pitch still reaches its upper/lower corners.
        const int lattice_extent = opening_height > opening_width * 2.0
            ? 9
            : 6;
        for (int line = -lattice_extent; line <= lattice_extent; ++line) {
            const double offset = line * opening_width / 6.0;
            shapes.push_back(showcase_bar(
                cx + offset, y, cz, diagonal, bar_depth, bar_width,
                0.72, opening_x, opening_z,
                opening_width, opening_height));
            shapes.push_back(showcase_bar(
                cx + offset, y, cz, diagonal, bar_depth, bar_width,
                -0.72, opening_x, opening_z,
                opening_width, opening_height));
        }
    } else if (showcase_fill == KitchenCabinetShowcaseFill::StainedGlass) {
        std::vector<TopoDS_Shape> stained_wires = stained_louisiana_wires(
            opening_x,
            y - std::max(0.5, thickness * 0.04),
            opening_z,
            opening_width,
            opening_height);
        TopoDS_Shape stained_pattern = compound_shape(stained_wires);
        if (!stained_pattern.IsNull()) {
            shapes.push_back(std::move(stained_pattern));
        }
    }
    if (profiled_showcase_frame.IsNull()) {
        add_rectangular_border(
            shapes, x, y, z, width, thickness, height, frame_width,
            round_screen_front ? 2.0 : 0.0);
    } else {
        shapes.push_back(std::move(profiled_showcase_frame));
    }
    return compound_shape(shapes);
}

TopoDS_Shape CFacadeFurniture::BuildPlanarFrameFromProfiles(
    const CSmartLine& frame_profile,
    const CSmartLine& panel_profile,
    double x,
    double y,
    double z,
    double width,
    double thickness,
    double height) {
    TopoDS_Face frame_source;
    TopoDS_Face panel_source;
    double frame_min_x = 0.0;
    double frame_min_y = 0.0;
    double frame_max_x = 0.0;
    double frame_max_y = 0.0;
    double panel_min_x = 0.0;
    double panel_min_y = 0.0;
    double panel_max_x = 0.0;
    double panel_max_y = 0.0;
    if (!profile_face_and_bounds(
            frame_profile, frame_source,
            frame_min_x, frame_min_y, frame_max_x, frame_max_y)
        || !profile_face_and_bounds(
            panel_profile, panel_source,
            panel_min_x, panel_min_y, panel_max_x, panel_max_y)) {
        return {};
    }

    const double minimum_side = std::min(width, height);
    const double frame_source_width = frame_max_x - frame_min_x;
    const double maximum_frame_width = minimum_side * 0.5 - 2.0;
    if (maximum_frame_width <= 2.0) {
        return {};
    }
    const double frame_scale = std::min(1.0,
        maximum_frame_width / frame_source_width);
    const double frame_width = frame_source_width * frame_scale;
    const double rear_y = y + thickness;
    const TopoDS_Shape frame = rectangular_profile_rails(
        frame_source, frame_min_x, frame_min_y,
        frame_scale, frame_width, x, rear_y, z, width, height);
    if (frame.IsNull()) {
        return {};
    }

    const double overlap = std::clamp(frame_width * 0.16, 5.0, 10.0);
    const double panel_inset = std::max(1.0, frame_width - overlap);
    const double panel_source_width = panel_max_x - panel_min_x;
    const double available_panel_half =
        std::min(width, height) * 0.5 - panel_inset - 2.0;
    const double panel_scale = std::min(
        frame_scale,
        std::max(0.01, available_panel_half / panel_source_width));
    const double panel_band_width = panel_source_width * panel_scale;
    const double panel_x = x + panel_inset;
    const double panel_z = z + panel_inset;
    const double panel_width = width - 2.0 * panel_inset;
    const double panel_height = height - 2.0 * panel_inset;
    if (panel_width <= 2.0 * panel_band_width + 1.0
        || panel_height <= 2.0 * panel_band_width + 1.0) {
        return {};
    }
    const TopoDS_Shape panel_band = rectangular_profile_rails(
        panel_source, panel_min_x, panel_min_y,
        panel_scale, panel_band_width, panel_x, rear_y, panel_z,
        panel_width, panel_height);
    if (panel_band.IsNull()) {
        return {};
    }

    const double panel_relief = (panel_max_y - panel_min_y) * panel_scale;
    const TopoDS_Shape plateau = box_shape(
        panel_x + panel_band_width,
        rear_y - panel_relief,
        panel_z + panel_band_width,
        panel_width - 2.0 * panel_band_width,
        panel_relief,
        panel_height - 2.0 * panel_band_width);
    TopoDS_Shape panel = panel_band;
    if (!plateau.IsNull()) {
        BRepAlgoAPI_Fuse fuse(panel_band, plateau);
        fuse.Build();
        if (fuse.IsDone()) {
            panel = fuse.Shape();
        }
    }
    return compound_shape({frame, panel});
}

TopoDS_Shape CFacadeFurniture::BuildPlanarMilledFromProfiles(
    const CSmartLine& cutter_profile,
    const std::vector<const CSmartLine*>& guides,
    double x,
    double y,
    double z,
    double width,
    double thickness,
    double height,
    CSmartLine* source_cutter_debug,
    bool require_evolved,
    TopoDS_Shape* cutter_volume_debug,
    double cutting_depth) {
    TopoDS_Shape panel = fillet_front_edges(
        box_shape(x, y, z, width, thickness, height), y, 7.0);
    if (panel.IsNull() || !cutter_profile.IsClosed() || guides.empty()) {
        return panel;
    }

    const CSmartLine* reference_guide = guides.front();
    if (!reference_guide) {
        return panel;
    }
    const SketchCoordinateSystem& reference =
        reference_guide->GetCoordinateSystem();
    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    for (const CSmartLine* guide : guides) {
        if (!guide) {
            continue;
        }
        for (const CPoint3d& world : guide->GetProfilePointsWorld()) {
            const CPoint3d local = reference_guide->WorldToLocal(world);
            min_x = std::min(min_x, local.x);
            max_x = std::max(max_x, local.x);
            min_y = std::min(min_y, local.y);
            max_y = std::max(max_y, local.y);
        }
    }
    if (min_x == std::numeric_limits<double>::max()) {
        return panel;
    }

    const double pattern_width = max_x - min_x;
    const double pattern_height = max_y - min_y;
    const double inset = std::clamp(
        std::min(width, height) * 0.12, 30.0, 70.0);
    const double scale_x = pattern_width > 1.0e-9
        ? std::max(1.0, width - 2.0 * inset) / pattern_width : 1.0;
    const double scale_y = pattern_height > 1.0e-9
        ? std::max(1.0, height - 2.0 * inset) / pattern_height : 1.0;
    const double pattern_scale_x = std::max(1.0e-6, scale_x);
    const double pattern_scale_y = std::max(1.0e-6, scale_y);
    const CPoint3d target_reference_origin{
        x + (width - pattern_scale_x * (min_x + max_x)) * 0.5,
        y - 0.1,
        z + (height - pattern_scale_y * (min_y + max_y)) * 0.5};
    const auto dot = [](const CPoint3d& a, const CPoint3d& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };

    // Physical catalog cutters use the cutting-tip datum at Y=0. Their
    // authored body may extend toward either local Y direction; the placement
    // resolver keeps the shank outside and advances the tip to cutting depth.
    const double safe_maximum_depth = std::max(0.1, thickness - 0.5);
    cutting_depth = std::clamp(
        cutting_depth, 0.1, safe_maximum_depth);
    const MillingCutterPlacement cutter_placement =
        ResolveMillingCutterPlacement(cutter_profile);
    const double cutter_delta_y = cutter_placement.delta_y
        - (cutter_placement.tip_at_cutting_depth ? cutting_depth : 0.0);

    std::vector<TopoDS_Shape> cutter_sweeps;
    cutter_sweeps.reserve(guides.size());
    for (const CSmartLine* source : guides) {
        if (!source) {
            continue;
        }
        CSmartLine placed = source->MakeCopy();
        placed.ScaleLocal(pattern_scale_x, pattern_scale_y);
        const SketchCoordinateSystem& system = source->GetCoordinateSystem();
        const CPoint3d relative_origin =
            reference_guide->WorldToLocal(system.origin);
        const CPoint3d target_origin{
            target_reference_origin.x + pattern_scale_x * relative_origin.x,
            target_reference_origin.y,
            target_reference_origin.z + pattern_scale_y * relative_origin.y};
        const CPoint3d target_x_axis{
            dot(system.x_axis, reference.x_axis),
            0.0,
            dot(system.x_axis, reference.y_axis)};
        if (!placed.SetCoordinateSystem(
                target_origin, target_x_axis, CPoint3d{0.0, -1.0, 0.0})) {
            continue;
        }
        if (source_cutter_debug && cutter_sweeps.empty()) {
            // The catalog object is also consumed in its own local XY system.
            // Keep an unmodified source copy for direct comparison with the
            // built-in cutter; placement happens only inside Swept.
            *source_cutter_debug = cutter_profile.MakeCopy();
            source_cutter_debug->SetName(
                "Catalog Milled Cutter XY Source (temporary)");
        }
        TopoDS_Shape cutter = BuildSweptSolidShape(
            cutter_profile, placed, 2,
            0.0, cutter_delta_y,
            cutter_placement.angle_degrees, {}, {}, require_evolved);
        if (!cutter.IsNull()) {
            if (cutter_volume_debug && cutter_volume_debug->IsNull()) {
                *cutter_volume_debug = cutter;
            }
            TopoDS_Shape limited_cutter = LimitMillingCutterToFacadeDepth(
                cutter, panel, cutting_depth);
            if (!limited_cutter.IsNull()) {
                cutter_sweeps.push_back(std::move(limited_cutter));
            }
        }
    }
    return cut_milled_panel(panel, cutter_sweeps);
}
