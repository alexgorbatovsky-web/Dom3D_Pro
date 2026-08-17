#include "SolidBeamTool.h"

#include "BeamShapeBuilder.h"
#include "Solid.h"

#include <algorithm>
#include <memory>

const char* SolidBeamTool::GetID() const {
    return "SolidBeamTool";
}

const char* SolidBeamTool::GetLabel() const {
    return "Beam";
}

const char* SolidBeamTool::GetHint() const {
    return "Create a parametric structural beam";
}

std::vector<ToolParameter> SolidBeamTool::GetDefaultParameters() const {
    return {
        {"type", "Type", 0.0, 0.0, 5.0, 1.0, ToolParameterType::Combo,
            {"L Beam", "Square", "Channel", "T Beam", "I Beam", "Tube"}},
        {"width", "Width", 20.0, 0.1, 1000000.0, 0.5,
            ToolParameterType::Number, {}, ToolParameterUnit::Length},
        {"height", "Height", 20.0, 0.1, 1000000.0, 0.5,
            ToolParameterType::Number, {}, ToolParameterUnit::Length},
        {"length", "Length", 100.0, 0.1, 1000000.0, 1.0,
            ToolParameterType::Number, {}, ToolParameterUnit::Length},
        {"thick", "Thick", 2.0, 0.01, 1000000.0, 0.1,
            ToolParameterType::Number, {}, ToolParameterUnit::Length}
    };
}

std::string SolidBeamTool::GetObjectName() const {
    return "Beam";
}

bool SolidBeamTool::RebuildShape(
    CSolid& solid,
    const std::vector<ToolParameter>& parameters) const {
    const int section_index = std::clamp(
        static_cast<int>(GetParameter(parameters, "type", 0.0)), 0, 5);
    TopoDS_Shape shape;
    if (!BuildBeamShape(
            static_cast<BeamSectionType>(section_index),
            GetParameter(parameters, "width", 20.0),
            GetParameter(parameters, "height", 20.0),
            GetParameter(parameters, "length", 100.0),
            GetParameter(parameters, "thick", 2.0),
            shape)) {
        return false;
    }

    solid.m_Shape = shape;
    return solid.ReBuldMesh();
}

std::unique_ptr<CAlfaObject> SolidBeamTool::CreateObject(
    const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }
    return solid;
}
