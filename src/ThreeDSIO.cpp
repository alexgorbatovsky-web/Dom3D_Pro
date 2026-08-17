#include "ThreeDSIO.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <map>
#include <tuple>
#include <utility>

namespace {
constexpr std::uint16_t kMain3ds = 0x4d4d;
constexpr std::uint16_t kEdit3ds = 0x3d3d;
constexpr std::uint16_t kMasterScale = 0x0100;
constexpr std::uint16_t kMaterialBlock = 0xafff;
constexpr std::uint16_t kMaterialName = 0xa000;
constexpr std::uint16_t kMaterialAmbient = 0xa010;
constexpr std::uint16_t kMaterialDiffuse = 0xa020;
constexpr std::uint16_t kMaterialSpecular = 0xa030;
constexpr std::uint16_t kMaterialShininess = 0xa040;
constexpr std::uint16_t kMaterialTransparency = 0xa050;
constexpr std::uint16_t kTextureMap = 0xa200;
constexpr std::uint16_t kTextureFile = 0xa300;
constexpr std::uint16_t kTextureUScale = 0xa354;
constexpr std::uint16_t kTextureVScale = 0xa356;
constexpr std::uint16_t kTextureUOffset = 0xa358;
constexpr std::uint16_t kTextureVOffset = 0xa35a;
constexpr std::uint16_t kTextureRotation = 0xa35c;
constexpr std::uint16_t kColorFloat = 0x0010;
constexpr std::uint16_t kColorByte = 0x0011;
constexpr std::uint16_t kColorByteGamma = 0x0012;
constexpr std::uint16_t kColorFloatGamma = 0x0013;
constexpr std::uint16_t kPercentInt = 0x0030;
constexpr std::uint16_t kPercentFloat = 0x0031;
constexpr std::uint16_t kObjectBlock = 0x4000;
constexpr std::uint16_t kTriangularMesh = 0x4100;
constexpr std::uint16_t kVerticesList = 0x4110;
constexpr std::uint16_t kFacesDescription = 0x4120;
constexpr std::uint16_t kFacesMaterial = 0x4130;
constexpr std::uint16_t kMappingCoordinates = 0x4140;
constexpr std::uint16_t kSmoothingGroups = 0x4150;
constexpr std::uint16_t kLocalCoordinates = 0x4160;
constexpr std::uint16_t kKeyframer = 0xb000;
constexpr std::uint16_t kObjectNode = 0xb002;
constexpr std::uint16_t kNodeHeader = 0xb010;
constexpr std::uint16_t kInstanceName = 0xb011;
constexpr std::uint16_t kPivot = 0xb013;
constexpr std::uint16_t kPositionTrack = 0xb020;
constexpr std::uint16_t kRotationTrack = 0xb021;
constexpr std::uint16_t kScaleTrack = 0xb022;
constexpr std::uint16_t kNodeId = 0xb030;

struct Chunk {
    std::uint16_t id = 0;
    size_t end = 0;
};

class Reader {
public:
    explicit Reader(std::vector<unsigned char> data)
        : data_(std::move(data)) {
    }

    size_t Position() const {
        return position_;
    }

    size_t Size() const {
        return data_.size();
    }

    bool Seek(size_t position) {
        if (position > data_.size()) {
            return false;
        }
        position_ = position;
        return true;
    }

    bool ReadU16(std::uint16_t& value) {
        if (position_ + 2 > data_.size()) {
            return false;
        }
        value = static_cast<std::uint16_t>(data_[position_])
            | (static_cast<std::uint16_t>(data_[position_ + 1]) << 8);
        position_ += 2;
        return true;
    }

    bool ReadU8(unsigned char& value) {
        if (position_ >= data_.size()) {
            return false;
        }
        value = data_[position_++];
        return true;
    }

    bool ReadI16(std::int16_t& value) {
        std::uint16_t bits = 0;
        if (!ReadU16(bits)) {
            return false;
        }
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool ReadU32(std::uint32_t& value) {
        if (position_ + 4 > data_.size()) {
            return false;
        }
        value = static_cast<std::uint32_t>(data_[position_])
            | (static_cast<std::uint32_t>(data_[position_ + 1]) << 8)
            | (static_cast<std::uint32_t>(data_[position_ + 2]) << 16)
            | (static_cast<std::uint32_t>(data_[position_ + 3]) << 24);
        position_ += 4;
        return true;
    }

    bool ReadFloat(float& value) {
        std::uint32_t bits = 0;
        if (!ReadU32(bits)) {
            return false;
        }
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool ReadString(size_t end, std::string& value) {
        value.clear();
        while (position_ < end && position_ < data_.size()) {
            const unsigned char ch = data_[position_++];
            if (ch == 0) {
                return true;
            }
            value.push_back(static_cast<char>(ch));
        }
        return false;
    }

    bool Skip(size_t count) {
        return Seek(position_ + count);
    }

    bool ReadChunk(size_t parent_end, Chunk& chunk) {
        const size_t start = position_;
        std::uint32_t length = 0;
        if (!ReadU16(chunk.id) || !ReadU32(length) || length < 6) {
            return false;
        }
        chunk.end = start + static_cast<size_t>(length);
        return chunk.end <= parent_end && chunk.end <= data_.size();
    }

private:
    std::vector<unsigned char> data_;
    size_t position_ = 0;
};

struct FaceData {
    std::array<std::uint16_t, 3> indices{};
    std::uint32_t smoothing_group = 0;
};

struct MaterialFaceGroup {
    std::string name;
    std::vector<size_t> face_indices;
};

struct MeshData {
    std::string name;
    std::vector<Vec3> vertices;
    std::vector<UV> uvs;
    std::vector<FaceData> faces;
    std::vector<MaterialFaceGroup> material_groups;
    std::array<Vec3, 3> axes{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    Vec3 origin{};
    bool has_local_coordinates = false;
};

struct Matrix4 {
    float m[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    };
};

struct NodeData {
    std::string name;
    std::string instance_name;
    std::int16_t parent_id = -1;
    std::uint16_t node_id = 0xffff;
    Vec3 pivot{};
    Vec3 position{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Vec3 rotation_axis{0.0f, 1.0f, 0.0f};
    float rotation_angle = 0.0f;
    bool has_position = false;
    bool has_scale = false;
    bool has_rotation = false;
};

struct SceneData {
    float master_scale = 1.0f;
    std::vector<MeshData> meshes;
    std::vector<NodeData> nodes;
    std::map<std::string, Material> materials;
};

struct MeshTransform {
    Matrix4 matrix;
    std::string instance_name;
};

Matrix4 multiply(const Matrix4& a, const Matrix4& b) {
    Matrix4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            result.m[row][column] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                result.m[row][column] += a.m[row][k] * b.m[k][column];
            }
        }
    }
    return result;
}

Vec3 transform_point(const Matrix4& matrix, Vec3 point) {
    return {
        matrix.m[0][0] * point.x + matrix.m[0][1] * point.y + matrix.m[0][2] * point.z + matrix.m[0][3],
        matrix.m[1][0] * point.x + matrix.m[1][1] * point.y + matrix.m[1][2] * point.z + matrix.m[1][3],
        matrix.m[2][0] * point.x + matrix.m[2][1] * point.y + matrix.m[2][2] * point.z + matrix.m[2][3]
    };
}

Matrix4 translation_matrix(Vec3 value) {
    Matrix4 result;
    result.m[0][3] = value.x;
    result.m[1][3] = value.y;
    result.m[2][3] = value.z;
    return result;
}

Matrix4 scale_matrix(Vec3 value) {
    Matrix4 result;
    result.m[0][0] = value.x;
    result.m[1][1] = value.y;
    result.m[2][2] = value.z;
    return result;
}

Matrix4 rotation_matrix(Vec3 axis, float angle) {
    axis = normalize(axis);
    if (dot(axis, axis) <= 0.0000001f) {
        return {};
    }

    // 3DS stores positive rotations clockwise relative to its coordinate convention.
    const float c = std::cos(-angle);
    const float s = std::sin(-angle);
    const float t = 1.0f - c;
    Matrix4 result;
    result.m[0][0] = t * axis.x * axis.x + c;
    result.m[0][1] = t * axis.x * axis.y - s * axis.z;
    result.m[0][2] = t * axis.x * axis.z + s * axis.y;
    result.m[1][0] = t * axis.x * axis.y + s * axis.z;
    result.m[1][1] = t * axis.y * axis.y + c;
    result.m[1][2] = t * axis.y * axis.z - s * axis.x;
    result.m[2][0] = t * axis.x * axis.z - s * axis.y;
    result.m[2][1] = t * axis.y * axis.z + s * axis.x;
    result.m[2][2] = t * axis.z * axis.z + c;
    return result;
}

Matrix4 local_mesh_matrix(const MeshData& mesh) {
    Matrix4 result;
    result.m[0][0] = mesh.axes[0].x;
    result.m[1][0] = mesh.axes[0].y;
    result.m[2][0] = mesh.axes[0].z;
    result.m[0][1] = mesh.axes[1].x;
    result.m[1][1] = mesh.axes[1].y;
    result.m[2][1] = mesh.axes[1].z;
    result.m[0][2] = mesh.axes[2].x;
    result.m[1][2] = mesh.axes[2].y;
    result.m[2][2] = mesh.axes[2].z;
    result.m[0][3] = mesh.origin.x;
    result.m[1][3] = mesh.origin.y;
    result.m[2][3] = mesh.origin.z;
    return result;
}

float linear_determinant(const Matrix4& matrix) {
    const float a = matrix.m[0][0], b = matrix.m[0][1], c = matrix.m[0][2];
    const float d = matrix.m[1][0], e = matrix.m[1][1], f = matrix.m[1][2];
    const float g = matrix.m[2][0], h = matrix.m[2][1], i = matrix.m[2][2];
    return a * (e * i - f * h)
        - b * (d * i - f * g)
        + c * (d * h - e * g);
}

bool invert_affine(const Matrix4& matrix, Matrix4& result) {
    const float a = matrix.m[0][0], b = matrix.m[0][1], c = matrix.m[0][2];
    const float d = matrix.m[1][0], e = matrix.m[1][1], f = matrix.m[1][2];
    const float g = matrix.m[2][0], h = matrix.m[2][1], i = matrix.m[2][2];
    const float determinant = a * (e * i - f * h)
        - b * (d * i - f * g)
        + c * (d * h - e * g);
    if (std::fabs(determinant) <= 0.0000001f) {
        return false;
    }

    const float inverse_det = 1.0f / determinant;
    result = {};
    result.m[0][0] = (e * i - f * h) * inverse_det;
    result.m[0][1] = (c * h - b * i) * inverse_det;
    result.m[0][2] = (b * f - c * e) * inverse_det;
    result.m[1][0] = (f * g - d * i) * inverse_det;
    result.m[1][1] = (a * i - c * g) * inverse_det;
    result.m[1][2] = (c * d - a * f) * inverse_det;
    result.m[2][0] = (d * h - e * g) * inverse_det;
    result.m[2][1] = (b * g - a * h) * inverse_det;
    result.m[2][2] = (a * e - b * d) * inverse_det;

    const Vec3 translation{matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]};
    const Vec3 inverse_translation = transform_point(result, translation) * -1.0f;
    result.m[0][3] = inverse_translation.x;
    result.m[1][3] = inverse_translation.y;
    result.m[2][3] = inverse_translation.z;
    return true;
}

bool skip_tcb_data(Reader& reader) {
    std::uint16_t flags = 0;
    if (!reader.ReadU16(flags)) {
        return false;
    }
    size_t extra_float_count = 0;
    for (std::uint16_t bit : {std::uint16_t{0x01}, std::uint16_t{0x02}, std::uint16_t{0x04},
                              std::uint16_t{0x08}, std::uint16_t{0x10}}) {
        if ((flags & bit) != 0) {
            ++extra_float_count;
        }
    }
    return reader.Skip(extra_float_count * sizeof(float));
}

bool read_track_header(Reader& reader, std::uint32_t& key_count) {
    return reader.Skip(10) && reader.ReadU32(key_count);
}

bool read_first_vector_key(Reader& reader, size_t end, Vec3& value, bool& has_value) {
    std::uint32_t key_count = 0;
    if (!read_track_header(reader, key_count)) {
        return false;
    }
    for (std::uint32_t index = 0; index < key_count; ++index) {
        std::uint32_t frame = 0;
        Vec3 key{};
        if (!reader.ReadU32(frame) || !skip_tcb_data(reader)
            || !reader.ReadFloat(key.x) || !reader.ReadFloat(key.y) || !reader.ReadFloat(key.z)) {
            return false;
        }
        if (!has_value) {
            value = key;
            has_value = true;
        }
    }
    return reader.Position() <= end;
}

bool read_first_rotation_key(Reader& reader, size_t end, NodeData& node) {
    std::uint32_t key_count = 0;
    if (!read_track_header(reader, key_count)) {
        return false;
    }
    for (std::uint32_t index = 0; index < key_count; ++index) {
        std::uint32_t frame = 0;
        float angle = 0.0f;
        Vec3 axis{};
        if (!reader.ReadU32(frame) || !skip_tcb_data(reader) || !reader.ReadFloat(angle)
            || !reader.ReadFloat(axis.x) || !reader.ReadFloat(axis.y) || !reader.ReadFloat(axis.z)) {
            return false;
        }
        if (!node.has_rotation) {
            node.rotation_angle = angle;
            node.rotation_axis = dot(axis, axis) <= 0.0000001f ? Vec3{0.0f, 1.0f, 0.0f} : axis;
            node.has_rotation = true;
        }
    }
    return reader.Position() <= end;
}

bool parse_object_node(Reader& reader, size_t end, SceneData& scene, std::string& error) {
    NodeData node;
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS keyframer object chunk is invalid.";
            return false;
        }

        if (chunk.id == kNodeHeader) {
            std::uint16_t flags1 = 0;
            std::uint16_t flags2 = 0;
            if (!reader.ReadString(chunk.end, node.name)
                || !reader.ReadU16(flags1) || !reader.ReadU16(flags2)
                || !reader.ReadI16(node.parent_id)) {
                error = "3DS keyframer node header is truncated.";
                return false;
            }
        } else if (chunk.id == kInstanceName) {
            if (!reader.ReadString(chunk.end, node.instance_name)) {
                error = "3DS instance name is truncated.";
                return false;
            }
        } else if (chunk.id == kNodeId) {
            if (!reader.ReadU16(node.node_id)) {
                error = "3DS keyframer node id is truncated.";
                return false;
            }
        } else if (chunk.id == kPivot) {
            if (!reader.ReadFloat(node.pivot.x)
                || !reader.ReadFloat(node.pivot.y)
                || !reader.ReadFloat(node.pivot.z)) {
                error = "3DS keyframer pivot is truncated.";
                return false;
            }
        } else if (chunk.id == kPositionTrack) {
            if (!read_first_vector_key(reader, chunk.end, node.position, node.has_position)) {
                error = "3DS position track is truncated.";
                return false;
            }
        } else if (chunk.id == kRotationTrack) {
            if (!read_first_rotation_key(reader, chunk.end, node)) {
                error = "3DS rotation track is truncated.";
                return false;
            }
        } else if (chunk.id == kScaleTrack) {
            if (!read_first_vector_key(reader, chunk.end, node.scale, node.has_scale)) {
                error = "3DS scale track is truncated.";
                return false;
            }
            if (std::fabs(node.scale.x) <= 0.000001f) node.scale.x = 1.0f;
            if (std::fabs(node.scale.y) <= 0.000001f) node.scale.y = 1.0f;
            if (std::fabs(node.scale.z) <= 0.000001f) node.scale.z = 1.0f;
        }

        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    if (!node.name.empty()) {
        scene.nodes.push_back(std::move(node));
    }
    return reader.Position() == end;
}

bool parse_keyframer(Reader& reader, size_t end, SceneData& scene, std::string& error) {
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS keyframer chunk is invalid.";
            return false;
        }
        if (chunk.id == kObjectNode && !parse_object_node(reader, chunk.end, scene, error)) {
            return false;
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_faces(Reader& reader, size_t end, MeshData& mesh, std::string& error) {
    std::uint16_t count = 0;
    if (!reader.ReadU16(count)) {
        error = "3DS face list is truncated.";
        return false;
    }

    mesh.faces.resize(count);
    for (FaceData& face : mesh.faces) {
        std::uint16_t flags = 0;
        if (!reader.ReadU16(face.indices[0])
            || !reader.ReadU16(face.indices[1])
            || !reader.ReadU16(face.indices[2])
            || !reader.ReadU16(flags)) {
            error = "3DS face list is truncated.";
            return false;
        }
    }

    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS face subchunk is invalid.";
            return false;
        }
        if (chunk.id == kSmoothingGroups) {
            for (FaceData& face : mesh.faces) {
                if (reader.Position() + 4 > chunk.end || !reader.ReadU32(face.smoothing_group)) {
                    error = "3DS smoothing group data is truncated.";
                    return false;
                }
            }
        } else if (chunk.id == kFacesMaterial) {
            MaterialFaceGroup group;
            std::uint16_t count = 0;
            if (!reader.ReadString(chunk.end, group.name) || !reader.ReadU16(count)) {
                error = "3DS face material assignment is truncated.";
                return false;
            }
            group.face_indices.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i) {
                std::uint16_t face_index = 0;
                if (!reader.ReadU16(face_index) || face_index >= mesh.faces.size()) {
                    error = "3DS face material assignment is invalid.";
                    return false;
                }
                group.face_indices.push_back(face_index);
            }
            if (!group.face_indices.empty()) {
                mesh.material_groups.push_back(std::move(group));
            }
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_triangular_mesh(Reader& reader, size_t end, MeshData& mesh, std::string& error) {
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS mesh chunk is invalid.";
            return false;
        }

        if (chunk.id == kVerticesList) {
            std::uint16_t count = 0;
            if (!reader.ReadU16(count)) {
                error = "3DS vertex list is truncated.";
                return false;
            }
            mesh.vertices.resize(count);
            for (Vec3& vertex : mesh.vertices) {
                if (!reader.ReadFloat(vertex.x) || !reader.ReadFloat(vertex.y) || !reader.ReadFloat(vertex.z)) {
                    error = "3DS vertex list is truncated.";
                    return false;
                }
            }
        } else if (chunk.id == kFacesDescription) {
            if (!parse_faces(reader, chunk.end, mesh, error)) {
                return false;
            }
        } else if (chunk.id == kMappingCoordinates) {
            std::uint16_t count = 0;
            if (!reader.ReadU16(count)) {
                error = "3DS texture coordinate list is truncated.";
                return false;
            }
            mesh.uvs.resize(count);
            for (UV& uv : mesh.uvs) {
                if (!reader.ReadFloat(uv.u) || !reader.ReadFloat(uv.v)) {
                    error = "3DS texture coordinate list is truncated.";
                    return false;
                }
            }
        } else if (chunk.id == kLocalCoordinates) {
            for (Vec3& axis : mesh.axes) {
                if (!reader.ReadFloat(axis.x) || !reader.ReadFloat(axis.y) || !reader.ReadFloat(axis.z)) {
                    error = "3DS local coordinate system is truncated.";
                    return false;
                }
            }
            if (!reader.ReadFloat(mesh.origin.x)
                || !reader.ReadFloat(mesh.origin.y)
                || !reader.ReadFloat(mesh.origin.z)) {
                error = "3DS local coordinate system is truncated.";
                return false;
            }
            mesh.has_local_coordinates = true;
        }

        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_object(Reader& reader, size_t end, SceneData& scene, std::string& error) {
    std::string name;
    if (!reader.ReadString(end, name)) {
        error = "3DS object name is truncated.";
        return false;
    }

    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS object chunk is invalid.";
            return false;
        }
        if (chunk.id == kTriangularMesh) {
            MeshData mesh;
            mesh.name = name;
            if (!parse_triangular_mesh(reader, chunk.end, mesh, error)) {
                return false;
            }
            scene.meshes.push_back(std::move(mesh));
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_color(Reader& reader, size_t end, Color& color, std::string& error) {
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS material color chunk is invalid.";
            return false;
        }
        if (chunk.id == kColorFloat || chunk.id == kColorFloatGamma) {
            if (!reader.ReadFloat(color.r) || !reader.ReadFloat(color.g) || !reader.ReadFloat(color.b)) {
                error = "3DS material color is truncated.";
                return false;
            }
        } else if (chunk.id == kColorByte || chunk.id == kColorByteGamma) {
            unsigned char red = 0;
            unsigned char green = 0;
            unsigned char blue = 0;
            if (!reader.ReadU8(red) || !reader.ReadU8(green) || !reader.ReadU8(blue)) {
                error = "3DS material color is truncated.";
                return false;
            }
            color = {
                static_cast<float>(red) / 255.0f,
                static_cast<float>(green) / 255.0f,
                static_cast<float>(blue) / 255.0f
            };
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_percent(Reader& reader, size_t end, float& value, std::string& error) {
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS material percentage chunk is invalid.";
            return false;
        }
        if (chunk.id == kPercentInt) {
            std::uint16_t percent = 0;
            if (!reader.ReadU16(percent)) {
                error = "3DS material percentage is truncated.";
                return false;
            }
            value = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);
        } else if (chunk.id == kPercentFloat) {
            if (!reader.ReadFloat(value)) {
                error = "3DS material percentage is truncated.";
                return false;
            }
            value = std::clamp(value, 0.0f, 1.0f);
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_texture_map(Reader& reader, size_t end, Material& material, std::string& error) {
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS texture map chunk is invalid.";
            return false;
        }
        if (chunk.id == kTextureFile) {
            if (!reader.ReadString(chunk.end, material.color_texture_path)) {
                error = "3DS texture file name is truncated.";
                return false;
            }
        } else if (chunk.id == kTextureUScale) {
            if (!reader.ReadFloat(material.texture_scale_u)) return false;
        } else if (chunk.id == kTextureVScale) {
            if (!reader.ReadFloat(material.texture_scale_v)) return false;
        } else if (chunk.id == kTextureUOffset) {
            if (!reader.ReadFloat(material.texture_offset_u)) return false;
        } else if (chunk.id == kTextureVOffset) {
            if (!reader.ReadFloat(material.texture_offset_v)) return false;
        } else if (chunk.id == kTextureRotation) {
            float radians = 0.0f;
            if (!reader.ReadFloat(radians)) return false;
            material.texture_rotation_degrees = radians * 180.0f / kPi;
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

bool parse_material(Reader& reader, size_t end, SceneData& scene, std::string& error) {
    Material material = Material::ImportedMesh();
    material.id = 0;
    material.name.clear();
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS material chunk is invalid.";
            return false;
        }
        if (chunk.id == kMaterialName) {
            if (!reader.ReadString(chunk.end, material.name)) {
                error = "3DS material name is truncated.";
                return false;
            }
        } else if (chunk.id == kMaterialAmbient) {
            if (!parse_color(reader, chunk.end, material.ambient, error)) return false;
        } else if (chunk.id == kMaterialDiffuse) {
            if (!parse_color(reader, chunk.end, material.diffuse, error)) return false;
        } else if (chunk.id == kMaterialSpecular) {
            Color specular{};
            if (!parse_color(reader, chunk.end, specular, error)) return false;
            material.specular = std::max({specular.r, specular.g, specular.b});
        } else if (chunk.id == kMaterialShininess) {
            float shininess = 0.0f;
            if (!parse_percent(reader, chunk.end, shininess, error)) return false;
            material.shininess = 1.0f + shininess * 127.0f;
        } else if (chunk.id == kMaterialTransparency) {
            float transparency = 0.0f;
            if (!parse_percent(reader, chunk.end, transparency, error)) return false;
            material.alpha = 1.0f - transparency;
        } else if (chunk.id == kTextureMap) {
            if (!parse_texture_map(reader, chunk.end, material, error)) return false;
        }
        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    if (!material.name.empty()) {
        scene.materials[material.name] = std::move(material);
    }
    return reader.Position() == end;
}

bool parse_container(Reader& reader, size_t end, SceneData& scene, std::string& error) {
    while (reader.Position() < end) {
        Chunk chunk;
        if (!reader.ReadChunk(end, chunk)) {
            error = "3DS chunk has an invalid length.";
            return false;
        }

        if (chunk.id == kMain3ds || chunk.id == kEdit3ds) {
            if (!parse_container(reader, chunk.end, scene, error)) {
                return false;
            }
        } else if (chunk.id == kKeyframer) {
            if (!parse_keyframer(reader, chunk.end, scene, error)) {
                return false;
            }
        } else if (chunk.id == kMasterScale) {
            if (!reader.ReadFloat(scene.master_scale) || scene.master_scale <= 0.0f) {
                error = "3DS master scale is invalid.";
                return false;
            }
        } else if (chunk.id == kObjectBlock) {
            if (!parse_object(reader, chunk.end, scene, error)) {
                return false;
            }
        } else if (chunk.id == kMaterialBlock) {
            if (!parse_material(reader, chunk.end, scene, error)) {
                return false;
            }
        }

        if (!reader.Seek(chunk.end)) {
            return false;
        }
    }
    return reader.Position() == end;
}

Vec3 to_dom_coordinates(Vec3 point, float scale) {
    // 3DS uses Z-up coordinates; Dom3D uses Y-up coordinates.
    return {point.x * scale, point.z * scale, -point.y * scale};
}

Matrix4 node_local_matrix(const NodeData& node) {
    const Matrix4 rotation = node.has_rotation
        ? rotation_matrix(node.rotation_axis, node.rotation_angle)
        : Matrix4{};
    const Matrix4 scaling = node.has_scale ? scale_matrix(node.scale) : Matrix4{};
    const Matrix4 translation = node.has_position ? translation_matrix(node.position) : Matrix4{};
    return multiply(translation, multiply(rotation, scaling));
}

Matrix4 node_global_matrix(size_t node_index,
                           const std::vector<NodeData>& nodes,
                           const std::map<std::uint16_t, size_t>& node_ids,
                           std::vector<Matrix4>& cache,
                           std::vector<unsigned char>& state) {
    if (state[node_index] == 2) {
        return cache[node_index];
    }
    if (state[node_index] == 1) {
        return node_local_matrix(nodes[node_index]);
    }

    state[node_index] = 1;
    Matrix4 result = node_local_matrix(nodes[node_index]);
    const std::int16_t parent_id = nodes[node_index].parent_id;
    if (parent_id >= 0) {
        const auto parent = node_ids.find(static_cast<std::uint16_t>(parent_id));
        if (parent != node_ids.end() && parent->second != node_index) {
            result = multiply(node_global_matrix(parent->second, nodes, node_ids, cache, state), result);
        }
    }
    cache[node_index] = result;
    state[node_index] = 2;
    return result;
}

std::map<std::string, std::vector<MeshTransform>> build_mesh_transforms(const SceneData& scene) {
    std::map<std::string, std::vector<MeshTransform>> transforms;
    std::map<std::uint16_t, size_t> node_ids;
    for (size_t index = 0; index < scene.nodes.size(); ++index) {
        if (scene.nodes[index].node_id != 0xffff) {
            node_ids[scene.nodes[index].node_id] = index;
        }
    }
    for (size_t index = 0; index < scene.nodes.size() && index <= 0xffff; ++index) {
        node_ids.emplace(static_cast<std::uint16_t>(index), index);
    }

    std::vector<Matrix4> cache(scene.nodes.size());
    std::vector<unsigned char> state(scene.nodes.size(), 0);
    for (size_t index = 0; index < scene.nodes.size(); ++index) {
        const NodeData& node = scene.nodes[index];
        if (node.name.empty() || node.name == "$$$DUMMY") {
            continue;
        }
        transforms[node.name].push_back({
            multiply(
                node_global_matrix(index, scene.nodes, node_ids, cache, state),
                translation_matrix(node.pivot * -1.0f)),
            node.instance_name
        });
    }
    for (auto it = transforms.begin(); it != transforms.end();) {
        const bool has_real_instances =
            it->second.size() > 1
            || std::any_of(it->second.begin(), it->second.end(), [](const MeshTransform& transform) {
                return !transform.instance_name.empty();
            });
        if (!has_real_instances) {
            // Ordinary 3DS objects already have their static placement baked into
            // the editor mesh. A keyframer track may contain animation keys that
            // should not move a static furniture import away from the scene.
            it = transforms.erase(it);
        } else {
            ++it;
        }
    }
    return transforms;
}

bool build_mesh(const MeshData& source,
                const Matrix4* keyframer_transform,
                const std::string& instance_name,
                const MaterialFaceGroup* material_group,
                const Material* material,
                float scale,
                int mesh_number,
                std::unique_ptr<CMesh3D>& result,
                std::string& error) {
    if (source.vertices.empty() || source.faces.empty()
        || (material_group && material_group->face_indices.empty())) {
        return false;
    }

    std::vector<size_t> selected_faces;
    if (material_group) {
        selected_faces = material_group->face_indices;
    } else {
        selected_faces.resize(source.faces.size());
        for (size_t i = 0; i < selected_faces.size(); ++i) {
            selected_faces[i] = i;
        }
    }

    std::vector<Vec3> transformed_vertices;
    transformed_vertices.reserve(source.vertices.size());
    Matrix4 object_transform;
    bool apply_object_transform = keyframer_transform != nullptr;
    if (apply_object_transform) {
        Matrix4 inverse_local;
        if (source.has_local_coordinates && !invert_affine(local_mesh_matrix(source), inverse_local)) {
            error = "3DS object has a singular local transformation matrix.";
            return false;
        }
        object_transform = source.has_local_coordinates
            ? multiply(*keyframer_transform, inverse_local)
            : *keyframer_transform;
    }
    const bool mirrored_transform = apply_object_transform && linear_determinant(object_transform) < 0.0f;
    for (Vec3 vertex : source.vertices) {
        if (apply_object_transform) {
            vertex = transform_point(object_transform, vertex);
        }
        transformed_vertices.push_back(to_dom_coordinates(vertex, scale));
    }

    std::vector<Vec3> face_normals(source.faces.size());
    std::vector<size_t> smoothing_vertex(source.vertices.size());
    std::map<std::tuple<long long, long long, long long>, size_t> coincident_vertices;
    constexpr double kSmoothingPositionScale = 100000.0;
    for (size_t vertex_index = 0; vertex_index < transformed_vertices.size(); ++vertex_index) {
        const Vec3& vertex = transformed_vertices[vertex_index];
        const auto key = std::make_tuple(
            std::llround(static_cast<double>(vertex.x) * kSmoothingPositionScale),
            std::llround(static_cast<double>(vertex.y) * kSmoothingPositionScale),
            std::llround(static_cast<double>(vertex.z) * kSmoothingPositionScale));
        const auto [it, inserted] = coincident_vertices.emplace(key, vertex_index);
        smoothing_vertex[vertex_index] = inserted ? vertex_index : it->second;
    }

    // Some 3DS exporters duplicate vertices at UV seams even though adjacent
    // faces belong to the same smoothing group. Build normal adjacency by
    // coincident position so those seams do not appear as dark stripes.
    std::vector<std::vector<size_t>> incident_faces(source.vertices.size());
    for (size_t face_index : selected_faces) {
        const FaceData& face = source.faces[face_index];
        for (std::uint16_t index : face.indices) {
            if (index >= transformed_vertices.size()) {
                error = "3DS face references a vertex outside the vertex list.";
                return false;
            }
            incident_faces[smoothing_vertex[index]].push_back(face_index);
        }
        const Vec3& a = transformed_vertices[face.indices[0]];
        const Vec3& b = transformed_vertices[face.indices[1]];
        const Vec3& c = transformed_vertices[face.indices[2]];
        face_normals[face_index] = normalize(cross(b - a, c - a));
        if (mirrored_transform) {
            face_normals[face_index] = face_normals[face_index] * -1.0f;
        }
    }

    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<UV> uvs;
    std::vector<CMesh3D::Face> faces;
    vertices.reserve(selected_faces.size() * 3);
    normals.reserve(selected_faces.size() * 3);
    uvs.reserve(selected_faces.size() * 3);
    faces.reserve(selected_faces.size());

    const bool has_uvs = source.uvs.size() == source.vertices.size();
    for (size_t face_index : selected_faces) {
        const FaceData& face = source.faces[face_index];
        CMesh3D::Face output_face;
        output_face.corners.reserve(3);
        const std::array<std::uint16_t, 3> oriented_indices = mirrored_transform
            ? std::array<std::uint16_t, 3>{face.indices[0], face.indices[2], face.indices[1]}
            : face.indices;
        for (std::uint16_t source_index : oriented_indices) {
            Vec3 normal = face_normals[face_index];
            if (face.smoothing_group != 0) {
                Vec3 sum{};
                for (size_t adjacent_index : incident_faces[smoothing_vertex[source_index]]) {
                    const FaceData& adjacent = source.faces[adjacent_index];
                    if ((face.smoothing_group & adjacent.smoothing_group) != 0) {
                        sum = sum + face_normals[adjacent_index];
                    }
                }
                if (dot(sum, sum) > 0.0000001f) {
                    normal = normalize(sum);
                }
            }

            const size_t output_index = vertices.size();
            output_face.corners.push_back({output_index, output_index, output_index});
            vertices.push_back(transformed_vertices[source_index]);
            normals.push_back(normal);
            uvs.push_back(has_uvs ? source.uvs[source_index] : UV{});
        }
        faces.push_back(std::move(output_face));
    }

    std::string name = !instance_name.empty()
        ? instance_name
        : source.name.empty()
        ? "Imported 3DS " + std::to_string(mesh_number)
        : source.name;
    if (material_group && !material_group->name.empty()) {
        name += " [" + material_group->name + "]";
    }
    auto mesh = std::make_unique<CMesh3D>(name);
    if (material) {
        mesh->SetMaterial(*material);
    } else {
        mesh->SetColor(kDefaultMeshObjectColor);
    }
    if (!mesh->SetGeometry(std::move(vertices),
                           std::move(faces),
                           has_uvs ? std::move(uvs) : std::vector<UV>{},
                           std::move(normals))) {
        error = "3DS file contains invalid mesh geometry.";
        return false;
    }
    result = std::move(mesh);
    return true;
}
}

bool ThreeDSIO::Import(const std::string& path,
                       std::vector<std::unique_ptr<CMesh3D>>& meshes,
                       std::string& error) const {
    meshes.clear();
    error.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open 3DS file.";
        return false;
    }

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
    if (bytes.size() < 6) {
        error = "3DS file is empty or truncated.";
        return false;
    }
    if (bytes[0] != static_cast<unsigned char>(kMain3ds & 0xff)
        || bytes[1] != static_cast<unsigned char>(kMain3ds >> 8)) {
        error = "The selected file is not a valid 3DS file.";
        return false;
    }

    Reader reader(std::move(bytes));
    SceneData scene;
    if (!parse_container(reader, reader.Size(), scene, error)) {
        return false;
    }
    const std::filesystem::path source_path(path);
    for (auto& [name, material] : scene.materials) {
        material.source_file_path.clear();
        if (!material.color_texture_path.empty()) {
            std::filesystem::path texture_path(material.color_texture_path);
            if (texture_path.is_relative()) {
                texture_path = source_path.parent_path() / texture_path;
            }
            material.color_texture_path = texture_path.lexically_normal().string();
        }
    }

    const float import_scale = scene.master_scale > 0.000001f
        ? 1.0f / scene.master_scale
        : 1.0f;
    const std::map<std::string, std::vector<MeshTransform>> mesh_transforms = build_mesh_transforms(scene);
    int mesh_number = 1;
    for (const MeshData& source : scene.meshes) {
        const auto append_parts = [&](const Matrix4* matrix, const std::string& instance_name) {
            const size_t part_count = source.material_groups.empty() ? 1 : source.material_groups.size();
            for (size_t part_index = 0; part_index < part_count; ++part_index) {
                const MaterialFaceGroup* material_group = source.material_groups.empty()
                    ? nullptr
                    : &source.material_groups[part_index];
                const Material* material = nullptr;
                if (material_group) {
                    const auto found = scene.materials.find(material_group->name);
                    if (found != scene.materials.end()) {
                        material = &found->second;
                    }
                }
                std::unique_ptr<CMesh3D> mesh;
                if (!build_mesh(source,
                                matrix,
                                instance_name,
                                material_group,
                                material,
                                import_scale,
                                mesh_number,
                                mesh,
                                error)) {
                    if (!error.empty()) {
                        return false;
                    }
                    continue;
                }
                meshes.push_back(std::move(mesh));
                ++mesh_number;
            }
            return true;
        };

        const auto transform = mesh_transforms.find(source.name);
        if (transform == mesh_transforms.end() || transform->second.empty()) {
            if (!append_parts(nullptr, {})) {
                meshes.clear();
                return false;
            }
            continue;
        }

        for (const MeshTransform& instance : transform->second) {
            if (!append_parts(&instance.matrix, instance.instance_name)) {
                meshes.clear();
                return false;
            }
        }
    }

    if (meshes.empty()) {
        error = "3DS file has no triangular mesh geometry.";
        return false;
    }
    return true;
}
