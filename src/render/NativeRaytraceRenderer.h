#pragma once

#include "RenderScene.h"

#include <QImage>
#include <QString>

#include <atomic>
#include <functional>

struct NativeRaytraceSettings {
    int width = 1280;
    int height = 720;
    int reflection_depth = 4;
    int anti_alias_level = 2;
    int progressive_passes = 5;
    int thread_count = 0;
    int light_count = 3;
    int soft_shadow_samples = 4;
    double light_strength = 0.62;
    double light_radius_fraction = 0.06;
    double shadow_density = 1.0;
    double ambient_strength = 0.10;
    double exposure_ev = 1.25;
    RenderSettings::LightMode light_mode = RenderSettings::LightMode::Auto;
    std::vector<RenderLight> custom_lights;
};

class NativeRaytraceRenderer {
public:
    using ProgressCallback = std::function<void(
        int percent, const QImage& preview, const QString& stage)>;

    static bool Render(const RenderScene& scene,
                       const NativeRaytraceSettings& settings,
                       QImage* image,
                       QString* error = nullptr,
                       std::atomic_bool* cancel = nullptr,
                       ProgressCallback progress = {});
};
