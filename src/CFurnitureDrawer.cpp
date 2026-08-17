#include "CFurnitureDrawer.h"

#include "solid/Solid.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <utility>

namespace {
TopoDS_Shape drawer_box(double x, double y, double z,
                        double width, double depth, double height) {
    BRepPrimAPI_MakeBox builder(gp_Pnt(x, y, z), width, depth, height);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

std::unique_ptr<CSolid> drawer_solid(
    const std::string& name, TopoDS_Shape shape, Color color) {
    if (shape.IsNull()) {
        return nullptr;
    }
    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName(name);
    solid->SetColor(color);
    return solid->ReBuldMesh() ? std::move(solid) : nullptr;
}
}

std::vector<std::unique_ptr<CAlfaObject>> CFurnitureDrawer::BuildParts(
    const FurnitureDrawerDefinition& drawer) {
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto add = [&parts](const std::string& name, TopoDS_Shape shape, Color color) {
        parts.push_back(drawer_solid(name, std::move(shape), color));
    };

    const double side = drawer.side_thickness;
    const double inner_width = std::max(1.0, drawer.width - 2.0 * side);
    add(drawer.part_prefix + " Left Side",
        drawer_box(drawer.left, drawer.front, drawer.bottom,
            side, drawer.depth, drawer.height), drawer.body_color);
    add(drawer.part_prefix + " Right Side",
        drawer_box(drawer.left + drawer.width - side,
            drawer.front, drawer.bottom,
            side, drawer.depth, drawer.height), drawer.body_color);
    add(drawer.part_prefix + " Back",
        drawer_box(drawer.left + side,
            drawer.front + drawer.depth - side, drawer.bottom,
            inner_width, side, drawer.height), drawer.body_color);
    add(drawer.part_prefix + " Front Wall",
        drawer_box(drawer.left + side, drawer.front, drawer.bottom,
            inner_width, side, drawer.height), drawer.body_color);
    add(drawer.part_prefix + " Bottom",
        drawer_box(drawer.left + side, drawer.front + side,
            drawer.bottom + drawer.bottom_offset,
            inner_width, drawer.depth - 2.0 * side,
            drawer.bottom_thickness), drawer.body_color);

    if (drawer.make_facade
        && drawer.facade_style == FurnitureDrawerFacadeStyle::Slab) {
        add(drawer.facade_name,
            drawer_box(drawer.facade_left, drawer.facade_front,
                drawer.facade_bottom, drawer.facade_width,
                drawer.facade_thickness, drawer.facade_height),
            drawer.facade_color);
    } else if (drawer.make_facade) {
        const double frame_width = std::clamp(
            std::min(drawer.facade_height, drawer.facade_width) * 0.12,
            18.0, 55.0);
        const std::string prefix = drawer.facade_name + " ";
        add(prefix + "Left Frame",
            drawer_box(drawer.facade_left, drawer.facade_front,
                drawer.facade_bottom, frame_width,
                drawer.facade_thickness, drawer.facade_height),
            drawer.facade_color);
        add(prefix + "Right Frame",
            drawer_box(drawer.facade_left + drawer.facade_width - frame_width,
                drawer.facade_front, drawer.facade_bottom, frame_width,
                drawer.facade_thickness, drawer.facade_height),
            drawer.facade_color);
        add(prefix + "Bottom Frame",
            drawer_box(drawer.facade_left + frame_width, drawer.facade_front,
                drawer.facade_bottom,
                drawer.facade_width - 2.0 * frame_width,
                drawer.facade_thickness, frame_width), drawer.facade_color);
        add(prefix + "Top Frame",
            drawer_box(drawer.facade_left + frame_width, drawer.facade_front,
                drawer.facade_bottom + drawer.facade_height - frame_width,
                drawer.facade_width - 2.0 * frame_width,
                drawer.facade_thickness, frame_width), drawer.facade_color);
        add(prefix + "Inset",
            drawer_box(drawer.facade_left + frame_width,
                drawer.facade_front + drawer.facade_thickness * 0.35,
                drawer.facade_bottom + frame_width,
                drawer.facade_width - 2.0 * frame_width,
                drawer.facade_thickness * 0.45,
                drawer.facade_height - 2.0 * frame_width),
            drawer.facade_color);
    }

    if (drawer.make_facade && drawer.make_handle) {
        const double handle_width = drawer.round_handle
            ? 24.0
            : std::min(drawer.handle_max_width,
                drawer.facade_width * drawer.handle_width_ratio);
        const double handle_height = drawer.round_handle
            ? 24.0 : drawer.handle_height;
        add(drawer.handle_name,
            drawer_box(
                drawer.facade_left + (drawer.facade_width - handle_width) * 0.5,
                drawer.facade_front - drawer.handle_gap,
                drawer.facade_bottom
                    + drawer.facade_height * drawer.handle_height_ratio,
                handle_width, drawer.handle_depth, handle_height),
            drawer.handle_color);
    }

    if (std::any_of(parts.begin(), parts.end(),
                    [](const std::unique_ptr<CAlfaObject>& part) {
                        return !part;
                    })) {
        return {};
    }
    return parts;
}
