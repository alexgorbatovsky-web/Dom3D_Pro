#include "NativeRaytraceRenderer.h"

#include <raytrace/rt.h>
#include <raytrace/rtu.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace {
QImage pixel_buffer_image(RtPixBuf pixels, double exposure_ev) {
    if (!pixels || !pixels->bits || pixels->w == 0 || pixels->h == 0) return {};
    QImage result(static_cast<int>(pixels->w), static_cast<int>(pixels->h),
                  QImage::Format_RGB32);
    const float exposure = std::pow(
        2.0f, static_cast<float>(std::clamp(exposure_ev, -4.0, 6.0)));
    const auto tone_map = [exposure](int channel) {
        const float value = (channel / 255.0f) * exposure;
        const float mapped = std::clamp(
            value * (2.51f * value + 0.03f)
                / (value * (2.43f * value + 0.59f) + 0.14f),
            0.0f, 1.0f);
        return static_cast<int>(mapped * 255.0f + 0.5f);
    };
    for (int y = 0; y < result.height(); ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const RtBgr value = pixels->bits[y * result.width() + x];
            row[x] = qRgb(tone_map((value >> 16) & 0xff),
                          tone_map((value >> 8) & 0xff),
                          tone_map(value & 0xff));
        }
    }
    return result;
}

struct CheckpointData {
    std::atomic_bool* cancel = nullptr;
    RtContext context = nullptr;
    double exposure_ev = 0.0;
    NativeRaytraceRenderer::ProgressCallback progress;
    std::chrono::steady_clock::time_point last_preview{};
    int checkpoint_count = 0;
    int last_percent = -1;
};

RtInt checkpoint(void* value) {
    auto* data = static_cast<CheckpointData*>(value);
    if (data && data->cancel
        && data->cancel->load(std::memory_order_relaxed)) return 0;
    if (!data || !data->context || !data->progress) return 1;

    const RtState state = rtState(data->context);
    if (state & RT_COMPILING) {
        RtCompilerStats stats = rtCompilerStats(data->context);
        const int percent = stats
            ? std::clamp(static_cast<int>(stats->percentage), 0, 100) : 0;
        if (percent != data->last_percent) {
            data->last_percent = percent;
            data->progress(std::min(percent / 10, 9), {}, "Compiling scene");
        }
        return 1;
    }
    if (!(state & RT_RENDERING)) return 1;

    // The legacy callback runs before every pixel. Check the clock only once
    // per 256 callbacks and copy the live buffer at most four times a second.
    if ((++data->checkpoint_count & 255) != 0) return 1;
    const auto now = std::chrono::steady_clock::now();
    if (data->last_preview.time_since_epoch().count() != 0
        && now - data->last_preview < std::chrono::milliseconds(250)) return 1;
    data->last_preview = now;
    RtRenderStats stats = rtRenderStats(data->context);
    const int traced = stats && stats->pixNum
        ? std::clamp(static_cast<int>(100ULL * stats->pixTraced / stats->pixNum),
                     0, 100)
        : 0;
    const int percent = 10 + traced * 89 / 100;
    data->last_percent = percent;
    data->progress(percent,
        pixel_buffer_image(rtPixelBuffer(data->context), data->exposure_ev),
        "Tracing image");
    return 1;
}

float dot3(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length3(Vec3 value) {
    return std::sqrt(dot3(value, value));
}

QString resolved_texture_path(const Material& material,
                              const std::string& source_path) {
    QString path = QString::fromStdString(source_path).trimmed();
    if (path.isEmpty()) return {};
    QFileInfo info(path);
    if (info.isAbsolute() && info.exists()) return info.absoluteFilePath();
    if (!material.source_file_path.empty()) {
        const QDir source_dir(QFileInfo(
            QString::fromStdString(material.source_file_path)).absolutePath());
        const QString sibling = source_dir.filePath(path);
        if (QFileInfo::exists(sibling)) return QFileInfo(sibling).absoluteFilePath();
    }
    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString in_materials = app_dir.filePath("materials/" + path);
    if (QFileInfo::exists(in_materials)) {
        return QFileInfo(in_materials).absoluteFilePath();
    }
    const QString beside_app = app_dir.filePath(path);
    return QFileInfo::exists(beside_app)
        ? QFileInfo(beside_app).absoluteFilePath() : QString{};
}

QString texture_key(const QString& path, const char* role, Color tint) {
    return QString("%1|%2|%3|%4|%5").arg(
        path, QString::fromLatin1(role), QString::number(tint.r, 'g', 7),
        QString::number(tint.g, 'g', 7), QString::number(tint.b, 'g', 7));
}

bool material_is_pbr(const Material& material) {
    return !material.normal_texture_path.empty()
        || !material.roughness_texture_path.empty()
        || !material.metallic_texture_path.empty()
        || !material.displacement_texture_path.empty()
        || material.coat_weight > 0.0001f
        || std::fabs(material.roughness - 0.5f) > 0.0001f
        || material.metallic > 0.0001f;
}

RtTexture create_texture(RtContext context, const QString& path,
                         Color tint, bool alpha_only = false) {
    QImage source(path);
    if (source.isNull()) return nullptr;
    source = source.convertToFormat(QImage::Format_RGBA8888).mirrored(false, true);
    RtTexture texture = rtTexture(
        context, static_cast<RtSize>(source.width()),
        static_cast<RtSize>(source.height()));
    RtBgr* bits = texture ? rtTexBits(texture) : nullptr;
    if (!bits) return nullptr;
    for (int y = 0; y < source.height(); ++y) {
        const uchar* row = source.constScanLine(y);
        for (int x = 0; x < source.width(); ++x) {
            const int offset = x * 4;
            const auto channel = [](float value) {
                return static_cast<RtBgr>(std::clamp(
                    static_cast<int>(value * 255.0f + 0.5f), 0, 255));
            };
            const RtBgr r = alpha_only ? row[offset + 3]
                : channel((row[offset] / 255.0f) * tint.r);
            const RtBgr g = alpha_only ? row[offset + 3]
                : channel((row[offset + 1] / 255.0f) * tint.g);
            const RtBgr b = alpha_only ? row[offset + 3]
                : channel((row[offset + 2] / 255.0f) * tint.b);
            bits[y * source.width() + x] = (r << 16) | (g << 8) | b;
        }
    }
    return texture;
}

QString resolved_environment_path(const QString& source_path) {
    QFileInfo info(source_path.trimmed());
    if (!info.exists()) return {};
    if (info.suffix().compare("exr", Qt::CaseInsensitive) == 0) {
        QString base = info.completeBaseName();
        if (base.endsWith("_HDR", Qt::CaseInsensitive)) base.chop(4);
        const QString preview = info.absoluteDir().filePath(
            base + "_TONEMAPPED.jpg");
        if (QFileInfo::exists(preview)) return preview;
    }
    return info.absoluteFilePath();
}

RtTexture create_environment_texture(RtContext context,
                                     const RenderEnvironment& environment) {
    QImage source(resolved_environment_path(environment.hdri_path));
    if (source.isNull()) {
        constexpr int width = 512;
        constexpr int height = 256;
        source = QImage(width, height, QImage::Format_RGBA8888);
        const Vec3 key{-0.46f, 0.54f, 0.70f};
        const Vec3 fill{0.72f, 0.12f, 0.68f};
        for (int y = 0; y < height; ++y) {
            auto* row = reinterpret_cast<QRgb*>(source.scanLine(y));
            const float latitude = ((y + 0.5f) / height - 0.5f)
                * 3.14159265f;
            const float cl = std::cos(latitude);
            for (int x = 0; x < width; ++x) {
                const float longitude = ((x + 0.5f) / width - 0.5f)
                    * 6.28318531f;
                const Vec3 direction{cl * std::cos(longitude),
                    cl * std::sin(longitude), std::sin(latitude)};
                const float sky = 0.22f + 0.16f
                    * std::clamp(direction.z * 0.5f + 0.5f, 0.0f, 1.0f);
                const float key_lobe = 3.2f * std::pow(
                    std::max(dot3(direction, key), 0.0f), 32.0f);
                const float fill_lobe = 1.1f * std::pow(
                    std::max(dot3(direction, fill), 0.0f), 10.0f);
                const auto channel = [](float value) {
                    return std::clamp(static_cast<int>(
                        std::pow(std::max(value, 0.0f), 1.0f / 2.2f)
                        * 255.0f + 0.5f), 0, 255);
                };
                row[x] = qRgba(channel(sky * 0.92f + key_lobe
                                       + fill_lobe * 0.78f),
                               channel(sky * 0.97f + key_lobe
                                       + fill_lobe * 0.88f),
                               channel(sky * 1.08f + key_lobe + fill_lobe), 255);
            }
        }
    }
    source = source.scaledToWidth(std::min(1024, source.width()),
        Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
    RtTexture texture = rtTexture(context, source.width(), source.height());
    RtBgr* bits = texture ? rtTexBits(texture) : nullptr;
    if (!bits) return nullptr;
    for (int y = 0; y < source.height(); ++y) {
        const uchar* row = source.constScanLine(source.height() - 1 - y);
        for (int x = 0; x < source.width(); ++x) {
            const int offset = x * 4;
            bits[y * source.width() + x] =
                (static_cast<RtBgr>(row[offset]) << 16)
                | (static_cast<RtBgr>(row[offset + 1]) << 8)
                | static_cast<RtBgr>(row[offset + 2]);
        }
    }
    return texture;
}

struct LegacyOpticalMaterial {
    Color reflection{};
    float specular = 0.0f;
    float gloss = 0.0f;
};

LegacyOpticalMaterial legacy_optical_material(const Material& material) {
    const float metallic = std::clamp(material.metallic, 0.0f, 1.0f);
    const float roughness = std::clamp(material.roughness, 0.04f, 1.0f);
    const float coat = std::clamp(material.coat_weight, 0.0f, 1.0f);
    const float coat_roughness = std::clamp(
        material.coat_roughness, 0.01f, 1.0f);

    // Dom3D's current materials are PBR, while the original renderer expects
    // the older diffuse/specular/reflect/gloss model.  An explicit legacy
    // reflectivity remains authoritative, but a metallic surface must also
    // become reflective: previously metallic=1 with reflectivity=0 was sent
    // as RT_REFLECT=0 and therefore lost every mirror reflection.
    const float explicit_reflection = std::clamp(
        material.reflectivity, 0.0f, 1.0f);
    const float metal_reflection = metallic
        * (0.72f + 0.24f * (1.0f - roughness));
    // A smooth dielectric is reflective as well.  OpenGL and Blender obtain
    // this from the Fresnel term even when Metallic is zero (the black tor
    // regression is exactly this case).  The legacy tracer has no
    // angle-dependent Fresnel coefficient, so use a conservative constant
    // approximation that grows as the surface becomes smoother.
    const bool pbr_dielectric = metallic < 0.9999f
        && (std::fabs(roughness - 0.5f) > 0.0001f
            || !material.roughness_texture_path.empty()
            || material.coat_weight > 0.0001f);
    const float dielectric_reflection = pbr_dielectric
        ? (1.0f - metallic)
            * std::clamp(material.specular, 0.0f, 1.0f)
            * (0.06f + 0.44f * std::pow(1.0f - roughness, 2.0f))
        : 0.0f;
    const float coat_reflection = coat
        * (0.04f + 0.12f * (1.0f - coat_roughness));
    const float reflection_strength = std::clamp(
        std::max({explicit_reflection, metal_reflection,
                  dielectric_reflection, coat_reflection}),
        0.0f, 0.96f);

    // Metals tint their reflections with the base colour.  Dielectrics and
    // the explicit old Reflectivity control retain neutral reflections.
    const float tint_r = (1.0f - metallic) + metallic
        * std::clamp(material.diffuse.r, 0.0f, 1.0f);
    const float tint_g = (1.0f - metallic) + metallic
        * std::clamp(material.diffuse.g, 0.0f, 1.0f);
    const float tint_b = (1.0f - metallic) + metallic
        * std::clamp(material.diffuse.b, 0.0f, 1.0f);

    LegacyOpticalMaterial result;
    result.reflection = {
        reflection_strength * tint_r,
        reflection_strength * tint_g,
        reflection_strength * tint_b};
    // Principled BSDF's Specular/IOR level is not the colour of an additive
    // highlight.  For a dielectric its default value of 0.5 corresponds to
    // roughly four percent reflected light (F0), whereas the legacy tracer
    // interprets RT_SPECULAR literally.  Passing the stored 0.18--0.24 wood
    // values through unchanged therefore produced broad, clipped white
    // streaks over colour textures.  Convert dielectrics to their effective
    // F0 and blend towards the deliberately strong metallic highlight.
    const float dielectric_specular = 0.08f * std::max({
        0.5f, std::clamp(material.specular, 0.0f, 1.0f),
        explicit_reflection});
    const float metal_specular = 0.55f + 0.35f * (1.0f - roughness);
    result.specular = std::clamp(
        dielectric_specular * (1.0f - metallic)
            + metal_specular * metallic,
        0.0f, 1.0f);

    // RT_GLOSS is normalized and converted by the legacy API to an exponent
    // in [10, 10000].  Derive that exponent from PBR roughness, while keeping
    // the old Shininess value useful for non-PBR materials.
    const float pbr_exponent = std::clamp(
        2.0f / std::pow(roughness, 4.0f) - 2.0f, 10.0f, 10000.0f);
    const bool has_pbr_optics = metallic > 0.0001f || coat > 0.0001f
        || std::fabs(roughness - 0.5f) > 0.0001f;
    const float exponent = has_pbr_optics
        ? pbr_exponent
        : std::clamp(material.shininess, 10.0f, 10000.0f);
    result.gloss = (exponent - 10.0f) / (10000.0f - 10.0f);
    return result;
}
}

bool NativeRaytraceRenderer::Render(
    const RenderScene& scene,
    const NativeRaytraceSettings& settings,
    QImage* image,
    QString* error,
    std::atomic_bool* cancel,
    ProgressCallback progress) {
    if (!image || scene.meshes.empty()) {
        if (error) *error = "The scene has no renderable geometry.";
        return false;
    }

    RtContext context = rtCreateContext();
    if (!context) {
        if (error) *error = "Could not create the native raytrace context.";
        return false;
    }
    const auto cleanup = [&]() { rtDeleteContext(context); };
    CheckpointData checkpoint_data;
    checkpoint_data.cancel = cancel;
    checkpoint_data.context = context;
    checkpoint_data.exposure_ev = settings.exposure_ev;
    checkpoint_data.progress = std::move(progress);

    RtFaces faces = rtFaces(context, static_cast<RtSize>(scene.meshes.size()));
    if (!faces) {
        cleanup();
        if (error) *error = "Not enough memory for raytrace faces.";
        return false;
    }

    QSet<QString> texture_keys;
    for (const RenderMesh& mesh : scene.meshes) {
        if (mesh.material_index < 0
            || mesh.material_index >= static_cast<int>(scene.materials.size())) {
            continue;
        }
        const Material& material = scene.materials[mesh.material_index];
        const QString color_path = resolved_texture_path(
            material, material.color_texture_path);
        if (!color_path.isEmpty()) {
            texture_keys.insert(texture_key(
                color_path, "diffuse", material.diffuse));
            texture_keys.insert(texture_key(
                color_path, "ambient", material.ambient));
            QImage source(color_path);
            if (source.hasAlphaChannel()) {
                texture_keys.insert(texture_key(
                    color_path, "opacity", {1.0f, 1.0f, 1.0f}));
            }
        }
        const QString bump_path = resolved_texture_path(
            material, material.bump_texture_path);
        const QString displacement_path = resolved_texture_path(
            material, material.displacement_texture_path);
        const QString normal_path = resolved_texture_path(
            material, material.normal_texture_path);
        const QString roughness_path = resolved_texture_path(
            material, material.roughness_texture_path);
        const QString metallic_path = resolved_texture_path(
            material, material.metallic_texture_path);
        const QString height_path = !bump_path.isEmpty()
            ? bump_path : displacement_path;
        if (!height_path.isEmpty()) {
            texture_keys.insert(texture_key(
                height_path, "bump", {1.0f, 1.0f, 1.0f}));
        }
        if (!normal_path.isEmpty()) texture_keys.insert(texture_key(
            normal_path, "normal", {1.0f, 1.0f, 1.0f}));
        if (!roughness_path.isEmpty()) texture_keys.insert(texture_key(
            roughness_path, "roughness", {1.0f, 1.0f, 1.0f}));
        if (!metallic_path.isEmpty()) texture_keys.insert(texture_key(
            metallic_path, "metallic", {1.0f, 1.0f, 1.0f}));
    }
    // The legacy API stores Texture objects in one contiguous array and faces
    // keep direct pointers to them. Reserve the complete array before the
    // first texture is created so a later realloc cannot invalidate pointers.
    const bool use_environment = scene.environment.enabled;
    if (!rtReserveTextures(context, static_cast<RtSize>(
            texture_keys.size() + (use_environment ? 1 : 0)))) {
        cleanup();
        if (error) *error = "Not enough memory for raytrace textures.";
        return false;
    }

    QHash<QString, RtTexture> textures;
    for (std::size_t mesh_index = 0; mesh_index < scene.meshes.size(); ++mesh_index) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            cleanup();
            if (error) *error = "Render canceled.";
            return false;
        }
        const RenderMesh& mesh = scene.meshes[mesh_index];
        RtFace face = rtFace(faces, static_cast<RtSize>(mesh_index));
        const Material material = mesh.material_index >= 0
                && mesh.material_index < static_cast<int>(scene.materials.size())
            ? scene.materials[mesh.material_index] : Material::DefaultSurface();
        const LegacyOpticalMaterial optics = legacy_optical_material(material);
        const bool pbr_material = material_is_pbr(material);
        const bool room_ceiling = mesh.name.startsWith(
            "Room Ceiling", Qt::CaseInsensitive);
        const Color render_ambient = room_ceiling
            ? Color{1.80f, 1.76f, 1.68f} : material.ambient;
        const Color render_diffuse = room_ceiling
            ? Color{0.18f, 0.18f, 0.17f} : material.diffuse;
        const float render_specular = room_ceiling ? 0.02f : optics.specular;
        rtFaceColor(face, render_ambient.r, render_ambient.g,
                    render_ambient.b, RT_AMBIENT);
        rtFaceColor(face, render_diffuse.r, render_diffuse.g,
                    render_diffuse.b, RT_DIFFUSE);
        rtFaceColor(face, render_specular, render_specular,
                    render_specular, RT_SPECULAR);
        rtFaceColor(face, material.alpha, material.alpha,
                    material.alpha, RT_OPACITY);
        const float explicit_reflection = std::clamp(
            material.reflectivity, 0.0f, 1.0f);
        rtFaceColor(face,
                    pbr_material ? explicit_reflection : optics.reflection.r,
                    pbr_material ? explicit_reflection : optics.reflection.g,
                    pbr_material ? explicit_reflection : optics.reflection.b,
                    RT_REFLECT);
        rtFaceColor(face, optics.gloss, optics.gloss, optics.gloss, RT_GLOSS);
        if (pbr_material) {
            rtFacePbr(face,
                std::clamp(material.metallic, 0.0f, 1.0f),
                std::clamp(material.roughness, 0.04f, 1.0f),
                std::clamp(material.specular, 0.0f, 1.0f),
                std::clamp(material.coat_weight, 0.0f, 1.0f),
                std::clamp(material.coat_roughness, 0.01f, 1.0f),
                std::clamp(material.normal_strength, 0.0f, 4.0f));
        }

        const QString color_path = resolved_texture_path(
            material, material.color_texture_path);
        if (!color_path.isEmpty()) {
            const auto assign_tinted = [&](const char* role, Color tint,
                                           RtFlags flags) {
                const QString key = texture_key(color_path, role, tint);
                RtTexture texture = textures.value(key, nullptr);
                if (!texture) {
                    texture = create_texture(context, color_path, tint);
                    if (texture) textures.insert(key, texture);
                }
                if (texture) rtFaceTexture(face, texture, flags);
            };
            assign_tinted("diffuse", material.diffuse, RT_DIFFUSE);
            if (!pbr_material) {
                assign_tinted("ambient", material.ambient, RT_AMBIENT);
            }
            QImage source(color_path);
            if (source.hasAlphaChannel()) {
                const Color white{1.0f, 1.0f, 1.0f};
                const QString key = texture_key(color_path, "opacity", white);
                RtTexture texture = textures.value(key, nullptr);
                if (!texture) {
                    texture = create_texture(
                        context, color_path, white, true);
                    if (texture) textures.insert(key, texture);
                }
                if (texture) rtFaceTexture(face, texture, RT_OPACITY);
            }
        }
        const QString bump_path = resolved_texture_path(
            material, material.bump_texture_path);
        const QString displacement_path = resolved_texture_path(
            material, material.displacement_texture_path);
        const QString height_path = !bump_path.isEmpty()
            ? bump_path : displacement_path;
        if (!height_path.isEmpty()) {
            const Color white{1.0f, 1.0f, 1.0f};
            const QString key = texture_key(height_path, "bump", white);
            RtTexture texture = textures.value(key, nullptr);
            if (!texture) {
                texture = create_texture(context, height_path, white);
                if (texture) textures.insert(key, texture);
            }
            if (texture) rtFaceTexture(face, texture, RT_BUMP);
        }
        const auto assign_pbr_map = [&](const std::string& source,
                                        const char* role, RtFlags flag) {
            const QString path = resolved_texture_path(material, source);
            if (path.isEmpty()) return;
            const Color white{1.0f, 1.0f, 1.0f};
            const QString key = texture_key(path, role, white);
            RtTexture texture = textures.value(key, nullptr);
            if (!texture) {
                texture = create_texture(context, path, white);
                if (texture) textures.insert(key, texture);
            }
            if (texture) rtFaceTexture(face, texture, flag);
        };
        if (pbr_material) {
            assign_pbr_map(material.normal_texture_path, "normal", RT_NORMAL);
            assign_pbr_map(
                material.roughness_texture_path, "roughness", RT_ROUGHNESS);
            assign_pbr_map(
                material.metallic_texture_path, "metallic", RT_METALLIC);
        }

        RtTriangles triangles = rtTriangles(
            face, static_cast<RtSize>(mesh.triangles.size()));
        if (!triangles && !mesh.triangles.empty()) {
            cleanup();
            if (error) *error = "Not enough memory for raytrace triangles.";
            return false;
        }
        for (std::size_t triangle_index = 0;
             triangle_index < mesh.triangles.size(); ++triangle_index) {
            const RenderTriangle& source = mesh.triangles[triangle_index];
            RtTriangle triangle = rtTriangle(
                triangles, static_cast<RtSize>(triangle_index));
            for (int corner = 0; corner < 3; ++corner) {
                const int vertex_index = source.vertices[corner];
                if (vertex_index < 0
                    || vertex_index >= static_cast<int>(mesh.vertices.size())) {
                    continue;
                }
                const Vec3 point = mesh.vertices[vertex_index];
                const Vec3 normal = source.normals[corner];
                const UV uv = source.uvs[corner];
                RtVertex vertex = rtVertex(triangle, corner);
                rtPoint(vertex, point.x, point.y, point.z);
                rtNormal(vertex, normal.x, normal.y, normal.z);
                rtTexCoord(vertex, uv.u, uv.v);
            }
        }
    }

    if (use_environment) {
        if (RtTexture environment = create_environment_texture(
                context, scene.environment)) {
            rtEnvironment(context, environment,
                std::max(0.0f, scene.environment.strength),
                scene.environment.rotation_degrees
                    * 3.14159265358979323846f / 180.0f);
        }
    }

    Vec3 minimum{
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    Vec3 maximum{
        std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    for (const RenderMesh& mesh : scene.meshes) {
        for (Vec3 point : mesh.vertices) {
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            minimum.z = std::min(minimum.z, point.z);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
            maximum.z = std::max(maximum.z, point.z);
        }
    }
    const Vec3 center{
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f};
    const RenderCamera& camera = scene.camera;
    const Vec3 extent{
        maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z};
    const float diagonal = std::max(1.0f, length3(extent));
    const bool has_room = std::any_of(
        scene.meshes.begin(), scene.meshes.end(), [](const RenderMesh& mesh) {
            return mesh.name.startsWith("Room ", Qt::CaseInsensitive);
        });
    const int light_count = std::clamp(settings.light_count, 1, 8);
    const int shadow_samples = std::clamp(
        settings.soft_shadow_samples, 1, 16);
    const float light_radius = diagonal * static_cast<float>(std::clamp(
        settings.light_radius_fraction, 0.0, 0.30));
    const std::vector<RenderLight>& explicit_lights =
        settings.light_mode == RenderSettings::LightMode::Customize
            ? settings.custom_lights : scene.lights;
    if (!explicit_lights.empty()) {
        for (const RenderLight& source : explicit_lights) {
            if (!source.enabled) continue;
            RtLight light = rtLight(context);
            RtFlags flags = source.type == RenderLight::Type::Directional
                ? RT_DIR : source.type == RenderLight::Type::Spot
                    ? RT_SPOT : RT_OMNI;
            if (!source.casts_shadows) flags |= RT_NOSHADOW;
            if (source.attenuation_enabled) flags |= RT_ATTENUATION;
            rtLightType(light, flags);
            if (source.type != RenderLight::Type::Directional) {
                rtLightPos(light, source.position.x, source.position.y,
                           source.position.z);
            }
            if (source.type != RenderLight::Type::Omni) {
                rtLightDir(light, source.direction.x, source.direction.y,
                           source.direction.z);
            }
            if (source.type == RenderLight::Type::Spot) {
                rtLightCone(light, source.hotspot_radians,
                            source.falloff_radians);
            }
            rtLightColor(light, source.ambient.r, source.ambient.g,
                         source.ambient.b, RT_AMBIENT);
            rtLightColor(light, source.diffuse.r, source.diffuse.g,
                         source.diffuse.b, RT_DIFFUSE);
            rtLightColor(light, source.specular.r, source.specular.g,
                         source.specular.b, RT_SPECULAR);
            if (source.attenuation_enabled) {
                rtLightAtten(light, source.range,
                    source.constant_attenuation,
                    source.linear_attenuation,
                    source.quadratic_attenuation);
            }
            rtLightSize(light, std::max(0.0f, source.size));
            rtLightDensity(light, std::clamp(
                source.shadow_density, 0.0f, 1.0f));
        }
    } else {
    for (int index = 0; index < light_count; ++index) {
        const float angle = static_cast<float>(index) * 6.28318530718f / light_count;
        const float radius_x = (maximum.x - minimum.x) * 0.28f;
        const float radius_y = (maximum.y - minimum.y) * 0.28f;
        Vec3 base_position;
        float light_weight = 1.0f / static_cast<float>(light_count);
        float ambient_weight = light_weight;
        float size_scale = 1.0f;
        float density_scale = 1.0f;
        Color diffuse_tint{1.0f, 0.88f, 0.74f};
        Color specular_tint{1.0f, 0.94f, 0.86f};
        const bool three_light_room = has_room && light_count == 3;
        if (three_light_room) {
            // Match the classic Dom3D three-light room setup while keeping
            // every source inside the walls so ray-traced shadows cannot
            // block the fill. Positions scale with the room dimensions.
            const std::array<Vec3, 3> positions{{
                {center.x + extent.x * 0.18f,
                 center.y + extent.y * 0.06f,
                 minimum.z + extent.z * 0.91f},
                {center.x - extent.x * 0.28f,
                 center.y - extent.y * 0.24f,
                 minimum.z + extent.z * 0.76f},
                {center.x - extent.x * 0.34f,
                 center.y + extent.y * 0.28f,
                 minimum.z + extent.z * 0.60f}
            }};
            constexpr std::array<float, 3> weights{0.45f, 0.33f, 0.22f};
            constexpr std::array<float, 3> ambient_weights{0.42f, 0.36f, 0.22f};
            constexpr std::array<float, 3> size_scales{1.0f, 1.3f, 0.9f};
            constexpr std::array<float, 3> density_scales{1.0f, 0.82f, 0.68f};
            constexpr std::array<Color, 3> diffuse_tints{{
                {1.00f, 0.86f, 0.72f},
                {0.90f, 0.95f, 1.00f},
                {0.70f, 0.64f, 0.56f}
            }};
            constexpr std::array<Color, 3> specular_tints{{
                {1.00f, 0.94f, 0.86f},
                {0.94f, 0.97f, 1.00f},
                {0.62f, 0.58f, 0.54f}
            }};
            base_position = positions[static_cast<size_t>(index)];
            light_weight = weights[static_cast<size_t>(index)];
            ambient_weight = ambient_weights[static_cast<size_t>(index)];
            size_scale = size_scales[static_cast<size_t>(index)];
            density_scale = density_scales[static_cast<size_t>(index)];
            diffuse_tint = diffuse_tints[static_cast<size_t>(index)];
            specular_tint = specular_tints[static_cast<size_t>(index)];
        } else if (has_room) {
            base_position = {
                center.x + std::cos(angle) * radius_x,
                center.y + std::sin(angle) * radius_y,
                maximum.z - std::max(
                    120.0f, (maximum.z - minimum.z) * 0.08f)};
        } else {
            // A furniture-only scene has no empty ceiling volume. Put a
            // compact studio rig on the camera side of the model; placing the
            // old ceiling lights at the bounds center hid them inside cabinets.
            const float centered = light_count == 1 ? 0.0f
                : (static_cast<float>(index) / (light_count - 1) - 0.5f);
            base_position = center
                - camera.forward * (diagonal * 0.45f)
                + camera.up * (diagonal * (0.22f + 0.08f * std::cos(angle)))
                + camera.right * (diagonal * centered * 0.65f);
        }
        RtLight light = rtLight(context);
        rtLightType(light, RT_OMNI);
        rtLightPos(light, base_position.x, base_position.y, base_position.z);
        rtLightSize(light, light_radius * size_scale);
        rtLightDensity(light, static_cast<float>(std::clamp(
            settings.shadow_density * density_scale, 0.0, 1.0)));
        const float ambient = static_cast<float>(
            settings.ambient_strength) * ambient_weight;
        const float strength = static_cast<float>(
            settings.light_strength) * light_weight;
        rtLightColor(light, ambient, ambient, ambient, RT_AMBIENT);
        rtLightColor(light, strength * diffuse_tint.r,
                     strength * diffuse_tint.g,
                     strength * diffuse_tint.b, RT_DIFFUSE);
        rtLightColor(light, strength * specular_tint.r,
                     strength * specular_tint.g,
                     strength * specular_tint.b, RT_SPECULAR);
    }
    }

    rtBackColor(context, scene.environment.background_color.x,
                scene.environment.background_color.y,
                scene.environment.background_color.z);
    rtViewport(context, 0, 0,
               std::clamp(settings.width, 64, 16384),
               std::clamp(settings.height, 64, 16384));
    rtDepth(context, std::clamp(settings.reflection_depth, 1, 16));
    rtAntiAliasLevel(context, std::clamp(settings.anti_alias_level, 1, 16));
    rtShadowAntiAliasLevel(context, shadow_samples);
    rtTexAntiAliasLevel(context, 2);
    rtNumPasses(context, std::clamp(settings.progressive_passes, 1, 16));
    rtThreads(context, std::max(0, settings.thread_count));

    const float view[16] = {
        camera.right.x, camera.up.x, -camera.forward.x, 0.0f,
        camera.right.y, camera.up.y, -camera.forward.y, 0.0f,
        camera.right.z, camera.up.z, -camera.forward.z, 0.0f,
        -dot3(camera.right, camera.position),
        -dot3(camera.up, camera.position),
        dot3(camera.forward, camera.position), 1.0f};
    rtLoadModelView(context, view);
    const float aspect = static_cast<float>(settings.width)
        / std::max(1, settings.height);
    if (camera.orthographic) {
        const float half_height = std::max(
            0.05f, camera.orthographic_scale_mm * 0.5f);
        const float half_width = half_height * aspect;
        constexpr float near_plane = 1.0f;
        constexpr float far_plane = 10000000.0f;
        const float projection[16] = {
            1.0f / half_width, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f / half_height, 0.0f, 0.0f,
            0.0f, 0.0f, -2.0f / (far_plane - near_plane), 0.0f,
            0.0f, 0.0f,
            -(far_plane + near_plane) / (far_plane - near_plane), 1.0f};
        rtLoadProjection(context, projection);
    } else {
        rtIdentity(rtProjection(context));
        rtuPerspective(rtProjection(context),
            camera.vertical_fov_degrees * 3.14159265358979323846f / 180.0f,
            aspect, 1.0f, 10000000.0f);
    }

    if (!rtCompile(context, checkpoint, &checkpoint_data)
        || !rtRender(context, checkpoint, &checkpoint_data)) {
        const bool canceled = cancel && cancel->load(std::memory_order_relaxed);
        cleanup();
        if (error) *error = canceled ? "Render canceled."
                                     : "Native raytrace failed.";
        return false;
    }

    RtPixBuf pixels = rtPixelBuffer(context);
    if (!pixels || !pixels->bits || pixels->w == 0 || pixels->h == 0) {
        cleanup();
        if (error) *error = "Native raytrace returned an empty image.";
        return false;
    }
    QImage result = pixel_buffer_image(pixels, settings.exposure_ev);
    if (checkpoint_data.progress) {
        checkpoint_data.progress(100, result, "Finished");
    }
    cleanup();
    *image = std::move(result);
    return true;
}
