#include "SolidBoxTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <Standard_Real.hxx>

#include <algorithm>

const char* SolidBoxTool::GetID() const {
    return "SolidBox";
}

const char* SolidBoxTool::GetLabel() const {
    return "SolidBox";
}

const char* SolidBoxTool::GetHint() const {
    return "SolidBox_HINT";
}

bool SolidBoxTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidBoxTool::GetDefaultParameters() const {
    return {
        {"width", "Length", Width, 2.0, 900.0, 0.5},
        {"height", "Width", Height, 2.0, 900.0, 0.5},
        {"depth", "Height", Depth, 3.0, 900.0, 0.5},
        {"origin.x", "Origin X", 0.0, -1000000.0, 1000000.0, 0.1},
        {"origin.y", "Origin Y", 0.0, -1000000.0, 1000000.0, 0.1},
        {"origin.z", "Origin Z", 0.0, -1000000.0, 1000000.0, 0.1},
        {"axis.u.x", "U X", 1.0, -1.0, 1.0, 0.01},
        {"axis.u.y", "U Y", 0.0, -1.0, 1.0, 0.01},
        {"axis.u.z", "U Z", 0.0, -1.0, 1.0, 0.01},
        {"axis.v.x", "V X", 0.0, -1.0, 1.0, 0.01},
        {"axis.v.y", "V Y", 1.0, -1.0, 1.0, 0.01},
        {"axis.v.z", "V Z", 0.0, -1.0, 1.0, 0.01},
        {"axis.n.x", "Normal X", 0.0, -1.0, 1.0, 0.01},
        {"axis.n.y", "Normal Y", 0.0, -1.0, 1.0, 0.01},
        {"axis.n.z", "Normal Z", 1.0, -1.0, 1.0, 0.01}
    };
}

Color SolidBoxTool::GetColor() const {
    return kDefaultSolidObjectColor;
}

std::string SolidBoxTool::GetObjectName() const {
    return "Solid Box";
}

bool SolidBoxTool::DoParamOperation(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const {
    RebuildSolid(document, object_index, parameters);
    return object_index < document.GetObjects().size();
}

bool SolidBoxTool::RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const {
    const float width = static_cast<float>(std::max(GetParameter(parameters, "width", Width), 0.001));
    const float height = static_cast<float>(std::max(GetParameter(parameters, "height", Height), 0.001));
    const float depth = static_cast<float>(std::max(GetParameter(parameters, "depth", Depth), 0.001));
    const gp_Pnt origin(GetParameter(parameters, "origin.x", 0.0),
                        GetParameter(parameters, "origin.y", 0.0),
                        GetParameter(parameters, "origin.z", 0.0));
    gp_Dir normal(0.0, 0.0, 1.0);
    gp_Dir u_direction(1.0, 0.0, 0.0);
    gp_Dir v_direction(0.0, 1.0, 0.0);
    try {
        normal = gp_Dir(GetParameter(parameters, "axis.n.x", 0.0),
                        GetParameter(parameters, "axis.n.y", 0.0),
                        GetParameter(parameters, "axis.n.z", 1.0));
        u_direction = gp_Dir(GetParameter(parameters, "axis.u.x", 1.0),
                             GetParameter(parameters, "axis.u.y", 0.0),
                             GetParameter(parameters, "axis.u.z", 0.0));
        v_direction = gp_Dir(GetParameter(parameters, "axis.v.x", 0.0),
                             GetParameter(parameters, "axis.v.y", 1.0),
                             GetParameter(parameters, "axis.v.z", 0.0));
    } catch (...) {
        normal = gp_Dir(0.0, 0.0, 1.0);
        u_direction = gp_Dir(1.0, 0.0, 0.0);
        v_direction = gp_Dir(0.0, 1.0, 0.0);
    }

    return CreateBox(solid, width, height, depth, origin, normal, u_direction, v_direction);
}

std::unique_ptr<CAlfaObject> SolidBoxTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }

    return solid;
}
bool SolidBoxTool::CreateBox(CSolid& solid,
                             float width,
                             float height,
                             float depth,
                             const gp_Pnt& origin,
                             const gp_Dir& normal,
                             const gp_Dir& u_direction,
                             const gp_Dir& v_direction) const {
    const Standard_Real dx = width;
    const Standard_Real dy = height;
    const Standard_Real dz = depth;
    const gp_Pnt opposite(origin.X() + u_direction.X() * dx + v_direction.X() * dy + normal.X() * dz,
                          origin.Y() + u_direction.Y() * dx + v_direction.Y() * dy + normal.Y() * dz,
                          origin.Z() + u_direction.Z() * dx + v_direction.Z() * dy + normal.Z() * dz);
    const gp_Pnt min_corner(std::min(origin.X(), opposite.X()),
                            std::min(origin.Y(), opposite.Y()),
                            std::min(origin.Z(), opposite.Z()));
    const gp_Pnt max_corner(std::max(origin.X(), opposite.X()),
                            std::max(origin.Y(), opposite.Y()),
                            std::max(origin.Z(), opposite.Z()));
    solid.m_Shape = BRepPrimAPI_MakeBox(min_corner, max_corner).Shape();
    return solid.ReBuldMesh();
}
