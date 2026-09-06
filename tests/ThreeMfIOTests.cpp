#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <lib3mf_implicit.hpp>
#include "ExchangeIO.h"
#include "CMesh3D.h"
#include "solid/Solid.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using namespace Lib3MF;
void require(bool condition, const std::string& message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
bool near_value(float a, float b) { return std::abs(a-b) < 0.001f; }
void save(const PModel& model, const QString& path) {
    std::vector<Lib3MF_uint8> bytes;
    model->QueryWriter("3mf")->WriteToBuffer(bytes);
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Could not create 3MF fixture");
    require(file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()) == qint64(bytes.size()), "Fixture write failed");
}
sTransform identity() {
    sTransform transform{};
    transform.m_Fields[0][0] = transform.m_Fields[1][1] = transform.m_Fields[2][2] = 1;
    return transform;
}
PMeshObject triangle(const PModel& model) {
    const auto mesh = model->AddMeshObject();
    mesh->SetName("Triangle");
    mesh->SetGeometry(std::vector<sPosition>{{{0,0,0}},{{1,0,0}},{{0,1,0}}},
                      std::vector<sTriangle>{{{0,1,2}}});
    return mesh;
}
}

void TestThreeMfIO(const QString& directory) {
    try {
    ThreeMfIO io;
    std::string error;
    const auto wrapper = Lib3MF::CWrapper::loadLibrary();
    const auto model = wrapper->CreateModel();
    model->SetUnit(eModelUnit::Inch);
    const auto palette = model->AddBaseMaterialGroup();
    const auto red = palette->AddMaterial("Plastic", {255,0,0,128});
    const auto blue = palette->AddMaterial("Plastic", {0,0,255,255});
    const auto mesh = triangle(model);
    mesh->SetObjectLevelProperty(palette->GetUniqueResourceID(), red);
    const auto assembly = model->AddComponentsObject();
    assembly->SetName("Rotated pair");
    auto rotate = identity();
    rotate.m_Fields[0][0] = 0; rotate.m_Fields[0][1] = 2;
    rotate.m_Fields[1][0] = -3; rotate.m_Fields[1][1] = 0;
    rotate.m_Fields[3][0] = 4;
    assembly->AddComponent(mesh.get(), rotate);
    const auto other = triangle(model);
    other->SetObjectLevelProperty(palette->GetUniqueResourceID(), blue);
    auto mirror = identity(); mirror.m_Fields[0][0] = -1;
    assembly->AddComponent(other.get(), mirror);
    auto move = identity(); move.m_Fields[3][1] = 10;
    model->AddBuildItem(assembly.get(), move);
    // Unused resources are not printable build items and must not be imported.
    triangle(model);
    const QString fixture = directory + "/transforms.3mf";
    save(model, fixture);
    std::vector<std::unique_ptr<CMesh3D>> imported;
    require(io.Import(fixture.toStdString(), imported, error), error);
    require(imported.size() == 2, "3MF must preserve instances and ignore unused resources");
    const auto& p = imported[0]->GetVertices();
    require(near_value(p[0].x,101.6f) && near_value(p[0].y,254) && near_value(p[1].y,304.8f)
            && near_value(p[2].x,25.4f), "3MF nested matrix order or inch conversion is wrong");
    require(imported[0]->GetMaterial().name == "Plastic" && near_value(imported[0]->GetMaterial().diffuse.r,1)
            && near_value(imported[0]->GetMaterial().alpha,128.0f/255)
            && near_value(imported[1]->GetMaterial().diffuse.b,1), "3MF base material colours/opacity were lost");
    require(imported[1]->GetFaces()[0].normal.z > 0.9f,
            "Mirrored 3MF instance must reverse winding to keep outward normals");

    const auto mixed_model = wrapper->CreateModel();
    const auto mixed = mixed_model->AddMeshObject();
    mixed->SetName("Two materials");
    mixed->SetGeometry(std::vector<sPosition>{{{0,0,0}},{{1,0,0}},{{1,1,0}},{{0,1,0}}},
                       std::vector<sTriangle>{{{0,1,2}},{{0,2,3}}});
    const auto mixed_palette = mixed_model->AddBaseMaterialGroup();
    const auto first = mixed_palette->AddMaterial("Same name", {255,0,0,255});
    const auto second = mixed_palette->AddMaterial("Same name", {0,0,255,255});
    mixed->SetObjectLevelProperty(mixed_palette->GetUniqueResourceID(), first);
    mixed->SetTriangleProperties(0, {mixed_palette->GetUniqueResourceID(),{first,first,first}});
    mixed->SetTriangleProperties(1, {mixed_palette->GetUniqueResourceID(),{second,second,second}});
    mixed_model->AddBuildItem(mixed.get(), identity());
    save(mixed_model, directory + "/mixed.3mf");
    std::vector<std::unique_ptr<CMesh3D>> mixed_parts;
    require(io.Import((directory + "/mixed.3mf").toStdString(), mixed_parts, error), error);
    require(mixed_parts.size() == 2 && mixed_parts[0]->GetFaces().size() == 1
            && mixed_parts[1]->GetFaces().size() == 1
            && mixed_parts[0]->GetMaterial().diffuse.r != mixed_parts[1]->GetMaterial().diffuse.r,
            "Per-triangle 3MF materials with identical names must remain distinct");

    CAlfaDoc document;
    for (auto& item : imported) {
        auto material = item->GetMaterial();
        material.roughness = 0.23f; material.metallic = 0.7f;
        item->SetMaterial(document.UpsertMaterial(material));
        document.AddMesh(std::move(item));
    }
    const QString unicode_path = directory + QString::fromUtf8("/модель 日本.3mf");
    require(io.Export(unicode_path.toStdString(), document, error), error);
    imported.clear();
    require(io.Import(unicode_path.toStdString(), imported, error), error);
    require(imported.size() == 2 && near_value(imported[0]->GetMaterial().roughness,0.23f)
            && near_value(imported[0]->GetMaterial().metallic,0.7f)
            && near_value(imported[0]->GetVertices()[0].x,101.6f),
            "3MF round trip must preserve material parameters and world placement");
    const auto exported = wrapper->CreateModel();
    exported->QueryReader("3mf")->ReadFromFile(unicode_path.toStdString());
    require(exported->GetUnit() == eModelUnit::MilliMeter && exported->GetBuildItems()->Count() == 2,
            "3MF export must declare millimetres and retain separate objects");

    // Three corner colours exercise the Materials extension independently of
    // Dom3D's exporter; the importer renders their gradient as an embedded atlas.
    const auto color_model = wrapper->CreateModel();
    const auto color_mesh = triangle(color_model);
    const auto colors = color_model->AddColorGroup();
    const auto a = colors->AddColor({255,0,0,255});
    const auto b = colors->AddColor({0,255,0,255});
    const auto c = colors->AddColor({0,0,255,255});
    color_mesh->SetObjectLevelProperty(colors->GetUniqueResourceID(), a);
    color_mesh->SetTriangleProperties(0, {colors->GetUniqueResourceID(),{a,b,c}});
    color_model->AddBuildItem(color_mesh.get(), identity());
    const QString color_path = directory + "/colors.3mf";
    save(color_model, color_path);
    imported.clear();
    require(io.Import(color_path.toStdString(), imported, error), error);
    require(imported.size() == 1 && !imported[0]->GetMaterial().color_texture_path.empty(),
            "3MF vertex colour gradient must have a renderable texture");
    const QImage atlas(QString::fromStdString(imported[0]->GetMaterial().color_texture_path));
    require(atlas.pixelColor(1,1) == QColor(255,0,0) && atlas.pixelColor(30,1) == QColor(0,255,0)
            && atlas.pixelColor(1,30) == QColor(0,0,255), "Gradient colour corners were changed");

    // Export and reimport a textured quad: two triangles must remain one mesh,
    // keep their UVs, and carry the image inside the ZIP package.
    CAlfaDoc textured;
    auto quad = std::make_unique<CMesh3D>("Textured quad");
    require(quad->SetGeometry({{0,0,0},{1,0,0},{1,1,0},{0,1,0}},
                             {{0,1,2,3}}, {{0,0},{1,0},{1,1},{0,1}}), "Quad fixture failed");
    auto material = Material::ImportedMesh();
    material.name = "Texture"; material.diffuse = {0.5f,0.8f,1};
    material.color_texture_path = imported[0]->GetMaterial().color_texture_path;
    material.normal_texture_path = material.color_texture_path;
    material.texture_offset_u = 0.25f;
    quad->SetMaterial(textured.UpsertMaterial(material));
    textured.AddMesh(std::move(quad));
    const QString texture_path = directory + "/texture.3mf";
    require(io.Export(texture_path.toStdString(), textured, error), error);
    imported.clear();
    require(io.Import(texture_path.toStdString(), imported, error), error);
    require(imported.size() == 1 && imported[0]->GetFaces().size() == 2,
            "3MF texture coordinates must not split a mesh into one object per triangle");
    require(near_value(imported[0]->GetUVs()[0].u,0.25f)
            && !imported[0]->GetMaterial().normal_texture_path.empty(), "3MF UV or PBR image round trip failed");

    CAlfaDoc solids;
    TopoDS_Shape shape = BRepPrimAPI_MakeBox(10,20,30).Shape();
    auto box = std::make_unique<CSolid>(shape);
    require(box->EnsureRenderMesh(), "Cannot tessellate solid fixture");
    box->SetName("Box assembly");
    auto base = Material::DefaultSurface(); base.name = "Base blue"; base.diffuse = {0,0,1};
    box->SetMaterial(solids.UpsertMaterial(base));
    auto surface_material = base; surface_material.id = 0;
    surface_material.name = "Red face"; surface_material.diffuse = {1,0,0};
    require(box->SetSurfaceMaterial(0,surface_material), "Cannot set fixture surface material");
    solids.AddObject(std::move(box));
    const QString solid_path = directory + "/solid.3mf";
    require(io.Export(solid_path.toStdString(), solids, error), error);
    std::vector<std::unique_ptr<CMesh3D>> solid_parts;
    require(io.Import(solid_path.toStdString(), solid_parts, error), error);
    require(solid_parts.size() == 6, "Solid 3MF must preserve its six surface components");
    size_t red_faces = 0;
    for (const auto& surface : solid_parts) if (surface->GetMaterial().diffuse.r > 0.99f) ++red_faces;
    require(red_faces == 1, "Solid surface material override was lost in 3MF export");

    const size_t old_count = imported.size();
    QFile broken(directory + "/broken.3mf");
    require(broken.open(QIODevice::WriteOnly), "Cannot make invalid 3MF fixture");
    broken.write("not a ZIP package"); broken.close();
    require(!io.Import(broken.fileName().toStdString(), imported, error) && imported.size() == old_count,
            "Failed 3MF import must not modify the result collection");
    const QString safe_path = directory + "/existing.3mf";
    QFile existing(safe_path); require(existing.open(QIODevice::WriteOnly), "Cannot make export sentinel");
    existing.write("keep"); existing.close();
    CAlfaDoc empty;
    require(!io.Export(safe_path.toStdString(), empty, error), "Empty 3MF export must fail");
    require(existing.open(QIODevice::ReadOnly) && existing.readAll() == "keep", "Failed export overwrote the destination");
    const QDir external(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("data/3mf"));
    std::vector<std::unique_ptr<CMesh3D>> wagon;
    require(io.Import(external.filePath("WagonWithWheels.3mf").toStdString(), wagon, error), error);
    require(wagon.size() == 6, "Consortium wagon must contain a body, four wheels and a spare wheel");
    std::vector<std::unique_ptr<CMesh3D>> external_texture;
    require(io.Import(external.filePath("Texture.3mf").toStdString(), external_texture, error), error);
    require(!external_texture.empty() && !external_texture.front()->GetMaterial().color_texture_path.empty(),
            "Consortium texture sample must retain its packaged image");
    std::vector<std::unique_ptr<CMesh3D>> slicer;
    require(io.Import(external.filePath("SlicerMetadata.3mf").toStdString(), slicer, error), error);
    require(slicer.size() == 1, "Slicer package with external model part was not imported");
    Vec3 slicer_low, slicer_high;
    require(slicer.front()->GetBounds(slicer_low, slicer_high)
            && near_value(slicer_low.x,12) && near_value(slicer_low.y,26)
            && near_value(slicer_low.z,42) && near_value(slicer_high.x,14)
            && near_value(slicer_high.y,29) && near_value(slicer_high.z,42),
            "Slicer package component/build transforms were lost");
    const size_t slicer_count = slicer.size();
    require(!io.Import(external.filePath("SlicerInvalidIndex.3mf").toStdString(), slicer, error)
            && slicer.size() == slicer_count, "Invalid slicer triangle was accepted or changed output");
    require(!io.Import(external.filePath("SlicerMissingCoordinate.3mf").toStdString(), slicer, error)
            && slicer.size() == slicer_count, "Invalid slicer vertex was accepted or changed output");
    require(!io.Import(external.filePath("SlicerRequiredExtension.3mf").toStdString(), slicer, error)
            && slicer.size() == slicer_count, "Unsupported required 3MF extension was silently ignored");
    std::cout << "3MF materials, units, transforms, textures and atomic failures passed.\n";
    } catch (const std::exception& exception) {
        require(false, std::string("3MF fixture/test exception: ") + exception.what());
    }
}
