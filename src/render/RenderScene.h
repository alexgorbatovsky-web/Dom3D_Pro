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
    std::vector<Vec3> vertices;
    std::vector<RenderTriangle> triangles;
};

struct RenderCamera {
    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    bool orthographic = false;
    float vertical_fov_degrees = 48.0f;
    float orthographic_scale_mm = 1000.0f;
};

struct RenderEnvironment {
    bool enabled = false;
    QString hdri_path;
    float strength = 1.0f;
    float rotation_degrees = 0.0f;
    Vec3 background_color{0.055f, 0.065f, 0.080f};
};

struct RenderSettings {
    enum class Device { Auto, CPU, GPU };
    int width = 1280;
    int height = 720;
    int samples = 64;
    double noise_threshold = 0.05;
    bool denoise = true;
    bool transparent_background = false;
    Device device = Device::Auto;
    QString output_file;
    QString view_transform = "Standard";
    QString look = "Medium High Contrast";
    double exposure = 0.0;
};

struct RenderScene {
    std::vector<RenderMesh> meshes;
    std::vector<Material> materials;
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

