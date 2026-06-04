#include "SolidTool.h"

#include "../CAlfaDoc.h"

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
    document.AddObject(CreateObject(parameters));
}

void SolidTool::RebuildSolid(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const {
    auto object = CreateObject(parameters);
    auto& objects = document.GetObjects();
    if (!object || object_index >= objects.size()) {
        return;
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
