#include "SolidBoxTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Standard_Real.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>

const char* SolidBoxTool::GetID() const {
    return "SolidBox";
}

const char* SolidBoxTool::GetLabel() const {
    return "SolidBox";
}

const char* SolidBoxTool::GetHint() const {
    return "SolidBox_HINT";
}

bool SolidBoxTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidBoxTool::GetDefaultParameters() const {
    return {
        {"width", "Length", Width, 2.0, 900.0, 0.5},
        {"height", "Width", Height, 2.0, 900.0, 0.5},
        {"depth", "Height", Depth, -900.0, 900.0, 0.5},
        {"origin.x", "Origin X", 0.0, -1000000.0, 1000000.0, 0.1},
        {"origin.y", "Origin Y", 0.0, -1000000.0, 1000000.0, 0.1},
        {"origin.z", "Origin Z", 0.0, -1000000.0, 1000000.0, 0.1},
        {"axis.u.x", "U X", 1.0, -1.0, 1.0, 0.01},
        {"axis.u.y", "U Y", 0.0, -1.0, 1.0, 0.01},
        {"axis.u.z", "U Z", 0.0, -1.0, 1.0, 0.01},
        {"axis.v.x", "V X", 0.0, -1.0, 1.0, 0.01},
        {"axis.v.y", "V Y", 1.0, -1.0, 1.0, 0.01},
        {"axis.v.z", "V Z", 0.0, -1.0, 1.0, 0.01},
        {"axis.n.x", "Normal X", 0.0, -1.0, 1.0, 0.01},
        {"axis.n.y", "Normal Y", 0.0, -1.0, 1.0, 0.01},
        {"axis.n.z", "Normal Z", 1.0, -1.0, 1.0, 0.01}
    };
}

Color SolidBoxTool::GetColor() const {
    return kDefaultSolidObjectColor;
}

std::string SolidBoxTool::GetObjectName() const {
    return "Solid Box";
}

bool SolidBoxTool::DoParamOperation(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const {
    RebuildSolid(document, object_index, parameters);
    return object_index < document.GetObjects().size();
}

bool SolidBoxTool::RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const {
	constexpr double kFaceBooleanOverlap = 0.01;
    const float width = static_cast<float>(std::max(GetParameter(parameters, "width", Width), 0.001));
    const float height = static_cast<float>(std::max(GetParameter(parameters, "height", Height), 0.001));
    const float depth = static_cast<float>(GetParameter(parameters, "depth", Depth));
    if (std::fabs(depth) <= 0.001f) {
        return false;
    }
	gp_Pnt origin(GetParameter(parameters, "origin.x", 0.0),
                        GetParameter(parameters, "origin.y", 0.0),
                        GetParameter(parameters, "origin.z", 0.0));
    gp_Dir normal(0.0, 0.0, 1.0);
    gp_Dir u_direction(1.0, 0.0, 0.0);
    gp_Dir v_direction(0.0, 1.0, 0.0);
    try {
        normal = gp_Dir(GetParameter(parameters, "axis.n.x", 0.0),
                        GetParameter(parameters, "axis.n.y", 0.0),
                        GetParameter(parameters, "axis.n.z", 1.0));
        u_direction = gp_Dir(GetParameter(parameters, "axis.u.x", 1.0),
                             GetParameter(parameters, "axis.u.y", 0.0),
                             GetParameter(parameters, "axis.u.z", 0.0));
        v_direction = gp_Dir(GetParameter(parameters, "axis.v.x", 0.0),
                             GetParameter(parameters, "axis.v.y", 1.0),
                             GetParameter(parameters, "axis.v.z", 0.0));
    } catch (...) {
        normal = gp_Dir(0.0, 0.0, 1.0);
        u_direction = gp_Dir(1.0, 0.0, 0.0);
        v_direction = gp_Dir(0.0, 1.0, 0.0);
    }
	if (GetParameter(parameters, "boolean.body_id", 0.0) > 0.0) {
		gp_Dir extrusion_direction = normal;
		if (depth < 0.0f)
			extrusion_direction.Reverse();
		origin.Translate(gp_Vec(extrusion_direction)
			* -kFaceBooleanOverlap);
	}

    return CreateBox(solid, width, height, depth, origin, normal, u_direction, v_direction);
}

std::unique_ptr<CAlfaObject> SolidBoxTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }

    return solid;
}
bool SolidBoxTool::CreateBox(CSolid& solid,
                             float width,
                             float height,
                             float depth,
                             const gp_Pnt& origin,
                             const gp_Dir& normal,
                             const gp_Dir& u_direction,
                             const gp_Dir& v_direction) const {
    try {
        const gp_Vec u(
            u_direction.X() * width,
            u_direction.Y() * width,
            u_direction.Z() * width);
        const gp_Vec v(
            v_direction.X() * height,
            v_direction.Y() * height,
            v_direction.Z() * height);
        const gp_Pnt p0 = origin;
        const gp_Pnt p1 = p0.Translated(u);
        const gp_Pnt p2 = p1.Translated(v);
        const gp_Pnt p3 = p0.Translated(v);

        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(p0);
        polygon.Add(p1);
        polygon.Add(p2);
        polygon.Add(p3);
        polygon.Close();
        if (!polygon.IsDone()) {
            return false;
        }

        const TopoDS_Wire wire = polygon.Wire();
        BRepBuilderAPI_MakeFace face_builder(wire);
        if (!face_builder.IsDone()) {
            return false;
        }
        const TopoDS_Face face = face_builder.Face();
        const gp_Vec extrusion(
            normal.X() * depth,
            normal.Y() * depth,
            normal.Z() * depth);
        BRepPrimAPI_MakePrism prism(face, extrusion);
        prism.Build();
        if (!prism.IsDone()) {
            return false;
        }
        solid.m_Shape = prism.Shape();
        return !solid.m_Shape.IsNull() && solid.ReBuldMesh();
    } catch (...) {
        return false;
    }
}

