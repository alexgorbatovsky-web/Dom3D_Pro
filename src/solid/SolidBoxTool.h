#pragma once

#include "SolidTool.h"

class CSolid;

class SolidBoxTool : public SolidTool {
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

    float Width = 2.0f;
    float Height = 2.0f;
    float Depth = 3.0f;

protected:
    std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const override;

private:
    bool CreateBox(CSolid& solid, float width, float height, float depth) const;
};
