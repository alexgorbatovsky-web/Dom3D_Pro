#pragma once

#include "../Common.h"
#include "../Material.h"

#include <QString>

#include <array>
#include <vector>

class CAlfaDoc;
class CMesh3D;

struct RenderTriangle {
    std::array<int, 3> vertices{};
    std::array<UV, 3> uvs{};
    std::array<Vec3, 3> normals{};
};

struct RenderMesh {
    QString name;
    int material_index = -1;
    // An imported SurfaceSet is tessellated as independent CAD patches.
    // Their UV frames do not form one continuous tangent space.
    bool independent_surface_patch = false;
    std::vector<Vec3> vertices;
    std::vector<RenderTriangle> triangles;
};

struct RenderCamera {
    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    bool orthographic = false;
    float vertical_fov_degrees = 50.0f;
    float orthographic_scale_mm = 1000.0f;
};

struct RenderEnvironment {
    bool enabled = false;
    QString hdri_path;
    float strength = 1.0f;
    float rotation_degrees = 0.0f;
    Vec3 background_color{0.055f, 0.065f, 0.080f};
};

struct RenderLight {
    enum class Type { Omni, Spot, Directional };
    Type type = Type::Omni;
    bool enabled = true;
    Vec3 position{};
    Vec3 direction{0.0f, 0.0f, -1.0f};
    Color ambient{};
    Color diffuse{1.0f, 1.0f, 1.0f};
    Color specular{1.0f, 1.0f, 1.0f};
    float hotspot_radians = 0.65f;
    float falloff_radians = 0.9f;
    float spot_exponent = 0.0f;
    float range = 0.0f;
    float constant_attenuation = 1.0f;
    float linear_attenuation = 0.0f;
    float quadratic_attenuation = 0.0f;
    float size = 0.0f;
    float shadow_density = 1.0f;
    bool casts_shadows = true;
    bool attenuation_enabled = false;
};

struct RenderSettings {
    enum class Device { Auto, CPU, GPU };
    enum class LightingPreset { Interior, Exterior, Studio };
    enum class LightMode { Auto, Customize };
    int width = 1280;
    int height = 720;
    int samples = 64;
    double noise_threshold = 0.05;
    bool denoise = true;
    bool transparent_background = false;
    Device device = Device::Auto;
    LightingPreset lighting_preset = LightingPreset::Interior;
    QString output_file;
    QString view_transform = "AgX";
    QString look = "Medium High Contrast";
    double exposure = -1.0;
    double environment_strength = 0.15;
    double interior_light_strength = 0.45;
    LightMode light_mode = LightMode::Auto;
    std::vector<RenderLight> custom_lights;
};

struct RenderScene {
    std::vector<RenderMesh> meshes;
    std::vector<Material> materials;
    std::vector<RenderLight> lights;
    RenderCamera camera;
    RenderEnvironment environment;

    int TriangleCount() const;
    bool SaveJson(const QString& path,
                  const RenderSettings& settings,
                  QString* error = nullptr) const;
};

RenderScene BuildRenderScene(const CAlfaDoc& document,
                             const Camera& camera,
                             bool orthographic,
                             Vec3 background_color = {0.055f, 0.065f, 0.080f});
