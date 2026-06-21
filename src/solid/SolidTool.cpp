#include "SolidTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

const char* SolidTool::GetHint() const {
    return "";
}

bool SolidTool::PickEmptySpace() const {
    return true;
}

Color SolidTool::GetColor() const {
    return {0.58f, 0.68f, 0.76f};
}

std::string SolidTool::GetObjectName() const {
    return GetLabel();
}

ToolDefinition SolidTool::CreateToolDefinition() const {
    return {
        GetID(),
        GetLabel(),
        GetDefaultParameters(),
        [this](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            CreateSolid(document, parameters);
        },
        [this](CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) {
            RebuildSolid(document, object_index, parameters);
        }
    };
}

void SolidTool::CreateSolid(CAlfaDoc& document, const std::vector<ToolParameter>& parameters) const {
    IndexCreatedSurfaces.clear();
    auto object = CreateObject(parameters);
    if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
        IndexCreatedSurfaces.reserve(static_cast<size_t>(solid->GetNumSurfaces()));
        for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
            IndexCreatedSurfaces.push_back(i);
        }
        solid->SetParametricOperation(0,
                                      GetID(),
                                      GetLabel(),
                                      {},
                                      IndexCreatedSurfaces);
    }
    document.AddObject(std::move(object));
}

void SolidTool::RebuildSolid(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const {
    auto object = CreateObject(parameters);
    auto& objects = document.GetObjects();
    if (!object || object_index >= objects.size()) {
        return;
    }
    IndexCreatedSurfaces.clear();
    if (const auto* solid = dynamic_cast<const CSolid*>(object.get())) {
        IndexCreatedSurfaces.reserve(static_cast<size_t>(solid->GetNumSurfaces()));
        for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
            IndexCreatedSurfaces.push_back(i);
        }
    }

    if (objects[object_index]) {
        object->m_id = objects[object_index]->m_id;
        object->SetName(objects[object_index]->GetName());
        object->SetMaterial(objects[object_index]->GetMaterial());
        object->SetMaterialId(objects[object_index]->GetMaterialId());
        object->SetGroupName(objects[object_index]->GetGroupName());
        object->SetVisible(objects[object_index]->IsVisible());
        if (auto* new_solid = dynamic_cast<CSolid*>(object.get())) {
            if (const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get())) {
                new_solid->CopyOperationTreeFrom(*old_solid);
            }
        }
    }
    objects[object_index] = std::move(object);
}

double SolidTool::GetParameter(const std::vector<ToolParameter>& parameters, const char* id, double fallback) {
    for (const ToolParameter& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }

    return fallback;
}

//==========================
SolidTransformTool::SolidTransformTool()
{
    Type = 0;
}
SolidTransformTool::SolidTransformTool(gp_Trsf tr, int type)
    : transform(tr), Type(type) {
}
const char* SolidTransformTool::GetID() const {
    return "SolidTransform";
}

const char* SolidTransformTool::GetLabel() const {
    return "SolidTransform";
}

const char* SolidTransformTool::GetHint() const {
    return "SolidTransform_HINT";
}

bool SolidTransformTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidTransformTool::GetDefaultParameters() const {
    return {
        {"type", "Type", static_cast<double>(Type), 0.0, 2.0, 1.0, ToolParameterType::Combo, {"Move", "Rotate", "Scale"}},
        {"dx", "Move X", 0.0, -900.0, 900.0, 0.1},
        {"dy", "Move Y", 0.0, -900.0, 900.0, 0.1},
        {"dz", "Move Z", 0.0, -900.0, 900.0, 0.1},
        {"center.x", "Center X", 0.0, -900.0, 900.0, 0.1},
        {"center.y", "Center Y", 0.0, -900.0, 900.0, 0.1},
        {"center.z", "Center Z", 0.0, -900.0, 900.0, 0.1},
        {"axis.x", "Axis X", 0.0, -1.0, 1.0, 0.1},
        {"axis.y", "Axis Y", 0.0, -1.0, 1.0, 0.1},
        {"axis.z", "Axis Z", 1.0, -1.0, 1.0, 0.1},
        {"angle", "Angle", 0.0, -6.28318530718, 6.28318530718, 0.0174532925199433},
        {"factor", "Scale", 1.0, 0.001, 100.0, 0.01}
    };
}

Color SolidTransformTool::GetColor() const {
    return { 0.64f, 0.70f, 0.58f };
}

std::string SolidTransformTool::GetObjectName() const {
    return "Solid Transform";
}

std::unique_ptr<CAlfaObject> SolidTransformTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    (void)parameters;
    return nullptr;
}
