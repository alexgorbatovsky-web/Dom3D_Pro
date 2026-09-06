#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "FbxIO.h"
#include "CMesh3D.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
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

std::array<double, 16> dom_matrix(const aiMatrix4x4& m) {
    // Assimp's FBX importer normalizes every source unit into centimeters.
    // Dom3D geometry is millimeters, so the world transform includes cm -> mm.
    return {10*m.a1,10*m.a2,10*m.a3,10*m.a4,
            10*m.b1,10*m.b2,10*m.b3,10*m.b4,
            10*m.c1,10*m.c2,10*m.c3,10*m.c4,
            m.d1,m.d2,m.d3,m.d4};
}

std::string stored_texture(const aiScene& scene, const aiString& source,
                           const QString& source_directory) {
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
        check(image.save(&buffer, "PNG"), "Could not decode an embedded FBX texture.");
        extension = ".png";
    }
    check(!bytes.isEmpty(), "FBX contains an empty embedded texture.");
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + "/ImportedFBXTextures";
    check(QDir().mkpath(root), "Could not create the imported FBX texture directory.");
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString path = QDir(root).filePath(digest + extension.toLower());
    if (!QFileInfo::exists(path)) {
        QSaveFile file(path);
        check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
              "Could not store an embedded FBX texture.");
    }
    return path.toStdString();
}

Material import_material(const aiScene& scene, unsigned index, const QString& directory) {
    Material result = Material::ImportedMesh();
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
            ? stored_texture(scene, path, directory) : std::string{};
    };
    result.color_texture_path = texture(source->GetTextureCount(aiTextureType_BASE_COLOR)
        ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE);
    result.normal_texture_path = texture(aiTextureType_NORMALS);
    result.bump_texture_path = texture(aiTextureType_HEIGHT);
    result.light_texture_path = texture(aiTextureType_EMISSIVE);
    result.roughness_texture_path = texture(aiTextureType_DIFFUSE_ROUGHNESS);
    result.metallic_texture_path = texture(aiTextureType_METALNESS);
    result.displacement_texture_path = texture(aiTextureType_DISPLACEMENT);
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
                                     const aiMatrix4x4& world, const std::string& group) {
    check(source.mNumVertices > 0 && source.mNumFaces > 0, "FBX contains an empty mesh.");
    std::vector<Vec3> vertices; vertices.reserve(source.mNumVertices);
    std::vector<Vec3> normals; if (source.HasNormals()) normals.reserve(source.mNumVertices);
    std::vector<UV> uvs; if (source.HasTextureCoords(0)) uvs.reserve(source.mNumVertices);
    for (unsigned i=0; i<source.mNumVertices; ++i) {
        check(finite(source.mVertices[i]), "FBX contains invalid vertex coordinates.");
        vertices.push_back({source.mVertices[i].x,source.mVertices[i].y,source.mVertices[i].z});
        if (source.HasNormals()) normals.push_back({source.mNormals[i].x,source.mNormals[i].y,source.mNormals[i].z});
        if (source.HasTextureCoords(0)) uvs.push_back({source.mTextureCoords[0][i].x,source.mTextureCoords[0][i].y});
    }
    std::vector<CMesh3D::Face> faces;
    for (unsigned i=0; i<source.mNumFaces; ++i) {
        const aiFace& f=source.mFaces[i];
        check(f.mNumIndices == 3, "FBX triangulation produced a non-triangle face.");
        CMesh3D::Face face;
        for (unsigned c=0;c<3;++c) {
            check(f.mIndices[c] < vertices.size(), "FBX face refers to a missing vertex.");
            face.corners.push_back({f.mIndices[c],f.mIndices[c],f.mIndices[c]});
        }
        faces.push_back(std::move(face));
    }
    auto result=std::make_unique<CMesh3D>(source.mName.length ? source.mName.C_Str() : "FBX Mesh");
    check(result->SetGeometry(std::move(vertices),std::move(faces),std::move(uvs),std::move(normals)),
          "Could not construct imported FBX mesh.");
    check(result->ApplyAffineTransform(dom_matrix(world)), "FBX contains a singular or invalid transform.");
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

bool FbxIO::Import(const std::string& path, std::vector<std::unique_ptr<CMesh3D>>& meshes,
                   std::string& error) const {
    error.clear();
    try {
        QFile file(QString::fromStdString(path));
        check(file.open(QIODevice::ReadOnly), "Could not open the FBX file.");
        check(file.size()>0 && file.size()<=kMaxFileBytes, "FBX file is empty or too large.");
        const QByteArray bytes=file.readAll();
        check(bytes.size()==file.size(), "Could not read the complete FBX file.");
        Assimp::Importer importer;
        const unsigned flags=aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_SortByPType
            |aiProcess_ValidateDataStructure|aiProcess_GenSmoothNormals;
        const aiScene* scene=importer.ReadFileFromMemory(bytes.constData(),static_cast<size_t>(bytes.size()),flags,"fbx");
        check(scene && scene->mRootNode, std::string("Assimp FBX reader: ")+importer.GetErrorString());
        std::vector<Material> materials;
        const QString directory=QFileInfo(file).absolutePath();
        for (unsigned i=0;i<scene->mNumMaterials;++i) materials.push_back(import_material(*scene,i,directory));
        std::vector<std::unique_ptr<CMesh3D>> result;
        size_t triangles=0;
        std::function<void(const aiNode*,aiMatrix4x4,std::string)> visit;
        visit=[&](const aiNode* node,aiMatrix4x4 parent,std::string group) {
            const aiMatrix4x4 world=parent*node->mTransformation;
            const std::string node_name=node->mName.C_Str();
            if (node!=scene->mRootNode && !node_name.empty()) group=group.empty()?node_name:group+" / "+node_name;
            for (unsigned i=0;i<node->mNumMeshes;++i) {
                check(node->mMeshes[i]<scene->mNumMeshes,"FBX node refers to a missing mesh.");
                const aiMesh* source=scene->mMeshes[node->mMeshes[i]];
                triangles+=source->mNumFaces; check(triangles<=kMaxTriangles,"FBX contains too many triangles.");
                const Material material=source->mMaterialIndex<materials.size()?materials[source->mMaterialIndex]:Material::ImportedMesh();
                result.push_back(import_mesh(*source,material,world,group));
            }
            for (unsigned i=0;i<node->mNumChildren;++i) visit(node->mChildren[i],world,group);
        };
        visit(scene->mRootNode,aiMatrix4x4(),{});
        check(!result.empty(),"FBX contains no triangle meshes.");
        meshes.reserve(meshes.size()+result.size()); for(auto& mesh:result) meshes.push_back(std::move(mesh));
        return true;
    } catch(const std::exception& exception) { error=std::string("FBX import: ")+exception.what(); return false; }
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
        for(const ExportPart& part:parts) {
            const auto& vertices=part.mesh->GetVertices(); const auto& faces=part.mesh->GetFaces();
            Vec3 low,high; check(part.mesh->GetBounds(low,high),"Cannot export invalid FBX mesh bounds.");
            const Vec3 center=(low+high)*0.5f;
            auto* mesh=new aiMesh(); mesh->mName.Set(part.name); mesh->mMaterialIndex=add_material(part.material);
            const auto& uvs=part.mesh->GetUVs(); const auto& normals=part.mesh->GetNormals();
            size_t corner_count=0;
            for(const auto& face:faces) if(!face.deleted&&face.corners.size()>=3) corner_count+=(face.corners.size()-2)*3;
            check(corner_count>0&&corner_count<=std::numeric_limits<unsigned>::max(),"FBX mesh has no valid faces or is too large.");
            mesh->mNumVertices=static_cast<unsigned>(corner_count); mesh->mVertices=new aiVector3D[mesh->mNumVertices];
            if(!uvs.empty()){mesh->mTextureCoords[0]=new aiVector3D[mesh->mNumVertices]{};mesh->mNumUVComponents[0]=2;}
            if(!normals.empty()) mesh->mNormals=new aiVector3D[mesh->mNumVertices]{};
            std::vector<aiFace> triangles;
            for(const auto& face:faces) if(!face.deleted&&face.corners.size()>=3) for(size_t c=1;c+1<face.corners.size();++c) {
                aiFace f; f.mNumIndices=3; f.mIndices=new unsigned[3];
                const MeshCorner corners[3]{face.corners[0],face.corners[c],face.corners[c+1]};
                for(unsigned k=0;k<3;++k) {
                    check(corners[k].v<vertices.size(),"FBX source face refers to a missing vertex.");
                    const unsigned dst=static_cast<unsigned>(triangles.size()*3+k); f.mIndices[k]=dst;
                    const Vec3 p=vertices[corners[k].v]; mesh->mVertices[dst]={(p.x-center.x)/10,(p.y-center.y)/10,(p.z-center.z)/10};
                    if(mesh->mTextureCoords[0]) { check(corners[k].uv<uvs.size(),"FBX source face refers to missing UV coordinates."); const UV uv=uvs[corners[k].uv];mesh->mTextureCoords[0][dst]={uv.u,uv.v,0}; }
                    if(mesh->mNormals) { check(corners[k].n<normals.size(),"FBX source face refers to a missing normal."); const Vec3 n=normals[corners[k].n];mesh->mNormals[dst]={n.x,n.y,n.z}; }
                }
                triangles.push_back(f);
            }
            check(!triangles.empty(),"FBX mesh has no valid faces."); triangle_count+=triangles.size();check(triangle_count<=kMaxTriangles,"FBX export contains too many triangles.");
            mesh->mNumFaces=static_cast<unsigned>(triangles.size()); mesh->mFaces=new aiFace[triangles.size()];
            for(size_t i=0;i<triangles.size();++i) mesh->mFaces[i]=std::move(triangles[i]);
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
        QSaveFile file(QString::fromStdString(path));check(file.open(QIODevice::WriteOnly)&&file.write(static_cast<const char*>(blob->data),static_cast<qint64>(blob->size))==static_cast<qint64>(blob->size)&&file.commit(),"Could not write the FBX file.");
        return true;
    } catch(const std::exception& exception) { error=std::string("FBX export: ")+exception.what(); return false; }
}
