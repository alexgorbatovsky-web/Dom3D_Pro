#include "Material.h"

Material Material::DefaultSurface() {
    Material material;
    material.id = 1001;
    material.name = "Default Surface";
    material.diffuse = {0.68f, 0.78f, 0.86f};
    material.ambient = {0.20f, 0.22f, 0.24f};
    material.alpha = 1.0f;
    material.specular = 0.28f;
    material.shininess = 46.0f;
    return material;
}

Material Material::DefaultMesh() {
    Material material;
    material.id = 1002;
    material.name = "Default Mesh";
    material.diffuse = {0.62f, 0.67f, 0.70f};
    material.ambient = {0.18f, 0.19f, 0.20f};
    material.alpha = 1.0f;
    material.specular = 0.18f;
    material.shininess = 38.0f;
    return material;
}

Material Material::ImportedMesh() {
    Material material;
    material.id = 1003;
    material.name = "Imported Mesh";
    material.diffuse = {0.42f, 0.57f, 0.36f};
    material.ambient = {0.15f, 0.20f, 0.13f};
    material.alpha = 1.0f;
    material.specular = 0.16f;
    material.shininess = 32.0f;
    return material;
}

Material Material::DefaultWhite() {
    Material material;
    material.id = 1;
    material.name = "White";
    material.diffuse = {0.92f, 0.92f, 0.88f};
    material.ambient = {0.20f, 0.20f, 0.19f};
    material.alpha = 1.0f;
    material.specular = 0.18f;
    material.shininess = 28.0f;
    return material;
}

Material Material::DefaultGloss() {
    Material material;
    material.id = 2;
    material.name = "Gloss";
    material.diffuse = {0.78f, 0.76f, 0.68f};
    material.ambient = {0.20f, 0.19f, 0.17f};
    material.alpha = 1.0f;
    material.specular = 0.62f;
    material.shininess = 82.0f;
    material.reflectivity = 0.18f;
    return material;
}

Material Material::DefaultTexture() {
    Material material;
    material.id = 3;
    material.name = "Texture";
    material.diffuse = {0.58f, 0.46f, 0.34f};
    material.ambient = {0.16f, 0.12f, 0.09f};
    material.alpha = 1.0f;
    material.specular = 0.24f;
    material.shininess = 36.0f;
    return material;
}

std::vector<Material> Material::InitialDocumentMaterials() {
    return {DefaultWhite(), DefaultGloss(), DefaultTexture()};
}
