#pragma once

#include "Common.h"

struct Material {
    Color diffuse{};
    float alpha = 1.0f;
    float specular = 0.2f;
    float shininess = 24.0f;

    static Material DefaultSurface();
    static Material DefaultMesh();
    static Material ImportedMesh();
};
