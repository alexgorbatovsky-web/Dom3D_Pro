#include "SolidTorusTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepPrimAPI_MakeTorus.hxx>
#include <Standard_Real.hxx>

#include <algorithm>

const char* SolidTorusTool::GetID() const {
    return "SolidTorusTool";
}

const char* SolidTorusTool::GetLabel() const {
    return "Torus";
}

const char* SolidTorusTool::GetHint() const {
    return "SolidTorusTool_HINT";
}

bool SolidTorusTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidTorusTool::GetDefaultParameters() const {
    return {
        {"major_diameter", "Major Diameter", MajorDiameter, 0.2, 900.0, 0.1},
        {"tube_diameter", "Tube Diameter", TubeDiameter, 0.1, 900.0, 0.1}
    };
}

Color SolidTorusTool::GetColor() const {
    return kDefaultSolidObjectColor;
}

std::string SolidTorusTool::GetObjectName() const {
    return "Torus";
}

bool SolidTorusTool::DoParamOperation(CAlfaDoc& document,
                                      size_t object_index,
                                      const std::vector<ToolParameter>& parameters) const {
    RebuildSolid(document, object_index, parameters);
    return object_index < document.GetObjects().size();
}

bool SolidTorusTool::RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const {
    const float major_diameter = static_cast<float>(std::max(GetParameter(parameters, "major_diameter", MajorDiameter), 0.001));
    const float tube_diameter = static_cast<float>(std::max(GetParameter(parameters, "tube_diameter", TubeDiameter), 0.001));
    return CreateTorus(solid, major_diameter, tube_diameter);
}

std::unique_ptr<CAlfaObject> SolidTorusTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }
    return solid;
}

bool SolidTorusTool::CreateTorus(CSolid& solid, float major_diameter, float tube_diameter) const {
    const Standard_Real major_radius = static_cast<Standard_Real>(major_diameter) * 0.5;
    const Standard_Real tube_radius = static_cast<Standard_Real>(tube_diameter) * 0.5;
    if (major_radius <= tube_radius) {
        return false;
    }

    BRepPrimAPI_MakeTorus torus_builder(major_radius, tube_radius);
    torus_builder.Build();
    if (!torus_builder.IsDone()) {
        return false;
    }
    solid.m_Shape = torus_builder.Shape();
    if (solid.m_Shape.IsNull()) {
        return false;
    }
    return solid.ReBuldMesh();
}
