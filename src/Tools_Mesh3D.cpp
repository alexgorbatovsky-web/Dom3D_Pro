#include "Tools_Mesh3D.h"
#include "CatmullClarkSurfaceBuilder.h"
#include "LiveFormEdit.h"

#include "CPolyline.h"
#include "ContourQuadrangulator3DCoat.h"
#include "solid/SurfaceFace.h"

#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <cmath>

namespace Tools_Mesh3D {

std::unique_ptr<CSolid> Subdivide(const CMesh3D& mesh, std::string& error)
{
    error.clear();
    std::map<std::pair<size_t, size_t>, std::pair<int, int>> edges;
    for (const auto& face : mesh.GetFaces()) {
        if (face.deleted) continue;
        std::set<size_t> vertices;
        if (face.corners.size() != 4) {
            error = "Subdivide requires quad faces.";
            return {};
        }
        for (size_t i = 0; i < 4; ++i) {
            const size_t a = face.corners[i].v, b = face.corners[(i + 1) % 4].v;
            if (a >= mesh.GetVertices().size() || !vertices.insert(a).second) {
                error = "Subdivide: invalid face vertex.";
                return {};
            }
            const auto p = mesh.GetVertices()[a];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
                error = "Subdivide: invalid vertex coordinates.";
                return {};
            }
            auto& edge = edges[std::minmax(a, b)];
            ++edge.first;
            edge.second += a < b ? 1 : -1;
        }
    }
    for (const auto& edge : edges) {
        if (edge.second.first != 2 || edge.second.second != 0) {
            error = "Subdivide requires a closed manifold quad mesh with consistent face normals.";
            return {};
        }
    }
    auto build = BuildCatmullClarkSurfaceBody(mesh, 1.0e-4, 3);
    if (!build.solid || build.shape.IsNull()) {
        error = "Subdivide requires a closed, connected manifold quad mesh. "
                "Check open boundaries, duplicated vertices and non-quad faces.";
        return {};
    }
    auto solid = std::make_unique<CSolid>(build.shape);
    solid->SetName(mesh.GetName() + " Subdivide");
    solid->SetParametricOperation(0,"LiveFormMesh","LiveForm control mesh",EncodeLiveFormMesh(mesh));
    solid->SetColor(mesh.GetColor());
    solid->SetMaterial(mesh.GetMaterial());
    solid->SetMaterialId(mesh.GetMaterialId());
    solid->m_LayerID = mesh.m_LayerID;
    if (!solid->EnsureRenderMesh()) {
        error = "Subdivide could not prepare the solid for display.";
        return {};
    }
    return solid;
}

namespace {

void CountFaces(FillContourResult& result)
{
    result.vertex_count = result.mesh->GetVertices().size();
    result.face_count = result.mesh->GetFaces().size();
    for (const CMesh3D::Face& face : result.mesh->GetFaces()) {
        if (face.deleted)
            continue;
        if (face.corners.size() == 4)
            ++result.quad_count;
        else if (face.corners.size() == 3)
            ++result.triangle_count;
    }
}

bool SaveBoundary(const std::filesystem::path& directory,
    FillContourResult& result)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        result.error = "Cannot create diagnostic directory: " + directory.string();
        return false;
    }
    const auto path = directory / "Dom3D_BoundaryLine.txt";
    std::ofstream stream(path, std::ios::out | std::ios::trunc);
    stream.precision(std::numeric_limits<float>::max_digits10);
    stream << "POLYLINE\n" << result.boundary.size() << "\n";
    for (Vec3 point : result.boundary)
        stream << point.x << " " << point.y << " " << point.z << "\n";
    stream.close();
    if (!stream) {
        result.error = "Cannot save " + path.string();
        return false;
    }
    return true;
}

bool SaveMesh(const CMesh3D& mesh, const std::filesystem::path& path,
    FillContourResult& result)
{
    if (mesh.ExportToObj(path.string()))
        return true;
    result.error = "Cannot save " + path.string();
    return false;
}

} // namespace

FillContourResult FillContour(const CPolyline& polyline,
    const FillContourOptions& options)
{
    FillContourResult result;
    if (polyline.GetPointCount() < 3) {
        result.error = "Select a CPolyline containing at least three points.";
        return result;
    }
    auto& contour = result.boundary;
    contour.reserve(polyline.GetPointCount());
    for (const CPoint3d& point : polyline.GetPoints()) {
        contour.push_back({static_cast<float>(point.x),
                           static_cast<float>(point.y),
                           static_cast<float>(point.z)});
    }
    // Preserve the existing Fill Contour treatment of text POLYLINE closure.
    // Filling closes the loop implicitly, so omit a repeated end record.
    const Vec3 closure = contour.front() - contour.back();
    if (contour.size() > 3 && dot(closure, closure) <= 1.0e-10f)
        contour.pop_back();
    if (contour.size() < 3) {
        result.error = "The CPolyline has fewer than three distinct boundary nodes.";
        return result;
    }

    Vec3 normal{};
    for (size_t index = 0; index < contour.size(); ++index) {
        const Vec3& current = contour[index];
        const Vec3& next = contour[(index + 1) % contour.size()];
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    normal = normalize(normal);
    if (dot(normal, normal) <= 1.0e-12f)
        normal = {0.0f, 0.0f, 1.0f};

    const auto& directory = options.diagnostic_directory;
    if (!directory.empty() && !SaveBoundary(directory, result))
        return result;

    result.mesh = std::make_unique<CMesh3D>(polyline.GetName() + " Fill Mesh");
    result.triangle_mesh = std::make_unique<CMesh3D>();
    auto& triangle_mesh = *result.triangle_mesh;
    auto& quade_mesh = *result.mesh;
    CSurfaceFace contour_filler;
    if (!contour_filler.MakeFilledContour(contour, normal, &quade_mesh,
            false, &result.error, &triangle_mesh, &result.quadrangulator_rejection)) {
        if (result.error.empty())
            result.error = "Mesh was not created.";
        return result;
    }
    // Temporary diagnostics: retain the old OBJ name and add the requested
    // triangle/quad pair. Save the actual intermediate mesh, not a rebuild.
    if (!directory.empty()) {
        if (!SaveMesh(triangle_mesh, directory / "Dom3D_ContourToFill.obj", result)
            || !SaveMesh(triangle_mesh,
                directory / "Dom3D_ContourToFill_Trisngle.obj", result))
            return result;
    }
    if (options.raw_quadrangulator
        && !Build3DCoatQuadrangulation(triangle_mesh.GetVertices(),
            triangle_mesh.GetFaces(), &quade_mesh, true)) {
        result.error = "The raw 3DCoat quadrangulator did not create a mesh.";
        return result;
    }
    if (!directory.empty()
        && !SaveMesh(quade_mesh, directory / "Dom3D_ContourToFillQuade.obj", result))
        return result;

    CountFaces(result);
    return result;
}

FillContourResult FillSurface(CSurfaceFace& surface, float density,
    const std::string& mesh_name)
{
    FillContourResult result;
    result.mesh = std::make_unique<CMesh3D>(mesh_name);
    if (!surface.MakeQuadMeshFromBoundary(density, result.mesh.get(), &result.boundary)) {
        result.error = "Mesh was not created. Check the surface boundary and density.";
        return result;
    }
    CountFaces(result);
    return result;
}

} // namespace Tools_Mesh3D
