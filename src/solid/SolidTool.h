#pragma once

#include "../Common.h"
#include "../CAlfaObject.h"
#include "../CMesh3D.h"
#include "../ui/ToolRegistry.h"

#include <memory>
#include <string>
#include <vector>

class CAlfaDoc;

class SolidTool {
public:
    virtual ~SolidTool() = default;

    virtual const char* GetID() const = 0;
    virtual const char* GetLabel() const = 0;
    virtual const char* GetHint() const;
    virtual bool PickEmptySpace() const;

    virtual std::vector<ToolParameter> GetDefaultParameters() const = 0;
    virtual Color GetColor() const;
    virtual std::string GetObjectName() const;
    std::vector<int> IndexCreatedSurfaces;

    ToolDefinition CreateToolDefinition() const;
    void CreateSolid(CAlfaDoc& document, const std::vector<ToolParameter>& parameters) const;
    void RebuildSolid(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const;

protected:
    virtual std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const = 0;

    static double GetParameter(const std::vector<ToolParameter>& parameters, const char* id, double fallback);
};
