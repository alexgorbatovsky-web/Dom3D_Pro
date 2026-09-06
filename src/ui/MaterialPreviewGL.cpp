#include "MaterialPreviewGL.h"
#include "QtSceneRenderer.h"
#include "../CMesh3D.h"
#include "../solid/Solid.h"
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QCache>
#include <QCryptographicHash>
#include <QDataStream>
#include "MaterialDrag.h"
#include <cmath>

namespace {
struct PreviewContext : QObject {
    QOffscreenSurface surface;QOpenGLContext context;QCache<QByteArray,QImage> cache{65536}; // KiB
    PreviewContext():QObject(qApp){QSurfaceFormat f;f.setVersion(2,1);f.setProfile(QSurfaceFormat::CompatibilityProfile);f.setDepthBufferSize(24);surface.setFormat(f);surface.create();context.setFormat(surface.format());context.create();}
};
}
QImage RenderMaterialSphereGL(const Material& material,int size) {
    if(!qApp || size<1)return {};
    static PreviewContext* state=nullptr;if(!state)state=new PreviewContext;
    QByteArray payload=MaterialDrag::Encode(material);
    QDataStream cacheSettings(&payload,QIODevice::Append);
    const auto& light=CMesh3D::GetLightingSettings();
    cacheSettings<<size<<light.light_x<<light.light_y<<light.light_z<<light.ambient<<light.wrap_light<<light.diffuse
        <<light.specular<<light.shininess_scale<<light.rim<<light.gamma<<light.environment_enabled
        <<QString::fromStdString(light.environment_path)<<light.environment_strength<<light.environment_rotation_degrees;
    QByteArray key=QCryptographicHash::hash(payload,QCryptographicHash::Sha256);
    if(auto* image=state->cache.object(key))return *image;
    QOpenGLContext* previous=QOpenGLContext::currentContext();QSurface* surface=previous?previous->surface():nullptr;
    if(!state->context.makeCurrent(&state->surface))return {};
    QImage result;
    {
        QOpenGLFramebufferObjectFormat format;format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        QOpenGLFramebufferObject buffer(size,size,format);
        if(buffer.isValid()){
            buffer.bind();QtSceneRenderer renderer;renderer.Initialize();
            CAlfaDoc document;auto sphere=std::make_unique<CMesh3D>("Material preview");
            std::vector<Vec3> vertices,normals;std::vector<UV> uvs;std::vector<CMesh3D::Face> faces;
            constexpr int rows=48,cols=96;constexpr float radius=25;
            std::vector<size_t> vertexIndex;
            for(int j=0;j<=rows;++j)for(int i=0;i<=cols;++i){
                double a=3.141592653589793*j/rows,b=6.283185307179586*i/cols;
                // Put UV poles at the top/bottom, not in the center of the preview.
                Vec3 n{float(std::sin(a)*std::cos(b)),float(std::cos(a)),float(-std::sin(a)*std::sin(b))};
                // UVs split at the meridian, but the geometry and poles are welded.
                size_t id=j==0?0:j==rows?1+(rows-1)*cols:1+(j-1)*cols+i%cols;
                vertexIndex.push_back(id);
                if((j==0||j==rows)?i==0:i<cols){vertices.push_back(n*radius);normals.push_back(n);}
                uvs.push_back({float(i)/cols,1-float(j)/rows});
            }
            auto add=[&](std::initializer_list<size_t> ids){CMesh3D::Face f;for(auto id:ids)f.corners.push_back({vertexIndex[id],vertexIndex[id],id});faces.push_back(std::move(f));};
            for(int j=0;j<rows;++j)for(int i=0;i<cols;++i){size_t a=j*(cols+1)+i,b=a+cols+1;
                if(j>0)add({a,b,a+1});if(j<rows-1)add({a+1,b,b+1});
            }
            sphere->SetGeometry(std::move(vertices),std::move(faces),std::move(uvs),std::move(normals));
            Material resolved=material;
            auto resolve=[&](std::string& path){if(path.empty()||QFileInfo(QString::fromStdString(path)).isAbsolute()||material.source_file_path.empty())return;
                QString sibling=QFileInfo(QString::fromStdString(material.source_file_path)).absoluteDir().filePath(QString::fromStdString(path));
                if(QFileInfo::exists(sibling))path=sibling.toStdString();};
            resolve(resolved.color_texture_path);resolve(resolved.normal_texture_path);resolve(resolved.roughness_texture_path);resolve(resolved.metallic_texture_path);resolve(resolved.displacement_texture_path);
            resolved=document.UpsertMaterial(resolved);
            sphere->SetMaterial(resolved);sphere->SetColor(material.diffuse);document.AddObject(std::move(sphere),false);document.ClearSelection();
            auto oldSolidMode=CSolid::GetDisplayMode();CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
            auto oldMode=CMesh3D::GetDisplayMode();CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceMaterial);
            const bool oldOpenEdges=CMesh3D::IsOpenEdgeDisplayEnabled();CMesh3D::SetOpenEdgeDisplayEnabled(false);
            const float oldOpacity=CMesh3D::GetSurfaceOpacity();CMesh3D::SetSurfaceOpacity(1.f);
            Camera camera;camera.target={0,0,0};camera.distance=67;camera.orientation={1,0,0,0};
            renderer.Render(document,camera,true,false,false,false,100,10,10,ToolMode::Select,TransformOperation::Move,TransformAxis::None,0,{0,0,1},false,size,size);
            state->context.functions()->glFinish();result=buffer.toImage();CMesh3D::SetDisplayMode(oldMode);CSolid::SetDisplayMode(oldSolidMode);
            CMesh3D::SetOpenEdgeDisplayEnabled(oldOpenEdges);CMesh3D::SetSurfaceOpacity(oldOpacity);buffer.release();
        }
    }
    state->context.doneCurrent();if(previous&&surface)previous->makeCurrent(surface);
    if(!result.isNull())state->cache.insert(key,new QImage(result),int((result.sizeInBytes()+1023)/1024));return result;
}
