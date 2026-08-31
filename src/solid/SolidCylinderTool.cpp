#include "SolidCylinderTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Standard_Real.hxx>

#include <algorithm>
#include <cmath>

const char* SolidCylinderTool::GetID() const {
    return "SolidCylinder";
}

const char* SolidCylinderTool::GetLabel() const {
    return "Cylinder";
}

const char* SolidCylinderTool::GetHint() const {
    return "SolidCylinder_HINT";
}

bool SolidCylinderTool::PickEmptySpace() const {
    return true;
}

std::vector<ToolParameter> SolidCylinderTool::GetDefaultParameters() const {
    return {
        {"diameter", "Diameter", Diameter, 0.1, 900.0, 0.1},
        {"height", "Height", Height, -900.0, 900.0, 0.1},
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

Color SolidCylinderTool::GetColor() const {
    return kDefaultSolidObjectColor;
}

std::string SolidCylinderTool::GetObjectName() const {
    return "Solid Cylinder";
}

bool SolidCylinderTool::DoParamOperation(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) const {
    RebuildSolid(document, object_index, parameters);
    return object_index < document.GetObjects().size();
}

bool SolidCylinderTool::RebuildShape(CSolid& solid, const std::vector<ToolParameter>& parameters) const {
	constexpr double kFaceBooleanOverlap = 0.01;
    const float diameter = static_cast<float>(std::max(GetParameter(parameters, "diameter", Diameter), 0.001));
    const float height = static_cast<float>(GetParameter(parameters, "height", Height));
    if (std::fabs(height) <= 0.001f) {
        return false;
    }
	gp_Pnt origin(GetParameter(parameters, "origin.x", 0.0),
                        GetParameter(parameters, "origin.y", 0.0),
                        GetParameter(parameters, "origin.z", 0.0));
    try {
        const gp_Dir normal(GetParameter(parameters, "axis.n.x", 0.0),
                            GetParameter(parameters, "axis.n.y", 0.0),
                            GetParameter(parameters, "axis.n.z", 1.0));
        const gp_Dir u_direction(GetParameter(parameters, "axis.u.x", 1.0),
                                 GetParameter(parameters, "axis.u.y", 0.0),
                                 GetParameter(parameters, "axis.u.z", 0.0));
		if (GetParameter(parameters, "boolean.body_id", 0.0) > 0.0) {
			gp_Dir extrusion_direction = normal;
			if (height < 0.0f)
				extrusion_direction.Reverse();
			origin.Translate(gp_Vec(extrusion_direction)
				* -kFaceBooleanOverlap);
		}
        return CreateCylinder(solid, diameter, height, origin, normal, u_direction);
    } catch (...) {
        return false;
    }
}

std::unique_ptr<CAlfaObject> SolidCylinderTool::CreateObject(const std::vector<ToolParameter>& parameters) const {
    auto solid = std::make_unique<CSolid>();
    solid->SetName(GetObjectName());
    solid->SetColor(GetColor());
    if (!RebuildShape(*solid, parameters)) {
        return nullptr;
    }

    return solid;
}

bool SolidCylinderTool::CreateCylinder(CSolid& solid,
                                       float diameter,
                                       float height,
                                       const gp_Pnt& origin,
                                       const gp_Dir& normal,
                                       const gp_Dir& u_direction) const {
    const Standard_Real radius = static_cast<Standard_Real>(diameter) * 0.5;
    try {
        gp_Dir extrusion_direction = normal;
        if (height < 0.0f) {
            extrusion_direction.Reverse();
        }
        BRepPrimAPI_MakeCylinder builder(
            gp_Ax2(origin, extrusion_direction, u_direction),
            radius,
            std::fabs(height));
        builder.Build();
        if (!builder.IsDone()) {
            return false;
        }
        solid.m_Shape = builder.Shape();
        return !solid.m_Shape.IsNull() && solid.ReBuldMesh();
    } catch (...) {
        return false;
    }
}
