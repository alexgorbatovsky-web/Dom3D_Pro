#include "Material.h"

Material Material::DefaultSurface() {
    return {{0.68f, 0.78f, 0.86f}, 1.0f, 0.28f, 46.0f};
}

Material Material::DefaultMesh() {
    return {{0.62f, 0.67f, 0.70f}, 1.0f, 0.18f, 38.0f};
}

Material Material::ImportedMesh() {
    return {{0.42f, 0.57f, 0.36f}, 1.0f, 0.16f, 32.0f};
}
