#include "MaterialLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    MaterialLibrary library;
    require(library.Load(MaterialLibrary::DefaultLibraryPath()),
            "Material library could not be loaded.");

    int ambient_cg_count = 0;
    int powder_coating_count = 0;
    int oracal_count = 0;
    for (const MaterialLibrary::Entry& entry : library.Entries()) {
        if (entry.category == "Powder Coating") {
            ++powder_coating_count;
            const Material& material = entry.material;
            require(material.metallic < 0.001f,
                    "Powder coating must be a dielectric coating, not bare metal.");
            require(material.roughness > 0.5f,
                    "Default powder coating must have a matte PBR response.");
            require(QFileInfo::exists(
                        QDir(MaterialLibrary::DefaultLibraryPath()).filePath(
                            QString::fromStdString(material.normal_texture_path))),
                    "Powder coating normal map was not packaged.");
        } else if (entry.category == "Oracal Film") {
            ++oracal_count;
            require(entry.material.color_texture_path.empty(),
                    "Oracal colour must come from RAL, not a baked colour texture.");
            require(entry.material.metallic < 0.001f,
                    "Oracal film must use a dielectric plastic response.");
        }
        if (!entry.category.startsWith("PBR ")) continue;
        ++ambient_cg_count;
        const Material& material = entry.material;
        require(QFileInfo::exists(QString::fromStdString(material.color_texture_path)),
                "PBR color map was not detected.");
        require(QFileInfo::exists(QString::fromStdString(material.normal_texture_path)),
                "PBR OpenGL normal map was not detected.");
        require(QFileInfo::exists(QString::fromStdString(material.roughness_texture_path)),
                "PBR roughness map was not detected.");
        require(QFileInfo::exists(QString::fromStdString(material.displacement_texture_path)),
                "PBR displacement map was not detected.");
    }
    require(ambient_cg_count >= 3,
            "The three ambientCG PBR packages were not discovered.");
    require(powder_coating_count == 24,
            "The powder coating RAL palette must contain 24 presets.");
    require(oracal_count == 2,
            "Both matte and translucent glossy Oracal presets are required.");

    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary material directory was not created.");
    Material source = Material::DefaultWhite();
    source.name = "PBR persistence";
    source.roughness = 0.27f;
    source.metallic = 0.63f;
    source.normal_strength = 1.4f;
    source.displacement_scale = 0.045f;
    source.normal_texture_path = "normal.png";
    source.roughness_texture_path = "roughness.png";
    source.metallic_texture_path = "metallic.png";
    source.displacement_texture_path = "height.png";
    const QString path = temporary.filePath("pbr.d3mat");
    QString error;
    require(library.SaveMaterial(path, source, &error),
            "PBR material could not be saved.");
    Material restored;
    require(library.LoadMaterial(path, restored, &error),
            "PBR material could not be loaded.");
    require(std::fabs(restored.roughness - source.roughness) < 0.0001f
                && std::fabs(restored.metallic - source.metallic) < 0.0001f
                && restored.normal_texture_path == source.normal_texture_path
                && restored.displacement_texture_path == source.displacement_texture_path,
            "PBR material fields did not survive a save/load round-trip.");

    std::cout << "PBR material discovery and persistence passed.\n";
    return EXIT_SUCCESS;
}
