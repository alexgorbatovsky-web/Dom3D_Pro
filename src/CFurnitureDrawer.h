#pragma once

#include "CAlfaObject.h"

#include <memory>
#include <string>
#include <vector>

enum class FurnitureDrawerFacadeStyle {
    Plain = 0,
    Frame = 1,
    Screen = 2,
    Milled = 3,
    Milano = 4
};

struct FurnitureDrawerDefinition {
    std::string part_prefix;
    std::string facade_name;
    std::string handle_name;

    double left = 0.0;
    double front = 0.0;
    double bottom = 0.0;
    double width = 400.0;
    double depth = 400.0;
    double height = 120.0;
    double side_thickness = 12.0;
    double bottom_thickness = 6.0;
    double bottom_offset = 7.0;

    double facade_left = 0.0;
    double facade_front = 0.0;
    double facade_bottom = 0.0;
    double facade_width = 400.0;
    double facade_height = 160.0;
    double facade_thickness = 18.0;
    FurnitureDrawerFacadeStyle facade_style = FurnitureDrawerFacadeStyle::Plain;

    bool make_facade = true;
    bool make_handle = true;
    bool round_handle = false;
    double handle_width_ratio = 0.35;
    double handle_max_width = 140.0;
    double handle_depth = 15.0;
    double handle_height = 10.0;
    double handle_gap = 15.0;
    double handle_height_ratio = 0.58;

    Color body_color{0.68f, 0.43f, 0.22f};
    Color facade_color{0.62f, 0.34f, 0.16f};
    Color handle_color{0.28f, 0.24f, 0.14f};
};

// Single geometry source for drawers used by furniture assemblies.
class CFurnitureDrawer {
public:
    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const FurnitureDrawerDefinition& definition);
};
