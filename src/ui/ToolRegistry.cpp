#include "ToolRegistry.h"
#include "../ExtrudeShapeBuilder.h"

#include "../CMesh3D.h"
#include "../CAssembled.h"
#include "../CFacadeFurniture.h"
#include "../CFurnitureDrawer.h"
#include "../CFurnitureAssemblies.h"
#include "../CKitchenCabinet.h"
#include "../FurnitureMaterialFactory.h"
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
#include "../solid/TwoSketchSolidBuilder.h"
#include "../solid/PlaneShapeBuilder.h"
#include "../solid/PolyhedronShapeBuilder.h"
#include "../solid/SketchFeatureShapeBuilder.h"
#include "../solid/OffsetFaceShapeBuilder.h"
#include "../solid/SheetBendShapeBuilder.h"
#include "../solid/SurfaceSet.h"
#include "../solid/TrimShapeBuilder.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
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
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepGProp.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepTools.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>

#include <QApplication>
#include <QEventLoop>
#include <QWidget>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>

namespace {
void process_progressive_furniture_creation() {
    for (QWidget* widget : QApplication::allWidgets()) {
        if (widget && widget->inherits("OpenGLViewport")) {
            widget->update();
            // Lazy furniture meshes are built from Render3d().  repaint()
            // guarantees that this batch is actually rendered before the
            // generator continues instead of waiting for a later scene update.
            widget->repaint();
        }
    }
    QCoreApplication::processEvents(
        QEventLoop::ExcludeUserInputEvents, 3);
}

bool is_cabinet_tool(const std::string& id) {
    return id == "cabinet"
        || id == "cabinet_advanced"
        || id == "cabinet_advanced_slx"
        || id == "cabinet_showcase";
}

bool is_furniture_assembly_tool(const std::string& id) {
    return id == "chair" || id == "chair_simple" || id == "table"
        || is_cabinet_tool(id) || id == "desk" || id == "drawer_box"
        || id == "single_drawer" || id == "single_facade"
        || id == "kitchen_nika_260" || id == "kitchen_corner";
}

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
    plane->SetColor({0.42f, 0.56f, 0.90f});
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

double cabinet_mounting_height(
    const std::vector<ToolParameter>& parameters) {
    return param(parameters, "overhead", 0.0) >= 0.5
        ? std::max(0.0, param(parameters, "mounting_height", 1300.0))
        : 0.0;
}

bool is_planar_surface(const CSurfaceSet& surface) {
    const TopoDS_Face face = first_face(surface.m_Shape);
    if (face.IsNull()) {
        return false;
    }
    try {
        return BRepAdaptor_Surface(face, true).GetType() == GeomAbs_Plane;
    } catch (...) {
        return false;
    }
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
            || !is_planar_surface(*plane)
            || !TrimSolidByFace(solid.m_Shape, face, positive, result)) {
            return false;
        }
    } else if (tool_id == "TrimBySurface") {
        const auto* surface = dynamic_cast<const CSurfaceSet*>(cutter);
        const TopoDS_Face face =
            surface ? first_face(surface->m_Shape) : TopoDS_Face();
        if (!surface
            || is_planar_surface(*surface)
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

const char* default_material_name(const std::string& parameter_id) {
    if (parameter_id.find("facade") != std::string::npos) {
        return "Facade wood";
    }
    if (parameter_id.find("hardware") != std::string::npos
        || parameter_id.find("leg_material") != std::string::npos) {
        return "Steel";
    }
    return "Wood";
}

std::vector<ToolParameter> prepare_material_parameters(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& source) {
    std::vector<ToolParameter> parameters = source;
    const bool has_material_parameter = std::any_of(
        parameters.begin(), parameters.end(), [](const ToolParameter& parameter) {
            return parameter.type == ToolParameterType::Material;
        });
    if (!has_material_parameter) {
        return parameters;
    }

    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    for (ToolParameter& parameter : parameters) {
        if (parameter.type != ToolParameterType::Material) {
            continue;
        }
        parameter.options.clear();
        parameter.option_values.clear();
        for (const Material& material : document.GetMaterials()) {
            parameter.options.push_back(material.name);
            parameter.option_values.push_back(static_cast<double>(material.id));
        }

        const unsigned long selected_id = static_cast<unsigned long>(parameter.value);
        if (!document.FindMaterial(selected_id)) {
            // Both kitchens request this exact worktop texture.  Keep the
            // independent table "Top Material" default unchanged.
            const char* preferred_name =
                parameter.id == "top_material_id"
                    && parameter.label == "Worktop Material"
                ? "Concrete"  // texture/surfaces/Finishes.Painting.Paint.White.Flaking.jpg
                : default_material_name(parameter.id);
            const Material* preferred =
                document.FindMaterial(preferred_name, true);
            if (!preferred && !document.GetMaterials().empty()) {
                preferred = &document.GetMaterials().front();
            }
            parameter.value = preferred ? static_cast<double>(preferred->id) : 0.0;
        }
    }
    return parameters;
}

const Material* material_parameter(
    const CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters,
    const char* id) {
    const unsigned long material_id =
        static_cast<unsigned long>(param(parameters, id, 0.0));
    return document.FindMaterial(material_id);
}

void assign_material(CAlfaObject& object, const Material* material) {
    if (!material) {
        return;
    }
    object.SetMaterial(*material);
    object.SetMaterialId(material->id);
}

void fit_texture_to_broad_faces(CAlfaObject& object) {
    auto* solid = dynamic_cast<CSolid*>(&object);
    if (!solid || solid->GetNumSurfaces() <= 0) {
        return;
    }

    std::vector<double> areas;
    areas.reserve(static_cast<size_t>(solid->GetNumSurfaces()));
    double maximum_area = 0.0;
    for (TopExp_Explorer explorer(solid->m_Shape, TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(TopoDS::Face(explorer.Current()), properties);
        const double area = std::max(0.0, properties.Mass());
        areas.push_back(area);
        maximum_area = std::max(maximum_area, area);
    }
    if (maximum_area <= 1.0e-9) {
        return;
    }

    constexpr double broad_face_tolerance = 0.995;
    const size_t surface_count = std::min(
        areas.size(), static_cast<size_t>(solid->GetNumSurfaces()));
    for (size_t index = 0; index < surface_count; ++index) {
        if (areas[index] < maximum_area * broad_face_tolerance) {
            continue;
        }
        const CSurfaceFace* surface = solid->GetSurfaceFace(static_cast<int>(index));
        SurfaceTextureTransform transform =
            surface ? surface->TextureTransform : SurfaceTextureTransform{};
        transform.fit_to_surface = true;
        solid->SetSurfaceTextureTransform(static_cast<int>(index), transform);
    }
}

void fit_texture_to_all_faces(CAlfaObject& object) {
    auto* solid = dynamic_cast<CSolid*>(&object);
    if (!solid) {
        return;
    }
    for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
        const CSurfaceFace* surface = solid->GetSurfaceFace(index);
        SurfaceTextureTransform transform =
            surface ? surface->TextureTransform : SurfaceTextureTransform{};
        transform.fit_to_surface = true;
        solid->SetSurfaceTextureTransform(index, transform);
    }
}

void assign_furniture_materials(
    CAlfaDoc& document,
    std::vector<std::unique_ptr<CAlfaObject>>& parts,
    const std::vector<ToolParameter>& parameters,
    const std::string& tool_id) {
    const Material* body = material_parameter(
        document, parameters,
        tool_id == "table" ? "base_material_id" : "body_material_id");
    const Material* facade = material_parameter(
        document, parameters, "facade_material_id");
    const Material* hardware = material_parameter(
        document, parameters, "hardware_material_id");
    const Material* top = material_parameter(
        document, parameters, "top_material_id");
    const bool is_kitchen = tool_id == "kitchen_nika_260"
        || tool_id == "kitchen_corner";

    for (const auto& part : parts) {
        if (!part) {
            continue;
        }
        const std::string& name = part->GetName();
        const bool is_facade = name.find("Facade") != std::string::npos;
        const bool is_round_handle_face =
            name.find("Facade Handle Face") != std::string::npos;
        const bool is_hardware = (name.find("Handle") != std::string::npos
                                  && !is_round_handle_face)
            || name.find("Guide") != std::string::npos
            || (name.find("Leg") != std::string::npos
                && (tool_id == "drawer_box" || is_kitchen));
        const bool is_top = (tool_id == "table" && name == "Table Top")
            || (is_kitchen && name.find("Worktop") != std::string::npos);
        const Material* selected_material =
            is_top ? top
            : is_hardware ? hardware
            : is_facade ? facade
            : body;
        assign_material(*part, selected_material);
        if (name.find("Showcase Glass") != std::string::npos) {
            FurnitureMaterialFactory::SetMaterial(
                document, part.get(), "Glass");
        } else if (name.find("Stained Glass Wire") != std::string::npos) {
            FurnitureMaterialFactory::SetMaterial(
                document, part.get(), "Gold");
        }
        const bool sheet_furniture_part = tool_id != "table"
            && tool_id != "chair" && !is_hardware;
        if (sheet_furniture_part
            && selected_material
            && !selected_material->color_texture_path.empty()) {
            if (is_kitchen && name.find("Worktop") != std::string::npos) {
                fit_texture_to_all_faces(*part);
            } else {
                fit_texture_to_broad_faces(*part);
            }
        }
    }
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
    const Vec3 sketch_origin{
        static_cast<float>(system.origin.x),
        static_cast<float>(system.origin.y),
        static_cast<float>(system.origin.z)};
    const Vec3 sketch_normal = normalize(Vec3{
        static_cast<float>(system.normal.x),
        static_cast<float>(system.normal.y),
        static_cast<float>(system.normal.z)});
    Vec3 outward_normal = sketch_normal;
    const int saved_face_index = static_cast<int>(
        param(parameters, "face.index", -1.0));
    Vec3 face_center{};
    Vec3 face_normal{};
    bool resolved_host_face = false;
    if (saved_face_index >= 0
        && solid.GetFaceCenterAndNormal(
            saved_face_index, face_center, face_normal)) {
        face_normal = normalize(face_normal);
        // A sketch coordinate system describes its 2D parametrization; its
        // normal is not guaranteed to be the outward normal of the host face.
        // Boss/Pocket direction, however, is defined by that oriented face.
        // Only trust the saved face when it is still coplanar with the sketch;
        // a later boolean can legitimately change face numbering.
        const double plane_distance = std::fabs(static_cast<double>(
            dot(face_center - sketch_origin, face_normal)));
        if (dot(face_normal, face_normal) > 1.0e-12f
            && plane_distance <= 1.0e-3) {
            outward_normal = face_normal;
            resolved_host_face = true;
        }
    }
    if (!resolved_host_face) {
        double best_plane_distance = std::numeric_limits<double>::max();
        for (int face_index = 0;
             face_index < solid.GetNumSurfaces(); ++face_index) {
            const TopoDS_Face face = solid.GetTopoFace(face_index);
            if (face.IsNull())
                continue;
            try {
                if (BRepAdaptor_Surface(face, true).GetType() != GeomAbs_Plane)
                    continue;
            } catch (const Standard_Failure&) {
                continue;
            }
            Vec3 candidate_center{};
            Vec3 candidate_normal{};
            if (!solid.GetFaceCenterAndNormal(
                    face_index, candidate_center, candidate_normal)) {
                continue;
            }
            candidate_normal = normalize(candidate_normal);
            if (std::fabs(dot(candidate_normal, sketch_normal)) < 0.999f)
                continue;
            const double plane_distance = std::fabs(static_cast<double>(
                dot(candidate_center - sketch_origin, candidate_normal)));
            if (plane_distance < best_plane_distance) {
                best_plane_distance = plane_distance;
                outward_normal = candidate_normal;
            }
        }
    }
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
                            double end_radius = -1.0,
                            std::vector<TopoDS_Face>* created_faces = nullptr,
                            std::vector<std::vector<TopoDS_Face>>* tracked_operations = nullptr,
                            const std::vector<double>& radius_points = {}) {
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
        const double final_radius = end_radius > 0.0001 ? end_radius : radius;
        const std::vector<double> law = radius_points.empty()
            ? std::vector<double>{radius, final_radius} : radius_points;
        for (const TopoDS_Edge& edge : edges) {
            const bool constant = std::all_of(
                law.begin() + 1, law.end(),
                [&law](double value) {
                    return std::abs(value - law.front()) <= 1.0e-9;
                });
            if (constant) {
                fillet.Add(law.front(), edge);
            } else {
                TColgp_Array1OfPnt2d radius_law(
                    1, static_cast<Standard_Integer>(law.size()));
                for (size_t index = 0; index < law.size(); ++index) {
                    radius_law.SetValue(
                        static_cast<Standard_Integer>(index + 1),
                        gp_Pnt2d(
                            static_cast<double>(index)
                                / static_cast<double>(law.size() - 1),
                            law[index]));
                }
                fillet.Add(radius_law, edge);
            }
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
                        double end_radius = -1.0,
                        std::vector<TopoDS_Face>* created_faces = nullptr,
                        std::vector<std::vector<TopoDS_Face>>* tracked_operations = nullptr,
                        const std::vector<double>& radius_points = {}) {
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
        const double final_radius = end_radius > 0.0001 ? end_radius : radius;
        const std::vector<double> law = radius_points.empty()
            ? std::vector<double>{radius, final_radius} : radius_points;
        for (const TopoDS_Edge& edge : edges) {
            const bool constant = std::all_of(
                law.begin() + 1, law.end(),
                [&law](double value) {
                    return std::abs(value - law.front()) <= 1.0e-9;
                });
            if (constant) {
                fillet.Add(law.front(), edge);
            } else {
                TColgp_Array1OfPnt2d radius_law(
                    1, static_cast<Standard_Integer>(law.size()));
                for (size_t index = 0; index < law.size(); ++index) {
                    radius_law.SetValue(
                        static_cast<Standard_Integer>(index + 1),
                        gp_Pnt2d(
                            static_cast<double>(index)
                                / static_cast<double>(law.size() - 1),
                            law[index]));
                }
                fillet.Add(radius_law, edge);
            }
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

Vec3 hole_center_from_references(const std::vector<ToolParameter>& parameters) {
    const Vec3 saved_center{
        static_cast<float>(param(parameters, "hole.center.x", 0.0)),
        static_cast<float>(param(parameters, "hole.center.y", 0.0)),
        static_cast<float>(param(parameters, "hole.center.z", 0.0))};
    if (param(parameters, "hole.refs.valid", 0.0) < 0.5)
        return saved_center;

    Vec3 face_normal{
        static_cast<float>(param(parameters, "hole.normal.x", 0.0)),
        static_cast<float>(param(parameters, "hole.normal.y", 0.0)),
        static_cast<float>(param(parameters, "hole.normal.z", 1.0))};
    face_normal = normalize(face_normal);
    std::array<Vec3, 2> starts{};
    std::array<Vec3, 2> ends{};
    std::array<double, 2> sides{};
    std::array<double, 2> distances{};
    for (int index = 0; index < 2; ++index) {
        const std::string prefix = "hole.edge" + std::to_string(index + 1);
        starts[static_cast<size_t>(index)] = {
            static_cast<float>(param(parameters, (prefix + ".start.x").c_str(), 0.0)),
            static_cast<float>(param(parameters, (prefix + ".start.y").c_str(), 0.0)),
            static_cast<float>(param(parameters, (prefix + ".start.z").c_str(), 0.0))};
        ends[static_cast<size_t>(index)] = {
            static_cast<float>(param(parameters, (prefix + ".end.x").c_str(), 0.0)),
            static_cast<float>(param(parameters, (prefix + ".end.y").c_str(), 0.0)),
            static_cast<float>(param(parameters, (prefix + ".end.z").c_str(), 0.0))};
        sides[static_cast<size_t>(index)] =
            param(parameters, (prefix + ".side").c_str(), 1.0) < 0.0 ? -1.0 : 1.0;
        distances[static_cast<size_t>(index)] = param(
            parameters, index == 0 ? "hole.distance1" : "hole.distance2", 0.0);
    }

    const Vec3 direction1 = normalize(ends[0] - starts[0]);
    const Vec3 direction2 = normalize(ends[1] - starts[1]);
    const Vec3 normal1 = normalize(cross(face_normal, direction1));
    const Vec3 normal2 = normalize(cross(face_normal, direction2));
    const double determinant = dot(normal1, cross(normal2, face_normal));
    if (std::fabs(determinant) <= 1.0e-8)
        return saved_center;

    const double rhs1 = dot(normal1, starts[0]) + sides[0] * distances[0];
    const double rhs2 = dot(normal2, starts[1]) + sides[1] * distances[1];
    const double rhs3 = dot(face_normal, saved_center);
    const Vec3 result = (cross(normal2, face_normal) * static_cast<float>(rhs1)
        + cross(face_normal, normal1) * static_cast<float>(rhs2)
        + cross(normal1, normal2) * static_cast<float>(rhs3))
        * static_cast<float>(1.0 / determinant);
    return result;
}

bool apply_hole(CSolid& solid, const std::vector<ToolParameter>& parameters) {
    if (solid.m_Shape.IsNull())
        return false;
    const double diameter = param(parameters, "diameter", 10.0);
    const int hole_type = static_cast<int>(param(parameters, "hole_type", 0.0));
    const double requested_depth = param(parameters, "depth", 10.0);
    const Vec3 center = hole_center_from_references(parameters);
    Vec3 outward{
        static_cast<float>(param(parameters, "hole.normal.x", 0.0)),
        static_cast<float>(param(parameters, "hole.normal.y", 0.0)),
        static_cast<float>(param(parameters, "hole.normal.z", 1.0))};
    outward = normalize(outward);
    if (!std::isfinite(diameter) || diameter <= 0.000001
        || dot(outward, outward) <= 0.000001f
        || (hole_type != 0
            && (!std::isfinite(requested_depth) || requested_depth <= 0.000001))) {
        return false;
    }

    double shape_diagonal = std::max(diameter, 1.0);
    Bnd_Box bounds;
    BRepBndLib::Add(solid.m_Shape, bounds);
    if (!bounds.IsVoid()) {
        Standard_Real xmin = 0.0;
        Standard_Real ymin = 0.0;
        Standard_Real zmin = 0.0;
        Standard_Real xmax = 0.0;
        Standard_Real ymax = 0.0;
        Standard_Real zmax = 0.0;
        bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        shape_diagonal = std::max(
            shape_diagonal,
            std::sqrt((xmax - xmin) * (xmax - xmin)
                + (ymax - ymin) * (ymax - ymin)
                + (zmax - zmin) * (zmax - zmin)));
    }
    const double outside_extension = std::max(
        {diameter, shape_diagonal * 0.05, 0.01});
    const double cut_depth = hole_type == 0
        ? shape_diagonal + outside_extension * 2.0
        : requested_depth + outside_extension;
    const Vec3 start = center + outward * static_cast<float>(outside_extension);
    const Vec3 inward = outward * -1.0f;

    try {
        const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(start.x, start.y, start.z),
                   gp_Dir(inward.x, inward.y, inward.z)),
            diameter * 0.5,
            cut_depth).Shape();
        BRepAlgoAPI_Cut cut(solid.m_Shape, cylinder);
        cut.Build();
        if (!cut.IsDone() || cut.Shape().IsNull())
            return false;
        solid.m_Shape = cut.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    return solid.ReBuldMesh();
}

bool apply_sheet_bend(CSolid& solid,
                      const std::vector<ParametricParameterValue>& saved_parameters,
                      const std::vector<ToolParameter>& parameters) {
    SheetBendParameters bend;
    bend.line_start_x = saved_param(saved_parameters, "point1.x", 0.0);
    bend.line_start_y = saved_param(saved_parameters, "point1.y", 0.0);
    bend.line_start_z = saved_param(saved_parameters, "point1.z", 0.0);
    bend.line_end_x = saved_param(saved_parameters, "point2.x", 0.0);
    bend.line_end_y = saved_param(saved_parameters, "point2.y", 0.0);
    bend.line_end_z = saved_param(saved_parameters, "point2.z", 0.0);
    bend.inner_radius = param(parameters, "radius", 2.0);
    bend.angle_degrees = param(parameters, "angle", 90.0);
    bend.clockwise = param(parameters, "direction", 0.0) < 0.5;

    TopoDS_Shape result;
    std::string error_message;
    if (!BuildSheetBendShape(solid.m_Shape, bend, result, error_message)) {
        return false;
    }
    solid.m_Shape = result;
    solid.ClearSelectedEdge();
    solid.ClearSelectedFace();
    return solid.ReBuldMesh();
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

std::vector<double> fillet_radius_points(
    const std::vector<ToolParameter>& parameters) {
    (void)parameters;
    return {};
}

bool rebuild_two_sketch_base(CAlfaDoc& document,
                             size_t object_index,
                             const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) {
        return false;
    }

    const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long first_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "profile.id", 0.0)));
    const unsigned long second_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "section.id", 0.0)));
    const auto* first = dynamic_cast<const CSmartLine*>(
        document.FindObjectById(first_id));
    const auto* second = dynamic_cast<const CSmartLine*>(
        document.FindObjectById(second_id));
    if (!old_solid || !first || !second) {
        return false;
    }

    TopoDS_Shape shape;
    if (!BuildSolidBetweenSketches(*first, *second, shape) || shape.IsNull()) {
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
    std::vector<double> width_scales;
    std::vector<double> height_scales;
    width_scales.reserve(5);
    height_scales.reserve(5);
    for (int point = 0; point < 5; ++point) {
        const std::string width_id = "width.scale." + std::to_string(point);
        const std::string height_id = "height.scale." + std::to_string(point);
        width_scales.push_back(param(parameters, width_id.c_str(), 1.0));
        height_scales.push_back(param(parameters, height_id.c_str(), 1.0));
    }
    if (!old_solid || !section || !guide) {
        return false;
    }

    TopoDS_Shape shape = BuildSweptSolidShape(
        *section, *guide, transition_mode, delta_x, delta_y, angle_degrees,
        width_scales, height_scales);
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

    // A sketch is attached to one host face and defines one Boss/Pocket
    // feature. Older builds appended a new operation every time the tool was
    // reopened. Two cuts with the same profile then overlap, so changing the
    // depth of the last one appears to do nothing. Keep the latest operation
    // for each profile when replaying legacy project histories.
    std::set<unsigned long> latest_sketch_features;
    for (size_t index = operations.size(); index-- > 1;) {
        StoredOperation& operation = operations[index];
        if (operation.tool_id != "SolidSketchFeature")
            continue;
        const unsigned long profile_id = static_cast<unsigned long>(
            std::max(0.0, saved_param(
                operation.saved_parameters, "profile.id", 0.0)));
        if (profile_id == 0)
            continue;
        if (!latest_sketch_features.insert(profile_id).second)
            operations.erase(operations.begin() + index);
    }

    const StoredOperation& base_operation = operations.front();
    const ToolDefinition* base_tool = registry.Find(base_operation.tool_id);
    if (!base_tool || !base_tool->rebuild) {
        return false;
    }

    if (base_operation.tool_id == "SolidSheetBend") {
        const int base_tool_index = static_cast<int>(saved_param(
            base_operation.saved_parameters, "base.tool.index", -1.0));
        const CSolid* frozen_base = base_tool_index >= 0
            ? solid->GetBooleanTool(static_cast<size_t>(base_tool_index))
            : nullptr;
        if (!frozen_base || frozen_base->m_Shape.IsNull()) {
            return false;
        }
        solid->m_Shape = frozen_base->m_Shape;
        if (!apply_sheet_bend(
                *solid,
                base_operation.saved_parameters,
                parameters_for_operation(registry, base_operation))) {
            return false;
        }
    } else if (base_operation.tool_id == "SolidExtrudeTool") {
        if (!rebuild_extrude_base(document,
                                  active_object.object_index,
                                  parameters_for_operation(registry, base_operation))) {
            return false;
        }
    } else if (base_operation.tool_id == "SolidTwoSketches") {
        if (!rebuild_two_sketch_base(
                document,
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
        if (operation.tool_id == "SurfaceFilmCoating") {
            for (int face_index : operation.created_surface_indices) {
                if (face_index >= 0
                    && face_index < static_cast<int>(faces_before.size())) {
                    operation_created_faces[i].push_back(
                        faces_before[static_cast<size_t>(face_index)]);
                }
            }
            history_tracked = true;
        } else if (operation.tool_id == "fillet_all_edges") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            const double radius_type = std::clamp(
                param(parameters, "radius_type", 0.0), 0.0, 1.0);
            const double start_radius = radius_type >= 0.5
                ? param(parameters, "radius_start", param(parameters, "radius", 1.0))
                : param(parameters, "radius", 1.0);
            const double end_radius = radius_type >= 0.5
                ? param(parameters, "radius_end", start_radius) : start_radius;
            const std::vector<double> radius_points =
                fillet_radius_points(parameters);
            if (!apply_fillet_all_edges(*solid,
                                        start_radius, end_radius,
                                        &operation_created_faces[i],
                                        &operation_created_faces,
                                        radius_points)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "fillet_edge") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            const double radius_type = std::clamp(
                param(parameters, "radius_type", 0.0), 0.0, 1.0);
            const double start_radius = radius_type >= 0.5
                ? param(parameters, "radius_start", param(parameters, "radius", 1.0))
                : param(parameters, "radius", 1.0);
            const double end_radius = radius_type >= 0.5
                ? param(parameters, "radius_end", start_radius) : start_radius;
            const std::vector<double> radius_points =
                fillet_radius_points(parameters);
            if (!apply_fillet_edges(*solid,
                                    edge_refs_from_saved_parameters(operation.saved_parameters),
                                    start_radius, end_radius,
                                    &operation_created_faces[i],
                                    &operation_created_faces,
                                    radius_points)) {
                return false;
            }
            history_tracked = true;
        } else if (operation.tool_id == "ChamferSolid") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_chamfer_edges(*solid,
                                     edge_refs_from_saved_parameters(operation.saved_parameters),
                                     param(parameters, "distance", 1.0),
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
        } else if (operation.tool_id == "SolidHole") {
            const std::vector<ToolParameter> parameters =
                parameters_for_operation(registry, operation);
            if (!apply_hole(*solid, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "SolidDraft") {
            const std::vector<ToolParameter> parameters = parameters_for_operation(registry, operation);
            if (!apply_draft_face(*solid, operation.saved_parameters, parameters)) {
                return false;
            }
        } else if (operation.tool_id == "SolidSheetBend") {
            const std::vector<ToolParameter> parameters =
                parameters_for_operation(registry, operation);
            if (!apply_sheet_bend(
                    *solid, operation.saved_parameters, parameters)) {
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
        const std::vector<int> final_surface_indices =
            face_indices_in_shape(operation_created_faces[i], final_faces);
        solid->SetParametricOperation(solid->GetOperationTree().size(),
                                      operation.tool_id,
                                      operation_label,
                                      operation.saved_parameters,
                                      final_surface_indices);
        if (operation.tool_id == "SurfaceFilmCoating") {
            unsigned long material_id = 0;
            for (const ParametricParameterValue& parameter : operation.saved_parameters) {
                if (parameter.id == "material.id") {
                    material_id = static_cast<unsigned long>(
                        std::max(0.0, parameter.value));
                    break;
                }
            }
            if (const Material* film = document.FindMaterial(material_id)) {
                for (int surface_index : final_surface_indices) {
                    solid->SetSurfaceCoating(surface_index, *film);
                }
            }
        }
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

void copy_solid_surface_appearance(const CAlfaObject& source, CAlfaObject& target) {
    const auto* source_solid = dynamic_cast<const CSolid*>(&source);
    auto* target_solid = dynamic_cast<CSolid*>(&target);
    if (!source_solid || !target_solid) {
        return;
    }
    const int count = std::min(source_solid->GetNumSurfaces(), target_solid->GetNumSurfaces());
    for (int index = 0; index < count; ++index) {
        const CSurfaceFace* source_surface = source_solid->GetSurfaceFace(index);
        if (!source_surface) {
            continue;
        }
        SurfaceTextureTransform transform = source_surface->TextureTransform;
        const CSurfaceFace* target_surface = target_solid->GetSurfaceFace(index);
        if (target_surface) {
            transform.fit_to_surface = transform.fit_to_surface
                || target_surface->TextureTransform.fit_to_surface;
        }
        target_solid->SetSurfaceTextureTransform(index, transform);
        if (source_surface->MaterialOverride.enabled) {
            target_solid->SetSurfaceMaterial(index, source_surface->MaterialOverride.material);
        }
        if (source_surface->MaterialOverride.coating_enabled) {
            target_solid->SetSurfaceCoating(index, source_surface->MaterialOverride.coating_material);
        }
    }
}

void replace_selected_mesh(CAlfaDoc& document, size_t index, std::unique_ptr<CMesh3D> mesh) {
    auto& objects = document.GetObjects();
    if (index >= objects.size() || !mesh) {
        return;
    }

    if (objects[index]) {
        mesh->m_id = objects[index]->m_id;
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

std::vector<CSmartLine> place_milling_pattern_on_facade(
        const std::vector<const CSmartLine*>& sources,
        const TopoDS_Shape& facade) {
    if (sources.empty()) {
        return {};
    }
    const SketchCoordinateSystem& reference =
        sources.front()->GetCoordinateSystem();
    double local_min_x = std::numeric_limits<double>::max();
    double local_max_x = std::numeric_limits<double>::lowest();
    double local_min_y = std::numeric_limits<double>::max();
    double local_max_y = std::numeric_limits<double>::lowest();
    for (const CSmartLine* source : sources) {
        for (const CPoint3d& world : source->GetProfilePointsWorld()) {
            const CPoint3d local = sources.front()->WorldToLocal(world);
            local_min_x = std::min(local_min_x, local.x);
            local_max_x = std::max(local_max_x, local.x);
            local_min_y = std::min(local_min_y, local.y);
            local_max_y = std::max(local_max_y, local.y);
        }
    }
    if (local_min_x == std::numeric_limits<double>::max()) {
        return {};
    }

    Bnd_Box bounds;
    BRepBndLib::Add(facade, bounds);
    if (bounds.IsVoid()) {
        return {};
    }
    Standard_Real min_x = 0.0;
    Standard_Real min_y = 0.0;
    Standard_Real min_z = 0.0;
    Standard_Real max_x = 0.0;
    Standard_Real max_y = 0.0;
    Standard_Real max_z = 0.0;
    bounds.Get(min_x, min_y, min_z, max_x, max_y, max_z);

    // Catalog guides are authored in local XY. Scale the whole composition
    // uniformly to the usable facade rectangle, preserving its proportions
    // and all distances between individual guides.
    const double pattern_width = local_max_x - local_min_x;
    const double pattern_height = local_max_y - local_min_y;
    const double facade_width = max_x - min_x;
    const double facade_height = max_z - min_z;
    const double minimum_side = std::min(facade_width, facade_height);
    const double inset = std::clamp(minimum_side * 0.12, 30.0, 70.0);
    const double available_width = std::max(1.0, facade_width - 2.0 * inset);
    const double available_height = std::max(1.0, facade_height - 2.0 * inset);
    const double scale_x = pattern_width > 1.0e-9
        ? available_width / pattern_width : 1.0;
    const double scale_y = pattern_height > 1.0e-9
        ? available_height / pattern_height : 1.0;
    const double pattern_scale_x = std::max(1.0e-6, scale_x);
    const double pattern_scale_y = std::max(1.0e-6, scale_y);

    // Put the guide just outside the visible (-Y) face. The 0.1 mm overlap
    // avoids a coincident-face boolean when the cutter profile starts at Y=0.
    const CPoint3d target_reference_origin{
        (min_x + max_x
            - pattern_scale_x * (local_min_x + local_max_x)) * 0.5,
        min_y - 0.1,
        (min_z + max_z
            - pattern_scale_y * (local_min_y + local_max_y)) * 0.5};

    const auto dot = [](const CPoint3d& left, const CPoint3d& right) {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    };
    std::vector<CSmartLine> result;
    result.reserve(sources.size());
    for (const CSmartLine* source : sources) {
        CSmartLine guide = source->MakeCopy();
        guide.ScaleLocal(pattern_scale_x, pattern_scale_y);
        const SketchCoordinateSystem& system = source->GetCoordinateSystem();
        const CPoint3d source_origin_in_reference =
            sources.front()->WorldToLocal(system.origin);
        const CPoint3d target_origin{
            target_reference_origin.x
                + pattern_scale_x * source_origin_in_reference.x,
            target_reference_origin.y,
            target_reference_origin.z
                + pattern_scale_y * source_origin_in_reference.y};
        CPoint3d target_x_axis{
            dot(system.x_axis, reference.x_axis),
            0.0,
            dot(system.x_axis, reference.y_axis)};
        if (!guide.SetCoordinateSystem(
                target_origin, target_x_axis, CPoint3d{0.0, -1.0, 0.0})) {
            continue;
        }
        result.push_back(std::move(guide));
    }
    return result;
}

void apply_slx_facade_features(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters,
    std::vector<std::unique_ptr<CAlfaObject>>& parts) {
    // Frame consumes only slx.profile.id + slx.panel.id while the cabinet
    // parts are being built.  Running the legacy contour sweep afterwards
    // used the same sketches a second time, twisted them around a closed
    // path and made cabinet creation need tens of seconds.
    const int facade_style = static_cast<int>(
        std::lround(param(parameters, "facade_style", 0.0)));
    if (facade_style != static_cast<int>(KitchenCabinetFacadeStyle::Milled)) {
        return;
    }

    const auto sketch_by_parameter = [&](const char* id) -> const CSmartLine* {
        const unsigned long object_id = static_cast<unsigned long>(
            std::max(0.0, param(parameters, id, 0.0)));
        return dynamic_cast<const CSmartLine*>(document.FindObjectById(object_id));
    };
    const CSmartLine* milling_profile =
        sketch_by_parameter("slx.milling.profile.id");
    if (!milling_profile) {
        // Compatibility with documents created by the first SLX prototype.
        milling_profile = sketch_by_parameter("slx.profile.id");
    }
    if (!milling_profile || !milling_profile->IsClosed()) {
        return;
    }

    std::vector<const CSmartLine*> contours;
    const unsigned long pattern_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "slx.milling.guide.id", 0.0)));
    std::vector<unsigned long> pending_ids{pattern_id};
    std::set<unsigned long> visited_ids;
    while (!pending_ids.empty()) {
        const unsigned long object_id = pending_ids.back();
        pending_ids.pop_back();
        if (object_id == 0 || !visited_ids.insert(object_id).second) {
            continue;
        }
        const CAlfaObject* object = document.FindObjectById(object_id);
        if (const auto* guide = dynamic_cast<const CSmartLine*>(object)) {
            contours.push_back(guide);
        } else if (const auto* group = dynamic_cast<const CGroup*>(object)) {
            pending_ids.insert(
                pending_ids.end(),
                group->GetElementIds().begin(), group->GetElementIds().end());
        }
    }
    if (pattern_id == 0) {
        for (int index = 1; index <= 8; ++index) {
            const std::string id = "slx.contour." + std::to_string(index) + ".id";
            const CSmartLine* contour = sketch_by_parameter(id.c_str());
            if (contour
                && std::find(contours.begin(), contours.end(), contour)
                    == contours.end()) {
                contours.push_back(contour);
            }
        }
    }
    if (contours.empty()) {
        return;
    }

    for (std::unique_ptr<CAlfaObject>& part : parts) {
        auto* solid = dynamic_cast<CSolid*>(part.get());
        if (!solid
            || part->GetName().find("Facade") == std::string::npos
            || part->GetName().find("Handle") != std::string::npos) {
            continue;
        }
        const TopoDS_Shape raw_facade = solid->m_Shape;
        TopoDS_Shape result = raw_facade;
        std::vector<CSmartLine> placed_guides =
            place_milling_pattern_on_facade(contours, result);
        const MillingCutterPlacement cutter_placement =
            ResolveMillingCutterPlacement(*milling_profile);
        const double cutting_depth = std::max(
            0.1, param(parameters, "milling_depth", 9.0));
        const double cutter_delta_y = cutter_placement.delta_y
            - (cutter_placement.tip_at_cutting_depth
                   ? cutting_depth : 0.0);
        std::vector<TopoDS_Shape> cutter_sweeps;
        for (const CSmartLine& placed_guide : placed_guides) {
            // Give OCCT the complete tool path. PipeShell then carries one
            // corrected cutter section around the whole guide and creates the
            // physical RoundCorner envelopes. Splitting the guide here would
            // manufacture four unrelated end caps and lose the router radii.
            TopoDS_Shape cutter = BuildSweptSolidShape(
                *milling_profile, placed_guide, 2,
                0.0, cutter_delta_y,
                cutter_placement.angle_degrees, {}, {}, true);
            if (!cutter.IsNull()) {
                TopoDS_Shape limited_cutter =
                    LimitMillingCutterToFacadeDepth(
                        cutter, raw_facade, cutting_depth);
                if (!limited_cutter.IsNull()) {
                    cutter_sweeps.push_back(std::move(limited_cutter));
                }
            }
        }
        if (!cutter_sweeps.empty()) {
            BRep_Builder builder;
            TopoDS_Compound all_cutters;
            builder.MakeCompound(all_cutters);
            for (const TopoDS_Shape& cutter : cutter_sweeps) {
                builder.Add(all_cutters, cutter);
            }
            BRepAlgoAPI_Cut cut(raw_facade, all_cutters);
            cut.SetFuzzyValue(0.02);
            cut.SetRunParallel(true);
            cut.Build();
            if (cut.IsDone() && !cut.Shape().IsNull()) {
                cut.SimplifyResult(true, true, 1.0e-5);
                result = cut.Shape();
            }
        }
        solid->m_Shape = result;
        solid->ReBuldMesh();
    }
}

std::vector<const CSmartLine*> slx_milling_guides(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters) {
    std::vector<const CSmartLine*> guides;
    const auto sketch_by_parameter = [&](const char* id) -> const CSmartLine* {
        const unsigned long object_id = static_cast<unsigned long>(
            std::max(0.0, param(parameters, id, 0.0)));
        return dynamic_cast<const CSmartLine*>(document.FindObjectById(object_id));
    };
    const unsigned long pattern_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "slx.milling.guide.id", 0.0)));
    std::vector<unsigned long> pending_ids{pattern_id};
    std::set<unsigned long> visited_ids;
    while (!pending_ids.empty()) {
        const unsigned long object_id = pending_ids.back();
        pending_ids.pop_back();
        if (object_id == 0 || !visited_ids.insert(object_id).second) {
            continue;
        }
        const CAlfaObject* object = document.FindObjectById(object_id);
        if (const auto* guide = dynamic_cast<const CSmartLine*>(object)) {
            guides.push_back(guide);
        } else if (const auto* group = dynamic_cast<const CGroup*>(object)) {
            pending_ids.insert(pending_ids.end(),
                group->GetElementIds().begin(), group->GetElementIds().end());
        }
    }
    if (pattern_id == 0) {
        for (int index = 1; index <= 8; ++index) {
            const std::string id =
                "slx.contour." + std::to_string(index) + ".id";
            const CSmartLine* guide = sketch_by_parameter(id.c_str());
            if (guide
                && std::find(guides.begin(), guides.end(), guide)
                    == guides.end()) {
                guides.push_back(guide);
            }
        }
    }
    return guides;
}

void apply_slx_catalog_handle(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters,
    std::vector<std::unique_ptr<CAlfaObject>>& parts) {
    const int handle_type = static_cast<int>(
        std::lround(param(parameters, "handle_type", 0.0)));
    if (handle_type != 3) {
        return;
    }
    const unsigned long source_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "slx.handle.id", 0.0)));
    const auto* source = dynamic_cast<const CSolid*>(
        document.FindObjectById(source_id));
    Vec3 source_min{};
    Vec3 source_max{};
    if (!source || source->m_Shape.IsNull()
        || !source->GetBounds(source_min, source_max)) {
        return;
    }

    const double source_lengths[] = {
        source_max.x - source_min.x,
        source_max.y - source_min.y,
        source_max.z - source_min.z};
    const int source_long_axis = static_cast<int>(std::distance(
        source_lengths,
        std::max_element(source_lengths, source_lengths + 3)));
    const double source_long = std::max(1.0e-6, source_lengths[source_long_axis]);
    const gp_Pnt source_center(
        (source_min.x + source_max.x) * 0.5,
        (source_min.y + source_max.y) * 0.5,
        (source_min.z + source_max.z) * 0.5);

    for (auto& part : parts) {
        auto* target = dynamic_cast<CSolid*>(part.get());
        if (!target || part->GetName().find("Handle") == std::string::npos) {
            continue;
        }
        Vec3 target_min{};
        Vec3 target_max{};
        if (!target->GetBounds(target_min, target_max)) {
            continue;
        }
        const double target_lengths[] = {
            target_max.x - target_min.x,
            target_max.y - target_min.y,
            target_max.z - target_min.z};
        const int target_long_axis = static_cast<int>(std::distance(
            target_lengths,
            std::max_element(target_lengths, target_lengths + 3)));
        const double target_long = std::max(1.0, target_lengths[target_long_axis]);
        const gp_Pnt target_center(
            (target_min.x + target_max.x) * 0.5,
            (target_min.y + target_max.y) * 0.5,
            (target_min.z + target_max.z) * 0.5);

        TopoDS_Shape placed = source->m_Shape;
        gp_Trsf center;
        center.SetTranslation(gp_Vec(
            -source_center.X(), -source_center.Y(), -source_center.Z()));
        placed = BRepBuilderAPI_Transform(placed, center, true).Shape();

        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), target_long / source_long);
        placed = BRepBuilderAPI_Transform(placed, scale, true).Shape();

        if (source_long_axis != target_long_axis) {
            gp_Dir rotation_axis(0.0, 1.0, 0.0);
            double angle = 0.0;
            if ((source_long_axis == 0 && target_long_axis == 2)
                || (source_long_axis == 2 && target_long_axis == 0)) {
                angle = 3.14159265358979323846 * 0.5;
            } else if ((source_long_axis == 0 && target_long_axis == 1)
                       || (source_long_axis == 1 && target_long_axis == 0)) {
                rotation_axis = gp_Dir(0.0, 0.0, 1.0);
                angle = 3.14159265358979323846 * 0.5;
            } else {
                rotation_axis = gp_Dir(1.0, 0.0, 0.0);
                angle = 3.14159265358979323846 * 0.5;
            }
            gp_Trsf rotation;
            rotation.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), rotation_axis), angle);
            placed = BRepBuilderAPI_Transform(placed, rotation, true).Shape();
        }

        gp_Trsf move;
        move.SetTranslation(gp_Vec(
            target_center.X(), target_center.Y(), target_center.Z()));
        target->Clear();
        target->m_Shape = BRepBuilderAPI_Transform(placed, move, true).Shape();
        target->InitSurfaces();
        target->ReBuldMesh();
        target->SetMaterial(source->GetMaterial());
        target->SetMaterialId(source->GetMaterialId());
    }
}

KitchenCabinetDefinition kitchen_cabinet_definition(
    const std::vector<ToolParameter>& parameters) {
    KitchenCabinetDefinition definition;
    definition.body_type = static_cast<KitchenCabinetBodyType>(std::clamp(
        static_cast<int>(param(parameters, "body_type", 0.0)), 0, 6));
    definition.facade_type = static_cast<KitchenCabinetFacadeType>(std::clamp(
        static_cast<int>(param(parameters, "facade_type", 2.0)), 0, 2));
    definition.facade_style = static_cast<KitchenCabinetFacadeStyle>(std::clamp(
        static_cast<int>(param(parameters, "facade_style", 0.0)), 0, 4));
    definition.showcase_fill = static_cast<KitchenCabinetShowcaseFill>(std::clamp(
        static_cast<int>(param(parameters, "showcase_fill", 4.0)), 0, 4));
    definition.shelf_count = std::clamp(
        static_cast<int>(param(parameters, "shelf_count", 2.0)), 0, 100);
    definition.door_open_angle = std::clamp(
        param(parameters, "door_open_angle", 0.0), 0.0, 180.0);
    definition.left_door_open_angle = std::clamp(
        param(parameters, "left_door_open_angle", definition.door_open_angle),
        0.0, 180.0);
    definition.right_door_open_angle = std::clamp(
        param(parameters, "right_door_open_angle", definition.door_open_angle),
        0.0, 180.0);
    definition.door_hinge_side = std::clamp(
        static_cast<int>(param(parameters, "door_hinge_side", 0.0)), 0, 1);
    definition.handle_orientation = std::clamp(
        static_cast<int>(param(parameters, "handle_orientation", 0.0)), 0, 1);
    definition.width = std::max(1.0, param(parameters, "width", 600.0));
    definition.depth = std::max(1.0, param(parameters, "depth", 560.0));
    definition.height = std::max(1.0, param(parameters, "height", 720.0));
    const bool legacy_meter_dimensions =
        definition.width <= 10.0 && definition.depth <= 10.0 && definition.height <= 10.0;
    if (legacy_meter_dimensions) {
        definition.width *= 1000.0;
        definition.depth *= 1000.0;
        definition.height *= 1000.0;
    }
    double panel_thickness = param(parameters, "panel_thickness", 18.0);
    if (panel_thickness > 0.0 && panel_thickness < 1.0) {
        panel_thickness *= 1000.0;
    }
    definition.panel_thickness = std::clamp(
        panel_thickness, 0.1,
        std::min({definition.width, definition.depth, definition.height}) * 0.49);
    definition.milling_depth = std::clamp(
        param(parameters, "milling_depth", 9.0),
        0.1, std::max(0.1, definition.panel_thickness - 0.5));
    double facade_bulge = param(
        parameters, "facade_bulge", definition.depth * 0.5);
    if (legacy_meter_dimensions && facade_bulge > 0.0 && facade_bulge < 10.0) {
        facade_bulge *= 1000.0;
    }
    definition.facade_bulge = std::clamp(
        facade_bulge, 1.0, definition.width);
    double radius2_bulge = param(parameters, "radius2_bulge", 120.0);
    if (legacy_meter_dimensions && radius2_bulge > 0.0 && radius2_bulge < 10.0) {
        radius2_bulge *= 1000.0;
    }
    definition.radius2_bulge = std::clamp(
        radius2_bulge,
        1.0,
        std::min(definition.width * 0.499, definition.depth - 1.0));
    double radius_side_straight = param(parameters, "radius_side_straight", 180.0);
    if (legacy_meter_dimensions
        && radius_side_straight > 0.0 && radius_side_straight < 10.0) {
        radius_side_straight *= 1000.0;
    }
    definition.radius_side_straight = std::clamp(
        radius_side_straight, 0.0, definition.depth - 1.0);
    return definition;
}

bool radius_milano_facade_detail(
    const std::string& name, std::string& facade_name) {
    static const std::array<const char*, 5> suffixes{{
        " Bottom Profile", " Top Profile", " Right Profile",
        " Left Profile", " Center Panel"}};
    if (name.find("Radius") == std::string::npos
        || name.find("Facade") == std::string::npos) {
        return false;
    }
    for (const char* suffix : suffixes) {
        const size_t suffix_length = std::char_traits<char>::length(suffix);
        if (name.size() >= suffix_length
            && name.compare(name.size() - suffix_length,
                            suffix_length, suffix) == 0) {
            facade_name = name.substr(0, name.size() - suffix_length);
            return true;
        }
    }
    return false;
}

void create_kitchen_cabinet(CAlfaDoc& document,
                            const std::vector<ToolParameter>& parameters) {
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    const KitchenCabinetDefinition definition = kitchen_cabinet_definition(parameters);
    const auto slx_sketch = [&](const char* id) -> const CSmartLine* {
        const unsigned long object_id = static_cast<unsigned long>(
            std::max(0.0, param(parameters, id, 0.0)));
        return dynamic_cast<const CSmartLine*>(document.FindObjectById(object_id));
    };
    const CSmartLine* frame_profile =
        definition.facade_style == KitchenCabinetFacadeStyle::Frame
            ? slx_sketch("slx.profile.id") : nullptr;
    const CSmartLine* panel_profile =
        definition.facade_style == KitchenCabinetFacadeStyle::Frame
            ? slx_sketch("slx.panel.id") : nullptr;
    const CSmartLine* milling_profile =
        definition.facade_style == KitchenCabinetFacadeStyle::Milled
            ? slx_sketch("slx.milling.profile.id") : nullptr;
    const std::vector<const CSmartLine*> milling_guides =
        definition.facade_style == KitchenCabinetFacadeStyle::Milled
            ? slx_milling_guides(document, parameters)
            : std::vector<const CSmartLine*>{};
    auto parts = CKitchenCabinet::BuildParts(
        definition, frame_profile, panel_profile,
        milling_profile, milling_guides);
    if (parts.empty()) {
        return;
    }
    assign_furniture_materials(document, parts, parameters, "cabinet");
    apply_slx_catalog_handle(document, parameters, parts);
    std::vector<unsigned long> ids;
    ids.reserve(parts.size());
    std::map<std::string, std::vector<unsigned long>> facade_detail_ids;
    for (auto& part : parts) {
        const std::string part_name = part ? part->GetName() : std::string{};
        document.AddObject(std::move(part));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            std::string facade_name;
            if (radius_milano_facade_detail(part_name, facade_name)) {
                facade_detail_ids[facade_name].push_back(added->m_id);
            } else {
                ids.push_back(added->m_id);
            }
        }
    }
    for (auto& [facade_name, detail_ids] : facade_detail_ids) {
        if (detail_ids.size() != 5) {
            ids.insert(ids.end(), detail_ids.begin(), detail_ids.end());
            continue;
        }
        document.AddObject(std::make_unique<CFacadeFurniture>(
            facade_name, std::move(detail_ids)));
        if (CAlfaObject* facade = document.GetSelectedObject()) {
            ids.push_back(facade->m_id);
        }
    }
    document.AddObject(std::make_unique<CKitchenCabinet>(
        "Kitchen Cabinet", std::move(ids), definition));
    const double mounting_height = cabinet_mounting_height(parameters);
    if (mounting_height > 0.0) {
        if (auto* cabinet = dynamic_cast<CKitchenCabinet*>(
                document.GetSelectedObject())) {
            cabinet->Translate(
                {0.0f, 0.0f, static_cast<float>(mounting_height)});
        }
    }
}

void rebuild_kitchen_cabinet(CAlfaDoc& document,
                             size_t cabinet_index,
                             const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (cabinet_index >= objects.size()) {
        return;
    }
    auto* cabinet = dynamic_cast<CKitchenCabinet*>(objects[cabinet_index].get());
    if (!cabinet) {
        return;
    }
    const KitchenCabinetDefinition definition = kitchen_cabinet_definition(parameters);
    const double previous_mounting_height =
        saved_param(cabinet->GetParametricParameters(), "overhead", 0.0) >= 0.5
        ? std::max(0.0, saved_param(
            cabinet->GetParametricParameters(), "mounting_height", 1300.0))
        : 0.0;
    const double next_mounting_height = cabinet_mounting_height(parameters);
    CAssembled::TransformMatrix cabinet_transform =
        cabinet->GetAssemblyTransform();
    const double mounting_delta =
        next_mounting_height - previous_mounting_height;
    cabinet_transform[3] += cabinet_transform[2] * mounting_delta;
    cabinet_transform[7] += cabinet_transform[6] * mounting_delta;
    cabinet_transform[11] += cabinet_transform[10] * mounting_delta;
    const auto slx_sketch = [&](const char* id) -> const CSmartLine* {
        const unsigned long object_id = static_cast<unsigned long>(
            std::max(0.0, param(parameters, id, 0.0)));
        return dynamic_cast<const CSmartLine*>(document.FindObjectById(object_id));
    };
    const CSmartLine* frame_profile =
        definition.facade_style == KitchenCabinetFacadeStyle::Frame
            ? slx_sketch("slx.profile.id") : nullptr;
    const CSmartLine* panel_profile =
        definition.facade_style == KitchenCabinetFacadeStyle::Frame
            ? slx_sketch("slx.panel.id") : nullptr;
    const CSmartLine* milling_profile =
        definition.facade_style == KitchenCabinetFacadeStyle::Milled
            ? slx_sketch("slx.milling.profile.id") : nullptr;
    const std::vector<const CSmartLine*> milling_guides =
        definition.facade_style == KitchenCabinetFacadeStyle::Milled
            ? slx_milling_guides(document, parameters)
            : std::vector<const CSmartLine*>{};
    auto replacements = CKitchenCabinet::BuildParts(
        definition, frame_profile, panel_profile,
        milling_profile, milling_guides);
    if (replacements.empty()) {
        return;
    }
    assign_furniture_materials(document, replacements, parameters, "cabinet");
    apply_slx_catalog_handle(document, parameters, replacements);
    const unsigned long cabinet_id = cabinet->m_id;
    const std::vector<unsigned long> cabinet_child_ids = cabinet->GetElementIds();

    struct ExistingFacadeAssembly {
        unsigned long id = 0;
        size_t object_index = 0;
        std::string name;
        bool used = false;
    };
    std::vector<ExistingFacadeAssembly> existing_facade_assemblies;
    std::vector<unsigned long> old_ids;
    for (unsigned long id : cabinet_child_ids) {
        const size_t child_index = document.FindObjectIndexById(id);
        auto* facade = child_index < objects.size()
            ? dynamic_cast<CAssembled*>(objects[child_index].get())
            : nullptr;
        std::string facade_name;
        const bool radius_facade_assembly = facade
            && facade->GetElementIds().size() == 5
            && std::all_of(
                facade->GetElementIds().begin(), facade->GetElementIds().end(),
                [&document, &facade_name](unsigned long detail_id) {
                    const CAlfaObject* detail =
                        document.FindObjectById(detail_id);
                    std::string detail_facade_name;
                    if (!detail || !radius_milano_facade_detail(
                            detail->GetName(), detail_facade_name)) {
                        return false;
                    }
                    if (facade_name.empty()) {
                        facade_name = detail_facade_name;
                    }
                    return facade_name == detail_facade_name;
                });
        if (!radius_facade_assembly) {
            old_ids.push_back(id);
            continue;
        }
        existing_facade_assemblies.push_back(
            {id, child_index, facade_name, false});
        old_ids.insert(old_ids.end(),
                       facade->GetElementIds().begin(),
                       facade->GetElementIds().end());
    }

    struct ExistingCabinetPart {
        unsigned long id = 0;
        size_t object_index = 0;
        std::string name;
        bool used = false;
    };
    std::vector<ExistingCabinetPart> existing_parts;
    existing_parts.reserve(old_ids.size());
    std::vector<unsigned long> facade_material_ids;
    for (unsigned long id : old_ids) {
        const size_t part_index = document.FindObjectIndexById(id);
        if (part_index >= objects.size() || !objects[part_index]) {
            continue;
        }
        const CAlfaObject& part = *objects[part_index];
        existing_parts.push_back({id, part_index, part.GetName(), false});
        const bool round_handle_face =
            part.GetName().find("Facade Handle Face") != std::string::npos;
        if (part.GetName().find("Facade") != std::string::npos
            && (part.GetName().find("Handle") == std::string::npos
                || round_handle_face)
            && part.GetMaterialId() != 0) {
            facade_material_ids.push_back(part.GetMaterialId());
        }
    }

    Material shared_handle_material;
    unsigned long shared_handle_material_id = 0;
    bool has_shared_handle_material = false;
    for (const ExistingCabinetPart& existing : existing_parts) {
        const CAlfaObject& part = *objects[existing.object_index];
        if (existing.name.find("Handle") == std::string::npos
            || existing.name.find("Facade Handle Face") != std::string::npos
            || part.GetMaterialId() == 0
            || std::find(facade_material_ids.begin(), facade_material_ids.end(),
                         part.GetMaterialId()) != facade_material_ids.end()) {
            continue;
        }
        shared_handle_material = part.GetMaterial();
        shared_handle_material_id = part.GetMaterialId();
        has_shared_handle_material = true;
        break;
    }

    std::vector<unsigned long> new_ids;
    new_ids.reserve(replacements.size());
    for (auto& replacement : replacements) {
        if (!replacement) {
            continue;
        }
        auto existing = std::find_if(
            existing_parts.begin(), existing_parts.end(),
            [&replacement](const ExistingCabinetPart& candidate) {
                return !candidate.used
                    && candidate.name == replacement->GetName();
            });
        if (existing == existing_parts.end()) {
            if (has_shared_handle_material
                && replacement->GetName().find("Handle") != std::string::npos
                && replacement->GetName().find("Facade Handle Face")
                    == std::string::npos
                && replacement->GetMaterialId() == 0) {
                replacement->SetMaterial(shared_handle_material);
                replacement->SetMaterialId(shared_handle_material_id);
            }
            document.AddObject(std::move(replacement));
            if (CAlfaObject* added = document.GetSelectedObject()) {
                new_ids.push_back(added->m_id);
            }
            continue;
        }

        existing->used = true;
        CAlfaObject& old_part = *objects[existing->object_index];
        replacement->m_id = old_part.m_id;
        replacement->m_LayerID = old_part.m_LayerID;
        if (replacement->GetMaterialId() == 0) {
            replacement->SetMaterial(old_part.GetMaterial());
            replacement->SetMaterialId(old_part.GetMaterialId());
        }
        const bool catalog_handle =
            static_cast<int>(param(parameters, "handle_type", 0.0)) == 3
            && replacement->GetName().find("Handle") != std::string::npos;
        if (!catalog_handle) {
            copy_solid_surface_appearance(old_part, *replacement);
        }
        replacement->SetVisible(old_part.IsVisible());

        const bool handle_has_facade_material =
            replacement->GetName().find("Handle") != std::string::npos
            && replacement->GetName().find("Facade Handle Face")
                == std::string::npos
            && std::find(facade_material_ids.begin(), facade_material_ids.end(),
                         replacement->GetMaterialId()) != facade_material_ids.end();
        if (has_shared_handle_material && handle_has_facade_material) {
            replacement->SetMaterial(shared_handle_material);
            replacement->SetMaterialId(shared_handle_material_id);
            if (auto* solid = dynamic_cast<CSolid*>(replacement.get())) {
                for (int surface = 0; surface < solid->GetNumSurfaces(); ++surface) {
                    solid->ClearSurfaceMaterial(surface);
                }
            }
        }

        objects[existing->object_index] = std::move(replacement);
        new_ids.push_back(existing->id);
    }
    for (const ExistingCabinetPart& existing : existing_parts) {
        if (!existing.used && existing.object_index < objects.size()) {
            objects[existing.object_index].reset();
        }
    }

    std::vector<unsigned long> grouped_ids;
    grouped_ids.reserve(new_ids.size());
    std::map<std::string, std::vector<unsigned long>> facade_detail_ids;
    for (unsigned long id : new_ids) {
        CAlfaObject* part = document.FindObjectById(id);
        std::string facade_name;
        if (part
            && radius_milano_facade_detail(part->GetName(), facade_name)) {
            facade_detail_ids[facade_name].push_back(id);
        } else {
            grouped_ids.push_back(id);
        }
    }
    for (auto& [facade_name, detail_ids] : facade_detail_ids) {
        if (detail_ids.size() != 5) {
            grouped_ids.insert(
                grouped_ids.end(), detail_ids.begin(), detail_ids.end());
            continue;
        }
        auto existing_facade = std::find_if(
            existing_facade_assemblies.begin(),
            existing_facade_assemblies.end(),
            [&facade_name](const ExistingFacadeAssembly& candidate) {
                return !candidate.used && candidate.name == facade_name;
            });
        if (existing_facade != existing_facade_assemblies.end()) {
            existing_facade->used = true;
            auto* facade = existing_facade->object_index < objects.size()
                ? dynamic_cast<CAssembled*>(
                    objects[existing_facade->object_index].get())
                : nullptr;
            if (facade) {
                facade->SetElementIds(std::move(detail_ids));
                grouped_ids.push_back(existing_facade->id);
                continue;
            }
        }
        document.AddObject(std::make_unique<CFacadeFurniture>(
            facade_name, std::move(detail_ids)));
        if (CAlfaObject* facade = document.GetSelectedObject()) {
            grouped_ids.push_back(facade->m_id);
        }
    }
    for (const ExistingFacadeAssembly& existing : existing_facade_assemblies) {
        if (!existing.used && existing.object_index < objects.size()) {
            objects[existing.object_index].reset();
        }
    }
    new_ids = std::move(grouped_ids);
    cabinet = cabinet_index < objects.size()
        ? dynamic_cast<CKitchenCabinet*>(objects[cabinet_index].get()) : nullptr;
    if (!cabinet || cabinet->m_id != cabinet_id) {
        return;
    }
    cabinet->SetElementIds(std::move(new_ids));
    cabinet->SetDefinition(definition);
    cabinet->SetAssemblyTransform(cabinet_transform);
    document.SelectObjectById(cabinet_id);
}

std::vector<ToolParameter> showcase_cabinet_parameters(
    const std::vector<ToolParameter>& parameters) {
    std::vector<ToolParameter> result = parameters;
    const bool facade_showcase =
        param(parameters, "facade_showcase", 0.0) >= 0.5;
    const double selected_style = std::clamp(
        param(parameters, "facade_style", 0.0), 0.0, 4.0);
    for (ToolParameter& parameter : result) {
        if (parameter.id == "facade_style") {
            // A Plain showcase uses the simple flat Screen border. Frame
            // keeps its profiled rails. With the checkbox off, both styles
            // retain their ordinary non-showcase construction.
            parameter.value = facade_showcase && selected_style == 0.0
                ? 2.0 : selected_style;
        } else if (parameter.id == "showcase_fill") {
            parameter.value = facade_showcase
                ? std::clamp(parameter.value, 0.0, 3.0)
                : static_cast<double>(KitchenCabinetShowcaseFill::None);
        }
    }
    result.push_back({
        "body_type", "Body Type", 0.0, 0.0, 6.0, 1.0});
    result.push_back({
        "facade_type", "Facade Type",
        std::clamp(param(parameters, "showcase_facade_type", 0.0), 0.0, 1.0) + 1.0,
        0.0, 2.0, 1.0});
    return result;
}

void create_showcase_cabinet(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters) {
    create_kitchen_cabinet(
        document, showcase_cabinet_parameters(parameters));
}

void rebuild_showcase_cabinet(
    CAlfaDoc& document,
    size_t cabinet_index,
    const std::vector<ToolParameter>& parameters) {
    rebuild_kitchen_cabinet(
        document, cabinet_index, showcase_cabinet_parameters(parameters));
}

DeskDefinition desk_definition(const std::vector<ToolParameter>& parameters) {
    DeskDefinition result;
    result.type = std::clamp(static_cast<int>(param(parameters, "desk_type", 0.0)), 0, 2);
    result.drawer_count = std::clamp(
        static_cast<int>(param(parameters, "drawer_count", 3.0)), 1, 8);
    result.open_drawer = std::clamp(
        static_cast<int>(param(parameters, "open_drawer", 0.0)),
        0,
        result.drawer_count);
    result.width = std::max(300.0, param(parameters, "width", 1100.0));
    result.depth = std::max(200.0, param(parameters, "depth", 500.0));
    result.height = std::max(300.0, param(parameters, "height", 700.0));
    result.panel_thickness = std::clamp(
        param(parameters, "panel_thickness", 18.0), 1.0,
        std::min({result.width, result.depth, result.height}) * 0.2);
    result.back_panel_height = std::clamp(
        param(parameters, "back_panel_height", 300.0),
        result.panel_thickness,
        result.height - result.panel_thickness);
    result.drawer_width = std::clamp(
        param(parameters, "drawer_width", 350.0),
        result.panel_thickness * 4.0,
        result.width * 0.6);
    result.pullout_distance = std::clamp(
        param(parameters, "pullout_distance", 300.0),
        0.0,
        result.depth * 0.9);
    return result;
}

void create_desk(CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    auto parts = CDeskFurniture::BuildParts(desk_definition(parameters));
    if (parts.empty()) {
        return;
    }
    assign_furniture_materials(document, parts, parameters, "desk");
    std::vector<unsigned long> ids;
    ids.reserve(parts.size());
    for (auto& part : parts) {
        document.AddObject(std::move(part));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            ids.push_back(added->m_id);
        }
    }
    document.AddObject(std::make_unique<CDeskFurniture>("Desk", std::move(ids)));
}

void rebuild_desk(CAlfaDoc& document,
                  size_t assembly_index,
                  const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size()) {
        return;
    }
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly) {
        return;
    }
    auto replacements = CDeskFurniture::BuildParts(desk_definition(parameters));
    if (replacements.empty()) {
        return;
    }
    assign_furniture_materials(document, replacements, parameters, "desk");
    const unsigned long assembly_id = assembly->m_id;
    const std::vector<unsigned long> old_ids = assembly->GetElementIds();
    std::vector<unsigned long> new_ids;
    new_ids.reserve(replacements.size());
    const size_t common_count = std::min(old_ids.size(), replacements.size());
    for (size_t i = 0; i < common_count; ++i) {
        const size_t part_index = document.FindObjectIndexById(old_ids[i]);
        if (part_index >= objects.size() || !objects[part_index]) {
            return;
        }
        CAlfaObject& old_part = *objects[part_index];
        replacements[i]->m_id = old_part.m_id;
        replacements[i]->m_LayerID = old_part.m_LayerID;
        if (replacements[i]->GetMaterialId() == 0) {
            replacements[i]->SetMaterial(old_part.GetMaterial());
            replacements[i]->SetMaterialId(old_part.GetMaterialId());
        }
        copy_solid_surface_appearance(old_part, *replacements[i]);
        replacements[i]->SetVisible(old_part.IsVisible());
        objects[part_index] = std::move(replacements[i]);
        new_ids.push_back(old_ids[i]);
    }
    for (size_t i = common_count; i < old_ids.size(); ++i) {
        const size_t part_index = document.FindObjectIndexById(old_ids[i]);
        if (part_index < objects.size()) {
            objects[part_index].reset();
        }
    }
    for (size_t i = common_count; i < replacements.size(); ++i) {
        document.AddObject(std::move(replacements[i]));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            new_ids.push_back(added->m_id);
        }
    }
    assembly = assembly_index < objects.size()
        ? dynamic_cast<CAssembled*>(objects[assembly_index].get()) : nullptr;
    if (!assembly || assembly->m_id != assembly_id) {
        return;
    }
    assembly->SetElementIds(std::move(new_ids));
    document.SelectObjectById(assembly_id);
}

bool rebuild_wire_base(CAlfaDoc& document,
                       size_t object_index,
                       const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (object_index >= objects.size() || !objects[object_index]) return false;
    const auto* old_solid = dynamic_cast<const CSolid*>(objects[object_index].get());
    const unsigned long path_id = static_cast<unsigned long>(
        std::max(0.0, param(parameters, "profile.id", 0.0)));
    const CAlfaObject* path = document.FindObjectById(path_id);
    const bool supported = dynamic_cast<const CPolyline*>(path)
        || dynamic_cast<const CSmartLine*>(path)
        || dynamic_cast<const CBSpline*>(path);
    const double radius = param(parameters, "radius", 10.0);
    if (!old_solid || !path || !supported) return false;
    TopoDS_Shape shape = BuildWireSolidShape(*path, radius);
    if (shape.IsNull()) return false;
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
    if (!solid->ReBuldMesh()) return false;
    objects[object_index] = std::move(solid);
    return true;
}

ChairDefinition chair_definition(
    const std::vector<ToolParameter>& parameters) {
    ChairDefinition result;
    result.back_style = std::clamp(
        static_cast<int>(param(parameters, "back_style", 0.0)), 0, 2);
    result.back_member_count = std::clamp(
        static_cast<int>(param(parameters, "back_member_count", 5.0)), 1, 12);
    result.width = std::max(280.0, param(parameters, "width", 430.0));
    result.depth = std::max(280.0, param(parameters, "depth", 480.0));
    result.seat_height = std::max(
        250.0, param(parameters, "seat_height", 470.0));
    result.total_height = std::max(
        result.seat_height + 180.0,
        param(parameters, "total_height", 980.0));
    result.seat_thickness = std::clamp(
        param(parameters, "seat_thickness", 25.0), 8.0,
        result.seat_height * 0.25);
    result.seat_edge_radius = std::clamp(
        param(parameters, "seat_edge_radius", 8.0), 0.0,
        result.seat_thickness * 0.45);
    result.seat_back_curve = std::clamp(
        param(parameters, "seat_back_curve", 55.0),
        -result.depth * 0.45, result.depth * 0.45);
    result.leg_size = std::clamp(
        param(parameters, "leg_size", 42.0), 18.0,
        std::min(result.width, result.depth) * 0.2);
    result.rear_post_size = std::clamp(
        param(parameters, "rear_post_size", 45.0), 18.0,
        result.width * 0.2);
    result.leg_taper = std::clamp(
        param(parameters, "leg_taper", 5.0), 0.0,
        std::min(result.leg_size, result.rear_post_size) * 0.45);
    result.leg_inset = std::clamp(
        param(parameters, "leg_inset", 12.0), 0.0,
        std::min(result.width, result.depth) * 0.18);
    result.apron_height = std::clamp(
        param(parameters, "apron_height", 60.0), 20.0,
        result.seat_height * 0.35);
    result.apron_thickness = std::clamp(
        param(parameters, "apron_thickness", 22.0), 8.0,
        result.leg_size);
    result.stretcher_height = std::clamp(
        param(parameters, "stretcher_height", 170.0), 0.0,
        result.seat_height - result.seat_thickness - result.apron_height);
    result.stretcher_size = std::clamp(
        param(parameters, "stretcher_size", 24.0), 8.0,
        result.leg_size);
    result.back_rake = std::clamp(
        param(parameters, "back_rake", 65.0), -150.0, 250.0);
    result.rear_post_curve = std::clamp(
        param(parameters, "rear_post_curve", -35.0), -200.0, 200.0);
    result.back_member_width = std::clamp(
        param(parameters, "back_member_width", 32.0), 8.0,
        result.width * 0.25);
    result.back_member_thickness = std::clamp(
        param(parameters, "back_member_thickness", 18.0), 5.0,
        result.rear_post_size);
    result.back_rail_height = std::clamp(
        param(parameters, "back_rail_height", 55.0), 20.0,
        (result.total_height - result.seat_height) * 0.25);
    result.back_curve = std::clamp(
        param(parameters, "back_curve", 18.0), -100.0, 100.0);
    result.back_depth_curve = std::clamp(
        param(parameters, "back_depth_curve", 25.0), -200.0, 200.0);
    return result;
}

ChairDefinition simple_chair_definition(
    const std::vector<ToolParameter>& parameters) {
    ChairDefinition result;
    result.back_style = std::clamp(
        static_cast<int>(param(parameters, "back_style", 0.0)), 0, 2);
    result.back_member_count = std::clamp(
        static_cast<int>(param(parameters, "back_member_count", 5.0)), 1, 12);
    result.width = std::max(280.0, param(parameters, "width", 430.0));
    result.depth = std::max(280.0, param(parameters, "depth", 430.0));
    result.seat_height = std::max(
        250.0, param(parameters, "seat_height", 460.0));
    result.total_height = std::max(
        result.seat_height + 180.0,
        param(parameters, "total_height", 900.0));
    result.seat_thickness = std::clamp(
        param(parameters, "seat_thickness", 24.0), 8.0,
        result.seat_height * 0.25);
    result.seat_edge_radius = std::clamp(
        param(parameters, "seat_edge_radius", 6.0), 0.0,
        result.seat_thickness * 0.45);
    result.leg_size = std::clamp(
        param(parameters, "leg_size", 42.0), 18.0,
        std::min(result.width, result.depth) * 0.2);
    result.rear_post_size = result.leg_size;
    result.leg_taper = std::clamp(
        param(parameters, "leg_taper", 3.0), 0.0,
        result.leg_size * 0.45);
    result.leg_inset = std::clamp(
        param(parameters, "leg_inset", 12.0), 0.0,
        std::min(result.width, result.depth) * 0.18);
    result.apron_height = std::clamp(
        param(parameters, "apron_height", 60.0), 20.0,
        result.seat_height * 0.35);
    result.apron_thickness = std::min(22.0, result.leg_size);
    result.stretcher_height = std::clamp(
        param(parameters, "stretcher_height", 170.0), 0.0,
        result.seat_height - result.seat_thickness - result.apron_height);
    result.stretcher_size = std::min(24.0, result.leg_size);
    result.back_rake = std::clamp(
        param(parameters, "back_rake", 35.0), -100.0, 150.0);
    result.back_member_width = std::clamp(
        param(parameters, "back_member_width", 28.0), 8.0,
        result.width * 0.25);
    result.back_member_thickness = std::min(18.0, result.rear_post_size);
    result.back_rail_height = std::clamp(
        param(parameters, "back_rail_height", 50.0), 20.0,
        (result.total_height - result.seat_height) * 0.25);

    // Chair Simple deliberately keeps the construction rectilinear.  These
    // capabilities remain available in Chair Advanced.
    result.seat_back_curve = 0.0;
    result.rear_post_curve = 0.0;
    result.back_curve = 0.0;
    result.back_depth_curve = 0.0;
    return result;
}

void create_chair_from_definition(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters,
    const ChairDefinition& definition,
    const char* assembly_name) {
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    auto parts = CChairFurniture::BuildParts(definition);
    if (parts.empty()) {
        return;
    }
    assign_furniture_materials(document, parts, parameters, "chair");
    std::vector<unsigned long> ids;
    ids.reserve(parts.size());
    for (auto& part : parts) {
        document.AddObject(std::move(part));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            ids.push_back(added->m_id);
        }
    }
    document.AddObject(std::make_unique<CChairFurniture>(
        assembly_name, std::move(ids)));
}

void create_chair(CAlfaDoc& document,
                  const std::vector<ToolParameter>& parameters) {
    create_chair_from_definition(
        document, parameters, chair_definition(parameters), "Chair Advanced");
}

void create_simple_chair(CAlfaDoc& document,
                         const std::vector<ToolParameter>& parameters) {
    create_chair_from_definition(
        document, parameters, simple_chair_definition(parameters),
        "Chair Simple");
}

void rebuild_chair_from_definition(
    CAlfaDoc& document,
    size_t assembly_index,
    const std::vector<ToolParameter>& parameters,
    const ChairDefinition& definition) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size()) {
        return;
    }
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly) {
        return;
    }
    auto replacements = CChairFurniture::BuildParts(definition);
    if (replacements.empty()) {
        return;
    }
    assign_furniture_materials(document, replacements, parameters, "chair");
    const unsigned long assembly_id = assembly->m_id;
    const std::vector<unsigned long> old_ids = assembly->GetElementIds();
    std::vector<unsigned long> new_ids;
    new_ids.reserve(replacements.size());
    const size_t common_count = std::min(old_ids.size(), replacements.size());
    for (size_t index = 0; index < common_count; ++index) {
        const size_t part_index = document.FindObjectIndexById(old_ids[index]);
        if (part_index >= objects.size() || !objects[part_index]) {
            return;
        }
        CAlfaObject& old_part = *objects[part_index];
        replacements[index]->m_id = old_part.m_id;
        replacements[index]->m_LayerID = old_part.m_LayerID;
        if (replacements[index]->GetMaterialId() == 0) {
            replacements[index]->SetMaterial(old_part.GetMaterial());
            replacements[index]->SetMaterialId(old_part.GetMaterialId());
        }
        copy_solid_surface_appearance(old_part, *replacements[index]);
        replacements[index]->SetVisible(old_part.IsVisible());
        objects[part_index] = std::move(replacements[index]);
        new_ids.push_back(old_ids[index]);
    }
    for (size_t index = common_count; index < old_ids.size(); ++index) {
        const size_t part_index = document.FindObjectIndexById(old_ids[index]);
        if (part_index < objects.size()) {
            objects[part_index].reset();
        }
    }
    for (size_t index = common_count; index < replacements.size(); ++index) {
        document.AddObject(std::move(replacements[index]));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            new_ids.push_back(added->m_id);
        }
    }
    assembly = assembly_index < objects.size()
        ? dynamic_cast<CAssembled*>(objects[assembly_index].get()) : nullptr;
    if (!assembly || assembly->m_id != assembly_id) {
        return;
    }
    assembly->SetElementIds(std::move(new_ids));
    document.SelectObjectById(assembly_id);
}

void rebuild_chair(CAlfaDoc& document,
                   size_t assembly_index,
                   const std::vector<ToolParameter>& parameters) {
    rebuild_chair_from_definition(
        document, assembly_index, parameters, chair_definition(parameters));
}

void rebuild_simple_chair(CAlfaDoc& document,
                          size_t assembly_index,
                          const std::vector<ToolParameter>& parameters) {
    rebuild_chair_from_definition(
        document, assembly_index, parameters,
        simple_chair_definition(parameters));
}

DrawerBoxDefinition drawer_box_definition(
    const std::vector<ToolParameter>& parameters) {
    DrawerBoxDefinition result;
    result.width = std::max(300.0, param(parameters, "width", 600.0));
    result.height = std::max(300.0, param(parameters, "height", 800.0));
    result.depth = std::max(250.0, param(parameters, "depth", 500.0));
    result.panel_thickness = std::clamp(
        param(parameters, "panel_thickness", 18.0), 5.0,
        std::min({result.width, result.depth, result.height}) * 0.15);
    result.drawer_side_thickness = std::clamp(
        param(parameters, "drawer_side_thickness", 12.0), 5.0, 30.0);
    result.drawer_bottom_thickness = std::clamp(
        param(parameters, "drawer_bottom_thickness", 6.0), 3.0, 20.0);
    result.slide_clearance = std::clamp(
        param(parameters, "slide_clearance", 13.0), 5.0, 40.0);
    result.facade_type = std::clamp(
        static_cast<int>(param(parameters, "facade_type", 0.0)), 0, 4);
    result.handle_type = std::clamp(
        static_cast<int>(param(parameters, "handle_type", 0.0)), 0, 3);
    result.drawer_count = std::clamp(
        static_cast<int>(param(parameters, "drawer_count", 3.0)), 1, 6);
    result.drawer_heights.clear();
    for (int drawer = 0; drawer < result.drawer_count; ++drawer) {
        const std::string id = "drawer_height_" + std::to_string(drawer + 1);
        const double fallback = drawer == 2 ? 400.0 : 200.0;
        result.drawer_heights.push_back(std::max(40.0, param(parameters, id.c_str(), fallback)));
    }
    result.make_legs = param(parameters, "make_legs", 1.0) >= 0.5;
    result.leg_height = std::clamp(
        param(parameters, "leg_height", 100.0), 20.0, 300.0);
    result.open_drawer = std::clamp(
        static_cast<int>(param(parameters, "open_drawer", 0.0)), 0, 6);
    result.pullout_distance = std::clamp(
        param(parameters, "pullout_distance", 300.0), 0.0, result.depth * 0.9);
    return result;
}

void create_drawer_box(CAlfaDoc& document,
                       const std::vector<ToolParameter>& parameters) {
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    auto parts = CDrawerBoxFurniture::BuildParts(
        drawer_box_definition(parameters));
    if (parts.empty()) {
        return;
    }
    assign_furniture_materials(document, parts, parameters, "drawer_box");
    std::vector<unsigned long> ids;
    ids.reserve(parts.size());
    for (auto& part : parts) {
        document.AddObject(std::move(part));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            ids.push_back(added->m_id);
        }
    }
    document.AddObject(std::make_unique<CDrawerBoxFurniture>(
        "Drawer Box", std::move(ids)));
}

void rebuild_drawer_box(CAlfaDoc& document,
                        size_t assembly_index,
                        const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size()) return;
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly) return;
    auto replacements = CDrawerBoxFurniture::BuildParts(
        drawer_box_definition(parameters));
    if (replacements.empty()) return;
    assign_furniture_materials(document, replacements, parameters, "drawer_box");
    const unsigned long assembly_id = assembly->m_id;
    const std::vector<unsigned long> old_ids = assembly->GetElementIds();
    std::vector<unsigned long> new_ids;
    new_ids.reserve(replacements.size());
    const size_t common_count = std::min(old_ids.size(), replacements.size());
    for (size_t i = 0; i < common_count; ++i) {
        const size_t part_index = document.FindObjectIndexById(old_ids[i]);
        if (part_index >= objects.size() || !objects[part_index]) return;
        CAlfaObject& old_part = *objects[part_index];
        replacements[i]->m_id = old_part.m_id;
        replacements[i]->m_LayerID = old_part.m_LayerID;
        if (replacements[i]->GetMaterialId() == 0) {
            replacements[i]->SetMaterial(old_part.GetMaterial());
            replacements[i]->SetMaterialId(old_part.GetMaterialId());
        }
        copy_solid_surface_appearance(old_part, *replacements[i]);
        replacements[i]->SetVisible(old_part.IsVisible());
        objects[part_index] = std::move(replacements[i]);
        new_ids.push_back(old_ids[i]);
    }
    for (size_t i = common_count; i < old_ids.size(); ++i) {
        const size_t part_index = document.FindObjectIndexById(old_ids[i]);
        if (part_index < objects.size()) objects[part_index].reset();
    }
    for (size_t i = common_count; i < replacements.size(); ++i) {
        document.AddObject(std::move(replacements[i]));
        if (CAlfaObject* added = document.GetSelectedObject()) new_ids.push_back(added->m_id);
    }
    assembly = assembly_index < objects.size()
        ? dynamic_cast<CAssembled*>(objects[assembly_index].get()) : nullptr;
    if (!assembly || assembly->m_id != assembly_id) return;
    assembly->SetElementIds(std::move(new_ids));
    document.SelectObjectById(assembly_id);
}

std::unique_ptr<CSolid> component_solid(
    const std::string& name, TopoDS_Shape shape, Color color) {
    if (shape.IsNull()) return nullptr;
    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName(name);
    solid->SetColor(color);
    return solid->ReBuldMesh() ? std::move(solid) : nullptr;
}

TopoDS_Shape component_box(double x, double y, double z,
                           double width, double depth, double height) {
    BRepPrimAPI_MakeBox builder(
        gp_Pnt(x, y, z), width, depth, height);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape{};
}

TopoDS_Shape rotate_component(
    const TopoDS_Shape& shape, double hinge_x, double angle_degrees) {
    if (shape.IsNull() || std::abs(angle_degrees) <= 1.0e-9) return shape;
    gp_Trsf rotation;
    rotation.SetRotation(
        gp_Ax1(gp_Pnt(hinge_x, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)),
        angle_degrees * 3.14159265358979323846 / 180.0);
    BRepBuilderAPI_Transform transformed(shape, rotation, true);
    transformed.Build();
    return transformed.IsDone() ? transformed.Shape() : TopoDS_Shape{};
}

std::vector<std::unique_ptr<CAlfaObject>> single_drawer_parts(
    const std::vector<ToolParameter>& parameters) {
    const double width = std::max(100.0, param(parameters, "width", 500.0));
    const double depth = std::max(100.0, param(parameters, "depth", 450.0));
    const double facade_height = std::max(
        60.0, param(parameters, "height", 180.0));
    const double facade_thickness = std::clamp(
        param(parameters, "facade_thickness", 18.0), 5.0, 60.0);
    const double side = std::clamp(
        param(parameters, "drawer_side_thickness", 12.0), 5.0, 30.0);
    const double bottom = std::clamp(
        param(parameters, "drawer_bottom_thickness", 6.0), 3.0, 20.0);
    const double clearance = std::clamp(
        param(parameters, "slide_clearance", 13.0), 0.0, width * 0.2);
    const double pullout = std::clamp(
        param(parameters, "pullout_distance", 300.0), 0.0, depth * 0.95);
    const double extension = param(parameters, "open_drawer", 0.0) >= 0.5
        ? -pullout : 0.0;
    const int handle_type = std::clamp(
        static_cast<int>(param(parameters, "handle_type", 0.0)), 0, 3);

    FurnitureDrawerDefinition drawer;
    drawer.part_prefix = "Drawer 1";
    drawer.facade_name = "Drawer 1 Facade";
    drawer.handle_name = "Drawer 1 Handle";
    drawer.left = -width * 0.5 + clearance;
    drawer.front = -depth * 0.5 + extension;
    drawer.bottom = 12.0;
    drawer.width = std::max(60.0, width - 2.0 * clearance);
    drawer.depth = depth;
    drawer.height = std::max(45.0, facade_height - 30.0);
    drawer.side_thickness = side;
    drawer.bottom_thickness = bottom;
    drawer.bottom_offset = 8.0;
    drawer.facade_left = -width * 0.5;
    drawer.facade_front = -depth * 0.5 - facade_thickness + extension;
    drawer.facade_bottom = 0.0;
    drawer.facade_width = width;
    drawer.facade_height = facade_height;
    drawer.facade_thickness = facade_thickness;
    drawer.facade_style = param(parameters, "facade_style", 0.0) >= 0.5
        ? FurnitureDrawerFacadeStyle::Frame
        : FurnitureDrawerFacadeStyle::Plain;
    drawer.make_handle = handle_type != 3;
    drawer.round_handle = handle_type == 2;
    drawer.handle_height_ratio = 0.62;
    return CFurnitureDrawer::BuildParts(drawer);
}

std::vector<std::unique_ptr<CAlfaObject>> single_facade_parts(
    const std::vector<ToolParameter>& parameters) {
    const double width = std::max(60.0, param(parameters, "width", 500.0));
    const double height = std::max(60.0, param(parameters, "height", 700.0));
    const double thickness = std::clamp(
        param(parameters, "thickness", 18.0), 5.0, 80.0);
    const auto style = static_cast<KitchenCabinetFacadeStyle>(std::clamp(
        static_cast<int>(param(parameters, "facade_style", 0.0)), 0, 4));
    const bool hinge_right = param(parameters, "hinge_side", 0.0) >= 0.5;
    const double angle = std::clamp(
        param(parameters, "open_angle", 0.0), 0.0, 150.0);
    const double signed_angle = hinge_right ? angle : -angle;
    const double hinge_x = hinge_right ? width * 0.5 : -width * 0.5;
    const Color facade_color{0.62f, 0.34f, 0.16f};
    const Color handle_color{0.28f, 0.24f, 0.14f};
    std::vector<std::unique_ptr<CAlfaObject>> parts;

    TopoDS_Shape facade = CFacadeFurniture::BuildPlanarShape(
        style, -width * 0.5, -thickness * 0.5, 0.0,
        width, thickness, height, true);
    facade = rotate_component(facade, hinge_x, signed_angle);
    parts.push_back(component_solid(
        "Single Facade", std::move(facade), facade_color));

    const int handle_type = std::clamp(
        static_cast<int>(param(parameters, "handle_type", 0.0)), 0, 3);
    if (handle_type != 3) {
        const bool vertical = param(parameters, "handle_orientation", 0.0) >= 0.5;
        const double long_size = handle_type == 2
            ? 24.0 : std::clamp(width * 0.30, 90.0, 180.0);
        const double handle_width = vertical ? 10.0 : long_size;
        const double handle_height = vertical ? long_size : 10.0;
        constexpr double handle_edge_margin = 28.0;
        const double handle_x = hinge_right
            ? -width * 0.5 + handle_edge_margin
            : width * 0.5 - handle_edge_margin - handle_width;
        const double handle_z = vertical
            ? height - handle_height - 30.0 : height - 42.0;
        TopoDS_Shape handle = component_box(
            handle_x, -thickness * 0.5 - 18.0, handle_z,
            handle_width, 14.0, handle_height);
        handle = rotate_component(handle, hinge_x, signed_angle);
        parts.push_back(component_solid(
            "Single Facade Handle", std::move(handle), handle_color));
    }
    if (std::any_of(parts.begin(), parts.end(),
                    [](const auto& part) { return !part; })) {
        return {};
    }
    return parts;
}

void create_component_assembly(
    CAlfaDoc& document,
    std::vector<std::unique_ptr<CAlfaObject>> parts,
    const std::vector<ToolParameter>& parameters,
    const std::string& tool_id,
    const std::string& name) {
    if (parts.empty()) return;
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    assign_furniture_materials(document, parts, parameters, tool_id);
    std::vector<unsigned long> ids;
    ids.reserve(parts.size());
    for (auto& part : parts) {
        document.AddObject(std::move(part));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            ids.push_back(added->m_id);
        }
    }
    document.AddObject(std::make_unique<CAssembled>(name, std::move(ids)));
}

void rebuild_component_assembly(
    CAlfaDoc& document,
    size_t assembly_index,
    std::vector<std::unique_ptr<CAlfaObject>> replacements,
    const std::vector<ToolParameter>& parameters,
    const std::string& tool_id) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size() || replacements.empty()) return;
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly) return;
    assign_furniture_materials(document, replacements, parameters, tool_id);
    const unsigned long assembly_id = assembly->m_id;
    const std::vector<unsigned long> old_ids = assembly->GetElementIds();
    std::vector<unsigned long> new_ids;
    const size_t common_count = std::min(old_ids.size(), replacements.size());
    for (size_t index = 0; index < common_count; ++index) {
        const size_t part_index = document.FindObjectIndexById(old_ids[index]);
        if (part_index >= objects.size() || !objects[part_index]) return;
        CAlfaObject& old_part = *objects[part_index];
        replacements[index]->m_id = old_part.m_id;
        replacements[index]->m_LayerID = old_part.m_LayerID;
        if (replacements[index]->GetMaterialId() == 0) {
            replacements[index]->SetMaterial(old_part.GetMaterial());
            replacements[index]->SetMaterialId(old_part.GetMaterialId());
        }
        if (tool_id != "room") {
            copy_solid_surface_appearance(old_part, *replacements[index]);
        }
        replacements[index]->SetVisible(old_part.IsVisible());
        objects[part_index] = std::move(replacements[index]);
        new_ids.push_back(old_ids[index]);
    }
    for (size_t index = common_count; index < old_ids.size(); ++index) {
        const size_t part_index = document.FindObjectIndexById(old_ids[index]);
        if (part_index < objects.size()) objects[part_index].reset();
    }
    for (size_t index = common_count; index < replacements.size(); ++index) {
        document.AddObject(std::move(replacements[index]));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            new_ids.push_back(added->m_id);
        }
    }
    assembly = assembly_index < objects.size()
        ? dynamic_cast<CAssembled*>(objects[assembly_index].get()) : nullptr;
    if (!assembly || assembly->m_id != assembly_id) return;
    assembly->SetElementIds(std::move(new_ids));
    document.SelectObjectById(assembly_id);
}

struct ArchitectureWallPlacement {
    bool attached = false;
    bool along_x = true;
    double local_inward_sign = 1.0;
    Vec3 minimum{};
    Vec3 maximum{};
};

unsigned long architecture_host_wall_id(
    const std::vector<ToolParameter>& parameters) {
    return static_cast<unsigned long>(
        std::max(0.0, param(parameters, "host.wall.id", 0.0)));
}

bool is_room_wall_name(const std::string& name) {
    return name == "Room Front Wall" || name == "Room Back Wall"
        || name == "Room Left Wall" || name == "Room Right Wall";
}

std::vector<unsigned long> visible_room_wall_ids(CAlfaDoc& document) {
    std::vector<unsigned long> result;
    for (const auto& object : document.GetObjects()) {
        auto* room = dynamic_cast<CAssembled*>(object.get());
        if (!room || room->GetParametricToolId() != "room"
            || !room->IsVisible()) {
            continue;
        }
        for (const unsigned long element_id : room->GetElementIds()) {
            auto* wall = dynamic_cast<CSolid*>(
                document.FindObjectById(element_id));
            if (!wall || !wall->IsVisible()
                || !is_room_wall_name(wall->GetName())) {
                continue;
            }
            document.EnsureObjectId(*wall);
            result.push_back(wall->m_id);
        }
    }
    return result;
}

unsigned long selected_room_wall_id(CAlfaDoc& document) {
    auto* wall = dynamic_cast<CSolid*>(document.GetSelectedObject());
    if (!wall || !is_room_wall_name(wall->GetName())) return 0;
    document.EnsureObjectId(*wall);
    for (const auto& object : document.GetObjects()) {
        const auto* room = dynamic_cast<const CAssembled*>(object.get());
        if (!room || room->GetParametricToolId() != "room") continue;
        const auto& ids = room->GetElementIds();
        if (std::find(ids.begin(), ids.end(), wall->m_id) != ids.end()) {
            return wall->m_id;
        }
    }
    return 0;
}

void set_tool_parameter_value(
    std::vector<ToolParameter>& parameters,
    const std::string& id,
    double value) {
    const auto found = std::find_if(
        parameters.begin(), parameters.end(), [&id](const ToolParameter& parameter) {
            return parameter.id == id;
        });
    if (found != parameters.end()) found->value = value;
}

ArchitectureWallPlacement architecture_wall_placement(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters) {
    ArchitectureWallPlacement placement;
    const unsigned long wall_id = architecture_host_wall_id(parameters);
    const auto* wall = dynamic_cast<const CSolid*>(document.FindObjectById(wall_id));
    if (!wall || !is_room_wall_name(wall->GetName())
        || !wall->GetBounds(placement.minimum, placement.maximum)) {
        return placement;
    }
    placement.attached = true;
    placement.along_x =
        placement.maximum.x - placement.minimum.x
        >= placement.maximum.y - placement.minimum.y;
    const std::string& wall_name = wall->GetName();
    if (wall_name == "Room Back Wall"
        || wall_name == "Room Left Wall") {
        placement.local_inward_sign = -1.0;
    }
    return placement;
}

void transform_architecture_parts(
    std::vector<std::unique_ptr<CAlfaObject>>& parts,
    const ArchitectureWallPlacement& placement) {
    const auto transform = [&parts](const gp_Trsf& operation) {
        for (auto& part : parts) {
            auto* solid = dynamic_cast<CSolid*>(part.get());
            if (!solid || solid->m_Shape.IsNull()) continue;
            BRepBuilderAPI_Transform builder(solid->m_Shape, operation, true);
            builder.Build();
            if (!builder.IsDone() || builder.Shape().IsNull()) continue;
            solid->Clear();
            solid->m_Shape = builder.Shape();
            solid->InitSurfaces();
            solid->ReBuldMesh();
        }
    };

    if (!placement.attached) {
        gp_Trsf lay_on_xy;
        lay_on_xy.SetRotation(
            gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)),
            -3.14159265358979323846 * 0.5);
        transform(lay_on_xy);
        return;
    }

    if (!placement.along_x) {
        gp_Trsf turn;
        turn.SetRotation(
            gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)),
            3.14159265358979323846 * 0.5);
        transform(turn);
    }

    gp_Trsf move;
    if (placement.along_x) {
        move.SetTranslation(gp_Vec(
            placement.minimum.x,
            (placement.minimum.y + placement.maximum.y) * 0.5,
            0.0));
    } else {
        move.SetTranslation(gp_Vec(
            (placement.minimum.x + placement.maximum.x) * 0.5,
            placement.minimum.y,
            0.0));
    }
    transform(move);
}

bool cut_architecture_opening(
    CSolid& wall,
    const std::vector<ParametricParameterValue>& parameters,
    const std::string& tool_id) {
    Vec3 minimum{};
    Vec3 maximum{};
    if (wall.m_Shape.IsNull() || !wall.GetBounds(minimum, maximum)) {
        return false;
    }
    const double width = std::max(
        tool_id == "window" ? 300.0 : 500.0,
        saved_param(parameters, "width", tool_id == "window" ? 1000.0 : 900.0));
    const double height = std::max(
        tool_id == "window" ? 300.0 : 1000.0,
        saved_param(parameters, "height", tool_id == "window" ? 1300.0 : 2000.0));
    const double distance = saved_param(
        parameters, "distance_along_wall", tool_id == "window" ? 1800.0 : 1270.0);
    const double bottom = std::max(0.0, saved_param(
        parameters,
        tool_id == "window" ? "distance_from_floor" : "distance_to_floor",
        tool_id == "window" ? 900.0 : 100.0));
    const bool along_x = maximum.x - minimum.x >= maximum.y - minimum.y;
    constexpr double margin = 20.0;
    TopoDS_Shape cutter;
    if (along_x) {
        cutter = component_box(
            minimum.x + distance - width * 0.5,
            minimum.y - margin,
            bottom,
            width,
            maximum.y - minimum.y + 2.0 * margin,
            height);
    } else {
        cutter = component_box(
            minimum.x - margin,
            minimum.y + distance - width * 0.5,
            bottom,
            maximum.x - minimum.x + 2.0 * margin,
            width,
            height);
    }
    if (cutter.IsNull()) return false;
    BRepAlgoAPI_Cut cut(wall.m_Shape, cutter);
    cut.Build();
    if (!cut.IsDone() || cut.Shape().IsNull()) return false;
    wall.Clear();
    wall.m_Shape = cut.Shape();
    wall.InitSurfaces();
    return wall.ReBuldMesh();
}

unsigned long ensure_room_material(
    CAlfaDoc& document,
    const std::string& name,
    const std::string& texture_path,
    Color diffuse,
    float roughness) {
    if (Material* existing = document.FindMaterial(name, true)) {
        return existing->id;
    }
    Material material;
    material.id = 0;
    material.name = name;
    material.diffuse = diffuse;
    material.ambient = {
        diffuse.r * 0.28f,
        diffuse.g * 0.28f,
        diffuse.b * 0.28f};
    material.specular = 0.12f;
    material.shininess = 18.0f;
    material.roughness = roughness;
    material.color_texture_path = texture_path;
    return document.UpsertMaterial(std::move(material)).id;
}

unsigned long ensure_room_ceiling_material(CAlfaDoc& document) {
    Material* material = document.FindMaterial(
        "Room Ceiling White Paint", true);
    if (!material) {
        Material ceiling;
        ceiling.id = 0;
        ceiling.name = "Room Ceiling White Paint";
        material = &document.UpsertMaterial(std::move(ceiling));
    }
    material->diffuse = {1.0f, 0.99f, 0.96f};
    material->ambient = {0.72f, 0.71f, 0.68f};
    material->emission = {0.08f, 0.08f, 0.07f};
    material->specular = 0.08f;
    material->shininess = 12.0f;
    material->roughness = 0.88f;
    return material->id;
}

unsigned long ensure_room_window_glass_material(CAlfaDoc& document) {
    if (Material* existing = document.FindMaterial(
            "Room Window Clear Glass", true)) {
        return existing->id;
    }
    Material glass;
    glass.id = 0;
    glass.name = "Room Window Clear Glass";
    glass.ambient = {0.10f, 0.13f, 0.15f};
    glass.diffuse = {0.64f, 0.82f, 0.92f};
    glass.alpha = 0.18f;
    glass.specular = 0.88f;
    glass.shininess = 110.0f;
    glass.reflectivity = 0.18f;
    glass.roughness = 0.025f;
    return document.UpsertMaterial(std::move(glass)).id;
}

unsigned long ensure_room_window_sill_material(CAlfaDoc& document) {
    if (Material* existing = document.FindMaterial(
            "Room Window Sill White", true)) {
        return existing->id;
    }
    Material sill;
    sill.id = 0;
    sill.name = "Room Window Sill White";
    sill.ambient = {0.70f, 0.70f, 0.68f};
    sill.diffuse = {0.96f, 0.95f, 0.92f};
    sill.specular = 0.22f;
    sill.shininess = 34.0f;
    sill.roughness = 0.38f;
    return document.UpsertMaterial(std::move(sill)).id;
}

void apply_room_materials(
    CAlfaDoc& document,
    std::vector<std::unique_ptr<CAlfaObject>>& parts,
    double left,
    double front,
    double length,
    double width,
    double wall_thickness) {
    const unsigned long wall_material_id = ensure_room_material(
        document,
        "Room Wall Textile 24708",
        "texture/textiles/24708.jpg",
        {0.78f, 0.76f, 0.70f},
        0.78f);
    const unsigned long floor_material_id = ensure_room_material(
        document,
        "Room Floor Unopark Merbau",
        "texture/parquet/Unopark_Merbau.jpg",
        {0.45f, 0.25f, 0.12f},
        0.56f);
    ensure_room_window_glass_material(document);
    const unsigned long ceiling_material_id = parts.size() > 5
        ? ensure_room_ceiling_material(document) : 0;
    const Material* wall_material = document.FindMaterial(wall_material_id);
    const Material* floor_material = document.FindMaterial(floor_material_id);
    if (!wall_material || !floor_material || parts.size() < 5) return;

    if (auto* floor_solid = dynamic_cast<CSolid*>(parts[0].get())) {
        floor_solid->SetMaterial(*floor_material);
        floor_solid->SetMaterialId(floor_material->id);
    }
    if (parts.size() > 5) {
        const Material* ceiling_material =
            document.FindMaterial(ceiling_material_id);
        if (!ceiling_material) return;
        if (auto* ceiling_solid = dynamic_cast<CSolid*>(parts[5].get())) {
            ceiling_solid->SetMaterial(*ceiling_material);
            ceiling_solid->SetMaterialId(ceiling_material->id);
        }
    }

    const std::array<std::pair<int, double>, 4> inner_planes{{
        {1, front + wall_thickness},
        {1, front + width - wall_thickness},
        {0, left + wall_thickness},
        {0, left + length - wall_thickness}
    }};
    constexpr double plane_tolerance = 0.25;
    for (size_t wall_index = 0; wall_index < inner_planes.size(); ++wall_index) {
        auto* wall_solid = dynamic_cast<CSolid*>(
            parts[wall_index + 1].get());
        if (!wall_solid) continue;
        const int axis = inner_planes[wall_index].first;
        const double plane = inner_planes[wall_index].second;
        for (int surface_index = 0;
             surface_index < wall_solid->GetNumSurfaces();
             ++surface_index) {
            Vec3 center{};
            Vec3 normal{};
            if (!wall_solid->GetFaceCenterAndNormal(
                    surface_index, center, normal)) {
                continue;
            }
            const double coordinate = axis == 0 ? center.x : center.y;
            if (std::abs(coordinate - plane) <= plane_tolerance) {
                wall_solid->SetSurfaceMaterial(
                    surface_index, *wall_material);
            }
        }
    }
}

std::vector<std::unique_ptr<CAlfaObject>> room_parts(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters,
    const CAssembled* existing_room = nullptr) {
    const double length = std::max(500.0, param(parameters, "length", 6000.0));
    const double width = std::max(500.0, param(parameters, "width", 4000.0));
    const double height = std::max(300.0, param(parameters, "height", 2900.0));
    const double wall = std::clamp(
        param(parameters, "wall_thickness", 200.0), 20.0,
        std::min(length, width) * 0.45);
    const double floor = std::clamp(
        param(parameters, "floor_thickness", 120.0), 10.0, 500.0);
    const bool ceiling = param(parameters, "ceiling", 0.0) >= 0.5;

    const double left = -length * 0.5;
    const double front = -width * 0.5;
    const Color wall_color{0.82f, 0.78f, 0.72f};
    const Color floor_color{0.48f, 0.30f, 0.16f};
    const Color ceiling_color{0.92f, 0.92f, 0.90f};
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    const auto add_box = [&parts](const std::string& name,
                                  double x, double y, double z,
                                  double dx, double dy, double dz,
                                  Color color) {
        parts.push_back(component_solid(
            name, component_box(x, y, z, dx, dy, dz), color));
    };

    add_box("Room Floor", left, front, -floor,
            length, width, floor, floor_color);
    add_box("Room Front Wall", left, front, 0.0,
            length, wall, height, wall_color);
    add_box("Room Back Wall", left, front + width - wall, 0.0,
            length, wall, height, wall_color);
    add_box("Room Left Wall", left, front + wall, 0.0,
            wall, width - 2.0 * wall, height, wall_color);
    add_box("Room Right Wall", left + length - wall, front + wall, 0.0,
            wall, width - 2.0 * wall, height, wall_color);
    if (ceiling) {
        add_box("Room Ceiling", left, front, height,
                length, width, floor, ceiling_color);
    }

    if (existing_room) {
        const std::vector<unsigned long>& wall_ids = existing_room->GetElementIds();
        const size_t count = std::min(parts.size(), wall_ids.size());
        for (size_t part_index = 1; part_index < count; ++part_index) {
            auto* wall_solid = dynamic_cast<CSolid*>(parts[part_index].get());
            if (!wall_solid || !is_room_wall_name(wall_solid->GetName())) continue;
            const unsigned long host_id = wall_ids[part_index];
            for (const auto& object : document.GetObjects()) {
                if (!object) continue;
                const std::string& tool_id = object->GetParametricToolId();
                if (tool_id != "window" && tool_id != "door") continue;
                const auto& saved = object->GetParametricParameters();
                if (static_cast<unsigned long>(std::max(
                        0.0, saved_param(saved, "host.wall.id", 0.0)))
                    != host_id) {
                    continue;
                }
                cut_architecture_opening(*wall_solid, saved, tool_id);
            }
        }
    }
    apply_room_materials(
        document, parts, left, front, length, width, wall);
    if (std::any_of(parts.begin(), parts.end(),
                    [](const auto& part) { return !part; })) {
        return {};
    }
    return parts;
}

TopoDS_Shape window_sill_shape(double x,
                               double y,
                               double z,
                               double width,
                               double depth,
                               double thickness) {
    constexpr double corner_radius = 15.0;
    constexpr double top_edge_radius = 5.0;
    constexpr double inverse_sqrt_two = 0.7071067811865475;
    const double radius = std::min(
        corner_radius, std::min(width, depth) * 0.45);
    if (width <= 2.0 * radius || depth <= 2.0 * radius
        || thickness <= 0.0) {
        return component_box(x, y, z, width, depth, thickness);
    }

    const double right = x + width;
    const double back = y + depth;
    const auto point = [z](double px, double py) {
        return gp_Pnt(px, py, z);
    };
    try {
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(
            point(x + radius, y), point(right - radius, y)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(
            point(right - radius, y),
            point(right - radius * (1.0 - inverse_sqrt_two),
                  y + radius * (1.0 - inverse_sqrt_two)),
            point(right, y + radius)).Value()).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            point(right, y + radius), point(right, back - radius)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(
            point(right, back - radius),
            point(right - radius * (1.0 - inverse_sqrt_two),
                  back - radius * (1.0 - inverse_sqrt_two)),
            point(right - radius, back)).Value()).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            point(right - radius, back), point(x + radius, back)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(
            point(x + radius, back),
            point(x + radius * (1.0 - inverse_sqrt_two),
                  back - radius * (1.0 - inverse_sqrt_two)),
            point(x, back - radius)).Value()).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(
            point(x, back - radius), point(x, y + radius)).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(
            point(x, y + radius),
            point(x + radius * (1.0 - inverse_sqrt_two),
                  y + radius * (1.0 - inverse_sqrt_two)),
            point(x + radius, y)).Value()).Edge());
        wire.Build();
        if (!wire.IsDone()) return {};
        BRepBuilderAPI_MakeFace face(wire.Wire());
        face.Build();
        if (!face.IsDone()) return {};
        BRepPrimAPI_MakePrism prism(
            face.Face(), gp_Vec(0.0, 0.0, thickness));
        prism.Build();
        if (!prism.IsDone()) return {};

        TopoDS_Shape panel = prism.Shape();
        BRepFilletAPI_MakeFillet fillet(panel);
        int edge_count = 0;
        const double top_z = z + thickness;
        for (TopExp_Explorer explorer(panel, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            TopoDS_Vertex first_vertex;
            TopoDS_Vertex last_vertex;
            TopExp::Vertices(edge, first_vertex, last_vertex);
            if (first_vertex.IsNull() || last_vertex.IsNull()) continue;
            const gp_Pnt first = BRep_Tool::Pnt(first_vertex);
            const gp_Pnt last = BRep_Tool::Pnt(last_vertex);
            if (std::abs(first.Z() - top_z) <= 1.0e-5
                && std::abs(last.Z() - top_z) <= 1.0e-5) {
                fillet.Add(top_edge_radius, edge);
                ++edge_count;
            }
        }
        if (edge_count > 0) {
            fillet.Build();
            if (fillet.IsDone() && !fillet.Shape().IsNull()) {
                return fillet.Shape();
            }
        }
        return panel;
    } catch (const Standard_Failure&) {
        return component_box(x, y, z, width, depth, thickness);
    }
}

std::vector<std::unique_ptr<CAlfaObject>> window_parts(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters) {
    const ArchitectureWallPlacement placement =
        architecture_wall_placement(document, parameters);
    const double width = std::max(300.0, param(parameters, "width", 1000.0));
    const double height = std::max(300.0, param(parameters, "height", 1300.0));
    const double frame_width = std::clamp(
        param(parameters, "frame_width", 60.0), 15.0,
        std::min(width, height) * 0.22);
    const double frame_depth = std::clamp(
        param(parameters, "frame_thickness", 60.0), 15.0, 300.0);
    const double center = placement.attached
        ? param(parameters, "distance_along_wall", 1800.0) : 0.0;
    const double sill = placement.attached
        ? std::max(0.0, param(parameters, "distance_from_floor", 900.0))
        : 0.0;
    const double split = std::clamp(param(parameters, "factor", 0.5), 0.1, 0.9);
    const bool wood = param(parameters, "wood_construction", 0.0) >= 0.5;
    const bool reverse = param(parameters, "reverse", 0.0) >= 0.5;
    const double sill_projection = std::clamp(
        param(parameters, "sill_projection", 50.0), 0.0, 500.0);
    const double sill_thickness = std::clamp(
        param(parameters, "sill_thickness", 30.0), 5.0, 100.0);
    const double sill_side_extension = std::clamp(
        param(parameters, "sill_side_extension", 50.0), 0.0, 500.0);
    const int vertical_bars = std::clamp(
        static_cast<int>(param(parameters, "vertical_bars", 0.0)), 0, 12);
    const int horizontal_bars = std::clamp(
        static_cast<int>(param(parameters, "horizontal_bars", 0.0)), 0, 12);

    const double left = center - width * 0.5;
    const double y = -frame_depth * 0.5;
    const double opening_width = width - 2.0 * frame_width;
    const double opening_height = height - 2.0 * frame_width;
    const double mullion_width = std::clamp(frame_width * 0.55, 18.0, 50.0);
    const double mullion_x = left + frame_width
        + opening_width * split - mullion_width * 0.5;
    const Color frame_color = wood
        ? Color{0.54f, 0.34f, 0.17f}
        : Color{0.66f, 0.52f, 0.68f};
    const Color glass_color{0.23f, 0.82f, 0.90f};
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    const auto add_box = [&parts](const std::string& name,
                                  double x, double by, double z,
                                  double dx, double dy, double dz,
                                  Color color) {
        parts.push_back(component_solid(
            name, component_box(x, by, z, dx, dy, dz), color));
    };

    add_box("Window Frame Left", left, y, sill,
            frame_width, frame_depth, height, frame_color);
    add_box("Window Frame Right", left + width - frame_width, y, sill,
            frame_width, frame_depth, height, frame_color);
    add_box("Window Frame Bottom", left + frame_width, y, sill,
            opening_width, frame_depth, frame_width, frame_color);
    add_box("Window Frame Top", left + frame_width, y,
            sill + height - frame_width,
            opening_width, frame_depth, frame_width, frame_color);
    add_box("Window Center Mullion", mullion_x, y - 1.0,
            sill + frame_width,
            mullion_width, frame_depth + 2.0, opening_height, frame_color);

    const double glass_depth = std::clamp(frame_depth * 0.12, 4.0, 12.0);
    const double glass_y = y + (frame_depth - glass_depth) * 0.5;
    add_box("Window Glass Left", left + frame_width, glass_y,
            sill + frame_width,
            std::max(1.0, mullion_x - (left + frame_width)),
            glass_depth, opening_height, glass_color);
    add_box("Window Glass Right", mullion_x + mullion_width, glass_y,
            sill + frame_width,
            std::max(1.0, left + width - frame_width
                - (mullion_x + mullion_width)),
            glass_depth, opening_height, glass_color);

    const double bar = std::clamp(frame_width * 0.24, 8.0, 24.0);
    for (int index = 1; index <= vertical_bars; ++index) {
        const double x = left + frame_width
            + opening_width * index / (vertical_bars + 1.0) - bar * 0.5;
        add_box("Window Vertical Muntin " + std::to_string(index),
                x, y - 2.0, sill + frame_width,
                bar, frame_depth + 4.0, opening_height, frame_color);
    }
    for (int index = 1; index <= horizontal_bars; ++index) {
        const double z = sill + frame_width
            + opening_height * index / (horizontal_bars + 1.0) - bar * 0.5;
        add_box("Window Horizontal Muntin " + std::to_string(index),
                left + frame_width, y - 2.0, z,
                opening_width, frame_depth + 4.0, bar, frame_color);
    }

    const double wall_depth = placement.attached
        ? (placement.along_x
            ? placement.maximum.y - placement.minimum.y
            : placement.maximum.x - placement.minimum.x)
        : frame_depth;
    const double sill_depth = wall_depth + sill_projection;
    const double sill_y = placement.local_inward_sign > 0.0
        ? -wall_depth * 0.5
        : -wall_depth * 0.5 - sill_projection;
    parts.push_back(component_solid(
        "Window Sill",
        window_sill_shape(
            left - sill_side_extension,
            sill_y,
            sill - sill_thickness + 2.0,
            width + 2.0 * sill_side_extension,
            sill_depth,
            sill_thickness),
        {0.96f, 0.95f, 0.92f}));

    const unsigned long glass_material_id =
        ensure_room_window_glass_material(document);
    const unsigned long sill_material_id =
        ensure_room_window_sill_material(document);
    const Material* glass_material =
        document.FindMaterial(glass_material_id);
    const Material* sill_material = document.FindMaterial(sill_material_id);
    if (!glass_material || !sill_material) return {};
    for (auto& part : parts) {
        if (!part) continue;
        if (part->GetName().rfind("Window Glass ", 0) == 0) {
            part->SetMaterial(*glass_material);
            part->SetMaterialId(glass_material->id);
        } else if (part->GetName() == "Window Sill") {
            part->SetMaterial(*sill_material);
            part->SetMaterialId(sill_material->id);
        }
    }
    if (std::any_of(parts.begin(), parts.end(),
                    [](const auto& part) { return !part; })) {
        return {};
    }
    (void)reverse;
    transform_architecture_parts(parts, placement);
    return parts;
}

Color door_material_color(int material) {
    switch (material) {
    case 1: return {0.42f, 0.23f, 0.10f};
    case 2: return {0.72f, 0.53f, 0.28f};
    case 3: return {0.88f, 0.88f, 0.86f};
    case 4: return {0.48f, 0.50f, 0.52f};
    default: return {0.66f, 0.44f, 0.24f};
    }
}

std::vector<std::unique_ptr<CAlfaObject>> door_parts(
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters) {
    const ArchitectureWallPlacement placement =
        architecture_wall_placement(document, parameters);
    const int type = std::clamp(
        static_cast<int>(param(parameters, "type", 0.0)), 0, 2);
    const double center = placement.attached
        ? param(parameters, "distance_along_wall", 1270.0) : 0.0;
    const double width = std::max(500.0, param(parameters, "width", 900.0));
    const double height = std::max(1000.0, param(parameters, "height", 2000.0));
    const double floor = placement.attached
        ? std::max(0.0, param(parameters, "distance_to_floor", 100.0))
        : 0.0;
    const bool reverse = param(parameters, "reverse", 0.0) >= 0.5;
    const bool double_door = param(parameters, "double_door", 0.0) >= 0.5;
    const bool door_frame = param(parameters, "door_frame", 1.0) >= 0.5;
    const int handle_side = std::clamp(
        static_cast<int>(param(parameters, "handle_side", 1.0)), 0, 1);
    const int handle_type = std::clamp(
        static_cast<int>(param(parameters, "handle_type", 0.0)), 0, 2);
    const int material = std::clamp(
        static_cast<int>(param(parameters, "material", 0.0)), 0, 4);

    const double frame_width = 60.0;
    const double frame_depth = 100.0;
    const double leaf_depth = 42.0;
    const double left = center - width * 0.5;
    const double y = -leaf_depth * 0.5;
    const Color wood_color = door_material_color(material);
    const Color frame_color = material == 4
        ? Color{0.48f, 0.50f, 0.52f} : Color{0.59f, 0.39f, 0.20f};
    const Color glass_color{0.34f, 0.78f, 0.86f};
    const Color handle_color{0.70f, 0.71f, 0.72f};
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    const auto add_box = [&parts](const std::string& name,
                                  double x, double by, double z,
                                  double dx, double dy, double dz,
                                  Color color) {
        parts.push_back(component_solid(
            name, component_box(x, by, z, dx, dy, dz), color));
    };

    if (door_frame) {
        add_box("Door Frame Left", left - frame_width, -frame_depth * 0.5, floor,
                frame_width, frame_depth, height + frame_width, frame_color);
        add_box("Door Frame Right", left + width, -frame_depth * 0.5, floor,
                frame_width, frame_depth, height + frame_width, frame_color);
        add_box("Door Frame Top", left, -frame_depth * 0.5, floor + height,
                width, frame_depth, frame_width, frame_color);
    }

    if (type != 2) {
        const int leaf_count = double_door ? 2 : 1;
        const double gap = double_door ? 5.0 : 0.0;
        const double leaf_width = (width - gap) / leaf_count;
        for (int leaf = 0; leaf < leaf_count; ++leaf) {
            const double leaf_left = left + leaf * (leaf_width + gap);
            const std::string prefix = leaf_count == 2
                ? (leaf == 0 ? "Door Left Leaf " : "Door Right Leaf ")
                : "Door Leaf ";
            if (type == 0) {
                const double rail = std::clamp(width * 0.09, 55.0, 95.0);
                add_box(prefix + "Left Rail", leaf_left, y, floor,
                        rail, leaf_depth, height, wood_color);
                add_box(prefix + "Right Rail", leaf_left + leaf_width - rail, y, floor,
                        rail, leaf_depth, height, wood_color);
                add_box(prefix + "Bottom Rail", leaf_left + rail, y, floor,
                        leaf_width - 2.0 * rail, leaf_depth, rail, wood_color);
                add_box(prefix + "Top Rail", leaf_left + rail, y,
                        floor + height - rail,
                        leaf_width - 2.0 * rail, leaf_depth, rail, wood_color);
                add_box(prefix + "Glass", leaf_left + rail,
                        y + (leaf_depth - 8.0) * 0.5, floor + rail,
                        leaf_width - 2.0 * rail, 8.0,
                        height - 2.0 * rail, glass_color);
            } else {
                add_box(prefix + "Panel", leaf_left, y, floor,
                        leaf_width, leaf_depth, height, wood_color);
            }
        }

        if (handle_type != 2) {
            const double handle_x = handle_side == 0
                ? left + 90.0 : left + width - 110.0;
            const double handle_z = floor + std::clamp(height * 0.48, 850.0, 1100.0);
            if (handle_type == 0) {
                const double handle_y = reverse ? y + leaf_depth : y - 28.0;
                add_box("Door Round Handle", handle_x, handle_y, handle_z,
                        24.0, 28.0, 24.0, handle_color);
            } else {
                const double handle_y = reverse ? y + leaf_depth : y - 28.0;
                add_box("Door Lever Handle", handle_x, handle_y, handle_z,
                        90.0, 28.0, 18.0, handle_color);
            }
        }
    }
    if (parts.empty() || std::any_of(parts.begin(), parts.end(),
                    [](const auto& part) { return !part; })) {
        return {};
    }
    transform_architecture_parts(parts, placement);
    return parts;
}

NikaKitchenDefinition nika_kitchen_definition(
    const std::vector<ToolParameter>& parameters) {
    NikaKitchenDefinition result;
    result.width = std::max(1600.0, param(parameters, "width", 2600.0));
    result.base_height = std::max(
        500.0, param(parameters, "base_height", 800.0));
    result.base_depth = std::max(
        300.0, param(parameters, "base_depth", 500.0));
    result.upper_height = std::max(
        300.0, param(parameters, "upper_height", 800.0));
    result.upper_depth = std::max(
        180.0, param(parameters, "upper_depth", 300.0));
    result.wall_gap = std::max(
        100.0, param(parameters, "wall_gap", 450.0));
    result.worktop_thickness = std::clamp(
        param(parameters, "worktop_thickness", 38.0), 10.0, 100.0);
    result.worktop_front_radius = std::clamp(
        param(parameters, "worktop_front_radius", 20.0), 0.0, 50.0);
    result.leg_height = std::clamp(
        param(parameters, "leg_height", 100.0), 20.0,
        result.base_height * 0.45);
    result.panel_thickness = std::clamp(
        param(parameters, "panel_thickness", 18.0), 5.0,
        std::min({result.base_depth, result.upper_depth,
                  result.base_height - result.leg_height,
                  result.upper_height}) * 0.15);
    result.facade_style = std::clamp(
        static_cast<int>(param(parameters, "facade_style", 4.0)), 0, 4);
    result.handle_type = std::clamp(
        static_cast<int>(param(parameters, "handle_type", 0.0)), 0, 3);
    const std::array<const char*, 8> door_angle_ids{
        "lower_1_door_angle", "lower_4_left_door_angle",
        "lower_4_right_door_angle", "upper_1_door_angle",
        "upper_2_door_angle", "upper_3_door_angle",
        "upper_4_left_door_angle", "upper_4_right_door_angle"};
    for (size_t index = 0; index < door_angle_ids.size(); ++index) {
        result.door_open_angles[index] = std::clamp(
            param(parameters, door_angle_ids[index], 0.0), 0.0, 150.0);
    }
    result.open_drawer = std::clamp(
        static_cast<int>(param(parameters, "open_drawer", 0.0)), 0, 8);
    result.pullout_distance = std::clamp(
        param(parameters, "pullout_distance", 350.0),
        0.0, result.base_depth * 0.9);
    return result;
}

int nika_module_slot(const std::string& name) {
    for (int module = 1; module <= 4; ++module) {
        if (name.rfind("Nika Lower " + std::to_string(module), 0) == 0) {
            return module - 1;
        }
        if (name.rfind("Nika Upper " + std::to_string(module), 0) == 0) {
            return 4 + module - 1;
        }
    }
    return -1;
}

std::vector<ParametricParameterValue> nika_cabinet_parameters(
    const KitchenCabinetDefinition& definition) {
    return {
        {"body_type", static_cast<double>(definition.body_type)},
        {"facade_type", static_cast<double>(definition.facade_type)},
        {"facade_style", static_cast<double>(definition.facade_style)},
        {"width", definition.width}, {"height", definition.height},
        {"depth", definition.depth},
        {"panel_thickness", definition.panel_thickness},
        {"shelf_count", static_cast<double>(definition.shelf_count)},
        {"door_open_angle", definition.door_open_angle},
        {"left_door_open_angle", definition.left_door_open_angle},
        {"right_door_open_angle", definition.right_door_open_angle},
        {"handle_orientation", 1.0}};
}

std::vector<ParametricParameterValue> nika_drawer_box_parameters(
    const NikaKitchenDefinition& kitchen,
    double width,
    int module) {
    const double carcass_height = std::max(
        100.0, kitchen.base_height - kitchen.leg_height);
    int open_drawer = 0;
    const int first_global_drawer = module == 2 ? 1 : 5;
    if (kitchen.open_drawer >= first_global_drawer
        && kitchen.open_drawer < first_global_drawer + 4) {
        open_drawer = kitchen.open_drawer - first_global_drawer + 1;
    }
    return {
        {"width", width}, {"height", carcass_height},
        {"depth", kitchen.base_depth},
        {"facade_type", static_cast<double>(kitchen.facade_style)},
        {"handle_type", static_cast<double>(kitchen.handle_type)},
        {"panel_thickness", kitchen.panel_thickness},
        {"drawer_side_thickness", 12.0},
        {"drawer_bottom_thickness", 6.0},
        {"slide_clearance", 13.0}, {"drawer_count", 4.0},
        {"drawer_height_1", 200.0}, {"drawer_height_2", 200.0},
        {"drawer_height_3", 200.0}, {"drawer_height_4", 200.0},
        {"drawer_height_5", 200.0}, {"drawer_height_6", 200.0},
        {"make_legs", 1.0}, {"leg_height", kitchen.leg_height},
        {"open_drawer", static_cast<double>(open_drawer)},
        {"pullout_distance", kitchen.pullout_distance}
    };
}

std::vector<ParametricParameterValue> nika_showcase_parameters(
    const KitchenCabinetDefinition& definition) {
    return {
        {"showcase_facade_type", 0.0},
        {"showcase_fill", static_cast<double>(
            KitchenCabinetShowcaseFill::Glass)},
        {"width", definition.width}, {"height", definition.height},
        {"depth", definition.depth},
        {"panel_thickness", definition.panel_thickness},
        {"shelf_count", static_cast<double>(definition.shelf_count)},
        {"door_open_angle", definition.door_open_angle},
        {"door_hinge_side", 0.0}, {"handle_orientation", 1.0}
    };
}

std::vector<unsigned long> add_nika_hierarchy(
    CAlfaDoc& document,
    std::vector<std::unique_ptr<CAlfaObject>> parts,
    const NikaKitchenDefinition& kitchen) {
    std::array<std::vector<unsigned long>, 8> module_part_ids;
    unsigned long worktop_id = 0;
    size_t added_part_count = 0;
    for (auto& part : parts) {
        if (!part) continue;
        const int slot = nika_module_slot(part->GetName());
        const bool worktop = part->GetName() == "Nika Worktop";
        document.AddObject(std::move(part));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            if (slot >= 0) {
                module_part_ids[static_cast<size_t>(slot)].push_back(
                    added->m_id);
            } else if (worktop) {
                worktop_id = added->m_id;
            }
        }
        if (++added_part_count % 6 == 0) {
            process_progressive_furniture_creation();
        }
    }

    const std::array<double, 4> ratios{
        600.0 / 2600.0, 600.0 / 2600.0,
        600.0 / 2600.0, 800.0 / 2600.0};
    const double upper_z = kitchen.base_height
        + kitchen.worktop_thickness + kitchen.wall_gap;
    double module_left = -kitchen.width * 0.5;
    std::vector<unsigned long> outer_ids;
    outer_ids.reserve(9);
    for (int module = 0; module < 4; ++module) {
        const double width = kitchen.width * ratios[static_cast<size_t>(module)];
        const double center_x = module_left + width * 0.5;
        for (int level = 0; level < 2; ++level) {
            const int slot = level * 4 + module;
            const bool drawer_box = level == 0
                && (module == 1 || module == 2);
            if (drawer_box) {
                auto drawers = std::make_unique<CDrawerBoxFurniture>(
                    "Nika Lower Drawer Box " + std::to_string(module + 1),
                    std::move(module_part_ids[static_cast<size_t>(slot)]));
                drawers->SetParametricDefinition(
                    "drawer_box",
                    nika_drawer_box_parameters(
                        kitchen, width, module + 1));
                CAssembled::TransformMatrix transform{
                    1.0, 0.0, 0.0, center_x,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0};
                drawers->SetAssemblyTransform(transform);
                document.AddObject(std::move(drawers));
                if (CAlfaObject* added = document.GetSelectedObject()) {
                    outer_ids.push_back(added->m_id);
                }
                continue;
            }
            KitchenCabinetDefinition definition;
            const bool showcase = level == 1
                && (module == 0 || module == 2);
            definition.body_type = KitchenCabinetBodyType::Straight;
            definition.facade_type = module == 3
                ? KitchenCabinetFacadeType::DoubleDoor
                : KitchenCabinetFacadeType::SingleDoor;
            definition.facade_style = static_cast<KitchenCabinetFacadeStyle>(
                std::clamp(kitchen.facade_style, 0, 4));
            if (showcase) {
                definition.facade_style = KitchenCabinetFacadeStyle::Screen;
                definition.showcase_fill = KitchenCabinetShowcaseFill::Glass;
            }
            definition.width = width;
            definition.depth = level == 0
                ? kitchen.base_depth : kitchen.upper_depth;
            definition.height = level == 0
                ? std::max(100.0, kitchen.base_height - kitchen.leg_height)
                : kitchen.upper_height;
            definition.panel_thickness = kitchen.panel_thickness;
            definition.shelf_count = level == 0 ? 0 : 1;

            auto cabinet = std::make_unique<CKitchenCabinet>(
                std::string(level == 0 ? "Nika Lower Cabinet "
                                       : "Nika Upper Cabinet ")
                    + std::to_string(module + 1),
                std::move(module_part_ids[static_cast<size_t>(slot)]),
                definition);
            cabinet->SetParametricDefinition(
                showcase ? "cabinet_showcase" : "cabinet_advanced",
                showcase ? nika_showcase_parameters(definition)
                         : nika_cabinet_parameters(definition));
            CAssembled::TransformMatrix transform{
                1.0, 0.0, 0.0, center_x,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0,
                    level == 0 ? kitchen.leg_height : upper_z,
                0.0, 0.0, 0.0, 1.0};
            cabinet->SetAssemblyTransform(transform);
            document.AddObject(std::move(cabinet));
            if (CAlfaObject* added = document.GetSelectedObject()) {
                outer_ids.push_back(added->m_id);
            }
        }
        module_left += width;
    }
    if (worktop_id != 0) outer_ids.push_back(worktop_id);
    return outer_ids;
}

void create_nika_kitchen(CAlfaDoc& document,
                         const std::vector<ToolParameter>& parameters) {
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    const NikaKitchenDefinition kitchen = nika_kitchen_definition(parameters);
    auto parts = CNikaKitchenFurniture::BuildParts(
        kitchen);
    if (parts.empty()) {
        return;
    }
    assign_furniture_materials(
        document, parts, parameters, "kitchen_nika_260");
    std::vector<unsigned long> ids = add_nika_hierarchy(
        document, std::move(parts), kitchen);
    document.AddObject(std::make_unique<CNikaKitchenFurniture>(
        "Kitchen Nika-260", std::move(ids)));
    process_progressive_furniture_creation();
}

void rebuild_nika_kitchen(CAlfaDoc& document,
                          size_t assembly_index,
                          const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size()) {
        return;
    }
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly) {
        return;
    }
    const NikaKitchenDefinition kitchen = nika_kitchen_definition(parameters);
    auto replacements = CNikaKitchenFurniture::BuildParts(kitchen);
    if (replacements.empty()) {
        return;
    }
    assign_furniture_materials(
        document, replacements, parameters, "kitchen_nika_260");
    const unsigned long assembly_id = assembly->m_id;
    const int assembly_layer = assembly->m_LayerID;
    const std::vector<unsigned long> old_ids = assembly->GetElementIds();
    std::function<void(unsigned long)> remove_tree;
    remove_tree = [&](unsigned long id) {
        const size_t index = document.FindObjectIndexById(id);
        if (index >= objects.size() || !objects[index]) return;
        if (const auto* group = dynamic_cast<const CGroup*>(objects[index].get())) {
            const std::vector<unsigned long> children = group->GetElementIds();
            for (unsigned long child_id : children) remove_tree(child_id);
        }
        if (index != assembly_index) objects[index].reset();
    };
    for (unsigned long id : old_ids) remove_tree(id);

    std::vector<unsigned long> new_ids = add_nika_hierarchy(
        document, std::move(replacements), kitchen);
    assembly = assembly_index < objects.size()
        ? dynamic_cast<CAssembled*>(objects[assembly_index].get()) : nullptr;
    if (!assembly || assembly->m_id != assembly_id) {
        return;
    }
    assembly->m_LayerID = assembly_layer;
    assembly->SetElementIds(std::move(new_ids));
    document.SelectObjectById(assembly_id);
}

CornerKitchenDefinition corner_kitchen_definition(
    const std::vector<ToolParameter>& parameters) {
    CornerKitchenDefinition result;
    result.left_length = std::max(
        1800.0, param(parameters, "left_length", 2700.0));
    result.right_length = std::max(
        1200.0, param(parameters, "right_length", 1800.0));
    result.base_height = std::max(
        500.0, param(parameters, "base_height", 800.0));
    result.base_depth = std::max(
        350.0, param(parameters, "base_depth", 570.0));
    result.upper_height = std::max(
        300.0, param(parameters, "upper_height", 800.0));
    result.upper_depth = std::max(
        180.0, param(parameters, "upper_depth", 300.0));
    result.wall_gap = std::max(
        100.0, param(parameters, "wall_gap", 600.0));
    result.worktop_thickness = std::clamp(
        param(parameters, "worktop_thickness", 38.0), 10.0, 100.0);
    result.worktop_front_radius = std::clamp(
        param(parameters, "worktop_front_radius", 20.0), 0.0, 50.0);
    result.leg_height = std::clamp(
        param(parameters, "leg_height", 100.0), 20.0,
        result.base_height * 0.45);
    result.panel_thickness = std::clamp(
        param(parameters, "panel_thickness", 18.0), 5.0, 50.0);
    result.facade_style = std::clamp(
        static_cast<int>(param(parameters, "facade_style", 2.0)), 0, 2);
    result.handle_type = std::clamp(
        static_cast<int>(param(parameters, "handle_type", 0.0)), 0, 3);
    const std::array<const char*, 8> door_suffixes{
        "lower_1_door_angle", "lower_4_left_door_angle",
        "lower_4_right_door_angle", "upper_1_door_angle",
        "upper_2_door_angle", "upper_3_door_angle",
        "upper_4_left_door_angle", "upper_4_right_door_angle"};
    for (size_t index = 0; index < door_suffixes.size(); ++index) {
        const std::string left_id = std::string("left_") + door_suffixes[index];
        const std::string right_id = std::string("right_") + door_suffixes[index];
        result.left_door_open_angles[index] = std::clamp(
            param(parameters, left_id.c_str(), 0.0),
            0.0, 150.0);
        result.right_door_open_angles[index] = std::clamp(
            param(parameters, right_id.c_str(), 0.0),
            0.0, 150.0);
    }
    result.lower_corner_door_angle = std::clamp(
        param(parameters, "lower_corner_door_angle", 0.0), 0.0, 150.0);
    result.upper_corner_door_angle = std::clamp(
        param(parameters, "upper_corner_door_angle", 0.0), 0.0, 150.0);
    return result;
}

void create_corner_kitchen(CAlfaDoc& document,
                           const std::vector<ToolParameter>& parameters) {
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    std::vector<unsigned long> ids;
    ids.reserve(128);
    size_t added_part_count = 0;
    const bool built = CCornerKitchenFurniture::BuildPartsIncremental(
        corner_kitchen_definition(parameters),
        [&](std::unique_ptr<CAlfaObject> part) {
            std::vector<std::unique_ptr<CAlfaObject>> batch;
            batch.push_back(std::move(part));
            assign_furniture_materials(
                document, batch, parameters, "kitchen_corner");
            document.AddObject(std::move(batch.front()));
            if (CAlfaObject* added = document.GetSelectedObject()) {
                ids.push_back(added->m_id);
            }
            if (++added_part_count % 6 == 0) {
                process_progressive_furniture_creation();
            }
        });
    if (!built || ids.empty()) {
        // A failed late OCCT operation must not leave half a kitchen behind.
        auto& objects = document.GetObjects();
        for (unsigned long id : ids) {
            const size_t index = document.FindObjectIndexById(id);
            if (index < objects.size()) objects[index].reset();
        }
        process_progressive_furniture_creation();
        return;
    }
    document.AddObject(std::make_unique<CCornerKitchenFurniture>(
        "Corner Kitchen", std::move(ids)));
    process_progressive_furniture_creation();
}

void rebuild_corner_kitchen(CAlfaDoc& document,
                            size_t assembly_index,
                            const std::vector<ToolParameter>& parameters) {
    auto& objects = document.GetObjects();
    if (assembly_index >= objects.size()) {
        return;
    }
    auto* assembly = dynamic_cast<CAssembled*>(objects[assembly_index].get());
    if (!assembly) {
        return;
    }
    auto replacements = CCornerKitchenFurniture::BuildParts(
        corner_kitchen_definition(parameters));
    if (replacements.empty()) {
        return;
    }
    assign_furniture_materials(
        document, replacements, parameters, "kitchen_corner");
    const unsigned long assembly_id = assembly->m_id;
    const std::vector<unsigned long> old_ids = assembly->GetElementIds();
    std::vector<unsigned long> new_ids;
    new_ids.reserve(replacements.size());
    const size_t common_count = std::min(old_ids.size(), replacements.size());
    for (size_t index = 0; index < common_count; ++index) {
        const size_t part_index = document.FindObjectIndexById(old_ids[index]);
        if (part_index >= objects.size() || !objects[part_index]) {
            return;
        }
        CAlfaObject& old_part = *objects[part_index];
        replacements[index]->m_id = old_part.m_id;
        replacements[index]->m_LayerID = old_part.m_LayerID;
        if (replacements[index]->GetMaterialId() == 0) {
            replacements[index]->SetMaterial(old_part.GetMaterial());
            replacements[index]->SetMaterialId(old_part.GetMaterialId());
        }
        copy_solid_surface_appearance(old_part, *replacements[index]);
        replacements[index]->SetVisible(old_part.IsVisible());
        objects[part_index] = std::move(replacements[index]);
        new_ids.push_back(old_ids[index]);
    }
    for (size_t index = common_count; index < old_ids.size(); ++index) {
        const size_t part_index = document.FindObjectIndexById(old_ids[index]);
        if (part_index < objects.size()) {
            objects[part_index].reset();
        }
    }
    for (size_t index = common_count; index < replacements.size(); ++index) {
        document.AddObject(std::move(replacements[index]));
        if (CAlfaObject* added = document.GetSelectedObject()) {
            new_ids.push_back(added->m_id);
        }
    }
    assembly = assembly_index < objects.size()
        ? dynamic_cast<CAssembled*>(objects[assembly_index].get()) : nullptr;
    if (!assembly || assembly->m_id != assembly_id) {
        return;
    }
    assembly->SetElementIds(std::move(new_ids));
    document.SelectObjectById(assembly_id);
}

struct TableGeometry {
    double width;
    double depth;
    double height;
    double top;
    double leg;
    double leg_taper;
    double inset;
    double apron_height;
    double apron_thickness;
    double apron_edge_radius;
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
        param(parameters, "leg_taper", 8.0),
        param(parameters, "leg_inset", 40.0),
        param(parameters, "apron_height", 60.0),
        param(parameters, "apron_thickness", 20.0),
        param(parameters, "apron_edge_radius", 2.0),
        param(parameters, "arc_bulge", 100.0),
        param(parameters, "top_shape", 0.0) >= 0.5};
    result.top = std::clamp(result.top, 1.0, result.height - 1.0);
    result.leg = std::clamp(result.leg, 1.0, std::min(result.width, result.depth) * 0.4);
    result.leg_taper = std::clamp(result.leg_taper, 0.0, result.leg * 0.49);
    result.inset = std::clamp(
        result.inset, 0.0,
        std::max(0.0, (std::min(result.width, result.depth) - result.leg) * 0.5));
    result.apron_height = std::clamp(result.apron_height, 1.0, result.height - result.top);
    result.apron_thickness = std::clamp(
        result.apron_thickness, 1.0, std::min(result.leg, result.depth * 0.25));
    result.apron_edge_radius = std::clamp(
        result.apron_edge_radius, 0.0,
        std::min(result.apron_thickness, result.apron_height) * 0.49);
    result.arc_bulge = std::clamp(result.arc_bulge, 1.0, result.width);
    return result;
}

TopoDS_Shape table_box_shape(double x, double y, double z,
                             double width, double depth, double height) {
    BRepPrimAPI_MakeBox builder(gp_Pnt(x, y, z), width, depth, height);
    builder.Build();
    return builder.IsDone() ? builder.Shape() : TopoDS_Shape();
}

TopoDS_Shape table_leg_shape(double x, double y, double height,
                             double size, double taper) {
    try {
        BRepBuilderAPI_MakePolygon bottom;
        bottom.Add(gp_Pnt(x + taper, y + taper, 0.0));
        bottom.Add(gp_Pnt(x + size - taper, y + taper, 0.0));
        bottom.Add(gp_Pnt(x + size - taper, y + size - taper, 0.0));
        bottom.Add(gp_Pnt(x + taper, y + size - taper, 0.0));
        bottom.Close();

        BRepBuilderAPI_MakePolygon top;
        top.Add(gp_Pnt(x, y, height));
        top.Add(gp_Pnt(x + size, y, height));
        top.Add(gp_Pnt(x + size, y + size, height));
        top.Add(gp_Pnt(x, y + size, height));
        top.Close();
        if (!bottom.IsDone() || !top.IsDone()) {
            return {};
        }

        BRepOffsetAPI_ThruSections loft(true, false, 1.0e-7);
        loft.CheckCompatibility(true);
        loft.AddWire(bottom.Wire());
        loft.AddWire(top.Wire());
        loft.Build();
        return loft.IsDone() ? loft.Shape() : TopoDS_Shape();
    } catch (...) {
        return {};
    }
}

bool table_edge_joins_points(const TopoDS_Edge& edge,
                             const gp_Pnt& first,
                             const gp_Pnt& second,
                             double tolerance = 1.0e-5) {
    TopoDS_Vertex first_vertex;
    TopoDS_Vertex last_vertex;
    TopExp::Vertices(edge, first_vertex, last_vertex);
    if (first_vertex.IsNull() || last_vertex.IsNull()) return false;
    const gp_Pnt edge_first = BRep_Tool::Pnt(first_vertex);
    const gp_Pnt edge_last = BRep_Tool::Pnt(last_vertex);
    return (edge_first.Distance(first) <= tolerance
            && edge_last.Distance(second) <= tolerance)
        || (edge_first.Distance(second) <= tolerance
            && edge_last.Distance(first) <= tolerance);
}

TopoDS_Shape round_table_top(const TopoDS_Shape& source,
                             double bottom_z,
                             double top_z) {
    if (source.IsNull()) return {};
    constexpr double corner_radius = 15.0;
    constexpr double face_radius = 5.0;
    constexpr double tolerance = 1.0e-5;

    // First operation: round the four plan-view corners by their vertical
    // edges. Keep the original endpoint pairs because the other three edges
    // remain unchanged after each individual fillet.
    std::vector<std::pair<gp_Pnt, gp_Pnt>> corner_edges;
    for (TopExp_Explorer explorer(source, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        TopoDS_Vertex first_vertex;
        TopoDS_Vertex last_vertex;
        TopExp::Vertices(edge, first_vertex, last_vertex);
        if (first_vertex.IsNull() || last_vertex.IsNull()) continue;
        const gp_Pnt first = BRep_Tool::Pnt(first_vertex);
        const gp_Pnt last = BRep_Tool::Pnt(last_vertex);
        const bool vertical = std::abs(first.X() - last.X()) <= tolerance
            && std::abs(first.Y() - last.Y()) <= tolerance;
        const bool spans_top =
            (std::abs(first.Z() - bottom_z) <= tolerance
             && std::abs(last.Z() - top_z) <= tolerance)
            || (std::abs(last.Z() - bottom_z) <= tolerance
                && std::abs(first.Z() - top_z) <= tolerance);
        if (vertical && spans_top) {
            corner_edges.emplace_back(first, last);
        }
    }

    TopoDS_Shape rounded_corners = source;
    for (const auto& endpoints : corner_edges) {
        TopoDS_Edge target;
        for (TopExp_Explorer explorer(rounded_corners, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            if (table_edge_joins_points(
                    edge, endpoints.first, endpoints.second)) {
                target = edge;
                break;
            }
        }
        if (target.IsNull()) continue;
        try {
            BRepFilletAPI_MakeFillet fillet(rounded_corners);
            fillet.Add(corner_radius, target);
            fillet.Build();
            if (fillet.IsDone() && !fillet.Shape().IsNull()) {
                rounded_corners = fillet.Shape();
            }
        } catch (const Standard_Failure&) {
            // Continue with the remaining corners.
        }
    }

    // Second operation: soften the complete perimeter of the upper face.
    try {
        BRepFilletAPI_MakeFillet fillet(rounded_corners);
        int edge_count = 0;
        for (TopExp_Explorer explorer(rounded_corners, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            TopoDS_Vertex first_vertex;
            TopoDS_Vertex last_vertex;
            TopExp::Vertices(edge, first_vertex, last_vertex);
            if (first_vertex.IsNull() || last_vertex.IsNull()) continue;
            const gp_Pnt first = BRep_Tool::Pnt(first_vertex);
            const gp_Pnt last = BRep_Tool::Pnt(last_vertex);
            if (std::abs(first.Z() - top_z) <= tolerance
                && std::abs(last.Z() - top_z) <= tolerance) {
                fillet.Add(face_radius, edge);
                ++edge_count;
            }
        }
        if (edge_count > 0) {
            fillet.Build();
            if (fillet.IsDone() && !fillet.Shape().IsNull()) {
                return fillet.Shape();
            }
        }
    } catch (const Standard_Failure&) {
    }
    return rounded_corners;
}

TopoDS_Shape table_top_shape(const TableGeometry& geometry) {
    const double z = geometry.height - geometry.top;
    if (!geometry.arc_top) {
        return round_table_top(table_box_shape(
            -geometry.width * 0.5, -geometry.depth * 0.5, z,
            geometry.width, geometry.depth, geometry.top),
            z, geometry.height);
    }
    try {
        const gp_Pnt front_left(-geometry.width * 0.5, -geometry.depth * 0.5, z);
        const gp_Pnt front_right(geometry.width * 0.5, -geometry.depth * 0.5, z);
        const gp_Pnt back_right(geometry.width * 0.5, geometry.depth * 0.5, z);
        const gp_Pnt back_left(-geometry.width * 0.5, geometry.depth * 0.5, z);
        GC_MakeArcOfCircle right_arc(
            front_right,
            gp_Pnt(geometry.width * 0.5 + geometry.arc_bulge, 0.0, z),
            back_right);
        GC_MakeArcOfCircle left_arc(
            back_left,
            gp_Pnt(-geometry.width * 0.5 - geometry.arc_bulge, 0.0, z),
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
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, geometry.top));
        prism.Build();
        return prism.IsDone()
            ? round_table_top(prism.Shape(), z, geometry.height)
            : TopoDS_Shape();
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

std::unique_ptr<CSolid> make_table_apron(
    const std::string& name, TopoDS_Shape shape, Color color, double edge_radius) {
    auto apron = make_table_solid(name, std::move(shape), color);
    if (!apron) {
        return nullptr;
    }
    if (edge_radius > 0.0001 && !apply_fillet_all_edges(*apron, edge_radius, edge_radius)) {
        return nullptr;
    }
    return apron;
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
    FurnitureMaterialFactory::EnsureStandardMaterials(document);
    const TableGeometry g = table_geometry(parameters);
    const Color wood{0.58f, 0.32f, 0.15f};
    const Color apron_color{0.72f, 0.16f, 0.12f};
    const double leg_height = g.height - g.top;
    const double left = -g.width * 0.5 + g.inset;
    const double right = g.width * 0.5 - g.inset - g.leg;
    const double front = -g.depth * 0.5 + g.inset;
    const double back = g.depth * 0.5 - g.inset - g.leg;
    const double apron_z = leg_height - g.apron_height;
    const double frame_left = left - g.apron_thickness;
    const double frame_right = right + g.leg + g.apron_thickness;
    const double frame_front = front - g.apron_thickness;
    const double frame_back = back + g.leg + g.apron_thickness;
    const double long_length = std::max(1.0, frame_right - frame_left);
    const double short_length = std::max(1.0, frame_back - frame_front - 2.0 * g.apron_thickness);

    std::vector<std::unique_ptr<CAlfaObject>> parts;
    auto top = make_table_solid("Table Top", table_top_shape(g), wood);
    auto leg = make_table_solid(
        "Table Leg", table_leg_shape(left, front, leg_height, g.leg, g.leg_taper), wood);
    if (!top || !leg) return;
    document.EnsureObjectId(*leg);
    CSolid* leg_source = leg.get();
    const unsigned long leg_id = leg->m_id;
    parts.push_back(std::move(top));
    parts.push_back(std::move(leg));
    parts.push_back(make_table_clone(*leg_source, leg_id, "Table Leg Clone 1", right - left, 0.0, 0.0));
    parts.push_back(make_table_clone(*leg_source, leg_id, "Table Leg Clone 2", right - left, back - front, 0.0));
    parts.push_back(make_table_clone(*leg_source, leg_id, "Table Leg Clone 3", 0.0, back - front, 0.0));

    auto long_apron = make_table_apron(
        "Table Long Apron",
        table_box_shape(frame_left, frame_front, apron_z,
                        long_length, g.apron_thickness, g.apron_height),
        apron_color, g.apron_edge_radius);
    if (!long_apron) return;
    document.EnsureObjectId(*long_apron);
    CSolid* long_source = long_apron.get();
    const unsigned long long_id = long_apron->m_id;
    parts.push_back(std::move(long_apron));
    parts.push_back(make_table_clone(
        *long_source, long_id, "Table Long Apron Clone",
        0.0, frame_back - frame_front - g.apron_thickness, 0.0));

    auto short_apron = make_table_apron(
        "Table Short Apron",
        table_box_shape(frame_left, front, apron_z,
                        g.apron_thickness, short_length, g.apron_height),
        apron_color, g.apron_edge_radius);
    if (!short_apron) return;
    document.EnsureObjectId(*short_apron);
    CSolid* short_source = short_apron.get();
    const unsigned long short_id = short_apron->m_id;
    parts.push_back(std::move(short_apron));
    parts.push_back(make_table_clone(
        *short_source, short_id, "Table Short Apron Clone",
        frame_right - frame_left - g.apron_thickness, 0.0, 0.0));

    if (parts.size() != 9
        || std::any_of(parts.begin(), parts.end(),
                       [](const std::unique_ptr<CAlfaObject>& item) { return !item; })) {
        return;
    }
    assign_furniture_materials(document, parts, parameters, "table");
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
    if (replacement.GetMaterialId() == 0) {
        replacement.SetMaterial(old.GetMaterial());
        replacement.SetMaterialId(old.GetMaterialId());
    }
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
    const double apron_z = leg_height - g.apron_height;
    const double frame_left = left - g.apron_thickness;
    const double frame_right = right + g.leg + g.apron_thickness;
    const double frame_front = front - g.apron_thickness;
    const double frame_back = back + g.leg + g.apron_thickness;
    const double long_length = std::max(1.0, frame_right - frame_left);
    const double short_length = std::max(1.0, frame_back - frame_front - 2.0 * g.apron_thickness);

    std::vector<std::unique_ptr<CAlfaObject>> replacements;
    replacements.push_back(make_table_solid("Table Top", table_top_shape(g), wood));
    auto leg = make_table_solid("Table Leg", table_leg_shape(left, front, leg_height, g.leg, g.leg_taper), wood);
    if (!leg) return;
    CSolid* leg_source = leg.get();
    replacements.push_back(std::move(leg));
    replacements.push_back(make_table_clone(*leg_source, ids[1], "Table Leg Clone 1", right - left, 0.0, 0.0));
    replacements.push_back(make_table_clone(*leg_source, ids[1], "Table Leg Clone 2", right - left, back - front, 0.0));
    replacements.push_back(make_table_clone(*leg_source, ids[1], "Table Leg Clone 3", 0.0, back - front, 0.0));
    auto long_apron = make_table_apron(
        "Table Long Apron",
        table_box_shape(frame_left, frame_front, apron_z,
                        long_length, g.apron_thickness, g.apron_height),
        red, g.apron_edge_radius);
    if (!long_apron) return;
    CSolid* long_source = long_apron.get();
    unsigned long long_source_id = migrate_mesh_table ? 0UL : ids[5];
    if (migrate_mesh_table) {
        document.EnsureObjectId(*long_apron);
        long_source_id = long_apron->m_id;
    }
    replacements.push_back(std::move(long_apron));
    replacements.push_back(make_table_clone(*long_source, long_source_id, "Table Long Apron Clone", 0.0, frame_back - frame_front - g.apron_thickness, 0.0));
    auto short_apron = make_table_apron(
        "Table Short Apron",
        table_box_shape(frame_left, front, apron_z,
                        g.apron_thickness, short_length, g.apron_height),
        red, g.apron_edge_radius);
    if (!short_apron) return;
    CSolid* short_source = short_apron.get();
    unsigned long short_source_id = migrate_mesh_table ? 0UL : ids[7];
    if (migrate_mesh_table) {
        document.EnsureObjectId(*short_apron);
        short_source_id = short_apron->m_id;
    }
    replacements.push_back(std::move(short_apron));
    replacements.push_back(make_table_clone(*short_source, short_source_id, "Table Short Apron Clone", frame_right - frame_left - g.apron_thickness, 0.0, 0.0));
    if (std::any_of(replacements.begin(), replacements.end(),
                    [](const std::unique_ptr<CAlfaObject>& item) { return !item; })) return;
    assign_furniture_materials(document, replacements, parameters, "table");

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
        "Fillet Solid",
        {
            {"radius_type", "Radius Type", 0.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Constant", "Variable"}},
            {"radius", "Radius", 1.0, 0.01, 100.0, 0.1},
            {"radius_start", "Start Radius", 1.0, 0.01, 100.0, 0.1},
            {"radius_end", "End Radius", 1.0, 0.01, 100.0, 0.1}
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
            {"radius_type", "Radius Type", 0.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Constant", "Variable"}},
            {"radius", "Radius", 1.0, 0.01, 100.0, 0.1},
            {"radius_start", "Start Radius", 1.0, 0.01, 100.0, 0.1},
            {"radius_end", "End Radius", 1.0, 0.01, 100.0, 0.1}
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
            {"distance", "Distance", 1.0, 0.01, 100.0, 0.1}
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
            {"distance", "Distance", 1.0, -1000.0, 1000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"taper", "Taper Angle", 0.0, -89.0, 89.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle}
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
            {"width.graph", "Width Scale Graph", 1.0, 0.05, 3.0, 0.01, ToolParameterType::Graph},
            {"height.graph", "Height Scale Graph", 1.0, 0.05, 3.0, 0.01, ToolParameterType::Graph},
            {"width.scale.0", "Width Scale 0", 1.0, 0.05, 3.0, 0.01},
            {"width.scale.1", "Width Scale 1", 1.0, 0.05, 3.0, 0.01},
            {"width.scale.2", "Width Scale 2", 1.0, 0.05, 3.0, 0.01},
            {"width.scale.3", "Width Scale 3", 1.0, 0.05, 3.0, 0.01},
            {"width.scale.4", "Width Scale 4", 1.0, 0.05, 3.0, 0.01},
            {"height.scale.0", "Height Scale 0", 1.0, 0.05, 3.0, 0.01},
            {"height.scale.1", "Height Scale 1", 1.0, 0.05, 3.0, 0.01},
            {"height.scale.2", "Height Scale 2", 1.0, 0.05, 3.0, 0.01},
            {"height.scale.3", "Height Scale 3", 1.0, 0.05, 3.0, 0.01},
            {"height.scale.4", "Height Scale 4", 1.0, 0.05, 3.0, 0.01},
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
        "SolidHole",
        "Hole",
        {
            {"diameter", "Diameter", 10.0, 0.001, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"hole_type", "Type", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Through", "Depth"}},
            {"depth", "Depth", 10.0, 0.001, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"hole.distance1", "Distance to Edge 1", 10.0, 0.0, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"hole.distance2", "Distance to Edge 2", 10.0, 0.0, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"hole.center.x", "Center X", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.center.y", "Center Y", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.center.z", "Center Z", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.normal.x", "Normal X", 0.0, -1.0, 1.0, 0.01},
            {"hole.normal.y", "Normal Y", 0.0, -1.0, 1.0, 0.01},
            {"hole.normal.z", "Normal Z", 1.0, -1.0, 1.0, 0.01},
            {"hole.refs.valid", "Reference Edges", 0.0, 0.0, 1.0, 1.0},
            {"hole.edge1.start.x", "Edge 1 Start X", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge1.start.y", "Edge 1 Start Y", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge1.start.z", "Edge 1 Start Z", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge1.end.x", "Edge 1 End X", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge1.end.y", "Edge 1 End Y", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge1.end.z", "Edge 1 End Z", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge1.side", "Edge 1 Side", 1.0, -1.0, 1.0, 1.0},
            {"hole.edge2.start.x", "Edge 2 Start X", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge2.start.y", "Edge 2 Start Y", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge2.start.z", "Edge 2 Start Z", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge2.end.x", "Edge 2 End X", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge2.end.y", "Edge 2 End Y", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge2.end.z", "Edge 2 End Z", 0.0, -1000000.0, 1000000.0, 0.1},
            {"hole.edge2.side", "Edge 2 Side", 1.0, -1.0, 1.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
    });

    tools_.push_back({
        "SurfaceRuled",
        "Ruled Surface",
        {
            {"length", "Length", 350.0, 0.001, 1000000.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"direction", "Direction", 2.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo, {"Axis X", "Axis Y", "Axis Z"}},
            {"reverse_normal", "Reverse Normal", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"profile.id", "Curve ID", 0.0, 0.0, 4294967295.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            document.CreateRuledSurfaceFromSpline(
                static_cast<unsigned long>(std::max(0.0, param(parameters, "profile.id", 0.0))),
                param(parameters, "length", 350.0),
                static_cast<int>(param(parameters, "direction", 2.0)),
                param(parameters, "reverse_normal", 0.0) >= 0.5);
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            document.RebuildRuledSurface(
                index,
                static_cast<unsigned long>(std::max(0.0, param(parameters, "profile.id", 0.0))),
                param(parameters, "length", 350.0),
                static_cast<int>(param(parameters, "direction", 2.0)),
                param(parameters, "reverse_normal", 0.0) >= 0.5);
        }
    });

    tools_.push_back({
        "SolidShell",
        "Shell",
        {
            {"distance", "Distance", 18.0, -1000.0, 1000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"surface.id", "Surface ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"face.index", "Face Index", 0.0, 0.0, 1000000.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            document.CreateShellFromSurface(
                static_cast<unsigned long>(std::max(
                    0.0, param(parameters, "surface.id", 0.0))),
                static_cast<int>(param(parameters, "face.index", 0.0)),
                param(parameters, "distance", 18.0));
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            document.RebuildShellFromSurface(
                index,
                static_cast<unsigned long>(std::max(
                    0.0, param(parameters, "surface.id", 0.0))),
                static_cast<int>(param(parameters, "face.index", 0.0)),
                param(parameters, "distance", 18.0));
        }
    });

    tools_.push_back({
        "DrawSpline",
        "Draw Spline",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    for (const auto& curve_construction :
         std::vector<std::pair<std::string, std::string>>{
             {"PlaneIntersection", "Body Section by Plane"},
             {"SurfaceIntersection", "Surface Intersection"},
             {"ProjectCurveToSurface", "Project Curve"},
             {"ExtractSurfaceEdge", "Extract Edge"}}) {
        tools_.push_back({
            curve_construction.first,
            curve_construction.second,
            {},
            [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
            [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
        });
    }

    tools_.push_back({
        "CurveFillets",
        "Fillets",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
    });

    tools_.push_back({
        "SolidWireTool",
        "Wire",
        {
            {"radius", "Radius", 10.0, 0.01, 1000000.0, 0.1,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"profile.id", "Path ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_wire_base(document, index, parameters);
        }
    });

    tools_.push_back({
        "BezierCurve3D",
        "Bezier 3D",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
    });

    tools_.push_back({
        "NurbsCurve3D",
        "NURBS 3D",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
    });

    for (const auto& curve_edit :
         std::vector<std::pair<std::string, std::string>>{
             {"CurveJoin", "Join"},
             {"CurveSplit", "Split"},
             {"CurveExtend", "Extend"},
             {"CurveTrimByPlane", "Trim by Plane"},
             {"CurveSimplifyByPoint", "Simplify by Point"},
             {"CurveReverse", "Reverse"},
             {"NurbsParametersTool", "NURBS Parameters"}}) {
        tools_.push_back({
            curve_edit.first,
            curve_edit.second,
            {},
            [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
            [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
        });
    }

    tools_.push_back({
        "SewingFaceTool",
        "Sewing Faces",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {},
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {}
    });

    tools_.push_back({
        "SolidSweepTwoRails",
        "Sweep Solid (2 Rails)",
        {
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"guide1.id", "Rail 1 ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"guide2.id", "Rail 2 ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.CreateTwoRailSweepSolidFromSelection();
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            document.RebuildTwoRailSweepSolid(
                index,
                static_cast<unsigned long>(std::max(0.0, param(parameters, "profile.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "guide1.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "guide2.id", 0.0))));
        }
    });

    tools_.push_back({
        "SurfaceFourSplines",
        "Surface by 4 Splines",
        {
            {"curve1.id", "Curve 1 ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"curve2.id", "Curve 2 ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"curve3.id", "Curve 3 ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"curve4.id", "Curve 4 ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.CreateFourSplineSurfaceFromSelection();
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            document.RebuildFourSplineSurface(
                index,
                static_cast<unsigned long>(std::max(0.0, param(parameters, "curve1.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "curve2.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "curve3.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "curve4.id", 0.0))));
        }
    });

    tools_.push_back({
        "SurfaceSweepTwoRails",
        "Sweep Surface (2 Rails)",
        {
            {"profile.id", "Profile ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"guide1.id", "Rail 1 ID", 0.0, 0.0, 1000000000.0, 1.0},
            {"guide2.id", "Rail 2 ID", 0.0, 0.0, 1000000000.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.CreateTwoRailSweepSurfaceFromSelection();
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            document.RebuildTwoRailSweepSurface(
                index,
                static_cast<unsigned long>(std::max(0.0, param(parameters, "profile.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "guide1.id", 0.0))),
                static_cast<unsigned long>(std::max(0.0, param(parameters, "guide2.id", 0.0))));
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
        "room",
        "Room",
        {
            {"length", "Length", 6000.0, 500.0, 100000.0, 100.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"width", "Width", 4000.0, 500.0, 100000.0, 100.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 2900.0, 300.0, 20000.0, 100.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"wall_thickness", "Wall Thickness", 200.0, 20.0, 2000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"floor_thickness", "Floor Thickness", 120.0, 10.0, 500.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"ceiling", "Ceiling", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_component_assembly(
                document, room_parts(document, parameters), parameters, "room", "Room");
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            const auto& objects = document.GetObjects();
            const auto* room = index < objects.size()
                ? dynamic_cast<const CAssembled*>(objects[index].get()) : nullptr;
            rebuild_component_assembly(
                document, index, room_parts(document, parameters, room), parameters, "room");
        }
    });

    tools_.push_back({
        "MeshIslandBoundaries",
        "Surface Island Boundaries",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "WeldingVertex",
        "Welding Vertex",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "MeshBoundaryLine",
        "Boundary Line Patch",
        {},
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "window",
        "Window",
        {
            {"distance_along_wall", "Distance Along Wall", 1800.0, 0.0, 100000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"distance_from_floor", "Distance From Floor", 900.0, 0.0, 20000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"factor", "Factor", 0.5, 0.1, 0.9, 0.05},
            {"wood_construction", "Wood Construction", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"shape", "Shape", 0.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo,
                {"Rectangular 1", "Rectangular 2", "Arched"}},
            {"width", "Width", 1000.0, 300.0, 10000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 1300.0, 300.0, 10000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"frame_width", "Frame Profile Width", 60.0, 15.0, 500.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"frame_thickness", "Frame Profile Thickness", 60.0, 15.0, 500.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"vertical_bars", "Vertical Muntin Bars", 0.0, 0.0, 12.0, 1.0},
            {"horizontal_bars", "Horizontal Muntin Bars", 0.0, 0.0, 12.0, 1.0},
            {"sill_projection", "Sill Projection", 50.0, 0.0, 500.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"sill_thickness", "Sill Thickness", 30.0, 5.0, 100.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"sill_side_extension", "Sill Side Extension", 50.0, 0.0, 500.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"reverse", "Reverse", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"host.wall.id", "Host Wall", 0.0, 0.0, 4294967295.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_component_assembly(
                document, window_parts(document, parameters), parameters, "window", "Window");
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_component_assembly(
                document, index, window_parts(document, parameters), parameters, "window");
        }
    });

    tools_.push_back({
        "door",
        "Door",
        {
            {"type", "Type", 0.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo,
                {"Glass Door", "Panel Door", "Opening"}},
            {"distance_along_wall", "Distance Along Wall", 1270.0, 0.0, 100000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"width", "Width", 900.0, 500.0, 10000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 2000.0, 1000.0, 10000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"reverse", "Reverse", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"distance_to_floor", "Distance To Floor", 100.0, 0.0, 5000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"handle_side", "Handle Position", 1.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Left", "Right"}},
            {"handle_type", "Handle Type", 0.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo, {"Round", "Lever", "None"}},
            {"double_door", "Double Door", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"door_frame", "Door Frame", 1.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"material", "Material", 0.0, 0.0, 4.0, 1.0,
                ToolParameterType::Combo,
                {"Beech", "Oak", "Pine", "White", "Aluminum"}},
            {"host.wall.id", "Host Wall", 0.0, 0.0, 4294967295.0, 1.0}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_component_assembly(
                document, door_parts(document, parameters), parameters, "door", "Door");
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_component_assembly(
                document, index, door_parts(document, parameters), parameters, "door");
        }
    });

    const std::vector<ToolParameter> cabinet_advanced_parameters = {
            {"body_type", "Body Type", 0.0, 0.0, 6.0, 1.0,
                ToolParameterType::Combo,
                {"Straight", "Corner", "Radius", "Corner-2", "Radius-2",
                 "Radius-3", "Radius-4"}},
            {"facade_type", "Facade", 2.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo, {"Open", "Single Door", "Double Door"}},
            {"facade_style", "Facade Style", 0.0, 0.0, 4.0, 1.0,
                ToolParameterType::Combo,
                {"Plain", "Frame", "Screen", "Milled", "Milano"}},
            {"width", "Width", 600.0, 300.0, 3000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 720.0, 300.0, 2600.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 560.0, 200.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"overhead", "Overhead", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"mounting_height", "Height Hanger", 1300.0, 0.0, 5000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"facade_bulge", "Facade Bulge", 280.0, 10.0, 1500.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"radius2_bulge", "Radius-2 Bulge", 120.0, 10.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"radius_side_straight", "Radius-4 Straight", 180.0, 0.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 100.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"shelf_count", "Shelf Count", 2.0, 0.0, 20.0, 1.0},
            {"door_open_angle", "Single/Corner Door Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_door_open_angle", "Left Door Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_door_open_angle", "Right Door Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"door_hinge_side", "Door Hinge", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Left", "Right"}},
            {"handle_orientation", "Handle", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Horizontal", "Vertical"}},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
    };

    tools_.push_back({
        "cabinet",
        "Cabinet",
        {
            {"facade_type", "Facade", 2.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo, {"Open", "Single Door", "Double Door"}},
            {"facade_style", "Facade Style", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo,
                {"Plain", "Frame"}},
            {"width", "Width", 600.0, 300.0, 3000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 720.0, 300.0, 2600.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 560.0, 200.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"overhead", "Overhead", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"mounting_height", "Height Hanger", 1300.0, 0.0, 5000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 100.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"shelf_count", "Shelf Count", 2.0, 0.0, 20.0, 1.0},
            {"door_open_angle", "Door Open Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"handle_orientation", "Handle", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Horizontal", "Vertical"}},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_kitchen_cabinet(document, parameters);
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_kitchen_cabinet(document, index, parameters);
        }
    });

    tools_.push_back({
        "chair_simple",
        "Chair Simple",
        {
            {"back_style", "Back Style", 0.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo,
                {"Vertical Slats", "Horizontal Rails", "Panel"}},
            {"width", "Width", 430.0, 280.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 430.0, 280.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_height", "Seat Height", 460.0, 250.0, 900.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"total_height", "Total Height", 900.0, 500.0, 1800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_thickness", "Seat Thickness", 24.0, 8.0, 120.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_edge_radius", "Seat Edge Radius", 6.0, 0.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_size", "Leg/Post Size", 42.0, 18.0, 120.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_taper", "Leg Taper", 3.0, 0.0, 40.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_inset", "Leg Inset", 12.0, 0.0, 100.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_height", "Apron Height", 60.0, 20.0, 180.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"stretcher_height", "Stretcher Height", 170.0, 0.0, 500.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_rake", "Back Rake", 35.0, -100.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_member_count", "Back Members", 5.0, 1.0, 12.0, 1.0},
            {"back_member_width", "Back Member Width", 28.0, 8.0, 120.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_rail_height", "Back Rail Height", 50.0, 20.0, 160.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"body_material_id", "Wood Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_simple_chair(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_simple_chair(document, index, parameters);
        }
    });

    tools_.push_back({
        "SolidSheetBend",
        "Sheet Bend",
        {
            {"radius", "Inner Radius", 2.0, 0.001, 1000000.0, 0.5,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"angle", "Bend Angle", 90.0, 0.01, 178.99, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"direction", "Direction", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo,
                {"Clockwise", "Counterclockwise"}}
        },
        [](CAlfaDoc&, const std::vector<ToolParameter>&) {
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "SurfaceJoin",
        "Join",
        {},
        [](CAlfaDoc& document, const std::vector<ToolParameter>&) {
            document.JoinSelectedSurfaces();
        },
        [](CAlfaDoc&, size_t, const std::vector<ToolParameter>&) {
        }
    });

    tools_.push_back({
        "cabinet_advanced",
        "Cabinet Advanced",
        cabinet_advanced_parameters,
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_kitchen_cabinet(document, parameters);
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_kitchen_cabinet(document, index, parameters);
        }
    });

    std::vector<ToolParameter> cabinet_slx_parameters = cabinet_advanced_parameters;
    cabinet_slx_parameters.insert(
        cabinet_slx_parameters.begin(),
        {
            {"slx.profile.id", "Frame Profile", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::CatalogSketch},
            {"slx.panel.id", "Panel Profile", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::CatalogSketch},
            {"slx.milling.profile.id", "Cutter Profile", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::CatalogSketch},
            {"slx.milling.guide.id", "Milling Pattern", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::CatalogSketch},
            {"milling_depth", "Milling Depth", 9.0, 0.1, 100.0, 0.5,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"handle_type", "Handle Type", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo,
                {"Shape", "Modern", "Round", "From Catalog"}},
            {"slx.handle.id", "Catalog Handle", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::CatalogProduct},
            {"slx.contour.1.id", "Milling Contour 1 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.2.id", "Milling Contour 2 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.3.id", "Milling Contour 3 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.4.id", "Milling Contour 4 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.5.id", "Milling Contour 5 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.6.id", "Milling Contour 6 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.7.id", "Milling Contour 7 ID", 0.0, 0.0, 4294967295.0, 1.0},
            {"slx.contour.8.id", "Milling Contour 8 ID", 0.0, 0.0, 4294967295.0, 1.0}
        });
    tools_.push_back({
        "cabinet_advanced_slx",
        "Cabinet Advanced SLX",
        std::move(cabinet_slx_parameters),
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_kitchen_cabinet(document, parameters);
        },
        [](CAlfaDoc& document, size_t index, const std::vector<ToolParameter>& parameters) {
            rebuild_kitchen_cabinet(document, index, parameters);
        }
    });

    tools_.push_back({
        "cabinet_showcase",
        "Cabinet Showcase",
        {
            {"showcase_facade_type", "Facade", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Single Door", "Double Door"}},
            {"facade_style", "Facade Style", 0.0, 0.0, 4.0, 1.0,
                ToolParameterType::Combo,
                {"Plain", "Frame", "Screen", "Milled", "Milano"}},
            {"facade_showcase", "Facade Showcase", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"showcase_fill", "Showcase Fill", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo,
                {"Glass", "Lattice", "Muntin Bars", "Stained Glass"}},
            {"width", "Width", 600.0, 300.0, 3000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 720.0, 300.0, 2600.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 350.0, 150.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"overhead", "Overhead", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"mounting_height", "Height Hanger", 1300.0, 0.0, 5000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 100.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"shelf_count", "Shelf Count", 2.0, 0.0, 20.0, 1.0},
            {"door_open_angle", "Door Open Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"door_hinge_side", "Door Hinge", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Left", "Right"}},
            {"handle_orientation", "Handle", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Horizontal", "Vertical"}},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Frame Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_showcase_cabinet(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_showcase_cabinet(document, index, parameters);
        }
    });

    tools_.push_back({
        "chair",
        "Chair Advanced",
        {
            {"back_style", "Back Style", 0.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo,
                {"Vertical Slats", "Horizontal Rails", "Panel"}},
            {"width", "Width", 430.0, 280.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 480.0, 280.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_height", "Seat Height", 470.0, 250.0, 900.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"total_height", "Total Height", 980.0, 500.0, 1800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_thickness", "Seat Thickness", 25.0, 8.0, 120.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_edge_radius", "Seat Edge Radius", 8.0, 0.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"seat_back_curve", "Seat Back Curve", 55.0, -200.0, 200.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_size", "Front Leg Size", 42.0, 18.0, 120.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"rear_post_size", "Back Post Size", 45.0, 18.0, 140.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_taper", "Leg Taper", 5.0, 0.0, 40.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_inset", "Leg Inset", 12.0, 0.0, 100.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_height", "Apron Height", 60.0, 20.0, 180.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_thickness", "Apron Thickness", 22.0, 8.0, 80.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"stretcher_height", "Stretcher Height", 170.0, 0.0, 500.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"stretcher_size", "Stretcher Size", 24.0, 8.0, 80.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_rake", "Back Rake", 65.0, -150.0, 250.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"rear_post_curve", "Rear Post Curve", -35.0, -200.0, 200.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_member_count", "Back Members", 5.0, 1.0, 12.0, 1.0},
            {"back_member_width", "Back Member Width", 32.0, 8.0, 120.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_member_thickness", "Back Member Thickness", 18.0, 5.0, 80.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_rail_height", "Back Rail Height", 55.0, 20.0, 160.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_curve", "Back Curve", 18.0, -100.0, 100.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_depth_curve", "Back Depth Curve", 25.0, -200.0, 200.0, 2.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"body_material_id", "Wood Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_chair(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_chair(document, index, parameters);
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
            {"leg_taper", "Leg Taper", 8.0, 0.0, 90.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_inset", "Leg Inset", 40.0, 0.0, 500.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_height", "Apron Height", 60.0, 10.0, 300.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_thickness", "Apron Thickness", 20.0, 5.0, 100.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"apron_edge_radius", "Apron Edge Radius", 2.0, 0.0, 50.0, 0.5,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"top_shape", "Top Shape", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Rectangle", "Arc Ends"}},
            {"arc_bulge", "Arc Bulge", 100.0, 10.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"top_material_id", "Top Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"base_material_id", "Base Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_table(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_table(document, index, parameters);
        }
    });

    tools_.push_back({
        "desk",
        "Desk",
        {
            {"desk_type", "Desk Type", 0.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo,
                {"Without Drawers", "Drawers Left", "Drawers Right"}},
            {"width", "Width", 1100.0, 600.0, 3000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 700.0, 500.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 500.0, 300.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 100.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"back_panel_height", "Back Panel Height", 300.0, 100.0, 800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_width", "Drawer Unit Width", 350.0, 200.0, 900.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_count", "Drawer Count", 3.0, 1.0, 8.0, 1.0},
            {"open_drawer", "Open Drawer", 0.0, 0.0, 8.0, 1.0,
                ToolParameterType::Combo,
                {"Closed", "Drawer 1", "Drawer 2", "Drawer 3", "Drawer 4",
                 "Drawer 5", "Drawer 6", "Drawer 7", "Drawer 8"}},
            {"pullout_distance", "Pullout Distance", 300.0, 0.0, 800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"hardware_material_id", "Hardware Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_desk(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_desk(document, index, parameters);
        }
    });

    tools_.push_back({
        "single_drawer",
        "Single Drawer",
        {
            {"width", "Facade Width", 500.0, 100.0, 1800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Facade Height", 180.0, 60.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Drawer Depth", 450.0, 100.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"facade_style", "Facade Style", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Plain", "Frame"}},
            {"handle_type", "Handle Type", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo, {"Modern", "Classic", "Knob", "None"}},
            {"facade_thickness", "Facade Thickness", 18.0, 5.0, 60.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_side_thickness", "Drawer Side Thickness", 12.0, 5.0, 30.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_bottom_thickness", "Drawer Bottom Thickness", 6.0, 3.0, 20.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"slide_clearance", "Side Clearance", 13.0, 0.0, 80.0, 0.5,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"open_drawer", "Drawer", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Closed", "Open"}},
            {"pullout_distance", "Pullout Distance", 300.0, 0.0, 900.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"body_material_id", "Drawer Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"hardware_material_id", "Hardware Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_component_assembly(
                document, single_drawer_parts(parameters), parameters,
                "single_drawer", "Single Drawer");
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_component_assembly(
                document, index, single_drawer_parts(parameters), parameters,
                "single_drawer");
        }
    });

    tools_.push_back({
        "single_facade",
        "Single Facade",
        {
            {"width", "Width", 500.0, 60.0, 3000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 700.0, 60.0, 2600.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"thickness", "Thickness", 18.0, 5.0, 80.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"facade_style", "Facade Style", 0.0, 0.0, 4.0, 1.0,
                ToolParameterType::Combo,
                {"Plain", "Frame", "Screen", "Milled", "Milano"}},
            {"hinge_side", "Hinge", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Left", "Right"}},
            {"open_angle", "Open Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"handle_orientation", "Handle", 0.0, 0.0, 1.0, 1.0,
                ToolParameterType::Combo, {"Horizontal", "Vertical"}},
            {"handle_type", "Handle Type", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo, {"Modern", "Classic", "Knob", "None"}},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"hardware_material_id", "Hardware Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_component_assembly(
                document, single_facade_parts(parameters), parameters,
                "single_facade", "Single Facade");
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_component_assembly(
                document, index, single_facade_parts(parameters), parameters,
                "single_facade");
        }
    });

    tools_.push_back({
        "drawer_box",
        "Drawer Box",
        {
            {"width", "Width", 600.0, 300.0, 1800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"height", "Height", 800.0, 300.0, 2400.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"depth", "Depth", 500.0, 250.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"facade_type", "Facade Type", 0.0, 0.0, 4.0, 1.0,
                ToolParameterType::Combo,
                {"Plain", "Frame", "Screen", "Milled", "Milano"}},
            {"handle_type", "Handle Type", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo, {"Modern", "Classic", "Knob", "None"}},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_side_thickness", "Drawer Side Thickness", 12.0, 5.0, 30.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_bottom_thickness", "Drawer Bottom Thickness", 6.0, 3.0, 20.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"slide_clearance", "Slide Clearance", 13.0, 5.0, 40.0, 0.5,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_count", "Drawer Count", 3.0, 1.0, 6.0, 1.0},
            {"drawer_height_1", "Drawer 1 Height", 200.0, 50.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_height_2", "Drawer 2 Height", 200.0, 50.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_height_3", "Drawer 3 Height", 400.0, 50.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_height_4", "Drawer 4 Height", 200.0, 50.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_height_5", "Drawer 5 Height", 200.0, 50.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"drawer_height_6", "Drawer 6 Height", 200.0, 50.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"make_legs", "Make Legs", 1.0, 0.0, 1.0, 1.0,
                ToolParameterType::Checkbox},
            {"leg_height", "Leg Height", 100.0, 20.0, 300.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"open_drawer", "Open Drawer", 0.0, 0.0, 6.0, 1.0,
                ToolParameterType::Combo,
                {"Closed", "Drawer 1", "Drawer 2", "Drawer 3",
                 "Drawer 4", "Drawer 5", "Drawer 6"}},
            {"pullout_distance", "Pullout Distance", 300.0, 0.0, 800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"hardware_material_id", "Hardware Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_drawer_box(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_drawer_box(document, index, parameters);
        }
    });

    tools_.push_back({
        "kitchen_nika_260",
        "Kitchen Nika-260",
        {
            {"width", "Kitchen Width", 2600.0, 1600.0, 6000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"base_height", "Base Height", 800.0, 500.0, 1400.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"base_depth", "Base Depth", 500.0, 300.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"upper_height", "Upper Height", 800.0, 300.0, 1600.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"upper_depth", "Upper Depth", 300.0, 180.0, 800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"wall_gap", "Worktop to Upper Gap", 450.0, 100.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"worktop_thickness", "Worktop Thickness", 38.0, 10.0, 100.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"worktop_front_radius", "Worktop Front Radius", 20.0, 0.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_height", "Leg Height", 100.0, 20.0, 300.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"facade_style", "Facade Style", 4.0, 0.0, 4.0, 1.0,
                ToolParameterType::Combo,
                {"Plain", "Frame", "Screen", "Milled", "Milano"}},
            {"handle_type", "Handle Type", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo,
                {"Modern", "Classic", "Knob", "None"}},
            {"lower_1_door_angle", "Lower Door 1 Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"lower_4_left_door_angle", "Lower Door 4 Left Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"lower_4_right_door_angle", "Lower Door 4 Right Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"upper_1_door_angle", "Upper Door 1 Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"upper_2_door_angle", "Upper Door 2 Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"upper_3_door_angle", "Upper Door 3 Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"upper_4_left_door_angle", "Upper Door 4 Left Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"upper_4_right_door_angle", "Upper Door 4 Right Angle", 0.0, 0.0, 150.0, 5.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"open_drawer", "Open Drawer", 0.0, 0.0, 8.0, 1.0,
                ToolParameterType::Combo,
                {"Closed", "Unit 2 / Drawer 1", "Unit 2 / Drawer 2",
                 "Unit 2 / Drawer 3", "Unit 2 / Drawer 4",
                 "Unit 3 / Drawer 1", "Unit 3 / Drawer 2",
                 "Unit 3 / Drawer 3", "Unit 3 / Drawer 4"}},
            {"pullout_distance", "Pullout Distance", 350.0, 0.0, 450.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"top_material_id", "Worktop Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"hardware_material_id", "Hardware Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_nika_kitchen(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_nika_kitchen(document, index, parameters);
        }
    });

    tools_.push_back({
        "kitchen_corner",
        "Corner Kitchen",
        {
            {"left_length", "Left Run Length", 2700.0, 1800.0, 6000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"right_length", "Right Run Length", 1800.0, 1200.0, 6000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"base_height", "Base Height", 800.0, 500.0, 1400.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"base_depth", "Base Depth", 570.0, 350.0, 1000.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"upper_height", "Upper Height", 800.0, 300.0, 1600.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"upper_depth", "Upper Depth", 300.0, 180.0, 800.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"wall_gap", "Worktop to Upper Gap", 600.0, 100.0, 1200.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"worktop_thickness", "Worktop Thickness", 38.0, 10.0, 100.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"worktop_front_radius", "Worktop Front Radius", 20.0, 0.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"panel_thickness", "Panel Thickness", 18.0, 5.0, 50.0, 1.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"leg_height", "Leg Height", 100.0, 20.0, 300.0, 10.0,
                ToolParameterType::Number, {}, ToolParameterUnit::Length},
            {"facade_style", "Facade Style", 2.0, 0.0, 2.0, 1.0,
                ToolParameterType::Combo,
                {"Chipboard Panel", "MDF Profile", "MDF Profile Milano"}},
            {"handle_type", "Handle Type", 0.0, 0.0, 3.0, 1.0,
                ToolParameterType::Combo,
                {"Modern", "Classic", "Knob", "None"}},
            {"left_lower_1_door_angle", "Left Wing Lower Door 1", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_lower_4_left_door_angle", "Left Wing Lower Door 4 Left", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_lower_4_right_door_angle", "Left Wing Lower Door 4 Right", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_upper_1_door_angle", "Left Wing Upper Door 1", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_upper_2_door_angle", "Left Wing Upper Door 2", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_upper_3_door_angle", "Left Wing Upper Door 3", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_upper_4_left_door_angle", "Left Wing Upper Door 4 Left", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"left_upper_4_right_door_angle", "Left Wing Upper Door 4 Right", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_lower_1_door_angle", "Right Wing Lower Door 1", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_lower_4_left_door_angle", "Right Wing Lower Door 4 Left", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_lower_4_right_door_angle", "Right Wing Lower Door 4 Right", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_upper_1_door_angle", "Right Wing Upper Door 1", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_upper_2_door_angle", "Right Wing Upper Door 2", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_upper_3_door_angle", "Right Wing Upper Door 3", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_upper_4_left_door_angle", "Right Wing Upper Door 4 Left", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"right_upper_4_right_door_angle", "Right Wing Upper Door 4 Right", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"lower_corner_door_angle", "Lower Corner Door", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"upper_corner_door_angle", "Upper Corner Door", 0.0, 0.0, 150.0, 5.0, ToolParameterType::Number, {}, ToolParameterUnit::Angle},
            {"body_material_id", "Body Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"facade_material_id", "Facade Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"top_material_id", "Worktop Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material},
            {"hardware_material_id", "Hardware Material", 0.0, 0.0, 4294967295.0, 1.0,
                ToolParameterType::Material}
        },
        [](CAlfaDoc& document, const std::vector<ToolParameter>& parameters) {
            create_corner_kitchen(document, parameters);
        },
        [](CAlfaDoc& document, size_t index,
           const std::vector<ToolParameter>& parameters) {
            rebuild_corner_kitchen(document, index, parameters);
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

    std::vector<ToolParameter> parameters =
        prepare_material_parameters(document, tool->defaults);
    if (id == "window" || id == "door") {
        set_tool_parameter_value(
            parameters, "host.wall.id",
            static_cast<double>(selected_room_wall_id(document)));
    }
    const size_t object_count_before = document.GetObjects().size();
    tool->create(document, parameters);
    if (id == "SolidTwoSketches") {
        if (document.GetObjects().size() <= object_count_before) {
            return {};
        }
        CAlfaObject* object = document.GetSelectedObject();
        return object
            ? ActiveObjectFromDocument(
                  document.GetSelectedObjectIndex(), *object, 0, &document)
            : ActiveParametricObject{};
    }
    if (is_furniture_assembly_tool(id)
        && document.GetObjects().size() <= object_count_before) {
        return {};
    }
    if (is_furniture_assembly_tool(id)
        && !dynamic_cast<CAssembled*>(document.GetSelectedObject())) {
        return {};
    }
    if (tool->defaults.empty()) {
        return {};
    }
    const size_t object_index = document.GetSelectedObjectIndex();
    store_parametric_definition(document, object_index, tool->id, tool->label, 0, parameters);
    if (id == "window" || id == "door") {
        RebuildArchitectureRooms(document);
    }
    return {tool->id, object_index, 0, parameters};
}

bool ToolRegistry::HasVisibleArchitectureWalls(CAlfaDoc& document) const {
    return !visible_room_wall_ids(document).empty();
}

ActiveParametricObject ToolRegistry::ActivateArchitectureOpening(
    const std::string& id,
    CAlfaDoc& document,
    unsigned long wall_id,
    CPoint3d approximate_point) const {
    if (id != "window" && id != "door") return {};
    const std::vector<unsigned long> visible_walls =
        visible_room_wall_ids(document);
    if (std::find(visible_walls.begin(), visible_walls.end(), wall_id)
        == visible_walls.end()) {
        return {};
    }

    const ToolDefinition* tool = Find(id);
    const auto* wall = dynamic_cast<const CSolid*>(
        document.FindObjectById(wall_id));
    Vec3 minimum{};
    Vec3 maximum{};
    if (!tool || !wall || !wall->GetBounds(minimum, maximum)) return {};

    std::vector<ToolParameter> parameters = tool->defaults;
    set_tool_parameter_value(
        parameters, "host.wall.id", static_cast<double>(wall_id));
    const bool along_x = maximum.x - minimum.x
        >= maximum.y - minimum.y;
    const double wall_length = along_x
        ? maximum.x - minimum.x
        : maximum.y - minimum.y;
    const double clicked_distance = along_x
        ? approximate_point.x - minimum.x
        : approximate_point.y - minimum.y;
    const double opening_width = std::max(
        id == "window" ? 300.0 : 500.0,
        param(parameters, "width", id == "window" ? 1000.0 : 900.0));
    const double half_width = std::min(opening_width * 0.5,
                                       wall_length * 0.5);
    const double distance = std::clamp(
        clicked_distance,
        half_width,
        std::max(half_width, wall_length - half_width));
    for (ToolParameter& parameter : parameters) {
        if (parameter.id != "distance_along_wall") continue;
        parameter.minimum = 0.0;
        parameter.maximum = std::max(parameter.step, wall_length);
        parameter.value = distance;
        break;
    }
    return CreateParametricObject(id, document, parameters);
}

ActiveParametricObject ToolRegistry::CreateParametricObject(const std::string& id,
                                                            CAlfaDoc& document,
                                                            const std::vector<ToolParameter>& parameters) const {
    const ToolDefinition* tool = Find(id);
    if (!tool || !tool->create || parameters.empty()) {
        return {};
    }

    ActiveParametricObject prepared = PrepareParametricObject(
        id, document, parameters);
    if (prepared.tool_id.empty()) {
        return {};
    }
    const size_t object_count_before = document.GetObjects().size();
    tool->create(document, prepared.parameters);
    if (id == "SolidTwoSketches") {
        if (document.GetObjects().size() <= object_count_before) {
            return {};
        }
        CAlfaObject* object = document.GetSelectedObject();
        return object
            ? ActiveObjectFromDocument(
                  document.GetSelectedObjectIndex(), *object, 0, &document)
            : ActiveParametricObject{};
    }
    if (is_furniture_assembly_tool(id)
        && (document.GetObjects().size() <= object_count_before
            || !dynamic_cast<CAssembled*>(document.GetSelectedObject()))) {
        return {};
    }
    const size_t object_index = document.GetSelectedObjectIndex();
    store_parametric_definition(
        document, object_index, tool->id, tool->label, 0, prepared.parameters);
    if (id == "window" || id == "door") {
        RebuildArchitectureRooms(document);
    }
    prepared.object_index = object_index;
    return prepared;
}

ActiveParametricObject ToolRegistry::PrepareParametricObject(
    const std::string& id,
    CAlfaDoc& document,
    const std::vector<ToolParameter>& parameters) const {
    const ToolDefinition* tool = Find(id);
    if (!tool || !tool->create || parameters.empty()) {
        return {};
    }
    return {
        tool->id,
        std::numeric_limits<size_t>::max(),
        0,
        prepare_material_parameters(document, parameters)};
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
            const bool is_plane = is_planar_surface(*surface);
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
    if (!body || !cutter) {
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
    if (body->GetNumOperations() <= 0) {
        document.EnsureObjectId(*body);
        transient_trim_bases_[body->m_id] = previous_shape;
        return {id, body_index, 0, std::move(parameters), true};
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

void ToolRegistry::AcceptTransientTrim(
    const ActiveParametricObject& active_object,
    const CAlfaDoc& document) const {
    if (!active_object.transient
        || active_object.object_index >= document.GetObjects().size()
        || !document.GetObjects()[active_object.object_index]) {
        return;
    }
    transient_trim_bases_.erase(
        document.GetObjects()[active_object.object_index]->m_id);
}

bool ToolRegistry::CancelTransientTrim(
    const ActiveParametricObject& active_object,
    CAlfaDoc& document) const {
    if (!active_object.transient
        || active_object.object_index >= document.GetObjects().size()) {
        return false;
    }
    auto* body = dynamic_cast<CSolid*>(
        document.GetObjects()[active_object.object_index].get());
    if (!body) {
        return false;
    }
    const auto found = transient_trim_bases_.find(body->m_id);
    if (found == transient_trim_bases_.end()) {
        return false;
    }
    body->m_Shape = found->second;
    transient_trim_bases_.erase(found);
    body->ClearSelectedEdge();
    body->ClearSelectedFace();
    return body->ReBuldMesh();
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

    // Reopening Boss/Pocket for the same attached sketch edits its existing
    // feature. Appending another operation with the identical profile makes
    // overlapping cuts hide each other's Depth changes.
    for (int operation_index = body->GetNumOperations() - 1;
         operation_index >= 1; --operation_index) {
        const ParametricFunction* operation = body->GetOperation(operation_index);
        if (!operation || operation->ToolId != kToolId)
            continue;
        const unsigned long operation_profile_id = static_cast<unsigned long>(
            std::max(0.0, saved_param(
                operation->Parameters, "profile.id", 0.0)));
        if (operation_profile_id == sketch->m_id) {
            return ActiveObjectFromDocument(
                body_index, *body, static_cast<size_t>(operation_index),
                &document);
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

ActiveParametricObject ToolRegistry::ApplyHole(
    CAlfaDoc& document,
    unsigned long body_id,
    const std::vector<ToolParameter>& input_parameters) const {
    constexpr const char* kToolId = "SolidHole";
    const ToolDefinition* tool = Find(kToolId);
    const size_t body_index = document.FindObjectIndexById(body_id);
    auto& objects = document.GetObjects();
    auto* body = body_index < objects.size()
        ? dynamic_cast<CSolid*>(objects[body_index].get()) : nullptr;
    if (!tool || !body || body->GetNumOperations() <= 0)
        return {};

    std::vector<ToolParameter> parameters = tool->defaults;
    for (ToolParameter& parameter : parameters) {
        const auto found = std::find_if(
            input_parameters.begin(), input_parameters.end(),
            [&parameter](const ToolParameter& input) {
                return input.id == parameter.id;
            });
        if (found != input_parameters.end())
            parameter.value = found->value;
    }

    const TopoDS_Shape previous_shape = body->m_Shape;
    if (!apply_hole(*body, parameters))
        return {};
    const size_t operation_index =
        static_cast<size_t>(body->GetNumOperations());
    body->SetParametricOperation(
        operation_index, kToolId, tool->label,
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
    if (active_object.transient
        && (active_object.tool_id == "TrimByPlane"
            || active_object.tool_id == "TrimBySketch"
            || active_object.tool_id == "TrimBySurface")
        && active_object.object_index < document.GetObjects().size()) {
        auto* body = dynamic_cast<CSolid*>(
            document.GetObjects()[active_object.object_index].get());
        if (!body) {
            return;
        }
        const auto found = transient_trim_bases_.find(body->m_id);
        if (found == transient_trim_bases_.end()) {
            return;
        }
        body->m_Shape = found->second;
        if (apply_trim_operation(
                *body, document, active_object.tool_id,
                active_object.parameters)) {
            document.UpdateAttachedSketches();
        } else {
            body->ReBuldMesh();
        }
        return;
    }

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
        if (active_object.object_index < document.GetObjects().size()) {
            if (auto* assembly = dynamic_cast<CAssembled*>(
                    document.GetObjects()[active_object.object_index].get())) {
                assembly->ApplyStoredTransformToElements();
            }
        }
        store_parametric_definition(document,
                                    active_object.object_index,
                                    active_object.tool_id,
                                    tool->label,
                                    active_object.operation_index,
                                    active_object.parameters);
        if (active_object.tool_id == "window"
            || active_object.tool_id == "door") {
            RebuildArchitectureRooms(document);
        } else if (active_object.tool_id == "room"
                   && active_object.object_index
                       < document.GetObjects().size()) {
            const auto* room = dynamic_cast<const CAssembled*>(
                document.GetObjects()[active_object.object_index].get());
            std::set<unsigned long> wall_ids;
            if (room) {
                wall_ids.insert(
                    room->GetElementIds().begin(), room->GetElementIds().end());
            }
            std::vector<unsigned long> opening_ids;
            for (const auto& object : document.GetObjects()) {
                if (!object || (object->GetParametricToolId() != "window"
                                && object->GetParametricToolId() != "door")) {
                    continue;
                }
                const unsigned long host_id = static_cast<unsigned long>(
                    std::max(0.0, saved_param(
                        object->GetParametricParameters(),
                        "host.wall.id", 0.0)));
                if (wall_ids.find(host_id) != wall_ids.end()) {
                    opening_ids.push_back(object->m_id);
                }
            }
            for (unsigned long opening_id : opening_ids) {
                const size_t opening_index =
                    document.FindObjectIndexById(opening_id);
                auto& objects = document.GetObjects();
                if (opening_index >= objects.size() || !objects[opening_index]) {
                    continue;
                }
                ActiveParametricObject opening = ActiveObjectFromDocument(
                    opening_index, *objects[opening_index], 0, &document);
                const ToolDefinition* opening_tool = Find(opening.tool_id);
                if (opening_tool && opening_tool->rebuild) {
                    opening_tool->rebuild(
                        document, opening_index, opening.parameters);
                }
            }
        }
        document.UpdateAttachedSketches();
    }
}

bool ToolRegistry::ApplyFurnitureMaterialParameter(
    const ActiveParametricObject& active_object,
    CAlfaDoc& document,
    const std::string& parameter_id) const {
    if (parameter_id != "body_material_id"
        && parameter_id != "facade_material_id"
        && parameter_id != "hardware_material_id"
        && parameter_id != "top_material_id"
        && parameter_id != "base_material_id") {
        return false;
    }
    if (active_object.object_index >= document.GetObjects().size()) {
        return false;
    }

    auto* assembly = dynamic_cast<CAssembled*>(
        document.GetObjects()[active_object.object_index].get());
    if (!assembly) {
        return false;
    }
    const Material* material = material_parameter(
        document, active_object.parameters, parameter_id.c_str());
    if (!material) {
        return false;
    }

    const bool is_kitchen = active_object.tool_id == "kitchen_nika_260"
        || active_object.tool_id == "kitchen_corner";
    bool changed = false;
    for (unsigned long element_id : assembly->GetElementIds()) {
        CAlfaObject* part = document.FindObjectById(element_id);
        if (!part) {
            continue;
        }
        const std::string& name = part->GetName();
        const bool protected_decor =
            name.find("Showcase Glass") != std::string::npos
            || name.find("Stained Glass Wire") != std::string::npos;
        const bool round_handle_face =
            name.find("Facade Handle Face") != std::string::npos;
        const bool facade = !protected_decor
            && name.find("Facade") != std::string::npos;
        const bool hardware = !round_handle_face
            && (name.find("Handle") != std::string::npos
                || name.find("Guide") != std::string::npos
                || (is_kitchen && name.find("Leg") != std::string::npos));
        const bool top = (active_object.tool_id == "table"
                          && name == "Table Top")
            || (is_kitchen && name.find("Worktop") != std::string::npos);
        const bool body = !protected_decor && !facade && !hardware && !top;

        const bool matches =
            (parameter_id == "facade_material_id" && facade)
            || (parameter_id == "hardware_material_id" && hardware)
            || (parameter_id == "top_material_id" && top)
            || (parameter_id == "base_material_id" && !top)
            || (parameter_id == "body_material_id" && body);
        if (!matches) {
            continue;
        }
        part->SetMaterial(*material);
        part->SetMaterialId(material->id);
        changed = true;
    }

    if (!changed) {
        return false;
    }
    std::vector<ParametricParameterValue> saved =
        assembly->GetParametricParameters();
    const double value = param(active_object.parameters, parameter_id.c_str(), 0.0);
    const auto existing = std::find_if(
        saved.begin(), saved.end(), [&parameter_id](const auto& parameter) {
            return parameter.id == parameter_id;
        });
    if (existing == saved.end()) {
        saved.push_back({parameter_id, value});
    } else {
        existing->value = value;
    }
    assembly->SetParametricDefinition(
        assembly->GetParametricToolId(), std::move(saved));
    return true;
}

void ToolRegistry::RebuildArchitectureRooms(CAlfaDoc& document) const {
    std::vector<unsigned long> room_ids;
    for (const auto& object : document.GetObjects()) {
        const auto* room = dynamic_cast<const CAssembled*>(object.get());
        if (room && room->GetParametricToolId() == "room") {
            room_ids.push_back(room->m_id);
        }
    }

    for (unsigned long room_id : room_ids) {
        const size_t room_index = document.FindObjectIndexById(room_id);
        auto& objects = document.GetObjects();
        auto* room = room_index < objects.size()
            ? dynamic_cast<CAssembled*>(objects[room_index].get()) : nullptr;
        if (!room) continue;

        std::vector<ToolParameter> parameters;
        parameters.reserve(room->GetParametricParameters().size());
        for (const ParametricParameterValue& saved
             : room->GetParametricParameters()) {
            parameters.push_back({
                saved.id, saved.id, saved.value,
                -1000000.0, 1000000.0, 1.0});
        }
        if (parameters.empty()) continue;
        rebuild_component_assembly(
            document, room_index,
            room_parts(document, parameters, room),
            parameters, "room");
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
        if (objects[i]
            && objects[i]->GetParametricToolId() == "cabinet_advanced_slx") {
            const bool uses_profile = std::any_of(
                objects[i]->GetParametricParameters().begin(),
                objects[i]->GetParametricParameters().end(),
                [profile_id](const ParametricParameterValue& parameter) {
                    return parameter.id.rfind("slx.", 0) == 0
                        && static_cast<unsigned long>(
                               std::max(0.0, parameter.value)) == profile_id;
                });
            if (uses_profile) {
                const ActiveParametricObject active = ActiveObjectFromDocument(
                    i, *objects[i], 0, &document);
                if (!active.tool_id.empty()) {
                    Rebuild(active, document);
                    rebuilt_any = true;
                }
            }
            continue;
        }
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
                     || parameter.id == "guide.id"
                     || parameter.id == "guide1.id"
                     || parameter.id == "guide2.id")
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
        if (objects[i]
            && objects[i]->GetParametricToolId() == "cabinet_advanced_slx") {
            const ActiveParametricObject active = ActiveObjectFromDocument(
                i, *objects[i], 0, &document);
            if (!active.tool_id.empty()) {
                Rebuild(active, document);
                rebuilt_any = true;
            }
            continue;
        }
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
                            || parameter.id == "guide.id"
                            || parameter.id == "guide1.id"
                            || parameter.id == "guide2.id";
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

ActiveParametricObject ToolRegistry::ActiveObjectFromDocument(
    size_t object_index,
    const CAlfaObject& object,
    size_t operation_index,
    CAlfaDoc* document) const {
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
    if (is_cabinet_tool(tool_id)) {
        double legacy_angle = 0.0;
        bool has_left_angle = false;
        bool has_right_angle = false;
        for (const ParametricParameterValue& parameter : saved_parameters) {
            if (parameter.id == "door_open_angle") {
                legacy_angle = parameter.value;
            } else if (parameter.id == "left_door_open_angle") {
                has_left_angle = true;
            } else if (parameter.id == "right_door_open_angle") {
                has_right_angle = true;
            }
        }
        if (!has_left_angle) {
            saved_parameters.push_back({"left_door_open_angle", legacy_angle});
        }
        if (!has_right_angle) {
            saved_parameters.push_back({"right_door_open_angle", legacy_angle});
        }
    }
    const ToolDefinition* tool = Find(tool_id);
    if (!tool || tool->defaults.empty()) {
        return {};
    }

    std::vector<ToolParameter> parameters =
        merge_saved_parameters(tool->defaults, saved_parameters);
    if (document) {
        parameters = prepare_material_parameters(*document, parameters);
    }
    return {tool_id, object_index, operation_index, std::move(parameters)};
}

std::string ToolRegistry::LabelFor(const std::string& id) const {
    const ToolDefinition* tool = Find(id);
    return tool ? tool->label : id;
}
