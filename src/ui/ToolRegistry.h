#pragma once

#include "../CAlfaDoc.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

enum class ToolParameterType {
    Number,
    Checkbox,
    Combo,
    Material,
    Graph
};

enum class ToolParameterUnit {
    None,
    Length,
    Angle
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
    ToolParameterUnit unit = ToolParameterUnit::None;
    std::vector<double> option_values;
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
    bool transient = false;
};

class ToolRegistry {
public:
    ToolRegistry();

    const std::vector<ToolDefinition>& Tools() const;
    const ToolDefinition* Find(const std::string& id) const;
    ActiveParametricObject Activate(const std::string& id, CAlfaDoc& document) const;
    ActiveParametricObject CreateParametricObject(const std::string& id,
                                                  CAlfaDoc& document,
                                                  const std::vector<ToolParameter>& parameters) const;
    ActiveParametricObject PrepareParametricObject(
        const std::string& id,
        CAlfaDoc& document,
        const std::vector<ToolParameter>& parameters) const;
    ActiveParametricObject ApplyTrimToSelection(const std::string& id,
                                                CAlfaDoc& document) const;
    void AcceptTransientTrim(const ActiveParametricObject& active_object,
                             const CAlfaDoc& document) const;
    bool CancelTransientTrim(const ActiveParametricObject& active_object,
                             CAlfaDoc& document) const;
    ActiveParametricObject ApplySketchFeatureToSelection(CAlfaDoc& document) const;
    ActiveParametricObject ApplyOffsetFaceToSelection(CAlfaDoc& document) const;
    bool ApplyOffsetFaceOnce(CAlfaDoc& document, double distance) const;
    void Rebuild(const ActiveParametricObject& active_object, CAlfaDoc& document) const;
    bool ReplayOperations(size_t object_index, CAlfaDoc& document) const;
    bool ReplayProfileDependents(unsigned long profile_id, CAlfaDoc& document) const;
    bool ReplayAllProfileDependents(CAlfaDoc& document) const;
    bool ReplayAllTrimDependents(CAlfaDoc& document,
                                 unsigned long cutter_id = 0) const;
    ActiveParametricObject ActiveObjectFromDocument(size_t object_index,
                                                    const CAlfaObject& object,
                                                    size_t operation_index = 0,
                                                    CAlfaDoc* document = nullptr) const;
    std::string LabelFor(const std::string& id) const;

private:
    std::vector<ToolDefinition> tools_;
    mutable std::map<unsigned long, TopoDS_Shape> transient_trim_bases_;
};
