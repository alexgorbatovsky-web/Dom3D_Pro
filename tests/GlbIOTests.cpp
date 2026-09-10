#include "GlbIO.h"
#include "CMesh3D.h"
#include "CAlfaDoc.h"
#include "render/RenderScene.h"

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtEndian>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
void require_glb(bool condition, const std::string& message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
bool near_glb(float a, float b);
}

void TestGlbExport(const QString& directory) {
    GlbIO io;std::string error;CAlfaDoc document;
    const QString color_path=directory+QString::fromUtf8("/дерево.png");
    const QString rough_path=directory+"/rough.png",metal_path=directory+"/metal.png";
    QImage image(4,4,QImage::Format_RGB32);image.fill(QColor(40,90,170));require_glb(image.save(color_path),"Cannot save color map");
    image.fill(QColor(67,67,67));require_glb(image.save(rough_path),"Cannot save roughness map");
    image.fill(QColor(149,149,149));require_glb(image.save(metal_path),"Cannot save metallic map");
    auto panel=std::make_unique<CMesh3D>("Oak facade");
    CMesh3D::Face face;face.corners={{0,0,0},{1,0,1},{2,0,2},{3,0,3}};
    require_glb(panel->SetGeometry({{100,200,300},{1100,200,300},{1100,700,300},{100,700,300}},
        {face},{{0,0},{1,0},{1,1},{0,1}},{{0,0,1}}),"Cannot build kitchen panel fixture");
    Material m=Material::DefaultSurface();m.name="Oak with PBR";m.id=0;m.diffuse={.4f,.5f,.6f};m.alpha=.7f;
    m.color_texture_path=QFileInfo(color_path).fileName().toStdString();m.source_file_path=(directory+"/source.d3mat").toStdString();
    m.roughness_texture_path=rough_path.toStdString();m.metallic_texture_path=metal_path.toStdString();
    m.texture_rotation_degrees=90;m.texture_scale_u=2;m.texture_offset_v=.25f;
    panel->SetMaterial(document.UpsertMaterial(m));panel->SetGroupName("Kitchen / Cabinet 01");
    CMesh3D* mesh=panel.get();
    auto hidden=panel->Clone();hidden->SetVisible(false);
    document.AddMesh(std::move(panel));document.AddObject(std::move(hidden));
    const auto source=BuildRenderScene(document,Camera{},false);
    const QString target=directory+QString::fromUtf8("/кухня.glb");
    require_glb(io.Export(target.toStdString(),document,error),error);
    QFile file(target);require_glb(file.open(QIODevice::ReadOnly),"Cannot read GLB export");const QByteArray bytes=file.readAll();file.close();
    require_glb(bytes.startsWith("glTF")&&qFromLittleEndian<quint32>(bytes.constData()+8)==bytes.size(),"Invalid GLB export header");
    const auto root=QJsonDocument::fromJson(bytes.mid(20,qFromLittleEndian<quint32>(bytes.constData()+12))).object();
    require_glb(root["meshes"].toArray().size()==1,"Hidden GLB objects must not be exported");
    require_glb(root["images"].toArray().size()==2,"GLB must embed base color and packed metallic/roughness");
    for(const auto& entry:root["images"].toArray())require_glb(entry.toObject().contains("bufferView")&&!entry.toObject().contains("uri"),"GLB texture is external");
    QFile::remove(color_path);QFile::remove(rough_path);QFile::remove(metal_path);
    std::vector<std::unique_ptr<CMesh3D>> imported;
    require_glb(io.Import(target.toStdString(),imported,error)&&imported.size()==1,error);
    Vec3 low,high;require_glb(imported[0]->GetBounds(low,high)&&near_glb(low.x,100)&&near_glb(low.y,200)&&near_glb(low.z,300)
        &&near_glb(high.x,1100)&&near_glb(high.y,700),"GLB export meters or Y-up conversion failed");
    require_glb(imported[0]->GetGroupName().find("Cabinet 01")!=std::string::npos,"GLB kitchen grouping lost");
    const auto& uv=imported[0]->GetUVs()[imported[0]->GetFaces()[0].corners[0].uv];
    require_glb(near_glb(uv.u,source.meshes[0].triangles[0].uvs[0].u)&&near_glb(uv.v,source.meshes[0].triangles[0].uvs[0].v),"GLB texture placement was not preserved");
    const Material restored=imported[0]->GetMaterial();
    require_glb(near_glb(restored.alpha,.7f)&&near_glb(restored.diffuse.b,.6f),"GLB base color/opacity failed");
    const QImage color(QString::fromStdString(restored.color_texture_path)),rough(QString::fromStdString(restored.roughness_texture_path)),metal(QString::fromStdString(restored.metallic_texture_path));
    require_glb(!color.isNull()&&qBlue(color.pixel(0,0))==170&&!rough.isNull()&&qRed(rough.pixel(0,0))==67
        &&!metal.isNull()&&qRed(metal.pixel(0,0))==149,"GLB textures did not survive removal of source files");
    // Missing maps, cancellation and empty scenes must preserve an existing export.
    require_glb(!io.Export(target.toStdString(),document,error)&&error.find("Missing texture")!=std::string::npos,"Missing texture must not silently disappear");
    require_glb(!io.Export(target.toStdString(),document,error,[](int,const std::string&){return false;}),"Export cancellation ignored");
    CAlfaDoc empty;require_glb(!io.Export(target.toStdString(),empty,error),"Empty GLB export must fail");
    file.open(QIODevice::ReadOnly);require_glb(file.readAll()==bytes,"Failed export overwrote the existing GLB");file.close();

    // Exercise the same GPU procedural shaders as the viewport on a small
    // planar cabinet panel; validate that real maps, UVs and geometry survive.
    m.color_texture_path.clear();m.roughness_texture_path.clear();m.metallic_texture_path.clear();
    m.fabric=FabricPreset(0);m.fabric.threadSize=4;m.fabric.scale=2;mesh->SetMaterial(m);
    require_glb(io.Export(target.toStdString(),document,error),error);
    imported.clear();require_glb(io.Import(target.toStdString(),imported,error),error);
    const auto baked=imported[0]->GetMaterial();
    require_glb(!baked.color_texture_path.empty()&&!baked.normal_texture_path.empty()&&!baked.roughness_texture_path.empty(),"Procedural GLB maps missing");
    QImage atlas(QString::fromStdString(baked.color_texture_path));
    require_glb(atlas.width()==2048,"Procedural GLB atlas missing");
    const QRgb pixel=atlas.pixel(atlas.width()/4,atlas.height()*3/4);
    require_glb(qRed(pixel)>0&&qBlue(pixel)>qRed(pixel),"Procedural bake lost its color");
    m.fabric.enabled=false;m.plaster.enabled=true;mesh->SetMaterial(m);
    require_glb(io.Export(target.toStdString(),document,error),error);
    imported.clear();require_glb(io.Import(target.toStdString(),imported,error),error);
    require_glb(!imported[0]->GetMaterial().normal_texture_path.empty(),"Fine Plaster normal map missing");
    require_glb(mesh->GetUVs().size()==4&&near_glb(mesh->GetUVs()[1].u,1),"GLB baking changed the source UVs");
    std::cout<<"GLB export: self-contained textures, placement, PBR, groups, visibility, atomic failures and procedural baking passed.\n";
}
namespace {
bool near_glb(float a, float b) { return std::abs(a-b)<.02f; }
void u32(QByteArray& bytes, quint32 value) {
    char data[4]; qToLittleEndian(value,data); bytes.append(data,4);
}
void floats(QByteArray& bytes, std::initializer_list<float> values) {
    for (float value:values) {quint32 bits;std::memcpy(&bits,&value,4);u32(bytes,bits);}
}
void write_glb(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    require_glb(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size(),"Cannot write GLB fixture");
}
QByteArray fixture(bool singular_instance=false) {
    QByteArray bin;
    floats(bin,{0,0,0, 1,0,0, 0,1,0});
    floats(bin,{0,0,1, 0,0,1, 0,0,1});
    floats(bin,{0,0, 1,0, 0,1});
    bin.append(QByteArray::fromHex("0000010002000000"));
    QImage image(2,2,QImage::Format_RGBA8888);image.fill(QColor(17,83,191,255));
    QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);
    require_glb(image.save(&buffer,"PNG"),"Cannot create embedded GLB texture");
    bin.append(png);
    QByteArray json=R"({
      "asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
      "nodes":[{"name":"Assembly","translation":[1,2,3],"children":[1,2]},
        {"name":"Mirrored part","mesh":0,"translation":[0.1,0.2,0.3],"scale":[-2,3,4]},
        {"name":"Instance","mesh":0}],
      "meshes":[{"name":"Triangle","primitives":[
        {"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0},
        {"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":1}]}],
      "materials":[{"name":"Paint","alphaMode":"BLEND","pbrMetallicRoughness":{
        "baseColorFactor":[0.2,0.4,0.6,0.35],"roughnessFactor":0.7,"metallicFactor":0.8,
        "baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0}}},
        {"name":"Paint","alphaMode":"OPAQUE","pbrMetallicRoughness":{"baseColorFactor":[1,0,0,0.1]}}],
      "textures":[{"source":0}],"images":[{"bufferView":4,"mimeType":"image/png"}],
      "buffers":[{"byteLength":BIN_LENGTH}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":36},
        {"buffer":0,"byteOffset":72,"byteLength":24},
        {"buffer":0,"byteOffset":96,"byteLength":6},
        {"buffer":0,"byteOffset":104,"byteLength":PNG_LENGTH}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}]
    })";
    if (singular_instance)
        json.replace("\"name\":\"Instance\",\"mesh\":0", "\"name\":\"Instance\",\"mesh\":0,\"scale\":[0,0,0]");
    json.replace("BIN_LENGTH",QByteArray::number(bin.size()));json.replace("PNG_LENGTH",QByteArray::number(png.size()));
    while(json.size()%4)json.append(' ');
    while(bin.size()%4)bin.append('\0');
    QByteArray glb("glTF",4);u32(glb,2);u32(glb,static_cast<quint32>(28+json.size()+bin.size()));
    u32(glb,static_cast<quint32>(json.size()));u32(glb,0x4e4f534a);glb.append(json);
    u32(glb,static_cast<quint32>(bin.size()));u32(glb,0x004e4942);glb.append(bin);
    return glb;
}
}

void TestGlbIO(const QString& directory) {
    GlbIO io;std::string error;std::vector<std::unique_ptr<CMesh3D>> meshes;
    const QString path=directory+QString::fromUtf8("/стул.GLB");
    const QByteArray bytes=fixture();write_glb(path,bytes);
    require_glb(io.Import(path.toStdString(),meshes,error),error);
    require_glb(meshes.size()==4,"GLB must preserve material primitives and node instances");
    Vec3 low,high;require_glb(meshes[0]->GetBounds(low,high),"GLB bounds missing");
    require_glb(near_glb(low.x,-900)&&near_glb(low.y,-3300)&&near_glb(low.z,2200)
        &&near_glb(high.x,1100)&&near_glb(high.z,5200),"GLB hierarchy, units, axes or mirror transform incorrect");
    require_glb(meshes[2]->GetBounds(low,high)&&near_glb(low.x,1000)&&near_glb(low.y,-3000)
        &&near_glb(low.z,2000),"GLB instance placement incorrect");
    require_glb(meshes[0]->GetGroupName().find("Mirrored part")!=std::string::npos,"GLB group name lost");
    const auto& mesh=*meshes[0];const auto& corners=mesh.GetFaces()[0].corners;
    const auto& vertices=mesh.GetVertices();
    const Vec3 a=vertices[corners[1].v]-vertices[corners[0].v];
    const Vec3 b=vertices[corners[2].v]-vertices[corners[0].v];
    require_glb(a.z*b.x-a.x*b.z<0,"GLB mirrored triangle winding incorrect");
    require_glb(!mesh.GetNormals().empty()&&near_glb(mesh.GetNormals()[0].y,-1),"GLB normal axis incorrect");
    require_glb(mesh.GetUVs().size()==3&&near_glb(mesh.GetUVs()[0].v,1),"GLB UV convention incorrect");
    const Material& m=mesh.GetMaterial();
    require_glb(m.id==0&&m.name=="Paint"&&near_glb(m.diffuse.g,.4f)&&near_glb(m.alpha,.35f)
        &&near_glb(m.roughness,.7f)&&near_glb(m.metallic,.8f),"GLB PBR factors lost");
    require_glb(near_glb(meshes[1]->GetMaterial().alpha,1),"GLB OPAQUE mode must ignore baseColor alpha");
    require_glb(meshes[1]->GetMaterial().name=="Paint (2)","GLB materials with identical names must remain distinct");
    QImage color(QString::fromStdString(m.color_texture_path));
    QImage rough(QString::fromStdString(m.roughness_texture_path));
    QImage metal(QString::fromStdString(m.metallic_texture_path));
    require_glb(!color.isNull()&&qRed(color.pixel(0,0))==17,"GLB embedded color image lost");
    require_glb(!rough.isNull()&&qRed(rough.pixel(0,0))==58&&qBlue(rough.pixel(0,0))==58,
        "GLB roughness must use green channel multiplied by its factor");
    require_glb(!metal.isNull()&&qRed(metal.pixel(0,0))==153&&qGreen(metal.pixel(0,0))==153,
        "GLB metallic must use blue channel multiplied by its factor");
    // Failed reads must leave the caller's existing scene intact.
    for (const QByteArray& invalid: {QByteArray("not GLB"),bytes.left(bytes.size()-8),QByteArray(),fixture(true)}) {
        write_glb(path,invalid);const size_t count=meshes.size();
        require_glb(!io.Import(path.toStdString(),meshes,error)&&!error.empty()&&meshes.size()==count,
            "Failed GLB import must report an error and preserve existing meshes");
    }
    std::cout<<"GLB hierarchy, instances, units, axes, mirrored normals, UVs, PBR, embedded textures and errors passed.\n";
}
