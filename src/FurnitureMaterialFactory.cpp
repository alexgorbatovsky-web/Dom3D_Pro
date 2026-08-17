#include "FurnitureMaterialFactory.h"

#include "CAlfaDoc.h"
#include "CAlfaObject.h"
#include "Material.h"
#include "solid/Solid.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <utility>

namespace {
Material furniture_material(
    std::string name,
    Color ambient,
    Color diffuse,
    Color emission,
    float shininess,
    float specular,
    float alpha = 1.0f,
    float reflectivity = 0.0f,
    std::string texture = {}) {
    Material material;
    material.id = 0;
    material.name = std::move(name);
    material.ambient = ambient;
    material.diffuse = diffuse;
    material.emission = emission;
    material.shininess = shininess;
    material.specular = specular;
    material.alpha = alpha;
    material.reflectivity = reflectivity;
    material.color_texture_path = std::move(texture);
    return material;
}

std::string texture_path(
    const std::string& root,
    const std::string& category,
    const std::string& file_name) {
    return (std::filesystem::path(root) / category / file_name).generic_string();
}

void add_if_missing(CAlfaDoc& document, Material material) {
    if (!document.FindMaterial(material.name, true)) {
        document.UpsertMaterial(std::move(material));
    }
}

void add_or_upgrade_legacy_black_material(
    CAlfaDoc& document,
    Material material) {
    Material* existing = document.FindMaterial(material.name, true);
    if (!existing) {
        document.UpsertMaterial(std::move(material));
        return;
    }

    constexpr float black_threshold = 1.0e-4f;
    const bool legacy_black_diffuse =
        existing->diffuse.r <= black_threshold
        && existing->diffuse.g <= black_threshold
        && existing->diffuse.b <= black_threshold;
    if (!legacy_black_diffuse) {
        return;
    }

    existing->diffuse = material.diffuse;
    const Material upgraded = *existing;
    for (const auto& object : document.GetObjects()) {
        if (object && object->GetMaterialId() == upgraded.id) {
            object->SetMaterial(upgraded);
            object->SetMaterialId(upgraded.id);
        }
    }
}
}

void FurnitureMaterialFactory::EnsureStandardMaterials(
    CAlfaDoc& document,
    const std::string& texture_root) {
    const Color emission{0.0f, 0.0f, 0.0f};
    const Color furniture_ambient{0.1405f, 0.1405f, 0.1405f};
    const Color furniture_diffuse{0.6108f, 0.6108f, 0.6108f};
    constexpr float furniture_shininess = 22.4324f;
    constexpr float furniture_specular = 0.7631f;
    const std::string cherry = texture_path(texture_root, "wood", "Cherry Canella.jpg");

    add_if_missing(document, furniture_material(
        "Facade wood", furniture_ambient, furniture_diffuse, emission,
        furniture_shininess, furniture_specular, 1.0f, 0.0f, cherry));
    add_if_missing(document, furniture_material(
        u8"Profile", furniture_ambient, furniture_diffuse, emission,
        furniture_shininess, furniture_specular, 1.0f, 0.0f, cherry));
    add_if_missing(document, furniture_material(
        "Marble", furniture_ambient, furniture_diffuse, emission,
        furniture_shininess, furniture_specular, 1.0f, 0.0f,
        texture_path(texture_root, "marble", "4077.jpg")));

    add_if_missing(document, furniture_material(
        "Glass", {0.0627f, 0.1361f, 0.1361f}, {0.4000f, 0.4431f, 0.6667f},
        emission, 4.21f, 0.5621f, 0.15f, 0.1f));
    add_if_missing(document, furniture_material(
        u8"Fiberboard", {0.1059f, 0.0824f, 0.0784f}, {0.2824f, 0.2431f, 0.2314f},
        emission, furniture_shininess, 0.4712f));
    add_if_missing(document, furniture_material(
        "Inside", furniture_ambient, furniture_diffuse, emission,
        furniture_shininess, furniture_specular, 1.0f, 0.0f,
        texture_path(texture_root, "wood", "vyaz_klassik.jpg")));
    add_if_missing(document, furniture_material(
        "Steel", {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, emission,
        2.551f, 0.7690f, 1.0f, 0.1f));
    add_or_upgrade_legacy_black_material(document, furniture_material(
        "Wood", {0.2196f, 0.1333f, 0.0471f}, furniture_diffuse, emission,
        6.0f, 0.2105f, 1.0f, 0.0f,
        texture_path(texture_root, "wood", "Oak Red FC.jpg")));
    add_if_missing(document, furniture_material(
        "Brick", furniture_ambient, furniture_diffuse, emission,
        furniture_shininess, furniture_specular, 1.0f, 0.0f,
        texture_path(texture_root, "brick", "BRKWEA_B_4.jpg")));
    add_if_missing(document, furniture_material(
        "Concrete", furniture_ambient, furniture_diffuse, emission,
        furniture_shininess, furniture_specular, 1.0f, 0.0f,
        texture_path(texture_root, "surfaces", "Finishes.Painting.Paint.White.Flaking.jpg")));
    // New standard materials must be appended after the legacy sequence.
    // Tool presets from older builds store numeric material IDs; inserting a
    // material in the middle would make a saved Wood ID point to Steel.
    add_if_missing(document, furniture_material(
        "Gold", {0.2473f, 0.1995f, 0.0745f}, {0.7516f, 0.6065f, 0.2265f},
        emission, 72.0f, 0.92f, 1.0f, 0.72f));
}

bool FurnitureMaterialFactory::SetMaterial(
    CAlfaDoc& document,
    CAlfaObject* object,
    const std::string& material_name) {
    if (!object) {
        return false;
    }
    const Material* material = document.FindMaterial(material_name, true);
    if (!material) {
        return false;
    }
    object->SetMaterial(*material);
    object->SetMaterialId(material->id);
    return true;
}

bool FurnitureMaterialFactory::SetMaterialToFace(
    CAlfaDoc& document,
    CSolid* solid,
    unsigned long surface_id,
    const std::string& material_name) {
    if (!solid) {
        return false;
    }
    const Material* material = document.FindMaterial(material_name, true);
    return material && solid->SetSurfaceMaterialById(surface_id, *material);
}
