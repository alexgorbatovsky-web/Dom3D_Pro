#pragma once

#include "CAssembled.h"
#include "CFacadeFurniture.h"

#include <cstdint>
#include <string>
#include <vector>

class CSmartLine;

enum class KitchenCabinetBodyType : std::uint8_t {
    Straight = 0,
    Corner = 1,
    Radius = 2,
    Corner2 = 3,
    Radius2 = 4,
    Radius3 = 5,
    Radius4 = 6
};

enum class KitchenCabinetFacadeType : std::uint8_t {
    Open = 0,
    SingleDoor = 1,
    DoubleDoor = 2
};

struct KitchenCabinetDefinition {
    KitchenCabinetBodyType body_type = KitchenCabinetBodyType::Straight;
    KitchenCabinetFacadeType facade_type = KitchenCabinetFacadeType::DoubleDoor;
    KitchenCabinetFacadeStyle facade_style = KitchenCabinetFacadeStyle::Plain;
    // Screen is an opaque facade style.  A transparent/decorative filling is
    // enabled only by the separate Cabinet Showcase tool.
    KitchenCabinetShowcaseFill showcase_fill = KitchenCabinetShowcaseFill::None;
    int shelf_count = 2;
    double width = 600.0;
    double depth = 560.0;
    double height = 720.0;
    double panel_thickness = 18.0;
    double milling_depth = 9.0;
    double facade_bulge = 280.0;
    double radius2_bulge = 120.0;
    double radius_side_straight = 180.0;
    double door_open_angle = 0.0;
    double left_door_open_angle = 0.0;
    double right_door_open_angle = 0.0;
    int door_hinge_side = 0;
    int handle_orientation = 0;
};

struct KitchenCabinetDoorAnimation {
    Vec3 hinge{0.0f, 0.0f, 0.0f};
    float angle_sign = 0.0f;
};

class CKitchenCabinet : public CAssembled {
public:
    explicit CKitchenCabinet(std::string name = "Kitchen Cabinet");
    CKitchenCabinet(std::string name,
                    std::vector<unsigned long> element_ids,
                    KitchenCabinetDefinition definition = {});

    const KitchenCabinetDefinition& GetDefinition() const;
    bool SetDefinition(const KitchenCabinetDefinition& definition);

    KitchenCabinetBodyType GetBodyType() const;
    KitchenCabinetFacadeType GetFacadeType() const;
    KitchenCabinetFacadeStyle GetFacadeStyle() const;
    int GetShelfCount() const;
    double GetWidth() const;
    double GetDepth() const;
    double GetHeight() const;
    double GetPanelThickness() const;
    double GetFacadeBulge() const;
    double GetRadius2Bulge() const;
    double GetRadiusSideStraight() const;

    // Returns the local hinge and the signed rotation direction for a door.
    // Corner bi-fold doors are excluded because their two leaves have coupled
    // rotations around different hinges.
    bool GetDoorAnimation(const std::string& part_name,
                          KitchenCabinetDoorAnimation* animation) const;

    // Builds all solid parts of a cabinet in their stable assembly order.
    // ToolRegistry only adds these parts to the document and assigns materials.
    static std::vector<std::unique_ptr<CAlfaObject>> BuildParts(
        const KitchenCabinetDefinition& definition,
        const CSmartLine* frame_profile = nullptr,
        const CSmartLine* panel_profile = nullptr,
        const CSmartLine* milling_profile = nullptr,
        const std::vector<const CSmartLine*>& milling_guides = {});

    bool Save(std::ostream& stream) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;

    static bool IsValid(const KitchenCabinetDefinition& definition);

private:
    KitchenCabinetDefinition definition_;
};
