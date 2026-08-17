#pragma once

#include "CAssembled.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

struct DeskDefinition {
    int type = 0;
    int drawer_count = 3;
    int open_drawer = 0;
    double width = 1100.0;
    double depth = 500.0;
    double height = 700.0;
    double panel_thickness = 18.0;
    double back_panel_height = 300.0;
    double drawer_width = 350.0;
    double pullout_distance = 300.0;
};

struct DrawerBoxDefinition {
    double width = 600.0;
    double height = 800.0;
    double depth = 500.0;
    double panel_thickness = 18.0;
    double drawer_side_thickness = 12.0;
    double drawer_bottom_thickness = 6.0;
    double slide_clearance = 13.0;
    int facade_type = 0;
    int handle_type = 0;
    int drawer_count = 3;
    std::vector<double> drawer_heights{200.0, 200.0, 400.0};
    bool make_legs = true;
    double leg_height = 100.0;
    int open_drawer = 0;
    double pullout_distance = 300.0;
};

struct NikaKitchenDefinition {
    double width = 2600.0;
    double base_height = 800.0;
    double base_depth = 500.0;
    double upper_height = 800.0;
    double upper_depth = 300.0;
    double wall_gap = 450.0;
    double worktop_thickness = 38.0;
    double worktop_front_radius = 20.0;
    double panel_thickness = 18.0;
    double leg_height = 100.0;
    int facade_style = 2;
    int handle_type = 0;
    std::array<double, 8> door_open_angles{};
    int open_drawer = 0;
    double pullout_distance = 350.0;
};

struct CornerKitchenDefinition {
    double left_length = 2700.0;
    double right_length = 1800.0;
    double base_height = 800.0;
    double base_depth = 570.0;
    double upper_height = 800.0;
    double upper_depth = 300.0;
    double wall_gap = 600.0;
    double worktop_thickness = 38.0;
    double worktop_front_radius = 20.0;
    double panel_thickness = 18.0;
    double leg_height = 100.0;
    int facade_style = 2;
    int handle_type = 0;
    std::array<double, 8> left_door_open_angles{};
    std::array<double, 8> right_door_open_angles{};
    double lower_corner_door_angle = 0.0;
    double upper_corner_door_angle = 0.0;
};

struct ChairDefinition {
    int back_style = 0;
    int back_member_count = 5;
    double width = 430.0;
    double depth = 480.0;
    double seat_height = 470.0;
    double total_height = 980.0;
    double seat_thickness = 25.0;
    double seat_edge_radius = 8.0;
    double seat_back_curve = 55.0;
    double leg_size = 42.0;
    double rear_post_size = 45.0;
    double leg_taper = 5.0;
    double leg_inset = 12.0;
    double apron_height = 60.0;
    double apron_thickness = 22.0;
    double stretcher_height = 170.0;
    double stretcher_size = 24.0;
    double back_rake = 65.0;
    double rear_post_curve = -35.0;
    double back_member_width = 32.0;
    double back_member_thickness = 18.0;
    double back_rail_height = 55.0;
    double back_curve = 18.0;
    double back_depth_curve = 25.0;
};

class CDeskFurniture : public CAssembled {
public:
    using CAssembled::CAssembled;

    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const DeskDefinition& definition);
};

class CDrawerBoxFurniture : public CAssembled {
public:
    using CAssembled::CAssembled;

    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const DrawerBoxDefinition& definition);
};

class CNikaKitchenFurniture : public CAssembled {
public:
    using CAssembled::CAssembled;

    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const NikaKitchenDefinition& definition);
};

class CCornerKitchenFurniture : public CAssembled {
public:
    using CAssembled::CAssembled;

    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const CornerKitchenDefinition& definition);
    static bool BuildPartsIncremental(
        const CornerKitchenDefinition& definition,
        const std::function<void(std::unique_ptr<CAlfaObject>)>& part_ready);
};

class CChairFurniture : public CAssembled {
public:
    using CAssembled::CAssembled;

    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const ChairDefinition& definition);
};
