#include "ObjIO.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

bool ObjIO::Import(const std::string& path, std::unique_ptr<CMesh3D>& mesh, std::string& error) const {
    std::ifstream file(path);
    if (!file) {
        error = "Could not open OBJ file.";
        return false;
    }

    std::vector<Vec3> vertices;
    std::vector<CMesh3D::Face> faces;
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
            faces.push_back(std::move(face));
        }
    }

    auto loaded = std::make_unique<CMesh3D>("Imported OBJ");
    loaded->SetColor({0.42f, 0.57f, 0.36f});
    if (!loaded->SetGeometry(std::move(vertices), std::move(faces))) {
        error = "OBJ file has no valid mesh geometry.";
        return false;
    }

    mesh = std::move(loaded);
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

    if (const CMesh3D* selected_mesh = document.GetSelectedMesh()) {
        if (!ExportMesh(file, *selected_mesh, vertex_offset)) {
            error = "Could not write selected mesh.";
            return false;
        }
        mesh_count = 1;
    } else {
        for (const auto& object : document.GetObjects()) {
            const auto* mesh = dynamic_cast<const CMesh3D*>(object.get());
            if (mesh) {
                if (!ExportMesh(file, *mesh, vertex_offset)) {
                    error = "Could not write mesh data.";
                    return false;
                }
                ++mesh_count;
            }
        }
    }

    if (mesh_count == 0) {
        error = "There are no mesh objects to export.";
        return false;
    }

    return static_cast<bool>(file);
}

bool ObjIO::ExportMesh(std::ostream& stream, const CMesh3D& mesh, size_t& vertex_offset) const {
    stream << "o " << mesh.GetName() << "\n";

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
