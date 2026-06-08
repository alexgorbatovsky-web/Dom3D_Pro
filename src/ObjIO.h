#pragma once

#include "CAlfaDoc.h"
#include "CMesh3D.h"

#include <memory>
#include <string>
#include <vector>

class ObjIO {
public:
    bool Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes, std::string& error) const;
    bool Export(const std::string& path, const CAlfaDoc& document, std::string& error) const;

private:
    bool ExportMesh(std::ostream& stream, const CMesh3D& mesh, size_t& vertex_offset, const std::string& object_name) const;
    bool ParseFaceIndex(const std::string& token, size_t vertex_count, size_t& index) const;
};
