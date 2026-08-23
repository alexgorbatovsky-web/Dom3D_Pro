#include "RenderScene.h"

#include "../CAlfaDoc.h"
#include "../CMesh3D.h"
#include "../ReferenceImage.h"
#include "../solid/Solid.h"
#include "../solid/SurfaceFace.h"
#include "../solid/SurfaceSet.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace {
QJsonArray vector_json(Vec3 value) {
    return {value.x, value.y, value.z};
}

QJsonArray color_json(Color value) {
    return {value.r, value.g, value.b};
}

QString resolved_texture_path(const Material& material,
                              const std::string& source) {
    QString path = QString::fromStdString(source).trimmed();
    if (path.isEmpty()) return {};
    QFileInfo info(path);
    if (info.isAbsolute() && info.exists()) return info.absoluteFilePath();
    if (!material.source_file_path.empty()) {
        const QDir source_dir(
            QFileInfo(QString::fromStdString(material.source_file_path))
                .absolutePath());
        const QString sibling = source_dir.filePath(path);
        if (QFileInfo::exists(sibling)) return QFileInfo(sibling).absoluteFilePath();
    }
    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString material_path = app_dir.filePath("materials/" + path);
    if (QFileInfo::exists(material_path)) return QFileInfo(material_path).absoluteFilePath();
    const QString app_path = app_dir.filePath(path);
    if (QFileInfo::exists(app_path)) return QFileInfo(app_path).absoluteFilePath();
    return info.exists() ? info.absoluteFilePath() : QString{};
}

UV transform_uv(UV uv, const Material& material) {
    const float radians = deg_to_rad(material.texture_rotation_degrees);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float u = (uv.u - 0.5f) * material.texture_scale_u;
    const float v = (uv.v - 0.5f) * material.texture_scale_v;
    return {u * c - v * s + 0.5f + material.texture_offset_u,
            u * s + v * c + 0.5f + material.texture_offset_v};
}

int append_material(RenderScene& scene, Material material) {
    scene.materials.push_back(std::move(material));
    return static_cast<int>(scene.materials.size() - 1);
}

void append_mesh(RenderScene& scene,
                 const CMesh3D& source,
                 Material material,
                 const QString& name,
                 bool independent_surface_patch = false) {
    if (source.GetVertices().empty()) return;
    RenderMesh mesh;
    mesh.name = name;
    mesh.independent_surface_patch = independent_surface_patch;
    mesh.material_index = append_material(scene, material);
    mesh.vertices = source.GetVertices();

    const bool has_uvs = !source.GetUVs().empty();
    UV uv_min{};
    UV uv_max{};
    if (has_uvs) {
        uv_min = uv_max = source.GetUVs().front();
        for (const UV& uv : source.GetUVs()) {
            uv_min.u = std::min(uv_min.u, uv.u);
            uv_min.v = std::min(uv_min.v, uv.v);
            uv_max.u = std::max(uv_max.u, uv.u);
            uv_max.v = std::max(uv_max.v, uv.v);
        }
    }
    // Generated CAD meshes use millimetres for planar UV coordinates, but
    // analytic curved surfaces can mix units: a cylinder, for example, has
    // an angular U coordinate and a millimetre V coordinate.  Scaling both
    // axes when only one is physical collapses the angular coordinate and
    // stretches the texture into long horizontal streaks. Convert each
    // physical axis independently; normalized/radian axes stay unchanged.
    const bool millimeter_u = has_uvs && !material.texture_fit_to_surface
        && uv_max.u - uv_min.u > 64.0f;
    const bool millimeter_v = has_uvs && !material.texture_fit_to_surface
        && uv_max.v - uv_min.v > 64.0f;

    for (const CMesh3D::Face& face : source.GetFaces()) {
        if (face.deleted || face.corners.size() < 3) continue;
        const size_t first_index = face.corners[0].v;
        if (first_index >= mesh.vertices.size()) continue;
        Vec3 face_normal{};
        for (size_t corner = 1; corner + 1 < face.corners.size(); ++corner) {
            const size_t second_index = face.corners[corner].v;
            const size_t third_index = face.corners[corner + 1].v;
            if (second_index >= mesh.vertices.size()
                || third_index >= mesh.vertices.size()) continue;
            face_normal = normalize(cross(
                mesh.vertices[second_index] - mesh.vertices[first_index],
                mesh.vertices[third_index] - mesh.vertices[first_index]));
            RenderTriangle triangle;
            const std::array<size_t, 3> corners{0, corner, corner + 1};
            for (size_t i = 0; i < 3; ++i) {
                const MeshCorner& mesh_corner = face.corners[corners[i]];
                triangle.vertices[i] = static_cast<int>(mesh_corner.v);
                UV uv{};
                if (has_uvs && mesh_corner.uv < source.GetUVs().size()) {
                    uv = source.GetUVs()[mesh_corner.uv];
                    if (millimeter_u) uv.u *= 0.001f;
                    if (millimeter_v) uv.v *= 0.001f;
                    if (material.texture_fit_to_surface) {
                        uv.u = (uv.u - uv_min.u)
                            / std::max(uv_max.u - uv_min.u, 0.00001f);
                        uv.v = (uv.v - uv_min.v)
                            / std::max(uv_max.v - uv_min.v, 0.00001f);
                    }
                    uv = transform_uv(uv, material);
                }
                triangle.uvs[i] = uv;
                triangle.normals[i] = mesh_corner.n < source.GetNormals().size()
                    ? normalize(source.GetNormals()[mesh_corner.n])
                    : face_normal;
            }
            mesh.triangles.push_back(triangle);
        }
    }
    if (!mesh.triangles.empty()) scene.meshes.push_back(std::move(mesh));
}

QJsonObject material_json(const Material& material) {
    QJsonObject object;
    object["name"] = QString::fromStdString(material.name);
    object["base_color"] = color_json(material.diffuse);
    object["emission"] = color_json(material.emission);
    object["alpha"] = material.alpha;
    object["specular"] = material.specular;
    object["shininess"] = material.shininess;
    object["reflectivity"] = material.reflectivity;
    object["metallic"] = material.metallic;
    object["roughness"] = material.roughness;
    object["coat_weight"] = material.coat_weight;
    object["coat_roughness"] = material.coat_roughness;
    object["normal_strength"] = material.normal_strength;
    object["bump_strength"] = material.displacement_scale;
    object["color_texture"] = resolved_texture_path(material, material.color_texture_path);
    object["roughness_texture"] = resolved_texture_path(material, material.roughness_texture_path);
    object["metallic_texture"] = resolved_texture_path(material, material.metallic_texture_path);
    object["normal_texture"] = resolved_texture_path(material, material.normal_texture_path);
    object["bump_texture"] = resolved_texture_path(material, material.bump_texture_path);
    object["displacement_texture"] = resolved_texture_path(material, material.displacement_texture_path);
    return object;
}

QJsonObject light_json(const RenderLight& light) {
    QJsonObject object;
    object["enabled"] = light.enabled;
    object["type"] = light.type == RenderLight::Type::Spot
        ? "SPOT" : light.type == RenderLight::Type::Directional
            ? "DIRECTIONAL" : "OMNI";
    object["position"] = vector_json(light.position);
    object["direction"] = vector_json(light.direction);
    object["ambient"] = color_json(light.ambient);
    object["diffuse"] = color_json(light.diffuse);
    object["specular"] = color_json(light.specular);
    object["hotspot"] = light.hotspot_radians;
    object["falloff"] = light.falloff_radians;
    object["spot_exponent"] = light.spot_exponent;
    object["range"] = light.range;
    object["constant_attenuation"] = light.constant_attenuation;
    object["linear_attenuation"] = light.linear_attenuation;
    object["quadratic_attenuation"] = light.quadratic_attenuation;
    object["size"] = light.size;
    object["shadow_density"] = light.shadow_density;
    object["casts_shadows"] = light.casts_shadows;
    object["attenuation_enabled"] = light.attenuation_enabled;
    return object;
}
}

int RenderScene::TriangleCount() const {
    int result = 0;
    for (const RenderMesh& mesh : meshes) {
        result += static_cast<int>(mesh.triangles.size());
    }
    return result;
}

bool RenderScene::SaveJson(const QString& path,
                           const RenderSettings& settings,
                           QString* error) const {
    QJsonObject root;
    root["format"] = "Dom3D RenderScene";
    root["version"] = 1;
    root["unit_scale"] = 0.001;

    QJsonObject camera_json;
    camera_json["position"] = vector_json(camera.position);
    camera_json["forward"] = vector_json(camera.forward);
    camera_json["right"] = vector_json(camera.right);
    camera_json["up"] = vector_json(camera.up);
    camera_json["orthographic"] = camera.orthographic;
    camera_json["vertical_fov_degrees"] = camera.vertical_fov_degrees;
    camera_json["orthographic_scale_mm"] = camera.orthographic_scale_mm;
    root["camera"] = camera_json;

    QJsonObject environment_json;
    environment_json["enabled"] = environment.enabled;
    environment_json["hdri_path"] = environment.hdri_path;
    environment_json["strength"] = environment.strength;
    environment_json["rotation_degrees"] = environment.rotation_degrees;
    environment_json["background_color"] = vector_json(environment.background_color);
    root["environment"] = environment_json;

    QJsonObject settings_json;
    settings_json["width"] = settings.width;
    settings_json["height"] = settings.height;
    settings_json["samples"] = settings.samples;
    settings_json["noise_threshold"] = settings.noise_threshold;
    settings_json["denoise"] = settings.denoise;
    settings_json["transparent_background"] = settings.transparent_background;
    settings_json["device"] = settings.device == RenderSettings::Device::CPU
        ? "CPU" : settings.device == RenderSettings::Device::GPU ? "GPU" : "AUTO";
    settings_json["lighting_preset"] =
        settings.lighting_preset == RenderSettings::LightingPreset::Exterior
        ? "EXTERIOR"
        : settings.lighting_preset == RenderSettings::LightingPreset::Studio
            ? "STUDIO" : "INTERIOR";
    settings_json["output_file"] = settings.output_file;
    settings_json["view_transform"] = settings.view_transform;
    settings_json["look"] = settings.look;
    settings_json["exposure"] = settings.exposure;
    settings_json["environment_strength"] = settings.environment_strength;
    settings_json["interior_light_strength"] = settings.interior_light_strength;
    settings_json["light_mode"] = settings.light_mode
        == RenderSettings::LightMode::Customize ? "CUSTOMIZE" : "AUTO";
    root["settings"] = settings_json;

    QJsonArray lights_json;
    for (const RenderLight& light : settings.custom_lights) {
        lights_json.push_back(light_json(light));
    }
    root["lights"] = lights_json;

    QJsonArray materials_json;
    for (const Material& material : materials) materials_json.push_back(material_json(material));
    root["materials"] = materials_json;

    QJsonArray meshes_json;
    for (const RenderMesh& mesh : meshes) {
        QJsonObject mesh_json;
        mesh_json["name"] = mesh.name;
        mesh_json["material"] = mesh.material_index;
        mesh_json["independent_surface_patch"] =
            mesh.independent_surface_patch;
        QJsonArray vertices_json;
        for (Vec3 vertex : mesh.vertices) vertices_json.push_back(vector_json(vertex));
        mesh_json["vertices"] = vertices_json;
        QJsonArray triangles_json;
        for (const RenderTriangle& triangle : mesh.triangles) {
            QJsonObject triangle_json;
            triangle_json["vertices"] = QJsonArray{
                triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]};
            QJsonArray uvs_json;
            QJsonArray normals_json;
            for (size_t i = 0; i < 3; ++i) {
                uvs_json.push_back(QJsonArray{triangle.uvs[i].u, triangle.uvs[i].v});
                normals_json.push_back(vector_json(triangle.normals[i]));
            }
            triangle_json["uvs"] = uvs_json;
            triangle_json["normals"] = normals_json;
            triangles_json.push_back(triangle_json);
        }
        mesh_json["triangles"] = triangles_json;
        meshes_json.push_back(mesh_json);
    }
    root["meshes"] = meshes_json;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

RenderScene BuildRenderScene(const CAlfaDoc& document,
                             const Camera& camera,
                             bool orthographic,
                             Vec3 background_color) {
    RenderScene scene;
    camera_basis(camera, scene.camera.forward, scene.camera.right, scene.camera.up);
    scene.camera.position = camera_position(camera, orthographic);
    scene.camera.orthographic = orthographic;
    scene.camera.vertical_fov_degrees = camera.vertical_fov_degrees;
    scene.camera.orthographic_scale_mm = std::max(0.1f, camera.distance * 0.84f);
    scene.environment.background_color = background_color;
    const ViewportLightingSettings& lighting = CMesh3D::GetLightingSettings();
    scene.environment.enabled = lighting.environment_enabled;
    scene.environment.hdri_path = QString::fromStdString(lighting.environment_path);
    scene.environment.strength = lighting.environment_strength;
    scene.environment.rotation_degrees = lighting.environment_rotation_degrees;

    for (const auto& object : document.GetObjects()) {
        if (!object || !document.IsObjectVisible(*object)) continue;
        if (const auto* mesh = dynamic_cast<const CMesh3D*>(object.get())) {
            if (!dynamic_cast<const CReferenceImage*>(mesh)) {
                append_mesh(scene, *mesh, mesh->GetMaterial(),
                            QString::fromStdString(mesh->GetName()));
            }
            continue;
        }
        const auto* solid = dynamic_cast<const CSolid*>(object.get());
        if (!solid) continue;
        const bool independent_surface_patch =
            dynamic_cast<const CSurfaceSet*>(solid) != nullptr;
        for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(index);
            if (!surface || !surface->pMesh3D) continue;
            Material material = ComposeSurfaceMaterial(
                solid->GetMaterial(), surface->MaterialOverride);
            material.texture_offset_u += surface->TextureTransform.offset_u;
            material.texture_offset_v += surface->TextureTransform.offset_v;
            material.texture_scale_u *= surface->TextureTransform.scale_u;
            material.texture_scale_v *= surface->TextureTransform.scale_v;
            material.texture_rotation_degrees += surface->TextureTransform.rotation_degrees;
            material.texture_fit_to_surface = material.texture_fit_to_surface
                || surface->TextureTransform.fit_to_surface;
            append_mesh(scene, *surface->pMesh3D, material,
                QString("%1 Surface %2")
                    .arg(QString::fromStdString(solid->GetName()))
                    .arg(index + 1), independent_surface_patch);
        }
    }
    return scene;
}
