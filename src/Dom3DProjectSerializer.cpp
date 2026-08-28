#include "Dom3DProjectSerializer.h"

#include "CadCurve3D.h"
#include "BezierSpline.h"
#include "CBSpline.h"
#include "CMesh3D.h"
#include "ReferenceImage.h"
#include "CGroup.h"
#include "CPart.h"
#include "CAssembled.h"
#include "CKitchenCabinet.h"
#include "CPolyline.h"
#include "DrawingText.h"
#include "LinkLineHor.h"
#include "LinkLineVert.h"
#include "SmartLine.h"
#include "SketchArcLine.h"
#include "solid/Solid.h"
#include "solid/AssociativeClone.h"
#include "solid/SurfaceSet.h"

#include <QDomDocument>
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QStringList>
#include <QXmlStreamWriter>

#include <cmath>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
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
    if (dynamic_cast<const CKitchenCabinet*>(&object)) {
        return "KitchenCabinet";
    }
    if (dynamic_cast<const CAssembled*>(&object)) {
        return "Assembly";
    }
    if (dynamic_cast<const CPart*>(&object)) {
        return "Part";
    }
    if (dynamic_cast<const CGroup*>(&object)) {
        return "Group";
    }
    if (dynamic_cast<const CAssociativeClone*>(&object)) {
        return "AssociativeClone";
    }
    if (dynamic_cast<const CSurfaceSet*>(&object)) {
        return "SurfaceSet";
    }
    if (dynamic_cast<const CSolid*>(&object)) {
        return "Solid";
    }
    if (dynamic_cast<const CReferenceImage*>(&object)) {
        return "ReferenceImage";
    }
    if (dynamic_cast<const CMesh3D*>(&object)) {
        return "Mesh";
    }
    if (dynamic_cast<const CPolyline*>(&object)) {
        return "Curve";
    }
    if (dynamic_cast<const CDrawingText*>(&object)) {
        return "DrawingText";
    }
    if (dynamic_cast<const CSmartLine*>(&object)) {
        return "Sketch";
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
    if (dynamic_cast<const CSmartLine*>(&object)) {
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
        xml.writeAttribute("fitToSurface", transform.fit_to_surface ? "1" : "0");
        if (surface->MaterialOverride.enabled) {
            xml.writeAttribute("materialId", QString::number(surface->MaterialOverride.material_id));
        }
        if (surface->MaterialOverride.coating_enabled) {
            xml.writeAttribute(
                "coatingMaterialId",
                QString::number(surface->MaterialOverride.coating_material_id));
        }
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

bool read_surface_texture_transforms(const QDomElement& object_element,
                                     CSolid& solid,
                                     const std::vector<Material>& materials,
                                     QString& error) {
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
        transform.fit_to_surface = surface_element.attribute("fitToSurface", "0") == "1";
        if (!surface_element.hasAttribute("scaleU")) {
            transform.scale_u = 1.0f;
        }
        if (!surface_element.hasAttribute("scaleV")) {
            transform.scale_v = 1.0f;
        }
        solid.SetSurfaceTextureTransform(index, transform);
        if (surface_element.hasAttribute("materialId")) {
            bool material_id_ok = false;
            const unsigned long material_id = surface_element.attribute("materialId").toULong(&material_id_ok);
            if (!material_id_ok) {
                error = "Surface material contains an invalid material ID.";
                return false;
            }
            const auto material = std::find_if(
                materials.begin(), materials.end(),
                [material_id](const Material& candidate) { return candidate.id == material_id; });
            if (material != materials.end()) {
                solid.SetSurfaceMaterial(index, *material);
            }
        }
        if (surface_element.hasAttribute("coatingMaterialId")) {
            bool material_id_ok = false;
            const unsigned long material_id = surface_element
                .attribute("coatingMaterialId").toULong(&material_id_ok);
            if (!material_id_ok) {
                error = "Surface coating contains an invalid material ID.";
                return false;
            }
            const auto material = std::find_if(
                materials.begin(), materials.end(),
                [material_id](const Material& candidate) {
                    return candidate.id == material_id;
                });
            if (material != materials.end()) {
                solid.SetSurfaceCoating(index, *material);
            }
        }
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
    xml.writeAttribute("verticalFovDegrees", QString::number(
        view_state.camera.vertical_fov_degrees, 'g', 9));

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
    if (!read_float_attr(camera_element, "verticalFovDegrees",
                         camera.vertical_fov_degrees, error, false)) {
        return false;
    }
    camera.vertical_fov_degrees = std::clamp(
        camera.vertical_fov_degrees, 20.0f, 100.0f);

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
    xml.writeAttribute("roughness", QString::number(material.roughness, 'g', 9));
    xml.writeAttribute("metallic", QString::number(material.metallic, 'g', 9));
    xml.writeAttribute("coatWeight", QString::number(material.coat_weight, 'g', 9));
    xml.writeAttribute("coatRoughness", QString::number(material.coat_roughness, 'g', 9));
    xml.writeAttribute("normalStrength", QString::number(material.normal_strength, 'g', 9));
    xml.writeAttribute("displacementScale", QString::number(material.displacement_scale, 'g', 9));
    xml.writeAttribute("textureOffsetU", QString::number(material.texture_offset_u, 'g', 9));
    xml.writeAttribute("textureOffsetV", QString::number(material.texture_offset_v, 'g', 9));
    xml.writeAttribute("textureScaleU", QString::number(material.texture_scale_u, 'g', 9));
    xml.writeAttribute("textureScaleV", QString::number(material.texture_scale_v, 'g', 9));
    xml.writeAttribute("textureRotation", QString::number(material.texture_rotation_degrees, 'g', 9));
    xml.writeAttribute("textureFitToSurface", material.texture_fit_to_surface ? "1" : "0");
    xml.writeAttribute("colorTexture", QString::fromStdString(material.color_texture_path));
    xml.writeAttribute("lightTexture", QString::fromStdString(material.light_texture_path));
    xml.writeAttribute("bumpTexture", QString::fromStdString(material.bump_texture_path));
    xml.writeAttribute("normalTexture", QString::fromStdString(material.normal_texture_path));
    xml.writeAttribute("roughnessTexture", QString::fromStdString(material.roughness_texture_path));
    xml.writeAttribute("metallicTexture", QString::fromStdString(material.metallic_texture_path));
    xml.writeAttribute("displacementTexture", QString::fromStdString(material.displacement_texture_path));
    xml.writeEndElement();
}

bool read_double_attr(const QDomElement& element, const char* name, double& value, QString& error) {
    if (!element.hasAttribute(name)) {
        error = QString("Missing XML attribute '%1'.").arg(name);
        return false;
    }
    bool ok = false;
    value = element.attribute(name).toDouble(&ok);
    if (!ok) {
        error = QString("Unsupported number in XML attribute '%1'.").arg(name);
        return false;
    }
    return true;
}

void write_layers(QXmlStreamWriter& xml, const CAlfaDoc& document) {
    xml.writeStartElement("layers");
    xml.writeAttribute("workLayer", QString::number(document.GetWorkLayerID()));
    for (const CLayer* layer : document.m_Layers) {
        if (!layer) {
            continue;
        }
        xml.writeEmptyElement("layer");
        xml.writeAttribute("id", QString::number(layer->ID()));
        xml.writeAttribute("name", QString::fromStdString(layer->Name));
        xml.writeAttribute("visible", layer->Visible ? "true" : "false");
        xml.writeAttribute("selectable", layer->Selectable ? "true" : "false");
    }
    xml.writeEndElement();
}

std::vector<CLayer*> read_layers(const QDomElement& root, int& work_layer) {
    std::vector<CLayer*> layers;
    const QDomElement layers_element = root.firstChildElement("layers");
    if (layers_element.isNull()) {
        layers.push_back(new CLayer(1, "Default"));
        work_layer = 1;
        return layers;
    }

    bool work_ok = false;
    work_layer = layers_element.attribute("workLayer", "1").toInt(&work_ok);
    if (!work_ok) {
        work_layer = 1;
    }

    for (QDomElement layer_element = layers_element.firstChildElement("layer");
         !layer_element.isNull();
         layer_element = layer_element.nextSiblingElement("layer")) {
        bool id_ok = false;
        const int id = layer_element.attribute("id").toInt(&id_ok);
        if (!id_ok || id <= 0) {
            continue;
        }
        auto* layer = new CLayer(id, layer_element.attribute("name", QString("Layer %1").arg(id)).toStdString());
        layer->Visible = layer_element.attribute("visible", "true") != "false";
        layer->Selectable = layer_element.attribute("selectable", "true") != "false";
        layers.push_back(layer);
    }

    if (layers.empty()) {
        layers.push_back(new CLayer(1, "Default"));
        work_layer = 1;
    }
    return layers;
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
        || !read_float_attr(material_element, "roughness", material.roughness, error, false)
        || !read_float_attr(material_element, "metallic", material.metallic, error, false)
        || !read_float_attr(material_element, "coatWeight", material.coat_weight, error, false)
        || !read_float_attr(material_element, "coatRoughness", material.coat_roughness, error, false)
        || !read_float_attr(material_element, "normalStrength", material.normal_strength, error, false)
        || !read_float_attr(material_element, "displacementScale", material.displacement_scale, error, false)
        || !read_float_attr(material_element, "textureOffsetU", material.texture_offset_u, error, false)
        || !read_float_attr(material_element, "textureOffsetV", material.texture_offset_v, error, false)
        || !read_float_attr(material_element, "textureScaleU", material.texture_scale_u, error, false)
        || !read_float_attr(material_element, "textureScaleV", material.texture_scale_v, error, false)
        || !read_float_attr(material_element, "textureRotation", material.texture_rotation_degrees, error, false)) {
        return false;
    }
    material.color_texture_path = material_element.attribute("colorTexture", QString::fromStdString(material.color_texture_path)).toStdString();
    material.texture_fit_to_surface = material_element.attribute("textureFitToSurface", "0") == "1";
    material.light_texture_path = material_element.attribute("lightTexture", QString::fromStdString(material.light_texture_path)).toStdString();
    material.bump_texture_path = material_element.attribute("bumpTexture", QString::fromStdString(material.bump_texture_path)).toStdString();
    material.normal_texture_path = material_element.attribute("normalTexture", QString::fromStdString(material.normal_texture_path)).toStdString();
    material.roughness_texture_path = material_element.attribute("roughnessTexture", QString::fromStdString(material.roughness_texture_path)).toStdString();
    material.metallic_texture_path = material_element.attribute("metallicTexture", QString::fromStdString(material.metallic_texture_path)).toStdString();
    material.displacement_texture_path = material_element.attribute("displacementTexture", QString::fromStdString(material.displacement_texture_path)).toStdString();
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
            if (!operation->Name.empty()) {
                xml.writeAttribute(
                    "name", QString::fromStdString(operation->Name));
            }
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
                                          operation.attribute("name").toStdString(),
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
                                  const QImage& thumbnail,
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
    if (!thumbnail.isNull()) {
        QByteArray thumbnail_data;
        QBuffer thumbnail_buffer(&thumbnail_data);
        thumbnail_buffer.open(QIODevice::WriteOnly);
        if (!thumbnail.save(&thumbnail_buffer, "PNG")) {
            error = "Could not encode project thumbnail.";
            return false;
        }
        xml.writeStartElement("thumbnail");
        xml.writeAttribute("format", "png");
        xml.writeAttribute("width", QString::number(thumbnail.width()));
        xml.writeAttribute("height", QString::number(thumbnail.height()));
        xml.writeCharacters(QString::fromLatin1(thumbnail_data.toBase64()));
        xml.writeEndElement();
    }
    xml.writeEndElement();

    if (!document.GetDraftingData().empty()) {
        xml.writeStartElement("drafting");
        xml.writeAttribute("encoding", "json-base64");
        xml.writeCharacters(QString::fromLatin1(
            QByteArray::fromStdString(document.GetDraftingData()).toBase64()));
        xml.writeEndElement();
    }

    xml.writeStartElement("materials");
    for (const Material& material : document.GetMaterials()) {
        write_material(xml, material);
    }
    xml.writeEndElement();

    write_layers(xml, document);

    const auto& objects = document.GetObjects();
    std::vector<const CSolid*> packed_solids;
    std::unordered_map<const CSolid*, size_t> packed_solid_indices;
    packed_solids.reserve(objects.size());
    for (const auto& object : objects) {
        const auto* solid = object
            ? dynamic_cast<const CSolid*>(object.get()) : nullptr;
        if (!solid) {
            continue;
        }
        packed_solid_indices.emplace(solid, packed_solids.size());
        packed_solids.push_back(solid);
    }
    if (!packed_solids.empty()) {
        QByteArray packed_geometry;
        if (!CSolid::SaveShapePack(
                packed_solids, packed_geometry, error)) {
            return false;
        }
        xml.writeStartElement("geometryStore");
        xml.writeAttribute("kind", "brep-native-pack");
        xml.writeAttribute("encoding", "base64-zlib");
        xml.writeAttribute("count", QString::number(packed_solids.size()));
        xml.writeCharacters(QString::fromLatin1(
            qCompress(packed_geometry, 6).toBase64()));
        xml.writeEndElement();
    }

    xml.writeStartElement("objects");
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!objects[i]) {
            continue;
        }
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
        const Color object_color = object.GetColor();
        xml.writeAttribute("objectColorR", QString::number(object_color.r, 'g', 9));
        xml.writeAttribute("objectColorG", QString::number(object_color.g, 'g', 9));
        xml.writeAttribute("objectColorB", QString::number(object_color.b, 'g', 9));
        xml.writeAttribute("visible", object.IsVisible() ? "true" : "false");
        xml.writeAttribute("layerId", QString::number(object.m_LayerID));
        xml.writeAttribute("lineWidth", QString::number(object.GetLineWidth(), 'g', 9));
        xml.writeAttribute("lineStyle", QString::fromStdString(object.GetLineStyle()));
        if (const auto* clone = dynamic_cast<const CAssociativeClone*>(&object)) {
            xml.writeAttribute("sourceId", QString::number(clone->GetSourceId()));
            const gp_GTrsf& placement = clone->GetPlacement();
            for (int row = 1; row <= 3; ++row) {
                for (int column = 1; column <= 4; ++column) {
                    xml.writeAttribute(QString("p%1%2").arg(row).arg(column),
                        QString::number(placement.Value(row, column), 'g', 17));
                }
            }
        }

        write_material(xml, object.GetMaterial());
        write_identity_transform(xml);
        write_parameters(xml, object);
        write_operation_history(xml, object);

        if (const auto* group = dynamic_cast<const CGroup*>(&object)) {
            xml.writeStartElement("geometry");
            const auto* assembly = dynamic_cast<const CAssembled*>(group);
            const auto* part = dynamic_cast<const CPart*>(group);
            xml.writeAttribute("kind", assembly ? "assembly" : part ? "part" : "group");
            if (part) {
                xml.writeAttribute("fileLinked", part->IsFileLinked() ? "true" : "false");
                if (!part->GetSourcePath().empty()) {
                    xml.writeAttribute("sourcePath", QString::fromStdString(part->GetSourcePath()));
                }
            }
            if (assembly) {
                xml.writeAttribute("drawParam", QString::number(assembly->GetDrawParam()));
                xml.writeAttribute("idDim", QString::number(assembly->GetIdDim()));
                const CAssembled::TransformMatrix& transform =
                    assembly->GetAssemblyTransform();
                for (int row = 0; row < 4; ++row) {
                    for (int column = 0; column < 4; ++column) {
                        xml.writeAttribute(
                            QString("transform%1%2").arg(row).arg(column),
                            QString::number(
                                transform[static_cast<size_t>(row * 4 + column)],
                                'g', 17));
                    }
                }
                if (const auto* cabinet = dynamic_cast<const CKitchenCabinet*>(assembly)) {
                    const KitchenCabinetDefinition& definition = cabinet->GetDefinition();
                    xml.writeAttribute("bodyType", QString::number(static_cast<unsigned int>(definition.body_type)));
                    xml.writeAttribute("facadeType", QString::number(static_cast<unsigned int>(definition.facade_type)));
                    xml.writeAttribute("facadeStyle", QString::number(static_cast<unsigned int>(definition.facade_style)));
                    xml.writeAttribute("showcaseFill", QString::number(static_cast<unsigned int>(definition.showcase_fill)));
                    xml.writeAttribute("shelfCount", QString::number(definition.shelf_count));
                    xml.writeAttribute("width", QString::number(definition.width, 'g', 17));
                    xml.writeAttribute("depth", QString::number(definition.depth, 'g', 17));
                    xml.writeAttribute("height", QString::number(definition.height, 'g', 17));
                    xml.writeAttribute("panelThickness", QString::number(definition.panel_thickness, 'g', 17));
                    xml.writeAttribute("millingDepth", QString::number(definition.milling_depth, 'g', 17));
                    xml.writeAttribute("facadeBulge", QString::number(definition.facade_bulge, 'g', 17));
                    xml.writeAttribute("radius2Bulge", QString::number(definition.radius2_bulge, 'g', 17));
                    xml.writeAttribute("radiusSideStraight", QString::number(definition.radius_side_straight, 'g', 17));
                    xml.writeAttribute("doorOpenAngle", QString::number(definition.door_open_angle, 'g', 17));
                    xml.writeAttribute("leftDoorOpenAngle", QString::number(definition.left_door_open_angle, 'g', 17));
                    xml.writeAttribute("rightDoorOpenAngle", QString::number(definition.right_door_open_angle, 'g', 17));
                    xml.writeAttribute("doorHingeSide", QString::number(definition.door_hinge_side));
                    xml.writeAttribute("handleOrientation", QString::number(definition.handle_orientation));
                }
            }
            for (unsigned long id : group->GetElementIds()) {
                xml.writeEmptyElement("element");
                xml.writeAttribute("id", QString::number(id));
            }
            if (assembly) {
                for (const CDimens3D* dimension : assembly->GetDimensions()) {
                    if (!dimension) {
                        continue;
                    }
                    xml.writeEmptyElement("dimension");
                    xml.writeAttribute("parameter", QString::fromStdString(dimension->GetParameterId()));
                    xml.writeAttribute("label", QString::fromStdString(dimension->GetLabel()));
                    xml.writeAttribute("value", QString::number(dimension->GetValue(), 'g', 17));
                    xml.writeAttribute("visible", dimension->IsVisible() ? "true" : "false");
                    xml.writeAttribute("sx", QString::number(dimension->GetStart().x, 'g', 17));
                    xml.writeAttribute("sy", QString::number(dimension->GetStart().y, 'g', 17));
                    xml.writeAttribute("sz", QString::number(dimension->GetStart().z, 'g', 17));
                    xml.writeAttribute("ex", QString::number(dimension->GetEnd().x, 'g', 17));
                    xml.writeAttribute("ey", QString::number(dimension->GetEnd().y, 'g', 17));
                    xml.writeAttribute("ez", QString::number(dimension->GetEnd().z, 'g', 17));
                    xml.writeAttribute("ox", QString::number(dimension->GetOffsetDirection().x, 'g', 17));
                    xml.writeAttribute("oy", QString::number(dimension->GetOffsetDirection().y, 'g', 17));
                    xml.writeAttribute("oz", QString::number(dimension->GetOffsetDirection().z, 'g', 17));
                    xml.writeAttribute("offset", QString::number(dimension->GetOffsetDistance(), 'g', 17));
                }
            }
            xml.writeEndElement();
        } else if (const auto* text = dynamic_cast<const CDrawingText*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "drawing-text");
            xml.writeAttribute("text", QString::fromStdString(text->GetText()));
            xml.writeAttribute("x", QString::number(text->GetInsertion().x, 'g', 17));
            xml.writeAttribute("y", QString::number(text->GetInsertion().y, 'g', 17));
            xml.writeAttribute("z", QString::number(text->GetInsertion().z, 'g', 17));
            xml.writeAttribute("height", QString::number(text->GetHeight(), 'g', 17));
            xml.writeAttribute("rotation", QString::number(text->GetRotationDegrees(), 'g', 17));
            xml.writeAttribute("font", QString::fromStdString(text->GetFontFamily()));
            xml.writeEndElement();
        } else if (const auto* polyline = dynamic_cast<const CPolyline*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "polyline");
            xml.writeAttribute("closed", polyline->IsClosed() ? "true" : "false");
            for (size_t point_index = 0; point_index < polyline->GetPoints().size(); ++point_index) {
                const CPoint3d& point = polyline->GetPoints()[point_index];
                xml.writeEmptyElement("point");
                xml.writeAttribute("x", QString::number(point.x, 'g', 9));
                xml.writeAttribute("y", QString::number(point.y, 'g', 9));
                xml.writeAttribute("z", QString::number(point.z, 'g', 9));
                const double radius = polyline->GetVertexRadius(point_index);
                if (radius > 0.0) {
                    xml.writeAttribute("radius", QString::number(radius, 'g', 17));
                }
            }
            xml.writeEndElement();
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "parametric-sketch");
            xml.writeAttribute("closed", sketch->IsClosed() ? "true" : "false");

            const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
            xml.writeEmptyElement("origin");
            xml.writeAttribute("x", QString::number(system.origin.x, 'g', 17));
            xml.writeAttribute("y", QString::number(system.origin.y, 'g', 17));
            xml.writeAttribute("z", QString::number(system.origin.z, 'g', 17));
            xml.writeEmptyElement("xAxis");
            xml.writeAttribute("x", QString::number(system.x_axis.x, 'g', 17));
            xml.writeAttribute("y", QString::number(system.x_axis.y, 'g', 17));
            xml.writeAttribute("z", QString::number(system.x_axis.z, 'g', 17));
            xml.writeEmptyElement("normal");
            xml.writeAttribute("x", QString::number(system.normal.x, 'g', 17));
            xml.writeAttribute("y", QString::number(system.normal.y, 'g', 17));
            xml.writeAttribute("z", QString::number(system.normal.z, 'g', 17));
            if (sketch->HasFaceAttachment()) {
                const SketchFaceAttachment& attachment = sketch->GetFaceAttachment();
                xml.writeEmptyElement("faceAttachment");
                xml.writeAttribute("bodyId", QString::number(attachment.body_id));
                xml.writeAttribute("faceIndex", QString::number(attachment.face_index));
            }

            xml.writeStartElement("lines");
            for (std::size_t line_index = 0; line_index < sketch->GetNumLines(); ++line_index) {
                const CLinkLine* line = sketch->GetLine(line_index);
                xml.writeEmptyElement("line");
                const char* line_type = "segment";
                if (line->GetType() == LinkLineType::Horizontal) {
                    line_type = "horizontal";
                } else if (line->GetType() == LinkLineType::Vertical) {
                    line_type = "vertical";
                } else if (line->GetType() == LinkLineType::Bezier) {
                    line_type = "bezier";
                } else if (line->GetType() == LinkLineType::Arc) {
                    line_type = "arc";
                }
                xml.writeAttribute("type", line_type);
                xml.writeAttribute("x1", QString::number(line->GetStart().x, 'g', 17));
                xml.writeAttribute("y1", QString::number(line->GetStart().y, 'g', 17));
                xml.writeAttribute("x2", QString::number(line->GetEnd().x, 'g', 17));
                xml.writeAttribute("y2", QString::number(line->GetEnd().y, 'g', 17));
                if (const auto* bezier = dynamic_cast<const CBezierSpline*>(line)) {
                    xml.writeAttribute("cx1", QString::number(bezier->GetControl1().x, 'g', 17));
                    xml.writeAttribute("cy1", QString::number(bezier->GetControl1().y, 'g', 17));
                    xml.writeAttribute("cx2", QString::number(bezier->GetControl2().x, 'g', 17));
                    xml.writeAttribute("cy2", QString::number(bezier->GetControl2().y, 'g', 17));
                } else if (const auto* arc = dynamic_cast<const CSketchArcLine*>(line)) {
                    xml.writeAttribute("mx", QString::number(arc->GetPointOnArc().x, 'g', 17));
                    xml.writeAttribute("my", QString::number(arc->GetPointOnArc().y, 'g', 17));
                }
            }
            xml.writeEndElement();

            xml.writeStartElement("constraints");
            for (std::size_t constraint_index = 0;
                 constraint_index < sketch->GetNumConstraints();
                 ++constraint_index) {
                const CConstraint* constraint = sketch->GetConstraint(constraint_index);
                xml.writeEmptyElement("constraint");
                QString constraint_type;
                switch (constraint->GetType()) {
                case ConstraintType::Horizontal:
                    constraint_type = "horizontal";
                    break;
                case ConstraintType::Vertical:
                    constraint_type = "vertical";
                    break;
                case ConstraintType::TangentAtStart:
                    constraint_type = "tangent-start";
                    break;
                case ConstraintType::TangentAtEnd:
                    constraint_type = "tangent-end";
                    break;
                }
                xml.writeAttribute("type", constraint_type);
                xml.writeAttribute("line", QString::number(constraint->GetLineIndex()));
            }
            xml.writeEndElement();

            xml.writeStartElement("fillets");
            for (std::size_t fillet_index = 0; fillet_index < sketch->GetNumFillets(); ++fillet_index) {
                const CFillet* fillet = sketch->GetFillet(fillet_index);
                xml.writeEmptyElement("fillet");
                xml.writeAttribute("firstLine", QString::number(fillet->GetFirstLineIndex()));
                xml.writeAttribute("secondLine", QString::number(fillet->GetSecondLineIndex()));
                xml.writeAttribute("radius", QString::number(fillet->GetRadius(), 'g', 17));
            }
            xml.writeEndElement();
            xml.writeEndElement();
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "b-spline");
            const char* curve_type = spline->GetCurveType() == SplineCurveType::Bezier
                ? "bezier" : spline->GetCurveType() == SplineCurveType::Nurbs
                    ? "nurbs" : "b-spline";
            xml.writeAttribute("curveType", curve_type);
            xml.writeAttribute("degree", QString::number(spline->GetDegree()));
            xml.writeAttribute("closed", spline->IsClosed() ? "true" : "false");
            const std::vector<double>& weights = spline->GetWeights();
            for (size_t point_index = 0;
                 point_index < spline->GetPoints().size(); ++point_index) {
                const CPoint3d& point = spline->GetPoints()[point_index];
                xml.writeEmptyElement("point");
                xml.writeAttribute("x", QString::number(point.x, 'g', 9));
                xml.writeAttribute("y", QString::number(point.y, 'g', 9));
                xml.writeAttribute("z", QString::number(point.z, 'g', 9));
                if (spline->GetCurveType() == SplineCurveType::Nurbs) {
                    xml.writeAttribute(
                        "weight",
                        QString::number(
                            point_index < weights.size() ? weights[point_index] : 1.0,
                            'g', 17));
                }
            }
            for (double knot : spline->GetKnots()) {
                xml.writeEmptyElement("knot");
                xml.writeAttribute("value", QString::number(knot, 'g', 17));
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
                for (const MeshCorner& corner : face.corners) {
                    indices.push_back(QString::number(corner.v));
                }
                xml.writeTextElement("face", indices.join(' '));
            }
            xml.writeEndElement();
            xml.writeEndElement();
        } else if (const auto* solid = dynamic_cast<const CSolid*>(&object)) {
            const auto packed = packed_solid_indices.find(solid);
            if (packed == packed_solid_indices.end()) {
                error = "Solid is missing from the project BRep pack.";
                return false;
            }
            xml.writeEmptyElement("geometry");
            xml.writeAttribute("kind", "brep-ref");
            xml.writeAttribute("index", QString::number(packed->second));
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

bool Dom3DProjectSerializer::LoadThumbnail(const QString& path, QImage& thumbnail, QString& error) const {
    thumbnail = QImage();

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

    const QDomElement thumbnail_element = root.firstChildElement("metadata").firstChildElement("thumbnail");
    if (thumbnail_element.isNull()) {
        return true;
    }

    const QByteArray thumbnail_data = QByteArray::fromBase64(thumbnail_element.text().toLatin1());
    if (thumbnail_data.isEmpty() || !thumbnail.loadFromData(thumbnail_data, "PNG")) {
        error = "Could not decode project thumbnail.";
        return false;
    }
    return true;
}

bool Dom3DProjectSerializer::Load(const QString& path,
                                  CAlfaDoc& document,
                                  QString& active_room,
                                  ProjectViewState& view_state,
                                  QString& error,
                                  const ProgressCallback& progress) const {
    const auto report = [&progress](int value, const QString& text) {
        if (progress) progress(value, text);
    };
    report(2, "Opening project file...");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QDomDocument dom;
    report(8, "Reading project data...");
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

    std::string loaded_drafting_data;
    const QDomElement drafting_element = root.firstChildElement("drafting");
    if (!drafting_element.isNull()) {
        if (drafting_element.attribute("encoding") != "json-base64") {
            error = "Unsupported Drafting data encoding.";
            return false;
        }
        loaded_drafting_data = QByteArray::fromBase64(
            drafting_element.text().toLatin1()).toStdString();
    }

    report(25, "Loading materials and layers...");
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

    int loaded_work_layer = 1;
    std::vector<CLayer*> loaded_layers = read_layers(root, loaded_work_layer);

    const QDomElement objects_element = required_child(root, "objects", error);
    if (objects_element.isNull()) {
        return false;
    }
    report(20, "Project data parsed");

    std::vector<TopoDS_Shape> packed_shapes;
    const QDomElement geometry_store = root.firstChildElement("geometryStore");
    if (!geometry_store.isNull()) {
        if (geometry_store.attribute("kind") != "brep-native-pack"
            || geometry_store.attribute("encoding") != "base64-zlib") {
            error = "Unsupported project geometry store.";
            return false;
        }
        report(32, "Decoding project geometry...");
        const QByteArray encoded = QByteArray::fromBase64(
            geometry_store.text().toLatin1());
        const QByteArray packed_data = qUncompress(encoded);
        report(40, "Loading BRep geometry...");
        if (packed_data.isEmpty()
            || !CSolid::LoadShapePack(packed_data, packed_shapes, error)) {
            if (error.isEmpty()) {
                error = "Project geometry store is empty or damaged.";
            }
            return false;
        }
        bool count_ok = false;
        const qsizetype expected_count = geometry_store.attribute("count")
            .toLongLong(&count_ok);
        if (!count_ok || expected_count < 0
            || expected_count != static_cast<qsizetype>(packed_shapes.size())) {
            error = "Project geometry store object count does not match.";
            return false;
        }
    }

    report(52, "Geometry loaded");
    CAlfaDoc::ObjectList loaded_objects;
    int object_count = 0;
    for (QDomElement element = objects_element.firstChildElement("object");
         !element.isNull();
         element = element.nextSiblingElement("object")) {
        ++object_count;
    }
    int object_index = 0;
    for (QDomElement object_element = objects_element.firstChildElement("object");
         !object_element.isNull();
         object_element = object_element.nextSiblingElement("object")) {
        report(
            52 + static_cast<int>(42LL * object_index
                / std::max(object_count, 1)),
            QString("Loading objects %1 / %2...")
                .arg(object_index + 1)
                .arg(object_count));
        const QString type = object_element.attribute("type");
        if (type.isEmpty()) {
            error = "Object is missing type.";
            return false;
        }

        std::unique_ptr<CAlfaObject> object;
        if (type == "Group" || type == "Part" || type == "Assembly" || type == "KitchenCabinet") {
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            std::vector<unsigned long> element_ids;
            for (QDomElement element = geometry.firstChildElement("element");
                 !element.isNull();
                 element = element.nextSiblingElement("element")) {
                bool ok = false;
                const unsigned long id = element.attribute("id").toULong(&ok);
                if (!ok || id == 0) {
                    error = "Group contains an invalid object ID.";
                    return false;
                }
                element_ids.push_back(id);
            }
            if (type == "Assembly" || type == "KitchenCabinet") {
                std::unique_ptr<CAssembled> assembly;
                if (type == "KitchenCabinet") {
                    KitchenCabinetDefinition definition;
                    bool body_ok = false;
                    const uint body_type = geometry.attribute("bodyType", "0").toUInt(&body_ok);
                    bool facade_ok = false;
                    const uint facade_type = geometry.attribute("facadeType", "2").toUInt(&facade_ok);
                    bool facade_style_ok = false;
                    const uint facade_style = geometry.attribute("facadeStyle", "0").toUInt(&facade_style_ok);
                    bool shelves_ok = false;
                    const int shelf_count = geometry.attribute("shelfCount", "2").toInt(&shelves_ok);
                    if (!body_ok || body_type > 6U || !facade_ok || facade_type > 2U
                        || !facade_style_ok || facade_style > 4U
                        || !shelves_ok
                        || !read_double_attr(geometry, "width", definition.width, error)
                        || !read_double_attr(geometry, "depth", definition.depth, error)
                        || !read_double_attr(geometry, "height", definition.height, error)
                        || !read_double_attr(geometry, "panelThickness", definition.panel_thickness, error)) {
                        if (error.isEmpty()) {
                            error = "Kitchen cabinet parameters are invalid.";
                        }
                        return false;
                    }
                    definition.body_type = static_cast<KitchenCabinetBodyType>(body_type);
                    definition.facade_type = static_cast<KitchenCabinetFacadeType>(facade_type);
                    definition.facade_style = static_cast<KitchenCabinetFacadeStyle>(facade_style);
                    bool showcase_fill_ok = false;
                    const uint showcase_fill = geometry.attribute(
                        "showcaseFill", "0").toUInt(&showcase_fill_ok);
                    if (!showcase_fill_ok || showcase_fill > 4U) {
                        error = "Kitchen cabinet showcase fill is invalid.";
                        return false;
                    }
                    definition.showcase_fill =
                        static_cast<KitchenCabinetShowcaseFill>(showcase_fill);
                    definition.shelf_count = shelf_count;
                    if (geometry.hasAttribute("millingDepth")
                        && !read_double_attr(
                            geometry, "millingDepth",
                            definition.milling_depth, error)) {
                        return false;
                    }
                    definition.milling_depth = std::clamp(
                        definition.milling_depth,
                        0.1, std::max(
                            0.1, definition.panel_thickness - 0.5));
                    definition.facade_bulge = definition.depth * 0.5;
                    if (geometry.hasAttribute("facadeBulge")
                        && !read_double_attr(geometry, "facadeBulge", definition.facade_bulge, error)) {
                        return false;
                    }
                    if (geometry.hasAttribute("radius2Bulge")
                        && !read_double_attr(geometry, "radius2Bulge", definition.radius2_bulge, error)) {
                        return false;
                    }
                    if (geometry.hasAttribute("radiusSideStraight")
                        && !read_double_attr(
                            geometry, "radiusSideStraight", definition.radius_side_straight, error)) {
                        return false;
                    }
                    if (geometry.hasAttribute("doorOpenAngle")
                        && !read_double_attr(geometry, "doorOpenAngle", definition.door_open_angle, error)) {
                        return false;
                    }
                    definition.left_door_open_angle = definition.door_open_angle;
                    definition.right_door_open_angle = definition.door_open_angle;
                    if (geometry.hasAttribute("leftDoorOpenAngle")
                        && !read_double_attr(
                            geometry, "leftDoorOpenAngle",
                            definition.left_door_open_angle, error)) {
                        return false;
                    }
                    if (geometry.hasAttribute("rightDoorOpenAngle")
                        && !read_double_attr(
                            geometry, "rightDoorOpenAngle",
                            definition.right_door_open_angle, error)) {
                        return false;
                    }
                    bool hinge_ok = true;
                    definition.door_hinge_side = geometry.attribute("doorHingeSide", "0").toInt(&hinge_ok);
                    if (!hinge_ok) {
                        error = "Kitchen cabinet door hinge side is invalid.";
                        return false;
                    }
                    bool handle_orientation_ok = true;
                    definition.handle_orientation = geometry.attribute(
                        "handleOrientation", "0").toInt(&handle_orientation_ok);
                    if (!handle_orientation_ok) {
                        error = "Kitchen cabinet handle orientation is invalid.";
                        return false;
                    }
                    if (!CKitchenCabinet::IsValid(definition)) {
                        error = "Kitchen cabinet parameters are invalid.";
                        return false;
                    }
                    assembly = std::make_unique<CKitchenCabinet>(
                        object_element.attribute("name", "Kitchen Cabinet").toStdString(),
                        std::move(element_ids), definition);
                } else {
                    assembly = std::make_unique<CAssembled>(
                        object_element.attribute("name", "Assembly").toStdString(),
                        std::move(element_ids));
                }
                bool draw_ok = false;
                const uint draw_param = geometry.attribute("drawParam", "0").toUInt(&draw_ok);
                bool id_ok = false;
                const unsigned long id_dim = geometry.attribute("idDim", "0").toULong(&id_ok);
                if (!draw_ok || !id_ok || draw_param > 255U) {
                    error = "Assembly parameters are invalid.";
                    return false;
                }
                assembly->SetDrawParam(static_cast<std::uint8_t>(draw_param));
                assembly->SetIdDim(id_dim);
                CAssembled::TransformMatrix assembly_transform{
                    1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0};
                for (int row = 0; row < 4; ++row) {
                    for (int column = 0; column < 4; ++column) {
                        const QString attribute =
                            QString("transform%1%2").arg(row).arg(column);
                        if (!geometry.hasAttribute(attribute)) {
                            continue;
                        }
                        bool transform_ok = false;
                        const double value =
                            geometry.attribute(attribute).toDouble(&transform_ok);
                        if (!transform_ok || !std::isfinite(value)) {
                            error = "Assembly transform matrix is invalid.";
                            return false;
                        }
                        assembly_transform[static_cast<size_t>(row * 4 + column)] = value;
                    }
                }
                assembly->SetAssemblyTransform(assembly_transform);
                for (QDomElement dim = geometry.firstChildElement("dimension");
                     !dim.isNull(); dim = dim.nextSiblingElement("dimension")) {
                    double sx, sy, sz, ex, ey, ez, ox, oy, oz, offset, value;
                    if (!read_double_attr(dim, "sx", sx, error)
                        || !read_double_attr(dim, "sy", sy, error)
                        || !read_double_attr(dim, "sz", sz, error)
                        || !read_double_attr(dim, "ex", ex, error)
                        || !read_double_attr(dim, "ey", ey, error)
                        || !read_double_attr(dim, "ez", ez, error)
                        || !read_double_attr(dim, "ox", ox, error)
                        || !read_double_attr(dim, "oy", oy, error)
                        || !read_double_attr(dim, "oz", oz, error)
                        || !read_double_attr(dim, "offset", offset, error)
                        || !read_double_attr(dim, "value", value, error)) {
                        return false;
                    }
                    CDimens3D dimension(
                        {sx, sy, sz}, {ex, ey, ez}, {ox, oy, oz}, offset,
                        dim.attribute("parameter").toStdString(),
                        dim.attribute("label").toStdString(), value);
                    dimension.SetVisible(dim.attribute("visible", "true") == "true");
                    assembly->AddDimension(dimension);
                }
                object = std::move(assembly);
            } else if (type == "Part") {
                auto part = std::make_unique<CPart>(
                    object_element.attribute("name", "Part").toStdString(),
                    std::move(element_ids));
                part->SetFileLinked(geometry.attribute("fileLinked", "false") == "true");
                part->SetSourcePath(geometry.attribute("sourcePath").toStdString());
                object = std::move(part);
            } else {
                object = std::make_unique<CGroup>(
                    object_element.attribute("name", "Group").toStdString(),
                    std::move(element_ids));
            }
        } else if (type == "DrawingText") {
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) return false;
            double x = 0.0, y = 0.0, z = 0.0, height = 1.0, rotation = 0.0;
            if (!read_double_attr(geometry, "x", x, error)
                || !read_double_attr(geometry, "y", y, error)
                || !read_double_attr(geometry, "z", z, error)
                || !read_double_attr(geometry, "height", height, error)
                || !read_double_attr(geometry, "rotation", rotation, error)) {
                return false;
            }
            object = std::make_unique<CDrawingText>(
                geometry.attribute("text").toStdString(),
                CPoint3d(x, y, z), height, rotation,
                geometry.attribute("font", "Arial").toStdString());
        } else if (type == "Curve") {
            auto polyline = std::make_unique<CPolyline>(object_element.attribute("name", "Curve").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            const bool closed = geometry.attribute("closed", "false") == "true";
            std::vector<double> vertex_radii;

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
                double radius = 0.0;
                if (point_element.hasAttribute("radius")) {
                    bool radius_ok = false;
                    radius = point_element.attribute("radius").toDouble(&radius_ok);
                    if (!radius_ok || radius < 0.0) {
                        error = "Invalid polyline vertex radius.";
                        return false;
                    }
                }
                vertex_radii.push_back(radius);
            }
            polyline->SetClosed(closed);
            for (size_t point_index = 0; point_index < vertex_radii.size(); ++point_index) {
                if (vertex_radii[point_index] > 0.0
                    && !polyline->SetVertexRadius(point_index, vertex_radii[point_index])) {
                    error = "Polyline vertex radius does not fit adjacent segments.";
                    return false;
                }
            }
            object = std::move(polyline);
        } else if (type == "Sketch") {
            auto sketch = std::make_unique<CSmartLine>(
                object_element.attribute("name", "Sketch").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }

            CPoint3d origin;
            CPoint3d x_axis;
            CPoint3d normal;
            const QDomElement origin_element = required_child(geometry, "origin", error);
            const QDomElement x_axis_element = required_child(geometry, "xAxis", error);
            const QDomElement normal_element = required_child(geometry, "normal", error);
            if (origin_element.isNull() || x_axis_element.isNull() || normal_element.isNull() ||
                !read_double_attr(origin_element, "x", origin.x, error) ||
                !read_double_attr(origin_element, "y", origin.y, error) ||
                !read_double_attr(origin_element, "z", origin.z, error) ||
                !read_double_attr(x_axis_element, "x", x_axis.x, error) ||
                !read_double_attr(x_axis_element, "y", x_axis.y, error) ||
                !read_double_attr(x_axis_element, "z", x_axis.z, error) ||
                !read_double_attr(normal_element, "x", normal.x, error) ||
                !read_double_attr(normal_element, "y", normal.y, error) ||
                !read_double_attr(normal_element, "z", normal.z, error) ||
                !sketch->SetCoordinateSystem(origin, x_axis, normal)) {
                if (error.isEmpty()) {
                    error = "Sketch coordinate system is invalid.";
                }
                return false;
            }

            const QDomElement attachment_element =
                geometry.firstChildElement("faceAttachment");
            if (!attachment_element.isNull()) {
                bool body_ok = false;
                bool face_ok = false;
                const qulonglong body_id =
                    attachment_element.attribute("bodyId").toULongLong(&body_ok);
                const int face_index =
                    attachment_element.attribute("faceIndex").toInt(&face_ok);
                if (!body_ok || body_id == 0 || !face_ok || face_index < 0) {
                    error = "Sketch face attachment is invalid.";
                    return false;
                }
                sketch->SetFaceAttachment(
                    static_cast<unsigned long>(body_id), face_index);
            }

            const QDomElement lines_element = required_child(geometry, "lines", error);
            if (lines_element.isNull()) {
                return false;
            }
            for (QDomElement line_element = lines_element.firstChildElement("line");
                 !line_element.isNull();
                 line_element = line_element.nextSiblingElement("line")) {
                CPoint3d start;
                CPoint3d end;
                if (!read_double_attr(line_element, "x1", start.x, error) ||
                    !read_double_attr(line_element, "y1", start.y, error) ||
                    !read_double_attr(line_element, "x2", end.x, error) ||
                    !read_double_attr(line_element, "y2", end.y, error)) {
                    return false;
                }
                const QString line_type = line_element.attribute("type", "segment");
                std::unique_ptr<CLinkLine> line;
                if (line_type == "horizontal") {
                    line = std::make_unique<CLinkLineHor>(start, end);
                } else if (line_type == "vertical") {
                    line = std::make_unique<CLinkLineVert>(start, end);
                } else if (line_type == "bezier") {
                    CPoint3d control1;
                    CPoint3d control2;
                    if (!read_double_attr(line_element, "cx1", control1.x, error)
                        || !read_double_attr(line_element, "cy1", control1.y, error)
                        || !read_double_attr(line_element, "cx2", control2.x, error)
                        || !read_double_attr(line_element, "cy2", control2.y, error)) {
                        return false;
                    }
                    line = std::make_unique<CBezierSpline>(
                        start, control1, control2, end);
                } else if (line_type == "arc") {
                    CPoint3d point_on_arc;
                    if (!read_double_attr(line_element, "mx", point_on_arc.x, error)
                        || !read_double_attr(line_element, "my", point_on_arc.y, error)) {
                        return false;
                    }
                    auto arc = std::make_unique<CSketchArcLine>(start, point_on_arc, end);
                    if (!arc->IsValid()) {
                        error = "Sketch contains an invalid arc.";
                        return false;
                    }
                    line = std::move(arc);
                } else {
                    line = std::make_unique<CLinkLine>(start, end);
                }
                if (!sketch->AddLine(std::move(line), false)) {
                    error = "Sketch contains an invalid line.";
                    return false;
                }
            }
            if (!sketch->SetClosed(geometry.attribute("closed", "false") == "true") &&
                geometry.attribute("closed", "false") == "true") {
                error = "Closed sketch has too few lines.";
                return false;
            }

            const QDomElement constraints_element = geometry.firstChildElement("constraints");
            for (QDomElement constraint_element = constraints_element.firstChildElement("constraint");
                 !constraint_element.isNull();
                 constraint_element = constraint_element.nextSiblingElement("constraint")) {
                bool index_ok = false;
                const std::size_t line_index =
                    constraint_element.attribute("line").toULongLong(&index_ok);
                if (!index_ok) {
                    error = "Sketch constraint has an invalid line index.";
                    return false;
                }
                const QString constraint_type = constraint_element.attribute("type");
                const bool added = constraint_type == "horizontal"
                    ? sketch->ConstrainHorizontal(line_index)
                    : constraint_type == "vertical"
                        ? sketch->ConstrainVertical(line_index)
                        : constraint_type == "tangent-start"
                            ? sketch->ConstrainBezierTangentAtStart(line_index)
                            : constraint_type == "tangent-end"
                                ? sketch->ConstrainBezierTangentAtEnd(line_index)
                                : false;
                if (!added) {
                    error = "Sketch constraint is invalid.";
                    return false;
                }
            }

            const QDomElement fillets_element = geometry.firstChildElement("fillets");
            for (QDomElement fillet_element = fillets_element.firstChildElement("fillet");
                 !fillet_element.isNull();
                 fillet_element = fillet_element.nextSiblingElement("fillet")) {
                bool index_ok = false;
                const std::size_t first_line =
                    fillet_element.attribute("firstLine").toULongLong(&index_ok);
                double radius = 0.0;
                if (!index_ok || !read_double_attr(fillet_element, "radius", radius, error) ||
                    !sketch->AddFillet(first_line, radius)) {
                    if (error.isEmpty()) {
                        error = "Sketch fillet is invalid.";
                    }
                    return false;
                }
            }
            object = std::move(sketch);
        } else if (type == "BSpline") {
            auto spline = std::make_unique<CBSpline>(object_element.attribute("name", "B-Spline").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            const bool closed = geometry.attribute("closed", "false") == "true";
            const QString curve_type = geometry.attribute("curveType", "b-spline");
            spline->SetCurveType(
                curve_type == "bezier" ? SplineCurveType::Bezier
                : curve_type == "nurbs" ? SplineCurveType::Nurbs
                : SplineCurveType::BSpline);
            bool degree_ok = false;
            const int degree = geometry.attribute("degree", "3").toInt(&degree_ok);
            if (!degree_ok || degree < 1) {
                error = "Invalid spline degree.";
                return false;
            }
            spline->SetDegree(degree);
            std::vector<double> weights;

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
                bool weight_ok = false;
                const double weight = point_element.attribute(
                    "weight", "1").toDouble(&weight_ok);
                if (!weight_ok || weight <= 0.0) {
                    error = "Invalid NURBS control point weight.";
                    return false;
                }
                weights.push_back(weight);
            }
            spline->SetWeights(std::move(weights));
            std::vector<double> knots;
            for (QDomElement knot_element = geometry.firstChildElement("knot");
                 !knot_element.isNull();
                 knot_element = knot_element.nextSiblingElement("knot")) {
                bool knot_ok = false;
                const double knot = knot_element.attribute("value").toDouble(&knot_ok);
                if (!knot_ok) {
                    error = "Invalid NURBS knot value.";
                    return false;
                }
                knots.push_back(knot);
            }
            if (!knots.empty() && !spline->SetKnots(std::move(knots))) {
                error = "Invalid NURBS knot vector.";
                return false;
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
        } else if (type == "Mesh" || type == "ReferenceImage") {
            std::unique_ptr<CMesh3D> mesh = type == "ReferenceImage"
                ? std::unique_ptr<CMesh3D>(new CReferenceImage(
                    object_element.attribute("name", "Reference Image").toStdString()))
                : std::make_unique<CMesh3D>(
                    object_element.attribute("name", "Mesh3D").toStdString());
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
                    const size_t vertex_index = static_cast<size_t>(index);
                    face.corners.push_back({vertex_index, vertex_index, vertex_index});
                }
                if (face.corners.size() < 3) {
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
        } else if (type == "Solid" || type == "SurfaceSet" || type == "AssociativeClone") {
            std::unique_ptr<CSolid> solid;
            const QDomElement geometry = required_child(
                object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            if (geometry.attribute("kind") == "brep-ref") {
                bool index_ok = false;
                const qsizetype packed_index = geometry.attribute("index")
                    .toLongLong(&index_ok);
                if (!index_ok || packed_index < 0
                    || packed_index >= static_cast<qsizetype>(packed_shapes.size())) {
                    error = "Solid contains an invalid project BRep reference.";
                    return false;
                }
                TopoDS_Shape shape = packed_shapes[static_cast<size_t>(packed_index)];
                solid = std::make_unique<CSolid>(shape);
                if (!solid->InitSurfaces()) {
                    error = "Could not initialize solid surfaces from project BRep pack.";
                    return false;
                }
            } else {
                // Version-1 projects saved before the shared geometry store
                // keep an individual native-BRep or STEP block per object.
                solid = CSolid::Load(object_element, error);
            }
            if (!solid) {
                return false;
            }
            if (!read_boolean_tools(object_element, *solid, error)) {
                return false;
            }
            if (type == "AssociativeClone") {
                bool source_ok = false;
                const unsigned long source_id = object_element.attribute("sourceId").toULong(&source_ok);
                if (!source_ok || source_id == 0) {
                    error = "Associative clone source ID is invalid.";
                    return false;
                }
                double placement_values[3][4]{};
                for (int row = 1; row <= 3; ++row) {
                    for (int column = 1; column <= 4; ++column) {
                        double value = 0.0;
                        const QByteArray attribute = QString("p%1%2").arg(row).arg(column).toLatin1();
                        if (!read_double_attr(object_element, attribute.constData(), value, error)) {
                            return false;
                        }
                        placement_values[row - 1][column - 1] = value;
                    }
                }
                const gp_Mat matrix(
                    placement_values[0][0], placement_values[0][1], placement_values[0][2],
                    placement_values[1][0], placement_values[1][1], placement_values[1][2],
                    placement_values[2][0], placement_values[2][1], placement_values[2][2]);
                const gp_GTrsf placement(matrix, gp_XYZ(
                    placement_values[0][3], placement_values[1][3], placement_values[2][3]));
                auto clone = std::make_unique<CAssociativeClone>(solid->m_Shape, source_id);
                clone->SetPlacement(placement);
                clone->SetName(object_element.attribute("name", "Linked Copy").toStdString());
                clone->InitSurfaces();
                object = std::move(clone);
            } else if (type == "SurfaceSet") {
                TopoDS_Shape shape = solid->m_Shape;
                auto surface_set = std::make_unique<CSurfaceSet>(shape);
                surface_set->SetName(object_element.attribute("name", "Surface Set").toStdString());
                surface_set->InitSurfaces();
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
            if (!read_surface_texture_transforms(object_element, *solid, loaded_materials, error)) {
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
        const bool reference_image =
            dynamic_cast<CReferenceImage*>(object.get()) != nullptr;
        if (object_element.hasAttribute("materialId") && !reference_image) {
            bool ok = false;
            const unsigned long material_id = object_element.attribute("materialId").toULong(&ok);
            if (ok) {
                object->SetMaterialId(material_id);
                if (const Material* material = find_loaded_material(loaded_materials, material_id)) {
                    object->SetMaterial(*material);
                }
            }
        }
        if (object_element.hasAttribute("objectColorR")
            || object_element.hasAttribute("objectColorG")
            || object_element.hasAttribute("objectColorB")) {
            Color object_color = object->GetColor();
            if (!read_float_attr(object_element, "objectColorR", object_color.r, error, false)
                || !read_float_attr(object_element, "objectColorG", object_color.g, error, false)
                || !read_float_attr(object_element, "objectColorB", object_color.b, error, false)) {
                return false;
            }
            object->SetColor(object_color);
        }
        if (object_element.hasAttribute("layerId")) {
            bool ok = false;
            const int layer_id = object_element.attribute("layerId").toInt(&ok);
            if (ok) {
                object->m_LayerID = layer_id;
            }
        } else if (!loaded_layers.empty() && loaded_layers.front()) {
            object->m_LayerID = loaded_layers.front()->ID();
        }
        if (reference_image) {
            // The nested object material contains the per-image alpha.  Old
            // projects may still carry a shared materialId; deliberately do
            // not let it replace that object-specific value.
            Material material = object->GetMaterial();
            material.id = 0;
            object->SetMaterial(material);
            object->SetMaterialId(0);
        } else {
            upsert_loaded_material(loaded_materials, object->GetMaterial());
        }
        read_parametric_definition(object_element, *object);
        object->SetGroupName(object_element.attribute("group").toStdString());
        if (object_element.hasAttribute("lineWidth")) {
            bool ok = false;
            const double width = object_element.attribute("lineWidth").toDouble(&ok);
            if (ok) object->SetLineWidth(width);
        }
        object->SetLineStyle(object_element.attribute("lineStyle", "CONTINUOUS").toStdString());
        object->SetVisible(object_element.attribute("visible", "true") != "false");
        loaded_objects.push_back(std::move(object));
        ++object_index;
    }

    report(95, "Finalizing project...");
    document.Clear();
    for (CLayer* layer : document.m_Layers) {
        delete layer;
    }
    document.m_Layers = std::move(loaded_layers);
    document.Work_layer = loaded_work_layer;
    document.EnsureDefaultLayer();
    document.GetMaterials() = std::move(loaded_materials);
    document.GetObjects() = std::move(loaded_objects);
    document.SetDraftingData(std::move(loaded_drafting_data));
    document.EnsureObjectIds();
    if (document.GetObjects().empty()) {
        document.CreatePolyline();
    }
    document.ClearSelection();
    report(100, "Project opened");
    return true;
}

namespace {
bool parameter_is_object_reference(const std::string& id) {
    return id == "id"
        || (id.size() > 3 && id.compare(id.size() - 3, 3, ".id") == 0);
}

void remap_parameters(std::vector<ParametricParameterValue>& parameters,
                      const std::map<unsigned long, unsigned long>& id_map) {
    for (ParametricParameterValue& parameter : parameters) {
        if (!parameter_is_object_reference(parameter.id)
            || !std::isfinite(parameter.value)
            || parameter.value < 0.0) {
            continue;
        }
        const auto found = id_map.find(static_cast<unsigned long>(parameter.value));
        if (found != id_map.end()) {
            parameter.value = static_cast<double>(found->second);
        }
    }
}

void copy_object_identity(const CAlfaObject& source, CAlfaObject& target) {
    target.m_col = source.m_col;
    target.m_selected = false;
    target.m_id = source.m_id;
    target.m_LayerID = source.m_LayerID;
    target.SetName(source.GetName());
    target.SetGroupName(source.GetGroupName());
    target.SetLineWidth(source.GetLineWidth());
    target.SetLineStyle(source.GetLineStyle());
    target.CAlfaObject::SetVisible(source.IsVisible());
    target.CAlfaObject::SetColor(source.GetColor());
    target.SetMaterial(source.GetMaterial());
    target.SetMaterialId(source.GetMaterialId());
    target.SetParametricDefinition(
        source.GetParametricToolId(), source.GetParametricParameters());
}
}

bool Dom3DProjectSerializer::ImportPart(const QString& path,
                                        CAlfaDoc& document,
                                        const QString& requested_name,
                                        Vec3 insertion_point,
                                        Vec3 scale,
                                        bool file_linked,
                                        bool wrap_as_part,
                                        QString& error) const {
    CAlfaDoc imported;
    QString room;
    ProjectViewState view;
    if (!Load(path, imported, room, view, error)) {
        SetAlfaDoc(&document);
        return false;
    }
    SetAlfaDoc(&document);

    auto& source_objects = imported.GetObjects();
    source_objects.erase(
        std::remove_if(source_objects.begin(), source_objects.end(),
            [](const CAlfaDoc::ObjectPtr& object) {
                const auto* curve = dynamic_cast<const CPolyline*>(object.get());
                return !object || (curve && curve->IsEmpty());
            }),
        source_objects.end());
    if (source_objects.empty()) {
        error = "The imported project does not contain any objects.";
        return false;
    }
    const bool has_parametric_sketch = std::any_of(
        source_objects.begin(), source_objects.end(),
        [](const CAlfaDoc::ObjectPtr& object) {
            return dynamic_cast<const CSmartLine*>(object.get()) != nullptr;
        });
    if (has_parametric_sketch
        && (std::fabs(scale.x - scale.y) > 1.0e-6f
            || std::fabs(scale.x - scale.z) > 1.0e-6f)) {
        error = "Non-uniform X/Y/Z scale cannot preserve circular arcs in a parametric sketch. Use equal scale values.";
        return false;
    }

    QString part_name = requested_name.trimmed();
    if (part_name.isEmpty()) {
        part_name = QFileInfo(path).completeBaseName();
    }
    const QString base_name = part_name;
    int suffix = 2;
    const auto part_name_exists = [&document](const QString& candidate) {
        for (const auto& object : document.GetObjects()) {
            const auto* part = dynamic_cast<const CPart*>(object.get());
            if (part && QString::fromStdString(part->GetName()).compare(
                    candidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };
    while (part_name_exists(part_name)) {
        part_name = QString("%1 %2").arg(base_name).arg(suffix++);
    }

    std::set<unsigned long> nested_ids;
    for (const auto& object : source_objects) {
        if (const auto* group = dynamic_cast<const CGroup*>(object.get())) {
            nested_ids.insert(group->GetElementIds().begin(), group->GetElementIds().end());
        }
    }
    std::vector<unsigned long> top_level_old_ids;
    for (const auto& object : source_objects) {
        if (object && nested_ids.count(object->m_id) == 0) {
            top_level_old_ids.push_back(object->m_id);
        }
    }

    std::map<unsigned long, unsigned long> material_map;
    for (const Material& source_material : imported.GetMaterials()) {
        if (source_material.id == 0) {
            continue;
        }
        if (const Material* existing = document.FindMaterial(source_material.name, true)) {
            material_map[source_material.id] = existing->id;
        } else {
            Material copy = source_material;
            const unsigned long old_id = copy.id;
            copy.id = 0;
            material_map[old_id] = document.UpsertMaterial(std::move(copy)).id;
        }
    }

    std::map<unsigned long, unsigned long> id_map;
    for (auto& object : source_objects) {
        const unsigned long old_id = object->m_id;
        object->m_id = 0;
        document.EnsureObjectId(*object);
        id_map[old_id] = object->m_id;
    }

    for (auto& object : source_objects) {
        if (auto* group = dynamic_cast<CGroup*>(object.get())) {
            std::vector<unsigned long> remapped;
            remapped.reserve(group->GetElementIds().size());
            for (unsigned long id : group->GetElementIds()) {
                const auto found = id_map.find(id);
                if (found != id_map.end()) {
                    remapped.push_back(found->second);
                }
            }
            group->SetElementIds(std::move(remapped));
        }
        if (auto* clone = dynamic_cast<CAssociativeClone*>(object.get())) {
            const auto found = id_map.find(clone->GetSourceId());
            if (found != id_map.end()) {
                clone->SetSourceId(found->second);
            }
        }
        std::vector<ParametricParameterValue> definition = object->GetParametricParameters();
        remap_parameters(definition, id_map);
        object->SetParametricDefinition(object->GetParametricToolId(), std::move(definition));
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            for (int operation_index = 0;
                 operation_index < solid->GetNumOperations(); ++operation_index) {
                if (ParametricFunction* operation = solid->GetOperation(operation_index)) {
                    remap_parameters(operation->Parameters, id_map);
                }
            }
        }
        const auto material = material_map.find(object->GetMaterialId());
        if (material != material_map.end()) {
            object->SetMaterialId(material->second);
            if (const Material* saved = document.FindMaterial(material->second)) {
                object->SetMaterial(*saved);
            }
        }
    }

    std::vector<unsigned long> top_level_ids;
    top_level_ids.reserve(top_level_old_ids.size());
    for (unsigned long old_id : top_level_old_ids) {
        const auto found = id_map.find(old_id);
        if (found != id_map.end()) {
            top_level_ids.push_back(found->second);
        }
    }

    auto& target_objects = document.GetObjects();
    for (auto& object : source_objects) {
        target_objects.push_back(std::move(object));
    }
    source_objects.clear();

    CLayer* part_layer = document.AddLayer(
        QString(wrap_as_part ? "Part:%1" : "Catalog Sketch:%1")
            .arg(part_name).toStdString());
    auto part = std::make_unique<CPart>(part_name.toStdString(), std::move(top_level_ids));
    part->SetFileLinked(file_linked);
    part->SetSourcePath(QFileInfo(path).absoluteFilePath().toStdString());
    CPart* part_pointer = part.get();
    if (wrap_as_part) {
        document.AddObject(std::move(part));
    }
    if (part_layer) {
        part_pointer->SetLayer(static_cast<unsigned long>(part_layer->ID()));
    }

    const Vec3 origin{};
    if (scale.x > 1.0e-6f && std::fabs(scale.x - 1.0f) > 1.0e-6f) {
        part_pointer->Scale(origin, {1.0f, 0.0f, 0.0f}, scale.x);
    }
    if (scale.y > 1.0e-6f && std::fabs(scale.y - 1.0f) > 1.0e-6f) {
        part_pointer->Scale(origin, {0.0f, 1.0f, 0.0f}, scale.y);
    }
    if (scale.z > 1.0e-6f && std::fabs(scale.z - 1.0f) > 1.0e-6f) {
        part_pointer->Scale(origin, {0.0f, 0.0f, 1.0f}, scale.z);
    }
    part_pointer->Translate(insertion_point);
    if (!wrap_as_part) {
        document.ClearSelection();
        const auto& ids = part_pointer->GetElementIds();
        if (!ids.empty()) {
            document.SelectObjectById(ids.front(), SelectionAction::Replace);
        }
    }
    return true;
}

bool Dom3DProjectSerializer::SaveSelection(
        const QString& path,
        CAlfaDoc& document,
        const std::vector<size_t>& selected_indices,
        const QString& active_room,
        const ProjectViewState& view_state,
        const QImage& thumbnail,
        QString& error) const {
    if (selected_indices.empty()) {
        error = "Select an object to add to the catalog.";
        return false;
    }

    const auto& objects = document.GetObjects();
    std::map<unsigned long, size_t> indices_by_id;
    for (size_t index = 0; index < objects.size(); ++index) {
        if (objects[index]) {
            indices_by_id[objects[index]->m_id] = index;
        }
    }
    std::set<size_t> included;
    std::vector<size_t> pending;
    pending.reserve(selected_indices.size());
    for (size_t index : selected_indices) {
        pending.push_back(document.ResolveGroupSelectionIndex(index));
    }
    while (!pending.empty()) {
        const size_t index = pending.back();
        pending.pop_back();
        if (index >= objects.size() || !objects[index]
            || !included.insert(index).second) {
            continue;
        }
        if (const auto* group = dynamic_cast<const CGroup*>(objects[index].get())) {
            for (unsigned long id : group->GetElementIds()) {
                const auto child = indices_by_id.find(id);
                if (child != indices_by_id.end()) {
                    pending.push_back(child->second);
                }
            }
        }
        auto add_parameter_dependencies = [&](const std::vector<ParametricParameterValue>& parameters) {
            for (const auto& parameter : parameters) {
                if (!parameter_is_object_reference(parameter.id)
                    || parameter.value < 0.0) {
                    continue;
                }
                const auto dependency = indices_by_id.find(
                    static_cast<unsigned long>(parameter.value));
                if (dependency != indices_by_id.end()) {
                    pending.push_back(dependency->second);
                }
            }
        };
        add_parameter_dependencies(objects[index]->GetParametricParameters());
        if (const auto* solid = dynamic_cast<const CSolid*>(objects[index].get())) {
            for (const ParametricFunction* operation : solid->GetOperationTree()) {
                if (operation) {
                    add_parameter_dependencies(operation->Parameters);
                }
            }
        }
    }

    CAlfaDoc subset;
    subset.GetObjects().clear();
    subset.GetMaterials() = document.GetMaterials();
    for (size_t index : included) {
        CAlfaDoc::ObjectPtr copy = objects[index]->Clone();
        if (!copy) {
            SetAlfaDoc(&document);
            error = "Could not copy the selected catalog object.";
            return false;
        }
        copy_object_identity(*objects[index], *copy);
        subset.GetObjects().push_back(std::move(copy));
    }
    const bool saved = Save(
        path, subset, active_room, view_state, thumbnail, error);
    SetAlfaDoc(&document);
    return saved;
}
