#include "SolidCylinderTool.h"

#include "../CAlfaDoc.h"
#include "Solid.h"

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <gp_Ax1.hxx>
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
        {"base_chamfer", "Hole Entrance Chamfer (45 deg)", 0.0, 0.0, 1.0, 1.0, ToolParameterType::Checkbox},
        {"base_chamfer_size", "Chamfer Size", 1.0, 0.01, 900.0, 0.1,
            ToolParameterType::Number, {}, ToolParameterUnit::Length},
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
	// Fixed model-space overlap; obsolete screen-based values in history are ignored.
    constexpr double overlap = 0.001;
    const float diameter = static_cast<float>(std::max(GetParameter(parameters, "diameter", Diameter), 0.001));
    float height = static_cast<float>(GetParameter(parameters, "height", Height));
    double entrance_chamfer = 0.0;
    if (height < -0.001f && GetParameter(parameters, "boolean.body_id", 0.0) > 0.0
        && GetParameter(parameters, "base_chamfer", 0.0) >= 0.5) {
        const double size = GetParameter(parameters, "base_chamfer_size", 1.0);
        if (!std::isfinite(size) || size <= 0.0 || size >= std::fabs(height) - 1.0e-6) {
            return false;
        }
        // Extend the cone outside the face by the Boolean overlap so the
        // requested radial and axial size is measured at the actual base plane.
        entrance_chamfer = size + overlap;
    }
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
				* -overlap);
            // Keep the far end at the requested distance from the original face.
            height += static_cast<float>(std::copysign(overlap, height));
		}
        return CreateCylinder(solid, diameter, height, origin, normal, u_direction, entrance_chamfer);
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
                                       const gp_Dir& u_direction,
                                       double entrance_chamfer) const {
    const Standard_Real radius = static_cast<Standard_Real>(diameter) * 0.5;
    try {
        gp_Dir extrusion_direction = normal;
        if (height < 0.0f) {
            extrusion_direction.Reverse();
        }
        const gp_Ax2 frame(origin, extrusion_direction, u_direction);
        TopoDS_Shape shape;
        if (entrance_chamfer > 0.0) {
            // Revolve a widened entrance followed by the nominal bore. Equal
            // radial and axial offsets make the entrance cone exactly 45 degrees.
            const auto point = [&](double r, double z) {
                return origin.Translated(gp_Vec(frame.XDirection()) * r
                    + gp_Vec(extrusion_direction) * z);
            };
            BRepBuilderAPI_MakePolygon profile;
            profile.Add(point(0.0, 0.0));
            profile.Add(point(radius + entrance_chamfer, 0.0));
            profile.Add(point(radius, entrance_chamfer));
            profile.Add(point(radius, std::fabs(height)));
            profile.Add(point(0.0, std::fabs(height)));
            profile.Close();
            if (!profile.IsDone()) return false;
            BRepBuilderAPI_MakeFace face(profile.Wire());
            if (!face.IsDone()) return false;
            BRepPrimAPI_MakeRevol builder(face.Face(), gp_Ax1(origin, extrusion_direction));
            builder.Build();
            if (!builder.IsDone()) return false;
            shape = builder.Shape();
        } else {
            BRepPrimAPI_MakeCylinder builder(frame, radius, std::fabs(height));
            builder.Build();
            if (!builder.IsDone()) return false;
            shape = builder.Shape();
        }
        if (shape.IsNull()) return false;
        solid.m_Shape = shape;
        solid.ClearCenterlines();
        solid.SetCenterline({"base:rotation", SolidCenterlineKind::RotationAxis,
            {origin, origin.Translated(gp_Vec(extrusion_direction) * std::fabs(height))}, false});
        return !solid.m_Shape.IsNull() && solid.ReBuldMesh();
    } catch (...) {
        return false;
    }
}
