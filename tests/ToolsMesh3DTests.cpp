#include "Tools_Mesh3D.h"
#include "CPolyline.h"
#include "ContourQuadrangulator3DCoat.h"
#include "solid/SurfaceFace.h"

#include <QTemporaryDir>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <cmath>

namespace {
void Check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string Read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    Check(bool(stream), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void CheckClosedQuadPatch(const Tools_Mesh3D::FillContourResult& result)
{
    const auto& vertices = result.mesh->GetVertices();
    const auto& faces = result.mesh->GetFaces();
    std::map<std::pair<size_t, size_t>, int> edges;
    std::map<std::pair<size_t, size_t>, int> directions;
    std::map<size_t, std::set<size_t>> neighbours;
    double mesh_area = 0.0;
    for (const auto& face : faces) {
        if (face.deleted) continue;
        Check(face.corners.size() == 4, "UV patch must contain only quads.");
        double area = 0.0;
        for (size_t i = 0; i < face.corners.size(); ++i) {
            const size_t a = face.corners[i].v;
            const size_t b = face.corners[(i + 1) % face.corners.size()].v;
            Check(a < vertices.size() && b < vertices.size() && a != b,
                "Invalid quad vertex index.");
            const auto key = std::minmax(a, b);
            ++edges[key];
            directions[key] += a < b ? 1 : -1;
            neighbours[a].insert(b);
            neighbours[b].insert(a);
            area += double(vertices[a].x) * vertices[b].y
                - double(vertices[b].x) * vertices[a].y;
        }
        Check(area > 0.0, "Quad has zero or reversed UV area.");
        mesh_area += area;
    }
    std::vector<int> input_index(vertices.size(), -1);
    for (size_t i = 0; i < vertices.size(); ++i)
        for (size_t j = 0; j < result.boundary.size(); ++j)
            if (vertices[i].x == result.boundary[j].x
                && vertices[i].y == result.boundary[j].y
                && vertices[i].z == result.boundary[j].z)
                input_index[i] = static_cast<int>(j);
    size_t boundary_count = 0;
    for (const auto& entry : edges) {
        Check(entry.second <= 2, "Non-manifold quad edge.");
        if (entry.second == 2) {
            Check(directions[entry.first] == 0, "Interior quad edge orientation mismatch.");
            continue;
        }
        ++boundary_count;
        const int a = input_index[entry.first.first];
        const int b = input_index[entry.first.second];
        const int count = static_cast<int>(result.boundary.size());
        Check(a >= 0 && b >= 0 && ((a + 1) % count == b || (b + 1) % count == a),
            "Quad patch has an open interior front (the UV slit).");
    }
    Check(boundary_count == result.boundary.size(), "Quad patch lost boundary edges.");
    std::set<size_t> visited;
    std::vector<size_t> queue{neighbours.begin()->first};
    for (size_t i = 0; i < queue.size(); ++i)
        if (visited.insert(queue[i]).second)
            for (size_t next : neighbours[queue[i]]) queue.push_back(next);
    Check(visited.size() == neighbours.size(), "Quad patch is disconnected.");
    double boundary_area = 0.0;
    for (size_t i = 0; i < result.boundary.size(); ++i) {
        const auto& a = result.boundary[i];
        const auto& b = result.boundary[(i + 1) % result.boundary.size()];
        boundary_area += double(a.x) * b.y - double(b.x) * a.y;
    }
    Check(std::abs(mesh_area - boundary_area) < std::abs(boundary_area) * 1.0e-6,
        "Quad patch does not cover the input UV area.");
}
} // namespace

void TestToolsMesh3D(const char* boundary_path, const char* diagnostic_directory)
{
    std::ifstream input(boundary_path);
    std::vector<std::unique_ptr<CPolyline>> lines;
    std::string error;
    Check(CPolyline::LoadTextPolylines(input, lines, error), error);
    Check(lines.size() == 1 && lines.front()->GetPointCount() == 60,
        "Expected the single 60-node Extrude_SL UV boundary.");
    auto& line = *lines.front();
    line.SetClosed(true);
    const auto original_points = line.GetPoints();

    QTemporaryDir temporary;
    Check(temporary.isValid(), "Cannot create test directory.");
    const std::filesystem::path directory(temporary.path().toStdString());
    Tools_Mesh3D::FillContourOptions options;
    options.diagnostic_directory = diagnostic_directory
        ? std::filesystem::path(diagnostic_directory) : directory / "dumps";
    for (bool raw : {false, true}) {
        options.raw_quadrangulator = raw;
        auto result = Tools_Mesh3D::FillContour(line, options);
        Check(bool(result), result.error);
        CheckClosedQuadPatch(result);
        Check(result.boundary.size() == 60, "The tool changed boundary node count.");
        for (size_t i = 0; i < original_points.size(); ++i) {
            const auto& point = original_points[i];
            const auto& actual = result.boundary[i];
            Check(actual.x == static_cast<float>(point.x)
                && actual.y == static_cast<float>(point.y)
                && actual.z == static_cast<float>(point.z),
                "The tool scaled, reordered or resampled the UV boundary.");
            const auto& unchanged = line.GetPoints()[i];
            Check(point.x == unchanged.x && point.y == unchanged.y
                && point.z == unchanged.z, "The tool mutated the source polyline.");
        }

        // Reproduce the pre-extraction UI code independently and compare the
        // complete OBJ bytes, including normals, UVs and face connectivity.
        Vec3 normal{};
        for (size_t i = 0; i < result.boundary.size(); ++i) {
            const Vec3& a = result.boundary[i];
            const Vec3& b = result.boundary[(i + 1) % result.boundary.size()];
            normal.x += (a.y - b.y) * (a.z + b.z);
            normal.y += (a.z - b.z) * (a.x + b.x);
            normal.z += (a.x - b.x) * (a.y + b.y);
        }
        normal = normalize(normal);
        CSurfaceFace filler;
        CMesh3D triangle, reference(line.GetName() + " Fill Mesh");
        std::string rejection;
        Check(filler.MakeFilledContour(result.boundary, normal, &reference,
            false, &error, &triangle, &rejection), error);
        if (raw)
            Check(Build3DCoatQuadrangulation(triangle.GetVertices(), triangle.GetFaces(),
                &reference, true), "Reference raw quadrangulation failed.");
        Check(result.quadrangulator_rejection == rejection,
            "Production diagnostic changed during extraction.");
        Check(triangle.ExportToObj((directory / "reference-triangle.obj").string()),
            "Reference triangle export failed.");
        Check(reference.ExportToObj((directory / "reference-quad.obj").string()),
            "Reference quad export failed.");
        const auto& dumps = options.diagnostic_directory;
        Check(Read(directory / "reference-triangle.obj")
            == Read(dumps / "Dom3D_ContourToFill_Trisngle.obj"),
            "Triangle output changed during extraction.");
        Check(Read(dumps / "Dom3D_ContourToFill.obj")
            == Read(dumps / "Dom3D_ContourToFill_Trisngle.obj"),
            "Legacy triangle dump differs.");
        Check(Read(directory / "reference-quad.obj")
            == Read(dumps / "Dom3D_ContourToFillQuade.obj"),
            "Quad output changed during extraction.");
        std::ifstream saved(dumps / "Dom3D_BoundaryLine.txt");
        std::vector<std::unique_ptr<CPolyline>> saved_lines;
        Check(CPolyline::LoadTextPolylines(saved, saved_lines, error), error);
        Check(saved_lines.size() == 1 && saved_lines.front()->GetPointCount() == 60,
            "Boundary dumps must overwrite, not append contours.");
        for (size_t i = 0; i < result.boundary.size(); ++i) {
            const auto& point = saved_lines.front()->GetPoints()[i];
            const auto& actual = result.boundary[i];
            Check(static_cast<float>(point.x) == actual.x
                && static_cast<float>(point.y) == actual.y
                && static_cast<float>(point.z) == actual.z,
                "Boundary dump loses input precision.");
        }
    }
    for (double scale : {0.5, 2.0, 10.0}) {
        for (bool scale_u_only : {false, true}) {
            CPolyline scaled;
            for (const auto& point : original_points)
                scaled.AddPoint(CPoint3d(point.x * scale,
                    point.y * (scale_u_only ? 1.0 : scale), point.z));
            scaled.SetClosed(true);
            auto result = Tools_Mesh3D::FillContour(scaled);
            Check(bool(result), result.error);
            CheckClosedQuadPatch(result);
        }
    }
    // Preserve repeated closing-point handling, also with diagnostics disabled.
    line.AddPoint(original_points.front());
    auto closed = Tools_Mesh3D::FillContour(line);
    Check(bool(closed) && closed.boundary.size() == 60,
        "Repeated closure point handling changed.");
    CPolyline empty;
    auto invalid = Tools_Mesh3D::FillContour(empty);
    Check(!invalid && !invalid.error.empty(), "Invalid input needs an error.");
    options.diagnostic_directory = directory / "reference-triangle.obj";
    auto blocked = Tools_Mesh3D::FillContour(line, options);
    Check(!blocked && !blocked.error.empty(), "Failed diagnostic writes need an error.");
    std::cout << "Tools_Mesh3D: unchanged UV input, triangle and quad outputs in both modes; diagnostics passed.\n";
}
