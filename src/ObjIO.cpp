#include "ObjIO.h"

#include "solid/Solid.h"
#include "solid/SurfaceFace.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
struct ObjMeshPart {
    std::string name = "Imported OBJ";
    std::vector<CMesh3D::Face> global_faces;
};

std::string read_obj_name(std::istringstream& stream, const std::string& fallback)
{
    std::string name;
    std::getline(stream, name);
    const size_t first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return fallback;
    }
    const size_t last = name.find_last_not_of(" \t\r\n");
    return name.substr(first, last - first + 1);
}
}

bool ObjIO::Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes, std::string& error) const {
    meshes.clear();
    std::ifstream file(path);
    if (!file) {
        error = "Could not open OBJ file.";
        return false;
    }

    std::vector<Vec3> vertices;
    std::vector<ObjMeshPart> parts;
    parts.push_back({});
    ObjMeshPart* current = &parts.back();
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        std::string keyword;
        line_stream >> keyword;
        if (keyword == "v") {
            Vec3 vertex{};
            line_stream >> vertex.x >> vertex.y >> vertex.z;
            if (!line_stream) {
                error = "OBJ file has an invalid vertex.";
                return false;
            }
            vertices.push_back(vertex);
        } else if (keyword == "o" || keyword == "g") {
            const std::string fallback = "Imported OBJ " + std::to_string(parts.size() + (current->global_faces.empty() ? 0 : 1));
            const std::string name = read_obj_name(line_stream, fallback);
            if (current->global_faces.empty()) {
                current->name = name;
            } else {
                parts.push_back({name, {}});
                current = &parts.back();
            }
        } else if (keyword == "f") {
            CMesh3D::Face face;
            std::string token;
            while (line_stream >> token) {
                size_t index = 0;
                if (!ParseFaceIndex(token, vertices.size(), index)) {
                    error = "OBJ file has an invalid face.";
                    return false;
                }
                face.push_back(index);
            }
            if (face.size() < 3) {
                error = "OBJ file has a face with fewer than 3 vertices.";
                return false;
            }
            current->global_faces.push_back(std::move(face));
        }
    }

    int mesh_index = 1;
    for (const ObjMeshPart& part : parts) {
        if (part.global_faces.empty()) {
            continue;
        }

        std::vector<Vec3> local_vertices;
        std::vector<CMesh3D::Face> local_faces;
        std::map<size_t, size_t> index_map;
        for (const CMesh3D::Face& global_face : part.global_faces) {
            CMesh3D::Face local_face;
            for (size_t global_index : global_face) {
                auto [it, inserted] = index_map.emplace(global_index, local_vertices.size());
                if (inserted) {
                    if (global_index >= vertices.size()) {
                        error = "OBJ file has a face index outside vertex list.";
                        return false;
                    }
                    local_vertices.push_back(vertices[global_index]);
                }
                local_face.push_back(it->second);
            }
            local_faces.push_back(std::move(local_face));
        }

        auto loaded = std::make_unique<CMesh3D>(part.name.empty() ? "Imported OBJ " + std::to_string(mesh_index) : part.name);
        loaded->SetColor({0.42f, 0.57f, 0.36f});
        if (!loaded->SetGeometry(std::move(local_vertices), std::move(local_faces))) {
            error = "OBJ file has invalid mesh geometry.";
            return false;
        }
        meshes.push_back(std::move(loaded));
        ++mesh_index;
    }

    if (meshes.empty()) {
        error = "OBJ file has no valid mesh geometry.";
        return false;
    }

    return true;
}

bool ObjIO::Export(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    std::ofstream file(path);
    if (!file) {
        error = "Could not save OBJ file.";
        return false;
    }

    file << "# Dom3D Pro OBJ export\n";
    size_t vertex_offset = 0;
    size_t mesh_count = 0;

    for (const auto& object : document.GetObjects()) {
        const auto* mesh = dynamic_cast<const CMesh3D*>(object.get());
        if (mesh && mesh->IsVisible()) {
            if (!ExportMesh(file, *mesh, vertex_offset, mesh->GetName())) {
                error = "Could not write mesh data.";
                return false;
            }
            ++mesh_count;
            continue;
        }

        const auto* solid = dynamic_cast<const CSolid*>(object.get());
        if (!solid || !solid->IsVisible()) {
            continue;
        }

        for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
            if (!surface || !surface->pMesh3D) {
                continue;
            }

            const std::string surface_name = solid->GetName() + "_Surface_" + std::to_string(surface_index + 1);
            if (!ExportMesh(file, *surface->pMesh3D, vertex_offset, surface_name)) {
                error = "Could not write surface mesh data.";
                return false;
            }
            ++mesh_count;
        }
    }

    if (mesh_count == 0) {
        error = "There are no visible mesh or surface objects to export.";
        return false;
    }

    return static_cast<bool>(file);
}

bool ObjIO::ExportMesh(std::ostream& stream, const CMesh3D& mesh, size_t& vertex_offset, const std::string& object_name) const {
    stream << "o " << object_name << "\n";

    for (const Vec3& vertex : mesh.GetVertices()) {
        stream << "v " << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
    }

    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        if (face.size() < 3) {
            continue;
        }

        stream << "f";
        for (size_t index : face) {
            if (index >= mesh.GetVertices().size()) {
                return false;
            }
            stream << " " << (index + vertex_offset + 1);
        }
        stream << "\n";
    }

    vertex_offset += mesh.GetVertices().size();
    return static_cast<bool>(stream);
}

bool ObjIO::ParseFaceIndex(const std::string& token, size_t vertex_count, size_t& index) const {
    const size_t slash = token.find('/');
    const std::string vertex_part = slash == std::string::npos ? token : token.substr(0, slash);
    if (vertex_part.empty()) {
        return false;
    }

    int obj_index = 0;
    try {
        obj_index = std::stoi(vertex_part);
    } catch (...) {
        return false;
    }

    if (obj_index > 0) {
        const size_t zero_based = static_cast<size_t>(obj_index - 1);
        if (zero_based >= vertex_count) {
            return false;
        }
        index = zero_based;
        return true;
    }

    if (obj_index < 0) {
        const int relative = static_cast<int>(vertex_count) + obj_index;
        if (relative < 0) {
            return false;
        }
        index = static_cast<size_t>(relative);
        return index < vertex_count;
    }

    return false;
}
