#include "PlasterBaker.h"
#include "ProceduralMaterialIO.h"
#include "ProceduralPlasterShader.h"
#include "../MaterialLibrary.h"

#include <QApplication>
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>
#include <QTemporaryDir>
#include <QUuid>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace {
struct BakeContext {
    QOpenGLContext* previous = QOpenGLContext::currentContext();
    QSurface* previousSurface = previous ? previous->surface() : nullptr;
    QOffscreenSurface surface;
    QOpenGLContext context;
    bool begin() {
        QSurfaceFormat format;
        format.setVersion(2, 1);
        format.setProfile(QSurfaceFormat::CompatibilityProfile);
        surface.setFormat(format); surface.create();
        context.setFormat(surface.format());
        return context.create() && context.makeCurrent(&surface);
    }
    ~BakeContext() {
        context.doneCurrent();
        if (previous && previousSurface) previous->makeCurrent(previousSurface);
    }
};
}

bool BakePlasterMaps(const Material& material, const PlasterBakeSettings& s,
                     const QString& parentDirectory, QString& packageDirectory,
                     QString& error, const std::function<bool(int, const QString&)>& progress) {
    packageDirectory.clear(); error.clear();
    Material validated;
    if (!material.plaster.enabled || !DecodePlaster(EncodePlaster(material), validated)) {
        error = "Enable Fine Plaster with valid parameters before baking."; return false;
    }
    auto finite = [](float v) { return std::isfinite(v); };
    if (s.resolution < 64 || s.resolution > 4096 || !finite(s.widthMm) || !finite(s.heightMm)
        || s.widthMm < .1f || s.heightMm < .1f || s.widthMm > 100000.f || s.heightMm > 100000.f
        || !finite(s.originMm.x) || !finite(s.originMm.y) || !finite(s.originMm.z)
        || !finite(material.diffuse.r) || !finite(material.diffuse.g) || !finite(material.diffuse.b)) {
        error = "Invalid bake size, origin, color or resolution (64–4096 pixels)."; return false;
    }
    if (!QDir(parentDirectory).exists()) { error = "The export directory does not exist."; return false; }
    if (progress && !progress(0, "Preparing PBR maps")) { error = "Canceled."; return false; }
    BakeContext glContext;
    if (!glContext.begin()) { error = "Cannot create an OpenGL context for baking."; return false; }
    auto* gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(&glContext.context);
    if (!gl || !gl->initializeOpenGLFunctions()) { error = "OpenGL 2.1 is required for baking."; return false; }
    GLint maxSize = 0; gl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (s.resolution > maxSize) { error = "The selected resolution exceeds the GPU limit."; return false; }

    // Exactly the same field, derivatives and filtering as the interactive material.
    const char* vertex = R"GLSL(#version 120
        varying vec3 materialPosition;
        varying mat3 materialNormalToEye;
        uniform vec3 bakeOrigin;
        uniform vec2 bakeSize;
        void main() {
            gl_Position = gl_Vertex;
            materialPosition = bakeOrigin + vec3((gl_Vertex.xy*.5+.5)*bakeSize, 0.0);
            materialNormalToEye = mat3(1.0);
        }
    )GLSL";
    std::string fragment = "#version 120\n";
    fragment += PlasterShaderSource();
    fragment += R"GLSL(
        uniform vec3 bakeColor;
        uniform int bakePass;
        uniform float heightAmplitude;
        void main() {
            MaterialSample sampleValue = EvaluatePlaster(vec3(0,0,1), bakeColor);
            vec3 value;
            if (bakePass == 0) value = clamp(sampleValue.baseColor, 0.0, 1.0);
            else if (bakePass == 1) value = sampleValue.normal*.5+.5;
            else if (bakePass == 2) value = vec3(sampleValue.normal.x,-sampleValue.normal.y,sampleValue.normal.z)*.5+.5;
            else if (bakePass == 3) value = vec3(sampleValue.roughness);
            else if (bakePass == 4) value = vec3(sampleValue.ao);
            else if (bakePass == 5) {
                vec3 p = materialPosition/plaster_scale;
                float footprint = max(length(dFdx(p)),length(dFdy(p)));
                float height = PlasterHeight(p,footprint)*plaster_scale*plaster_relief;
                float v = floor(clamp(.5+height/(2.0*heightAmplitude),0.0,1.0)*65535.0+.5);
                value = vec3(floor(v/256.0),mod(v,256.0),0.0)/255.0;
            } else value = vec3(0.0);
            gl_FragColor = vec4(value,1.0);
        }
    )GLSL";
    QOpenGLShaderProgram shader;
    if (!shader.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex)
        || !shader.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment.c_str()) || !shader.link()) {
        error = "PBR bake shader failed: " + shader.log(); return false;
    }
    QOpenGLFramebufferObjectFormat format; format.setInternalTextureFormat(GL_RGBA8);
    QOpenGLFramebufferObject framebuffer(s.resolution, s.resolution, format);
    if (!framebuffer.isValid() || !framebuffer.bind() || !shader.bind()) {
        error = "Cannot allocate the PBR bake framebuffer."; return false;
    }
    const auto& p = material.plaster;
    const float amplitude = std::max(.000001f, (p.macroStrength+p.grainStrength
        +(p.highQuality?p.microStrength:0.f)+p.poreDepth)*p.scale*p.relief);
    shader.setUniformValue("plasterSeed", QVector2D(float(p.seed&65535),float(p.seed>>16)));
    shader.setUniformValue("plasterHighQuality", p.highQuality);
#define BAKE_UNIFORM(name,defaultValue,lo,hi,label) shader.setUniformValue("plaster_" #name,p.name);
    DOM_PLASTER_PARAMETERS(BAKE_UNIFORM)
#undef BAKE_UNIFORM
    shader.setUniformValue("bakeOrigin", QVector3D(s.originMm.x,s.originMm.y,s.originMm.z));
    shader.setUniformValue("bakeSize", QVector2D(s.widthMm,s.heightMm));
    shader.setUniformValue("bakeColor", QVector3D(material.diffuse.r,material.diffuse.g,material.diffuse.b));
    shader.setUniformValue("heightAmplitude", amplitude);
    gl->glViewport(0,0,s.resolution,s.resolution);
    gl->glDisable(GL_DEPTH_TEST); gl->glDisable(GL_BLEND); gl->glDisable(GL_DITHER);

    QTemporaryDir staging(QDir(parentDirectory).filePath(".plaster-bake-XXXXXX"));
    if (!staging.isValid()) { error = "Cannot create the export directory."; return false; }
    const char* names[] = {"BaseColor", "NormalGL", "NormalDX", "Roughness", "AO", "Height", "Metallic"};
    for (int pass=0; pass<7; ++pass) {
        const bool keepGoing = !progress || progress(pass*100/7, QString("Baking %1").arg(names[pass]));
        // Progress may process GUI events and switch the current context.
        if (!glContext.context.makeCurrent(&glContext.surface) || !framebuffer.bind() || !shader.bind()) {
            error = "The bake context was lost."; return false;
        }
        if(!keepGoing) { error="Canceled."; return false; }
        shader.setUniformValue("bakePass",pass);
        gl->glBegin(GL_QUADS);
        gl->glVertex2f(-1,-1); gl->glVertex2f(1,-1); gl->glVertex2f(1,1); gl->glVertex2f(-1,1);
        gl->glEnd(); gl->glFinish();
        QImage image=framebuffer.toImage();
        if (image.isNull() || gl->glGetError()!=GL_NO_ERROR) { error="GPU readback failed."; return false; }
        if (pass==5) {
            QImage height(s.resolution,s.resolution,QImage::Format_Grayscale16);
            for(int y=0;y<s.resolution;++y) {
                auto* row=reinterpret_cast<quint16*>(height.scanLine(y));
                for(int x=0;x<s.resolution;++x){QRgb pixel=image.pixel(x,y);row[x]=quint16(qRed(pixel)*256+qGreen(pixel));}
            }
            image=height;
        } else if (pass>=3) image=image.convertToFormat(QImage::Format_Grayscale8);
        else image=image.convertToFormat(QImage::Format_RGB888);
        if(pass==0) image.setColorSpace(QColorSpace::SRgb);
        if(!image.save(staging.filePath(QString(names[pass])+".png"))) { error="Cannot write a PBR map."; return false; }
    }
    Material baked=material;
    baked.name=material.name+" (baked)"; baked.id=0; baked.plaster={}; baked.diffuse={1,1,1};
    baked.color_texture_path="BaseColor.png"; baked.normal_texture_path="NormalGL.png";
    baked.roughness_texture_path="Roughness.png"; baked.metallic_texture_path="Metallic.png";
    baked.roughness=p.roughness; baked.metallic=0; baked.normal_strength=1;
    // Height is exported with physical units, not assigned to the legacy UV parallax control.
    baked.light_texture_path.clear(); baked.bump_texture_path.clear(); baked.displacement_texture_path.clear();
    baked.source_file_path.clear(); baked.texture_offset_u=baked.texture_offset_v=baked.texture_rotation_degrees=0;
    baked.texture_scale_u=baked.texture_scale_v=1; baked.texture_fit_to_surface=false;
    MaterialLibrary library;
    if(!library.SaveMaterial(staging.filePath("FinePlaster.d3mat"),baked,&error))return false;
    QJsonObject metadata{{"version",1},{"generator","Dom3D Fine Plaster"},
        {"resolution",s.resolution},{"widthMm",s.widthMm},{"heightMm",s.heightMm},
        {"originMm",QJsonArray{s.originMm.x,s.originMm.y,s.originMm.z}},
        {"plane","XY; U=+X, V=+Y; image top is +Y"}, {"seamless",false},
        {"parameters",QJsonDocument::fromJson(EncodePlaster(material).toUtf8()).object()},
        {"baseColor",QJsonArray{material.diffuse.r,material.diffuse.g,material.diffuse.b}},
        {"baseColorSpace","sRGB; other maps are non-color data"},
        {"heightBits",16},{"heightMinMm",-amplitude},{"heightMaxMm",amplitude},
        {"heightDecode","millimeters = (sample16 / 65535 - 0.5) * (heightMaxMm - heightMinMm)"},
        {"normalGL","+Y tangent space"},{"normalDX","-Y tangent space"},
        {"note","Finite non-seamless region. Height and AO are exported for external renderers; not assigned to legacy Dom3D parallax."}};
    QFile manifest(staging.filePath("manifest.json"));
    if(!manifest.open(QIODevice::WriteOnly)||manifest.write(QJsonDocument(metadata).toJson())<0) {error="Cannot write bake metadata.";return false;}
    manifest.close();
    shader.release(); framebuffer.release();
    const bool finish = !progress || progress(100,"Finishing export");
    glContext.context.makeCurrent(&glContext.surface);
    if(!finish){error="Canceled.";return false;}
    const QString name="FinePlaster_"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QDir parent(parentDirectory);
    if(!parent.rename(QFileInfo(staging.path()).fileName(),name)){error="Cannot finish the export package.";return false;}
    staging.setAutoRemove(false); packageDirectory=parent.filePath(name); return true;
}
