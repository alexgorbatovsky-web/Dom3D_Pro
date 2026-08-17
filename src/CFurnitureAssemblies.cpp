#include "CFurnitureAssemblies.h"

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

#include "CFurnitureDrawer.h"
#include "CKitchenCabinet.h"
#include "solid/Solid.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
TopoDS_Shape furniture_box(double x, double y, double z,
                           double width, double depth, double height) {
    BRepPrimAPI_MakeBox builder(gp_Pnt(x, y, z), width, depth, height);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape corner_worktop_shape(double center_x, double center_y,
                                  double z, double arm_depth,
                                  double height,
                                  double requested_radius) {
    // Match the straight Nika worktops: 25 mm at the two facade lines,
    // 15 mm at the wall/joint sides and a tiny overlap at both joints.
    // The rounded inner plan corner produces three continuous front pieces:
    // horizontal, quarter-circle and vertical.
    const double back = arm_depth + 15.0;
    const double joint = -arm_depth + 13.0;
    const double front = -25.0;
    const double plan_radius = std::clamp(
        requested_radius, 0.0, std::max(0.0, arm_depth * 0.25));
    const double tangent = front - plan_radius;

    try {
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(center_x + back, center_y + back, z),
            gp_Pnt(center_x + joint, center_y + back, z)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(center_x + joint, center_y + back, z),
            gp_Pnt(center_x + joint, center_y + front, z)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(center_x + joint, center_y + front, z),
            gp_Pnt(center_x + tangent, center_y + front, z)).Edge());

        if (plan_radius > 1.0e-6) {
            constexpr double inv_sqrt_two = 0.7071067811865475;
            GC_MakeArcOfCircle inner_arc(
                gp_Pnt(center_x + tangent, center_y + front, z),
                gp_Pnt(center_x + front
                           - plan_radius * (1.0 - inv_sqrt_two),
                       center_y + front
                           - plan_radius * (1.0 - inv_sqrt_two), z),
                gp_Pnt(center_x + front, center_y + tangent, z));
            if (!inner_arc.IsDone()) return {};
            wire.Add(BRepBuilderAPI_MakeEdge(inner_arc.Value()).Edge());
        }
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(center_x + front, center_y + tangent, z),
            gp_Pnt(center_x + front, center_y + joint, z)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(center_x + front, center_y + joint, z),
            gp_Pnt(center_x + back, center_y + joint, z)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(center_x + back, center_y + joint, z),
            gp_Pnt(center_x + back, center_y + back, z)).Edge());
        wire.Build();
        if (!wire.IsDone()) return {};

        BRepBuilderAPI_MakeFace face(wire.Wire());
        face.Build();
        if (!face.IsDone()) return {};
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
        prism.Build();
        if (!prism.IsDone()) return {};

        TopoDS_Shape shape = prism.Shape();
        const double edge_radius = std::clamp(
            requested_radius, 0.0, std::max(0.0, height * 0.48));
        if (edge_radius <= 1.0e-6) return shape;

        // Round the top and bottom edges of all three exposed front pieces.
        BRepFilletAPI_MakeFillet fillet(shape);
        int selected_edges = 0;
        constexpr double tolerance = 1.0e-4;
        for (TopExp_Explorer explorer(shape, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            TopoDS_Vertex first_vertex;
            TopoDS_Vertex last_vertex;
            TopExp::Vertices(edge, first_vertex, last_vertex);
            if (first_vertex.IsNull() || last_vertex.IsNull()) continue;
            const gp_Pnt first_point = BRep_Tool::Pnt(first_vertex);
            const gp_Pnt last_point = BRep_Tool::Pnt(last_vertex);
            const bool bottom_edge = std::abs(first_point.Z() - z) < tolerance
                && std::abs(last_point.Z() - z) < tolerance;
            const bool top_edge = std::abs(first_point.Z() - z - height) < tolerance
                && std::abs(last_point.Z() - z - height) < tolerance;
            if (!bottom_edge && !top_edge) continue;
            const bool horizontal_front =
                std::abs(first_point.Y() - center_y - front) < tolerance
                && std::abs(last_point.Y() - center_y - front) < tolerance;
            const bool vertical_front =
                std::abs(first_point.X() - center_x - front) < tolerance
                && std::abs(last_point.X() - center_x - front) < tolerance;
            const bool inner_curve =
                std::abs(first_point.X() - center_x - tangent) < tolerance
                && std::abs(first_point.Y() - center_y - front) < tolerance
                && std::abs(last_point.X() - center_x - front) < tolerance
                && std::abs(last_point.Y() - center_y - tangent) < tolerance;
            const bool reverse_inner_curve =
                std::abs(last_point.X() - center_x - tangent) < tolerance
                && std::abs(last_point.Y() - center_y - front) < tolerance
                && std::abs(first_point.X() - center_x - front) < tolerance
                && std::abs(first_point.Y() - center_y - tangent) < tolerance;
            if (horizontal_front || vertical_front
                || inner_curve || reverse_inner_curve) {
                fillet.Add(edge_radius, edge);
                ++selected_edges;
            }
        }
        if (selected_edges > 0) {
            fillet.Build();
            if (fillet.IsDone()) return fillet.Shape();
        }
        return shape;
    } catch (...) {
        return {};
    }
}

TopoDS_Shape rounded_worktop_shape(double x, double y, double z,
                                   double width, double depth, double height,
                                   double requested_radius) {
    const double radius = std::clamp(
        requested_radius, 0.0, std::max(0.0, height * 0.48));
    if (radius <= 1.0e-6) {
        return furniture_box(x, y, z, width, depth, height);
    }
    try {
        const double back = y + depth;
        const gp_Pnt top_front(x, y + radius, z + height);
        const gp_Pnt top_back(x, back, z + height);
        const gp_Pnt bottom_back(x, back, z);
        const gp_Pnt bottom_front(x, y + radius, z);
        const gp_Pnt lower_front(x, y, z + radius);
        const gp_Pnt upper_front(x, y, z + height - radius);
        constexpr double inv_sqrt_two = 0.7071067811865475;
        const gp_Pnt lower_arc_mid(
            x, y + radius * (1.0 - inv_sqrt_two),
            z + radius * (1.0 - inv_sqrt_two));
        const gp_Pnt upper_arc_mid(
            x, y + radius * (1.0 - inv_sqrt_two),
            z + height - radius * (1.0 - inv_sqrt_two));

        GC_MakeArcOfCircle lower_arc(
            bottom_front, lower_arc_mid, lower_front);
        GC_MakeArcOfCircle upper_arc(
            upper_front, upper_arc_mid, top_front);
        if (!lower_arc.IsDone() || !upper_arc.IsDone()) {
            return furniture_box(x, y, z, width, depth, height);
        }
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(top_front, top_back).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(top_back, bottom_back).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(bottom_back, bottom_front).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(lower_arc.Value()).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(lower_front, upper_front).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(upper_arc.Value()).Edge());
        wire.Build();
        if (!wire.IsDone()) {
            return furniture_box(x, y, z, width, depth, height);
        }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        face.Build();
        if (!face.IsDone()) {
            return furniture_box(x, y, z, width, depth, height);
        }
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(width, 0.0, 0.0));
        prism.Build();
        return prism.IsDone()
            ? prism.Shape()
            : furniture_box(x, y, z, width, depth, height);
    } catch (...) {
        return furniture_box(x, y, z, width, depth, height);
    }
}

TopoDS_Shape move_furniture_shape(const TopoDS_Shape& shape,
                                  double pullout,
                                  double hinge_x,
                                  double hinge_y,
                                  double angle_degrees) {
    if (shape.IsNull()) {
        return {};
    }
    TopoDS_Shape result = shape;
    if (std::abs(angle_degrees) > 1.0e-9) {
        gp_Trsf rotation;
        rotation.SetRotation(
            gp_Ax1(gp_Pnt(hinge_x, hinge_y, 0.0), gp_Dir(0.0, 0.0, 1.0)),
            angle_degrees * 3.14159265358979323846 / 180.0);
        BRepBuilderAPI_Transform transformed(result, rotation, true);
        transformed.Build();
        if (!transformed.IsDone()) {
            return {};
        }
        result = transformed.Shape();
    }
    if (pullout > 1.0e-9) {
        gp_Trsf translation;
        translation.SetTranslation(gp_Vec(0.0, -pullout, 0.0));
        BRepBuilderAPI_Transform transformed(result, translation, true);
        transformed.Build();
        if (!transformed.IsDone()) {
            return {};
        }
        result = transformed.Shape();
    }
    return result;
}

std::unique_ptr<CSolid> furniture_solid(
    const std::string& name, TopoDS_Shape shape, Color color) {
    if (shape.IsNull()) {
        return nullptr;
    }
    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName(name);
    solid->SetColor(color);
    // Furniture generation creates and places the BRep only.  Triangulating
    // here used to mesh every corner-kitchen part once before placement and
    // once again after placement.  Keep only the lightweight face metadata
    // needed by material assignment; the display mesh is built on first Draw.
    return solid->InitSurfaces() ? std::move(solid) : nullptr;
}

bool has_invalid_part(const std::vector<std::unique_ptr<CAlfaObject>>& parts) {
    return std::any_of(parts.begin(), parts.end(),
        [](const std::unique_ptr<CAlfaObject>& part) { return !part; });
}

TopoDS_Wire rectangular_section(double x, double y, double z,
                                double width, double depth) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(x, y, z));
    polygon.Add(gp_Pnt(x + width, y, z));
    polygon.Add(gp_Pnt(x + width, y + depth, z));
    polygon.Add(gp_Pnt(x, y + depth, z));
    polygon.Close();
    return polygon.IsDone() ? polygon.Wire() : TopoDS_Wire{};
}

TopoDS_Wire oriented_rectangular_section(
    double center_x, double center_y, double z,
    double width, double depth,
    double tangent_x, double tangent_y,
    double plane_slope_x, double plane_slope_y) {
    const double tangent_length = std::sqrt(
        tangent_x * tangent_x + tangent_y * tangent_y);
    if (tangent_length <= 1.0e-12 || width <= 1.0e-6
        || depth <= 1.0e-6) {
        return {};
    }

    tangent_x /= tangent_length;
    tangent_y /= tangent_length;
    const double normal_x = -tangent_y;
    const double normal_y = tangent_x;
    const auto point = [&](double width_offset, double depth_offset) {
        const double x = center_x
            + tangent_x * width_offset + normal_x * depth_offset;
        const double y = center_y
            + tangent_y * width_offset + normal_y * depth_offset;
        return gp_Pnt(x, y,
            z + plane_slope_x * (x - center_x)
              + plane_slope_y * (y - center_y));
    };

    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(point(-width * 0.5, -depth * 0.5));
    polygon.Add(point( width * 0.5, -depth * 0.5));
    polygon.Add(point( width * 0.5,  depth * 0.5));
    polygon.Add(point(-width * 0.5,  depth * 0.5));
    polygon.Close();
    return polygon.IsDone() ? polygon.Wire() : TopoDS_Wire{};
}

TopoDS_Shape tapered_vertical_member(double bottom_x, double bottom_y,
                                     double top_x, double top_y,
                                     double z0, double z1,
                                     double bottom_width, double bottom_depth,
                                     double top_width, double top_depth) {
    try {
        const TopoDS_Wire bottom = rectangular_section(
            bottom_x, bottom_y, z0, bottom_width, bottom_depth);
        const TopoDS_Wire top = rectangular_section(
            top_x, top_y, z1, top_width, top_depth);
        if (bottom.IsNull() || top.IsNull()) {
            return {};
        }
        BRepOffsetAPI_ThruSections loft(true, false, 1.0e-7);
        loft.CheckCompatibility(true);
        loft.AddWire(bottom);
        loft.AddWire(top);
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
    } catch (...) {
        return {};
    }
}

TopoDS_Shape oriented_vertical_member(
    double center_x,
    double bottom_center_y, double top_center_y,
    double z0, double z1,
    double width, double depth,
    double tangent_x, double tangent_y,
    double bottom_plane_slope_x, double bottom_plane_slope_y,
    double top_plane_slope_x, double top_plane_slope_y) {
    try {
        const TopoDS_Wire bottom = oriented_rectangular_section(
            center_x, bottom_center_y, z0, width, depth,
            tangent_x, tangent_y,
            bottom_plane_slope_x, bottom_plane_slope_y);
        const TopoDS_Wire top = oriented_rectangular_section(
            center_x, top_center_y, z1, width, depth,
            tangent_x, tangent_y,
            top_plane_slope_x, top_plane_slope_y);
        if (bottom.IsNull() || top.IsNull()) {
            return {};
        }
        BRepOffsetAPI_ThruSections loft(true, false, 1.0e-7);
        loft.CheckCompatibility(false);
        loft.SetSmoothing(false);
        loft.AddWire(bottom);
        loft.AddWire(top);
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
    } catch (...) {
        return {};
    }
}

double height_arch_factor(double z, double total_height) {
    if (std::abs(total_height) <= 1.0e-9) {
        return 0.0;
    }
    const double t = std::clamp(z / total_height, 0.0, 1.0);
    return 4.0 * t * (1.0 - t);
}

double height_arch_derivative(double z, double total_height) {
    if (std::abs(total_height) <= 1.0e-9) {
        return 0.0;
    }
    const double t = std::clamp(z / total_height, 0.0, 1.0);
    return 4.0 * (1.0 - 2.0 * t) / total_height;
}

TopoDS_Shape curved_tapered_vertical_member(
    double bottom_x, double bottom_y,
    double top_x, double top_y,
    double z0, double z1,
    double bottom_width, double bottom_depth,
    double top_width, double top_depth,
    double side_curve) {
    if (z1 - z0 <= 1.0e-6 || bottom_width <= 1.0e-6
        || bottom_depth <= 1.0e-6 || top_width <= 1.0e-6
        || top_depth <= 1.0e-6 || !std::isfinite(side_curve)) {
        return {};
    }

    try {
        BRepOffsetAPI_ThruSections loft(true, false, 1.0e-7);
        loft.CheckCompatibility(false);
        loft.SetSmoothing(false);
        loft.SetMaxDegree(3);
        constexpr int kSections = 9;
        for (int index = 0; index < kSections; ++index) {
            const double t = static_cast<double>(index)
                / static_cast<double>(kSections - 1);
            const double arch = 4.0 * t * (1.0 - t);
            const double x = bottom_x + (top_x - bottom_x) * t;
            const double y = bottom_y + (top_y - bottom_y) * t
                + side_curve * arch;
            const double z = z0 + (z1 - z0) * t;
            const double width = bottom_width
                + (top_width - bottom_width) * t;
            const double depth = bottom_depth
                + (top_depth - bottom_depth) * t;
            const TopoDS_Wire section = rectangular_section(
                x, y, z, width, depth);
            if (section.IsNull()) {
                return {};
            }
            loft.AddWire(section);
        }
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
    } catch (...) {
        return {};
    }
}

TopoDS_Shape rounded_shape(TopoDS_Shape shape, double radius) {
    if (shape.IsNull() || radius <= 1.0e-6) {
        return shape;
    }
    try {
        BRepFilletAPI_MakeFillet fillet(shape);
        for (TopExp_Explorer edges(shape, TopAbs_EDGE); edges.More(); edges.Next()) {
            fillet.Add(radius, TopoDS::Edge(edges.Current()));
        }
        fillet.Build();
        return fillet.IsDone() ? fillet.Shape() : shape;
    } catch (...) {
        return shape;
    }
}

TopoDS_Shape rounded_box(double x, double y, double z,
                        double width, double depth, double height,
                        double radius) {
    return rounded_shape(
        furniture_box(x, y, z, width, depth, height), radius);
}

TopoDS_Shape curved_back_seat(double left, double front, double bottom,
                              double width, double depth, double height,
                              double back_curve, double edge_radius) {
    if (width <= 1.0e-6 || depth <= 1.0e-6 || height <= 1.0e-6
        || !std::isfinite(back_curve)) {
        return {};
    }

    const double curve = std::clamp(
        back_curve, -depth * 0.45, depth * 0.45);
    if (std::abs(curve) <= 1.0e-4) {
        return rounded_box(
            left, front, bottom, width, depth, height, edge_radius);
    }

    try {
        const double right = left + width;
        const double back = front + depth;
        const gp_Pnt front_left(left, front, bottom);
        const gp_Pnt front_right(right, front, bottom);
        // Keep both rear corners on the original seat boundary so the seat
        // still meets the back posts.  The parameter controls only the crown
        // at the middle of the rear edge (positive values bow backwards).
        const gp_Pnt back_right(right, back, bottom);
        const gp_Pnt back_middle(left + width * 0.5, back + curve, bottom);
        const gp_Pnt back_left(left, back, bottom);

        GC_MakeArcOfCircle arc_builder(
            back_right, back_middle, back_left);
        if (!arc_builder.IsDone()) {
            return {};
        }

        BRepBuilderAPI_MakeWire wire_builder;
        wire_builder.Add(BRepBuilderAPI_MakeEdge(
            front_left, front_right).Edge());
        wire_builder.Add(BRepBuilderAPI_MakeEdge(
            front_right, back_right).Edge());
        wire_builder.Add(BRepBuilderAPI_MakeEdge(
            arc_builder.Value()).Edge());
        wire_builder.Add(BRepBuilderAPI_MakeEdge(
            back_left, front_left).Edge());
        if (!wire_builder.IsDone() || wire_builder.Wire().IsNull()) {
            return {};
        }

        BRepBuilderAPI_MakeFace face_builder(wire_builder.Wire());
        if (!face_builder.IsDone()) {
            return {};
        }
        BRepPrimAPI_MakePrism prism_builder(
            face_builder.Face(), gp_Vec(0.0, 0.0, height));
        prism_builder.Build();
        if (!prism_builder.IsDone()) {
            return {};
        }
        return rounded_shape(prism_builder.Shape(), edge_radius);
    } catch (...) {
        return {};
    }
}

TopoDS_Shape curved_back_rail(double left, double right,
                             double center_z, double height, double depth,
                             double curve, double depth_curve, double back_y,
                             double rake, double post_curve,
                             double total_height) {
	if (!std::isfinite(left) || !std::isfinite(right)
		|| !std::isfinite(center_z) || !std::isfinite(height)
		|| !std::isfinite(depth) || !std::isfinite(curve)
		|| !std::isfinite(depth_curve) || !std::isfinite(back_y)
		|| !std::isfinite(rake) || !std::isfinite(post_curve)
		|| !std::isfinite(total_height)
		|| right - left <= 1.0e-6 || height <= 1.0e-6
		|| depth <= 1.0e-6 || std::abs(total_height) <= 1.0e-6) {
		return {};
	}
    try {
        BRepOffsetAPI_ThruSections loft(true, false, 1.0e-7);
		// Every section is created with the same four edges and orientation.
		// Compatibility rewriting and variational smoothing are unnecessary
		// here.  On a rail curved in both Y and Z, that combination may leave a
		// null intermediate edge which OCCT later dereferences in
		// EncodeRegularity().  The ordinary interpolating loft remains smooth.
		loft.CheckCompatibility(false);
		loft.SetSmoothing(false);
		loft.SetMaxDegree(3);
        constexpr int kSections = 7;
        for (int index = 0; index < kSections; ++index) {
            const double t = static_cast<double>(index)
                / static_cast<double>(kSections - 1);
            const double x = left + (right - left) * t;
            const double arch = curve * (1.0 - std::pow(2.0 * t - 1.0, 2.0));
            const double depth_arch = depth_curve
                * (1.0 - std::pow(2.0 * t - 1.0, 2.0));
            const double z = center_z + arch;
            const double y = back_y + rake * (z / total_height)
                + post_curve * height_arch_factor(z, total_height)
                + depth_arch;

            // Rotate the YZ section to the local tangent of the rear post.
            // With a straight upright post this reduces to the old axis-
            // aligned rectangle.  With rake or curvature it prevents the
            // rail ends from protruding through the posts in side view.
            const double dy_dz = rake / total_height
                + post_curve * height_arch_derivative(z, total_height);
            const double tangent_length = std::sqrt(1.0 + dy_dz * dy_dz);
            const double height_y = dy_dz / tangent_length;
            const double height_z = 1.0 / tangent_length;
            const double depth_y = height_z;
            const double depth_z = -height_y;
            const auto section_point = [&](double depth_offset,
                                           double height_offset) {
                return gp_Pnt(
                    x,
                    y + depth_y * depth_offset + height_y * height_offset,
                    z + depth_z * depth_offset + height_z * height_offset);
            };
            BRepBuilderAPI_MakePolygon section;
            section.Add(section_point(-depth * 0.5, -height * 0.5));
            section.Add(section_point( depth * 0.5, -height * 0.5));
            section.Add(section_point( depth * 0.5,  height * 0.5));
            section.Add(section_point(-depth * 0.5,  height * 0.5));
            section.Close();
            if (!section.IsDone()) {
                return {};
            }
			const TopoDS_Wire wire = section.Wire();
			if (wire.IsNull()) {
				return {};
			}
			loft.AddWire(wire);
        }
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
    } catch (...) {
        return {};
    }
}

TopoDS_Shape curved_back_panel(double left, double right,
                              double bottom_z, double top_z,
                              double depth, double lower_curve,
                              double upper_curve, double depth_curve,
                              double back_center_y,
                              double rake, double post_curve,
                              double total_height) {
	if (!std::isfinite(left) || !std::isfinite(right)
		|| !std::isfinite(bottom_z) || !std::isfinite(top_z)
		|| !std::isfinite(depth) || !std::isfinite(lower_curve)
		|| !std::isfinite(upper_curve) || !std::isfinite(depth_curve)
		|| !std::isfinite(back_center_y) || !std::isfinite(rake)
		|| !std::isfinite(post_curve)
		|| !std::isfinite(total_height) || right - left <= 1.0e-6
		|| top_z - bottom_z <= 1.0e-6 || depth <= 1.0e-6
		|| std::abs(total_height) <= 1.0e-6) {
		return {};
	}
    try {
        BRepOffsetAPI_ThruSections loft(true, false, 1.0e-7);
		loft.CheckCompatibility(false);
		loft.SetSmoothing(false);
		loft.SetMaxDegree(3);
        constexpr int kSections = 9;
        for (int index = 0; index < kSections; ++index) {
            const double t = static_cast<double>(index)
                / static_cast<double>(kSections - 1);
            const double x = left + (right - left) * t;
            const double arch_factor =
                1.0 - std::pow(2.0 * t - 1.0, 2.0);
            const double section_bottom =
                bottom_z + lower_curve * arch_factor - 0.5;
            const double section_top =
                top_z + upper_curve * arch_factor + 0.5;
            const double depth_arch = depth_curve * arch_factor;
            const double bottom_y = back_center_y
                + rake * (section_bottom / total_height)
                + post_curve * height_arch_factor(
                    section_bottom, total_height)
                + depth_arch;
            const double top_y = back_center_y
                + rake * (section_top / total_height)
                + post_curve * height_arch_factor(section_top, total_height)
                + depth_arch;
            BRepBuilderAPI_MakePolygon section;
            section.Add(gp_Pnt(
                x, bottom_y - depth * 0.5, section_bottom));
            section.Add(gp_Pnt(
                x, bottom_y + depth * 0.5, section_bottom));
            section.Add(gp_Pnt(
                x, top_y + depth * 0.5, section_top));
            section.Add(gp_Pnt(
                x, top_y - depth * 0.5, section_top));
            section.Close();
            if (!section.IsDone()) {
                return {};
            }
			const TopoDS_Wire wire = section.Wire();
			if (wire.IsNull()) {
				return {};
			}
			loft.AddWire(wire);
        }
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape{};
    } catch (...) {
        return {};
    }
}
}

std::vector<std::unique_ptr<CAlfaObject>> CDeskFurniture::BuildParts(
    const DeskDefinition& desk) {
    const Color panel_color{0.58f, 0.32f, 0.15f};
    const Color facade_color{0.66f, 0.37f, 0.17f};
    const Color handle_color{0.22f, 0.22f, 0.20f};
    const double thickness = desk.panel_thickness;
    const double left = -desk.width * 0.5;
    const double right = desk.width * 0.5;
    const double front = -desk.depth * 0.5;
    const double leg_height = desk.height - thickness;
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto add = [&parts](const std::string& name, TopoDS_Shape shape, Color color) {
        parts.push_back(furniture_solid(name, std::move(shape), color));
    };

    add("Desk Top",
        furniture_box(left, front, leg_height,
            desk.width, desk.depth, thickness), panel_color);
    add("Desk Left Side",
        furniture_box(left, front, 0.0,
            thickness, desk.depth, leg_height), panel_color);
    add("Desk Right Side",
        furniture_box(right - thickness, front, 0.0,
            thickness, desk.depth, leg_height), panel_color);
    add("Desk Back Panel",
        furniture_box(left + thickness, front + desk.depth - thickness,
            leg_height - desk.back_panel_height,
            desk.width - 2.0 * thickness, thickness,
            desk.back_panel_height), panel_color);

    if (desk.type != 0) {
        const bool drawers_left = desk.type == 1;
        const double pedestal_left = drawers_left
            ? left + thickness
            : right - thickness - desk.drawer_width;
        const double partition_x = drawers_left
            ? pedestal_left + desk.drawer_width - thickness
            : pedestal_left;
        add("Desk Drawer Partition",
            furniture_box(partition_x, front, 0.0,
                thickness, desk.depth, leg_height), panel_color);
        add("Desk Drawer Bottom",
            furniture_box(pedestal_left, front, 0.0,
                desk.drawer_width, desk.depth, thickness), panel_color);

        const double gap = 3.0;
        const double facade_left = drawers_left
            ? pedestal_left + gap
            : pedestal_left + thickness + gap;
        const double facade_width = std::max(
            1.0, desk.drawer_width - thickness - 2.0 * gap);
        const double facade_height = std::max(
            1.0,
            (leg_height - gap * static_cast<double>(desk.drawer_count + 1))
                / static_cast<double>(desk.drawer_count));
        for (int drawer = 0; drawer < desk.drawer_count; ++drawer) {
            const double facade_z = gap
                + static_cast<double>(drawer) * (facade_height + gap);
            const double extension = desk.open_drawer == drawer + 1
                ? -desk.pullout_distance : 0.0;
            const double drawer_side = std::clamp(
                thickness * 0.65, 8.0, 14.0);
            const double drawer_height = std::max(
                45.0, std::min(facade_height - 24.0,
                    facade_height * 0.72));
            const std::string number = std::to_string(drawer + 1);

            FurnitureDrawerDefinition definition;
            definition.part_prefix = "Desk Drawer " + number;
            definition.facade_name = "Desk Drawer Facade " + number;
            definition.handle_name = "Desk Drawer Handle " + number;
            definition.left = facade_left;
            definition.front = front + extension;
            definition.bottom = facade_z + 10.0;
            definition.width = facade_width;
            definition.depth = std::max(
                80.0, desk.depth - thickness - 15.0);
            definition.height = drawer_height;
            definition.side_thickness = drawer_side;
            definition.bottom_thickness = std::max(
                5.0, drawer_side * 0.5);
            definition.bottom_offset = 7.0;
            definition.facade_left = facade_left;
            definition.facade_front = front - thickness + extension;
            definition.facade_bottom = facade_z;
            definition.facade_width = facade_width;
            definition.facade_height = facade_height;
            definition.facade_thickness = thickness;
            definition.handle_width_ratio = 0.45;
            definition.handle_max_width = 110.0;
            definition.handle_depth = 12.0;
            definition.handle_height = 8.0;
            definition.handle_gap = 12.0;
            definition.handle_height_ratio = 0.62;
            definition.body_color = panel_color;
            definition.facade_color = facade_color;
            definition.handle_color = handle_color;
            auto drawer_parts = CFurnitureDrawer::BuildParts(definition);
            if (drawer_parts.empty()) {
                return {};
            }
            for (auto& part : drawer_parts) {
                parts.push_back(std::move(part));
            }
        }
    }

    return has_invalid_part(parts)
        ? std::vector<std::unique_ptr<CAlfaObject>>{} : std::move(parts);
}

std::vector<std::unique_ptr<CAlfaObject>> CDrawerBoxFurniture::BuildParts(
    const DrawerBoxDefinition& box) {
    const Color carcass_color{0.58f, 0.32f, 0.15f};
    const Color drawer_color{0.68f, 0.43f, 0.22f};
    const Color facade_color{0.62f, 0.34f, 0.16f};
    const Color rail_color{0.55f, 0.56f, 0.57f};
    const Color handle_color{0.28f, 0.24f, 0.14f};
    const double t = box.panel_thickness;
    const double left = -box.width * 0.5;
    const double front = -box.depth * 0.5;
    const double base_z = box.make_legs ? box.leg_height : 0.0;
    const double inner_width = std::max(1.0, box.width - 2.0 * t);
    const double inner_height = std::max(1.0, box.height - 2.0 * t);
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto add = [&parts](const std::string& name, TopoDS_Shape shape, Color color) {
        parts.push_back(furniture_solid(name, std::move(shape), color));
    };

    add("Drawer Box Left Side",
        furniture_box(left, front, base_z,
            t, box.depth, box.height), carcass_color);
    add("Drawer Box Right Side",
        furniture_box(left + box.width - t, front, base_z,
            t, box.depth, box.height), carcass_color);
    add("Drawer Box Bottom",
        furniture_box(left + t, front, base_z,
            inner_width, box.depth, t), carcass_color);
    add("Drawer Box Top",
        furniture_box(left + t, front, base_z + box.height - t,
            inner_width, box.depth, t), carcass_color);
    add("Drawer Box Back",
        furniture_box(left + t, front + box.depth - t, base_z + t,
            inner_width, t, inner_height), carcass_color);

    if (box.make_legs) {
        const double leg_size = std::min(45.0, t * 2.5);
        const double inset = 20.0;
        const double leg_left = left + inset;
        const double leg_right = left + box.width - inset - leg_size;
        const double leg_front = front + inset;
        const double leg_back = front + box.depth - inset - leg_size;
        for (const auto& position : std::vector<std::pair<double, double>>{
                 {leg_left, leg_front}, {leg_right, leg_front},
                 {leg_right, leg_back}, {leg_left, leg_back}}) {
            add("Drawer Box Leg",
                furniture_box(position.first, position.second, 0.0,
                    leg_size, leg_size, box.leg_height), rail_color);
        }
    }

    const double gap = 3.0;
    const double available_height = std::max(
        1.0, box.height - gap * static_cast<double>(box.drawer_count + 1));
    double requested_height = 0.0;
    for (double value : box.drawer_heights) {
        requested_height += value;
    }
    const double height_scale = requested_height > 0.0
        ? available_height / requested_height : 1.0;
    const double drawer_depth = std::max(
        80.0, box.depth - t - 15.0);
    const double guide_depth = std::max(
        80.0, box.depth - t - 35.0);
    const double drawer_width = std::max(
        80.0, inner_width - 2.0 * box.slide_clearance);
    const double drawer_left = left + t + box.slide_clearance;
    double facade_z = base_z + gap;

    for (int drawer = 0; drawer < box.drawer_count; ++drawer) {
        const double facade_height =
            box.drawer_heights[static_cast<size_t>(drawer)] * height_scale;
        const double drawer_height = std::max(
            45.0, std::min(facade_height - 28.0,
                facade_height * 0.72));
        const double drawer_z = facade_z + 12.0;
        const double extension = box.open_drawer == drawer + 1
            ? -box.pullout_distance : 0.0;
        const std::string number = std::to_string(drawer + 1);
        const double facade_left = left + gap;
        const double facade_width = box.width - 2.0 * gap;

        FurnitureDrawerDefinition definition;
        definition.part_prefix = "Drawer " + number;
        definition.facade_name = "Drawer " + number + " Facade";
        definition.handle_name = "Drawer " + number + " Handle";
        definition.left = drawer_left;
        definition.front = front + extension;
        definition.bottom = drawer_z;
        definition.width = drawer_width;
        definition.depth = drawer_depth;
        definition.height = drawer_height;
        definition.side_thickness = box.drawer_side_thickness;
        definition.bottom_thickness = box.drawer_bottom_thickness;
        definition.bottom_offset = 8.0;
        definition.facade_left = facade_left;
        definition.facade_front = front - t + extension;
        definition.facade_bottom = facade_z;
        definition.facade_width = facade_width;
        definition.facade_height = facade_height;
        definition.facade_thickness = t;
        definition.facade_style = box.facade_type == 0
            ? FurnitureDrawerFacadeStyle::Slab
            : FurnitureDrawerFacadeStyle::Frame;
        definition.make_handle = box.handle_type != 3;
        definition.round_handle = box.handle_type == 2;
        definition.body_color = drawer_color;
        definition.facade_color = facade_color;
        definition.handle_color = handle_color;
        auto drawer_parts = CFurnitureDrawer::BuildParts(definition);
        if (drawer_parts.empty()) {
            return {};
        }

        constexpr size_t body_part_count = 5;
        for (size_t index = 0;
             index < std::min(body_part_count, drawer_parts.size()); ++index) {
            parts.push_back(std::move(drawer_parts[index]));
        }
        const double rail_z = drawer_z + 18.0;
        add("Drawer " + number + " Left Guide",
            furniture_box(left + t, front + 22.0, rail_z,
                8.0, guide_depth, 12.0), rail_color);
        add("Drawer " + number + " Right Guide",
            furniture_box(left + box.width - t - 8.0,
                front + 22.0, rail_z,
                8.0, guide_depth, 12.0), rail_color);
        for (size_t index = body_part_count;
             index < drawer_parts.size(); ++index) {
            parts.push_back(std::move(drawer_parts[index]));
        }
        facade_z += facade_height + gap;
    }

    return has_invalid_part(parts)
        ? std::vector<std::unique_ptr<CAlfaObject>>{} : std::move(parts);
}

std::vector<std::unique_ptr<CAlfaObject>> CNikaKitchenFurniture::BuildParts(
    const NikaKitchenDefinition& kitchen) {
    const Color body_color{0.48f, 0.24f, 0.10f};
    const Color facade_color{0.61f, 0.31f, 0.12f};
    const Color panel_color{0.52f, 0.25f, 0.10f};
    const Color glass_color{0.66f, 0.82f, 0.88f};
    const Color hardware_color{0.36f, 0.32f, 0.18f};
    const Color worktop_color{0.46f, 0.22f, 0.09f};
    const double t = kitchen.panel_thickness;
    const double gap = 3.0;
    const double left = -kitchen.width * 0.5;
    const double lower_carcass_height = std::max(
        100.0, kitchen.base_height - kitchen.leg_height);
    const double upper_bottom = kitchen.base_height
        + kitchen.worktop_thickness + kitchen.wall_gap;
    const std::array<double, 4> ratios{600.0 / 2600.0, 600.0 / 2600.0,
                                      600.0 / 2600.0, 800.0 / 2600.0};
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto add = [&parts](const std::string& name, TopoDS_Shape shape, Color color) {
        parts.push_back(furniture_solid(name, std::move(shape), color));
    };
    const auto add_moving = [&add](const std::string& name,
                                   TopoDS_Shape shape, Color color,
                                   double pullout, double hinge_x,
                                   double hinge_y, double angle) {
        add(name,
            move_furniture_shape(
                shape, pullout, hinge_x, hinge_y, angle),
            color);
    };

    const auto add_handle = [&](const std::string& prefix,
                                double x, double front, double z,
                                double width, bool vertical,
                                bool handle_on_left, double pullout,
                                double hinge_x, double hinge_y,
                                double angle) {
        if (kitchen.handle_type == 3) {
            return;
        }
        if (kitchen.handle_type == 2) {
            BRepPrimAPI_MakeCylinder knob(
                gp_Ax2(gp_Pnt(x + width * 0.5, front - 12.0, z),
                       gp_Dir(0.0, -1.0, 0.0)),
                10.0, 22.0);
            knob.Build();
            add_moving(prefix + " Handle",
                knob.IsDone() ? knob.Shape() : TopoDS_Shape{},
                hardware_color, pullout, hinge_x, hinge_y, angle);
            return;
        }
        const double bar = std::clamp(width * 0.45, 90.0, 180.0);
        if (vertical) {
            const double handle_x = handle_on_left ? x + 24.0 : x + width - 34.0;
            add_moving(prefix + " Handle Bar",
                furniture_box(handle_x, front - 25.0, z - bar * 0.5,
                              10.0, 10.0, bar), hardware_color,
                pullout, hinge_x, hinge_y, angle);
            add_moving(prefix + " Handle Mount",
                furniture_box(handle_x, front - 15.0, z - bar * 0.35,
                              10.0, 15.0, 8.0), hardware_color,
                pullout, hinge_x, hinge_y, angle);
            add_moving(prefix + " Handle Mount",
                furniture_box(handle_x, front - 15.0, z + bar * 0.35,
                              10.0, 15.0, 8.0), hardware_color,
                pullout, hinge_x, hinge_y, angle);
        } else {
            const double bar_x = x + (width - bar) * 0.5;
            add_moving(prefix + " Handle Bar",
                furniture_box(bar_x, front - 25.0, z, bar, 10.0, 10.0),
                hardware_color, pullout, hinge_x, hinge_y, angle);
            add_moving(prefix + " Handle Mount",
                furniture_box(bar_x + 12.0, front - 15.0, z,
                              8.0, 15.0, 10.0), hardware_color,
                pullout, hinge_x, hinge_y, angle);
            add_moving(prefix + " Handle Mount",
                furniture_box(bar_x + bar - 20.0, front - 15.0, z,
                              8.0, 15.0, 10.0), hardware_color,
                pullout, hinge_x, hinge_y, angle);
        }
    };

    const auto add_facade = [&](const std::string& prefix,
                                double x, double front, double z,
                                double width, double height, bool glass,
                                bool vertical_handle, bool handle_on_left,
                                double open_angle, bool hinge_on_right,
                                double pullout) {
        const double hinge_x = hinge_on_right ? x + width : x;
        const double signed_angle = hinge_on_right ? open_angle : -open_angle;
        const auto add_part = [&](const std::string& name,
                                  TopoDS_Shape shape, Color color) {
            add_moving(name, std::move(shape), color, pullout,
                       hinge_x, front, signed_angle);
        };
        if (kitchen.facade_style == 0 && !glass) {
            add_part(prefix + " Facade",
                furniture_box(x, front - t, z, width, t, height), facade_color);
        } else {
            const double frame = std::clamp(
                std::min(width, height) * 0.12, 45.0, 75.0);
            add_part(prefix + " Facade Left Frame",
                furniture_box(x, front - t, z, frame, t, height), facade_color);
            add_part(prefix + " Facade Right Frame",
                furniture_box(x + width - frame, front - t, z,
                              frame, t, height), facade_color);
            add_part(prefix + " Facade Bottom Frame",
                furniture_box(x + frame, front - t, z,
                              width - 2.0 * frame, t, frame), facade_color);
            add_part(prefix + " Facade Top Frame",
                furniture_box(x + frame, front - t, z + height - frame,
                              width - 2.0 * frame, t, frame), facade_color);
            const double inset = kitchen.facade_style == 2 ? 5.0 : 2.0;
            add_part(glass ? prefix + " Showcase Glass" : prefix + " Facade Panel",
                furniture_box(x + frame, front - t + inset, z + frame,
                              width - 2.0 * frame, std::max(2.0, t - inset),
                              height - 2.0 * frame),
                glass ? glass_color : panel_color);
        }
        add_handle(prefix, x, front - t, z + height * 0.72,
                   width, vertical_handle, handle_on_left, pullout,
                   hinge_x, front, signed_angle);
    };

    const auto add_drawer_body = [&](const std::string& prefix,
                                     double x, double front, double z,
                                     double width, double height,
                                     double pullout) {
        const double side = 12.0;
        const double bottom = 6.0;
        const double depth = std::max(100.0, kitchen.base_depth - 45.0);
        const double body_height = std::max(45.0, height * 0.65);
        const double body_z = z + 12.0;
        const auto drawer_add = [&](const std::string& name, TopoDS_Shape shape) {
            add_moving(name, std::move(shape), body_color,
                       pullout, 0.0, 0.0, 0.0);
        };
        drawer_add(prefix + " Left Side",
            furniture_box(x + 18.0, front + 8.0, body_z,
                          side, depth, body_height));
        drawer_add(prefix + " Right Side",
            furniture_box(x + width - 18.0 - side, front + 8.0, body_z,
                          side, depth, body_height));
        drawer_add(prefix + " Bottom",
            furniture_box(x + 18.0, front + 8.0, body_z,
                          width - 36.0, depth, bottom));
        drawer_add(prefix + " Back",
            furniture_box(x + 18.0, front + depth - side, body_z,
                          width - 36.0, side, body_height));
    };

    const auto add_carcass = [&](const std::string& prefix,
                                 double x, double front, double bottom,
                                 double width, double depth, double height,
                                 int shelves) {
        add(prefix + " Left Side",
            furniture_box(x, front, bottom, t, depth, height), body_color);
        add(prefix + " Right Side",
            furniture_box(x + width - t, front, bottom,
                          t, depth, height), body_color);
        add(prefix + " Bottom",
            furniture_box(x + t, front, bottom,
                          width - 2.0 * t, depth, t), body_color);
        add(prefix + " Top",
            furniture_box(x + t, front, bottom + height - t,
                          width - 2.0 * t, depth, t), body_color);
        add(prefix + " Back",
            furniture_box(x + t, front + depth - t, bottom + t,
                          width - 2.0 * t, t, height - 2.0 * t), body_color);
        for (int shelf = 1; shelf <= shelves; ++shelf) {
            const double shelf_z = bottom
                + height * static_cast<double>(shelf)
                    / static_cast<double>(shelves + 1);
            add(prefix + " Shelf " + std::to_string(shelf),
                furniture_box(x + t, front + 8.0, shelf_z,
                              width - 2.0 * t, depth - t - 16.0, t),
                body_color);
        }
    };

    double x = left;
    for (size_t module = 0; module < ratios.size(); ++module) {
        const double width = kitchen.width * ratios[module];
        const std::string number = std::to_string(module + 1);
        const double lower_front = -kitchen.base_depth * 0.5;
        add_carcass("Nika Lower " + number, x, lower_front,
                    kitchen.leg_height, width, kitchen.base_depth,
                    lower_carcass_height, 0);

        const double leg_size = 38.0;
        for (double leg_x : {x + 28.0, x + width - 28.0 - leg_size}) {
            add("Nika Lower " + number + " Leg",
                furniture_box(leg_x, lower_front + 28.0, 0.0,
                              leg_size, leg_size, kitchen.leg_height),
                hardware_color);
        }

        const double facade_x = x + gap;
        const double facade_width = width - 2.0 * gap;
        const double facade_bottom = kitchen.leg_height + gap;
        const double facade_height = lower_carcass_height - 2.0 * gap;
        if (module == 1 || module == 2) {
            const double drawer_gap = 3.0;
            const double drawer_height =
                (facade_height - 3.0 * drawer_gap) / 4.0;
            for (int drawer = 0; drawer < 4; ++drawer) {
                const double drawer_z = facade_bottom
                    + drawer * (drawer_height + drawer_gap);
                const int drawer_index = (module == 1 ? 0 : 4) + drawer + 1;
                const double pullout = kitchen.open_drawer == drawer_index
                    ? kitchen.pullout_distance : 0.0;
                const std::string drawer_prefix = "Nika Lower " + number
                    + " Drawer " + std::to_string(drawer + 1);
                add_drawer_body(drawer_prefix, facade_x, lower_front,
                                drawer_z, facade_width, drawer_height, pullout);
                add_facade(drawer_prefix,
                           facade_x, lower_front, drawer_z,
                           facade_width, drawer_height, false, false,
                           false, 0.0, false, pullout);
            }
        } else if (module == 3) {
            const double door_width = (facade_width - gap) * 0.5;
            add_facade("Nika Lower 4 Left Door", facade_x, lower_front,
                       facade_bottom, door_width, facade_height, false, true,
                       false, kitchen.door_open_angles[1], false, 0.0);
            add_facade("Nika Lower 4 Right Door",
                       facade_x + door_width + gap, lower_front,
                       facade_bottom, door_width, facade_height, false, true,
                       true, kitchen.door_open_angles[2], true, 0.0);
        } else {
            add_facade("Nika Lower 1 Door", facade_x, lower_front,
                       facade_bottom, facade_width, facade_height, false, true,
                       false, kitchen.door_open_angles[0], false, 0.0);
        }

        const double upper_front = -kitchen.upper_depth * 0.5;
        add_carcass("Nika Upper " + number, x, upper_front, upper_bottom,
                    width, kitchen.upper_depth, kitchen.upper_height, 1);
        const double upper_facade_z = upper_bottom + gap;
        const double upper_facade_height = kitchen.upper_height - 2.0 * gap;
        if (module == 3) {
            const double door_width = (facade_width - gap) * 0.5;
            add_facade("Nika Upper 4 Left Door", facade_x, upper_front,
                       upper_facade_z, door_width, upper_facade_height,
                       false, true, false,
                       kitchen.door_open_angles[6], false, 0.0);
            add_facade("Nika Upper 4 Right Door",
                       facade_x + door_width + gap, upper_front,
                       upper_facade_z, door_width, upper_facade_height,
                       false, true, true,
                       kitchen.door_open_angles[7], true, 0.0);
        } else {
            const bool showcase = module == 0 || module == 2;
            const size_t angle_index = 3 + module;
            add_facade("Nika Upper " + number + " Door", facade_x,
                       upper_front, upper_facade_z, facade_width,
                       upper_facade_height, showcase, true, false,
                       kitchen.door_open_angles[angle_index], false, 0.0);
        }
        x += width;
    }

    add("Nika Worktop",
        rounded_worktop_shape(
            left - 15.0, -kitchen.base_depth * 0.5 - 25.0,
            kitchen.base_height,
            kitchen.width + 30.0, kitchen.base_depth + 40.0,
            kitchen.worktop_thickness, kitchen.worktop_front_radius),
        worktop_color);

    return has_invalid_part(parts)
        ? std::vector<std::unique_ptr<CAlfaObject>>{} : std::move(parts);
}

namespace {
bool build_corner_kitchen_parts(
    const CornerKitchenDefinition& corner,
    const std::function<void(std::unique_ptr<CAlfaObject>)>& part_ready,
    std::vector<std::unique_ptr<CAlfaObject>>* collected_parts) {
    bool valid = true;
    const auto emit = [&part_ready, collected_parts, &valid](
        std::unique_ptr<CAlfaObject> part) {
        if (!part) {
            valid = false;
            return;
        }
        if (part_ready) {
            part_ready(std::move(part));
        } else if (collected_parts) {
            collected_parts->push_back(std::move(part));
        }
    };
    const auto nika_definition = [&corner](double width,
                                           const std::array<double, 8>& angles) {
        NikaKitchenDefinition result;
        result.width = width;
        result.base_height = corner.base_height;
        result.base_depth = corner.base_depth;
        result.upper_height = corner.upper_height;
        result.upper_depth = corner.upper_depth;
        result.wall_gap = corner.wall_gap;
        result.worktop_thickness = corner.worktop_thickness;
        result.worktop_front_radius = corner.worktop_front_radius;
        result.panel_thickness = corner.panel_thickness;
        result.leg_height = corner.leg_height;
        result.facade_style = corner.facade_style;
        result.handle_type = corner.handle_type;
        result.door_open_angles = angles;
        return result;
    };
    // Each straight run stops before the corner.  The missing area is filled
    // by a genuine L-shaped base cabinet and a separate upper corner cabinet.
    const double left_run = std::max(900.0,
        corner.left_length - corner.base_depth);
    const double right_run = std::max(900.0,
        corner.right_length - corner.base_depth);
    auto left_parts = CNikaKitchenFurniture::BuildParts(
        nika_definition(left_run, corner.left_door_open_angles));
    if (left_parts.empty()) return false;

    const auto transformed_part = [](
        const CAlfaObject& source, double angle_degrees,
        double dx, double dy, const std::string& prefix, double dz = 0.0)
            -> std::unique_ptr<CAlfaObject> {
        const auto* solid = dynamic_cast<const CSolid*>(&source);
        if (!solid) {
            return {};
        }
        TopoDS_Shape shape = solid->m_Shape;
        gp_Trsf placement;
        if (std::abs(angle_degrees) > 1.0e-9) {
            placement.SetRotation(
                gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)),
                angle_degrees * 3.14159265358979323846 / 180.0);
        }
        placement.SetTranslationPart(gp_Vec(dx, dy, dz));
        // This is a rigid placement.  Keeping the underlying BRep shared
        // avoids two deep copies and a costly re-triangulation per part.
        BRepBuilderAPI_Transform placed(shape, placement, false);
        placed.Build();
        if (!placed.IsDone()) {
            return {};
        }
        std::string name = source.GetName();
        if (name.rfind("Nika ", 0) == 0) {
            name = prefix + name.substr(5);
        } else {
            name = prefix + name;
        }
        return furniture_solid(name, placed.Shape(), source.GetColor());
    };

    if (collected_parts)
        collected_parts->reserve(left_parts.size() + 96);
    for (const auto& part : left_parts) {
        const bool upper = part->GetName().find("Upper") != std::string::npos;
        const double depth = upper ? corner.upper_depth : corner.base_depth;
        // The finished corner cabinet has its two facade lines on X=0 and
        // Y=0.  Put the straight run's facade on Y=0 and stop it exactly at
        // the horizontal corner facade (X=-depth).
        emit(transformed_part(
            *part, 0.0,
            -depth - left_run * 0.5,
            depth * 0.5,
            "Corner Left "));
    }

    // Build the second run only after the first one has been emitted.  The
    // creation tool can therefore paint the left cabinets immediately instead
    // of waiting for the complete corner kitchen to exist in memory.
    auto right_parts = CNikaKitchenFurniture::BuildParts(
        nika_definition(right_run, corner.right_door_open_angles));
    if (right_parts.empty()) return false;
    for (const auto& part : right_parts) {
        const bool upper = part->GetName().find("Upper") != std::string::npos;
        const double depth = upper ? corner.upper_depth : corner.base_depth;
        // After the -90 degree rotation the straight facade is parallel to
        // X=0.  Its end meets the vertical corner facade at Y=-depth.
        emit(transformed_part(
            *part, -90.0, depth * 0.5,
            -depth - right_run * 0.5,
            "Corner Right "));
    }

    const auto append_corner_cabinet = [&emit, &transformed_part, &corner](
        const std::string& prefix, double depth, double height,
        double z, double door_angle) {
        KitchenCabinetDefinition definition;
        definition.body_type = KitchenCabinetBodyType::Corner;
        definition.facade_type = KitchenCabinetFacadeType::DoubleDoor;
        // Nika and CKitchenCabinet use different numeric facade enums:
        // Nika 2 is Milano, while cabinet 2 is Screen (showcase).
        switch (std::clamp(corner.facade_style, 0, 2)) {
        case 0:
            definition.facade_style = KitchenCabinetFacadeStyle::Plain;
            break;
        case 1:
            definition.facade_style = KitchenCabinetFacadeStyle::Frame;
            break;
        default:
            definition.facade_style = KitchenCabinetFacadeStyle::Milano;
            break;
        }
        definition.width = depth * 2.0;
        definition.depth = depth * 2.0;
        definition.height = height;
        definition.panel_thickness = corner.panel_thickness;
        definition.shelf_count = 1;
        definition.door_open_angle = door_angle;
        auto cabinet_parts = CKitchenCabinet::BuildParts(definition);
        for (const auto& part : cabinet_parts) {
            emit(transformed_part(
                *part, 0.0, 0.0, 0.0, prefix, z));
        }
    };
    append_corner_cabinet(
        "Corner Base ", corner.base_depth,
        std::max(100.0, corner.base_height - corner.leg_height),
        corner.leg_height, corner.lower_corner_door_angle);
    append_corner_cabinet(
        "Corner Upper ", corner.upper_depth, corner.upper_height,
        corner.base_height + corner.worktop_thickness + corner.wall_gap,
        corner.upper_corner_door_angle);

    // The corner section gets its own rounded worktop.  The two straight
    // worktops meet it instead of intersecting each other.
    emit(furniture_solid(
        "Corner Base Worktop",
        corner_worktop_shape(
            0.0, 0.0, corner.base_height,
            corner.base_depth, corner.worktop_thickness,
            corner.worktop_front_radius),
        Color{0.46f, 0.22f, 0.09f}));
    return valid;
}
}

std::vector<std::unique_ptr<CAlfaObject>> CCornerKitchenFurniture::BuildParts(
    const CornerKitchenDefinition& corner) {
    std::vector<std::unique_ptr<CAlfaObject>> result;
    if (!build_corner_kitchen_parts(corner, {}, &result)
        || has_invalid_part(result)) {
        return {};
    }
    return result;
}

bool CCornerKitchenFurniture::BuildPartsIncremental(
    const CornerKitchenDefinition& corner,
    const std::function<void(std::unique_ptr<CAlfaObject>)>& part_ready) {
    return part_ready
        && build_corner_kitchen_parts(corner, part_ready, nullptr);
}

std::vector<std::unique_ptr<CAlfaObject>> CChairFurniture::BuildParts(
    const ChairDefinition& chair) {
    const Color wood{0.57f, 0.28f, 0.12f};
    const Color seat_color{0.68f, 0.38f, 0.17f};
    const double left = -chair.width * 0.5;
    const double right = chair.width * 0.5;
    const double front = -chair.depth * 0.5;
    const double back = chair.depth * 0.5;
    const double seat_bottom = chair.seat_height - chair.seat_thickness;
    const double leg_x_left = left + chair.leg_inset;
    const double leg_x_right = right - chair.leg_inset - chair.leg_size;
    const double front_leg_y = front + chair.leg_inset;
    const double rear_post_y = back - chair.leg_inset - chair.rear_post_size;
    const double taper = std::min(chair.leg_taper, chair.leg_size * 0.45);
    const double rear_taper = std::min(chair.leg_taper, chair.rear_post_size * 0.45);
    const double apron_z = seat_bottom - chair.apron_height;

    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto add = [&parts](const std::string& name, TopoDS_Shape shape, Color color) {
        parts.push_back(furniture_solid(name, std::move(shape), color));
    };

    add("Chair Seat",
        curved_back_seat(left, front, seat_bottom,
                         chair.width, chair.depth, chair.seat_thickness,
                         chair.seat_back_curve,
                         std::min(chair.seat_edge_radius,
                                  chair.seat_thickness * 0.45)),
        seat_color);

    add("Chair Front Left Leg",
        tapered_vertical_member(
            leg_x_left + taper, front_leg_y + taper,
            leg_x_left, front_leg_y,
            0.0, seat_bottom,
            chair.leg_size - 2.0 * taper,
            chair.leg_size - 2.0 * taper,
            chair.leg_size, chair.leg_size), wood);
    add("Chair Front Right Leg",
        tapered_vertical_member(
            leg_x_right + taper, front_leg_y + taper,
            leg_x_right, front_leg_y,
            0.0, seat_bottom,
            chair.leg_size - 2.0 * taper,
            chair.leg_size - 2.0 * taper,
            chair.leg_size, chair.leg_size), wood);

    const double rear_x_left = left + chair.leg_inset;
    const double rear_x_right = right - chair.leg_inset - chair.rear_post_size;
    add("Chair Left Back Post",
        curved_tapered_vertical_member(
            rear_x_left + rear_taper, rear_post_y + rear_taper,
            rear_x_left, rear_post_y + chair.back_rake,
            0.0, chair.total_height,
            chair.rear_post_size - 2.0 * rear_taper,
            chair.rear_post_size - 2.0 * rear_taper,
            chair.rear_post_size, chair.rear_post_size,
            chair.rear_post_curve), wood);
    add("Chair Right Back Post",
        curved_tapered_vertical_member(
            rear_x_right + rear_taper, rear_post_y + rear_taper,
            rear_x_right, rear_post_y + chair.back_rake,
            0.0, chair.total_height,
            chair.rear_post_size - 2.0 * rear_taper,
            chair.rear_post_size - 2.0 * rear_taper,
            chair.rear_post_size, chair.rear_post_size,
            chair.rear_post_curve), wood);

    const double front_apron_left = leg_x_left + chair.leg_size;
    const double front_apron_width = std::max(
        1.0, leg_x_right - front_apron_left);
    const double rear_apron_y = rear_post_y + chair.back_rake
        * ((apron_z + chair.apron_height * 0.5) / chair.total_height)
        + chair.rear_post_curve * height_arch_factor(
            apron_z + chair.apron_height * 0.5, chair.total_height);
    add("Chair Front Apron",
        furniture_box(front_apron_left, front_leg_y, apron_z,
                      front_apron_width, chair.apron_thickness,
                      chair.apron_height), wood);
    add("Chair Back Apron",
        furniture_box(rear_x_left + chair.rear_post_size, rear_apron_y,
                      apron_z,
                      std::max(1.0, rear_x_right - rear_x_left
                                      - chair.rear_post_size),
                      chair.apron_thickness, chair.apron_height), wood);
    const double side_apron_depth = std::max(
        1.0, rear_apron_y - front_leg_y - chair.leg_size);
    add("Chair Left Side Apron",
        furniture_box(leg_x_left, front_leg_y + chair.leg_size, apron_z,
                      chair.apron_thickness, side_apron_depth,
                      chair.apron_height), wood);
    add("Chair Right Side Apron",
        furniture_box(leg_x_right + chair.leg_size - chair.apron_thickness,
                      front_leg_y + chair.leg_size, apron_z,
                      chair.apron_thickness, side_apron_depth,
                      chair.apron_height), wood);

    if (chair.stretcher_height > chair.stretcher_size
        && chair.stretcher_height < apron_z) {
        const double stretcher_z = chair.stretcher_height - chair.stretcher_size;
        const double rear_stretcher_y = rear_post_y + chair.back_rake
            * ((stretcher_z + chair.stretcher_size * 0.5)
               / chair.total_height)
            + chair.rear_post_curve * height_arch_factor(
                stretcher_z + chair.stretcher_size * 0.5,
                chair.total_height);
        const double side_stretcher_depth = std::max(
            1.0, rear_stretcher_y - front_leg_y - chair.leg_size);
        add("Chair Left Stretcher",
            furniture_box(leg_x_left, front_leg_y + chair.leg_size,
                          stretcher_z, chair.stretcher_size,
                          side_stretcher_depth, chair.stretcher_size), wood);
        add("Chair Right Stretcher",
            furniture_box(leg_x_right + chair.leg_size - chair.stretcher_size,
                          front_leg_y + chair.leg_size,
                          stretcher_z, chair.stretcher_size,
                          side_stretcher_depth, chair.stretcher_size), wood);
        add("Chair Front Stretcher",
            furniture_box(front_apron_left, front_leg_y, stretcher_z,
                          front_apron_width, chair.stretcher_size,
                          chair.stretcher_size), wood);
    }

    const double inner_left = rear_x_left + chair.rear_post_size;
    const double inner_right = rear_x_right;
    // Back members are centered through the depth of the rear posts.  The
    // post loft keeps this center line while it rakes backwards.
    const double back_center_y =
        rear_post_y + chair.rear_post_size * 0.5;
    const double lower_rail_z = chair.seat_height
        + std::max(75.0, (chair.total_height - chair.seat_height) * 0.18);
    const double upper_rail_z = chair.total_height
        - chair.back_rail_height * 0.65;
    add("Chair Lower Back Rail",
        curved_back_rail(inner_left, inner_right, lower_rail_z,
                         chair.back_rail_height, chair.back_member_thickness,
                         chair.back_curve * 0.35, chair.back_depth_curve,
                         back_center_y,
                         chair.back_rake, chair.rear_post_curve,
                         chair.total_height), wood);
    add("Chair Top Back Rail",
        curved_back_rail(inner_left, inner_right, upper_rail_z,
                         chair.back_rail_height, chair.back_member_thickness,
                         chair.back_curve, chair.back_depth_curve,
                         back_center_y,
                         chair.back_rake, chair.rear_post_curve,
                         chair.total_height), wood);

    const double member_bottom = lower_rail_z + chair.back_rail_height * 0.5;
    const double member_top = upper_rail_z - chair.back_rail_height * 0.5;
    const double inner_width = std::max(1.0, inner_right - inner_left);
    if (chair.back_style == 0) {
        const int count = std::max(1, chair.back_member_count);
        for (int member = 0; member < count; ++member) {
            const double t = static_cast<double>(member + 1)
                / static_cast<double>(count + 1);
            const double arch_factor =
                1.0 - std::pow(2.0 * t - 1.0, 2.0);
            // Both rails are arched.  Give every vertical slat its own end
            // heights so it actually meets the rail surfaces along the
            // corresponding section of each curve.
            const double slat_bottom = member_bottom
                + chair.back_curve * 0.35 * arch_factor - 0.5;
            const double slat_top = member_top
                + chair.back_curve * arch_factor + 0.5;
            const double center_x = inner_left + inner_width * t;
            const double bottom_y = back_center_y
                + chair.back_rake * (slat_bottom / chair.total_height)
                + chair.rear_post_curve * height_arch_factor(
                    slat_bottom, chair.total_height)
                + chair.back_depth_curve * arch_factor;
            const double top_y = back_center_y
                + chair.back_rake * (slat_top / chair.total_height)
                + chair.rear_post_curve * height_arch_factor(
                    slat_top, chair.total_height)
                + chair.back_depth_curve * arch_factor;
            const double arch_slope =
                4.0 * (1.0 - 2.0 * t) / inner_width;
            const double bottom_z_slope =
                chair.back_curve * 0.35 * arch_slope;
            const double top_z_slope = chair.back_curve * arch_slope;
            // The apparent top-view tangent also contains the Y displacement
            // caused by travelling up or down the raked/curved back post as
            // the rail arches in Z.  Upper and lower rails use different Z
            // curves, so the slat must twist between two distinct tangents.
            const double bottom_y_per_z = chair.back_rake / chair.total_height
                + chair.rear_post_curve * height_arch_derivative(
                    slat_bottom, chair.total_height);
            const double top_y_per_z = chair.back_rake / chair.total_height
                + chair.rear_post_curve * height_arch_derivative(
                    slat_top, chair.total_height);
            const double depth_curve_slope =
                chair.back_depth_curve * arch_slope;
            const double bottom_centerline_y_slope = depth_curve_slope
                + bottom_y_per_z * bottom_z_slope;
            const double top_centerline_y_slope = depth_curve_slope
                + top_y_per_z * top_z_slope;
            // Tangent plane z = ax + by of each mating rail surface.  The
            // depth direction of the rotated YZ rail section gives b=-dy/dz;
            // the centerline tangent then determines a.  Only the end faces
            // are tilted; the slat section itself keeps one constant angle.
            const double bottom_plane_slope_y = -bottom_y_per_z;
            const double top_plane_slope_y = -top_y_per_z;
            const double bottom_plane_slope_x = bottom_z_slope
                - bottom_plane_slope_y * bottom_centerline_y_slope;
            const double top_plane_slope_x = top_z_slope
                - top_plane_slope_y * top_centerline_y_slope;
            add("Chair Vertical Back Slat " + std::to_string(member + 1),
                oriented_vertical_member(
                    center_x, bottom_y, top_y,
                    slat_bottom, slat_top,
                    chair.back_member_width, chair.back_member_thickness,
                    1.0, depth_curve_slope,
                    bottom_plane_slope_x, bottom_plane_slope_y,
                    top_plane_slope_x, top_plane_slope_y), wood);
        }
    } else if (chair.back_style == 1) {
        const int count = std::max(1, chair.back_member_count);
        for (int member = 0; member < count; ++member) {
            const double t = static_cast<double>(member + 1)
                / static_cast<double>(count + 1);
            const double z = member_bottom + (member_top - member_bottom) * t;
            add("Chair Horizontal Back Slat " + std::to_string(member + 1),
                curved_back_rail(inner_left, inner_right, z,
                                 chair.back_member_width,
                                 chair.back_member_thickness,
                                 chair.back_curve * 0.6,
                                 chair.back_depth_curve,
                                 back_center_y,
                                 chair.back_rake, chair.rear_post_curve,
                                 chair.total_height), wood);
        }
    } else {
        add("Chair Back Panel",
            curved_back_panel(
                inner_left, inner_right,
                member_bottom, member_top,
                chair.back_member_thickness,
                chair.back_curve * 0.35, chair.back_curve,
                chair.back_depth_curve, back_center_y, chair.back_rake,
                chair.rear_post_curve, chair.total_height), wood);
    }

    return has_invalid_part(parts)
        ? std::vector<std::unique_ptr<CAlfaObject>>{} : std::move(parts);
}
