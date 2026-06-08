#include "CMesh3D.h"

#include "OpenGLCompat.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
QString resolve_texture_path(const std::string& texture_path) {
    const QString path = QString::fromStdString(texture_path).trimmed();
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    if (info.isAbsolute() && info.exists()) {
        return info.absoluteFilePath();
    }

    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString material_path = app_dir.filePath("materials/" + path);
    if (QFileInfo::exists(material_path)) {
        return QFileInfo(material_path).absoluteFilePath();
    }

    const QString app_relative_path = app_dir.filePath(path);
    if (QFileInfo::exists(app_relative_path)) {
        return QFileInfo(app_relative_path).absoluteFilePath();
    }

    if (info.exists()) {
        return info.absoluteFilePath();
    }

    return {};
}

GLuint texture_id_for_path(const std::string& texture_path) {
    const QString resolved = resolve_texture_path(texture_path);
    if (resolved.isEmpty()) {
        return 0;
    }

    static std::unordered_map<std::string, GLuint> texture_cache;
    const std::string key = QDir::toNativeSeparators(resolved).toStdString();
    const auto found = texture_cache.find(key);
    if (found != texture_cache.end()) {
        return found->second;
    }

    QImage image(resolved);
    if (image.isNull()) {
        texture_cache[key] = 0;
        return 0;
    }

    image = image.convertToFormat(QImage::Format_RGBA8888).mirrored(false, true);
    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 image.width(),
                 image.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image.constBits());
    glBindTexture(GL_TEXTURE_2D, 0);

    texture_cache[key] = texture_id;
    return texture_id;
}

UV projected_uv_for_face(Vec3 vertex, Vec3 face_normal) {
    const Vec3 n = normalize(face_normal);
    const float ax = std::fabs(n.x);
    const float ay = std::fabs(n.y);
    const float az = std::fabs(n.z);

    if (ay >= ax && ay >= az) {
        return {vertex.x, vertex.z};
    }
    if (az >= ax) {
        return {vertex.x, vertex.y};
    }
    return {vertex.z, vertex.y};
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Color shaded_color(Color base, Vec3 normal, float specular_strength, float shininess, bool selected) {
    const Vec3 n = normalize(normal);
    const Vec3 key_light = normalize({-0.45f, 0.82f, 0.36f});
    const Vec3 fill_light = normalize({0.72f, 0.42f, -0.58f});
    const Vec3 camera_fill_light = normalize({0.28f, 0.28f, 0.92f});
    const Vec3 view_dir = normalize({0.30f, 0.45f, 0.84f});
    const Vec3 half_vector = normalize(key_light + view_dir);
    const float key = std::max(0.0f, dot(n, key_light));
    const float fill = std::max(0.0f, dot(n, fill_light));
    const float camera_fill = std::max(0.0f, dot(n, camera_fill_light));
    const float sky = std::max(0.0f, n.y);
    const float side = 1.0f - std::fabs(n.y);
    const float shade = std::min(1.38f, 0.40f + key * 0.56f + fill * 0.18f + camera_fill * 0.14f + sky * 0.13f + side * 0.07f);
    const float gloss = std::pow(std::max(0.0f, dot(n, half_vector)), std::max(4.0f, shininess));
    const float rim = std::pow(std::max(0.0f, 1.0f - std::fabs(dot(n, view_dir))), 3.0f) * std::max(key, camera_fill * 0.45f);
    const float highlight = std::min(0.32f, specular_strength * (gloss * 0.68f + rim * 0.12f));
    const float selected_boost = selected ? 1.08f : 1.0f;

    return {
        clamp01(base.r * shade * selected_boost + key * 0.06f + highlight),
        clamp01(base.g * shade * selected_boost + key * 0.06f + highlight),
        clamp01(base.b * shade * selected_boost + key * 0.08f + highlight)
    };
}

Color normal_rgb_color(Vec3 normal) {
    const Vec3 n = normalize(normal);
    const float x = std::fabs(n.x);
    const float y = std::fabs(n.y);
    const float z = std::fabs(n.z);
    const float sum = std::max(0.0001f, x + y + z);
    const Color x_color{1.00f, 0.42f, 0.38f};
    const Color y_color{0.42f, 1.00f, 0.30f};
    const Color z_color{0.42f, 0.68f, 1.00f};
    const float ambient = 0.16f;

    return {
        clamp01(ambient + (x_color.r * x + y_color.r * y + z_color.r * z) / sum * 0.84f),
        clamp01(ambient + (x_color.g * x + y_color.g * y + z_color.g * z) / sum * 0.84f),
        clamp01(ambient + (x_color.b * x + y_color.b * y + z_color.b * z) / sum * 0.84f)
    };
}

Color wire_color(Color base, MeshDisplayMode mode, bool selected) {
    if (mode == MeshDisplayMode::SurfaceGray) {
        return selected ? Color{0.045f, 0.050f, 0.052f} : Color{0.075f, 0.083f, 0.087f};
    }
    if (mode == MeshDisplayMode::Wire) {
        return selected ? Color{0.080f, 0.130f, 0.105f} : Color{0.120f, 0.175f, 0.140f};
    }

    const float luminance = base.r * 0.30f + base.g * 0.59f + base.b * 0.11f;
    const float shade = selected ? 0.20f : (luminance > 0.58f ? 0.27f : 0.34f);
    const float neutral = selected ? 0.022f : (luminance > 0.58f ? 0.035f : 0.050f);
    return {
        clamp01(base.r * shade + neutral),
        clamp01(base.g * shade + neutral),
        clamp01(base.b * shade + neutral)
    };
}

Material colored_mesh_material() {
    return Material::ImportedMesh();
}

}

CMesh3D::CMesh3D()
    : CAlfaObject("Mesh3D") {
    SetMaterial(colored_mesh_material());
}

Material CMesh3D::material_Defailt = Material::DefaultMesh();
float CMesh3D::s_WireOpacity = 0.76f;
MeshDisplayMode CMesh3D::s_DisplayMode = MeshDisplayMode::SurfaceGray;

CMesh3D::CMesh3D(std::string name)
    : CAlfaObject(std::move(name)) {
    SetMaterial(colored_mesh_material());
}

const std::vector<Vec3>& CMesh3D::GetVertices() const {
    return vertices_;
}

const std::vector<CMesh3D::Face>& CMesh3D::GetFaces() const {
    return faces_;
}

const std::vector<UV>& CMesh3D::GetUVs() const {
    return uvs_;
}

bool CMesh3D::SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces) {
    return SetGeometry(std::move(vertices), std::move(faces), {});
}

bool CMesh3D::SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces, std::vector<UV> uvs) {
    if (vertices.empty() || faces.empty()) {
        return false;
    }

    for (const Face& face : faces) {
        if (!IsValidFace(face, vertices.size())) {
            return false;
        }
    }

    vertices_ = std::move(vertices);
    faces_ = std::move(faces);
    uvs_ = std::move(uvs);
    if (uvs_.size() != vertices_.size()) {
        GeneratePlanarUVs();
    }
    return true;
}

void CMesh3D::GeneratePlanarUVs() {
    uvs_.clear();
    uvs_.resize(vertices_.size());
    if (vertices_.empty()) {
        return;
    }

    float min_x = vertices_.front().x;
    float max_x = vertices_.front().x;
    float min_z = vertices_.front().z;
    float max_z = vertices_.front().z;
    for (const Vec3& vertex : vertices_) {
        min_x = std::min(min_x, vertex.x);
        max_x = std::max(max_x, vertex.x);
        min_z = std::min(min_z, vertex.z);
        max_z = std::max(max_z, vertex.z);
    }

    const float width = std::max(max_x - min_x, 0.0001f);
    const float depth = std::max(max_z - min_z, 0.0001f);
    for (size_t i = 0; i < vertices_.size(); ++i) {
        uvs_[i].u = (vertices_[i].x - min_x) / width;
        uvs_[i].v = (vertices_[i].z - min_z) / depth;
    }
}

void CMesh3D::Render() {
    Render3d(false);
}

void CMesh3D::Render3d(bool selected) const {
    const MeshDisplayMode mode = GetDisplayMode();
    if (mode == MeshDisplayMode::Wire) {
        const Material material = mode == MeshDisplayMode::SurfaceGray ? material_Defailt : GetMaterial();
        RenderWire(selected, true, &material.diffuse);
        return;
    }

    const Material material = mode == MeshDisplayMode::SurfaceGray ? material_Defailt : GetMaterial();

    RenderFaces(selected, true, &material);
    RenderWire(selected, true, &material.diffuse);
}

void CMesh3D::RenderFaces(bool selected, bool offset_fill, const Material* material_override) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Material material = material_override ? *material_override : GetMaterial();
    const Color color = material.diffuse;
    const float specular_strength = std::clamp(material.specular <= 0.0f ? 0.18f : material.specular, 0.0f, 1.0f);
    const float shininess = std::clamp(material.shininess <= 0.0f ? 36.0f : material.shininess, 4.0f, 96.0f);
    const float alpha = selected ? std::min(material.alpha + 0.04f, 1.0f) : material.alpha;
    const GLuint color_texture = texture_id_for_path(material.color_texture_path);
    const bool has_texture = color_texture != 0;

    std::vector<Vec3> vertex_normals(vertices_.size(), {0.0f, 0.0f, 0.0f});
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }
        const Vec3 normal = FaceNormal(face);
        for (size_t index : face) {
            vertex_normals[index] = vertex_normals[index] + normal;
        }
    }
    for (Vec3& normal : vertex_normals) {
        normal = normalize(normal);
        if (std::fabs(normal.x) <= 0.00001f && std::fabs(normal.y) <= 0.00001f && std::fabs(normal.z) <= 0.00001f) {
            normal = {0.0f, 1.0f, 0.0f};
        }
    }

    if (alpha < 0.999f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    glDisable(GL_LIGHTING);
    if (has_texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    if (offset_fill) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.55f, 0.55f);
    }

    glBegin(GL_TRIANGLES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }
        const Vec3 face_normal = FaceNormal(face);

        for (size_t i = 1; i + 1 < face.size(); ++i) {
            const size_t indices[] = {face[0], face[i], face[i + 1]};
            for (size_t vertex_index : indices) {
                const Vec3& normal = vertex_normals[vertex_index];
                const Color shade = (selected && !has_texture) ? normal_rgb_color(normal) : shaded_color(color, normal, specular_strength, shininess, selected);
                const Vec3& vertex = vertices_[vertex_index];
                glNormal3f(normal.x, normal.y, normal.z);
                glColor4f(shade.r, shade.g, shade.b, alpha);
                if (has_texture) {
                    const UV uv = projected_uv_for_face(vertex, face_normal);
                    glTexCoord2f(uv.u, uv.v);
                }
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
        }
    }
    glEnd();

    if (has_texture) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
    if (offset_fill) {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (alpha < 0.999f) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void CMesh3D::RenderWire(bool selected, bool draw_on_top, const Color* color_override) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Color base_color = color_override ? *color_override : GetColor();
    const MeshDisplayMode mode = GetDisplayMode();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(draw_on_top ? (selected ? 1.16f : 1.02f) : (selected ? 1.02f : 0.92f));
    const float opacity = std::clamp(s_WireOpacity, 0.05f, 1.0f);
    const Color wire = wire_color(base_color, mode, selected);
    const float alpha = opacity * (draw_on_top ? (selected ? 0.98f : 0.92f) : (selected ? 0.88f : 0.82f));
    glColor4f(wire.r, wire.g, wire.b, alpha);

    std::unordered_set<unsigned long long> drawn_edges;
    drawn_edges.reserve(faces_.size() * 3);

    glBegin(GL_LINES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        for (size_t i = 0; i < face.size(); ++i) {
            const size_t a_index = face[i];
            const size_t b_index = face[(i + 1) % face.size()];
            const size_t edge_min = std::min(a_index, b_index);
            const size_t edge_max = std::max(a_index, b_index);
            const unsigned long long key = (static_cast<unsigned long long>(edge_min) << 32)
                | static_cast<unsigned long long>(edge_max);
            if (!drawn_edges.insert(key).second) {
                continue;
            }

            const Vec3& a = vertices_[a_index];
            const Vec3& b_vertex = vertices_[b_index];
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b_vertex.x, b_vertex.y, b_vertex.z);
        }
    }
    glEnd();

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
}

float CMesh3D::GetWireOpacity() {
    return s_WireOpacity;
}

void CMesh3D::SetWireOpacity(float opacity) {
    s_WireOpacity = std::clamp(opacity, 0.05f, 1.0f);
}

MeshDisplayMode CMesh3D::GetDisplayMode() {
    return s_DisplayMode;
}

void CMesh3D::SetDisplayMode(MeshDisplayMode mode) {
    s_DisplayMode = mode;
}

void CMesh3D::Render2d(float center_x, float center_y, float scale) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Color color = GetColor();
    glLineWidth(1.5f);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        for (size_t i = 0; i < face.size(); ++i) {
            const Vec3& a = vertices_[face[i]];
            const Vec3& b = vertices_[face[(i + 1) % face.size()]];
            glVertex2f(center_x + a.x * scale, center_y + a.z * scale);
            glVertex2f(center_x + b.x * scale, center_y + b.z * scale);
        }
    }
    glEnd();
}

bool CMesh3D::HitTest(CurvePoint point, float tolerance) const {
    bool has_projected_face_hit = false;
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        if (PointInFace2d(point, face)) {
            has_projected_face_hit = true;
        }

        for (size_t i = 0; i < face.size(); ++i) {
            const Vec3& a = vertices_[face[i]];
            const Vec3& b = vertices_[face[(i + 1) % face.size()]];
            if (DistanceToSegment2d(point, a, b) <= tolerance) {
                return true;
            }
        }
    }

    if (has_projected_face_hit) {
        return true;
    }

    Vec3 min_point{};
    Vec3 max_point{};
    if (!GetBounds(min_point, max_point)) {
        return false;
    }

    return point.x >= min_point.x - tolerance
        && point.x <= max_point.x + tolerance
        && point.z >= min_point.z - tolerance
        && point.z <= max_point.z + tolerance;
}

namespace {
float edge_function(DomPoint a, DomPoint b, DomPoint c)
{
    return static_cast<float>((c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x));
}

bool point_in_screen_triangle(DomPoint point, DomPoint a, DomPoint b, DomPoint c)
{
    const float area = edge_function(a, b, c);
    if (std::abs(area) <= 0.0001f) {
        return false;
    }

    const float w0 = edge_function(b, c, point);
    const float w1 = edge_function(c, a, point);
    const float w2 = edge_function(a, b, point);
    if (area < 0.0f) {
        return w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f;
    }
    return w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f;
}
}

bool CMesh3D::HitTestMeshScreen(DomPoint point,
                                const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                float& depth) const
{
    bool hit = false;
    float best_depth = std::numeric_limits<float>::max();

    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size()) || face.size() < 3) {
            continue;
        }

        std::vector<DomPoint> projected;
        std::vector<float> depths;
        projected.reserve(face.size());
        depths.reserve(face.size());
        bool face_visible = true;
        for (size_t index : face) {
            DomPoint screen{};
            float vertex_depth = 0.0f;
            if (!project_world(vertices_[index], screen, vertex_depth)) {
                face_visible = false;
                break;
            }
            projected.push_back(screen);
            depths.push_back(vertex_depth);
        }

        if (!face_visible) {
            continue;
        }

        for (size_t i = 1; i + 1 < projected.size(); ++i) {
            if (!point_in_screen_triangle(point, projected[0], projected[i], projected[i + 1])) {
                continue;
            }

            const float triangle_depth = (depths[0] + depths[i] + depths[i + 1]) / 3.0f;
            if (triangle_depth < best_depth) {
                best_depth = triangle_depth;
                hit = true;
            }
        }
    }

    if (!hit) {
        return false;
    }

    depth = best_depth;
    return true;
}

std::unique_ptr<CAlfaObject> CMesh3D::Clone() const {
    auto copy = std::make_unique<CMesh3D>(GetName() + " Copy");
    copy->vertices_ = vertices_;
    copy->uvs_ = uvs_;
    copy->faces_ = faces_;
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    return copy;
}

void CMesh3D::Translate(Vec3 delta) {
    for (Vec3& vertex : vertices_) {
        vertex = vertex + delta;
    }
}

void CMesh3D::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (Vec3& vertex : vertices_) {
        vertex = rotate_around_axis(vertex - center, axis, angle) + center;
    }
}

void CMesh3D::Scale(Vec3 center, Vec3 axis, float factor) {
    for (Vec3& vertex : vertices_) {
        const Vec3 local = vertex - center;
        if (dot(axis, axis) <= 0.000001f) {
            vertex = scale_uniform(local, factor) + center;
        } else {
            vertex = scale_along_axis(local, axis, factor) + center;
        }
    }
}

bool CMesh3D::GetBounds(Vec3& min_point, Vec3& max_point) const {
    if (vertices_.empty()) {
        return false;
    }

    min_point = vertices_[0];
    max_point = vertices_[0];
    for (const Vec3& vertex : vertices_) {
        min_point.x = std::min(min_point.x, vertex.x);
        min_point.y = std::min(min_point.y, vertex.y);
        min_point.z = std::min(min_point.z, vertex.z);
        max_point.x = std::max(max_point.x, vertex.x);
        max_point.y = std::max(max_point.y, vertex.y);
        max_point.z = std::max(max_point.z, vertex.z);
    }

    return true;
}

bool CMesh3D::Save(std::ostream& stream) const {
    const Material material = GetMaterial();
    stream << "Mesh3D \"" << GetName() << "\" "
           << material.diffuse.r << " " << material.diffuse.g << " " << material.diffuse.b << " "
           << material.alpha << " " << material.specular << " " << material.shininess << " "
           << vertices_.size() << " " << faces_.size() << "\n";
    for (const Vec3& vertex : vertices_) {
        stream << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
    }
    for (const Face& face : faces_) {
        stream << face.size();
        for (size_t index : face) {
            stream << " " << index;
        }
        stream << "\n";
    }

    return static_cast<bool>(stream);
}

bool CMesh3D::Load(std::istream& stream) {
    std::string keyword;
    stream >> keyword;
    if (keyword != "Mesh3D") {
        return false;
    }

    stream >> std::ws;
    if (stream.peek() != '"') {
        return false;
    }

    stream.get();
    std::string name;
    std::getline(stream, name, '"');
    SetName(name);

    size_t vertex_count = 0;
    size_t face_count = 0;
    Material material{};
    stream >> material.diffuse.r >> material.diffuse.g >> material.diffuse.b;
    stream >> std::ws;
    std::string header_line;
    std::getline(stream, header_line);
    std::istringstream header_stream(header_line);
    std::vector<float> values;
    float value = 0.0f;
    while (header_stream >> value) {
        values.push_back(value);
    }
    if (values.size() == 2) {
        vertex_count = static_cast<size_t>(values[0]);
        face_count = static_cast<size_t>(values[1]);
    } else if (values.size() == 5) {
        material.alpha = values[0];
        material.specular = values[1];
        material.shininess = values[2];
        vertex_count = static_cast<size_t>(values[3]);
        face_count = static_cast<size_t>(values[4]);
    } else {
        return false;
    }
    if (!stream) {
        return false;
    }
    SetMaterial(material);

    std::vector<Vec3> loaded_vertices;
    loaded_vertices.reserve(vertex_count);
    for (size_t i = 0; i < vertex_count; ++i) {
        Vec3 vertex{};
        stream >> vertex.x >> vertex.y >> vertex.z;
        if (!stream) {
            return false;
        }
        loaded_vertices.push_back(vertex);
    }

    stream >> std::ws;

    std::vector<Face> loaded_faces;
    loaded_faces.reserve(face_count);
    std::string line;
    for (size_t i = 0; i < face_count; ++i) {
        if (!std::getline(stream, line)) {
            return false;
        }

        std::istringstream line_stream(line);
        std::vector<size_t> face_values;
        size_t face_value = 0;
        while (line_stream >> face_value) {
            face_values.push_back(face_value);
        }

        if (face_values.empty()) {
            return false;
        }

        Face face;
        if (face_values.size() == face_values[0] + 1 && face_values[0] > 2) {
            face.assign(face_values.begin() + 1, face_values.end());
        } else if (face_values.size() == 3) {
            face = std::move(face_values);
        } else {
            return false;
        }

        if (!IsValidFace(face, loaded_vertices.size())) {
            return false;
        }
        loaded_faces.push_back(std::move(face));
    }

    return SetGeometry(std::move(loaded_vertices), std::move(loaded_faces));
}

bool CMesh3D::Create(CPolyline* pline, CVector3d dir, float dist) {
    if (!pline || pline->GetPointCount() < 2 || dist <= 0.0f) {
        Clear();
        return false;
    }

    const Vec3 offset = normalize(dir) * dist;
    if (std::fabs(offset.x) <= 0.00001f && std::fabs(offset.y) <= 0.00001f && std::fabs(offset.z) <= 0.00001f) {
        Clear();
        return false;
    }

    const auto& points = pline->GetPoints();
    std::vector<Vec3> created_vertices;
    created_vertices.reserve(points.size() * 2);
    for (const CPoint3d& point : points) {
        created_vertices.push_back({static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)});
    }
    for (const CPoint3d& point : points) {
        created_vertices.push_back({
            static_cast<float>(point.x) + offset.x,
            static_cast<float>(point.y) + offset.y,
            static_cast<float>(point.z) + offset.z
        });
    }

    std::vector<Face> created_faces;
    created_faces.reserve(points.size() - 1 + (pline->IsClosed() ? 1 : 0));
    const size_t upper_offset = points.size();
    for (size_t i = 1; i < points.size(); ++i) {
        const size_t a = i - 1;
        const size_t b = i;
        const size_t c = upper_offset + i;
        const size_t d = upper_offset + i - 1;
        created_faces.push_back({a, b, c, d});
    }
    if (pline->IsClosed()) {
        created_faces.push_back({points.size() - 1, 0, upper_offset, upper_offset + points.size() - 1});
    }

    SetName(pline->GetName() + " Mesh");
    SetMaterial(colored_mesh_material());
    vertices_ = std::move(created_vertices);
    faces_ = std::move(created_faces);
    GeneratePlanarUVs();
    return true;
}

void CMesh3D::Clear() {
    vertices_.clear();
    uvs_.clear();
    faces_.clear();
}

bool CMesh3D::IsValidFace(const Face& face, size_t vertex_count) const {
    if (face.size() < 3) {
        return false;
    }

    for (size_t index : face) {
        if (index >= vertex_count) {
            return false;
        }
    }

    return true;
}

Vec3 CMesh3D::FaceNormal(const Face& face) const {
    if (!IsValidFace(face, vertices_.size())) {
        return {0.0f, 1.0f, 0.0f};
    }

    const Vec3& origin = vertices_[face[0]];
    for (size_t i = 1; i + 1 < face.size(); ++i) {
        const Vec3 edge_a = vertices_[face[i]] - origin;
        const Vec3 edge_b = vertices_[face[i + 1]] - origin;
        const Vec3 normal = normalize(cross(edge_a, edge_b));
        if (std::fabs(normal.x) > 0.00001f || std::fabs(normal.y) > 0.00001f || std::fabs(normal.z) > 0.00001f) {
            return normal;
        }
    }

    return {0.0f, 1.0f, 0.0f};
}

bool CMesh3D::PointInFace2d(CurvePoint point, const Face& face) const {
    if (!IsValidFace(face, vertices_.size())) {
        return false;
    }

    bool inside = false;
    bool has_area = false;
    for (size_t i = 0, j = face.size() - 1; i < face.size(); j = i++) {
        const Vec3& a = vertices_[face[i]];
        const Vec3& b = vertices_[face[j]];
        if (std::fabs((a.x - b.x) * (a.z + b.z)) > 0.00001f) {
            has_area = true;
        }

        const bool crosses = ((a.z > point.z) != (b.z > point.z))
            && (point.x < (b.x - a.x) * (point.z - a.z) / (b.z - a.z) + a.x);
        if (crosses) {
            inside = !inside;
        }
    }

    return has_area && inside;
}

float CMesh3D::DistanceToSegment2d(CurvePoint point, Vec3 start, Vec3 end) const {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float length_sq = dx * dx + dz * dz;
    if (length_sq <= 0.00001f) {
        const float px = point.x - start.x;
        const float pz = point.z - start.z;
        return std::sqrt(px * px + pz * pz);
    }

    const float t = std::clamp(((point.x - start.x) * dx + (point.z - start.z) * dz) / length_sq, 0.0f, 1.0f);
    const float closest_x = start.x + t * dx;
    const float closest_z = start.z + t * dz;
    const float px = point.x - closest_x;
    const float pz = point.z - closest_z;
    return std::sqrt(px * px + pz * pz);
}
