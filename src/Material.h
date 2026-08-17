#pragma once

#include "Common.h"

#include <string>
#include <vector>

struct Material {
    Color diffuse{};
    Color ambient{0.18f, 0.18f, 0.18f};
    Color emission{0.0f, 0.0f, 0.0f};
    float alpha = 1.0f;
    float specular = 0.2f;
    float shininess = 24.0f;
    float reflectivity = 0.0f;
    float roughness = 0.5f;
    float metallic = 0.0f;
    float coat_weight = 0.0f;
    float coat_roughness = 0.05f;
    float normal_strength = 1.0f;
    float displacement_scale = 0.03f;
    float texture_offset_u = 0.0f;
    float texture_offset_v = 0.0f;
    float texture_scale_u = 1.0f;
    float texture_scale_v = 1.0f;
    float texture_rotation_degrees = 0.0f;
    bool texture_fit_to_surface = false;
    unsigned long id = 0;
    std::string name = "Material";
    std::string color_texture_path;
    std::string light_texture_path;
    std::string bump_texture_path;
    std::string normal_texture_path;
    std::string roughness_texture_path;
    std::string metallic_texture_path;
    std::string displacement_texture_path;
    std::string source_file_path;

    static Material DefaultSurface();
    static Material DefaultMesh();
    static Material ImportedMesh();
    static Material DefaultWhite();
    static Material DefaultGloss();
    static Material DefaultTexture();
    static std::vector<Material> InitialDocumentMaterials();
};
