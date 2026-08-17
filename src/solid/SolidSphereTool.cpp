#include "SolidSphereTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepPrimAPI_MakeSphere.hxx>
#include <Standard_Real.hxx>

#include <algorithm>

const char* SolidSphereTool::GetID() const {
    return "SolidSphereTool";
}

const char* SolidSphereTool::GetLabel() const {
    return "Ball";
}

const char* SolidSphereTool::GetHint() const {
    return "SolidSphereTool_HINT";
}

bool SolidSphereTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidSphereTool::GetDefaultParameters() const {
    return {
        {"diameter", "Diameter", Diameter, 0.1, 900.0, 0.1}
    };
}

Color SolidSphereTool::GetColor() const {
    return kDefaultSolidObjectColor;
}

std::string SolidSphereTool::GetObjectName() const {
    return "Ball";
}

bool SolidSphereTool::DoParamOperation(CAlfaDoc& document,
                                       size_t object_index,
                                       const std::vector<ToolParameter>& parameters) const {
    RebuildSolid(document, object_index, parameters);
    return object_index < document.GetObjects().size();
}

bool SolidSphereTool::RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const {
    const float diameter = static_cast<float>(std::max(GetParameter(parameters, "diameter", Diameter), 0.001));
    return CreateSphere(solid, diameter);
}

std::unique_ptr<CAlfaObject> SolidSphereTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }
    return solid;
}

bool SolidSphereTool::CreateSphere(CSolid& solid, float diameter) const {
    const Standard_Real radius = static_cast<Standard_Real>(diameter) * 0.5;
    BRepPrimAPI_MakeSphere sphere_builder(radius);
    sphere_builder.Build();
    if (!sphere_builder.IsDone()) {
        return false;
    }
    solid.m_Shape = sphere_builder.Shape();
    if (solid.m_Shape.IsNull()) {
        return false;
    }
    return solid.ReBuldMesh();
}
