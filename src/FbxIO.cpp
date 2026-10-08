#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "FbxIO.h"
#include "GlbIO.h"
#include "CMesh3D.h"
#include "FbxSharpEdges.h"
#include "ObjSharpEdges.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <assimp/GltfMaterial.h>

#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <unordered_map>

namespace {
constexpr qint64 kMaxFileBytes = 1024ll * 1024 * 1024;
constexpr size_t kMaxTriangles = 10000000;

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

bool finite(const aiVector3D& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Color color(const aiColor3D& value) { return {value.r, value.g, value.b}; }

std::array<double, 16> dom_matrix(const aiMatrix4x4& m, bool glb) {
    if (glb) {
        // Right-handed Y-up meters -> right-handed Z-up millimeters.
        return {1000*m.a1,1000*m.a2,1000*m.a3,1000*m.a4,
                -1000*m.c1,-1000*m.c2,-1000*m.c3,-1000*m.c4,
                1000*m.b1,1000*m.b2,1000*m.b3,1000*m.b4,
                m.d1,m.d2,m.d3,m.d4};
    }
    // Assimp's FBX importer normalizes every source unit into centimeters.
    // Dom3D geometry is millimeters, so the world transform includes cm -> mm.
    return {10*m.a1,10*m.a2,10*m.a3,10*m.a4,
            10*m.b1,10*m.b2,10*m.b3,10*m.b4,
            10*m.c1,10*m.c2,10*m.c3,10*m.c4,
            m.d1,m.d2,m.d3,m.d4};
}

std::string stored_texture(const aiScene& scene, const aiString& source,
                           const QString& source_directory, bool glb) {
    const std::string name = source.C_Str();
    if (name.empty()) return {};
    const aiTexture* embedded = scene.GetEmbeddedTexture(name.c_str());
    if (!embedded) {
        QString candidate = QString::fromUtf8(name.c_str());
        candidate.replace('\\', '/');
        if (QDir::isRelativePath(candidate)) candidate = QDir(source_directory).filePath(candidate);
        const QFileInfo info(candidate);
        return info.exists() ? info.absoluteFilePath().toStdString() : std::string{};
    }

    QByteArray bytes;
    QString extension;
    if (embedded->mHeight == 0) {
        bytes = QByteArray(reinterpret_cast<const char*>(embedded->pcData),
                           static_cast<qsizetype>(embedded->mWidth));
        extension = QString::fromLatin1(embedded->achFormatHint).trimmed();
        if (!extension.isEmpty() && !extension.startsWith('.')) extension.prepend('.');
    } else {
        QImage image(reinterpret_cast<const uchar*>(embedded->pcData),
                     static_cast<int>(embedded->mWidth), static_cast<int>(embedded->mHeight),
                     QImage::Format_RGBA8888);
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        check(image.save(&buffer, "PNG"), "Could not decode an embedded texture.");
        extension = ".png";
    }
    check(!bytes.isEmpty(), "The scene contains an empty embedded texture.");
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + (glb ? "/ImportedGLBTextures" : "/ImportedFBXTextures");
    check(QDir().mkpath(root), "Could not create the imported texture directory.");
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString path = QDir(root).filePath(digest + extension.toLower());
    if (!QFileInfo::exists(path)) {
        QSaveFile file(path);
        check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
              "Could not store an embedded texture.");
    }
    return path.toStdString();
}

// Dom3D scalar texture slots consume grayscale; glTF packs roughness in G
// and metallic in B. Extract them instead of importing the RGB image twice.
std::string scalar_texture(const std::string& path, int channel, float factor) {
    if (path.empty()) return {};
    const QImage source(QString::fromStdString(path));
    check(!source.isNull(), "Could not decode a GLB metallic/roughness texture.");
    QImage gray(source.size(), QImage::Format_RGB32);
    for (int y=0; y<source.height(); ++y) {
        auto* row=reinterpret_cast<QRgb*>(gray.scanLine(y));
        for (int x=0; x<source.width(); ++x) {
            const QRgb pixel=source.pixel(x,y);
            // Dom3D renderers replace the scalar with the map, so bake the
            // glTF factor into the channel instead of dropping that factor.
            const int v=static_cast<int>(std::lround((channel==1 ? qGreen(pixel) : qBlue(pixel))*factor));
            row[x]=qRgb(v,v,v);
        }
    }
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    check(gray.save(&buffer,"PNG"),"Could not encode a GLB scalar texture.");
    const QString root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/ImportedGLBTextures";
    check(QDir().mkpath(root),"Could not create the GLB texture directory.");
    const QString digest=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    const QString target=QDir(root).filePath(digest+".png");
    if (!QFileInfo::exists(target)) {
        QSaveFile file(target);
        check(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit(),
              "Could not store a GLB scalar texture.");
    }
    return target.toStdString();
}

Material import_material(const aiScene& scene, unsigned index, const QString& directory, bool glb) {
    Material result = Material::ImportedMesh();
    if (glb) result.id=0; // Register source materials, not the built-in Imported Mesh ID.
    if (index >= scene.mNumMaterials) return result;
    const aiMaterial* source = scene.mMaterials[index];
    aiString name;
    if (source->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length) result.name = name.C_Str();
    aiColor3D value;
    if (source->Get(AI_MATKEY_BASE_COLOR, value) == AI_SUCCESS
        || source->Get(AI_MATKEY_COLOR_DIFFUSE, value) == AI_SUCCESS) result.diffuse = color(value);
    if (source->Get(AI_MATKEY_COLOR_AMBIENT, value) == AI_SUCCESS) result.ambient = color(value);
    if (source->Get(AI_MATKEY_COLOR_EMISSIVE, value) == AI_SUCCESS) result.emission = color(value);
    float scalar = 0;
    if (source->Get(AI_MATKEY_OPACITY, scalar) == AI_SUCCESS) result.alpha = std::clamp(scalar,0.0f,1.0f);
    if (source->Get(AI_MATKEY_SHININESS, scalar) == AI_SUCCESS) result.shininess = std::max(0.0f,scalar);
    if (source->Get(AI_MATKEY_REFLECTIVITY, scalar) == AI_SUCCESS) result.reflectivity = std::clamp(scalar,0.0f,1.0f);
    if (source->Get(AI_MATKEY_ROUGHNESS_FACTOR, scalar) == AI_SUCCESS) result.roughness = std::clamp(scalar,0.0f,1.0f);
    if (source->Get(AI_MATKEY_METALLIC_FACTOR, scalar) == AI_SUCCESS) result.metallic = std::clamp(scalar,0.0f,1.0f);
    const auto texture = [&](aiTextureType type) {
        aiString path;
        return source->GetTexture(type, 0, &path) == AI_SUCCESS
            ? stored_texture(scene, path, directory, glb) : std::string{};
    };
    result.color_texture_path = texture(source->GetTextureCount(aiTextureType_BASE_COLOR)
        ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE);
    result.normal_texture_path = texture(aiTextureType_NORMALS);
    result.bump_texture_path = texture(aiTextureType_HEIGHT);
    result.light_texture_path = texture(aiTextureType_EMISSIVE);
    result.roughness_texture_path = texture(aiTextureType_DIFFUSE_ROUGHNESS);
    result.metallic_texture_path = texture(aiTextureType_METALNESS);
    result.displacement_texture_path = texture(aiTextureType_DISPLACEMENT);
    if (glb) {
        result.roughness_texture_path=scalar_texture(result.roughness_texture_path,1,result.roughness);
        result.metallic_texture_path=scalar_texture(result.metallic_texture_path,2,result.metallic);
        aiString mode;
        if (source->Get(AI_MATKEY_GLTF_ALPHAMODE,mode)==AI_SUCCESS
            && std::string(mode.C_Str())=="OPAQUE") result.alpha=1.0f;
    }
    aiUVTransform uv;
    if (source->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE, 0), uv) == AI_SUCCESS) {
        result.texture_offset_u = uv.mTranslation.x;
        result.texture_offset_v = uv.mTranslation.y;
        result.texture_scale_u = uv.mScaling.x;
        result.texture_scale_v = uv.mScaling.y;
        result.texture_rotation_degrees = uv.mRotation * 180.0f / 3.14159265358979323846f;
    }
    return result;
}

std::unique_ptr<CMesh3D> import_mesh(const aiMesh& source, const Material& material,
                                     const aiMatrix4x4& world, const std::string& group, bool glb) {
    check(source.mNumVertices > 0 && source.mNumFaces > 0, "The scene contains an empty mesh.");
    std::vector<Vec3> vertices; vertices.reserve(source.mNumVertices);
    std::vector<Vec3> normals; if (source.HasNormals()) normals.reserve(source.mNumVertices);
    std::vector<UV> uvs; if (source.HasTextureCoords(0)) uvs.reserve(source.mNumVertices);
    for (unsigned i=0; i<source.mNumVertices; ++i) {
        check(finite(source.mVertices[i]), "The mesh contains invalid vertex coordinates.");
        vertices.push_back({source.mVertices[i].x,source.mVertices[i].y,source.mVertices[i].z});
        if (source.HasNormals()) normals.push_back({source.mNormals[i].x,source.mNormals[i].y,source.mNormals[i].z});
        if (source.HasTextureCoords(0)) uvs.push_back({source.mTextureCoords[0][i].x,source.mTextureCoords[0][i].y});
    }
    std::vector<CMesh3D::Face> faces;
    for (unsigned i=0; i<source.mNumFaces; ++i) {
        const aiFace& f=source.mFaces[i];
        check(f.mNumIndices == 3, "Triangulation produced a non-triangle face.");
        CMesh3D::Face face;
        for (unsigned c=0;c<3;++c) {
            check(f.mIndices[c] < vertices.size(), "A face refers to a missing vertex.");
            face.corners.push_back({f.mIndices[c],f.mIndices[c],f.mIndices[c]});
        }
        faces.push_back(std::move(face));
    }
    auto result=std::make_unique<CMesh3D>(source.mName.length ? source.mName.C_Str() : (glb ? "GLB Mesh" : "FBX Mesh"));
    check(result->SetGeometry(std::move(vertices),std::move(faces),std::move(uvs),std::move(normals)),
          "Could not construct the imported mesh.");
    const auto transform=dom_matrix(world,glb);
    check(std::all_of(transform.begin(),transform.end(),[](double v){return std::isfinite(v);}),
          "Imported mesh contains a non-finite transform.");
    check(result->ApplyAffineTransform(transform), "Imported mesh contains a singular or invalid transform.");
    result->SetMaterial(material); result->SetColor(material.diffuse); result->SetGroupName(group);
    return result;
}

struct ExportPart { const CMesh3D* mesh; Material material; std::string name; std::string group; };

std::string material_key(const Material& m) {
    std::ostringstream key; key.precision(9);
    key<<m.name<<'|'<<m.diffuse.r<<'|'<<m.diffuse.g<<'|'<<m.diffuse.b
       <<'|'<<m.ambient.r<<'|'<<m.ambient.g<<'|'<<m.ambient.b
       <<'|'<<m.emission.r<<'|'<<m.emission.g<<'|'<<m.emission.b
       <<'|'<<m.alpha<<'|'<<m.specular<<'|'<<m.shininess<<'|'<<m.reflectivity
       <<'|'<<m.roughness<<'|'<<m.metallic<<'|'<<m.color_texture_path
       <<'|'<<m.light_texture_path<<'|'<<m.bump_texture_path<<'|'<<m.normal_texture_path
       <<'|'<<m.roughness_texture_path<<'|'<<m.metallic_texture_path<<'|'<<m.displacement_texture_path
       <<'|'<<m.texture_offset_u<<'|'<<m.texture_offset_v<<'|'<<m.texture_scale_u
       <<'|'<<m.texture_scale_v<<'|'<<m.texture_rotation_degrees;
    return key.str();
}
}

namespace {
bool import_assimp(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes,
                   std::string& error, bool glb) {
    error.clear();
    try {
        QFile file(QString::fromStdString(path));
        check(file.open(QIODevice::ReadOnly), "Could not open the file.");
        check(file.size()>0 && file.size()<=kMaxFileBytes, "File is empty or too large.");
        const QByteArray bytes=file.readAll();
        check(bytes.size()==file.size(), "Could not read the complete file.");
        if (glb) {
            check(bytes.size()>=20 && bytes.startsWith("glTF"),"Invalid GLB header.");
            check(qFromLittleEndian<quint32>(bytes.constData()+4)==2,"Only GLB version 2 is supported.");
            check(qFromLittleEndian<quint32>(bytes.constData()+8)==static_cast<quint32>(bytes.size()),
                  "GLB file length does not match its header.");
        }
        Assimp::Importer importer;
        const unsigned flags=aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_SortByPType
            |aiProcess_ValidateDataStructure|aiProcess_GenSmoothNormals;
        const aiScene* scene=importer.ReadFileFromMemory(bytes.constData(),static_cast<size_t>(bytes.size()),flags,glb?"glb":"fbx");
        check(scene && scene->mRootNode, std::string("Assimp reader: ")+importer.GetErrorString());
        std::vector<Material> materials;
        const QString directory=QFileInfo(file).absolutePath();
        std::set<std::string> material_names;
        for (unsigned i=0;i<scene->mNumMaterials;++i) {
            Material material=import_material(*scene,i,directory,glb);
            if (glb) {
                const std::string base=material.name.empty()||material.name=="Imported Mesh"
                    ? "GLB Material" : material.name;
                material.name=base;
                unsigned suffix=2;
                while(!material_names.insert(material.name).second)
                    material.name=base+" ("+std::to_string(suffix++)+")";
            }
            materials.push_back(std::move(material));
        }
        std::vector<std::unique_ptr<CMesh3D>> result;
        size_t triangles=0;
        std::function<void(const aiNode*,aiMatrix4x4,std::string)> visit;
        visit=[&](const aiNode* node,aiMatrix4x4 parent,std::string group) {
            const aiMatrix4x4 world=parent*node->mTransformation;
            const std::string node_name=node->mName.C_Str();
            if (node!=scene->mRootNode && !node_name.empty()) group=group.empty()?node_name:group+" / "+node_name;
            for (unsigned i=0;i<node->mNumMeshes;++i) {
                check(node->mMeshes[i]<scene->mNumMeshes,"A node refers to a missing mesh.");
                const aiMesh* source=scene->mMeshes[node->mMeshes[i]];
                triangles+=source->mNumFaces; check(triangles<=kMaxTriangles,"The scene contains too many triangles.");
                const Material material=source->mMaterialIndex<materials.size()?materials[source->mMaterialIndex]:Material::ImportedMesh();
                result.push_back(import_mesh(*source,material,world,group,glb));
            }
            for (unsigned i=0;i<node->mNumChildren;++i) visit(node->mChildren[i],world,group);
        };
        visit(scene->mRootNode,aiMatrix4x4(),{});
        check(!result.empty(),"The scene contains no triangle meshes.");
        meshes.reserve(meshes.size()+result.size()); for(auto& mesh:result) meshes.push_back(std::move(mesh));
        return true;
    } catch(const std::exception& exception) {
        error=std::string(glb?"GLB import: ":"FBX import: ")+exception.what();
        return false;
    }
}
}

bool FbxIO::Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes,
                   std::string& error) const {
    return import_assimp(path,meshes,error,false);
}

bool GlbIO::Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes,
                   std::string& error) const {
    return import_assimp(path,meshes,error,true);
}

bool FbxIO::Export(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    error.clear();
    try {
        std::vector<ExportPart> parts;
        for(const auto& object:document.GetObjects()) {
            if(!object||!document.IsObjectVisible(*object)) continue;
            if(const auto* mesh=dynamic_cast<const CMesh3D*>(object.get()))
                parts.push_back({mesh,mesh->GetMaterial(),mesh->GetName(),mesh->GetGroupName()});
            else if(const auto* solid=dynamic_cast<const CSolid*>(object.get())) {
                check(solid->EnsureRenderMesh(),"Could not tessellate solid: "+solid->GetName());
                for(int i=0;i<solid->GetNumSurfaces();++i) {
                    const auto* surface=solid->GetSurfaceFace(i); if(!surface||!surface->pMesh3D) continue;
                    Material material=ComposeSurfaceMaterial(solid->GetMaterial(),surface->MaterialOverride);
                    material.texture_offset_u+=surface->TextureTransform.offset_u;
                    material.texture_offset_v+=surface->TextureTransform.offset_v;
                    material.texture_scale_u*=surface->TextureTransform.scale_u;
                    material.texture_scale_v*=surface->TextureTransform.scale_v;
                    material.texture_rotation_degrees+=surface->TextureTransform.rotation_degrees;
                    parts.push_back({surface->pMesh3D,material,solid->GetName()+" / "+std::to_string(i+1),solid->GetGroupName()});
                }
            }
        }
        check(!parts.empty(),"There are no visible mesh or solid objects to export.");
        auto scene=std::make_unique<aiScene>();
        scene->mRootNode=new aiNode("Dom3D Pro");
        std::vector<aiMesh*> out_meshes; std::vector<aiMaterial*> out_materials;
        std::vector<aiTexture*> out_textures; std::unordered_map<std::string,unsigned> material_ids,texture_ids;
        const auto embed=[&](const std::string& path)->std::string {
            if(path.empty()) return {};
            auto found=texture_ids.find(path); if(found!=texture_ids.end()) return "*"+std::to_string(found->second);
            QFile file(QString::fromStdString(path)); if(!file.open(QIODevice::ReadOnly)) return {};
            const QByteArray bytes=file.readAll(); if(bytes.isEmpty()) return {};
            auto* texture=new aiTexture(); texture->mWidth=static_cast<unsigned>(bytes.size()); texture->mHeight=0;
            texture->pcData=new aiTexel[(bytes.size()+sizeof(aiTexel)-1)/sizeof(aiTexel)]{};
            std::memcpy(texture->pcData,bytes.constData(),static_cast<size_t>(bytes.size()));
            const QByteArray suffix=QFileInfo(file).suffix().toLatin1().left(8); std::memcpy(texture->achFormatHint,suffix.constData(),suffix.size());
            texture->mFilename.Set(QFileInfo(file).fileName().toUtf8().constData());
            const unsigned id=static_cast<unsigned>(out_textures.size()); out_textures.push_back(texture); texture_ids[path]=id;
            return "*"+std::to_string(id);
        };
        const auto add_material=[&](const Material& m)->unsigned {
            const std::string key=material_key(m); auto found=material_ids.find(key); if(found!=material_ids.end()) return found->second;
            auto* a=new aiMaterial(); aiString n(m.name); a->AddProperty(&n,AI_MATKEY_NAME);
            aiColor3D d(m.diffuse.r,m.diffuse.g,m.diffuse.b),amb(m.ambient.r,m.ambient.g,m.ambient.b),em(m.emission.r,m.emission.g,m.emission.b);
            aiColor3D transparent(1.0f-m.alpha,1.0f-m.alpha,1.0f-m.alpha);
            aiColor3D specular(m.specular,m.specular,m.specular);
            a->AddProperty(&d,1,AI_MATKEY_COLOR_DIFFUSE); a->AddProperty(&d,1,AI_MATKEY_BASE_COLOR);
            a->AddProperty(&amb,1,AI_MATKEY_COLOR_AMBIENT); a->AddProperty(&em,1,AI_MATKEY_COLOR_EMISSIVE);
            a->AddProperty(&transparent,1,AI_MATKEY_COLOR_TRANSPARENT); a->AddProperty(&specular,1,AI_MATKEY_COLOR_SPECULAR);
            a->AddProperty(&m.alpha,1,AI_MATKEY_OPACITY); a->AddProperty(&m.shininess,1,AI_MATKEY_SHININESS);
            a->AddProperty(&m.reflectivity,1,AI_MATKEY_REFLECTIVITY); a->AddProperty(&m.roughness,1,AI_MATKEY_ROUGHNESS_FACTOR);
            a->AddProperty(&m.metallic,1,AI_MATKEY_METALLIC_FACTOR);
            const auto set_texture=[&](const std::string& p,aiTextureType type) { const std::string id=embed(p); if(!id.empty()){aiString s(id);a->AddProperty(&s,AI_MATKEY_TEXTURE(type,0));} };
            set_texture(m.color_texture_path,aiTextureType_DIFFUSE); set_texture(m.normal_texture_path,aiTextureType_NORMALS);
            set_texture(m.bump_texture_path,aiTextureType_HEIGHT); set_texture(m.light_texture_path,aiTextureType_EMISSIVE);
            set_texture(m.displacement_texture_path,aiTextureType_DISPLACEMENT);
            aiUVTransform uv; uv.mTranslation={m.texture_offset_u,m.texture_offset_v}; uv.mScaling={m.texture_scale_u,m.texture_scale_v};
            uv.mRotation=m.texture_rotation_degrees*3.14159265358979323846f/180.0f; a->AddProperty(&uv,1,AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE,0));
            const unsigned id=static_cast<unsigned>(out_materials.size()); out_materials.push_back(a); material_ids[key]=id; return id;
        };
        std::vector<aiNode*> nodes; size_t triangle_count=0;
        std::vector<fbxsharp::Geometry> edge_geometry;
        for(const ExportPart& part:parts) {
            const auto& vertices=part.mesh->GetVertices(); const auto& faces=part.mesh->GetFaces();
            Vec3 low,high; check(part.mesh->GetBounds(low,high),"Cannot export invalid FBX mesh bounds.");
            const Vec3 center=(low+high)*0.5f;
            fbxsharp::Geometry geometry;
            geometry.enabled = !part.mesh->GetSharpEdges().empty();
            objsharp::Data sharp;
            std::vector<size_t> position_ids(vertices.size());
            if (geometry.enabled) {
                // Low-poly CAD patches may have separate indices at the same
                // surface boundary. Match Assimp's position welding, including
                // when writing explicit Sharp/Crease layers.
                std::map<std::array<float,3>,size_t> positions;
                for (size_t i=0;i<vertices.size();++i) {
                    const auto& p=vertices[i];
                    const std::array<float,3> local{(p.x-center.x)/10, (p.y-center.y)/10, (p.z-center.z)/10};
                    const auto entry=positions.emplace(local, positions.size());
                    position_ids[i]=entry.first->second;
                    if(entry.second) geometry.vertices.insert(geometry.vertices.end(),local.begin(),local.end());
                }
                sharp = objsharp::Build(*part.mesh, position_ids);
                check(sharp.valid, "FBX sharp edges refer to invalid geometry.");
            }
            std::map<std::pair<size_t,size_t>,size_t> edge_ids;
            auto* mesh=new aiMesh(); mesh->mName.Set(part.name); mesh->mMaterialIndex=add_material(part.material);
            const auto& uvs=part.mesh->GetUVs(); const auto& normals=part.mesh->GetNormals();
            size_t corner_count=0;
            for(const auto& face:faces) if(!face.deleted&&face.corners.size()>=3) corner_count+=face.corners.size();
            check(corner_count>0&&corner_count<=std::numeric_limits<unsigned>::max(),"FBX mesh has no valid faces or is too large.");
            mesh->mNumVertices=static_cast<unsigned>(corner_count); mesh->mVertices=new aiVector3D[mesh->mNumVertices];
            if(!uvs.empty()){mesh->mTextureCoords[0]=new aiVector3D[mesh->mNumVertices]{};mesh->mNumUVComponents[0]=2;}
            if(!normals.empty() || geometry.enabled) mesh->mNormals=new aiVector3D[mesh->mNumVertices]{};
            std::vector<aiFace> polygons;
            unsigned next_corner=0;
            for(size_t face_id=0;face_id<faces.size();++face_id) {
                const auto& face=faces[face_id];
                if(face.deleted || face.corners.size()<3) continue;
                // FBX supports polygons. Keep quads/n-gons intact: triangulating
                // the control cage changes Catmull-Clark subdivision.
                aiFace f; f.mNumIndices=static_cast<unsigned>(face.corners.size());
                f.mIndices=new unsigned[f.mNumIndices];
                triangle_count+=face.corners.size()-2;
                check(triangle_count<=kMaxTriangles,"FBX export contains too many polygons.");
                for(unsigned k=0;k<f.mNumIndices;++k) {
                    const auto& corner=face.corners[k];
                    check(corner.v<vertices.size(),"FBX source face refers to a missing vertex.");
                    const unsigned dst=next_corner++; f.mIndices[k]=dst;
                    const Vec3 p=vertices[corner.v]; mesh->mVertices[dst]={(p.x-center.x)/10,(p.y-center.y)/10,(p.z-center.z)/10};
                    if(mesh->mTextureCoords[0]) { check(corner.uv<uvs.size(),"FBX source face refers to missing UV coordinates."); const UV uv=uvs[corner.uv];mesh->mTextureCoords[0][dst]={uv.u,uv.v,0}; }
                    if(mesh->mNormals) {
                        Vec3 n;
                        if (geometry.enabled) n=sharp.normals[sharp.corner_normals[face_id][k]];
                        else { check(corner.n<normals.size(),"FBX source face refers to a missing normal."); n=normals[corner.n]; }
                        mesh->mNormals[dst]={n.x,n.y,n.z};
                    }
                    if (geometry.enabled) {
                        const size_t a=position_ids[corner.v];
                        const size_t b=position_ids[face.corners[(k+1)%f.mNumIndices].v];
                        check(a <= size_t(INT32_MAX), "FBX vertex index exceeds the format limit.");
                        const auto v=static_cast<qint32>(a);
                        geometry.polygons.push_back(k+1==f.mNumIndices ? ~v : v);
                        const auto edge=std::minmax(a,b);
                        if(edge_ids.emplace(edge, edge_ids.size()).second) {
                            geometry.edges.push_back(static_cast<qint32>(dst));
                            const bool hard=sharp.edges.count(edge)!=0;
                            geometry.smoothing.push_back(hard ? 0 : 1);
                            geometry.creases.push_back(hard ? 1.0 : 0.0);
                        }
                    }
                }
                polygons.push_back(std::move(f));
            }
            edge_geometry.push_back(std::move(geometry));
            check(!polygons.empty(),"FBX mesh has no valid faces.");
            mesh->mNumFaces=static_cast<unsigned>(polygons.size()); mesh->mFaces=new aiFace[polygons.size()];
            for(size_t i=0;i<polygons.size();++i) mesh->mFaces[i]=std::move(polygons[i]);
            const unsigned mesh_id=static_cast<unsigned>(out_meshes.size()); out_meshes.push_back(mesh);
            auto* node=new aiNode(part.name); node->mNumMeshes=1; node->mMeshes=new unsigned[1]{mesh_id};
            node->mTransformation.a4=center.x/10;node->mTransformation.b4=center.y/10;node->mTransformation.c4=center.z/10; nodes.push_back(node);
        }
        scene->mNumMeshes=static_cast<unsigned>(out_meshes.size());scene->mMeshes=new aiMesh*[scene->mNumMeshes];std::copy(out_meshes.begin(),out_meshes.end(),scene->mMeshes);
        scene->mNumMaterials=static_cast<unsigned>(out_materials.size());scene->mMaterials=new aiMaterial*[scene->mNumMaterials];std::copy(out_materials.begin(),out_materials.end(),scene->mMaterials);
        scene->mNumTextures=static_cast<unsigned>(out_textures.size());if(scene->mNumTextures){scene->mTextures=new aiTexture*[scene->mNumTextures];std::copy(out_textures.begin(),out_textures.end(),scene->mTextures);}
        scene->mRootNode->mNumChildren=static_cast<unsigned>(nodes.size());scene->mRootNode->mChildren=new aiNode*[nodes.size()];
        for(size_t i=0;i<nodes.size();++i){scene->mRootNode->mChildren[i]=nodes[i];nodes[i]->mParent=scene->mRootNode;}
        Assimp::Exporter exporter; const aiExportDataBlob* blob=exporter.ExportToBlob(scene.get(),"fbx");
        check(blob&&blob->data&&blob->size,std::string("Assimp FBX writer: ")+exporter.GetErrorString());
        QByteArray bytes(static_cast<const char*>(blob->data), static_cast<qsizetype>(blob->size));
        if(std::any_of(edge_geometry.begin(),edge_geometry.end(),[](const auto& g){return g.enabled;}))
            bytes=fbxsharp::Complete(bytes,edge_geometry);
        QSaveFile file(QString::fromStdString(path));check(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit(),"Could not write the FBX file.");
        return true;
    } catch(const std::exception& exception) { error=std::string("FBX export: ")+exception.what(); return false; }
}
