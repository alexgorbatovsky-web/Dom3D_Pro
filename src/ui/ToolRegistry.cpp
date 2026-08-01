#include "ToolRegistry.h"
#include "../ExtrudeShapeBuilder.h"

#include "../CMesh3D.h"
#include "../CAssembled.h"
#include "../solid/AssociativeClone.h"
#include "../CPolyline.h"
#include "../SmartLine.h"
#include "../SketchProfileBuilder.h"
#include "../SweptSolidBuilder.h"
#include "../solid/Solid.h"
#include "../solid/SolidBeamTool.h"
#include "../solid/SolidBoxTool.h"
#include "../solid/SolidCylinderTool.h"
#include "../solid/SolidPrismTool.h"
#include "../solid/SolidSphereTool.h"
#include "../solid/SolidTorusTool.h"
#include "../solid/SolidTool.h"
#include "../solid/PlaneShapeBuilder.h"
#include "../solid/PolyhedronShapeBuilder.h"
#include "../solid/SketchFeatureShapeBuilder.h"
#include "../solid/OffsetFaceShapeBuilder.h"
#include "../solid/SurfaceSet.h"
#include "../solid/TrimShapeBuilder.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GC_MakeArcOfCircle.hxx>
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

bool plane_definition(const std::vector<ToolParameter>& parameters,
                      Vec3& origin,
                      Vec3& normal,
                      double& size) {
    const int mode = std::clamp(
        static_cast<int>(param(parameters, "mode", 1.0)), 0, 5);
    size = param(parameters, "size", 200.0);
    if (mode == 0) {
        const double a = param(parameters, "a", 0.0);
        const double b = param(parameters, "b", 0.0);
        const double c = param(parameters, "c", 1.0);
        const double d = param(parameters, "d", 0.0);
        const double squared_length = a * a + b * b + c * c;
        if (squared_length <= 1.0e-18) {
            return false;
        }
        origin = {
            static_cast<float>(-a * d / squared_length),
            static_cast<float>(-b * d / squared_length),
            static_cast<float>(-c * d / squared_length)};
        normal = {
            static_cast<float>(a),
            static_cast<float>(b),
            static_cast<float>(c)};
    } else if (mode == 1) {
        origin = {
            static_cast<float>(param(parameters, "plane.origin.x", 0.0)),
            static_cast<float>(param(parameters, "plane.origin.y", 0.0)),
            static_cast<float>(param(parameters, "plane.origin.z", 0.0))};
        normal = {
            static_cast<float>(param(parameters, "plane.normal.x", 0.0)),
            static_cast<float>(param(parameters, "plane.normal.y", 0.0)),
            static_cast<float>(param(parameters, "plane.normal.z", 1.0))};
    } else if (mode == 2) {
        const Vec3 first{
            static_cast<float>(param(parameters, "p1.x", 0.0)),
            static_cast<float>(param(parameters, "p1.y", 0.0)),
            static_cast<float>(param(parameters, "p1.z", 0.0))};
        const Vec3 second{
            static_cast<float>(param(parameters, "p2.x", 100.0)),
            static_cast<float>(param(parameters, "p2.y", 0.0)),
            static_cast<float>(param(parameters, "p2.z", 0.0))};
        const Vec3 third{
            static_cast<float>(param(parameters, "p3.x", 0.0)),
            static_cast<float>(param(parameters, "p3.y", 100.0)),
            static_cast<float>(param(parameters, "p3.z", 0.0))};
        const Vec3 first_edge{
            second.x - first.x,
            second.y - first.y,
            second.z - first.z};
        const Vec3 second_edge{
            third.x - first.x,
            third.y - first.y,
            third.z - first.z};
        origin = first;
        normal = cross(first_edge, second_edge);
    } else {
        const float offset =
            static_cast<float>(param(parameters, "offset", 0.0));
        if (mode == 3) {
            origin = {0.0f, 0.0f, offset};
            normal = {0.0f, 0.0f, 1.0f};
        } else if (mode == 4) {
            origin = {0.0f, offset, 0.0f};
            normal = {0.0f, 1.0f, 0.0f};
        } else {
            origin = {offset, 0.0f, 0.0f};
            normal = {1.0f, 0.0f, 0.0f};
        }
    }
    return size > 1.0e-6 && dot(normal, normal) > 1.0e-12f;
}

bool build_plane_shape(const std::vector<ToolParameter>& parameters,
                       TopoDS_Shape& shape) {
    Vec3 origin{};
    Vec3 normal{};
    double size = 0.0;
    TopoDS_Face face;
    if (!plane_definition(parameters, origin, normal, size)
        || !BuildFinitePlaneFace(origin, normal, size, face)) {
        return false;
    }
    shape = face;
    return true;
}

void create_plane(CAlfaDoc& document,
                  const std::vector<ToolParameter>& parameters) {
    TopoDS_Shape shape;
    if (!build_plane_shape(parameters, shape)) {
        return;
    }
    auto plane = std::make_unique<CSurfaceSet>(shape);
    plane->SetName("Plane");
    plane->SetColor({0.72f, 0.42f, 0.86f});
    if (!plane->ReBuldMesh()) {
        return;
    }
    document.AddObject(std::move(plane));
}

void rebuild_plane(CAlfaDoc& document,
                   size_t object_index,
                   const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return;
    }
    const auto* old_plane =
        dynamic_cast<const CSurfaceSet*>(objects[object_index].get());
    TopoDS_Shape shape;
    if (!old_plane || !build_plane_shape(parameters, shape)) {
        return;
    }

    auto plane = std::make_unique<CSurfaceSet>(shape);
    plane->m_id = old_plane->m_id;
    plane->SetName(old_plane->GetName());
    plane->SetColor(old_plane->GetColor());
    plane->SetMaterial(old_plane->GetMaterial());
    plane->SetMaterialId(old_plane->GetMaterialId());
    plane->SetGroupName(old_plane->GetGroupName());
    plane->SetVisible(old_plane->IsVisible());
    plane->m_LayerID = old_plane->m_LayerID;
    plane->CopyOperationTreeFrom(*old_plane);
    if (!plane->ReBuldMesh()) {
        return;
    }
    objects[object_index] = std::move(plane);
}

TopoDS_Face first_face(const TopoDS_Shape& shape) {
    TopExp_Explorer explorer(shape, TopAbs_FACE);
    if (!explorer.More()) {
        return {};
    }
    return TopoDS::Face(explorer.Current());
}

bool build_open_sketch_wire(const CSmartLine& sketch,
                            TopoDS_Wire& result) {
    result.Nullify();
    const std::vector<CPoint3d> points = sketch.GetProfilePointsWorld();
    if (points.size() < 2) {
        return false;
    }

    try {
        BRepBuilderAPI_MakeWire wire;
        for (size_t index = 1; index < points.size(); ++index) {
            const CPoint3d& first = points[index - 1];
            const CPoint3d& second = points[index];
            BRepBuilderAPI_MakeEdge edge(
                gp_Pnt(first.x, first.y, first.z),
                gp_Pnt(second.x, second.y, second.z));
            if (!edge.IsDone()) {
                return false;
            }
            wire.Add(edge.Edge());
        }
        if (!wire.IsDone() || wire.Wire().IsNull()) {
            return false;
        }
        result = wire.Wire();
        return true;
    } catch (...) {
        return false;
    }
}

bool apply_trim_operation(CSolid& solid,
                          CAlfaDoc& document,
                          const std::string& tool_id,
                          const std::vector<ToolParameter>& parameters) {
    const unsigned long cutter_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "cutter.id", 0.0)));
    const bool positive = param(parameters, "direction", 0.0) < 0.5;
    const CAlfaObject* cutter = document.FindObjectById(cutter_id);
    if (!cutter || solid.m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Shape result;
    if (tool_id == "TrimByPlane") {
        const auto* plane = dynamic_cast<const CSurfaceSet*>(cutter);
        const TopoDS_Face face =
            plane ? first_face(plane->m_Shape) : TopoDS_Face();
        if (!plane
            || plane->GetParametricToolId() != "PlaneTool"
            || !TrimSolidByFace(solid.m_Shape, face, positive, result)) {
            return false;
        }
    } else if (tool_id == "TrimBySurface") {
        const auto* surface = dynamic_cast<const CSurfaceSet*>(cutter);
        const TopoDS_Face face =
            surface ? first_face(surface->m_Shape) : TopoDS_Face();
        if (!surface
            || surface->GetParametricToolId() == "PlaneTool"
            || !TrimSolidByFace(solid.m_Shape, face, positive, result)) {
            return false;
        }
    } else if (tool_id == "TrimBySketch") {
        const auto* sketch = dynamic_cast<const CSmartLine*>(cutter);
        if (!sketch) {
            return false;
        }
        const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
        const Vec3 extrusion{
            static_cast<float>(system.normal.x),
            static_cast<float>(system.normal.y),
            static_cast<float>(system.normal.z)};
        if (sketch->IsClosed()) {
            TopoDS_Face profile;
            Vec3 profile_normal{};
            if (!BuildSketchProfileFace(*sketch, profile, profile_normal)
                || !TrimSolidByClosedProfile(
                    solid.m_Shape, profile, extrusion, positive, result)) {
                return false;
            }
        } else {
            TopoDS_Wire wire;
            const std::vector<CPoint3d> points =
                sketch->GetProfilePointsWorld();
            if (points.size() < 2 || !build_open_sketch_wire(*sketch, wire)) {
                return false;
            }
            const Vec3 tangent{
                static_cast<float>(points[1].x - points[0].x),
                static_cast<float>(points[1].y - points[0].y),
                static_cast<float>(points[1].z - points[0].z)};
            const Vec3 side = cross(extrusion, tangent);
            const Vec3 side_origin{
                static_cast<float>(points[0].x),
                static_cast<float>(points[0].y),
                static_cast<float>(points[0].z)};
            if (!TrimSolidByOpenProfile(
                    solid.m_Shape,
                    wire,
                    extrusion,
                    side,
                    side_origin,
                    positive,
                    result)) {
                return false;
            }
        }
    } else {
        return false;
    }

    solid.m_Shape = result;
    return solid.ReBuldMesh();
}

bool apply_sketch_feature(CSolid& solid,
                          CAlfaDoc& document,
                          const std::vector<ToolParameter>& parameters) {
    const unsigned long profile_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "profile.id", 0.0)));
    const auto* sketch = dynamic_cast<const CSmartLine*>(
        document.FindObjectById(profile_id));
    if (!sketch || !sketch->IsClosed() || solid.m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Face profile_face;
    Vec3 profile_normal{};
    if (!BuildSketchProfileFace(*sketch, profile_face, profile_normal)) {
        return false;
    }

    const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
    const Vec3 outward_normal = normalize(Vec3{
        static_cast<float>(system.normal.x),
        static_cast<float>(system.normal.y),
        static_cast<float>(system.normal.z)});
    const double depth = param(parameters, "depth", 10.0);
    const double taper = param(parameters, "taper", 0.0);
    const SketchFeatureOperation operation =
        param(parameters, "operation", 1.0) >= 0.5
        ? SketchFeatureOperation::Cut
        : SketchFeatureOperation::Protrusion;

    TopoDS_Shape result;
    if (!BuildSketchFeatureShape(solid.m_Shape,
                                 profile_face,
                                 outward_normal,
                                 depth,
                                 taper,
                                 operation,
                                 result)) {
        return false;
    }
    solid.m_Shape = result;
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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

bool apply_offset_face(CSolid& solid,
                       const std::vector<ParametricParameterValue>& saved_parameters,
                       const std::vector<ToolParameter>& parameters) {
    const int face_index = static_cast<int>(
        saved_param(saved_parameters, "face.index", -1.0));
    const double distance = param(parameters, "distance", 0.0);
    if (face_index < 0 || solid.m_Shape.IsNull()
        || std::fabs(distance) <= 0.00001) {
        return false;
    }

    const TopoDS_Face face = solid.GetTopoFace(face_index);
    TopoDS_Shape result;
    if (face.IsNull()
        || !BuildOffsetFaceShape(solid.m_Shape, face, distance, result)) {
        return false;
    }

    solid.m_Shape = result;
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    return solid.ReBuldMesh();
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
    const CAlfaObject* profile_object = document.FindObjectById(profile_id);
    const auto* profile = dynamic_cast<const CPolyline*>(profile_object);
    const auto* sketch = dynamic_cast<const CSmartLine*>(profile_object);
    if (!old_solid || (!profile && !sketch)) {
        return false;
    }

    TopoDS_Face profile_face;
    Vec3 normal{};
    const bool profile_built = sketch
        ? BuildSketchProfileFace(*sketch, profile_face, normal)
        : build_profile_face_from_polyline(*profile, profile_face, normal);
    if (!profile_built) {
        return false;
    }

    const double distance = param(parameters, "distance", 1.0);
    if (std::fabs(distance) <= 0.0001) {
        return false;
    }
    const bool reverse = param(parameters, "reverse", 0.0) >= 0.5;
    const double signed_distance = reverse ? -distance : distance;
    const double taper_angle_degrees = param(parameters, "taper", 0.0);

    TopoDS_Shape shape =
        BuildExtrudeShape(profile_face, normal, signed_distance, taper_angle_degrees);

    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->m_id = old_solid->m_id;
    solid->SetName(old_solid->GetName());
    solid->SetColor(old_solid->GetColor());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->m_LayerID = old_solid->m_LayerID;
    solid->CopyOperationTreeFrom(*old_solid);
    if (!solid->ReBuldMesh()) {
        return false;
    }

    objects[object_index] = std::move(solid);
    return true;
}

bool rebuild_swept_base(CAlfaDoc& document,
                        size_t object_index,
                        const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return false;
    }

    const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long section_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "section.id", 0.0)));
    const unsigned long guide_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "guide.id", 0.0)));
    const auto* section = dynamic_cast<const CSmartLine*>(document.FindObjectById(section_id));
    const CAlfaObject* guide = document.FindObjectById(guide_id);
    const int transition_mode = static_cast<int>(param(parameters, "transition", 1.0));
    const double delta_x = param(parameters, "dx", 0.0);
    const double delta_y = param(parameters, "dy", 0.0);
    const double angle_degrees = param(parameters, "angle", 0.0);
    if (!old_solid || !section || !guide) {
        return false;
    }

    TopoDS_Shape shape = BuildSweptSolidShape(
        *section, *guide, transition_mode, delta_x, delta_y, angle_degrees);
    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->m_id = old_solid->m_id;
    solid->SetName(old_solid->GetName());
    solid->SetColor(old_solid->GetColor());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->m_LayerID = old_solid->m_LayerID;
    solid->CopyOperationTreeFrom(*old_solid);
    if (!solid->ReBuldMesh()) {
        return false;
    }
    objects[object_index] = std::move(solid);
    return true;
}

bool rebuild_frame_base(CAlfaDoc& document,
                        size_t object_index,
                        const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return false;
    }

    const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long profile_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "profile.id", 0.0)));
    const auto* profile = dynamic_cast<const CSmartLine*>(
        document.FindObjectById(profile_id));
    const double width = param(parameters, "width", 40.0);
    const double height = param(parameters, "height", 30.0);
    if (!old_solid || !profile) {
        return false;
    }

    TopoDS_Shape shape = BuildFrameSolidShape(*profile, width, height);
    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->m_id = old_solid->m_id;
    solid->SetName(old_solid->GetName());
    solid->SetColor(old_solid->GetColor());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->m_LayerID = old_solid->m_LayerID;
    solid->CopyOperationTreeFrom(*old_solid);
    if (!solid->ReBuldMesh()) {
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

bool rebuild_polyhedron_base(
    CAlfaDoc& document,
    size_t object_index,
    const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return false;
    }

    const auto* old_solid =
        dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long profile_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "profile.id", 0.0)));
    const auto* profile = dynamic_cast<const CSmartLine*>(
        document.FindObjectById(profile_id));
    if (!old_solid || !profile) {
        return false;
    }

    std::vector<Vec3> profile_points;
    for (const CPoint3d& point : profile->GetProfilePointsWorld()) {
        profile_points.push_back({
            static_cast<float>(point.x),
            static_cast<float>(point.y),
            static_cast<float>(point.z)});
    }
    const SketchCoordinateSystem& system = profile->GetCoordinateSystem();
    const Vec3 axis_origin{
        static_cast<float>(system.origin.x),
        static_cast<float>(system.origin.y),
        static_cast<float>(system.origin.z)};
    const int axis_index = std::clamp(
        static_cast<int>(param(parameters, "axis", 2.0)), 0, 2);
    const int turns = std::max(
        3, static_cast<int>(param(parameters, "turns", 8.0)));

    const Vec3 axis_direction = revolve_axis_direction(axis_index);
    TopoDS_Shape shape;
    bool shape_built = false;
    if (profile->IsClosed()) {
        TopoDS_Face profile_face;
        Vec3 profile_normal{};
        shape_built = BuildSketchProfileFace(
                *profile, profile_face, profile_normal)
            && BuildPolyhedronShapeFromProfileFace(
                profile_face,
                axis_origin,
                axis_direction,
                turns,
                shape);
    } else {
        TopoDS_Wire profile_wire;
        shape_built = BuildOpenSketchProfileWire(
                *profile, profile_wire)
            && BuildPolyhedronShapeFromOpenProfileWire(
                profile_wire,
                axis_origin,
                axis_direction,
                turns,
                shape);
        if (!shape_built) {
            TopoDS_Face axis_closed_face;
            Vec3 profile_normal{};
            shape_built = BuildSketchRevolveProfileFace(
                    *profile,
                    axis_origin,
                    axis_direction,
                    axis_closed_face,
                    profile_normal)
                && BuildPolyhedronShapeFromProfileFace(
                    axis_closed_face,
                    axis_origin,
                    axis_direction,
                    turns,
                    shape);
        }
    }
    bool has_exact_curves = profile->GetNumFillets() > 0;
    for (std::size_t line_index = 0;
         !has_exact_curves && line_index < profile->GetNumLines();
         ++line_index) {
        const CLinkLine* line = profile->GetLine(line_index);
        has_exact_curves = line
            && (line->GetType() == LinkLineType::Bezier
                || line->GetType() == LinkLineType::Arc);
    }
    if (!shape_built && has_exact_curves) {
        return false;
    }
    if (!shape_built
        && !BuildPolyhedronShape(
            profile_points,
            profile->IsClosed(),
            axis_origin,
            axis_direction,
            turns,
            shape)) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->m_id = old_solid->m_id;
    solid->SetName(old_solid->GetName());
    solid->SetColor(old_solid->GetColor());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->m_LayerID = old_solid->m_LayerID;
    solid->CopyOperationTreeFrom(*old_solid);
    if (!solid->ReBuldMesh()) {
        return false;
    }
    objects[object_index] = std::move(solid);
    return true;
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
    const CAlfaObject* profile_object = document.FindObjectById(profile_id);
    const auto* profile = dynamic_cast<const CPolyline*>(profile_object);
    const auto* sketch = dynamic_cast<const CSmartLine*>(profile_object);
    const double angle_degrees = param(parameters, "angle", 360.0);
    const int axis_index = std::clamp(static_cast<int>(param(parameters, "axis", 2.0)), 0, 2);
    if (!old_solid || (!profile && !sketch) || angle_degrees <= 0.0001) {
        return false;
    }

    TopoDS_Shape profile_shape;
    Vec3 axis_origin{};
    Vec3 axis = revolve_axis_direction(axis_index);
    bool profile_built = false;
    if (sketch) {
        TopoDS_Face sketch_face;
        Vec3 sketch_normal{};
        const SketchCoordinateSystem& system =
            sketch->GetCoordinateSystem();
        axis_origin = {
            static_cast<float>(system.origin.x),
            static_cast<float>(system.origin.y),
            static_cast<float>(system.origin.z)};
        profile_built = BuildSketchRevolveProfileFace(
            *sketch,
            axis_origin,
            axis,
            sketch_face,
            sketch_normal);
        if (profile_built) {
            profile_shape = sketch_face;
        }
    } else {
        profile_built = build_revolve_profile(
            *profile, axis_index, profile_shape, axis_origin);
    }
    if (!profile_built || dot(axis, axis) <= 1.0e-12f) {
        return false;
    }

    TopoDS_Shape shape;
    try {
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
    solid->SetColor(old_solid->GetColor());
    solid->SetMaterial(old_solid->GetMaterial());
    solid->SetMaterialId(old_solid->GetMaterialId());
    solid->SetGroupName(old_solid->GetGroupName());
    solid->SetVisible(old_solid->IsVisible());
    solid->m_LayerID = old_solid->m_LayerID;
    solid->CopyOperationTreeFrom(*old_solid);
    if (!solid->ReBuldMesh()) {
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
                                        param(parameters, "radius", 2.0),
                                        &operation_created_faces[i],
                                        &operation_created_faces)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "fillet_edge") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_fillet_edges(*solid,
                                    edge_refs_from_saved_parameters(operation.saved_parameters),
                                    param(parameters, "radius", 2.0),
                                    &operation_created_faces[i],
                                    &operation_created_faces)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "ChamferSolid") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_chamfer_edges(*solid,
                                     edge_refs_from_saved_parameters(operation.saved_parameters),
                                     param(parameters, "distance", 2.0),
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
        } else if (operation.tool_id == "SolidOffsetFace") {
            const std::vector<ToolParameter> parameters =
                parameters_for_operation(registry, operation);
            if (!apply_offset_face(*solid, operation.saved_parameters, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "SolidSketchFeature") {
            const std::vector<ToolParameter> parameters =
                parameters_for_operation(registry, operation);
            if (!apply_sketch_feature(*solid, document, parameters)) {
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
        } else if (operation.tool_id == "TrimByPlane"
                   || operation.tool_id == "TrimBySketch"
                   || operation.tool_id == "TrimBySurface") {
            const std::vector<ToolParameter> parameters =
                parameters_for_operation(registry, operation);
            if (!apply_trim_operation(
                    *solid, document, operation.tool_id, parameters)) {
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
        const std::string operation_label = operation.tool_id == "SolidSketchFeature"
            ? registry.LabelFor(operation.tool_id)
            : operation.label;
        solid->SetParametricOperation(solid->GetOperationTree().size(),
                                      operation.tool_id,
                                      operation_label,
                                      operation.saved_parameters,
                                      face_indices_in_shape(operation_created_faces[i], final_faces));
    }
    return true;
}

std::unique_ptr<CMesh3D> make_box(const std::string& name, float width, float height, float depth, float x, float y, float z, Color color) {
    (void)color;
    auto mesh = std::make_unique<CMesh3D>(name);
    mesh->SetColor(kDefaultMeshObjectColor);

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

    if (objects[index]) {
        mesh->SetColor(objects[index]->GetColor());
        mesh->SetMaterial(objects[index]->GetMaterial());
        mesh->SetMaterialId(objects[index]->GetMaterialId());
        mesh->SetGroupName(objects[index]->GetGroupName());
        mesh->SetVisible(objects[index]->IsVisible());
        mesh->m_LayerID = objects[index]->m_LayerID;
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

struct TableGeometry {
    double width;
    double depth;
    double height;
    double top;
    double leg;
    double inset;
    double apron_height;
    double apron_thickness;
    double arc_bulge;
    bool arc_top;
};

TableGeometry table_geometry(const std::vector<ToolParameter>& parameters) {
    TableGeometry result{
        param(parameters, "width", 1000.0),
        param(parameters, "depth", 800.0),
        param(parameters, "height", 750.0),
        param(parameters, "top_thickness", 50.0),
        param(parameters, "leg_size", 40.0),
        param(parameters, "leg_inset", 40.0),
        param(parameters, "apron_height", 60.0),
        param(parameters, "apron_thickness", 20.0),
        param(parameters, "arc_bulge", 100.0),
        param(parameters, "top_shape", 0.0) >= 0.5};
    result.top = std::clamp(result.top, 1.0, result.height - 1.0);
    result.leg = std::clamp(result.leg, 1.0, std::min(result.width, result.depth) * 0.4);
    result.inset = std::clamp(
        result.inset, 0.0,
        std::max(0.0, (std::min(result.width, result.depth) - result.leg) * 0.5));
    result.apron_height = std::clamp(result.apron_height, 1.0, result.height - result.top);
    result.apron_thickness = std::clamp(
        result.apron_thickness, 1.0, std::min(result.leg, result.depth * 0.25));
    result.arc_bulge = std::clamp(result.arc_bulge, 1.0, result.width);
    return result;
}

TopoDS_Shape table_box_shape(double x, double y, double z,
                             double width, double height, double depth) {
    BRepPrimAPI_MakeBox builder(gp_Pnt(x, y, z), width, height, depth);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape table_top_shape(const TableGeometry& geometry) {
    const double y = geometry.height - geometry.top;
    if (!geometry.arc_top) {
        return table_box_shape(
            -geometry.width * 0.5, y, -geometry.depth * 0.5,
            geometry.width, geometry.top, geometry.depth);
    }
    try {
        const gp_Pnt front_left(-geometry.width * 0.5, y, -geometry.depth * 0.5);
        const gp_Pnt front_right(geometry.width * 0.5, y, -geometry.depth * 0.5);
        const gp_Pnt back_right(geometry.width * 0.5, y, geometry.depth * 0.5);
        const gp_Pnt back_left(-geometry.width * 0.5, y, geometry.depth * 0.5);
        GC_MakeArcOfCircle right_arc(
            front_right,
            gp_Pnt(geometry.width * 0.5 + geometry.arc_bulge, y, 0.0),
            back_right);
        GC_MakeArcOfCircle left_arc(
            back_left,
            gp_Pnt(-geometry.width * 0.5 - geometry.arc_bulge, y, 0.0),
            front_left);
        if (!right_arc.IsDone() || !left_arc.IsDone()) {
            return {};
        }
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(front_left, front_right).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(right_arc.Value()).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(back_right, back_left).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(left_arc.Value()).Edge());
        if (!wire.IsDone()) {
            return {};
        }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        if (!face.IsDone()) {
            return {};
        }
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, geometry.top, 0.0));
        prism.Build();
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
    } catch (...) {
        return {};
    }
}

std::unique_ptr<CSolid> make_table_solid(
    const std::string& name, TopoDS_Shape shape, Color color) {
    if (shape.IsNull()) {
        return nullptr;
    }
    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName(name);
    solid->SetColor(color);
    if (!solid->ReBuldMesh()) {
        return nullptr;
    }
    return solid;
}

std::unique_ptr<CAssociativeClone> make_table_clone(
    const CSolid& source, unsigned long source_id, const std::string& name,
    double dx, double dy, double dz) {
    auto clone = std::make_unique<CAssociativeClone>(source.m_Shape, source_id);
    clone->SetName(name);
    clone->SetColor(source.GetColor());
    clone->SetMaterial(source.GetMaterial());
    clone->SetMaterialId(source.GetMaterialId());
    if (!clone->ReBuldMesh()) {
        return nullptr;
    }
    clone->Translate({
        static_cast<float>(dx),
        static_cast<float>(dy),
        static_cast<float>(dz)});
    return clone;
}

void add_table_object(CAlfaDoc& document,
                      std::unique_ptr<CAlfaObject> object,
                      std::vector<unsigned long>& element_ids) {
    if (!object) {
        return;
    }
    document.AddObject(std::move(object));
    if (CAlfaObject* added = document.GetSelectedObject()) {
        element_ids.push_back(added->m_id);
    }
}

void create_table(CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
    const TableGeometry g = table_geometry(parameters);
    const Color wood{0.58f, 0.32f, 0.15f};
    const Color apron_color{0.72f, 0.16f, 0.12f};
    const double leg_height = g.height - g.top;
    const double left = -g.width * 0.5 + g.inset;
    const double right = g.width * 0.5 - g.inset - g.leg;
    const double front = -g.depth * 0.5 + g.inset;
    const double back = g.depth * 0.5 - g.inset - g.leg;
    const double apron_y = leg_height - g.apron_height;
    const double long_x = left + g.leg;
    const double long_length = std::max(1.0, right - long_x);
    const double long_front_z = front + g.leg;
    const double long_back_z = back - g.apron_thickness;
    const double short_z = front + g.leg;
    const double short_length = std::max(1.0, back - short_z);
    const double short_left_x = left + g.leg;
    const double short_right_x = right - g.apron_thickness;

    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto top = make_table_solid("Table Top", table_top_shape(g), wood);
    auto leg = make_table_solid(
        "Table Leg", table_box_shape(left, 0.0, front, g.leg, leg_height, g.leg), wood);
    if (!top || !leg) return;
    document.EnsureObjectId(*leg);
    CSolid* leg_source = leg.get();
    const unsigned long leg_id = leg->m_id;
    parts.push_back(std::move(top));
    parts.push_back(std::move(leg));
    parts.push_back(make_table_clone(*leg_source, leg_id, "Table Leg Clone 1", right - left, 0.0, 0.0));
    parts.push_back(make_table_clone(*leg_source, leg_id, "Table Leg Clone 2", right - left, 0.0, back - front));
    parts.push_back(make_table_clone(*leg_source, leg_id, "Table Leg Clone 3", 0.0, 0.0, back - front));

    auto long_apron = make_table_solid(
        "Table Long Apron",
        table_box_shape(long_x, apron_y, long_front_z,
                        long_length, g.apron_height, g.apron_thickness),
        apron_color);
    if (!long_apron) return;
    document.EnsureObjectId(*long_apron);
    CSolid* long_source = long_apron.get();
    const unsigned long long_id = long_apron->m_id;
    parts.push_back(std::move(long_apron));
    parts.push_back(make_table_clone(
        *long_source, long_id, "Table Long Apron Clone",
        0.0, 0.0, long_back_z - long_front_z));

    auto short_apron = make_table_solid(
        "Table Short Apron",
        table_box_shape(short_left_x, apron_y, short_z,
                        g.apron_thickness, g.apron_height, short_length),
        apron_color);
    if (!short_apron) return;
    document.EnsureObjectId(*short_apron);
    CSolid* short_source = short_apron.get();
    const unsigned long short_id = short_apron->m_id;
    parts.push_back(std::move(short_apron));
    parts.push_back(make_table_clone(
        *short_source, short_id, "Table Short Apron Clone",
        short_right_x - short_left_x, 0.0, 0.0));

    if (parts.size() != 9
        || std::any_of(parts.begin(), parts.end(),
                       [](const std::unique_ptr<CAlfaObject>& item) { return !item; })) {
        return;
    }
    std::vector<unsigned long> ids;
    for (auto& part : parts) {
        add_table_object(document, std::move(part), ids);
    }
    document.AddObject(std::make_unique<CAssembled>("Table", std::move(ids)));
}

void copy_table_identity(const CAlfaObject& old, CAlfaObject& replacement) {
    replacement.m_id = old.m_id;
    replacement.m_LayerID = old.m_LayerID;
    replacement.SetColor(old.GetColor());
    replacement.SetMaterial(old.GetMaterial());
    replacement.SetMaterialId(old.GetMaterialId());
    replacement.SetVisible(old.IsVisible());
}

void rebuild_table(CAlfaDoc& document,
                   size_t assembly_index,
                   const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size()) return;
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly
        || (assembly->GetElementIds().size() != 5
            && assembly->GetElementIds().size() != 9)) return;
    const std::vector<unsigned long> ids = assembly->GetElementIds();
    const bool migrate_mesh_table = ids.size() == 5;
    const TableGeometry g = table_geometry(parameters);
    const Color wood{0.58f, 0.32f, 0.15f};
    const Color red{0.72f, 0.16f, 0.12f};
    const double leg_height = g.height - g.top;
    const double left = -g.width * 0.5 + g.inset;
    const double right = g.width * 0.5 - g.inset - g.leg;
    const double front = -g.depth * 0.5 + g.inset;
    const double back = g.depth * 0.5 - g.inset - g.leg;
    const double apron_y = leg_height - g.apron_height;
    const double long_x = left + g.leg;
    const double long_front_z = front + g.leg;
    const double long_back_z = back - g.apron_thickness;
    const double short_z = front + g.leg;
    const double short_left_x = left + g.leg;
    const double short_right_x = right - g.apron_thickness;

    std::vector<std::unique_ptr<CAlfaObject>> replacements;
    replacements.push_back(make_table_solid("Table Top", table_top_shape(g), wood));
    auto leg = make_table_solid("Table Leg", table_box_shape(left, 0.0, front, g.leg, leg_height, g.leg), wood);
    if (!leg) return;
    CSolid* leg_source = leg.get();
    replacements.push_back(std::move(leg));
    replacements.push_back(make_table_clone(*leg_source, ids[1], "Table Leg Clone 1", right - left, 0.0, 0.0));
    replacements.push_back(make_table_clone(*leg_source, ids[1], "Table Leg Clone 2", right - left, 0.0, back - front));
    replacements.push_back(make_table_clone(*leg_source, ids[1], "Table Leg Clone 3", 0.0, 0.0, back - front));
    auto long_apron = make_table_solid(
        "Table Long Apron",
        table_box_shape(long_x, apron_y, long_front_z,
                        std::max(1.0, right - long_x), g.apron_height, g.apron_thickness), red);
    if (!long_apron) return;
    CSolid* long_source = long_apron.get();
    unsigned long long_source_id = migrate_mesh_table ? 0UL : ids[5];
    if (migrate_mesh_table) {
        document.EnsureObjectId(*long_apron);
        long_source_id = long_apron->m_id;
    }
    replacements.push_back(std::move(long_apron));
    replacements.push_back(make_table_clone(*long_source, long_source_id, "Table Long Apron Clone", 0.0, 0.0, long_back_z - long_front_z));
    auto short_apron = make_table_solid(
        "Table Short Apron",
        table_box_shape(short_left_x, apron_y, short_z,
                        g.apron_thickness, g.apron_height, std::max(1.0, back - short_z)), red);
    if (!short_apron) return;
    CSolid* short_source = short_apron.get();
    unsigned long short_source_id = migrate_mesh_table ? 0UL : ids[7];
    if (migrate_mesh_table) {
        document.EnsureObjectId(*short_apron);
        short_source_id = short_apron->m_id;
    }
    replacements.push_back(std::move(short_apron));
    replacements.push_back(make_table_clone(*short_source, short_source_id, "Table Short Apron Clone", short_right_x - short_left_x, 0.0, 0.0));
    if (std::any_of(replacements.begin(), replacements.end(),
                    [](const std::unique_ptr<CAlfaObject>& item) { return !item; })) return;

    const size_t existing_count = ids.size();
    for (size_t i = 0; i < existing_count; ++i) {
        const size_t index = document.FindObjectIndexById(ids[i]);
        if (index >= objects.size() || !objects[index]) continue;
        copy_table_identity(*objects[index], *replacements[i]);
        objects[index] = std::move(replacements[i]);
    }
    if (migrate_mesh_table) {
        std::vector<unsigned long> upgraded_ids = ids;
        for (size_t i = existing_count; i < replacements.size(); ++i) {
            add_table_object(document, std::move(replacements[i]), upgraded_ids);
        }
        assembly->SetElementIds(std::move(upgraded_ids));
    }
}

void apply_boolean_to_selected_solids(CAlfaDoc& document, BooleanKind kind, const char* result_name, Color color) {
    (void)color;
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
    result->SetColor(kDefaultSolidObjectColor);
    result->ReBuldMesh();

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
    static const SolidBeamTool solid_beam_tool;
    static const SolidBoxTool solid_box_tool;
    static const SolidCylinderTool solid_cylinder_tool;
    static const SolidPrismTool solid_prism_tool;
    static const SolidSphereTool solid_sphere_tool;
    static const SolidTorusTool solid_torus_tool;
    static const SolidTransformTool solid_transform_tool;
    tools_.push_back(solid_beam_tool.CreateToolDefinition());
    tools_.push_back(solid_box_tool.CreateToolDefinition());
    tools_.push_back(solid_cylinder_tool.CreateToolDefinition());
    tools_.push_back(solid_prism_tool.CreateToolDefinition());
    tools_.push_back(solid_sphere_tool.CreateToolDefinition());
    tools_.push_back(solid_torus_tool.CreateToolDefinition());
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
        "SolidTwoSketches",
        "Body by Two Sketches",
        {
            {"profile.id", "First Sketch ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"section.id", "Second Sketch ID", 0.0, 0.0, 4294967295.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.CreateSolidFromTwoSelectedSketches();
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>&) {
            document.RebuildTwoSketchSolid(index);
        }
    });

    tools_.push_back({
        "PlaneTool",
        "Plane",
        {
            {"mode", "Method", 1.0, 0.0, 5.0, 1.0, ToolParameterType::Combo,
                {"Factors A B C D", "Point + Normal", "3 Points",
                 "Plane XY", "Plane XZ", "Plane YZ"}},
            {"a", "A", 0.0, -1000000.0, 1000000.0, 0.1},
            {"b", "B", 0.0, -1000000.0, 1000000.0, 0.1},
            {"c", "C", 1.0, -1000000.0, 1000000.0, 0.1},
            {"d", "D", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"plane.origin.x", "Point X", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"plane.origin.y", "Point Y", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"plane.origin.z", "Point Z", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"plane.normal.x", "Normal X", 0.0, -1.0, 1.0, 0.1},
            {"plane.normal.y", "Normal Y", 0.0, -1.0, 1.0, 0.1},
            {"plane.normal.z", "Normal Z", 1.0, -1.0, 1.0, 0.1},
            {"p1.x", "Point 1 X", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p1.y", "Point 1 Y", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p1.z", "Point 1 Z", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p2.x", "Point 2 X", 100.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p2.y", "Point 2 Y", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p2.z", "Point 2 Z", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p3.x", "Point 3 X", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p3.y", "Point 3 Y", 100.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"p3.z", "Point 3 Z", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"offset", "Offset", 0.0, -1000000.0, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"size", "Size", 200.0, 1.0, 1000000.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_plane(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_plane(document, index, parameters);
        }
    });

    const auto trim_parameters = []() {
        return std::vector<ToolParameter>{
            {"direction", "Direction", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo,
                {"Positive / Inside", "Negative / Outside"}},
            {"cutter.id", "Cutter ID", 0.0, 0.0, 4294967295.0, 1.0}
        };
    };
    for (const auto& trim :
         std::vector<std::pair<std::string, std::string>>{
             {"TrimByPlane", "Trim By Plane"},
             {"TrimBySketch", "Trim By Sketch"},
             {"TrimBySurface", "Trim By Surface"}}) {
        tools_.push_back({
            trim.first,
            trim.second,
            trim_parameters(),
            [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
            [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
        });
    }

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

    tools_.push_back({
        "TrimMeshTest",
        "Trim Mesh Test",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "ClassifyFaceCut",
        "Classify Face Cut",
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
            {"radius", "Radius", 2.0, 0.01, 100.0, 0.1}
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
            {"radius", "Radius", 2.0, 0.01, 100.0, 0.1}
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
            {"distance", "Distance", 2.0, 0.01, 100.0, 0.1}
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
        "SolidOffsetFace",
        "Offset Face",
        {
            {"distance", "Distance", 1.0, -1000000.0, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SolidSketchFeature",
        "Boss / Pocket",
        {
            {"operation", "Operation", 1.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Boss", "Pocket"}},
            {"depth", "Depth", 10.0, 0.001, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"taper", "Taper Angle", 0.0, -89.0, 89.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"face.index", "Face Index", -1.0, -1.0, 1000000.0, 1.0}
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
        "SolidSweptTool",
        "Swept",
        {
            {"dx", "Delta X", 0.0, -1000000.0, 1000000.0, 0.1, ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"dy", "Delta Y", 0.0, -1000000.0, 1000000.0, 0.1, ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"angle", "Angle", 0.0, -360.0, 360.0, 1.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"transition", "Corner", 1.0, 0.0, 2.0, 1.0, ToolParameterType::Combo,
                {"Transformed", "Right Corner", "Round Corner"}},
            {"section.id", "Section ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"guide.id", "Guide ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_swept_base(document, index, parameters);
        }
    });

    tools_.push_back({
        "SolidFrameTool",
        "Frame",
        {
            {"width", "Width", 40.0, 0.1, 1000000.0, 1.0, ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 30.0, 0.1, 1000000.0, 1.0, ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_frame_base(document, index, parameters);
        }
    });

    tools_.push_back({
        "SolidPolyhedronTool",
        "Polyhedron",
        {
            {"turns", "Turns", 8.0, 3.0, 128.0, 1.0},
            {"axis", "Axis", 2.0, 0.0, 2.0, 1.0, ToolParameterType::Combo,
                {"Axis X", "Axis Y", "Axis Z"}},
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_polyhedron_base(document, index, parameters);
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
        "SolidLowPoly",
        "Low Poly",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "MeshFillContour",
        "Fiill Contour",
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

    tools_.push_back({
        "table",
        "Table",
        {
            {"width", "Width", 1000.0, 300.0, 5000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 800.0, 300.0, 3000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 750.0, 300.0, 1500.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"top_thickness", "Top Thickness", 50.0, 10.0, 200.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_size", "Leg Size", 40.0, 10.0, 200.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_inset", "Leg Inset", 40.0, 0.0, 500.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_height", "Apron Height", 60.0, 10.0, 300.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_thickness", "Apron Thickness", 20.0, 5.0, 100.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"top_shape", "Top Shape", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Rectangle", "Arc Ends"}},
            {"arc_bulge", "Arc Bulge", 100.0, 10.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_table(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_table(document, index, parameters);
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

    const size_t object_count_before = document.GetObjects().size();
    tool->create(document, tool->defaults);
    if (id == "table"
        && document.GetObjects().size() <= object_count_before) {
        return {};
    }
    if (id == "table"
        && !dynamic_cast<CAssembled*>(document.GetSelectedObject())) {
        return {};
    }
    if (tool->defaults.empty()) {
        return {};
    }
    const size_t object_index = document.GetSelectedObjectIndex();
    store_parametric_definition(document, object_index, tool->id, tool->label, 0, tool->defaults);
    return {tool->id, object_index, 0, tool->defaults};
}

ActiveParametricObject ToolRegistry::CreateParametricObject(const std::string& id,
                                                            CAlfaDoc& document,
                                                            const std::vector<ToolParameter>& parameters) const {
    const ToolDefinition* tool = Find(id);
    if (!tool || !tool->create || parameters.empty()) {
        return {};
    }

    tool->create(document, parameters);
    const size_t object_index = document.GetSelectedObjectIndex();
    store_parametric_definition(document, object_index, tool->id, tool->label, 0, parameters);
    return {tool->id, object_index, 0, parameters};
}

ActiveParametricObject ToolRegistry::ApplyTrimToSelection(
    const std::string& id,
    CAlfaDoc& document) const {
    const ToolDefinition* tool = Find(id);
    if (!tool
        || (id != "TrimByPlane"
            && id != "TrimBySketch"
            && id != "TrimBySurface")) {
        return {};
    }

    auto& objects = document.GetObjects();
    size_t body_index = objects.size();
    CAlfaObject* cutter = nullptr;
    for (size_t index : document.GetSelectedObjectIndices()) {
        if (index >= objects.size() || !objects[index]) {
            continue;
        }
        CAlfaObject* object = objects[index].get();
        if (auto* surface = dynamic_cast<CSurfaceSet*>(object)) {
            const bool is_plane =
                surface->GetParametricToolId() == "PlaneTool";
            if ((id == "TrimByPlane" && is_plane)
                || (id == "TrimBySurface" && !is_plane)) {
                if (cutter) {
                    return {};
                }
                cutter = object;
            }
            continue;
        }
        if (id == "TrimBySketch"
            && dynamic_cast<CSmartLine*>(object)) {
            if (cutter) {
                return {};
            }
            cutter = object;
            continue;
        }
        if (dynamic_cast<CSolid*>(object)) {
            if (body_index != objects.size()) {
                return {};
            }
            body_index = index;
        }
    }

    auto* body = body_index < objects.size()
        ? dynamic_cast<CSolid*>(objects[body_index].get())
        : nullptr;
    if (!body || !cutter || body->GetNumOperations() <= 0) {
        return {};
    }

    document.EnsureObjectId(*cutter);
    std::vector<ToolParameter> parameters = tool->defaults;
    for (ToolParameter& parameter : parameters) {
        if (parameter.id == "cutter.id") {
            parameter.value = static_cast<double>(cutter->m_id);
        }
    }

    const TopoDS_Shape previous_shape = body->m_Shape;
    if (!apply_trim_operation(*body, document, id, parameters)) {
        return {};
    }
    const size_t operation_index =
        static_cast<size_t>(body->GetNumOperations());
    body->SetParametricOperation(
        operation_index,
        id,
        tool->label,
        parameter_values(parameters),
        body->FindCreatedSurfaceIndices(previous_shape));
    return {id, body_index, operation_index, std::move(parameters)};
}

ActiveParametricObject ToolRegistry::ApplySketchFeatureToSelection(
    CAlfaDoc& document) const {
    constexpr const char* kToolId = "SolidSketchFeature";
    const ToolDefinition* tool = Find(kToolId);
    if (!tool) {
        return {};
    }

    auto& objects = document.GetObjects();
    CSmartLine* sketch = nullptr;
    size_t selected_body_index = objects.size();
    for (size_t index : document.GetSelectedObjectIndices()) {
        if (index >= objects.size() || !objects[index]) {
            continue;
        }
        if (auto* candidate = dynamic_cast<CSmartLine*>(objects[index].get())) {
            if (sketch) {
                return {};
            }
            sketch = candidate;
        } else if (dynamic_cast<CSolid*>(objects[index].get())) {
            if (selected_body_index != objects.size()) {
                return {};
            }
            selected_body_index = index;
        }
    }
    if (!sketch || !sketch->IsClosed()) {
        return {};
    }

    document.EnsureObjectId(*sketch);
    size_t body_index = selected_body_index;
    int face_index = -1;
    if (sketch->HasFaceAttachment()) {
        const SketchFaceAttachment& attachment = sketch->GetFaceAttachment();
        body_index = document.FindObjectIndexById(attachment.body_id);
        face_index = attachment.face_index;
        if (selected_body_index < objects.size()
            && selected_body_index != body_index) {
            return {};
        }
    } else if (selected_body_index < objects.size()) {
        auto* selected_body = dynamic_cast<CSolid*>(objects[selected_body_index].get());
        if (!selected_body || !selected_body->HasSelectedFace()) {
            return {};
        }
        document.EnsureObjectId(*selected_body);
        face_index = selected_body->GetSelectedFaceIndex();
        sketch->SetFaceAttachment(selected_body->m_id, face_index);
    }

    auto* body = body_index < objects.size()
        ? dynamic_cast<CSolid*>(objects[body_index].get())
        : nullptr;
    if (!body || body->GetNumOperations() <= 0 || face_index < 0) {
        return {};
    }

    std::vector<ToolParameter> parameters = tool->defaults;
    for (ToolParameter& parameter : parameters) {
        if (parameter.id == "profile.id") {
            parameter.value = static_cast<double>(sketch->m_id);
        } else if (parameter.id == "face.index") {
            parameter.value = static_cast<double>(face_index);
        }
    }

    const TopoDS_Shape previous_shape = body->m_Shape;
    if (!apply_sketch_feature(*body, document, parameters)) {
        return {};
    }
    const size_t operation_index =
        static_cast<size_t>(body->GetNumOperations());
    body->SetParametricOperation(
        operation_index,
        kToolId,
        tool->label,
        parameter_values(parameters),
        body->FindCreatedSurfaceIndices(previous_shape));
    document.UpdateAttachedSketches();
    return {kToolId, body_index, operation_index, std::move(parameters)};
}

ActiveParametricObject ToolRegistry::ApplyOffsetFaceToSelection(
    CAlfaDoc& document) const {
    constexpr const char* kToolId = "SolidOffsetFace";
    const ToolDefinition* tool = Find(kToolId);
    if (!tool) {
        return {};
    }

    auto& objects = document.GetObjects();
    CSolid* body = document.GetSelectedFaceSolid();
    size_t body_index = objects.size();
    if (body) {
        for (size_t index = 0; index < objects.size(); ++index) {
            if (objects[index].get() == body) {
                body_index = index;
                break;
            }
        }
    }
    if (!body || body->GetNumOperations() <= 0 || !body->HasSelectedFace()) {
        return {};
    }
    if (body_index >= objects.size()) {
        return {};
    }

    const int face_index = body->GetSelectedFaceIndex();
    if (face_index < 0) {
        return {};
    }

    std::vector<ToolParameter> parameters = tool->defaults;
    std::vector<ParametricParameterValue> saved_parameters =
        parameter_values(parameters);
    saved_parameters.push_back(
        {"face.index", static_cast<double>(face_index)});

    const TopoDS_Shape previous_shape = body->m_Shape;
    if (!apply_offset_face(*body, saved_parameters, parameters)) {
        return {};
    }

    const size_t operation_index =
        static_cast<size_t>(body->GetNumOperations());
    body->SetParametricOperation(
        operation_index,
        kToolId,
        tool->label,
        std::move(saved_parameters),
        body->FindCreatedSurfaceIndices(previous_shape));
    document.UpdateAttachedSketches();
    return {kToolId, body_index, operation_index, std::move(parameters)};
}

bool ToolRegistry::ApplyOffsetFaceOnce(CAlfaDoc& document,
                                       double distance) const {
    CSolid* body = document.GetSelectedFaceSolid();
    if (!body || !body->HasSelectedFace()) {
        return false;
    }

    const int face_index = body->GetSelectedFaceIndex();
    std::vector<ParametricParameterValue> saved_parameters = {
        {"face.index", static_cast<double>(face_index)}
    };
    std::vector<ToolParameter> parameters = {
        {"distance", "Distance", distance, -1000000.0, 1000000.0, 0.1,
            ToolParameterType::Number, {}, ToolParameterUnit::Length}
    };
    const bool applied =
        apply_offset_face(*body, saved_parameters, parameters);
    if (applied) {
        document.UpdateAttachedSketches();
    }
    return applied;
}

void ToolRegistry::Rebuild(const ActiveParametricObject& active_object, CAlfaDoc& document) const {
    const int boolean_tool_index = static_cast<int>(
        param(active_object.parameters, "boolean.tool_index", -1.0));
    if (boolean_tool_index >= 0
        && (active_object.tool_id == "SolidBox"
            || active_object.tool_id == "SolidCylinder")
        && active_object.object_index < document.GetObjects().size()) {
        auto* parent = dynamic_cast<CSolid*>(
            document.GetObjects()[active_object.object_index].get());
        CSolid* box = parent
            ? parent->GetBooleanTool(static_cast<size_t>(boolean_tool_index))
            : nullptr;
        ParametricFunction* boolean_operation = parent
            ? parent->GetOperation(static_cast<int>(active_object.operation_index))
            : nullptr;
        if (!box || !boolean_operation || boolean_operation->ToolId != "boolean") {
            return;
        }

        const bool rebuilt = active_object.tool_id == "SolidBox"
            ? SolidBoxTool().RebuildShape(*box, active_object.parameters)
            : SolidCylinderTool().RebuildShape(*box, active_object.parameters);
        if (!rebuilt) return;
        std::vector<int> box_surfaces;
        box_surfaces.reserve(static_cast<size_t>(box->GetNumSurfaces()));
        for (int index = 0; index < box->GetNumSurfaces(); ++index) {
            box_surfaces.push_back(index);
        }
        const std::string primitive_label = active_object.tool_id == "SolidBox"
            ? SolidBoxTool().GetLabel()
            : SolidCylinderTool().GetLabel();
        box->SetParametricOperation(
            0,
            active_object.tool_id,
            primitive_label,
            parameter_values(active_object.parameters),
            std::move(box_surfaces));

        std::vector<ParametricParameterValue> boolean_parameters =
            boolean_operation->Parameters;
        const double boolean_kind = param(
            active_object.parameters,
            active_object.tool_id == "SolidBox" ? "depth" : "height",
            0.0) >= 0.0 ? 0.0 : 1.0;
        auto operation_parameter = std::find_if(
            boolean_parameters.begin(),
            boolean_parameters.end(),
            [](const ParametricParameterValue& parameter) {
                return parameter.id == "operation";
            });
        if (operation_parameter == boolean_parameters.end()) {
            boolean_parameters.push_back({"operation", boolean_kind});
        } else {
            operation_parameter->value = boolean_kind;
        }
        parent->SetParametricOperation(
            active_object.operation_index,
            "boolean",
            boolean_kind == 0.0 ? "Boolean Union" : "Boolean Cut",
            std::move(boolean_parameters),
            boolean_operation->CreatedSurfaceIndices);

        ActiveParametricObject replay;
        replay.object_index = active_object.object_index;
        if (rebuild_solid_operation_tree(*this, replay, document)) {
            document.UpdateAttachedSketches();
        }
        return;
    }

    if (rebuild_solid_operation_tree(*this, active_object, document)) {
        document.UpdateAttachedSketches();
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
        document.UpdateAttachedSketches();
    }
}

bool ToolRegistry::ReplayOperations(size_t object_index, CAlfaDoc& document) const {
    ActiveParametricObject active_object;
    active_object.object_index = object_index;
    const bool rebuilt =
        rebuild_solid_operation_tree(*this, active_object, document);
    if (rebuilt) {
        document.UpdateAttachedSketches();
    }
    return rebuilt;
}

bool ToolRegistry::ReplayProfileDependents(unsigned long profile_id, CAlfaDoc& document) const {
    if (profile_id == 0) {
        return false;
    }

    bool rebuilt_any = false;
    auto& objects = document.GetObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto* solid = dynamic_cast<const CSolid*>(objects[i].get());
        if (!solid || solid->GetNumOperations() <= 0) {
            continue;
        }

        bool uses_profile = false;
        for (const ParametricFunction* operation : solid->GetOperationTree()) {
            if (!operation) {
                continue;
            }
            for (const ParametricParameterValue& parameter : operation->Parameters) {
                if ((parameter.id == "profile.id"
                     || parameter.id == "section.id"
                     || parameter.id == "guide.id")
                    && static_cast<unsigned long>(std::max(0.0, parameter.value)) == profile_id) {
                    uses_profile = true;
                    break;
                }
            }
            if (uses_profile) {
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
        bool uses_profile = false;
        if (solid) {
            for (const ParametricFunction* operation : solid->GetOperationTree()) {
                if (!operation) {
                    continue;
                }
                uses_profile = std::any_of(
                    operation->Parameters.begin(),
                    operation->Parameters.end(),
                    [](const ParametricParameterValue& parameter) {
                        return parameter.id == "profile.id"
                            || parameter.id == "section.id"
                            || parameter.id == "guide.id";
                    });
                if (uses_profile) {
                    break;
                }
            }
        }
        if (uses_profile && ReplayOperations(i, document)) {
            rebuilt_any = true;
        }
    }
    return rebuilt_any;
}

bool ToolRegistry::ReplayAllTrimDependents(
    CAlfaDoc& document,
    unsigned long cutter_id) const {
    bool rebuilt_any = false;
    auto& objects = document.GetObjects();
    for (size_t object_index = 0;
         object_index < objects.size();
         ++object_index) {
        const auto* solid = dynamic_cast<const CSolid*>(
            objects[object_index].get());
        if (!solid || dynamic_cast<const CSurfaceSet*>(solid)) {
            continue;
        }

        bool depends = false;
        for (const ParametricFunction* operation :
             solid->GetOperationTree()) {
            if (!operation
                || (operation->ToolId != "TrimByPlane"
                    && operation->ToolId != "TrimBySketch"
                    && operation->ToolId != "TrimBySurface")) {
                continue;
            }
            for (const ParametricParameterValue& parameter :
                 operation->Parameters) {
                if (parameter.id == "cutter.id"
                    && (cutter_id == 0
                        || static_cast<unsigned long>(
                               std::max(0.0, parameter.value))
                            == cutter_id)) {
                    depends = true;
                    break;
                }
            }
            if (depends) {
                break;
            }
        }
        if (!depends) {
            continue;
        }

        ActiveParametricObject replay;
        replay.object_index = object_index;
        rebuilt_any =
            rebuild_solid_operation_tree(*this, replay, document)
            || rebuilt_any;
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
            if (tool_id == "boolean") {
                int boolean_tool_index = 0;
                for (const ParametricParameterValue& parameter : saved_parameters) {
                    if (parameter.id == "tool") {
                        boolean_tool_index = std::max(0, static_cast<int>(parameter.value));
                        break;
                    }
                }
                const CSolid* boolean_tool =
                    solid->GetBooleanTool(static_cast<size_t>(boolean_tool_index));
                const ParametricFunction* box_operation =
                    boolean_tool ? boolean_tool->GetOperation(0) : nullptr;
                if (box_operation && box_operation->ToolId == "SolidBox") {
                    const ToolDefinition* box_definition = Find("SolidBox");
                    if (!box_definition) {
                        return {};
                    }
                    std::vector<ToolParameter> parameters =
                        merge_saved_parameters(
                            box_definition->defaults,
                            box_operation->Parameters);
                    for (const ParametricParameterValue& saved :
                         box_operation->Parameters) {
                        if (saved.id == "boolean.body_id") {
                            parameters.push_back({
                                saved.id,
                                "Boolean Body",
                                saved.value,
                                0.0,
                                4294967295.0,
                                1.0});
                        }
                    }
                    parameters.push_back({
                        "boolean.tool_index",
                        "Boolean Tool",
                        static_cast<double>(boolean_tool_index),
                        0.0,
                        1000000.0,
                        1.0});
                    return {
                        "SolidBox",
                        object_index,
                        operation_index,
                        std::move(parameters)};
                }
            }
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
