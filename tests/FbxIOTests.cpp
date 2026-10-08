#include "FbxIO.h"
#include "CMesh3D.h"
#include "FbxSharpEdges.h"

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

const fbxsharp::Node& child_fbx(const fbxsharp::Node& parent, const char* name) {
    for (const auto& child : parent.children) if(child.name == name) return child;
    require_fbx(false, std::string("Missing FBX node: ")+name);
    return parent;
}
std::vector<qint32> integers_fbx(const fbxsharp::Node& node) {
    const auto& p=node.properties;
    require_fbx(p.size()>=13 && p[0]=='i' && qFromLittleEndian<quint32>(p.constData()+5)==0,
        "Expected an uncompressed FBX integer array");
    const auto count=qFromLittleEndian<quint32>(p.constData()+1);
    require_fbx(p.size()==13+qint64(count)*4,"Invalid FBX array length");
    std::vector<qint32> result;
    for(quint32 i=0;i<count;++i) result.push_back(qFromLittleEndian<qint32>(p.constData()+13+4*i));
    return result;
}
void test_sharp_fbx(const QString& directory) {
    CAlfaDoc doc;
    // Three bent quads: one hard join and one smooth join. Face-varying UVs
    // exercise normal/UV seams without splitting the shared control points.
    auto mesh=std::make_unique<CMesh3D>("Sharp strip");
    const std::vector<Vec3> vertices{{0,0,0},{10,0,0},{10,10,0},{0,10,0},
        {10,0,10},{10,10,10},{20,0,20},{20,10,20}};
    std::vector<CMesh3D::Face> faces(3);
    faces[0].corners={{0,0,0},{1,0,1},{2,0,2},{3,0,3}};
    faces[1].corners={{2,0,4},{1,0,5},{4,0,6},{5,0,7}};
    faces[2].corners={{5,0,8},{4,0,9},{6,0,10},{7,0,11}};
    std::vector<UV> uv(12);
    for(size_t i=0;i<uv.size();++i) uv[i]={float(i)/12.0f,float(i%4)/4.0f};
    require_fbx(mesh->SetGeometry(vertices,faces,uv),"Cannot create sharp FBX fixture");
    require_fbx(mesh->AddSharpEdge(1,2),"Cannot mark sharp FBX edge");
    const auto* source=mesh.get(); doc.AddMesh(std::move(mesh));
    // A second mesh checks that the completion step keeps geometry ordering.
    auto second=std::make_unique<CMesh3D>(*source); second->SetName("Second sharp strip");
    require_fbx(second->AddSharpEdge(4,5),"Cannot mark second sharp FBX edge");
    doc.AddMesh(std::move(second));
    auto plain=std::make_unique<CMesh3D>(*source); plain->ClearSharpEdges(); plain->SetName("Plain strip");
    doc.AddMesh(std::move(plain));
    // CAD tessellation can store each surface with its own boundary indices.
    // FBX must weld these positions even when Sharp layers are present.
    auto cube=std::make_unique<CMesh3D>("Split CAD cube");
    const std::vector<Vec3> cube_points{{0,0,0},{10,0,0},{10,10,0},{0,10,0},
        {0,0,10},{10,0,10},{10,10,10},{0,10,10}};
    const size_t cube_faces[6][4]={{3,2,1,0},{4,5,6,7},{0,1,5,4},
        {1,2,6,5},{2,3,7,6},{3,0,4,7}};
    std::vector<Vec3> split_points; std::vector<CMesh3D::Face> split_faces;
    for(const auto& indices:cube_faces) {
        CMesh3D::Face face;
        for(size_t index:indices) {
            face.corners.push_back({split_points.size(),0,0});
            split_points.push_back(cube_points[index]);
        }
        split_faces.push_back(std::move(face));
    }
    require_fbx(cube->SetGeometry(split_points,split_faces),"Cannot create split CAD cube");
    for(size_t i=0;i<24;++i)
        require_fbx(cube->AddSharpEdge(i,(i/4)*4+(i+1)%4),"Cannot mark CAD cube edge");
    doc.AddMesh(std::move(cube));
    FbxIO io; std::string error;
    const QString path=directory+"/sharp-edges.fbx";
    require_fbx(io.Export(path.toStdString(),doc,error),error);
    QFile file(path); require_fbx(file.open(QIODevice::ReadOnly),"Cannot read sharp FBX fixture");
    const auto bytes=file.readAll(); qsizetype pos=27; size_t found=0;
    while(qFromLittleEndian<quint64>(bytes.constData()+pos)!=0) {
        const auto node=fbxsharp::Read(bytes,pos,quint64(bytes.size()));
        if(node.name!="Objects") continue;
        for(const auto& geometry:node.children) if(geometry.name=="Geometry") {
            if(found>=2) { ++found; continue; }
            const auto& positions=child_fbx(geometry,"Vertices").properties;
            require_fbx(qFromLittleEndian<quint32>(positions.constData()+1)==vertices.size()*3,
                "FBX sharp/UV seams split shared control points");
            const auto polygons=integers_fbx(child_fbx(geometry,"PolygonVertexIndex"));
            const auto edges=integers_fbx(child_fbx(geometry,"Edges"));
            const auto& layer=child_fbx(geometry,"LayerElementSmoothing");
            require_fbx(child_fbx(layer,"MappingInformationType").properties.endsWith("ByEdge"),"FBX smoothing is not ByEdge");
            require_fbx(child_fbx(layer,"ReferenceInformationType").properties.endsWith("Direct"),"FBX smoothing is not Direct");
            const auto smooth=integers_fbx(child_fbx(layer,"Smoothing"));
            require_fbx(edges.size()==10 && smooth.size()==edges.size(),"FBX edge table has incorrect size");
            const auto& creases=child_fbx(child_fbx(geometry,"LayerElementEdgeCrease"),"EdgeCrease").properties;
            require_fbx(qFromLittleEndian<quint32>(creases.constData()+1)==edges.size(),"FBX crease count mismatch");
            size_t hard=0;
            for(size_t e=0;e<edges.size();++e) {
                const size_t start=size_t(edges[e]);
                require_fbx(start<polygons.size(),"FBX edge index out of range");
                const auto decode=[](qint32 v){return v<0 ? ~v : v;};
                const size_t next=start%4==3 ? start-3 : start+1;
                const auto a=decode(polygons[start]), b=decode(polygons[next]);
                const auto pair=std::minmax(a,b);
                const bool expected=(pair.first==1 && pair.second==2)
                    || (found==1 && pair.first==4 && pair.second==5);
                require_fbx(smooth[e]==(expected?0:1),"FBX sharp or smooth edge lost, or triangulation diagonal marked sharp");
                const quint64 bits=qFromLittleEndian<quint64>(creases.constData()+13+8*e);
                double crease; std::memcpy(&crease,&bits,8);
                require_fbx(crease==(expected?1.0:0.0),"FBX subdivision crease mismatch");
                hard+=smooth[e]==0;
            }
            require_fbx(hard==found+1,"FBX sharp edge count mismatch"); ++found;
        }
    }
    require_fbx(found==4,"FBX geometry ordering changed");
    std::vector<std::unique_ptr<CMesh3D>> imported;
    require_fbx(io.Import(path.toStdString(),imported,error),error);
    require_fbx(imported.size()==4 && imported[0]->GetFaces().size()==6,"Completed FBX is not readable");
    require_fbx(source->GetVertices().size()==8 && source->GetSharpEdges().size()==1
        && source->GetNormals().empty(),"FBX export mutated source geometry");
}
}

void TestFbxIO(const QString& directory) {
    test_sharp_fbx(directory);
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
    require_fbx(mesh->AddSharpEdge(0,2),"Cannot mark textured FBX edge");
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
