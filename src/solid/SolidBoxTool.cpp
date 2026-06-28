#include "SolidBoxTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepPrimAPI_MakeBox.hxx>
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
        {"width", "Width", Width, 2.0, 900.0, 0.5},
        {"height", "Height", Height, 2.0, 900.0, 0.5},
        {"depth", "Depth", Depth, 3.0, 900.0, 0.5}
    };
}

Color SolidBoxTool::GetColor() const {
    return {0.64f, 0.70f, 0.58f};
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
    return CreateBox(solid, width, height, depth);
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
bool SolidBoxTool::CreateBox(CSolid& solid, float width, float height, float depth) const {
    const Standard_Real dx = width;
    const Standard_Real dy = height;
    const Standard_Real dz = depth;
    solid.m_Shape = BRepPrimAPI_MakeBox(dx, dy, dz).Shape();
    return solid.ReBuldMesh();
}
