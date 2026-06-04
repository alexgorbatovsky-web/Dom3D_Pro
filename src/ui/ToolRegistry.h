#pragma once

#include "../CAlfaDoc.h"

#include <functional>
#include <string>
#include <vector>

struct ToolParameter {
    std::string id;
    std::string label;
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    double step = 0.1;
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
    std::vector<ToolParameter> parameters;
};

class ToolRegistry {
public:
    ToolRegistry();

    const std::vector<ToolDefinition>& Tools() const;
    const ToolDefinition* Find(const std::string& id) const;
    ActiveParametricObject Activate(const std::string& id, CAlfaDoc& document) const;
    void Rebuild(const ActiveParametricObject& active_object, CAlfaDoc& document) const;

private:
    std::vector<ToolDefinition> tools_;
};
