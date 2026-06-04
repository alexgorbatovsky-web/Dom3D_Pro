#include "ToolRegistry.h"

#include "../CMesh3D.h"
#include "../solid/Solid.h"
#include "../solid/SolidBoxTool.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>

#include <algorithm>
#include <memory>

namespace {
double param(const std::vector<ToolParameter>& parameters, const char* id, double fallback) {
    for (const ToolParameter& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }
    return fallback;
}

std::unique_ptr<CMesh3D> make_box(const std::string& name, float width, float height, float depth, float x, float y, float z, Color color) {
    auto mesh = std::make_unique<CMesh3D>(name);
    mesh->SetColor(color);

    const float x2 = x + width;
    const float y2 = y + height;
    const float z2 = z + depth;
    std::vector<Vec3> vertices = {
        {x, y, z}, {x2, y, z}, {x2, y2, z}, {x, y2, z},
        {x, y, z2}, {x2, y, z2}, {x2, y2, z2}, {x, y2, z2}
    };
    std::vector<CMesh3D::Face> faces = {
        {0, 1, 2, 3},
        {5, 4, 7, 6},
        {4, 0, 3, 7},
        {1, 5, 6, 2},
        {3, 2, 6, 7},
        {4, 5, 1, 0}
    };
    mesh->SetGeometry(std::move(vertices), std::move(faces));
    return mesh;
}

void replace_selected_mesh(CAlfaDoc& document, size_t index, std::unique_ptr<CMesh3D> mesh) {
    auto& objects = document.GetObjects();
    if (index >= objects.size() || !mesh) {
        return;
    }

    objects[index] = std::move(mesh);
}

void rebuild_box(CAlfaDoc& document,
                 size_t object_index,
                 const std::vector<ToolParameter>& parameters,
                 const std::string& name,
                 Color color,
                 float default_depth) {
    const float width = static_cast<float>(param(parameters, "width", 1.0));
    const float height = static_cast<float>(param(parameters, "height", 1.0));
    const float depth = static_cast<float>(param(parameters, "depth", default_depth));
    replace_selected_mesh(document, object_index, make_box(name, width, height, depth, -width * 0.5f, 0.0f, -depth * 0.5f, color));
}

enum class BooleanKind {
    Union,
    Cut,
    Common
};

bool build_boolean_shape(BooleanKind kind, const TopoDS_Shape& first, const TopoDS_Shape& second, TopoDS_Shape& result) {
    if (first.IsNull() || second.IsNull()) {
        return false;
    }

    if (kind == BooleanKind::Union) {
        BRepAlgoAPI_Fuse operation(first, second);
        operation.Build();
        if (!operation.IsDone()) {
            return false;
        }
        result = operation.Shape();
    } else if (kind == BooleanKind::Cut) {
        BRepAlgoAPI_Cut operation(first, second);
        operation.Build();
        if (!operation.IsDone()) {
            return false;
        }
        result = operation.Shape();
    } else {
        BRepAlgoAPI_Common operation(first, second);
        operation.Build();
        if (!operation.IsDone()) {
            return false;
        }
        result = operation.Shape();
    }

    return !result.IsNull();
}

void apply_boolean_to_selected_solids(CAlfaDoc& document, BooleanKind kind, const char* result_name, Color color) {
    const std::vector<size_t> selected = document.GetSelectedObjectIndices();
    if (selected.size() < 2) {
        return;
    }

    auto& objects = document.GetObjects();
    const size_t first_index = selected[0];
    const size_t second_index = selected[1];
    if (first_index >= objects.size() || second_index >= objects.size() || first_index == second_index) {
        return;
    }

    auto* first_solid = dynamic_cast<CSolid*>(objects[first_index].get());
    auto* second_solid = dynamic_cast<CSolid*>(objects[second_index].get());
    if (!first_solid || !second_solid) {
        return;
    }

    TopoDS_Shape result_shape;
    if (!build_boolean_shape(kind, first_solid->m_Shape, second_solid->m_Shape, result_shape)) {
        return;
    }

    auto result = std::make_unique<CSolid>(result_shape);
    result->SetName(result_name);
    result->SetColor(color);
    result->InitSurfaces();
    result->InitEdges();
    result->BuldMesh(0.1f);

    std::vector<size_t> erase_indices = {first_index, second_index};
    std::sort(erase_indices.begin(), erase_indices.end(), std::greater<size_t>());
    erase_indices.erase(std::unique(erase_indices.begin(), erase_indices.end()), erase_indices.end());
    for (size_t index : erase_indices) {
        if (index < objects.size()) {
            objects.erase(objects.begin() + static_cast<CAlfaDoc::ObjectList::difference_type>(index));
        }
    }

    document.AddObject(std::move(result));
}
}

ToolRegistry::ToolRegistry() {
    static const SolidBoxTool solid_box_tool;
    tools_.push_back(solid_box_tool.CreateToolDefinition());

    tools_.push_back({
        "boolean_union",
        "Boolean Union",
        {},
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            apply_boolean_to_selected_solids(document, BooleanKind::Union, "Boolean Union", {0.58f, 0.68f, 0.76f});
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "boolean_cut",
        "Boolean Cut",
        {},
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            apply_boolean_to_selected_solids(document, BooleanKind::Cut, "Boolean Cut", {0.70f, 0.55f, 0.42f});
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "boolean_common",
        "Boolean Common",
        {},
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            apply_boolean_to_selected_solids(document, BooleanKind::Common, "Boolean Common", {0.48f, 0.70f, 0.62f});
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "fillet_edge",
        "Fillet Edge",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "fillet_all_edges",
        "Fillet All Edges",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "stair",
        "Stair",
        {
            {"width", "Width", 1.2, 0.4, 4.0, 0.1},
            {"height", "Height", 1.4, 0.2, 4.0, 0.1},
            {"depth", "Depth", 2.6, 0.6, 6.0, 0.1}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            document.AddMesh(make_box("Parametric Stair", static_cast<float>(param(parameters, "width", 1.2)), static_cast<float>(param(parameters, "height", 1.4)), static_cast<float>(param(parameters, "depth", 2.6)), -0.6f, 0.0f, -1.3f, {0.70f, 0.47f, 0.28f}));
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_box(document, index, parameters, "Parametric Stair", {0.70f, 0.47f, 0.28f}, 2.6f);
        }
    });

    tools_.push_back({
        "window",
        "Window",
        {
            {"width", "Width", 1.4, 0.4, 4.0, 0.1},
            {"height", "Height", 1.1, 0.3, 3.0, 0.1},
            {"depth", "Frame Depth", 0.16, 0.05, 0.5, 0.01}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            document.AddMesh(make_box("Parametric Window", static_cast<float>(param(parameters, "width", 1.4)), static_cast<float>(param(parameters, "height", 1.1)), static_cast<float>(param(parameters, "depth", 0.16)), -0.7f, 0.8f, -0.08f, {0.40f, 0.68f, 0.88f}));
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_box(document, index, parameters, "Parametric Window", {0.40f, 0.68f, 0.88f}, 0.16f);
        }
    });

    tools_.push_back({
        "door",
        "Door",
        {
            {"width", "Width", 0.9, 0.5, 2.0, 0.05},
            {"height", "Height", 2.1, 1.2, 3.0, 0.1},
            {"depth", "Thickness", 0.12, 0.04, 0.35, 0.01}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            document.AddMesh(make_box("Parametric Door", static_cast<float>(param(parameters, "width", 0.9)), static_cast<float>(param(parameters, "height", 2.1)), static_cast<float>(param(parameters, "depth", 0.12)), -0.45f, 0.0f, -0.06f, {0.62f, 0.42f, 0.24f}));
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_box(document, index, parameters, "Parametric Door", {0.62f, 0.42f, 0.24f}, 0.12f);
        }
    });

    tools_.push_back({
        "cabinet",
        "Cabinet",
        {
            {"width", "Width", 1.6, 0.5, 4.0, 0.1},
            {"height", "Height", 0.9, 0.3, 2.6, 0.1},
            {"depth", "Depth", 0.7, 0.3, 2.0, 0.1}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            document.AddMesh(make_box("Parametric Cabinet", static_cast<float>(param(parameters, "width", 1.6)), static_cast<float>(param(parameters, "height", 0.9)), static_cast<float>(param(parameters, "depth", 0.7)), -0.8f, 0.0f, -0.35f, {0.38f, 0.56f, 0.43f}));
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_box(document, index, parameters, "Parametric Cabinet", {0.38f, 0.56f, 0.43f}, 0.7f);
        }
    });
}

const std::vector<ToolDefinition>& ToolRegistry::Tools() const {
    return tools_;
}

const ToolDefinition* ToolRegistry::Find(const std::string& id) const {
    for (const ToolDefinition& tool : tools_) {
        if (tool.id == id) {
            return &tool;
        }
    }
    return nullptr;
}

ActiveParametricObject ToolRegistry::Activate(const std::string& id, CAlfaDoc& document) const {
    const ToolDefinition* tool = Find(id);
    if (!tool) {
        return {};
    }

    tool->create(document, tool->defaults);
    if (tool->defaults.empty()) {
        return {};
    }
    return {tool->id, document.GetSelectedObjectIndex(), tool->defaults};
}

void ToolRegistry::Rebuild(const ActiveParametricObject& active_object, CAlfaDoc& document) const {
    const ToolDefinition* tool = Find(active_object.tool_id);
    if (tool && tool->rebuild) {
        tool->rebuild(document, active_object.object_index, active_object.parameters);
    }
}
