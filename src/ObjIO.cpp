#include "ObjIO.h"

#include "solid/Solid.h"
#include "solid/SurfaceFace.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
struct ObjFaceVertex {
    size_t vertex = 0;
    size_t uv = 0;
    bool has_uv = false;
};

struct ObjMeshPart {
    std::string name = "Imported OBJ";
    std::string material_name;
    std::vector<std::vector<ObjFaceVertex>> faces;
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

bool parse_obj_index(const std::string& text, size_t count, size_t& index) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    try {
        value = std::stoi(text);
    } catch (...) {
        return false;
    }
    if (value > 0) {
        index = static_cast<size_t>(value - 1);
        return index < count;
    }
    if (value < 0) {
        const long long relative = static_cast<long long>(count) + value;
        if (relative < 0) {
            return false;
        }
        index = static_cast<size_t>(relative);
        return index < count;
    }
    return false;
}

bool parse_face_vertex(const std::string& token,
                       size_t vertex_count,
                       size_t uv_count,
                       ObjFaceVertex& result) {
    const size_t first_slash = token.find('/');
    const std::string vertex_text = token.substr(0, first_slash);
    if (!parse_obj_index(vertex_text, vertex_count, result.vertex)) {
        return false;
    }
    if (first_slash == std::string::npos) {
        return true;
    }
    const size_t second_slash = token.find('/', first_slash + 1);
    const std::string uv_text = token.substr(
        first_slash + 1,
        second_slash == std::string::npos ? std::string::npos : second_slash - first_slash - 1);
    if (!uv_text.empty()) {
        if (!parse_obj_index(uv_text, uv_count, result.uv)) {
            return false;
        }
        result.has_uv = true;
    }
    return true;
}

std::string unquoted(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    const size_t last = value.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    value = value.substr(first, last - first + 1);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string obj_identifier(std::string value, const std::string& fallback) {
    for (char& ch : value) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte <= 32 || ch == '#' || ch == '\\' || ch == '/') {
            ch = '_';
        }
    }
    while (!value.empty() && value.back() == '_') {
        value.pop_back();
    }
    return value.empty() ? fallback : value;
}

std::string unique_identifier(const std::string& requested, std::set<std::string>& used) {
    const std::string base = obj_identifier(requested, "Material");
    std::string candidate = base;
    int suffix = 2;
    while (!used.insert(candidate).second) {
        candidate = base + "_" + std::to_string(suffix++);
    }
    return candidate;
}

std::string material_export_key(const Material& material) {
    if (material.id != 0) {
        return "id:" + std::to_string(material.id);
    }

    std::ostringstream key;
    key.precision(9);
    key << "value:" << material.name
        << '|' << material.diffuse.r << '|' << material.diffuse.g << '|' << material.diffuse.b
        << '|' << material.ambient.r << '|' << material.ambient.g << '|' << material.ambient.b
        << '|' << material.emission.r << '|' << material.emission.g << '|' << material.emission.b
        << '|' << material.alpha << '|' << material.specular << '|' << material.shininess
        << '|' << material.reflectivity
        << '|' << material.color_texture_path
        << '|' << material.light_texture_path
        << '|' << material.bump_texture_path;
    return key.str();
}

std::string export_texture(const std::string& source,
                           const std::filesystem::path& texture_directory,
                           const std::string& material_name,
                           const std::string& role) {
    if (source.empty()) {
        return {};
    }

    const std::filesystem::path source_path(source);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(source_path, ec)) {
        return source_path.generic_string();
    }

    std::filesystem::create_directories(texture_directory, ec);
    if (ec) {
        return source_path.generic_string();
    }

    std::filesystem::path extension = source_path.extension();
    std::filesystem::path destination =
        texture_directory / (material_name + "_" + role + extension.string());
    int suffix = 2;
    while (std::filesystem::exists(destination, ec)) {
        ec.clear();
        if (std::filesystem::equivalent(source_path, destination, ec) && !ec) {
            return (texture_directory.filename() / destination.filename()).generic_string();
        }
        destination = texture_directory
            / (material_name + "_" + role + "_" + std::to_string(suffix++) + extension.string());
    }
    ec.clear();
    std::filesystem::copy_file(
        source_path, destination, std::filesystem::copy_options::overwrite_existing, ec);
    return ec ? source_path.generic_string()
              : (texture_directory.filename() / destination.filename()).generic_string();
}

void write_mtl_material(std::ostream& stream,
                        const Material& material,
                        const std::string& material_name,
                        const std::filesystem::path& texture_directory) {
    const float specular = std::clamp(material.specular, 0.0f, 1.0f);
    stream << "newmtl " << material_name << "\n";
    stream << "Ka " << material.ambient.r << " " << material.ambient.g << " " << material.ambient.b << "\n";
    stream << "Kd " << material.diffuse.r << " " << material.diffuse.g << " " << material.diffuse.b << "\n";
    stream << "Ks " << specular << " " << specular << " " << specular << "\n";
    stream << "Ke " << material.emission.r << " " << material.emission.g << " " << material.emission.b << "\n";
    stream << "Ns " << std::clamp(material.shininess, 1.0f, 256.0f) << "\n";
    stream << "d " << std::clamp(material.alpha, 0.0f, 1.0f) << "\n";
    stream << "illum 2\n";

    const std::string color_texture =
        export_texture(material.color_texture_path, texture_directory, material_name, "color");
    const std::string light_texture =
        export_texture(material.light_texture_path, texture_directory, material_name, "light");
    const std::string bump_texture =
        export_texture(material.bump_texture_path, texture_directory, material_name, "bump");
    if (!color_texture.empty()) stream << "map_Kd " << color_texture << "\n";
    if (!light_texture.empty()) stream << "map_Ke " << light_texture << "\n";
    if (!bump_texture.empty()) stream << "map_Bump " << bump_texture << "\n";
    stream << "\n";
}

UV transformed_uv(UV uv, const Material& material) {
    const float radians = material.texture_rotation_degrees * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float u = uv.u * material.texture_scale_u;
    const float v = uv.v * material.texture_scale_v;
    return {
        u * cosine - v * sine + material.texture_offset_u,
        u * sine + v * cosine + material.texture_offset_v
    };
}

bool load_mtl(const std::filesystem::path& path,
              std::map<std::string, Material>& materials) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    Material* current = nullptr;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string keyword;
        stream >> keyword;
        if (keyword == "newmtl") {
            const std::string name = read_obj_name(stream, "Material");
            Material material = Material::ImportedMesh();
            material.id = 0;
            material.name = name;
            material.source_file_path.clear();
            current = &materials.insert_or_assign(name, std::move(material)).first->second;
        } else if (current && (keyword == "Ka" || keyword == "Kd" || keyword == "Ke")) {
            Color color{};
            stream >> color.r >> color.g >> color.b;
            if (!stream) {
                continue;
            }
            if (keyword == "Ka") current->ambient = color;
            else if (keyword == "Kd") current->diffuse = color;
            else current->emission = color;
        } else if (current && keyword == "Ks") {
            Color color{};
            stream >> color.r >> color.g >> color.b;
            if (stream) current->specular = std::max({color.r, color.g, color.b});
        } else if (current && keyword == "Ns") {
            float value = 0.0f;
            if (stream >> value) current->shininess = std::clamp(value, 1.0f, 256.0f);
        } else if (current && keyword == "d") {
            float value = 1.0f;
            if (stream >> value) current->alpha = std::clamp(value, 0.0f, 1.0f);
        } else if (current && keyword == "Tr") {
            float value = 0.0f;
            if (stream >> value) current->alpha = 1.0f - std::clamp(value, 0.0f, 1.0f);
        } else if (current && (keyword == "map_Kd" || keyword == "map_Ke"
                               || keyword == "map_Bump" || keyword == "map_bump" || keyword == "bump")) {
            std::string texture;
            std::getline(stream, texture);
            texture = unquoted(texture);
            if (texture.empty()) {
                continue;
            }
            std::filesystem::path texture_path(texture);
            if (texture_path.is_relative()) {
                texture_path = path.parent_path() / texture_path;
            }
            const std::string resolved = texture_path.lexically_normal().string();
            if (keyword == "map_Kd") current->color_texture_path = resolved;
            else if (keyword == "map_Ke") current->light_texture_path = resolved;
            else current->bump_texture_path = resolved;
        }
    }
    return true;
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
    std::vector<UV> texture_coordinates;
    std::map<std::string, Material> materials;
    std::vector<ObjMeshPart> parts;
    parts.push_back({});
    ObjMeshPart* current = &parts.back();
    std::string current_object_name = current->name;
    std::string current_material_name;
    const std::filesystem::path obj_path(path);
    const auto start_part = [&]() -> ObjMeshPart* {
        if (current->faces.empty()) {
            current->name = current_object_name;
            current->material_name = current_material_name;
            return current;
        }
        parts.push_back({current_object_name, current_material_name, {}});
        return &parts.back();
    };
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
        } else if (keyword == "vt") {
            UV uv{};
            line_stream >> uv.u >> uv.v;
            if (!line_stream) {
                error = "OBJ file has an invalid texture coordinate.";
                return false;
            }
            texture_coordinates.push_back(uv);
        } else if (keyword == "mtllib") {
            std::string library_name;
            std::getline(line_stream, library_name);
            library_name = unquoted(library_name);
            if (!library_name.empty()) {
                std::filesystem::path mtl_path(library_name);
                if (mtl_path.is_relative()) {
                    mtl_path = obj_path.parent_path() / mtl_path;
                }
                load_mtl(mtl_path.lexically_normal(), materials);
            }
        } else if (keyword == "usemtl") {
            current_material_name = read_obj_name(line_stream, {});
            current = start_part();
        } else if (keyword == "o" || keyword == "g") {
            const std::string fallback = "Imported OBJ " + std::to_string(parts.size() + (current->faces.empty() ? 0 : 1));
            current_object_name = read_obj_name(line_stream, fallback);
            current = start_part();
        } else if (keyword == "f") {
            std::vector<ObjFaceVertex> face;
            std::string token;
            while (line_stream >> token) {
                ObjFaceVertex index;
                if (!parse_face_vertex(token, vertices.size(), texture_coordinates.size(), index)) {
                    error = "OBJ file has an invalid face.";
                    return false;
                }
                face.push_back(index);
            }
            if (face.size() < 3) {
                error = "OBJ file has a face with fewer than 3 vertices.";
                return false;
            }
            current->faces.push_back(std::move(face));
        }
    }

    int mesh_index = 1;
    for (const ObjMeshPart& part : parts) {
        if (part.faces.empty()) {
            continue;
        }

        std::vector<Vec3> local_vertices;
        std::vector<UV> local_uvs;
        std::vector<CMesh3D::Face> local_faces;
        std::map<std::pair<size_t, long long>, size_t> index_map;
        bool has_complete_uvs = true;
        for (const std::vector<ObjFaceVertex>& global_face : part.faces) {
            CMesh3D::Face local_face;
            for (const ObjFaceVertex& global_index : global_face) {
                const long long uv_key = global_index.has_uv ? static_cast<long long>(global_index.uv) : -1;
                auto [it, inserted] = index_map.emplace(
                    std::make_pair(global_index.vertex, uv_key), local_vertices.size());
                if (inserted) {
                    if (global_index.vertex >= vertices.size()) {
                        error = "OBJ file has a face index outside vertex list.";
                        return false;
                    }
                    local_vertices.push_back(vertices[global_index.vertex]);
                    local_uvs.push_back(global_index.has_uv ? texture_coordinates[global_index.uv] : UV{});
                }
                has_complete_uvs = has_complete_uvs && global_index.has_uv;
                local_face.push_back(it->second);
            }
            local_faces.push_back(std::move(local_face));
        }

        auto loaded = std::make_unique<CMesh3D>(part.name.empty() ? "Imported OBJ " + std::to_string(mesh_index) : part.name);
        const auto material = materials.find(part.material_name);
        if (material != materials.end()) {
            loaded->SetMaterial(material->second);
            if (!part.material_name.empty()) {
                loaded->SetName(loaded->GetName() + " [" + part.material_name + "]");
            }
        } else {
            loaded->SetColor({0.42f, 0.57f, 0.36f});
        }
        if (!loaded->SetGeometry(std::move(local_vertices),
                                 std::move(local_faces),
                                 has_complete_uvs ? std::move(local_uvs) : std::vector<UV>{})) {
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
    const std::filesystem::path obj_path(path);
    const std::filesystem::path mtl_path = obj_path.parent_path() / (obj_path.stem().string() + ".mtl");
    const std::filesystem::path texture_directory =
        obj_path.parent_path() / (obj_identifier(obj_path.stem().string(), "obj") + "_textures");

    std::ofstream file(obj_path);
    if (!file) {
        error = "Could not save OBJ file.";
        return false;
    }
    std::ofstream material_file(mtl_path);
    if (!material_file) {
        error = "Could not save OBJ material library.";
        return false;
    }

    file << "# Dom3D Pro OBJ export\n";
    file << "mtllib " << mtl_path.filename().generic_string() << "\n\n";
    material_file << "# Dom3D Pro material library\n\n";
    size_t vertex_offset = 0;
    size_t uv_offset = 0;
    size_t mesh_count = 0;
    std::set<std::string> used_material_names;
    std::map<std::string, std::string> exported_materials;
    const auto export_material = [&](const Material& material, const std::string& fallback_name) {
        const std::string key = material_export_key(material);
        const auto existing = exported_materials.find(key);
        if (existing != exported_materials.end()) {
            return existing->second;
        }

        const std::string material_name =
            unique_identifier(material.name.empty() ? fallback_name : material.name, used_material_names);
        write_mtl_material(material_file, material, material_name, texture_directory);
        exported_materials.emplace(key, material_name);
        return material_name;
    };

    for (const auto& object : document.GetObjects()) {
        const auto* mesh = dynamic_cast<const CMesh3D*>(object.get());
        if (mesh && mesh->IsVisible()) {
            const Material material = mesh->GetMaterial();
            const std::string material_name = export_material(material, mesh->GetName());
            if (!ExportMesh(file, *mesh, material, material_name, vertex_offset, uv_offset, mesh->GetName())) {
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

        const Material solid_material = solid->GetMaterial();
        const std::string solid_material_name = export_material(solid_material, solid->GetName());
        for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
            if (!surface || !surface->pMesh3D) {
                continue;
            }

            const std::string surface_name = solid->GetName() + "_Surface_" + std::to_string(surface_index + 1);
            Material material = solid_material;
            material.texture_offset_u += surface->TextureTransform.offset_u;
            material.texture_offset_v += surface->TextureTransform.offset_v;
            material.texture_scale_u *= surface->TextureTransform.scale_u;
            material.texture_scale_v *= surface->TextureTransform.scale_v;
            material.texture_rotation_degrees += surface->TextureTransform.rotation_degrees;
            if (!ExportMesh(file,
                            *surface->pMesh3D,
                            material,
                            solid_material_name,
                            vertex_offset,
                            uv_offset,
                            surface_name)) {
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

    if (!file || !material_file) {
        error = "Could not finish writing OBJ material data.";
        return false;
    }
    return true;
}

bool ObjIO::ExportMesh(std::ostream& stream,
                       const CMesh3D& mesh,
                       const Material& material,
                       const std::string& material_name,
                       size_t& vertex_offset,
                       size_t& uv_offset,
                       const std::string& object_name) const {
    stream << "o " << obj_identifier(object_name, "Object") << "\n";
    stream << "usemtl " << material_name << "\n";

    for (const Vec3& vertex : mesh.GetVertices()) {
        stream << "v " << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
    }

    const bool has_uvs = mesh.GetUVs().size() == mesh.GetVertices().size();
    if (has_uvs) {
        for (UV uv : mesh.GetUVs()) {
            uv = transformed_uv(uv, material);
            stream << "vt " << uv.u << " " << uv.v << "\n";
        }
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
            if (has_uvs) {
                stream << "/" << (index + uv_offset + 1);
            }
        }
        stream << "\n";
    }

    vertex_offset += mesh.GetVertices().size();
    if (has_uvs) {
        uv_offset += mesh.GetUVs().size();
    }
    stream << "\n";
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
