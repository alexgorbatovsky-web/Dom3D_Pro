#include "GlbMaterialBaker.h"
#include "ProceduralPlasterShader.h"
#include "ProceduralFabricShader.h"
#include "ProceduralPerforationShader.h"
#include "../render/RenderScene.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
void ensure(bool ok,const std::string& error) {if(!ok)throw std::runtime_error(error);}
struct Context {
    QOpenGLContext* previous=QOpenGLContext::currentContext();
    QSurface* previous_surface=previous?previous->surface():nullptr;
    QOffscreenSurface surface;
    QOpenGLContext context;
    Context() {
        QSurfaceFormat format;format.setVersion(2,1);format.setProfile(QSurfaceFormat::CompatibilityProfile);
        surface.setFormat(format);surface.create();context.setFormat(surface.format());
        ensure(context.create()&&context.makeCurrent(&surface),"Cannot create OpenGL context for GLB material baking.");
    }
    ~Context(){context.doneCurrent();if(previous&&previous_surface)previous->makeCurrent(previous_surface);}
};
}

GlbBakedMaps BakeGlbMaterial(RenderMesh& mesh,const Material& material,
    const QImage& color,const QImage& normal,const QImage& emission) {
    ensure(qobject_cast<QGuiApplication*>(QCoreApplication::instance())!=nullptr,
        "Procedural GLB export requires a GUI application for material baking.");
    ensure(!mesh.triangles.empty(),"Cannot bake an empty mesh.");
    const int grid=static_cast<int>(std::ceil(std::sqrt(double(mesh.triangles.size()))));
    const int resolution=2048;
    ensure(grid<=128,"Procedural surface is too dense for the GLB texture atlas; reduce its mesh density.");
    Context context;
    auto* gl=QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(&context.context);
    ensure(gl&&gl->initializeOpenGLFunctions(),"OpenGL 2.1 is required for GLB baking.");
    const char* vertex=R"GLSL(#version 120
varying vec3 materialPosition,eyePosition,bakeNormal;
varying vec2 textureUV;
varying mat3 materialNormalToEye;
void main(){gl_Position=gl_Vertex;materialPosition=gl_MultiTexCoord0.xyz;
eyePosition=materialPosition;bakeNormal=gl_Normal;textureUV=gl_MultiTexCoord1.xy;
materialNormalToEye=mat3(1.0);}
)GLSL";
    std::string fragment="#version 120\nvarying vec3 eyePosition,bakeNormal;\nvarying vec2 textureUV;\n";
    fragment+=PlasterShaderSource();fragment+=FabricShaderSource();fragment+=PerforationShaderSource();
    fragment+=R"GLSL(
uniform vec3 bakeColor;uniform int bakePass;
uniform sampler2D colorMap,normalMap,emissionMap;uniform bool hasColor,hasNormal;
uniform float normalStrength,bakeRoughness,bakeMetallic;
vec3 srgb(vec3 c){c=max(c,vec3(0));return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-.055,step(vec3(.0031308),c));}
void main(){vec3 n=normalize(bakeNormal);
vec2 uv=MaterialMappedUV(textureUV);
vec3 source=bakeColor;if(hasColor)source*=pow(texture2D(colorMap,uv).rgb,vec3(2.2));
if(hasNormal){
    vec3 dx=dFdx(materialPosition),dy=dFdy(materialPosition);vec2 ux=dFdx(uv),uy=dFdy(uv);
    vec3 t=normalize(dx*uy.y-dy*ux.y),b=normalize(-dx*uy.x+dy*ux.x);
    vec3 map=texture2D(normalMap,uv).rgb*2.0-1.0;map.xy*=normalStrength;
    n=normalize(t*map.x+b*map.y+n*map.z);
}
MaterialSample m;
if(proceduralPerforation){m.normal=PerforationNormal(n);m.baseColor=source;m.roughness=bakeRoughness;m.metallic=bakeMetallic;m.ao=1.0;}
else if(proceduralFabric)m=EvaluateFabric(n,source);else m=EvaluatePlaster(n,source);
vec3 value;
if(bakePass==0)value=srgb(clamp(m.baseColor,0.0,1.0));
else if(bakePass==1){
    vec3 base=normalize(bakeNormal);
    vec3 t=dFdx(materialPosition);t=normalize(t-base*dot(t,base));
    vec3 b=normalize(cross(base,t));if(dot(b,dFdy(materialPosition))<0.0)b=-b;
    value=vec3(dot(m.normal,t),dot(m.normal,b),dot(m.normal,base))*.5+.5;
}else if(bakePass==2)value=vec3(m.ao,m.roughness,m.metallic);
else value=texture2D(emissionMap,uv).rgb;
gl_FragColor=vec4(value,proceduralPerforation && bakePass==0 ? PerforationMask(n) : 1.0);}
)GLSL";
    QOpenGLShaderProgram shader;
    ensure(shader.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)
        &&shader.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment.c_str())&&shader.link(),
        "GLB bake shader: "+shader.log().toStdString());
    QOpenGLFramebufferObjectFormat format;format.setInternalTextureFormat(GL_RGBA8);
    QOpenGLFramebufferObject target(resolution,resolution,format);
    ensure(target.isValid()&&target.bind()&&shader.bind(),"Cannot allocate GLB texture atlas.");
    const QImage inputs[]{color,normal,emission};GLuint texture_ids[3]{};gl->glGenTextures(3,texture_ids);
    for(int i=0;i<3;++i)if(!inputs[i].isNull()){
        const QImage image=inputs[i].flipped(Qt::Vertical).convertToFormat(QImage::Format_RGBA8888);
        gl->glActiveTexture(GL_TEXTURE0+i);gl->glBindTexture(GL_TEXTURE_2D,texture_ids[i]);
        gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        gl->glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width(),image.height(),0,GL_RGBA,GL_UNSIGNED_BYTE,image.constBits());
    }
    gl->glActiveTexture(GL_TEXTURE0);
    shader.setUniformValue("colorMap",0);shader.setUniformValue("normalMap",1);shader.setUniformValue("emissionMap",2);
    shader.setUniformValue("hasColor",!color.isNull());shader.setUniformValue("hasNormal",!normal.isNull());
    shader.setUniformValue("normalStrength",material.normal_strength);
    shader.setUniformValue("bakeColor",QVector3D(material.diffuse.r,material.diffuse.g,material.diffuse.b));
    shader.setUniformValue("bakeRoughness",material.roughness);
    shader.setUniformValue("bakeMetallic",material.metallic);
    shader.setUniformValue("proceduralPerforation",material.perforation.enabled);
    shader.setUniformValue("perforationUseUV",material.perforation.useUV);
    shader.setUniformValue("perforationPattern",material.perforation.pattern);
#define PERFORATION_UNIFORM(name,value,lo,hi,label) shader.setUniformValue("perforation_" #name,material.perforation.name);
    DOM_PERFORATION_PARAMETERS(PERFORATION_UNIFORM)
#undef PERFORATION_UNIFORM
    shader.setUniformValue("proceduralFabric",material.fabric.enabled);
    shader.setUniformValue("fabricUseUV",material.fabric.useUV);
    shader.setUniformValue("fabricWeave",material.fabric.weave);
    shader.setUniformValue("fabricSeed",QVector2D(float(material.fabric.seed&65535),float(material.fabric.seed>>16)));
#define FABRIC_UNIFORM(name,value,lo,hi,label) shader.setUniformValue("fabric_" #name,material.fabric.name);
    DOM_FABRIC_PARAMETERS(FABRIC_UNIFORM)
#undef FABRIC_UNIFORM
    shader.setUniformValue("plasterSeed",QVector2D(float(material.plaster.seed&65535),float(material.plaster.seed>>16)));
    shader.setUniformValue("plasterHighQuality",material.plaster.highQuality);
#define PLASTER_UNIFORM(name,value,lo,hi,label) shader.setUniformValue("plaster_" #name,material.plaster.name);
    DOM_PLASTER_PARAMETERS(PLASTER_UNIFORM)
#undef PLASTER_UNIFORM
    Vec3 low=mesh.vertices.front(),high=low;
    for(const auto& p:mesh.vertices){low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
        high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);}
    const Vec3 center=(low+high)*.5f,extent=(high-low)*.5f;
    shader.setUniformValue("wrapObject",material.texture_wrap_object);
    shader.setUniformValue("wrapCenter",QVector3D(center.x,center.y,center.z));
    shader.setUniformValue("wrapExtent",QVector3D(extent.x,extent.y,extent.z));
    shader.setUniformValue("uvTransform",QVector4D(material.texture_offset_u,material.texture_offset_v,material.texture_scale_u,material.texture_scale_v));
    shader.setUniformValue("uvRotation",deg_to_rad(material.texture_rotation_degrees));
    gl->glViewport(0,0,resolution,resolution);gl->glDisable(GL_DEPTH_TEST);gl->glDisable(GL_BLEND);gl->glDisable(GL_DITHER);
    const float pad=2.5f*grid/resolution;
    GlbBakedMaps result;
    for(int pass=0;pass<(emission.isNull()?3:4);++pass){
        shader.setUniformValue("bakePass",pass);
        gl->glClearColor(pass==1?.5f:1,pass==1?.5f:1,1,1);gl->glClear(GL_COLOR_BUFFER_BIT);
        gl->glBegin(GL_QUADS);
        for(size_t i=0;i<mesh.triangles.size();++i){
            const auto& tri=mesh.triangles[i];const Vec3 p0=mesh.vertices[tri.vertices[0]],p1=mesh.vertices[tri.vertices[1]],p2=mesh.vertices[tri.vertices[2]];
            // Extrapolate into the cell's padding, so filtered samples at UV
            // island edges retain this triangle's material instead of black.
            for(const UV c: {UV{0,0},UV{1,0},UV{1,1},UV{0,1}}){
                const float u=(c.u-pad)/(1-2*pad),v=(c.v-pad)/(1-2*pad);
                const Vec3 p=p0+(p1-p0)*u+(p2-p0)*v;
                const Vec3 n=tri.normals[0]+(tri.normals[1]-tri.normals[0])*u+(tri.normals[2]-tri.normals[0])*v;
                const UV uv{tri.uvs[0].u+(tri.uvs[1].u-tri.uvs[0].u)*u+(tri.uvs[2].u-tri.uvs[0].u)*v,
                    tri.uvs[0].v+(tri.uvs[1].v-tri.uvs[0].v)*u+(tri.uvs[2].v-tri.uvs[0].v)*v};
                gl->glNormal3f(n.x,n.y,n.z);gl->glMultiTexCoord3f(GL_TEXTURE0,p.x,p.y,p.z);gl->glMultiTexCoord2f(GL_TEXTURE1,uv.u,uv.v);
                gl->glVertex2f(2*(float(i%grid)+c.u)/grid-1,2*(float(i/grid)+c.v)/grid-1);
            }
        }
        gl->glEnd();gl->glFinish();QImage image=target.toImage().convertToFormat(QImage::Format_RGBA8888);
        ensure(!image.isNull()&&gl->glGetError()==GL_NO_ERROR,"GLB procedural texture readback failed.");
        if(pass==0)result.color=image;else if(pass==1)result.normal=image;else if(pass==2)result.orm=image;else result.emission=image;
    }
    for(size_t i=0;i<mesh.triangles.size();++i){
        mesh.triangles[i].uvs={UV{(float(i%grid)+pad)/grid,(float(i/grid)+pad)/grid},
            UV{(float(i%grid)+1-pad)/grid,(float(i/grid)+pad)/grid},
            UV{(float(i%grid)+pad)/grid,(float(i/grid)+1-pad)/grid}};
    }
    gl->glDeleteTextures(3,texture_ids);
    return result;
}
