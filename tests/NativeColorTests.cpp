#include "render/NativeRaytraceRenderer.h"
#include <cmath>
#include <iostream>
#include <cstdlib>

int TestNativeColors() {
    auto check = [](bool ok, const char* text) { if (!ok) { std::cerr << text << std::endl; std::exit(1); } };
    RenderScene scene;
    scene.camera.position = {0, 0, 1000};
    scene.camera.orthographic = true;
    scene.camera.orthographic_scale_mm = 400;
    Material material = Material::DefaultSurface();
    material.diffuse = {0.2f, 0.5f, 0.7f};
    material.ambient = {0, 0, 0};
    material.specular = 0;
    material.shininess = 128;
    material.reflectivity = 0;
    scene.materials.push_back(material);
    RenderMesh mesh;
    mesh.material_index = 0;
    mesh.vertices = {{-1000,-1000,0},{1000,-1000,0},{0,1000,0}};
    RenderTriangle triangle;
    triangle.vertices = {0,1,2};
    triangle.normals = {Vec3{0,0,1},Vec3{0,0,1},Vec3{0,0,1}};
    mesh.triangles.push_back(triangle);
    scene.meshes.push_back(mesh);
    NativeRaytraceSettings settings;
    settings.width = settings.height = 64;
    settings.progressive_passes = 1;
    settings.anti_alias_level = 1;
    settings.thread_count = 1;
    settings.light_mode = RenderSettings::LightMode::Customize;
    RenderLight light;
    light.type = RenderLight::Type::Directional;
    light.direction = {0,0,-1};
    light.diffuse = {0.6f,0.6f,0.6f};
    light.ambient = light.specular = {0,0,0};
    settings.custom_lights = {light};
    auto render = [&]() {
        QImage image; QString error;
        check(NativeRaytraceRenderer::Render(scene,settings,&image,&error),qPrintable(error));
        return image;
    };
    for (double exposure : {-2.0, 0.0, 1.25, 4.0}) {
        settings.exposure_ev = exposure;
        const QColor color = render().pixelColor(32,32);
        check(color.blue() > 10, "Directional light did not illuminate test surface");
        check(std::abs(color.red()*3.5-color.blue()) < 6,
              "Exposure changed red/blue ratio");
        check(std::abs(color.green()*1.4-color.blue()) < 6,
              "Exposure changed green/blue ratio");
    }
    settings.exposure_ev = 0;
    settings.custom_lights[0].direction = {-0.995f,0,0.1f};
    settings.custom_lights[0].casts_shadows = false;
    for (auto& normal : scene.meshes[0].triangles[0].normals) normal = {0.8f,0,0.6f};
    check(render().pixelColor(32,32).blue() > 70,
          "Flat triangle orientation clipped smooth-normal illumination");
    settings.custom_lights[0].direction = {0.995f,0,0.1f};
    check(render().pixelColor(32,32).blue() < 3,
          "Back-facing smooth normal received diffuse light");
    // A separate opaque surface must still cast a shadow.
    settings.custom_lights[0] = light;
    settings.custom_lights[0].direction = {-0.6f,0,-0.8f};
    for (auto& normal : scene.meshes[0].triangles[0].normals) normal = {0,0,1};
    const int lit = render().pixelColor(32,32).blue();
    RenderMesh blocker = mesh;
    blocker.vertices = {{40,-100,100},{110,-100,100},{75,100,100}};
    scene.meshes.push_back(blocker);
    const QImage shadow = render();
    check(shadow.pixelColor(32,32).blue() < lit/4, "Opaque blocker lost its shadow");
    settings.thread_count = 4;
    check(render() == shadow, "Native color render differs across thread counts");
    // Sloping silhouette: adaptive 2x2 should converge towards 8x8,
    // while preserving solid regions and the explicitly unsmoothed mode.
    scene.meshes = {mesh};
    scene.meshes[0].vertices = {{-180,-170,0},{190,-130,0},{-45,175,0}};
    scene.materials[0].ambient = {1,1,1};
    scene.environment.background_color = {0,0,0};
    settings.custom_lights[0].ambient = {1,1,1};
    settings.custom_lights[0].diffuse = {0,0,0};
    settings.anti_alias_level = 1;
    const QImage jagged = render();
    settings.anti_alias_level = 2;
    const QImage adaptive = render();
    settings.anti_alias_level = 8;
    const QImage reference = render();
    int jagged_error = 0, adaptive_error = 0, fractional = 0;
    const int white = reference.pixelColor(32,32).red();
    for (int y=0; y<64; ++y) for (int x=0; x<64; ++x) {
        const int target = reference.pixelColor(x,y).red();
        const int value = adaptive.pixelColor(x,y).red();
        jagged_error += std::abs(jagged.pixelColor(x,y).red()-target);
        adaptive_error += std::abs(value-target);
        if (value>0 && value<white) ++fractional;
    }
    check(fractional > 60 && adaptive_error < jagged_error/5,
          "Silhouette antialiasing did not converge towards 8x8 coverage");
    settings.anti_alias_level = 2;
    settings.thread_count = 1;
    settings.progressive_passes = 5;
    check(render() == adaptive, "Edge antialiasing depends on threads or preview passes");
    // Oblique orthographic views of small models used to quantize rays on
    // a world-space grid because the inverse float matrix spanned 10^7 units.
    scene.camera.position = {0,-600,800};
    scene.camera.forward = {0,0.6f,-0.8f};
    scene.camera.up = {0,0.8f,0.6f};
    settings.anti_alias_level = 4;
    const RenderScene unscaled = scene;
    const QImage full_scale = render();
    for (float scale : {0.01f, 100.0f}) {
        scene = unscaled;
        scene.camera.position = scene.camera.position * scale;
        scene.camera.orthographic_scale_mm *= scale;
        for (auto& object : scene.meshes)
            for (auto& vertex : object.vertices) vertex = vertex * scale;
        const QImage scaled = render();
        int error = 0;
        for (int y=0; y<64; ++y) for (int x=0; x<64; ++x)
            error += std::abs(scaled.pixelColor(x,y).red()
                              -full_scale.pixelColor(x,y).red());
        check(error < 64, "Camera precision changed silhouette with model scale");
    }
    std::cout << "Native colors, smooth lighting, occlusion and threads passed\n";
    return 0;
}
