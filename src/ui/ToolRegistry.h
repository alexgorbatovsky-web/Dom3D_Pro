#pragma once

#include "../CAlfaDoc.h"

#include <functional>
#include <string>
#include <vector>

enum class ToolParameterType {
    Number,
    Checkbox,
    Combo
};

struct ToolParameter {
    std::string id;
    std::string label;
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    double step = 0.1;
    ToolParameterType type = ToolParameterType::Number;
    std::vector<std::string> options;
};

struct ToolDefinition {
    std::string id;
    std::string label;
    std::vector<ToolParameter> defaults;
    std::function<void(CAlfaDoc&, const std::vector<ToolParameter>&)> create;
    std::function<void(CAlfaDoc&, size_t, const std::vector<ToolParameter>&)> rebuild;
};

struct ActiveParametricObject {
    std::string tool_id;
    size_t object_index = 0;
    size_t operation_index = 0;
    std::vector<ToolParameter> parameters;
};

class ToolRegistry {
public:
    ToolRegistry();

    const std::vector<ToolDefinition>& Tools() const;
    const ToolDefinition* Find(const std::string& id) const;
    ActiveParametricObject Activate(const std::string& id, CAlfaDoc& document) const;
    void Rebuild(const ActiveParametricObject& active_object, CAlfaDoc& document) const;
    bool ReplayOperations(size_t object_index, CAlfaDoc& document) const;
    bool ReplayProfileDependents(unsigned long profile_id, CAlfaDoc& document) const;
    bool ReplayAllProfileDependents(CAlfaDoc& document) const;
    ActiveParametricObject ActiveObjectFromDocument(size_t object_index, const CAlfaObject& object, size_t operation_index = 0) const;
    std::string LabelFor(const std::string& id) const;

private:
    std::vector<ToolDefinition> tools_;
};
