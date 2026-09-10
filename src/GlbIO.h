#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>

class CMesh3D;
class CAlfaDoc;

// Static glTF 2.0 binary scenes. Shares Assimp conversion with FbxIO.cpp.
// Converts glTF meters/Y-up to Dom3D millimeters/Z-up.
class GlbIO {
public:
    bool Import(const std::string& path,
                std::vector<std::unique_ptr<CMesh3D>>& meshes,
                std::string& error) const;
    bool Export(const std::string& path, const CAlfaDoc& document,
                std::string& error,
                const std::function<bool(int, const std::string&)>& progress = {}) const;
};
