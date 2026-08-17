#pragma once

#include "SolidTool.h"

class CSolid;

class SolidTorusTool : public SolidTool {
public:
    const char* GetID() const override;
    const char* GetLabel() const override;
    const char* GetHint() const override;
    bool PickEmptySpace() const override;

    std::vector<ToolParameter> GetDefaultParameters() const override;
    Color GetColor() const override;
    std::string GetObjectName() const override;

    bool DoParamOperation(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const;
    bool RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const;

    float MajorDiameter = 30.0f;
    float TubeDiameter = 8.0f;

protected:
    std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const override;

private:
    bool CreateTorus(CSolid& solid, float major_diameter, float tube_diameter) const;
};
