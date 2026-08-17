#pragma once

#include "SolidTool.h"

class CSolid;
class gp_Dir;
class gp_Pnt;

class SolidCylinderTool : public SolidTool {
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

    float Diameter = 10.0f;
    float Height = 12.0f;

protected:
    std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const override;

private:
    bool CreateCylinder(CSolid& solid,
                        float diameter,
                        float height,
                        const gp_Pnt& origin,
                        const gp_Dir& normal,
                        const gp_Dir& u_direction) const;
};
