#pragma once

#include "CMesh3D.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class CPolyline;
class CSurfaceFace;

namespace Tools_Mesh3D {

struct FillContourOptions {
    bool raw_quadrangulator = true;
    // Empty disables temporary dumps. The UI currently uses C:/temp.
    std::filesystem::path diagnostic_directory;
};

struct FillContourResult {
    std::unique_ptr<CMesh3D> mesh;
    std::unique_ptr<CMesh3D> triangle_mesh;
    std::vector<Vec3> boundary;
    size_t vertex_count = 0;
    size_t face_count = 0;
    size_t quad_count = 0;
    size_t triangle_count = 0;
    std::string error;
    std::string quadrangulator_rejection;

    explicit operator bool() const { return mesh && error.empty(); }
};

// No resampling, UV scaling, document mutation or UI dependencies.
FillContourResult FillContour(const CPolyline& polyline,
    const FillContourOptions& options = {});
FillContourResult FillSurface(CSurfaceFace& surface, float density,
    const std::string& mesh_name);

} // namespace Tools_Mesh3D
