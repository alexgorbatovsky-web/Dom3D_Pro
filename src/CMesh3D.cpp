#include "CMesh3D.h"

#include "OpenGLCompat.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <sstream>
#include <utility>

CMesh3D::CMesh3D()
    : CAlfaObject("Mesh3D") {
    SetColor({0.38f, 0.68f, 0.88f});
}

CMesh3D::CMesh3D(std::string name)
    : CAlfaObject(std::move(name)) {
    SetColor({0.38f, 0.68f, 0.88f});
}

const std::vector<Vec3>& CMesh3D::GetVertices() const {
    return vertices_;
}

const std::vector<CMesh3D::Face>& CMesh3D::GetFaces() const {
    return faces_;
}

bool CMesh3D::SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces) {
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
    return true;
}

void CMesh3D::Render() {
    Render3d(false);
}

void CMesh3D::Render3d(bool selected) const {
    if (vertices_.empty() || faces_.empty()) {
        return;
    }

    const Color color = GetColor();
    const Material material = GetMaterial();
    const float r = selected ? 0.35f : color.r;
    const float g = selected ? 0.86f : color.g;
    const float b = selected ? 1.0f : color.b;
    const float alpha = selected ? std::min(material.alpha + 0.12f, 1.0f) : material.alpha;

    const Vec3 light_dir = normalize({-0.35f, 0.85f, 0.38f});

    if (alpha < 0.999f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDisable(GL_LIGHTING);

    glBegin(GL_TRIANGLES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        const Vec3 normal = FaceNormal(face);
        const float lambert = std::fabs(dot(normal, light_dir));
        const float shade = selected ? 1.0f : (0.34f + lambert * 0.78f);
        glNormal3f(normal.x, normal.y, normal.z);
        glColor4f(std::min(r * shade, 1.0f), std::min(g * shade, 1.0f), std::min(b * shade, 1.0f), alpha);
        for (size_t i = 1; i + 1 < face.size(); ++i) {
            const Vec3& a = vertices_[face[0]];
            const Vec3& b_vertex = vertices_[face[i]];
            const Vec3& c = vertices_[face[i + 1]];
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b_vertex.x, b_vertex.y, b_vertex.z);
            glVertex3f(c.x, c.y, c.z);
        }
    }
    glEnd();

    if (alpha < 0.999f) {
        glDisable(GL_BLEND);
    }

    glLineWidth(selected ? 3.0f : 1.5f);
    glColor3f(r * 0.72f, g * 0.72f, b * 0.72f);
    glBegin(GL_LINES);
    for (const Face& face : faces_) {
        if (!IsValidFace(face, vertices_.size())) {
            continue;
        }

        for (size_t i = 0; i < face.size(); ++i) {
            const Vec3& a = vertices_[face[i]];
            const Vec3& b_vertex = vertices_[face[(i + 1) % face.size()]];
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b_vertex.x, b_vertex.y, b_vertex.z);
        }
    }
    glEnd();
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
        vertex = scale_along_axis(vertex - center, axis, factor) + center;
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

    vertices_ = std::move(loaded_vertices);
    faces_ = std::move(loaded_faces);
    return true;
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
    for (const CurvePoint& point : points) {
        created_vertices.push_back({point.x, 0.0f, point.z});
    }
    for (const CurvePoint& point : points) {
        created_vertices.push_back({point.x + offset.x, offset.y, point.z + offset.z});
    }

    std::vector<Face> created_faces;
    created_faces.reserve(points.size() - 1);
    const size_t upper_offset = points.size();
    for (size_t i = 1; i < points.size(); ++i) {
        const size_t a = i - 1;
        const size_t b = i;
        const size_t c = upper_offset + i;
        const size_t d = upper_offset + i - 1;
        created_faces.push_back({a, b, c, d});
    }

    SetName(pline->GetName() + " Mesh");
    SetColor(pline->GetColor());
    vertices_ = std::move(created_vertices);
    faces_ = std::move(created_faces);
    return true;
}

void CMesh3D::Clear() {
    vertices_.clear();
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
