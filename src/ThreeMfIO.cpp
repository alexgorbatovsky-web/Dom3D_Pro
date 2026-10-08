#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <lib3mf_implicit.hpp>

#include "ExchangeIO.h"
#include "CMesh3D.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace {
using namespace Lib3MF;
using Matrix = std::array<double, 16>;
constexpr const char* kMetadataNamespace = "https://dom3d.pro/3mf/2026";
constexpr const char* kTextureRelationship = "http://schemas.microsoft.com/3dmanufacturing/2013/01/3dtexture";
constexpr const char* kMaterialRelationship = "https://dom3d.pro/3mf/2026/material-texture";
constexpr size_t kMaxTriangles = 10000000;
constexpr qint64 kMaxPackageBytes = 512ll * 1024 * 1024;

void check(bool valid, const std::string& message) {
    if (!valid) throw std::runtime_error(message);
}

Matrix identity() { return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; }
Matrix matrix(const sTransform& transform) {
    Matrix result = identity();
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col) {
            const double value = transform.m_Fields[col][row];
            check(std::isfinite(value), "3MF contains a non-finite transformation.");
            result[row * 4 + col] = value;
        }
    return result;
}
Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k) result[r*4+c] += a[r*4+k] * b[k*4+c];
    return result;
}
sTransform translation(Vec3 origin) {
    sTransform result{};
    for (int i = 0; i < 3; ++i) result.m_Fields[i][i] = 1;
    result.m_Fields[3][0] = origin.x;
    result.m_Fields[3][1] = origin.y;
    result.m_Fields[3][2] = origin.z;
    return result;
}
double millimeters(eModelUnit unit) {
    switch (unit) {
    case eModelUnit::MicroMeter: return 0.001;
    case eModelUnit::MilliMeter: return 1;
    case eModelUnit::CentiMeter: return 10;
    case eModelUnit::Meter: return 1000;
    case eModelUnit::Inch: return 25.4;
    case eModelUnit::Foot: return 304.8;
    default: throw std::runtime_error("Unsupported 3MF length unit.");
    }
}
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

sColor color(const Material& material) {
    const auto channel = [](float value) -> Lib3MF_uint8 {
        check(std::isfinite(value), "Material contains a non-finite colour.");
        return static_cast<Lib3MF_uint8>(std::clamp(value, 0.0f, 1.0f) * 255 + 0.5f);
    };
    return {channel(material.diffuse.r), channel(material.diffuse.g),
            channel(material.diffuse.b), channel(material.alpha)};
}
void apply_color(Material& material, sColor value) {
    material.diffuse = {value.m_Red / 255.0f, value.m_Green / 255.0f, value.m_Blue / 255.0f};
    material.alpha = value.m_Alpha / 255.0f;
}
QRgb rgba(sColor c) { return qRgba(c.m_Red, c.m_Green, c.m_Blue, c.m_Alpha); }

// Keep extracted images in application data, not next to the input file or in
// a temporary directory. The project serializer subsequently embeds them.
std::string save_image(const QImage& image) {
    check(!image.isNull() && qint64(image.width()) * image.height() <= 64000000,
          "3MF texture is invalid or too large.");
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    check(image.save(&buffer, "PNG"), "Could not encode a 3MF texture.");
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + "/Imported3MFTextures";
    check(QDir().mkpath(folder), "Could not create the imported texture directory.");
    const QString path = folder + '/' + hash + ".png";
    if (!QFile::exists(path)) {
        QSaveFile file(path);
        check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
              "Could not save an imported 3MF texture.");
    }
    return path.toStdString();
}
std::string extract_texture(const PAttachment& attachment) {
    check(attachment->GetStreamSize() <= kMaxPackageBytes, "3MF texture is too large.");
    std::vector<Lib3MF_uint8> data;
    attachment->WriteToBuffer(data);
    QByteArray bytes(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    check(reader.size().isValid() && qint64(reader.size().width()) * reader.size().height() <= 64000000,
          "3MF texture dimensions are invalid or too large.");
    return save_image(reader.read());
}

struct ScalarField { const char* name; float Material::* member; };
const ScalarField kScalars[]{
    {"specular", &Material::specular}, {"shininess", &Material::shininess},
    {"reflectivity", &Material::reflectivity}, {"roughness", &Material::roughness},
    {"metallic", &Material::metallic}, {"coat_weight", &Material::coat_weight},
    {"coat_roughness", &Material::coat_roughness}, {"normal_strength", &Material::normal_strength},
    {"displacement_scale", &Material::displacement_scale}
};
struct ColorField { const char* name; Color Material::* member; };
const ColorField kColors[]{{"ambient", &Material::ambient}, {"emission", &Material::emission}};
struct TextureField { const char* name; std::string Material::* member; };
const TextureField kTextures[]{
    {"color", &Material::color_texture_path}, {"light", &Material::light_texture_path},
    {"bump", &Material::bump_texture_path}, {"normal", &Material::normal_texture_path},
    {"roughness", &Material::roughness_texture_path}, {"metallic", &Material::metallic_texture_path},
    {"displacement", &Material::displacement_texture_path}
};
QJsonObject read_metadata(const PObject& object) {
    const auto group = object->GetMetaDataGroup();
    for (Lib3MF_uint32 i = 0; i < group->GetMetaDataCount(); ++i) {
        const auto entry = group->GetMetaData(i);
        if (entry->GetNameSpace() == kMetadataNamespace && entry->GetName() == "appearance")
            return QJsonDocument::fromJson(QByteArray::fromStdString(entry->GetValue())).object();
    }
    return {};
}

struct Appearance {
    Material material = Material::ImportedMesh();
    std::string key;
    std::array<UV, 3> uv{};
    std::array<QRgb, 3> colors{};
    bool textured = false;
    bool gradient = false;
};

class Importer {
public:
    PModel model;
    std::vector<std::unique_ptr<CMesh3D>> meshes;
    size_t triangle_count = 0;
    size_t instance_count = 0;
    std::set<Lib3MF_uint32> active;
    std::map<Lib3MF_uint32, std::string> texture_paths;

    Appearance appearance(sTriangleProperties properties, unsigned depth = 0) {
        check(depth < 16, "3MF material properties are cyclic or too deeply nested.");
        Appearance result;
        result.material.id = 0;
        result.material.name = "3MF Material";
        const auto id = properties.m_ResourceID;
        if (!id) { result.key = "default"; return result; }
        result.key = std::to_string(id) + ':' + std::to_string(properties.m_PropertyIDs[0]);
        switch (model->GetPropertyTypeByID(id)) {
        case ePropertyType::BaseMaterial: {
            check(properties.m_PropertyIDs[0] == properties.m_PropertyIDs[1]
                    && properties.m_PropertyIDs[0] == properties.m_PropertyIDs[2],
                  "3MF base materials cannot form vertex gradients.");
            const auto palette = model->GetBaseMaterialGroupByID(id);
            result.material.name = palette->GetName(properties.m_PropertyIDs[0]);
            apply_color(result.material, palette->GetDisplayColor(properties.m_PropertyIDs[0]));
            break;
        }
        case ePropertyType::Colors: {
            const auto palette = model->GetColorGroupByID(id);
            for (int i = 0; i < 3; ++i) result.colors[i] = rgba(palette->GetColor(properties.m_PropertyIDs[i]));
            result.gradient = result.colors[0] != result.colors[1] || result.colors[0] != result.colors[2];
            if (result.gradient) {
                result.key = std::to_string(id) + ":gradient";
                result.material.diffuse = {1,1,1};
                result.material.alpha = 1;
                result.material.name = "3MF Vertex Colors";
            } else {
                apply_color(result.material, palette->GetColor(properties.m_PropertyIDs[0]));
                result.material.name = "3MF Color " + QColor::fromRgba(result.colors[0]).name(QColor::HexArgb).toStdString();
            }
            break;
        }
        case ePropertyType::TexCoord: {
            const auto palette = model->GetTexture2DGroupByID(id);
            const auto texture = palette->GetTexture2D();
            const auto texture_id = texture->GetUniqueResourceID();
            auto found = texture_paths.find(texture_id);
            if (found == texture_paths.end())
                found = texture_paths.emplace(texture_id, extract_texture(texture->GetAttachment())).first;
            result.key = "texture:" + std::to_string(texture_id);
            result.material.name = "3MF Texture " + std::to_string(texture_id);
            result.material.diffuse = {1,1,1};
            result.material.alpha = 1;
            result.material.color_texture_path = found->second;
            result.textured = true;
            for (int i = 0; i < 3; ++i) {
                const auto uv = palette->GetTex2Coord(properties.m_PropertyIDs[i]);
                check(std::isfinite(uv.m_U) && std::isfinite(uv.m_V)
                        && std::abs(uv.m_U) < 1e20 && std::abs(uv.m_V) < 1e20,
                      "3MF has invalid texture coordinates.");
                result.uv[i] = {static_cast<float>(uv.m_U), static_cast<float>(uv.m_V)};
            }
            break;
        }
        case ePropertyType::Multi: {
            const auto multi = model->GetMultiPropertyGroupByID(id);
            result.key = "multi:" + std::to_string(id);
            std::array<std::vector<Lib3MF_uint32>, 3> ids;
            for (int i = 0; i < 3; ++i) multi->GetMultiProperty(properties.m_PropertyIDs[i], ids[i]);
            bool has_colour = false;
            for (Lib3MF_uint32 layer = 0; layer < multi->GetLayerCount(); ++layer) {
                const auto info = multi->GetLayer(layer);
                check(layer == 0 || info.m_TheBlendMethod == eBlendMethod::Multiply,
                      "Unsupported blend mode in 3MF material layers.");
                sTriangleProperties part{};
                part.m_ResourceID = info.m_ResourceID;
                for (int i = 0; i < 3; ++i) {
                    check(layer < ids[i].size(), "Invalid 3MF multi-property reference.");
                    part.m_PropertyIDs[i] = ids[i][layer];
                }
                const auto item = appearance(part, depth + 1);
                check(!item.gradient, "Gradient layers combined with other 3MF properties are not supported.");
                if (item.textured) {
                    check(!result.textured, "Multiple layered 3MF textures are not supported.");
                    result.material.color_texture_path = item.material.color_texture_path;
                    result.uv = item.uv;
                    result.textured = true;
                } else {
                    check(!has_colour, "Multiple blended 3MF colour layers are not supported.");
                    result.material.name = item.material.name;
                    result.material.diffuse = item.material.diffuse;
                    result.material.alpha = item.material.alpha;
                    has_colour = true;
                }
                result.key += '/' + item.key;
            }
            break;
        }
        case ePropertyType::Composite: {
            check(properties.m_PropertyIDs[0] == properties.m_PropertyIDs[1]
                    && properties.m_PropertyIDs[0] == properties.m_PropertyIDs[2],
                  "3MF composite material gradients are not supported.");
            const auto composite = model->GetCompositeMaterialsByID(id);
            const auto palette = composite->GetBaseMaterialGroup();
            std::vector<sCompositeConstituent> constituents;
            composite->GetComposite(properties.m_PropertyIDs[0], constituents);
            double total = 0;
            result.material.diffuse = {0,0,0};
            result.material.alpha = 0;
            result.material.name = "3MF Composite";
            for (const auto& part : constituents) {
                check(std::isfinite(part.m_MixingRatio) && part.m_MixingRatio >= 0,
                      "Invalid 3MF composite mixing ratio.");
                const auto c = palette->GetDisplayColor(part.m_PropertyID);
                const float weight = static_cast<float>(part.m_MixingRatio);
                result.material.diffuse.r += weight * c.m_Red / 255;
                result.material.diffuse.g += weight * c.m_Green / 255;
                result.material.diffuse.b += weight * c.m_Blue / 255;
                result.material.alpha += weight * c.m_Alpha / 255;
                total += weight;
            }
            check(std::abs(total - 1) < 0.001, "Invalid 3MF composite material weights.");
            break;
        }
        default: throw std::runtime_error("Unsupported 3MF material property type.");
        }
        return result;
    }

    void visit(const PObject& object, const Matrix& transform, const std::string& group, unsigned depth = 0) {
        check(++instance_count <= 100000, "3MF has too many expanded object instances.");
        check(depth < 64 && active.insert(object->GetUniqueResourceID()).second,
              "3MF components are cyclic or too deeply nested.");
        if (object->IsComponentsObject()) {
            const auto components = model->GetComponentsObjectByID(object->GetUniqueResourceID());
            const std::string name = group.empty() ? object->GetName() : group;
            for (Lib3MF_uint32 i = 0; i < components->GetComponentCount(); ++i) {
                const auto component = components->GetComponent(i);
                visit(component->GetObjectResource(), multiply(transform, matrix(component->GetTransform())), name, depth + 1);
            }
        } else if (object->IsMeshObject()) {
            load_mesh(model->GetMeshObjectByID(object->GetUniqueResourceID()), transform, group);
        } else {
            throw std::runtime_error("3MF contains a non-mesh object. Convert it to a mesh before importing.");
        }
        active.erase(object->GetUniqueResourceID());
    }

    void load_mesh(const PMeshObject& source, const Matrix& transform, const std::string& group) {
        triangle_count += source->GetTriangleCount();
        check(triangle_count <= kMaxTriangles && source->GetVertexCount() <= kMaxTriangles * 3,
              "3MF expanded geometry is too large.");
        std::vector<sPosition> positions;
        std::vector<sTriangle> triangles;
        std::vector<sTriangleProperties> properties;
        source->GetVertices(positions);
        source->GetTriangleIndices(triangles);
        source->GetAllTriangleProperties(properties);
        check(properties.size() == triangles.size(), "3MF triangle properties are incomplete.");
        Lib3MF_uint32 default_resource = 0, default_property = 0;
        source->GetObjectLevelProperty(default_resource, default_property);
        struct Part {
            Material material;
            std::vector<Vec3> vertices;
            std::vector<CMesh3D::Face> faces;
            std::vector<UV> uvs;
            std::map<Lib3MF_uint32, size_t> vertex_map;
            std::vector<std::array<QRgb, 3>> gradients;
            bool textured = false;
        };
        std::map<std::string, Part> parts;
        for (size_t t = 0; t < triangles.size(); ++t) {
            auto property = properties[t];
            if (!property.m_ResourceID && default_resource)
                property = {default_resource, {default_property, default_property, default_property}};
            const auto style = appearance(property);
            // Bound gradient atlases to 2048 x 2048 and avoid one mesh per face.
            std::string key = style.key;
            if (style.gradient) key += ':' + std::to_string(t / 4096);
            auto& part = parts[key];
            part.material = style.material;
            part.textured = style.textured || style.gradient;
            CMesh3D::Face face;
            for (int corner = 0; corner < 3; ++corner) {
                const auto index = triangles[t].m_Indices[corner];
                check(index < positions.size(), "3MF triangle refers to a missing vertex.");
                auto inserted = part.vertex_map.emplace(index, part.vertices.size());
                if (inserted.second) {
                    const auto& p = positions[index].m_Coordinates;
                    check(finite({p[0],p[1],p[2]}), "3MF contains invalid vertex coordinates.");
                    part.vertices.push_back({p[0],p[1],p[2]});
                }
                face.corners.push_back({inserted.first->second, 0, part.uvs.size()});
                part.uvs.push_back(style.uv[corner]);
            }
            part.faces.push_back(std::move(face));
            if (style.gradient) part.gradients.push_back(style.colors);
        }
        const QJsonObject metadata = read_metadata(source);
        for (auto& entry : parts) {
            auto& part = entry.second;
            if (!part.gradients.empty()) {
                constexpr int tile = 32;
                const int columns = static_cast<int>(std::ceil(std::sqrt(double(part.gradients.size()))));
                const int rows = static_cast<int>((part.gradients.size() + columns - 1) / columns);
                QImage atlas(columns * tile, rows * tile, QImage::Format_ARGB32);
                atlas.fill(Qt::transparent);
                for (size_t i = 0; i < part.gradients.size(); ++i) {
                    const int x0 = int(i % columns) * tile, y0 = int(i / columns) * tile;
                    const auto& c = part.gradients[i];
                    for (int y = 0; y < tile; ++y) for (int x = 0; x < tile; ++x) {
                        double u = std::clamp((x - 1.0) / (tile - 3), 0.0, 1.0);
                        double v = std::clamp((y - 1.0) / (tile - 3), 0.0, 1.0);
                        if (u + v > 1) { const double sum = u + v; u /= sum; v /= sum; }
                        const auto channel = [&](int a, int b, int d) {
                            return int(std::clamp(a * (1-u-v) + b*u + d*v, 0.0, 255.0) + 0.5);
                        };
                        atlas.setPixel(x0+x, y0+y, qRgba(channel(qRed(c[0]),qRed(c[1]),qRed(c[2])),
                            channel(qGreen(c[0]),qGreen(c[1]),qGreen(c[2])),
                            channel(qBlue(c[0]),qBlue(c[1]),qBlue(c[2])),
                            channel(qAlpha(c[0]),qAlpha(c[1]),qAlpha(c[2]))));
                    }
                    const auto uv = [&](int x, int y) -> UV {
                        return {float(x0+x+0.5f)/atlas.width(), 1.0f-float(y0+y+0.5f)/atlas.height()};
                    };
                    part.uvs[i*3] = uv(1,1);
                    part.uvs[i*3+1] = uv(tile-2,1);
                    part.uvs[i*3+2] = uv(1,tile-2);
                }
                part.material.color_texture_path = save_image(atlas);
            }
            for (const auto& field : kScalars) {
                const auto value = metadata.value(field.name);
                if (value.isDouble() && std::isfinite(value.toDouble()) && std::abs(value.toDouble()) <= 1e6)
                    part.material.*field.member = static_cast<float>(value.toDouble());
            }
            for (const auto& field : kColors) {
                const auto value = metadata.value(field.name).toArray();
                if (value.size() != 3) continue;
                const Color c{float(value[0].toDouble()),float(value[1].toDouble()),float(value[2].toDouble())};
                if (std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b)) part.material.*field.member = c;
            }
            const auto textures = metadata.value("textures").toObject();
            for (const auto& field : kTextures) {
                const QString uri = textures.value(field.name).toString();
                if (!uri.isEmpty()) part.material.*field.member = extract_texture(model->FindAttachment(uri.toStdString()));
            }
            std::string name = source->GetName();
            if (name.empty()) name = "Imported 3MF";
            if (parts.size() > 1) name += " [" + part.material.name + ']';
            auto mesh = std::make_unique<CMesh3D>(name);
            check(mesh->SetGeometry(std::move(part.vertices), std::move(part.faces),
                                   part.textured ? std::move(part.uvs) : std::vector<UV>{}),
                  "3MF contains invalid mesh geometry.");
            check(mesh->ApplyAffineTransform(transform), "3MF contains a singular transformation.");
            for (auto p : mesh->GetVertices()) check(finite(p), "Transformed 3MF coordinates exceed the supported range.");
            mesh->SetMaterial(part.material);
            const QString wire = metadata.value("wireColor").toString();
            const QColor wire_color(wire);
            mesh->SetColor(wire_color.isValid() ? Color{float(wire_color.redF()),float(wire_color.greenF()),float(wire_color.blueF())}
                                               : part.material.diffuse);
            mesh->SetGroupName(metadata.contains("group") ? metadata.value("group").toString().toStdString() : group);
            meshes.push_back(std::move(mesh));
        }
    }
};

class Exporter {
public:
    PModel model;
    PBaseMaterialGroup palette;
    size_t triangle_count = 0;
    std::map<QString, PAttachment> attachments;

    PAttachment texture(const std::string& path, bool colour) {
        const QString filename = QString::fromStdString(path);
        const auto key = filename + (colour ? ":color" : ":pbr");
        const auto found = attachments.find(key);
        if (found != attachments.end()) return found->second;
        QImageReader reader(filename);
        check(reader.size().isValid() && qint64(reader.size().width()) * reader.size().height() <= 64000000,
              "Cannot read material texture: " + path);
        const QImage image = reader.read();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        check(!image.isNull() && image.save(&buffer, "PNG"), "Cannot encode material texture: " + path);
        const std::string uri = "/3D/Textures/texture" + std::to_string(attachments.size()) + ".png";
        const auto attachment = model->AddAttachment(uri, colour ? kTextureRelationship : kMaterialRelationship);
        std::vector<Lib3MF_uint8> data(bytes.begin(), bytes.end());
        attachment->ReadFromBuffer(data);
        attachments.emplace(key, attachment);
        return attachment;
    }

    std::pair<PMeshObject, sTransform> mesh(const CMesh3D& source, const Material& material,
                                           const std::string& name, const std::string& group, Color wire) {
        const auto& vertices = source.GetVertices();
        const auto& uvs = source.GetUVs();
        Vec3 low{}, high{};
        check(source.GetBounds(low, high) && finite(low) && finite(high), "Cannot export invalid mesh bounds.");
        const Vec3 origin = low * 0.5f + high * 0.5f;
        std::vector<sPosition> positions;
        positions.reserve(vertices.size());
        for (auto p : vertices) {
            check(finite(p) && finite(p-origin), "Cannot export non-finite mesh coordinates.");
            p = p - origin;
            positions.push_back({{p.x,p.y,p.z}});
        }
        check(vertices.size() <= std::numeric_limits<Lib3MF_uint32>::max(), "3MF mesh has too many vertices.");
        std::vector<sTriangle> triangles;
        std::vector<sTriangleProperties> properties;
        const auto material_id = palette->AddMaterial(material.name.empty() ? "Material" : material.name, color(material));
        PTexture2DGroup texture_group;
        PMultiPropertyGroup multi;
        QJsonObject metadata;
        for (const auto& field : kScalars) {
            check(std::isfinite(material.*field.member), "Material contains a non-finite parameter.");
            metadata.insert(field.name, material.*field.member);
        }
        for (const auto& field : kColors) {
            const Color c = material.*field.member;
            check(std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b), "Material contains an invalid colour.");
            metadata.insert(field.name, QJsonArray{c.r,c.g,c.b});
        }
        metadata.insert("group", QString::fromStdString(group));
        metadata.insert("wireColor", QColor::fromRgbF(std::clamp(wire.r,0.f,1.f),std::clamp(wire.g,0.f,1.f),std::clamp(wire.b,0.f,1.f)).name());
        QJsonObject texture_metadata;
        for (const auto& field : kTextures) {
            if ((material.*field.member).empty()) continue;
            const auto attachment = texture(material.*field.member, std::string(field.name) == "color");
            texture_metadata.insert(field.name, QString::fromStdString(attachment->GetPath()));
            if (std::string(field.name) == "color") {
                const auto image = model->AddTexture2DFromAttachment(attachment);
                image->SetContentType(eTextureType::PNG);
                texture_group = model->AddTexture2DGroup(image);
                multi = model->AddMultiPropertyGroup();
                multi->AddLayer({palette->GetUniqueResourceID(), eBlendMethod::Multiply});
                multi->AddLayer({texture_group->GetUniqueResourceID(), eBlendMethod::Multiply});
            }
        }
        metadata.insert("textures", texture_metadata);
        UV uv_min{}, uv_max{};
        if (material.texture_fit_to_surface && !uvs.empty()) {
            uv_min = uv_max = uvs.front();
            for (const auto& uv : uvs) {
                uv_min.u = std::min(uv_min.u, uv.u); uv_min.v = std::min(uv_min.v, uv.v);
                uv_max.u = std::max(uv_max.u, uv.u); uv_max.v = std::max(uv_max.v, uv.v);
            }
        }
        const auto property_at = [&](const MeshCorner& corner) {
            check(corner.uv < uvs.size(), "Textured mesh contains invalid UV indices.");
            auto original = uvs[corner.uv];
            if (material.texture_fit_to_surface) {
                original.u = (original.u-uv_min.u) / std::max(uv_max.u-uv_min.u, 0.00001f);
                original.v = (original.v-uv_min.v) / std::max(uv_max.v-uv_min.v, 0.00001f);
            }
            const double angle = material.texture_rotation_degrees * 3.14159265358979323846 / 180;
            const double u = (original.u-0.5) * material.texture_scale_u;
            const double v = (original.v-0.5) * material.texture_scale_v;
            const sTex2Coord uv{u*std::cos(angle)-v*std::sin(angle)+0.5+material.texture_offset_u,
                               u*std::sin(angle)+v*std::cos(angle)+0.5+material.texture_offset_v};
            check(std::isfinite(uv.m_U) && std::isfinite(uv.m_V), "Cannot export non-finite texture coordinates.");
            const auto uv_id = texture_group->AddTex2Coord(uv);
            return multi->AddMultiProperty(std::vector<Lib3MF_uint32>{material_id, uv_id});
        };
        for (const auto& face : source.GetFaces()) {
            if (face.deleted || face.corners.size() < 3) continue;
            for (size_t i = 1; i+1 < face.corners.size(); ++i) {
                const MeshCorner corners[]{face.corners[0],face.corners[i],face.corners[i+1]};
                for (const auto& corner : corners)
                    check(corner.v < vertices.size(), "Mesh contains an invalid triangle index.");
                // Welding can collapse polygon corners. Lib3MF rejects repeated
                // triangle indices, so omit these zero-area fan triangles while
                // retaining any valid triangles from the same polygon.
                if (corners[0].v == corners[1].v || corners[1].v == corners[2].v
                    || corners[2].v == corners[0].v) continue;
                check(++triangle_count <= kMaxTriangles, "Scene contains too many 3MF triangles.");
                sTriangle triangle{};
                sTriangleProperties property{};
                property.m_ResourceID = multi ? multi->GetUniqueResourceID() : palette->GetUniqueResourceID();
                for (int c = 0; c < 3; ++c) {
                    triangle.m_Indices[c] = static_cast<Lib3MF_uint32>(corners[c].v);
                    property.m_PropertyIDs[c] = multi ? property_at(corners[c]) : material_id;
                }
                triangles.push_back(triangle);
                properties.push_back(property);
            }
        }
        check(!triangles.empty(), "Mesh has no exportable triangles: " + name);
        const auto result = model->AddMeshObject();
        result->SetName(name);
        result->SetGeometry(positions, triangles);
        result->SetObjectLevelProperty(palette->GetUniqueResourceID(), material_id);
        result->SetAllTriangleProperties(properties);
        result->GetMetaDataGroup()->AddMetaData(kMetadataNamespace, "appearance",
            QJsonDocument(metadata).toJson(QJsonDocument::Compact).toStdString(), "string", true);
        return {result, translation(origin)};
    }
};
}

bool ThreeMfIO::Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes,
                       std::string& error) const {
    error.clear();
    try {
        QFile file(QString::fromStdString(path));
        check(file.open(QIODevice::ReadOnly), "Could not open 3MF file.");
        check(file.size() > 0 && file.size() <= kMaxPackageBytes, "3MF package is empty or too large.");
        const QByteArray bytes = file.readAll();
        check(bytes.size() == file.size(), "Could not read the complete 3MF package.");
        const auto wrapper = CWrapper::loadLibrary();
        Importer importer;
        importer.model = wrapper->CreateModel();
        const auto reader = importer.model->QueryReader("3mf");
        reader->AddRelationToRead(kMaterialRelationship);
        // Slicers add unqualified metadata and attributes (e.g. OrcaSlicer,
        // auto_drop). Let lib3mf collect warnings, then reject everything except
        // these ignorable extensions before exposing any geometry to the scene.
        reader->SetStrictModeActive(false);
        reader->ReadFromBuffer(std::vector<Lib3MF_uint8>(bytes.begin(), bytes.end()));
        // NMR_ErrorConst.h from the pinned lib3mf 2.5.0 SDK source. These reader
        // warning codes are not part of the public bindings' API error enum.
        constexpr Lib3MF_uint32 unknown_metadata = 0x80B2;
        constexpr Lib3MF_uint32 unknown_attribute = 0x80A7;
        for (Lib3MF_uint32 i = 0; i < reader->GetWarningCount(); ++i) {
            Lib3MF_uint32 code = 0;
            const std::string warning = reader->GetWarning(i, code);
            check(code == unknown_metadata || code == unknown_attribute,
                  "Invalid 3MF package (" + std::to_string(code) + "): " + warning);
        }
        const double scale = millimeters(importer.model->GetUnit());
        Matrix units = identity();
        units[0] = units[5] = units[10] = scale;
        const auto items = importer.model->GetBuildItems();
        while (items->MoveNext()) {
            const auto item = items->GetCurrent();
            importer.visit(item->GetObjectResource(), multiply(units, matrix(item->GetObjectTransform())), {});
        }
        check(!importer.meshes.empty(), "3MF build contains no triangle meshes.");
        // A failed import leaves the caller's scene completely unchanged.
        meshes.reserve(meshes.size() + importer.meshes.size());
        for (auto& mesh : importer.meshes) meshes.push_back(std::move(mesh));
        return true;
    } catch (const std::exception& exception) {
        error = std::string("3MF import: ") + exception.what();
        return false;
    }
}

bool ThreeMfIO::Export(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    error.clear();
    try {
        const auto wrapper = CWrapper::loadLibrary();
        Exporter exporter;
        exporter.model = wrapper->CreateModel();
        exporter.model->SetUnit(eModelUnit::MilliMeter);
        exporter.model->AddCustomContentType("png", "image/png");
        exporter.palette = exporter.model->AddBaseMaterialGroup();
        exporter.model->GetMetaDataGroup()->AddMetaData("", "Application", "Dom3D Pro", "string", false);
        for (const auto& object : document.GetObjects()) {
            if (!object || !document.IsObjectVisible(*object)) continue;
            if (const auto* mesh = dynamic_cast<const CMesh3D*>(object.get())) {
                const auto result = exporter.mesh(*mesh, mesh->GetMaterial(), mesh->GetName(), mesh->GetGroupName(), mesh->GetColor());
                exporter.model->AddBuildItem(result.first.get(), result.second);
            } else if (const auto* solid = dynamic_cast<const CSolid*>(object.get())) {
                check(solid->EnsureRenderMesh(), "Could not tessellate solid: " + solid->GetName());
                const auto assembly = exporter.model->AddComponentsObject();
                assembly->SetName(solid->GetName());
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    const auto* surface = solid->GetSurfaceFace(i);
                    if (!surface || !surface->pMesh3D) continue;
                    Material material = ComposeSurfaceMaterial(solid->GetMaterial(), surface->MaterialOverride);
                    material.texture_offset_u += surface->TextureTransform.offset_u;
                    material.texture_offset_v += surface->TextureTransform.offset_v;
                    material.texture_scale_u *= surface->TextureTransform.scale_u;
                    material.texture_scale_v *= surface->TextureTransform.scale_v;
                    material.texture_rotation_degrees += surface->TextureTransform.rotation_degrees;
                    const auto result = exporter.mesh(*surface->pMesh3D, material,
                        solid->GetName() + " / " + std::to_string(i+1), solid->GetGroupName(), solid->GetColor());
                    assembly->AddComponent(result.first.get(), result.second);
                }
                check(assembly->GetComponentCount() > 0, "Solid has no exportable surfaces.");
                exporter.model->AddBuildItem(assembly.get(), translation({}));
            }
        }
        check(exporter.triangle_count > 0, "There are no visible mesh or solid objects to export.");
        std::vector<Lib3MF_uint8> bytes;
        const auto writer = exporter.model->QueryWriter("3mf");
        writer->SetDecimalPrecision(9);
        writer->WriteToBuffer(bytes);
        QSaveFile file(QString::fromStdString(path));
        check(file.open(QIODevice::WriteOnly)
                && file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size())
                && file.commit(), "Could not write the 3MF file.");
        return true;
    } catch (const std::exception& exception) {
        error = std::string("3MF export: ") + exception.what();
        return false;
    }
}
