#pragma once

#include <string>

class CAlfaDoc;
class CAlfaObject;
class CSolid;

class FurnitureMaterialFactory {
public:
    static void EnsureStandardMaterials(
        CAlfaDoc& document,
        const std::string& texture_root = "texture");

    static bool SetMaterial(
        CAlfaDoc& document,
        CAlfaObject* object,
        const std::string& material_name);

    static bool SetMaterialToFace(
        CAlfaDoc& document,
        CSolid* solid,
        unsigned long surface_id,
        const std::string& material_name);
};
