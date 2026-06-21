#include "Dom3DProjectSerializer.h"

#include "CadCurve3D.h"
#include "CBSpline.h"
#include "CMesh3D.h"
#include "CPolyline.h"
#include "solid/Solid.h"
#include "solid/SurfaceSet.h"

#include <QDomDocument>
#include <QFile>
#include <QSaveFile>
#include <QStringList>
#include <QXmlStreamWriter>

#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr const char* kProjectVersion = "1";

QString bool_text(bool value) {
    return value ? "true" : "false";
}

QString orbit_mode_text(OrbitMode mode) {
    return mode == OrbitMode::Architectural ? "architectural" : "cad";
}

OrbitMode parse_orbit_mode(const QString& value) {
    return value.compare("architectural", Qt::CaseInsensitive) == 0 ? OrbitMode::Architectural : OrbitMode::CAD;
}

QString object_type_name(const CAlfaObject& object) {
    if (dynamic_cast<const CSurfaceSet*>(&object)) {
        return "SurfaceSet";
    }
    if (dynamic_cast<const CSolid*>(&object)) {
        return "Solid";
    }
    if (dynamic_cast<const CMesh3D*>(&object)) {
        return "Mesh";
    }
    if (dynamic_cast<const CPolyline*>(&object)) {
        return "Curve";
    }
    if (dynamic_cast<const CBSpline*>(&object)) {
        return "BSpline";
    }
    if (dynamic_cast<const CCadCurve3D*>(&object)) {
        return "CadCurve3D";
    }
    return "Object";
}

QString default_room_name(const CAlfaObject& object) {
    if (dynamic_cast<const CSurfaceSet*>(&object)) {
        return "Surfaces";
    }
    if (dynamic_cast<const CSolid*>(&object)) {
        return "Solid";
    }
    if (dynamic_cast<const CMesh3D*>(&object)) {
        return "Surfaces";
    }
    if (dynamic_cast<const CPolyline*>(&object)) {
        return "Lines";
    }
    if (dynamic_cast<const CBSpline*>(&object)) {
        return "Lines";
    }
    if (dynamic_cast<const CCadCurve3D*>(&object)) {
        return "Lines";
    }
    return "Architecture";
}

QString operation_name_for_object(const CAlfaObject& object) {
    const QString name = QString::fromStdString(object.GetName());
    if (name.startsWith("Boolean Union")) {
        return "BooleanUnion";
    }
    if (name.startsWith("Boolean Cut")) {
        return "BooleanCut";
    }
    if (name.startsWith("Boolean Common")) {
        return "BooleanCommon";
    }
    if (name.startsWith("Fillet")) {
        return "Fillet";
    }
    return {};
}

void write_surface_texture_transforms(QXmlStreamWriter& xml, const CSolid& solid) {
    xml.writeStartElement("surfaceTextureTransforms");
    for (int i = 0; i < solid.GetNumSurfaces(); ++i) {
        const CSurfaceFace* surface = solid.GetSurfaceFace(i);
        if (!surface) {
            continue;
        }
        const SurfaceTextureTransform& transform = surface->TextureTransform;
        xml.writeEmptyElement("surface");
        xml.writeAttribute("index", QString::number(i));
        xml.writeAttribute("offsetU", QString::number(transform.offset_u, 'g', 9));
        xml.writeAttribute("offsetV", QString::number(transform.offset_v, 'g', 9));
        xml.writeAttribute("scaleU", QString::number(transform.scale_u, 'g', 9));
        xml.writeAttribute("scaleV", QString::number(transform.scale_v, 'g', 9));
        xml.writeAttribute("rotation", QString::number(transform.rotation_degrees, 'g', 9));
    }
    xml.writeEndElement();
}

bool read_float_attr(const QDomElement& element, const char* name, float& value, QString& error, bool required = true) {
    if (!element.hasAttribute(name)) {
        if (required) {
            error = QString("Missing XML attribute '%1'.").arg(name);
            return false;
        }
        return true;
    }

    bool ok = false;
    const float parsed = element.attribute(name).toFloat(&ok);
    if (!ok) {
        error = QString("Unsupported float value in XML attribute '%1'.").arg(name);
        return false;
    }
    value = parsed;
    return true;
}

bool read_surface_texture_transforms(const QDomElement& object_element, CSolid& solid, QString& error) {
    const QDomElement transforms = object_element.firstChildElement("surfaceTextureTransforms");
    if (transforms.isNull()) {
        return true;
    }

    for (QDomElement surface_element = transforms.firstChildElement("surface");
         !surface_element.isNull();
         surface_element = surface_element.nextSiblingElement("surface")) {
        bool index_ok = false;
        const int index = surface_element.attribute("index").toInt(&index_ok);
        if (!index_ok || index < 0 || index >= solid.GetNumSurfaces()) {
            error = "Surface texture transform contains an invalid face index.";
            return false;
        }

        SurfaceTextureTransform transform;
        if (!read_float_attr(surface_element, "offsetU", transform.offset_u, error, false)
            || !read_float_attr(surface_element, "offsetV", transform.offset_v, error, false)
            || !read_float_attr(surface_element, "scaleU", transform.scale_u, error, false)
            || !read_float_attr(surface_element, "scaleV", transform.scale_v, error, false)
            || !read_float_attr(surface_element, "rotation", transform.rotation_degrees, error, false)) {
            return false;
        }
        if (!surface_element.hasAttribute("scaleU")) {
            transform.scale_u = 1.0f;
        }
        if (!surface_element.hasAttribute("scaleV")) {
            transform.scale_v = 1.0f;
        }
        solid.SetSurfaceTextureTransform(index, transform);
    }
    return true;
}

bool read_bool_attr(const QDomElement& element, const char* name, bool& value, bool required = true) {
    if (!element.hasAttribute(name)) {
        return !required;
    }
    const QString text = element.attribute(name).trimmed().toLower();
    value = text == "true" || text == "1" || text == "yes";
    return true;
}

void write_view_state(QXmlStreamWriter& xml, const ProjectViewState& view_state) {
    xml.writeStartElement("view");
    xml.writeAttribute("orthographicProjection", bool_text(view_state.orthographic_projection));
    xml.writeAttribute("orbitMode", orbit_mode_text(view_state.orbit_mode));
    xml.writeAttribute("showCoordinateAxes", bool_text(view_state.show_coordinate_axes));
    xml.writeAttribute("showFloorGrid", bool_text(view_state.show_floor_grid));
    xml.writeAttribute("xyPlaneView", bool_text(view_state.xy_plane_view));

    xml.writeStartElement("camera");
    xml.writeAttribute("distance", QString::number(view_state.camera.distance, 'g', 9));

    xml.writeEmptyElement("target");
    xml.writeAttribute("x", QString::number(view_state.camera.target.x, 'g', 9));
    xml.writeAttribute("y", QString::number(view_state.camera.target.y, 'g', 9));
    xml.writeAttribute("z", QString::number(view_state.camera.target.z, 'g', 9));

    xml.writeEmptyElement("orientation");
    xml.writeAttribute("w", QString::number(view_state.camera.orientation.w, 'g', 9));
    xml.writeAttribute("x", QString::number(view_state.camera.orientation.x, 'g', 9));
    xml.writeAttribute("y", QString::number(view_state.camera.orientation.y, 'g', 9));
    xml.writeAttribute("z", QString::number(view_state.camera.orientation.z, 'g', 9));

    xml.writeEndElement();
    xml.writeEndElement();
}

bool read_view_state(const QDomElement& metadata, ProjectViewState& view_state, QString& error) {
    view_state = {};
    const QDomElement view_element = metadata.firstChildElement("view");
    if (view_element.isNull()) {
        return true;
    }

    if (read_bool_attr(view_element, "orthographicProjection", view_state.orthographic_projection, false)) {
        view_state.has_orthographic_projection = view_element.hasAttribute("orthographicProjection");
    }
    if (view_element.hasAttribute("orbitMode")) {
        view_state.orbit_mode = parse_orbit_mode(view_element.attribute("orbitMode"));
        view_state.has_orbit_mode = true;
    }
    if (read_bool_attr(view_element, "showCoordinateAxes", view_state.show_coordinate_axes, false)) {
        view_state.has_show_coordinate_axes = view_element.hasAttribute("showCoordinateAxes");
    }
    if (read_bool_attr(view_element, "showFloorGrid", view_state.show_floor_grid, false)) {
        view_state.has_show_floor_grid = view_element.hasAttribute("showFloorGrid");
    }
    if (read_bool_attr(view_element, "xyPlaneView", view_state.xy_plane_view, false)) {
        view_state.has_xy_plane_view = view_element.hasAttribute("xyPlaneView");
    }

    const QDomElement camera_element = view_element.firstChildElement("camera");
    if (camera_element.isNull()) {
        return true;
    }

    Camera camera{};
    if (!read_float_attr(camera_element, "distance", camera.distance, error, false)) {
        return false;
    }

    const QDomElement target_element = camera_element.firstChildElement("target");
    if (!target_element.isNull()) {
        if (!read_float_attr(target_element, "x", camera.target.x, error)
            || !read_float_attr(target_element, "y", camera.target.y, error)
            || !read_float_attr(target_element, "z", camera.target.z, error)) {
            return false;
        }
    }

    const QDomElement orientation_element = camera_element.firstChildElement("orientation");
    if (!orientation_element.isNull()) {
        if (!read_float_attr(orientation_element, "w", camera.orientation.w, error)
            || !read_float_attr(orientation_element, "x", camera.orientation.x, error)
            || !read_float_attr(orientation_element, "y", camera.orientation.y, error)
            || !read_float_attr(orientation_element, "z", camera.orientation.z, error)) {
            return false;
        }
    }

    view_state.camera = camera;
    view_state.has_camera = true;
    return true;
}

bool read_size_attr(const QDomElement& element, const char* name, size_t& value, QString& error, bool required = true) {
    if (!element.hasAttribute(name)) {
        if (required) {
            error = QString("Missing XML attribute '%1'.").arg(name);
            return false;
        }
        return true;
    }

    bool ok = false;
    const quint64 parsed = element.attribute(name).toULongLong(&ok);
    if (!ok) {
        error = QString("Unsupported integer value in XML attribute '%1'.").arg(name);
        return false;
    }
    value = static_cast<size_t>(parsed);
    return true;
}

void write_material(QXmlStreamWriter& xml, const Material& material) {
    xml.writeStartElement("material");
    xml.writeAttribute("id", QString::number(material.id));
    xml.writeAttribute("name", QString::fromStdString(material.name));
    xml.writeAttribute("r", QString::number(material.diffuse.r, 'g', 9));
    xml.writeAttribute("g", QString::number(material.diffuse.g, 'g', 9));
    xml.writeAttribute("b", QString::number(material.diffuse.b, 'g', 9));
    xml.writeAttribute("ambientR", QString::number(material.ambient.r, 'g', 9));
    xml.writeAttribute("ambientG", QString::number(material.ambient.g, 'g', 9));
    xml.writeAttribute("ambientB", QString::number(material.ambient.b, 'g', 9));
    xml.writeAttribute("emissionR", QString::number(material.emission.r, 'g', 9));
    xml.writeAttribute("emissionG", QString::number(material.emission.g, 'g', 9));
    xml.writeAttribute("emissionB", QString::number(material.emission.b, 'g', 9));
    xml.writeAttribute("alpha", QString::number(material.alpha, 'g', 9));
    xml.writeAttribute("specular", QString::number(material.specular, 'g', 9));
    xml.writeAttribute("shininess", QString::number(material.shininess, 'g', 9));
    xml.writeAttribute("reflectivity", QString::number(material.reflectivity, 'g', 9));
    xml.writeAttribute("textureOffsetU", QString::number(material.texture_offset_u, 'g', 9));
    xml.writeAttribute("textureOffsetV", QString::number(material.texture_offset_v, 'g', 9));
    xml.writeAttribute("textureScaleU", QString::number(material.texture_scale_u, 'g', 9));
    xml.writeAttribute("textureScaleV", QString::number(material.texture_scale_v, 'g', 9));
    xml.writeAttribute("textureRotation", QString::number(material.texture_rotation_degrees, 'g', 9));
    xml.writeAttribute("colorTexture", QString::fromStdString(material.color_texture_path));
    xml.writeAttribute("lightTexture", QString::fromStdString(material.light_texture_path));
    xml.writeAttribute("bumpTexture", QString::fromStdString(material.bump_texture_path));
    xml.writeEndElement();
}

bool read_material_element(const QDomElement& material_element, Material& material, QString& error) {
    if (material_element.hasAttribute("id")) {
        bool ok = false;
        const unsigned long id = material_element.attribute("id").toULong(&ok);
        if (ok) {
            material.id = id;
        }
    }
    if (material_element.hasAttribute("name")) {
        material.name = material_element.attribute("name").toStdString();
    }
    if (!read_float_attr(material_element, "r", material.diffuse.r, error, false)
        || !read_float_attr(material_element, "g", material.diffuse.g, error, false)
        || !read_float_attr(material_element, "b", material.diffuse.b, error, false)
        || !read_float_attr(material_element, "ambientR", material.ambient.r, error, false)
        || !read_float_attr(material_element, "ambientG", material.ambient.g, error, false)
        || !read_float_attr(material_element, "ambientB", material.ambient.b, error, false)
        || !read_float_attr(material_element, "emissionR", material.emission.r, error, false)
        || !read_float_attr(material_element, "emissionG", material.emission.g, error, false)
        || !read_float_attr(material_element, "emissionB", material.emission.b, error, false)
        || !read_float_attr(material_element, "alpha", material.alpha, error, false)
        || !read_float_attr(material_element, "specular", material.specular, error, false)
        || !read_float_attr(material_element, "shininess", material.shininess, error, false)
        || !read_float_attr(material_element, "reflectivity", material.reflectivity, error, false)
        || !read_float_attr(material_element, "textureOffsetU", material.texture_offset_u, error, false)
        || !read_float_attr(material_element, "textureOffsetV", material.texture_offset_v, error, false)
        || !read_float_attr(material_element, "textureScaleU", material.texture_scale_u, error, false)
        || !read_float_attr(material_element, "textureScaleV", material.texture_scale_v, error, false)
        || !read_float_attr(material_element, "textureRotation", material.texture_rotation_degrees, error, false)) {
        return false;
    }
    material.color_texture_path = material_element.attribute("colorTexture", QString::fromStdString(material.color_texture_path)).toStdString();
    material.light_texture_path = material_element.attribute("lightTexture", QString::fromStdString(material.light_texture_path)).toStdString();
    material.bump_texture_path = material_element.attribute("bumpTexture", QString::fromStdString(material.bump_texture_path)).toStdString();
    return true;
}

bool read_material(const QDomElement& object_element, CAlfaObject& object, QString& error) {
    const QDomElement material_element = object_element.firstChildElement("material");
    if (material_element.isNull()) {
        return true;
    }

    Material material = object.GetMaterial();
    if (!read_material_element(material_element, material, error)) {
        return false;
    }
    object.SetMaterial(material);
    return true;
}

const Material* find_loaded_material(const std::vector<Material>& materials, unsigned long id) {
    for (const Material& material : materials) {
        if (material.id == id) {
            return &material;
        }
    }
    return nullptr;
}

void upsert_loaded_material(std::vector<Material>& materials, Material material) {
    if (material.id == 0) {
        unsigned long next_id = 1;
        for (const Material& existing : materials) {
            next_id = std::max(next_id, existing.id + 1);
        }
        material.id = next_id;
    }

    for (Material& existing : materials) {
        if (existing.id == material.id) {
            existing = std::move(material);
            return;
        }
    }
    materials.push_back(std::move(material));
}

void write_identity_transform(QXmlStreamWriter& xml) {
    xml.writeStartElement("transform");

    xml.writeStartElement("position");
    xml.writeAttribute("x", "0");
    xml.writeAttribute("y", "0");
    xml.writeAttribute("z", "0");
    xml.writeEndElement();

    xml.writeStartElement("rotation");
    xml.writeAttribute("x", "0");
    xml.writeAttribute("y", "0");
    xml.writeAttribute("z", "0");
    xml.writeEndElement();

    xml.writeStartElement("scale");
    xml.writeAttribute("x", "1");
    xml.writeAttribute("y", "1");
    xml.writeAttribute("z", "1");
    xml.writeEndElement();

    xml.writeEndElement();
}

void write_parameters(QXmlStreamWriter& xml, const CAlfaObject& object) {
    const Material material = object.GetMaterial();
    xml.writeStartElement("parameters");
    xml.writeEmptyElement("parameter");
    xml.writeAttribute("name", "color.r");
    xml.writeAttribute("value", QString::number(material.diffuse.r, 'g', 9));
    xml.writeEmptyElement("parameter");
    xml.writeAttribute("name", "color.g");
    xml.writeAttribute("value", QString::number(material.diffuse.g, 'g', 9));
    xml.writeEmptyElement("parameter");
    xml.writeAttribute("name", "color.b");
    xml.writeAttribute("value", QString::number(material.diffuse.b, 'g', 9));
    for (const ParametricParameterValue& parameter : object.GetParametricParameters()) {
        xml.writeEmptyElement("parameter");
        xml.writeAttribute("name", QString::fromStdString(parameter.id));
        xml.writeAttribute("value", QString::number(parameter.value, 'g', 12));
    }
    xml.writeEndElement();
}

void write_operation_history(QXmlStreamWriter& xml, const CAlfaObject& object) {
    xml.writeStartElement("operationHistory");
    if (const auto* solid = dynamic_cast<const CSolid*>(&object)) {
        bool wrote_operation = false;
        for (const ParametricFunction* operation : solid->GetOperationTree()) {
            if (!operation || operation->ToolId.empty()) {
                continue;
            }
            wrote_operation = true;
            xml.writeStartElement("operation");
            xml.writeAttribute("type", QString::fromStdString(operation->ToolId));
            xml.writeAttribute("status", "parametric");
            for (const ParametricParameterValue& parameter : operation->Parameters) {
                xml.writeEmptyElement("parameter");
                xml.writeAttribute("name", QString::fromStdString(parameter.id));
                xml.writeAttribute("value", QString::number(parameter.value, 'g', 12));
            }
            for (int surface_index : operation->CreatedSurfaceIndices) {
                xml.writeEmptyElement("createdSurface");
                xml.writeAttribute("index", QString::number(surface_index));
            }
            xml.writeEndElement();
        }
        if (wrote_operation) {
            xml.writeEndElement();
            return;
        }
    }
    QString operation_name = QString::fromStdString(object.GetParametricToolId());
    if (operation_name.isEmpty()) {
        operation_name = operation_name_for_object(object);
    }
    if (!operation_name.isEmpty()) {
        xml.writeEmptyElement("operation");
        xml.writeAttribute("type", operation_name);
        xml.writeAttribute("status", object.IsParametric() ? "parametric" : "baked");
    }
    xml.writeEndElement();
}

bool write_boolean_tools(QXmlStreamWriter& xml, const CSolid& solid, QString& error) {
    if (solid.GetBooleanToolCount() == 0) {
        return true;
    }

    xml.writeStartElement("booleanTools");
    for (size_t i = 0; i < solid.GetBooleanToolCount(); ++i) {
        const CSolid* tool = solid.GetBooleanTool(i);
        if (!tool) {
            continue;
        }
        xml.writeStartElement("tool");
        xml.writeAttribute("index", QString::number(i));
        if (!tool->Save(xml, error)) {
            return false;
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return true;
}

bool read_boolean_tools(const QDomElement& object_element, CSolid& solid, QString& error) {
    const QDomElement tools_element = object_element.firstChildElement("booleanTools");
    if (tools_element.isNull()) {
        return true;
    }

    solid.ClearBooleanTools();
    for (QDomElement tool_element = tools_element.firstChildElement("tool");
         !tool_element.isNull();
         tool_element = tool_element.nextSiblingElement("tool")) {
        std::unique_ptr<CSolid> tool = CSolid::Load(tool_element, error);
        if (!tool) {
            return false;
        }
        solid.AddBooleanTool(std::move(tool));
    }
    return true;
}

std::vector<ParametricParameterValue> read_parameters_from(const QDomElement& parameters_element) {
    std::vector<ParametricParameterValue> parameters;
    for (QDomElement parameter_element = parameters_element.firstChildElement("parameter");
         !parameter_element.isNull();
         parameter_element = parameter_element.nextSiblingElement("parameter")) {
        const QString name = parameter_element.attribute("name");
        if (name.isEmpty() || name.startsWith("color.")) {
            continue;
        }
        bool ok = false;
        const double value = parameter_element.attribute("value").toDouble(&ok);
        if (ok) {
            parameters.push_back({name.toStdString(), value});
        }
    }
    return parameters;
}

void read_parametric_definition(const QDomElement& object_element, CAlfaObject& object) {
    const QDomElement history = object_element.firstChildElement("operationHistory");
    const QDomElement first_operation = history.firstChildElement("operation");
    if (first_operation.isNull()) {
        return;
    }

    const std::vector<ParametricParameterValue> legacy_parameters =
        read_parameters_from(object_element.firstChildElement("parameters"));

    auto* solid = dynamic_cast<CSolid*>(&object);
    if (solid) {
        solid->ClearOperationTree();
    }

    bool first_parametric_operation = true;
    size_t operation_index = 0;
    for (QDomElement operation = first_operation;
         !operation.isNull();
         operation = operation.nextSiblingElement("operation")) {
        const QString tool_id = operation.attribute("type");
        if (tool_id.isEmpty() || operation.attribute("status") == "baked") {
            continue;
        }

        std::vector<ParametricParameterValue> parameters = read_parameters_from(operation);
        if (parameters.empty()) {
            parameters = legacy_parameters;
        }

        if (first_parametric_operation) {
            object.SetParametricDefinition(tool_id.toStdString(), parameters);
            first_parametric_operation = false;
        }
        if (solid) {
            std::vector<int> created_surface_indices;
            for (QDomElement surface = operation.firstChildElement("createdSurface");
                 !surface.isNull();
                 surface = surface.nextSiblingElement("createdSurface")) {
                bool ok = false;
                const int surface_index = surface.attribute("index").toInt(&ok);
                if (ok && surface_index >= 0) {
                    created_surface_indices.push_back(surface_index);
                }
            }
            solid->SetParametricOperation(operation_index,
                                          tool_id.toStdString(),
                                          std::string(),
                                          parameters,
                                          std::move(created_surface_indices));
        }
        ++operation_index;
    }
}

QDomElement required_child(const QDomElement& parent, const char* name, QString& error) {
    const QDomElement child = parent.firstChildElement(name);
    if (child.isNull()) {
        error = QString("Missing XML element '%1'.").arg(name);
    }
    return child;
}
}

bool Dom3DProjectSerializer::Save(const QString& path,
                                  const CAlfaDoc& document,
                                  const QString& active_room,
                                  const ProjectViewState& view_state,
                                  QString& error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument("1.0");
    xml.writeStartElement("dom3dProject");
    xml.writeAttribute("version", kProjectVersion);

    xml.writeStartElement("metadata");
    xml.writeTextElement("activeRoom", active_room);
    write_view_state(xml, view_state);
    xml.writeEndElement();

    xml.writeStartElement("materials");
    for (const Material& material : document.GetMaterials()) {
        write_material(xml, material);
    }
    xml.writeEndElement();

    xml.writeStartElement("objects");
    const auto& objects = document.GetObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const CAlfaObject& object = *objects[i];
        const QString type = object_type_name(object);

        xml.writeStartElement("object");
        xml.writeAttribute("id", QString::number(object.m_id != 0 ? object.m_id : static_cast<unsigned long>(i + 1)));
        xml.writeAttribute("type", type);
        xml.writeAttribute("name", QString::fromStdString(object.GetName()));
        xml.writeAttribute("room", default_room_name(object));
        if (!object.GetGroupName().empty()) {
            xml.writeAttribute("group", QString::fromStdString(object.GetGroupName()));
        }
        xml.writeAttribute("materialId", QString::number(object.GetMaterialId()));
        xml.writeAttribute("visible", object.IsVisible() ? "true" : "false");

        write_material(xml, object.GetMaterial());
        write_identity_transform(xml);
        write_parameters(xml, object);
        write_operation_history(xml, object);

        if (const auto* polyline = dynamic_cast<const CPolyline*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "polyline");
            xml.writeAttribute("closed", polyline->IsClosed() ? "true" : "false");
            for (const CPoint3d& point : polyline->GetPoints()) {
                xml.writeEmptyElement("point");
                xml.writeAttribute("x", QString::number(point.x, 'g', 9));
                xml.writeAttribute("y", QString::number(point.y, 'g', 9));
                xml.writeAttribute("z", QString::number(point.z, 'g', 9));
            }
            xml.writeEndElement();
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "b-spline");
            xml.writeAttribute("closed", spline->IsClosed() ? "true" : "false");
            for (const CPoint3d& point : spline->GetPoints()) {
                xml.writeEmptyElement("point");
                xml.writeAttribute("x", QString::number(point.x, 'g', 9));
                xml.writeAttribute("y", QString::number(point.y, 'g', 9));
                xml.writeAttribute("z", QString::number(point.z, 'g', 9));
            }
            xml.writeEndElement();
        } else if (const auto* cad_curve = dynamic_cast<const CCadCurve3D*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "cad-curve-3d");
            for (const Vec3& point : cad_curve->GetPoints()) {
                xml.writeEmptyElement("point");
                xml.writeAttribute("x", QString::number(point.x, 'g', 9));
                xml.writeAttribute("y", QString::number(point.y, 'g', 9));
                xml.writeAttribute("z", QString::number(point.z, 'g', 9));
            }
            xml.writeEndElement();
        } else if (const auto* mesh = dynamic_cast<const CMesh3D*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "mesh");
            xml.writeStartElement("vertices");
            for (const Vec3& vertex : mesh->GetVertices()) {
                xml.writeEmptyElement("vertex");
                xml.writeAttribute("x", QString::number(vertex.x, 'g', 9));
                xml.writeAttribute("y", QString::number(vertex.y, 'g', 9));
                xml.writeAttribute("z", QString::number(vertex.z, 'g', 9));
            }
            xml.writeEndElement();

            if (mesh->GetUVs().size() == mesh->GetVertices().size()) {
                xml.writeStartElement("uvs");
                for (const UV& uv : mesh->GetUVs()) {
                    xml.writeEmptyElement("uv");
                    xml.writeAttribute("u", QString::number(uv.u, 'g', 9));
                    xml.writeAttribute("v", QString::number(uv.v, 'g', 9));
                }
                xml.writeEndElement();
            }

            if (mesh->GetNormals().size() == mesh->GetVertices().size()) {
                xml.writeStartElement("normals");
                for (const Vec3& normal : mesh->GetNormals()) {
                    xml.writeEmptyElement("normal");
                    xml.writeAttribute("x", QString::number(normal.x, 'g', 9));
                    xml.writeAttribute("y", QString::number(normal.y, 'g', 9));
                    xml.writeAttribute("z", QString::number(normal.z, 'g', 9));
                }
                xml.writeEndElement();
            }

            xml.writeStartElement("faces");
            for (const CMesh3D::Face& face : mesh->GetFaces()) {
                QStringList indices;
                for (size_t index : face) {
                    indices.push_back(QString::number(index));
                }
                xml.writeTextElement("face", indices.join(' '));
            }
            xml.writeEndElement();
            xml.writeEndElement();
        } else if (const auto* solid = dynamic_cast<const CSolid*>(&object)) {
            if (!solid->Save(xml, error)) {
                return false;
            }
            write_surface_texture_transforms(xml, *solid);
            if (!write_boolean_tools(xml, *solid, error)) {
                return false;
            }
        }

        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndDocument();

    if (xml.hasError()) {
        error = "Could not write XML project.";
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool Dom3DProjectSerializer::Load(const QString& path,
                                  CAlfaDoc& document,
                                  QString& active_room,
                                  ProjectViewState& view_state,
                                  QString& error) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QDomDocument dom;
    QString parse_error;
    int error_line = 0;
    int error_column = 0;
    if (!dom.setContent(&file, &parse_error, &error_line, &error_column)) {
        error = QString("XML parse error at line %1, column %2: %3").arg(error_line).arg(error_column).arg(parse_error);
        return false;
    }

    const QDomElement root = dom.documentElement();
    if (root.tagName() != "dom3dProject") {
        error = "Unsupported XML root element.";
        return false;
    }
    if (root.attribute("version").isEmpty()) {
        error = "Missing project version.";
        return false;
    }
    if (root.attribute("version") != kProjectVersion) {
        error = QString("Unsupported Dom3D project version '%1'.").arg(root.attribute("version"));
        return false;
    }

    const QDomElement metadata = root.firstChildElement("metadata");
    active_room = metadata.firstChildElement("activeRoom").text();
    if (active_room.isEmpty()) {
        active_room = "Architecture";
    }
    if (!read_view_state(metadata, view_state, error)) {
        return false;
    }

    std::vector<Material> loaded_materials = Material::InitialDocumentMaterials();
    const QDomElement materials_element = root.firstChildElement("materials");
    if (!materials_element.isNull()) {
        loaded_materials.clear();
        for (QDomElement material_element = materials_element.firstChildElement("material");
             !material_element.isNull();
             material_element = material_element.nextSiblingElement("material")) {
            Material material;
            if (!read_material_element(material_element, material, error)) {
                return false;
            }
            upsert_loaded_material(loaded_materials, material);
        }
        if (loaded_materials.empty()) {
            loaded_materials = Material::InitialDocumentMaterials();
        }
    }

    const QDomElement objects_element = required_child(root, "objects", error);
    if (objects_element.isNull()) {
        return false;
    }

    CAlfaDoc::ObjectList loaded_objects;
    for (QDomElement object_element = objects_element.firstChildElement("object");
         !object_element.isNull();
         object_element = object_element.nextSiblingElement("object")) {
        const QString type = object_element.attribute("type");
        if (type.isEmpty()) {
            error = "Object is missing type.";
            return false;
        }

        std::unique_ptr<CAlfaObject> object;
        if (type == "Curve") {
            auto polyline = std::make_unique<CPolyline>(object_element.attribute("name", "Curve").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            const bool closed = geometry.attribute("closed", "false") == "true";

            for (QDomElement point_element = geometry.firstChildElement("point");
                 !point_element.isNull();
                 point_element = point_element.nextSiblingElement("point")) {
                float x = 0.0f;
                float y = 0.08f;
                float z = 0.0f;
                if (!read_float_attr(point_element, "x", x, error)
                    || !read_float_attr(point_element, "z", z, error)) {
                    return false;
                }
                if (point_element.hasAttribute("y") && !read_float_attr(point_element, "y", y, error)) {
                    return false;
                }
                polyline->AddPoint(CPoint3d(x, y, z));
            }
            polyline->SetClosed(closed);
            object = std::move(polyline);
        } else if (type == "BSpline") {
            auto spline = std::make_unique<CBSpline>(object_element.attribute("name", "B-Spline").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            const bool closed = geometry.attribute("closed", "false") == "true";

            for (QDomElement point_element = geometry.firstChildElement("point");
                 !point_element.isNull();
                 point_element = point_element.nextSiblingElement("point")) {
                float x = 0.0f;
                float y = 0.08f;
                float z = 0.0f;
                if (!read_float_attr(point_element, "x", x, error)
                    || !read_float_attr(point_element, "z", z, error)) {
                    return false;
                }
                if (point_element.hasAttribute("y") && !read_float_attr(point_element, "y", y, error)) {
                    return false;
                }
                spline->AddPoint(CPoint3d(x, y, z));
            }
            spline->SetClosed(closed);
            object = std::move(spline);
        } else if (type == "CadCurve3D") {
            auto cad_curve = std::make_unique<CCadCurve3D>(object_element.attribute("name", "CAD Curve").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }

            std::vector<Vec3> points;
            for (QDomElement point_element = geometry.firstChildElement("point");
                 !point_element.isNull();
                 point_element = point_element.nextSiblingElement("point")) {
                Vec3 point{};
                if (!read_float_attr(point_element, "x", point.x, error)
                    || !read_float_attr(point_element, "y", point.y, error)
                    || !read_float_attr(point_element, "z", point.z, error)) {
                    return false;
                }
                points.push_back(point);
            }
            cad_curve->SetPoints(std::move(points));
            object = std::move(cad_curve);
        } else if (type == "Mesh") {
            auto mesh = std::make_unique<CMesh3D>(object_element.attribute("name", "Mesh3D").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }

            std::vector<Vec3> vertices;
            const QDomElement vertices_element = required_child(geometry, "vertices", error);
            if (vertices_element.isNull()) {
                return false;
            }
            for (QDomElement vertex_element = vertices_element.firstChildElement("vertex");
                 !vertex_element.isNull();
                 vertex_element = vertex_element.nextSiblingElement("vertex")) {
                Vec3 vertex{};
                if (!read_float_attr(vertex_element, "x", vertex.x, error)
                    || !read_float_attr(vertex_element, "y", vertex.y, error)
                    || !read_float_attr(vertex_element, "z", vertex.z, error)) {
                    return false;
                }
                vertices.push_back(vertex);
            }

            std::vector<UV> uvs;
            const QDomElement uvs_element = geometry.firstChildElement("uvs");
            if (!uvs_element.isNull()) {
                for (QDomElement uv_element = uvs_element.firstChildElement("uv");
                     !uv_element.isNull();
                     uv_element = uv_element.nextSiblingElement("uv")) {
                    UV uv{};
                    if (!read_float_attr(uv_element, "u", uv.u, error)
                        || !read_float_attr(uv_element, "v", uv.v, error)) {
                        return false;
                    }
                    uvs.push_back(uv);
                }
                if (!uvs.empty() && uvs.size() != vertices.size()) {
                    error = "Mesh UV count must match vertex count.";
                    return false;
                }
            }

            std::vector<CMesh3D::Face> faces;
            std::vector<Vec3> normals;
            const QDomElement normals_element = geometry.firstChildElement("normals");
            if (!normals_element.isNull()) {
                for (QDomElement normal_element = normals_element.firstChildElement("normal");
                     !normal_element.isNull();
                     normal_element = normal_element.nextSiblingElement("normal")) {
                    Vec3 normal{};
                    if (!read_float_attr(normal_element, "x", normal.x, error)
                        || !read_float_attr(normal_element, "y", normal.y, error)
                        || !read_float_attr(normal_element, "z", normal.z, error)) {
                        return false;
                    }
                    normals.push_back(normal);
                }
                if (!normals.empty() && normals.size() != vertices.size()) {
                    error = "Mesh normal count must match vertex count.";
                    return false;
                }
            }

            const QDomElement faces_element = required_child(geometry, "faces", error);
            if (faces_element.isNull()) {
                return false;
            }
            for (QDomElement face_element = faces_element.firstChildElement("face");
                 !face_element.isNull();
                 face_element = face_element.nextSiblingElement("face")) {
                CMesh3D::Face face;
                const QStringList parts = face_element.text().split(' ', Qt::SkipEmptyParts);
                for (const QString& part : parts) {
                    bool ok = false;
                    const quint64 index = part.toULongLong(&ok);
                    if (!ok) {
                        error = "Mesh face contains an unsupported vertex index.";
                        return false;
                    }
                    face.push_back(static_cast<size_t>(index));
                }
                if (face.size() < 3) {
                    error = "Mesh face has fewer than 3 vertices.";
                    return false;
                }
                faces.push_back(std::move(face));
            }

            if (!mesh->SetGeometry(std::move(vertices),
                                   std::move(faces),
                                   std::move(uvs),
                                   std::move(normals))) {
                error = "Mesh geometry is invalid.";
                return false;
            }
            object = std::move(mesh);
        } else if (type == "Solid" || type == "SurfaceSet") {
            std::unique_ptr<CSolid> solid = CSolid::Load(object_element, error);
            if (!solid) {
                return false;
            }
            if (!read_boolean_tools(object_element, *solid, error)) {
                return false;
            }
            if (type == "SurfaceSet") {
                TopoDS_Shape shape = solid->m_Shape;
                auto surface_set = std::make_unique<CSurfaceSet>(shape);
                surface_set->SetName(object_element.attribute("name", "Surface Set").toStdString());
                surface_set->InitSurfaces();
                surface_set->InitEdges();
                surface_set->BuldMesh(0.1f);
                object = std::move(surface_set);
            } else {
                solid->SetName(object_element.attribute("name", "Solid").toStdString());
                object = std::move(solid);
            }
        } else {
            error = QString("Unsupported object type '%1'.").arg(type);
            return false;
        }

        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            if (!read_surface_texture_transforms(object_element, *solid, error)) {
                return false;
            }
        }
        if (!read_material(object_element, *object, error)) {
            return false;
        }
        if (object_element.hasAttribute("id")) {
            bool ok = false;
            const unsigned long object_id = object_element.attribute("id").toULong(&ok);
            if (ok) {
                object->m_id = object_id;
            }
        }
        if (object_element.hasAttribute("materialId")) {
            bool ok = false;
            const unsigned long material_id = object_element.attribute("materialId").toULong(&ok);
            if (ok) {
                object->SetMaterialId(material_id);
                if (const Material* material = find_loaded_material(loaded_materials, material_id)) {
                    object->SetMaterial(*material);
                }
            }
        }
        upsert_loaded_material(loaded_materials, object->GetMaterial());
        read_parametric_definition(object_element, *object);
        object->SetGroupName(object_element.attribute("group").toStdString());
        object->SetVisible(object_element.attribute("visible", "true") != "false");
        loaded_objects.push_back(std::move(object));
    }

    document.Clear();
    document.GetMaterials() = std::move(loaded_materials);
    document.GetObjects() = std::move(loaded_objects);
    document.EnsureObjectIds();
    if (document.GetObjects().empty()) {
        document.CreatePolyline();
    }
    document.ClearSelection();
    return true;
}
