#include "FbxIO.h"
#include "CMesh3D.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QString>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void require_fbx(bool condition, const std::string& message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
bool near_fbx(float a,float b) { return std::abs(a-b)<0.01f; }
}

void TestFbxIO(const QString& directory) {
    FbxIO io; std::string error;
    const QString texture_path=directory+QString::fromUtf8("/цвет.png");
    QImage texture(2,2,QImage::Format_RGBA8888); texture.fill(qRgba(40,180,90,255));
    require_fbx(texture.save(texture_path),"Could not create FBX texture fixture");

    CAlfaDoc document;
    auto mesh=std::make_unique<CMesh3D>("Placed quad");
    std::vector<Vec3> vertices{{100,200,300},{140,200,300},{140,260,300},{100,260,300}};
    CMesh3D::Face first; first.corners={{0,0,0},{1,0,1},{2,0,2}};
    CMesh3D::Face second; second.corners={{0,0,0},{2,0,2},{3,0,3}};
    std::vector<CMesh3D::Face> faces{first,second};
    std::vector<UV> uvs{{0,0},{1,0},{1,1},{0,1}};
    require_fbx(mesh->SetGeometry(vertices,faces,uvs),"Could not create FBX mesh fixture");
    Material material=Material::DefaultSurface(); material.id=0; material.name="Green lacquer";
    material.diffuse={0.1f,0.7f,0.3f}; material.ambient={0.02f,0.1f,0.04f};
    material.emission={0.01f,0.02f,0.03f}; material.alpha=0.65f; material.roughness=0.28f;
    material.metallic=0.45f; material.color_texture_path=texture_path.toStdString();
    mesh->SetMaterial(document.UpsertMaterial(material)); document.AddMesh(std::move(mesh));

    const QString file_path=directory+QString::fromUtf8("/обмен.fbx");
    require_fbx(io.Export(file_path.toStdString(),document,error),error);
    QFile output(file_path); require_fbx(output.open(QIODevice::ReadOnly),"Could not read exported FBX");
    require_fbx(output.read(21).startsWith("Kaydara FBX Binary"),"FBX export is not a binary FBX file");
    std::vector<std::unique_ptr<CMesh3D>> imported;
    require_fbx(io.Import(file_path.toStdString(),imported,error),error);
    require_fbx(imported.size()==1&&imported[0]->GetFaces().size()==2,"FBX mesh/faces round trip failed");
    require_fbx(imported[0]->GetUVs().size()>=4,"FBX texture coordinates round trip failed");
    Vec3 low,high; require_fbx(imported[0]->GetBounds(low,high),"Imported FBX has no bounds");
    require_fbx(near_fbx(low.x,100)&&near_fbx(low.y,200)&&near_fbx(low.z,300)
        &&near_fbx(high.x,140)&&near_fbx(high.y,260),"FBX units or node transform round trip failed");
    const Material restored=imported[0]->GetMaterial();
    require_fbx(restored.name=="Green lacquer"&&near_fbx(restored.diffuse.g,0.7f)
        &&near_fbx(restored.alpha,0.65f),"FBX material colour or opacity round trip failed");
    require_fbx(!restored.color_texture_path.empty()&&QFileInfo::exists(QString::fromStdString(restored.color_texture_path)),
        "FBX embedded texture round trip failed");

    const QDir external(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("data/fbx"));
    std::vector<std::unique_ptr<CMesh3D>> box;
    require_fbx(io.Import(external.filePath("box.fbx").toStdString(),box,error)&&!box.empty(),error);
    std::vector<std::unique_ptr<CMesh3D>> mirrored;
    require_fbx(io.Import(external.filePath("cubes_with_mirroring_and_pivot.fbx").toStdString(),mirrored,error)
        &&mirrored.size()>=2,"FBX mirrored hierarchy fixture failed: "+error);

    const size_t old_count=imported.size(); QFile broken(directory+"/broken.fbx");
    require_fbx(broken.open(QIODevice::WriteOnly),"Cannot create bad FBX fixture"); broken.write("not an FBX"); broken.close();
    require_fbx(!io.Import(broken.fileName().toStdString(),imported,error)&&imported.size()==old_count,
        "Failed FBX import changed the destination collection");
    std::cout<<"FBX binary geometry, units, node transform, material and embedded texture passed.\n";
}
