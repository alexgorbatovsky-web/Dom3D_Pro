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
    float texture_offset_u = 0.0f;
    float texture_offset_v = 0.0f;
    float texture_scale_u = 1.0f;
    float texture_scale_v = 1.0f;
    float texture_rotation_degrees = 0.0f;
    unsigned long id = 0;
    std::string name = "Material";
    std::string color_texture_path;
    std::string light_texture_path;
    std::string bump_texture_path;
    std::string source_file_path;

    static Material DefaultSurface();
    static Material DefaultMesh();
    static Material ImportedMesh();
    static Material DefaultWhite();
    static Material DefaultGloss();
    static Material DefaultTexture();
    static std::vector<Material> InitialDocumentMaterials();
};
