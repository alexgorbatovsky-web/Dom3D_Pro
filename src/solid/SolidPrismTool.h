#pragma once

#include "SolidTool.h"

class CSolid;

class SolidPrismTool : public SolidTool {
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

    float Length = 20.0f;
    int Qty = 6;
    float Height = 40.0f;
    int Axis = 1;

protected:
    std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const override;

private:
    bool CreatePrism(CSolid& solid, float length, int qty, float height, int axis) const;
};
