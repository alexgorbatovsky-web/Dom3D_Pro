#pragma once

#include "CAlfaDoc.h"
#include "CMesh3D.h"

#include <memory>
#include <string>
#include <vector>

class CSurfaceFace;

enum class ObjLengthUnit {
    Meters,
    Decimeters,
    Centimeters,
    Millimeters,
    Feet,
    Inches
};

class ObjIO {
public:
    bool Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes, std::string& error) const;
    bool Export(const std::string& path,
                const CAlfaDoc& document,
                std::string& error,
                ObjLengthUnit unit = ObjLengthUnit::Meters) const;

private:
    bool ExportMesh(std::ostream& stream,
                    const CMesh3D& mesh,
                    const Material& material,
                    const std::string& material_name,
                    size_t& vertex_offset,
                    size_t& uv_offset,
                    size_t& normal_offset,
                    size_t& next_smoothing_group,
                    const std::string& object_name,
                    double vertex_scale,
                    const CSurfaceFace* surface = nullptr,
                    bool write_object_header = true) const;
    bool ParseFaceIndex(const std::string& token, size_t vertex_count, size_t& index) const;
};
