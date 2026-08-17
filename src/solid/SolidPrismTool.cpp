#include "SolidPrismTool.h"

#include "../CAlfaDoc.h"
#include "../CPolyline.h"
#include "Solid.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>

const char* SolidPrismTool::GetID() const {
    return "SolidPrismTool";
}

const char* SolidPrismTool::GetLabel() const {
    return "Prism";
}

const char* SolidPrismTool::GetHint() const {
    return "SolidPrismTool_HINT";
}

bool SolidPrismTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidPrismTool::GetDefaultParameters() const {
    return {
        {"length", "Length", Length, 0.1, 900.0, 0.1},
        {"qty", "Qty", static_cast<double>(Qty), 3.0, 128.0, 1.0},
        {"height", "Height", Height, 0.1, 900.0, 0.1},
        {"axis", "Axis Type", static_cast<double>(Axis), 0.0, 2.0, 1.0,
         ToolParameterType::Combo, {"Axis X", "Axis Y", "Axis Z"}}
    };
}

Color SolidPrismTool::GetColor() const {
    return kDefaultSolidObjectColor;
}

std::string SolidPrismTool::GetObjectName() const {
    return "Prism";
}

bool SolidPrismTool::DoParamOperation(CAlfaDoc& document,
                                      size_t object_index,
                                      const std::vector<ToolParameter>& parameters) const {
    RebuildSolid(document, object_index, parameters);
    return object_index < document.GetObjects().size();
}

bool SolidPrismTool::RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const {
    const float length = static_cast<float>(std::max(GetParameter(parameters, "length", Length), 0.001));
    const int qty = std::clamp(static_cast<int>(GetParameter(parameters, "qty", Qty)), 3, 128);
    const float height = static_cast<float>(std::max(GetParameter(parameters, "height", Height), 0.001));
    const int axis = std::clamp(static_cast<int>(GetParameter(parameters, "axis", Axis)), 0, 2);
    return CreatePrism(solid, length, qty, height, axis);
}

std::unique_ptr<CAlfaObject> SolidPrismTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }
    return solid;
}

bool SolidPrismTool::CreatePrism(CSolid& solid, float length, int qty, float height, int axis) const {
    CPolyline polygon;
    if (!polygon.CreatePolygone(length, qty)) {
        return false;
    }

    BRepBuilderAPI_MakePolygon polygon_builder;
    const std::vector<CPoint3d>& points = polygon.GetPoints();
    for (int i = 0; i < qty && i < static_cast<int>(points.size()); ++i) {
        const CPoint3d& point = points[static_cast<size_t>(i)];
        if (axis == 0) {
            polygon_builder.Add(gp_Pnt(0.0, point.y, -point.x));
        } else if (axis == 1) {
            polygon_builder.Add(gp_Pnt(point.x, 0.0, point.y));
        } else {
            polygon_builder.Add(gp_Pnt(point.x, point.y, 0.0));
        }
    }
    polygon_builder.Close();
    if (!polygon_builder.IsDone()) {
        return false;
    }

    const TopoDS_Face face = BRepBuilderAPI_MakeFace(polygon_builder.Wire());
    const gp_Vec direction = axis == 0
        ? gp_Vec(height, 0.0, 0.0)
        : axis == 1 ? gp_Vec(0.0, height, 0.0) : gp_Vec(0.0, 0.0, height);
    BRepPrimAPI_MakePrism prism_builder(face, direction, true);
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
        return false;
    }

    solid.m_Shape = prism_builder.Shape();
    if (solid.m_Shape.IsNull()) {
        return false;
    }
    return solid.ReBuldMesh();
}
