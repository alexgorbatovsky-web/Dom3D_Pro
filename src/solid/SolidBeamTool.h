#pragma once

#include "SolidTool.h"

class CSolid;

class SolidBeamTool : public SolidTool {
public:
    const char* GetID() const override;
    const char* GetLabel() const override;
    const char* GetHint() const override;

    std::vector<ToolParameter> GetDefaultParameters() const override;
    std::string GetObjectName() const override;

    bool RebuildShape(CSolid& solid,
                      const std::vector<ToolParameter>& parameters) const;

protected:
    std::unique_ptr<CAlfaObject> CreateObject(
        const std::vector<ToolParameter>& parameters) const override;
};
