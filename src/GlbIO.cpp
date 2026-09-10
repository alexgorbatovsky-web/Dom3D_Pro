#include "GlbIO.h"
#include "CAlfaDoc.h"
#include "solid/Solid.h"
#include "render/RenderScene.h"
#include "materials/GlbMaterialBaker.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
void word(QByteArray& data,quint32 value){char bytes[4];qToLittleEndian(value,bytes);data.append(bytes,4);}
void scalar(QByteArray& data,float value){check(std::isfinite(value),"Non-finite mesh data.");quint32 bits;std::memcpy(&bits,&value,4);word(data,bits);}
void align(QByteArray& data,char fill='\0'){while(data.size()%4)data.append(fill);}
QString resolve(const Material& m,const std::string& source){
    if(source.empty())return {};
    const QString path=QString::fromStdString(source);
    QStringList candidates;
    if(QFileInfo(path).isAbsolute())candidates<<path;
    else {
        if(!m.source_file_path.empty())candidates<<QFileInfo(QString::fromStdString(m.source_file_path)).absoluteDir().filePath(path);
        candidates<<QDir(QCoreApplication::applicationDirPath()).filePath("materials/"+path)
            <<QDir(QCoreApplication::applicationDirPath()).filePath(path)<<path;
    }
    for(const QString& candidate:candidates)if(QFileInfo(candidate).isFile())return QFileInfo(candidate).absoluteFilePath();
    throw std::runtime_error("Missing texture: "+source+" (material: "+m.name+")");
}
QImage load(const Material& m,const std::string& source){
    const QString path=resolve(m,source);if(path.isEmpty())return {};
    QImage image(path);check(!image.isNull(),"Cannot decode texture: "+path.toStdString());return image;
}
QImage relief(const QImage& height,float strength){
    QImage normal(height.size(),QImage::Format_RGB888);
    for(int y=0;y<height.height();++y)for(int x=0;x<height.width();++x){
        const auto h=[&](int a,int b){return qGray(height.pixel((a+height.width())%height.width(),(b+height.height())%height.height()))/255.f;};
        const Vec3 n=normalize(Vec3{-(h(x+1,y)-h(x-1,y))*strength*height.width(),
            (h(x,y+1)-h(x,y-1))*strength*height.height(),1});
        normal.setPixelColor(x,y,QColor::fromRgbF(n.x*.5f+.5f,n.y*.5f+.5f,n.z*.5f+.5f));
    }
    return normal;
}
class Writer {
public:
    QByteArray bin;
    QJsonArray views,accessors,images,textures,materials,meshes,nodes,roots;
    std::map<QByteArray,int> image_ids,material_ids;
    std::map<std::string,int> source_material_ids;
    std::map<QString,int> groups;
    int view(const QByteArray& bytes){align(bin);int id=views.size();views.append(QJsonObject{{"buffer",0},{"byteOffset",double(bin.size())},{"byteLength",double(bytes.size())}});bin.append(bytes);
        check(bin.size()<1024ll*1024*1024,"GLB exceeds the 1 GB export limit.");return id;}
    int accessor(const QByteArray& bytes,int count,const char* type,const QJsonArray& low={},const QJsonArray& high={}){
        QJsonObject a{{"bufferView",view(bytes)},{"componentType",5126},{"count",count},{"type",type}};
        if(!low.isEmpty()){a["min"]=low;a["max"]=high;}
        int id=accessors.size();accessors.append(a);return id;
    }
    int texture(const QImage& image){
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
        check(image.save(&buffer,"PNG"),"Could not encode a GLB texture.");
        const QByteArray hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
        auto found=image_ids.find(hash);if(found!=image_ids.end())return found->second;
        const int id=textures.size();images.append(QJsonObject{{"bufferView",view(bytes)},{"mimeType","image/png"}});
        textures.append(QJsonObject{{"source",images.size()-1},{"sampler",0}});image_ids.emplace(hash,id);return id;
    }
    QJsonObject tex(const QImage& image){return {{"index",texture(image)}};}
    int material(const Material& m,const GlbBakedMaps* baked){
        std::ostringstream source;source.precision(9);
        source<<m.name<<'\n'<<m.source_file_path<<'\n'<<m.color_texture_path<<'\n'<<m.normal_texture_path<<'\n'
            <<m.roughness_texture_path<<'\n'<<m.metallic_texture_path<<'\n'<<m.bump_texture_path<<'\n'<<m.displacement_texture_path<<'\n'<<m.light_texture_path
            <<'\n'<<m.diffuse.r<<','<<m.diffuse.g<<','<<m.diffuse.b<<','<<m.alpha<<','<<m.roughness<<','<<m.metallic
            <<','<<m.normal_strength<<','<<m.displacement_scale<<','<<m.emission.r<<','<<m.emission.g<<','<<m.emission.b;
        if(!baked){auto found=source_material_ids.find(source.str());if(found!=source_material_ids.end())return found->second;}
        QJsonObject pbr{{"baseColorFactor",QJsonArray{m.diffuse.r,m.diffuse.g,m.diffuse.b,std::clamp(m.alpha,0.f,1.f)}},
            {"roughnessFactor",std::clamp(m.roughness,0.f,1.f)},{"metallicFactor",std::clamp(m.metallic,0.f,1.f)}};
        QJsonObject out{{"name",QString::fromStdString(m.name)},{"doubleSided",true},
            {"emissiveFactor",QJsonArray{m.emission.r,m.emission.g,m.emission.b}}};
        if(m.alpha<.9999f)out["alphaMode"]="BLEND";
        if(baked){
            pbr["baseColorFactor"]=QJsonArray{1,1,1,std::clamp(m.alpha,0.f,1.f)};
            pbr["baseColorTexture"]=tex(baked->color);pbr["metallicRoughnessTexture"]=tex(baked->orm);
            pbr["roughnessFactor"]=1;pbr["metallicFactor"]=1;out["normalTexture"]=tex(baked->normal);out["occlusionTexture"]=tex(baked->orm);
        }else{
            QImage color=load(m,m.color_texture_path);if(!color.isNull())pbr["baseColorTexture"]=tex(color);
            const QImage rough=load(m,m.roughness_texture_path),metal=load(m,m.metallic_texture_path);
            if(!rough.isNull()||!metal.isNull()){
                const int width=std::max(rough.width(),metal.width()),height=std::max(rough.height(),metal.height());
                QImage orm(width,height,QImage::Format_RGB888);
                const QImage r=rough.isNull()?QImage():rough.scaled(width,height,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                const QImage b=metal.isNull()?QImage():metal.scaled(width,height,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                for(int y=0;y<height;++y)for(int x=0;x<width;++x)orm.setPixel(x,y,qRgb(255,
                    r.isNull()?int(std::clamp(m.roughness,0.f,1.f)*255+.5f):qRed(r.pixel(x,y)),
                    b.isNull()?int(std::clamp(m.metallic,0.f,1.f)*255+.5f):qRed(b.pixel(x,y))));
                pbr["metallicRoughnessTexture"]=tex(orm);pbr["roughnessFactor"]=1;pbr["metallicFactor"]=1;
            }
            QImage normal=load(m,m.normal_texture_path);
            if(normal.isNull()){
                QImage heightmap=load(m,!m.bump_texture_path.empty()?m.bump_texture_path:m.displacement_texture_path);
                if(!heightmap.isNull())normal=relief(heightmap,m.displacement_scale);
            }
            if(!normal.isNull()){auto t=tex(normal);t["scale"]=m.normal_strength;out["normalTexture"]=t;}
        }
        const QImage emission=baked?baked->emission:load(m,m.light_texture_path);if(!emission.isNull())out["emissiveTexture"]=tex(emission);
        out["pbrMetallicRoughness"]=pbr;
        const QByteArray key=QJsonDocument(out).toJson(QJsonDocument::Compact);
        auto found=material_ids.find(key);if(found!=material_ids.end())return found->second;
        int id=materials.size();materials.append(out);material_ids.emplace(key,id);if(!baked)source_material_ids[source.str()]=id;return id;
    }
    void child(int parent,int child){if(parent<0)roots.append(child);else{auto p=nodes[parent].toObject();auto c=p["children"].toArray();c.append(child);p["children"]=c;nodes[parent]=p;}}
    int group(const QString& path){int parent=-1;QString key;
        for(const QString& name:path.split('/',Qt::SkipEmptyParts)){key+='/'+name;
            auto it=groups.find(key);if(it!=groups.end()){parent=it->second;continue;}
            int id=nodes.size();nodes.append(QJsonObject{{"name",name}});child(parent,id);groups.emplace(key,id);parent=id;}
        return parent;
    }
    void mesh(RenderMesh mesh,const Material& m){
        GlbBakedMaps maps;const bool procedural=m.fabric.enabled||m.plaster.enabled;
        if(m.texture_wrap_object&&!procedural){
            Vec3 low=mesh.vertices.front(),high=low;
            for(const auto& p:mesh.vertices){low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
                high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);}
            const Vec3 center=(low+high)*.5f,extent=(high-low)*.5f;
            const float angle=deg_to_rad(m.texture_rotation_degrees),c=std::cos(angle),s=std::sin(angle);
            for(auto& triangle:mesh.triangles)for(int k=0;k<3;++k){
                const auto p=mesh.vertices[triangle.vertices[k]]-center;
                const Vec3 d=normalize(Vec3{p.x/std::max(extent.x,.0001f),p.y/std::max(extent.y,.0001f),p.z/std::max(extent.z,.0001f)});
                const float denom=std::sqrt(std::max(2*(1+d.z),.0001f));
                const float u=d.x/denom*.5f*m.texture_scale_u,v=d.y/denom*.5f*m.texture_scale_v;
                triangle.uvs[k]={u*c-v*s+.5f+m.texture_offset_u,u*s+v*c+.5f+m.texture_offset_v};
            }
        }
        if(procedural){
            QImage normal=load(m,m.normal_texture_path);
            if(normal.isNull()){
                const auto height=load(m,!m.bump_texture_path.empty()?m.bump_texture_path:m.displacement_texture_path);
                if(!height.isNull())normal=relief(height,m.displacement_scale);
            }
            maps=BakeGlbMaterial(mesh,m,load(m,m.color_texture_path),normal,load(m,m.light_texture_path));
        }
        const int mat=material(m,procedural?&maps:nullptr);
        QByteArray positions,normals,uvs;
        Vec3 low{1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f};
        for(const auto& triangle:mesh.triangles)for(int c=0;c<3;++c){
            check(triangle.vertices[c]>=0&&size_t(triangle.vertices[c])<mesh.vertices.size(),"Invalid source vertex index.");
            const Vec3 p=mesh.vertices[triangle.vertices[c]],n=triangle.normals[c];
            const Vec3 v{p.x*.001f,p.z*.001f,-p.y*.001f};
            for(float value:{v.x,v.y,v.z})scalar(positions,value);
            for(float value:{n.x,n.z,-n.y})scalar(normals,value);
            scalar(uvs,triangle.uvs[c].u);scalar(uvs,1-triangle.uvs[c].v);
            low.x=std::min(low.x,v.x);low.y=std::min(low.y,v.y);low.z=std::min(low.z,v.z);
            high.x=std::max(high.x,v.x);high.y=std::max(high.y,v.y);high.z=std::max(high.z,v.z);
        }
        check(!mesh.triangles.empty(),"Cannot export an empty mesh.");
        const int count=int(mesh.triangles.size()*3);
        const int position=accessor(positions,count,"VEC3",{low.x,low.y,low.z},{high.x,high.y,high.z});
        QJsonObject attributes{{"POSITION",position},{"NORMAL",accessor(normals,count,"VEC3")},{"TEXCOORD_0",accessor(uvs,count,"VEC2")}};
        const int id=meshes.size();meshes.append(QJsonObject{{"name",mesh.name},{"primitives",QJsonArray{QJsonObject{{"attributes",attributes},{"material",mat},{"mode",4}}}}});
        const int parent=group(mesh.group_name),node=nodes.size();nodes.append(QJsonObject{{"name",mesh.name},{"mesh",id}});child(parent,node);
    }
    void save(const QString& path){
        QJsonObject root{{"asset",QJsonObject{{"version","2.0"},{"generator","Dom3D Pro"}}},{"scene",0},
            {"scenes",QJsonArray{QJsonObject{{"name","Kitchen / Scene"},{"nodes",roots}}}},
            {"nodes",nodes},{"meshes",meshes},{"materials",materials},{"accessors",accessors},{"bufferViews",views},
            {"buffers",QJsonArray{QJsonObject{{"byteLength",double(bin.size())}}}}};
        if(!images.isEmpty()){root["images"]=images;root["textures"]=textures;root["samplers"]=QJsonArray{QJsonObject{{"magFilter",9729},{"minFilter",9987},{"wrapS",10497},{"wrapT",10497}}};}
        QByteArray json=QJsonDocument(root).toJson(QJsonDocument::Compact);align(json,' ');align(bin);
        QByteArray header("glTF",4);word(header,2);word(header,quint32(28+json.size()+bin.size()));word(header,quint32(json.size()));word(header,0x4e4f534a);
        QByteArray chunk;word(chunk,quint32(bin.size()));word(chunk,0x004e4942);
        QSaveFile file(path);check(file.open(QIODevice::WriteOnly),file.errorString().toStdString());
        for(const auto* bytes:{&header,&json,&chunk,&bin})check(file.write(*bytes)==bytes->size(),"Could not write GLB data.");
        check(file.commit(),file.errorString().toStdString());
    }
};
}
bool GlbIO::Export(const std::string& path,const CAlfaDoc& document,std::string& error,
                  const std::function<bool(int,const std::string&)>& progress) const {
    error.clear();
    try {
        const auto report=[&](int percent,const std::string& text){check(!progress||progress(percent,text),"Canceled.");};
        report(0,"Preparing kitchen geometry");
        for(const auto& object:document.GetObjects())if(object&&document.IsObjectVisible(*object))
            if(const auto* solid=dynamic_cast<const CSolid*>(object.get()))
                check(solid->EnsureRenderMesh(),"Could not tessellate: "+solid->GetName());
        RenderScene scene=BuildRenderScene(document,Camera{},false);
        check(!scene.meshes.empty(),"There are no visible mesh or solid objects to export.");
        check(scene.TriangleCount()<=10000000,"GLB contains too many triangles.");
        Writer writer;
        for(size_t i=0;i<scene.meshes.size();++i){
            auto& mesh=scene.meshes[i];
            report(5+int(90*i/scene.meshes.size()),"Packing "+mesh.name.toStdString());
            check(mesh.material_index>=0&&size_t(mesh.material_index)<scene.materials.size(),"Missing source material.");
            const Material& material=scene.materials[mesh.material_index];writer.mesh(std::move(mesh),material);
        }
        report(98,"Writing GLB file");
        writer.save(QString::fromStdString(path));return true;
    }catch(const std::exception& exception){error=std::string("GLB export: ")+exception.what();return false;}
}
