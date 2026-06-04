#pragma once

#include "CAlfaDoc.h"
#include "CMesh3D.h"

#include <memory>
#include <string>

class ObjIO {
public:
    bool Import(const std::string& path, std::unique_ptr<CMesh3D>& mesh, std::string& error) const;
    bool Export(const std::string& path, const CAlfaDoc& document, std::string& error) const;

private:
    bool ExportMesh(std::ostream& stream, const CMesh3D& mesh, size_t& vertex_offset) const;
    bool ParseFaceIndex(const std::string& token, size_t vertex_count, size_t& index) const;
};
