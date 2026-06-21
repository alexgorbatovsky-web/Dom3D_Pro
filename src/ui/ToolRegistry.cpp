#include "ToolRegistry.h"

#include "../CMesh3D.h"
#include "../CPolyline.h"
#include "../solid/Solid.h"
#include "../solid/SolidBoxTool.h"
#include "../solid/SolidCylinderTool.h"
#include "../solid/SolidPrismTool.h"
#include "../solid/SolidTool.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
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

std::vector<ParametricParameterValue> parameter_values(const std::vector<ToolParameter>& parameters) {
    std::vector<ParametricParameterValue> values;
    values.reserve(parameters.size());
    for (const ToolParameter& parameter : parameters) {
        values.push_back({parameter.id, parameter.value});
    }
    return values;
}

std::vector<ParametricParameterValue> update_saved_parameter_values(std::vector<ParametricParameterValue> saved,
                                                                    const std::vector<ToolParameter>& parameters) {
    for (const ToolParameter& parameter : parameters) {
        auto found = std::find_if(saved.begin(), saved.end(), [&parameter](const ParametricParameterValue& saved_parameter) {
            return saved_parameter.id == parameter.id;
        });
        if (found != saved.end()) {
            found->value = parameter.value;
        } else {
            saved.push_back({parameter.id, parameter.value});
        }
    }
    return saved;
}

void store_parametric_definition(CAlfaDoc& document,
                                 size_t object_index,
                                 const std::string& tool_id,
                                 const std::string& tool_label,
                                 size_t operation_index,
                                 const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index < objects.size() && objects[object_index]) {
        if (auto* solid = dynamic_cast<CSolid*>(objects[object_index].get())) {
            std::vector<int> created_surface_indices;
            if (operation_index == 0) {
                created_surface_indices.reserve(static_cast<size_t>(solid->GetNumSurfaces()));
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    created_surface_indices.push_back(i);
                }
            } else if (const ParametricFunction* existing = solid->GetOperation(static_cast<int>(operation_index))) {
                created_surface_indices = existing->CreatedSurfaceIndices;
            }
            solid->SetParametricOperation(operation_index,
                                          tool_id,
                                          tool_label,
                                          parameter_values(parameters),
                                          std::move(created_surface_indices));
            return;
        }
        objects[object_index]->SetParametricDefinition(tool_id, parameter_values(parameters));
    }
}

std::vector<ToolParameter> merge_saved_parameters(const std::vector<ToolParameter>& defaults,
                                                  const std::vector<ParametricParameterValue>& saved) {
    std::vector<ToolParameter> parameters = defaults;
    for (ToolParameter& parameter : parameters) {
        for (const ParametricParameterValue& saved_parameter : saved) {
            if (saved_parameter.id == parameter.id) {
                parameter.value = saved_parameter.value;
                break;
            }
        }
    }
    return parameters;
}

struct StoredOperation {
    std::string tool_id;
    std::string label;
    std::vector<ParametricParameterValue> saved_parameters;
    std::vector<int> created_surface_indices;
};

std::vector<TopoDS_Face> shape_faces(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        faces.push_back(TopoDS::Face(explorer.Current()));
    }
    return faces;
}

std::vector<TopoDS_Face> newly_created_faces(const std::vector<TopoDS_Face>& before,
                                             const std::vector<TopoDS_Face>& after) {
    std::vector<TopoDS_Face> created;
    for (const TopoDS_Face& candidate : after) {
        const bool existed = std::any_of(before.begin(), before.end(), [&candidate](const TopoDS_Face& previous) {
            return candidate.IsSame(previous);
        });
        if (!existed) {
            created.push_back(candidate);
        }
    }
    return created;
}

std::vector<int> face_indices_in_shape(const std::vector<TopoDS_Face>& tracked,
                                       const std::vector<TopoDS_Face>& final_faces) {
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(final_faces.size()); ++i) {
        const TopoDS_Face& candidate = final_faces[static_cast<size_t>(i)];
        if (std::any_of(tracked.begin(), tracked.end(), [&candidate](const TopoDS_Face& face) {
                return candidate.IsSame(face);
            })) {
            indices.push_back(i);
        }
    }
    return indices;
}

std::vector<TopoDS_Face> generated_faces_for_edges(BRepBuilderAPI_MakeShape& builder,
                                                   const std::vector<TopoDS_Edge>& edges) {
    std::vector<TopoDS_Face> faces;
    for (const TopoDS_Edge& edge : edges) {
        const TopTools_ListOfShape& generated = builder.Generated(edge);
        for (TopTools_ListIteratorOfListOfShape it(generated); it.More(); it.Next()) {
            if (it.Value().ShapeType() == TopAbs_FACE) {
                faces.push_back(TopoDS::Face(it.Value()));
            }
        }
    }
    return faces;
}

void update_tracked_faces(BRepBuilderAPI_MakeShape& builder,
                          std::vector<std::vector<TopoDS_Face>>& tracked_operations) {
    for (std::vector<TopoDS_Face>& tracked_faces : tracked_operations) {
        std::vector<TopoDS_Face> updated;
        for (const TopoDS_Face& tracked : tracked_faces) {
            const TopTools_ListOfShape& modified = builder.Modified(tracked);
            if (!modified.IsEmpty()) {
                for (TopTools_ListIteratorOfListOfShape it(modified); it.More(); it.Next()) {
                    if (it.Value().ShapeType() == TopAbs_FACE) {
                        updated.push_back(TopoDS::Face(it.Value()));
                    }
                }
            } else if (!builder.IsDeleted(tracked)) {
                updated.push_back(tracked);
            }
        }
        tracked_faces = std::move(updated);
    }
}

std::vector<ToolParameter> parameters_for_operation(const ToolRegistry& registry, const StoredOperation& operation) {
    const ToolDefinition* tool = registry.Find(operation.tool_id);
    if (!tool) {
        return {};
    }
    return merge_saved_parameters(tool->defaults, operation.saved_parameters);
}

bool apply_fillet_all_edges(CSolid& solid,
                            double radius,
                            std::vector<TopoDS_Face>* created_faces = nullptr,
                            std::vector<std::vector<TopoDS_Face>>* tracked_operations = nullptr) {
    if (radius <= 0.0001 || solid.m_Shape.IsNull()) {
        return false;
    }

    const std::vector<TopoDS_Edge> edges = solid.GetAllTopoEdges();
    if (edges.empty()) {
        return false;
    }

    TopoDS_Shape result_shape;
    try {
        BRepFilletAPI_MakeFillet fillet(solid.m_Shape);
        for (const TopoDS_Edge& edge : edges) {
            fillet.Add(radius, edge);
        }
        fillet.Build();
        if (!fillet.IsDone()) {
            return false;
        }
        result_shape = fillet.Shape();
        if (tracked_operations) {
            update_tracked_faces(fillet, *tracked_operations);
        }
        if (created_faces) {
            *created_faces = generated_faces_for_edges(fillet, edges);
        }
    } catch (const Standard_Failure&) {
        return false;
    }

    if (result_shape.IsNull()) {
        return false;
    }

    solid.m_Shape = result_shape;
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

std::vector<std::pair<int, int>> edge_refs_from_saved_parameters(const std::vector<ParametricParameterValue>& saved_parameters) {
    int edge_count = 0;
    for (const ParametricParameterValue& parameter : saved_parameters) {
        if (parameter.id == "edge.count") {
            edge_count = std::max(0, static_cast<int>(parameter.value));
            break;
        }
    }

    std::vector<std::pair<int, int>> edge_refs;
    edge_refs.reserve(static_cast<size_t>(edge_count));
    for (int i = 0; i < edge_count; ++i) {
        int surface_index = -1;
        int edge_index = -1;
        const std::string surface_id = "edge." + std::to_string(i) + ".surface";
        const std::string edge_id = "edge." + std::to_string(i) + ".edge";
        for (const ParametricParameterValue& parameter : saved_parameters) {
            if (parameter.id == surface_id) {
                surface_index = static_cast<int>(parameter.value);
            } else if (parameter.id == edge_id) {
                edge_index = static_cast<int>(parameter.value);
            }
        }
        if (surface_index >= 0 && edge_index >= 0) {
            edge_refs.push_back({surface_index, edge_index});
        }
    }
    return edge_refs;
}

bool apply_fillet_edges(CSolid& solid,
                        const std::vector<std::pair<int, int>>& edge_refs,
                        double radius,
                        std::vector<TopoDS_Face>* created_faces = nullptr,
                        std::vector<std::vector<TopoDS_Face>>* tracked_operations = nullptr) {
    if (radius <= 0.0001 || edge_refs.empty() || solid.m_Shape.IsNull()) {
        return false;
    }

    const std::vector<TopoDS_Edge> edges = solid.GetTopoEdgesByRefs(edge_refs);
    if (edges.empty()) {
        return false;
    }

    TopoDS_Shape result_shape;
    try {
        BRepFilletAPI_MakeFillet fillet(solid.m_Shape);
        for (const TopoDS_Edge& edge : edges) {
            fillet.Add(radius, edge);
        }
        fillet.Build();
        if (!fillet.IsDone()) {
            return false;
        }
        result_shape = fillet.Shape();
        if (tracked_operations) {
            update_tracked_faces(fillet, *tracked_operations);
        }
        if (created_faces) {
            *created_faces = generated_faces_for_edges(fillet, edges);
        }
    } catch (const Standard_Failure&) {
        return false;
    }

    if (result_shape.IsNull()) {
        return false;
    }

    solid.m_Shape = result_shape;
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

bool apply_chamfer_edges(CSolid& solid,
                         const std::vector<std::pair<int, int>>& edge_refs,
                         double distance,
                         std::vector<TopoDS_Face>* created_faces = nullptr,
                         std::vector<std::vector<TopoDS_Face>>* tracked_operations = nullptr) {
    if (distance <= 0.0001 || edge_refs.empty() || solid.m_Shape.IsNull()) {
        return false;
    }

    const std::vector<TopoDS_Edge> edges = solid.GetTopoEdgesByRefs(edge_refs);
    if (edges.empty()) {
        return false;
    }

    try {
        BRepFilletAPI_MakeChamfer chamfer(solid.m_Shape);
        for (const TopoDS_Edge& edge : edges) {
            chamfer.Add(distance, edge);
        }
        chamfer.Build();
        if (!chamfer.IsDone() || chamfer.Shape().IsNull()) {
            return false;
        }
        if (tracked_operations) {
            update_tracked_faces(chamfer, *tracked_operations);
        }
        if (created_faces) {
            *created_faces = generated_faces_for_edges(chamfer, edges);
        }
        solid.m_Shape = chamfer.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

bool apply_extrude_face(CSolid& solid,
                        const std::vector<ParametricParameterValue>& saved_parameters,
                        const std::vector<ToolParameter>& parameters) {
    const int face_index = static_cast<int>(std::max(0.0, [&saved_parameters]() {
        for (const ParametricParameterValue& parameter : saved_parameters) {
            if (parameter.id == "face.index") {
                return parameter.value;
            }
        }
        return -1.0;
    }()));
    const double distance = param(parameters, "distance", 0.0);
    const double taper_angle = param(parameters, "taper", 0.0);
    if (std::fabs(distance) <= 0.0001 || solid.m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Face face = solid.GetTopoFace(face_index);
    Vec3 center{};
    Vec3 normal{};
    if (face.IsNull() || !solid.GetFaceCenterAndNormal(face_index, center, normal)) {
        return false;
    }

    TopoDS_Shape prism_shape;
    try {
        const Vec3 vector = normal * static_cast<float>(distance);
        BRepPrimAPI_MakePrism prism(face, gp_Vec(vector.x, vector.y, vector.z), false, true);
        prism.Build();
        if (!prism.IsDone()) {
            return false;
        }
        prism_shape = prism.Shape();

        if (!prism_shape.IsNull() && std::fabs(taper_angle) > 0.0001) {
            BRepAdaptor_Surface base_surface(face, true);
            if (base_surface.GetType() != GeomAbs_Plane) {
                return false;
            }
            const double angle = taper_angle * 3.14159265358979323846 / 180.0;
            const double direction_sign = distance >= 0.0 ? 1.0 : -1.0;
            const gp_Dir draft_direction(normal.x * direction_sign,
                                         normal.y * direction_sign,
                                         normal.z * direction_sign);
            BRepOffsetAPI_DraftAngle draft(prism_shape);
            for (TopExp_Explorer explorer(prism_shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
                const TopoDS_Face current_face = TopoDS::Face(explorer.Current());
                BRepAdaptor_Surface surface(current_face, true);
                if (surface.GetType() != GeomAbs_Plane) {
                    continue;
                }
                const gp_Dir face_normal = surface.Plane().Axis().Direction();
                const double alignment = std::fabs(face_normal.X() * normal.x
                    + face_normal.Y() * normal.y + face_normal.Z() * normal.z);
                if (alignment > 0.98) {
                    continue;
                }
                draft.Add(current_face, draft_direction, angle, base_surface.Plane());
                if (!draft.AddDone()) {
                    draft.Remove(current_face);
                    return false;
                }
            }
            draft.Build();
            if (!draft.IsDone() || draft.Shape().IsNull()) {
                return false;
            }
            prism_shape = draft.Shape();
        }

        TopoDS_Shape result_shape;
        if (distance >= 0.0) {
            BRepAlgoAPI_Fuse operation(solid.m_Shape, prism_shape);
            operation.Build();
            if (!operation.IsDone()) {
                return false;
            }
            result_shape = operation.Shape();
        } else {
            BRepAlgoAPI_Cut operation(solid.m_Shape, prism_shape);
            operation.Build();
            if (!operation.IsDone()) {
                return false;
            }
            result_shape = operation.Shape();
        }
        if (result_shape.IsNull()) {
            return false;
        }
        solid.m_Shape = result_shape;
    } catch (const Standard_Failure&) {
        return false;
    }

    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

double saved_param(const std::vector<ParametricParameterValue>& parameters,
                   const char* id,
                   double fallback) {
    for (const ParametricParameterValue& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }
    return fallback;
}

bool apply_draft_face(CSolid& solid,
                      const std::vector<ParametricParameterValue>& saved_parameters,
                      const std::vector<ToolParameter>& parameters) {
    const int face_index = static_cast<int>(saved_param(saved_parameters, "face.index", -1.0));
    const int edge_index = static_cast<int>(saved_param(saved_parameters, "axis.edge", -1.0));
    const double angle_degrees = param(parameters, "angle", 0.0);
    if (face_index < 0 || edge_index < 0 || std::fabs(angle_degrees) <= 0.0001 || solid.m_Shape.IsNull()) {
        return false;
    }

    const TopoDS_Face face = solid.GetTopoFace(face_index);
    const CSurfaceFace* surface = solid.GetSurfaceFace(face_index);
    Vec3 face_center{};
    Vec3 face_normal{};
    Vec3 edge_start{};
    Vec3 edge_end{};
    if (face.IsNull() || !surface
        || !solid.GetFaceCenterAndNormal(face_index, face_center, face_normal)
        || !surface->GetEdgeEndpoints(edge_index, edge_start, edge_end)) {
        return false;
    }

    const Vec3 axis_dir = normalize(edge_end - edge_start);
    const Vec3 plane_normal = normalize(cross(axis_dir, face_normal));
    if (dot(axis_dir, axis_dir) <= 0.000001f || dot(plane_normal, plane_normal) <= 0.000001f) {
        return false;
    }
    const Vec3 axis_point = (edge_start + edge_end) * 0.5f;

    try {
        BRepOffsetAPI_DraftAngle draft(solid.m_Shape);
        draft.Add(face,
                  gp_Dir(plane_normal.x, plane_normal.y, plane_normal.z),
                  angle_degrees * 3.14159265358979323846 / 180.0,
                  gp_Pln(gp_Pnt(axis_point.x, axis_point.y, axis_point.z),
                         gp_Dir(plane_normal.x, plane_normal.y, plane_normal.z)));
        if (!draft.AddDone()) {
            return false;
        }
        draft.Build();
        if (!draft.IsDone() || draft.Shape().IsNull()) {
            return false;
        }
        solid.m_Shape = draft.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

std::vector<int> face_indices_from_saved_parameters(
    const std::vector<ParametricParameterValue>& saved_parameters) {
    const int face_count = std::max(0, static_cast<int>(
        saved_param(saved_parameters, "face.count", 0.0)));
    std::vector<int> indices;
    indices.reserve(static_cast<size_t>(face_count));
    for (int i = 0; i < face_count; ++i) {
        const std::string id = "face." + std::to_string(i) + ".index";
        const int index = static_cast<int>(saved_param(saved_parameters, id.c_str(), -1.0));
        if (index >= 0) {
            indices.push_back(index);
        }
    }
    return indices;
}

bool apply_thick_solid(CSolid& solid,
                       const std::vector<ParametricParameterValue>& saved_parameters,
                       const std::vector<ToolParameter>& parameters) {
    const double thickness = param(parameters, "thick", 0.0);
    const std::vector<int> face_indices = face_indices_from_saved_parameters(saved_parameters);
    if (solid.m_Shape.IsNull() || face_indices.empty() || std::fabs(thickness) <= 0.0001) {
        return false;
    }

    TopTools_ListOfShape closing_faces;
    for (int face_index : face_indices) {
        const TopoDS_Face face = solid.GetTopoFace(face_index);
        if (!face.IsNull()) {
            closing_faces.Append(face);
        }
    }
    if (closing_faces.IsEmpty()) {
        return false;
    }

    try {
        BRepOffsetAPI_MakeThickSolid thick_solid;
        thick_solid.MakeThickSolidByJoin(solid.m_Shape,
                                         closing_faces,
                                         thickness,
                                         0.001,
                                         BRepOffset_Skin,
                                         Standard_False,
                                         Standard_False,
                                         GeomAbs_Arc,
                                         Standard_False);
        thick_solid.Build();
        if (!thick_solid.IsDone() || thick_solid.Shape().IsNull()) {
            return false;
        }
        solid.m_Shape = thick_solid.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

bool apply_solid_shape_transform(CSolid& solid, const gp_Trsf& transform) {
    if (solid.m_Shape.IsNull()) {
        return false;
    }

    try {
        BRepBuilderAPI_Transform builder(solid.m_Shape, transform, true);
        if (!builder.IsDone()) {
            return false;
        }
        solid.m_Shape = builder.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    if (solid.m_Shape.IsNull()) {
        return false;
    }
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

bool apply_solid_shape_transform(CSolid& solid, const gp_GTrsf& transform) {
    if (solid.m_Shape.IsNull()) {
        return false;
    }

    try {
        BRepBuilderAPI_GTransform builder(solid.m_Shape, transform, true);
        if (!builder.IsDone()) {
            return false;
        }
        solid.m_Shape = builder.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    if (solid.m_Shape.IsNull()) {
        return false;
    }
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

bool apply_solid_transform(CSolid& solid, const std::vector<ToolParameter>& parameters) {
    const int type = std::clamp(static_cast<int>(param(parameters, "type", 0.0)), 0, 2);
    if (type == 0) {
        gp_Trsf transform;
        transform.SetTranslation(gp_Vec(param(parameters, "dx", 0.0),
                                        param(parameters, "dy", 0.0),
                                        param(parameters, "dz", 0.0)));
        return apply_solid_shape_transform(solid, transform);
    }

    const Vec3 center{
        static_cast<float>(param(parameters, "center.x", 0.0)),
        static_cast<float>(param(parameters, "center.y", 0.0)),
        static_cast<float>(param(parameters, "center.z", 0.0))
    };
    const Vec3 axis{
        static_cast<float>(param(parameters, "axis.x", 0.0)),
        static_cast<float>(param(parameters, "axis.y", 0.0)),
        static_cast<float>(param(parameters, "axis.z", 1.0))
    };
    const Vec3 unit_axis = normalize(axis);

    if (type == 1) {
        const double angle = param(parameters, "angle", 0.0);
        if (std::fabs(angle) <= 0.000001 || dot(unit_axis, unit_axis) <= 0.000001f) {
            return true;
        }
        gp_Trsf transform;
        transform.SetRotation(gp_Ax1(gp_Pnt(center.x, center.y, center.z),
                                     gp_Dir(unit_axis.x, unit_axis.y, unit_axis.z)),
                              angle);
        return apply_solid_shape_transform(solid, transform);
    }

    const double factor = param(parameters, "factor", 1.0);
    if (factor <= 0.000001 || std::fabs(factor - 1.0) <= 0.000001) {
        return true;
    }
    if (dot(unit_axis, unit_axis) <= 0.000001f) {
        gp_Trsf transform;
        transform.SetScale(gp_Pnt(center.x, center.y, center.z), factor);
        return apply_solid_shape_transform(solid, transform);
    }

    const double k = factor - 1.0;
    const double m00 = 1.0 + k * unit_axis.x * unit_axis.x;
    const double m01 = k * unit_axis.x * unit_axis.y;
    const double m02 = k * unit_axis.x * unit_axis.z;
    const double m10 = k * unit_axis.y * unit_axis.x;
    const double m11 = 1.0 + k * unit_axis.y * unit_axis.y;
    const double m12 = k * unit_axis.y * unit_axis.z;
    const double m20 = k * unit_axis.z * unit_axis.x;
    const double m21 = k * unit_axis.z * unit_axis.y;
    const double m22 = 1.0 + k * unit_axis.z * unit_axis.z;

    gp_GTrsf transform;
    transform.SetValue(1, 1, m00);
    transform.SetValue(1, 2, m01);
    transform.SetValue(1, 3, m02);
    transform.SetValue(1, 4, center.x - (m00 * center.x + m01 * center.y + m02 * center.z));
    transform.SetValue(2, 1, m10);
    transform.SetValue(2, 2, m11);
    transform.SetValue(2, 3, m12);
    transform.SetValue(2, 4, center.y - (m10 * center.x + m11 * center.y + m12 * center.z));
    transform.SetValue(3, 1, m20);
    transform.SetValue(3, 2, m21);
    transform.SetValue(3, 3, m22);
    transform.SetValue(3, 4, center.z - (m20 * center.x + m21 * center.y + m22 * center.z));
    return apply_solid_shape_transform(solid, transform);
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

BooleanKind boolean_kind_from_parameter(double value) {
    const int operation = std::clamp(static_cast<int>(value), 0, 2);
    if (operation == 0) {
        return BooleanKind::Union;
    }
    if (operation == 1) {
        return BooleanKind::Cut;
    }
    return BooleanKind::Common;
}

bool apply_boolean_tool(CSolid& solid, const std::vector<ToolParameter>& parameters) {
    const int tool_index = std::max(0, static_cast<int>(param(parameters, "tool", 0.0)));
    const CSolid* tool = solid.GetBooleanTool(static_cast<size_t>(tool_index));
    if (!tool || tool->m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Shape result_shape;
    if (!build_boolean_shape(boolean_kind_from_parameter(param(parameters, "operation", 0.0)),
                             solid.m_Shape,
                             tool->m_Shape,
                             result_shape)) {
        return false;
    }

    solid.m_Shape = result_shape;
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    solid.InitSurfaces();
    solid.InitEdges();
    return solid.BuldMesh(0.1f);
}

Vec3 point_to_vec3(const CPoint3d& point) {
    return {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
}

bool build_profile_face_from_polyline(const CPolyline& polyline, TopoDS_Face& face, Vec3& normal) {
    if (!polyline.IsClosed() || polyline.GetPoints().size() < 3) {
        return false;
    }

    const std::vector<CPoint3d>& points = polyline.GetPoints();
    Vec3 newell{};
    for (size_t i = 0; i < points.size(); ++i) {
        const CPoint3d& current = points[i];
        const CPoint3d& next = points[(i + 1) % points.size()];
        newell.x += (current.y - next.y) * (current.z + next.z);
        newell.y += (current.z - next.z) * (current.x + next.x);
        newell.z += (current.x - next.x) * (current.y + next.y);
    }

    const float normal_length = std::sqrt(dot(newell, newell));
    if (normal_length <= 0.0001f) {
        return false;
    }
    normal = newell * (1.0f / normal_length);

    const Vec3 origin = point_to_vec3(points.front());
    for (const CPoint3d& point : points) {
        if (std::fabs(dot(point_to_vec3(point) - origin, normal)) > 0.001f) {
            return false;
        }
    }

    BRepBuilderAPI_MakePolygon polygon;
    for (const CPoint3d& point : points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return false;
    }

    const TopoDS_Wire wire = polygon.Wire();
    if (wire.IsNull()) {
        return false;
    }

    BRepBuilderAPI_MakeFace face_builder(wire, true);
    if (!face_builder.IsDone()) {
        return false;
    }

    face = face_builder.Face();
    return !face.IsNull();
}

bool rebuild_extrude_base(CAlfaDoc& document, size_t object_index, const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return false;
    }

    const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long profile_id = static_cast<unsigned long>(std::max(0.0, param(parameters, "profile.id", 0.0)));
    const auto* profile = dynamic_cast<const CPolyline*>(document.FindObjectById(profile_id));
    if (!old_solid || !profile) {
        return false;
    }

    TopoDS_Face profile_face;
    Vec3 normal{};
    if (!build_profile_face_from_polyline(*profile, profile_face, normal)) {
        return false;
    }

    const double distance = param(parameters, "distance", 1.0);
    if (std::fabs(distance) <= 0.0001) {
        return false;
    }
    const bool reverse = param(parameters, "reverse", 0.0) >= 0.5;
    const double signed_distance = reverse ? -distance : distance;
    const double taper_angle_degrees = param(parameters, "taper", 0.0);

    TopoDS_Shape shape;
    try {
        const Vec3 vector = normal * static_cast<float>(signed_distance);
        BRepPrimAPI_MakePrism prism_builder(profile_face, gp_Vec(vector.x, vector.y, vector.z), false, true);
        prism_builder.Build();
        if (!prism_builder.IsDone()) {
            return false;
        }
        shape = prism_builder.Shape();
        if (!shape.IsNull() && std::fabs(taper_angle_degrees) > 0.0001) {
            BRepAdaptor_Surface base_surface(profile_face, true);
            if (base_surface.GetType() != GeomAbs_Plane) {
                return false;
            }

            const double angle = taper_angle_degrees * 3.14159265358979323846 / 180.0;
            const double direction_sign = signed_distance >= 0.0 ? 1.0 : -1.0;
            gp_Dir draft_direction(normal.x * direction_sign, normal.y * direction_sign, normal.z * direction_sign);
            const gp_Pln neutral_plane = base_surface.Plane();

            BRepOffsetAPI_DraftAngle draft(shape);
            for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
                const TopoDS_Face current_face = TopoDS::Face(explorer.Current());
                BRepAdaptor_Surface surface(current_face, true);
                if (surface.GetType() != GeomAbs_Plane) {
                    continue;
                }

                const gp_Dir face_normal = surface.Plane().Axis().Direction();
                const double alignment = std::fabs(face_normal.X() * normal.x + face_normal.Y() * normal.y + face_normal.Z() * normal.z);
                if (alignment > 0.98) {
                    continue;
                }

                draft.Add(current_face, draft_direction, angle, neutral_plane);
                if (!draft.AddDone()) {
                    draft.Remove(current_face);
                }
            }

            draft.Build();
            if (draft.IsDone()) {
                TopoDS_Shape drafted_shape = draft.Shape();
                if (!drafted_shape.IsNull()) {
                    shape = drafted_shape;
                }
            }
        }
    } catch (const Standard_Failure&) {
        return false;
    }

    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->m_id = old_solid->m_id;
    solid->SetName(old_solid->GetName());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->CopyOperationTreeFrom(*old_solid);
    solid->InitSurfaces();
    solid->InitEdges();
    if (!solid->BuldMesh(0.1f)) {
        return false;
    }

    objects[object_index] = std::move(solid);
    return true;
}

Vec3 revolve_axis_direction(int axis_index) {
    if (axis_index == 0) {
        return {1.0f, 0.0f, 0.0f};
    }
    if (axis_index == 1) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {0.0f, 0.0f, 1.0f};
}

bool build_revolve_profile(const CPolyline& polyline,
                           int axis_index,
                           TopoDS_Shape& profile_shape,
                           Vec3& axis_origin) {
    const std::vector<CPoint3d>& points = polyline.GetPoints();
    if (points.size() < 2) {
        return false;
    }
    const double plane_z = points.front().z;
    for (const CPoint3d& point : points) {
        if (std::fabs(point.z - plane_z) > 0.001) {
            return false;
        }
    }
    axis_origin = {0.0f, 0.0f, static_cast<float>(plane_z)};

    std::vector<Vec3> face_points;
    face_points.reserve(points.size() + 2);
    for (const CPoint3d& point : points) {
        const Vec3 value = point_to_vec3(point);
        if (face_points.empty() || dot(value - face_points.back(), value - face_points.back()) > 0.00000001f) {
            face_points.push_back(value);
        }
    }

    if (!polyline.IsClosed()) {
        const Vec3 axis = revolve_axis_direction(axis_index);
        const auto project = [axis_origin, axis](Vec3 point) {
            return axis_origin + axis * dot(point - axis_origin, axis);
        };
        const Vec3 first = face_points.front();
        const Vec3 last = face_points.back();
        const Vec3 last_on_axis = project(last);
        const Vec3 first_on_axis = project(first);
        if (dot(face_points.back() - last_on_axis, face_points.back() - last_on_axis) > 0.00000001f) {
            face_points.push_back(last_on_axis);
        }
        if (dot(face_points.back() - first_on_axis, face_points.back() - first_on_axis) > 0.00000001f
            && dot(first - first_on_axis, first - first_on_axis) > 0.00000001f) {
            face_points.push_back(first_on_axis);
        }
    }

    if (face_points.size() < 3) {
        return false;
    }
    BRepBuilderAPI_MakePolygon polygon;
    for (Vec3 point : face_points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return false;
    }
    BRepBuilderAPI_MakeFace face_builder(polygon.Wire(), true);
    if (!face_builder.IsDone() || face_builder.Face().IsNull()) {
        return false;
    }
    profile_shape = face_builder.Face();
    return true;
}

bool rebuild_revolve_base(CAlfaDoc& document,
                          size_t object_index,
                          const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return false;
    }

    const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long profile_id =
        static_cast<unsigned long>(std::max(0.0, param(parameters, "profile.id", 0.0)));
    const auto* profile = dynamic_cast<const CPolyline*>(document.FindObjectById(profile_id));
    const double angle_degrees = param(parameters, "angle", 360.0);
    const int axis_index = std::clamp(static_cast<int>(param(parameters, "axis", 2.0)), 0, 2);
    if (!old_solid || !profile || angle_degrees <= 0.0001) {
        return false;
    }

    TopoDS_Shape profile_shape;
    Vec3 axis_origin{};
    if (!build_revolve_profile(*profile, axis_index, profile_shape, axis_origin)) {
        return false;
    }

    TopoDS_Shape shape;
    try {
        const Vec3 axis = revolve_axis_direction(axis_index);
        BRepPrimAPI_MakeRevol revol(
            profile_shape,
            gp_Ax1(gp_Pnt(axis_origin.x, axis_origin.y, axis_origin.z),
                   gp_Dir(axis.x, axis.y, axis.z)),
            std::clamp(angle_degrees, 0.0, 360.0) * 3.14159265358979323846 / 180.0,
            Standard_False);
        revol.Build();
        if (!revol.IsDone()) {
            return false;
        }
        shape = revol.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }
    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->m_id = old_solid->m_id;
    solid->SetName(old_solid->GetName());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->CopyOperationTreeFrom(*old_solid);
    solid->InitSurfaces();
    solid->InitEdges();
    if (!solid->BuldMesh(0.1f)) {
        return false;
    }
    objects[object_index] = std::move(solid);
    return true;
}

bool rebuild_solid_operation_tree(const ToolRegistry& registry,
                                  const ActiveParametricObject& active_object,
                                  CAlfaDoc& document) {
    auto& objects = document.GetObjects();
    if (active_object.object_index >= objects.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects[active_object.object_index].get());
    if (!solid || solid->GetNumOperations() <= 0) {
        return false;
    }

    std::vector<StoredOperation> operations;
    operations.reserve(static_cast<size_t>(solid->GetNumOperations()));
    for (const ParametricFunction* operation : solid->GetOperationTree()) {
        if (!operation || operation->ToolId.empty()) {
            continue;
        }
        operations.push_back({
            operation->ToolId,
            operation->Name,
            operation->Parameters,
            operation->CreatedSurfaceIndices
        });
    }
    if (operations.empty()) {
        return false;
    }

    if (!active_object.tool_id.empty()) {
        if (active_object.operation_index >= operations.size()) {
            return false;
        }
        operations[active_object.operation_index].tool_id = active_object.tool_id;
        operations[active_object.operation_index].label = registry.LabelFor(active_object.tool_id);
        operations[active_object.operation_index].saved_parameters =
            update_saved_parameter_values(operations[active_object.operation_index].saved_parameters,
                                          active_object.parameters);
    }

    const StoredOperation& base_operation = operations.front();
    const ToolDefinition* base_tool = registry.Find(base_operation.tool_id);
    if (!base_tool || !base_tool->rebuild) {
        return false;
    }

    if (base_operation.tool_id == "SolidExtrudeTool") {
        if (!rebuild_extrude_base(document,
                                  active_object.object_index,
                                  parameters_for_operation(registry, base_operation))) {
            return false;
        }
    } else if (base_operation.tool_id == "SurfaceOfRevolution") {
        if (!rebuild_revolve_base(document,
                                  active_object.object_index,
                                  parameters_for_operation(registry, base_operation))) {
            return false;
        }
    } else {
        base_tool->rebuild(document,
                           active_object.object_index,
                           parameters_for_operation(registry, base_operation));
    }

    solid = dynamic_cast<CSolid*>(objects[active_object.object_index].get());
    if (!solid) {
        return false;
    }
    std::vector<std::vector<TopoDS_Face>> operation_created_faces(operations.size());
    operation_created_faces[0] = shape_faces(solid->m_Shape);

    for (size_t i = 1; i < operations.size(); ++i) {
        const StoredOperation& operation = operations[i];
        const std::vector<TopoDS_Face> faces_before = shape_faces(solid->m_Shape);
        bool history_tracked = false;
        if (operation.tool_id == "fillet_all_edges") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_fillet_all_edges(*solid,
                                        param(parameters, "radius", 0.20),
                                        &operation_created_faces[i],
                                        &operation_created_faces)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "fillet_edge") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_fillet_edges(*solid,
                                    edge_refs_from_saved_parameters(operation.saved_parameters),
                                    param(parameters, "radius", 0.20),
                                    &operation_created_faces[i],
                                    &operation_created_faces)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "ChamferSolid") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_chamfer_edges(*solid,
                                     edge_refs_from_saved_parameters(operation.saved_parameters),
                                     param(parameters, "distance", 0.20),
                                     &operation_created_faces[i],
                                     &operation_created_faces)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "SolidExtrudeFace") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_extrude_face(*solid, operation.saved_parameters, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "SolidDraft") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_draft_face(*solid, operation.saved_parameters, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "ThickSolidTool") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_thick_solid(*solid, operation.saved_parameters, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "boolean") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_boolean_tool(*solid, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "SolidTransform") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_solid_transform(*solid, parameters)) {
                return false;
            }
        }
        const std::vector<TopoDS_Face> faces_after = shape_faces(solid->m_Shape);
        if (operation.tool_id == "SolidTransform" && faces_before.size() == faces_after.size()) {
            for (size_t previous_operation = 0; previous_operation < i; ++previous_operation) {
                for (TopoDS_Face& tracked_face : operation_created_faces[previous_operation]) {
                    for (size_t face_index = 0; face_index < faces_before.size(); ++face_index) {
                        if (tracked_face.IsSame(faces_before[face_index])) {
                            tracked_face = faces_after[face_index];
                            break;
                        }
                    }
                }
            }
        } else if (!history_tracked) {
            operation_created_faces[i] = newly_created_faces(faces_before, faces_after);
        }
    }

    const std::vector<TopoDS_Face> final_faces = shape_faces(solid->m_Shape);
    solid->ClearOperationTree();
    for (size_t i = 0; i < operations.size(); ++i) {
        const StoredOperation& operation = operations[i];
        solid->SetParametricOperation(solid->GetOperationTree().size(),
                                      operation.tool_id,
                                      operation.label,
                                      operation.saved_parameters,
                                      face_indices_in_shape(operation_created_faces[i], final_faces));
    }
    return true;
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
    static const SolidCylinderTool solid_cylinder_tool;
    static const SolidPrismTool solid_prism_tool;
    static const SolidTransformTool solid_transform_tool;
    tools_.push_back(solid_box_tool.CreateToolDefinition());
    tools_.push_back(solid_cylinder_tool.CreateToolDefinition());
    tools_.push_back(solid_prism_tool.CreateToolDefinition());
    tools_.push_back({
        solid_transform_tool.GetID(),
        solid_transform_tool.GetLabel(),
        solid_transform_tool.GetDefaultParameters(),
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "PolylineCurve",
        "Polyline",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "BSplineCurve",
        "B-Spline",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "EditPoint",
        "Edit Point",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    // ������ boolean-���������� � UI ������� ������ ������ �������� � �������� ������������� ������
    tools_.push_back({
        "boolean",
        "Boolean",
        {
            {"operation", "Operation", 0.0, 0.0, 2.0, 1.0, ToolParameterType::Combo, {"Union", "Cut", "Common"}},
            {"tool", "Tool", 0.0, 0.0, 1000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
            // �������� ��������� ���������� � UI (MainWindow) � ����� ������ ����������
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    // ��������������� ����������� (������)
    tools_.push_back({
        "fillet_edge",
        "Fillet Edge",
        {
            {"radius", "Radius", 0.20, 0.01, 10.0, 0.01}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "fillet_all_edges",
        "Fillet All Edges",
        {
            {"radius", "Radius", 0.20, 0.01, 10.0, 0.01}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "ChamferSolid",
        "Chamfer",
        {
            {"distance", "Distance", 0.20, 0.01, 10.0, 0.01}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SolidExtrudeFace",
        "Extrude Face",
        {
            {"distance", "Distance", 1.0, -1000.0, 1000.0, 0.1},
            {"taper", "Taper Angle", 0.0, -89.0, 89.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SolidExtrudeTool",
        "Extrude",
        {
            {"distance", "Distance", 1.0, 0.0, 1000.0, 0.1},
            {"reverse", "Reverse", 0.0, 0.0, 1.0, 1.0, ToolParameterType::Checkbox},
            {"taper", "Angle taper", 0.0, -89.0, 89.0, 1.0},
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SurfaceOfRevolution",
        "Revolve",
        {
            {"angle", "Angle", 360.0, 0.0, 360.0, 1.0},
            {"axis", "Axis", 2.0, 0.0, 2.0, 1.0, ToolParameterType::Combo, {"Axis X", "Axis Y", "Axis Z"}},
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SurfaceLoft",
        "Loft Surface",
        {},
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.CreateLoftSurfaceFromSelectedBSplines();
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SurfaceReverseNormals",
        "Reverse Normals",
        {},
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.ReverseSelectedSurfaceNormals();
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SolidDraft",
        "Draft Face",
        {
            {"angle", "Angle", 1.0, -89.0, 89.0, 0.1}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "ThickSolidTool",
        "Thick Solid",
        {
            {"thick", "Thick", 1.0, -100.0, 100.0, 0.1}
        },
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
    const size_t object_index = document.GetSelectedObjectIndex();
    store_parametric_definition(document, object_index, tool->id, tool->label, 0, tool->defaults);
    return {tool->id, object_index, 0, tool->defaults};
}

void ToolRegistry::Rebuild(const ActiveParametricObject& active_object, CAlfaDoc& document) const {
    if (rebuild_solid_operation_tree(*this, active_object, document)) {
        return;
    }

    const ToolDefinition* tool = Find(active_object.tool_id);
    if (tool && tool->rebuild) {
        tool->rebuild(document, active_object.object_index, active_object.parameters);
        store_parametric_definition(document,
                                    active_object.object_index,
                                    active_object.tool_id,
                                    tool->label,
                                    active_object.operation_index,
                                    active_object.parameters);
    }
}

bool ToolRegistry::ReplayOperations(size_t object_index, CAlfaDoc& document) const {
    ActiveParametricObject active_object;
    active_object.object_index = object_index;
    return rebuild_solid_operation_tree(*this, active_object, document);
}

bool ToolRegistry::ReplayProfileDependents(unsigned long profile_id, CAlfaDoc& document) const {
    if (profile_id == 0) {
        return false;
    }

    bool rebuilt_any = false;
    auto& objects = document.GetObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto* solid = dynamic_cast<const CSolid*>(objects[i].get());
        const ParametricFunction* base_operation = solid ? solid->GetOperation(0) : nullptr;
        if (!base_operation
            || (base_operation->ToolId != "SolidExtrudeTool"
                && base_operation->ToolId != "SurfaceOfRevolution")) {
            continue;
        }

        bool uses_profile = false;
        for (const ParametricParameterValue& parameter : base_operation->Parameters) {
            if (parameter.id == "profile.id" && static_cast<unsigned long>(std::max(0.0, parameter.value)) == profile_id) {
                uses_profile = true;
                break;
            }
        }
        if (uses_profile && ReplayOperations(i, document)) {
            rebuilt_any = true;
        }
    }
    return rebuilt_any;
}

bool ToolRegistry::ReplayAllProfileDependents(CAlfaDoc& document) const {
    bool rebuilt_any = false;
    auto& objects = document.GetObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto* solid = dynamic_cast<const CSolid*>(objects[i].get());
        const ParametricFunction* base_operation = solid ? solid->GetOperation(0) : nullptr;
        if (base_operation
            && (base_operation->ToolId == "SolidExtrudeTool"
                || base_operation->ToolId == "SurfaceOfRevolution")
            && ReplayOperations(i, document)) {
            rebuilt_any = true;
        }
    }
    return rebuilt_any;
}

ActiveParametricObject ToolRegistry::ActiveObjectFromDocument(size_t object_index, const CAlfaObject& object, size_t operation_index) const {
    std::string tool_id = object.GetParametricToolId();
    std::vector<ParametricParameterValue> saved_parameters = object.GetParametricParameters();
    if (const auto* solid = dynamic_cast<const CSolid*>(&object)) {
        if (const ParametricFunction* operation = solid->GetOperation(static_cast<int>(operation_index))) {
            tool_id = operation->ToolId;
            saved_parameters = operation->Parameters;
        }
    }
    const ToolDefinition* tool = Find(tool_id);
    if (!tool || tool->defaults.empty()) {
        return {};
    }

    return {tool_id, object_index, operation_index, merge_saved_parameters(tool->defaults, saved_parameters)};
}

std::string ToolRegistry::LabelFor(const std::string& id) const {
    const ToolDefinition* tool = Find(id);
    return tool ? tool->label : id;
}
