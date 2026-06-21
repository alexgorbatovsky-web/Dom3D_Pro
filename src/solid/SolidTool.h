#pragma once

#include "../Common.h"
#include "../CAlfaObject.h"
#include "../CMesh3D.h"
#include "../ui/ToolRegistry.h"
#include <gp_Trsf.hxx>

#include <memory>
#include <string>
#include <vector>

class CAlfaDoc;

class SolidTool {
public:
	int Type = 0;//0-Build,1-Modify,2-transform

    virtual ~SolidTool() = default;

    virtual const char* GetID() const = 0;
    virtual const char* GetLabel() const = 0;
    virtual const char* GetHint() const;
    virtual bool PickEmptySpace() const;

    virtual std::vector<ToolParameter> GetDefaultParameters() const = 0;
    virtual Color GetColor() const;
    virtual std::string GetObjectName() const;
    mutable std::vector<int> IndexCreatedSurfaces;

    ToolDefinition CreateToolDefinition() const;
    void CreateSolid(CAlfaDoc& document, const std::vector<ToolParameter>& parameters) const;
    void RebuildSolid(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const;

protected:
    virtual std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const = 0;

    static double GetParameter(const std::vector<ToolParameter>& parameters, const char* id, double fallback);
};

//

class SolidTransformTool : public SolidTool {
public:
    SolidTransformTool();

    SolidTransformTool(gp_Trsf tr, int type);
    const char* GetID() const override;
    const char* GetLabel() const override;
    const char* GetHint() const override;
    bool PickEmptySpace() const override;

    std::vector<ToolParameter> GetDefaultParameters() const override;
    Color GetColor() const override;
    std::string GetObjectName() const override;
    std::unique_ptr<CAlfaObject> CreateObject(const std::vector<ToolParameter>& parameters) const override;


    gp_Trsf transform;
	int Type = 0;//0-translate,1-rotate,2-scale 

};
