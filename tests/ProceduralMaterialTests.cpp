#include "materials/ProceduralMaterialIO.h"
#include "materials/EnvironmentPrefilter.h"
#include "materials/PlasterBaker.h"
#include "materials/GlbMaterialBaker.h"
#include "render/RenderScene.h"
#include "ui/MaterialPreviewGL.h"
#include "ui/MaterialDrag.h"
#include "ui/OpenGLViewport.h"
#include "ui/MaterialEditorDialog.h"
#include "solid/Solid.h"
#include "MaterialLibrary.h"
#include "Dom3DProjectSerializer.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QMimeData>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <algorithm>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QMouseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <iostream>
#include <cstdlib>
#include <set>

namespace {
void require(bool value,const char* reason){if(!value){std::cerr<<reason<<std::endl;std::exit(1);}}
}
int TestProceduralMaterial(int argc,char** argv){
    QApplication app(argc,argv);
    if(app.arguments().contains("--perforation")) {
        QTemporaryDir dir;MaterialLibrary library;QString error;
        QDir().mkpath("output/perforation");
        QImage previous;
        for(int type=0;type<2;++type){
            Material m=Material::DefaultSurface();m.id=811+type;m.name=type?"Honeycomb":"Perforation";
            m.diffuse={.55f,.57f,.6f};m.metallic=.9f;m.roughness=.25f;
            m.perforation.enabled=true;m.perforation.pattern=type;
            if(type){m.perforation.holeSize=6;m.perforation.bridge=.6f;}
            const QString encoded=EncodePerforation(m);Material copy;
            require(DecodePerforation(encoded,copy)&&EncodePerforation(copy)==encoded,"Perforation JSON round trip failed");
            QMimeData mime;mime.setData(MaterialDrag::MimeType(),MaterialDrag::Encode(m));
            require(MaterialDrag::Decode(&mime,copy)&&EncodePerforation(copy)==encoded,"Drag/drop loses perforation");
            require(library.SaveMaterial(dir.filePath("pattern.d3mat"),m,&error)&&library.LoadMaterial(dir.filePath("pattern.d3mat"),copy,&error)&&EncodePerforation(copy)==encoded,"Library loses perforation");
            CAlfaDoc doc;doc.UpsertMaterial(m);Dom3DProjectSerializer serializer;ProjectViewState view;QString room;
            require(serializer.Save(dir.filePath("pattern.dom3d"),doc,room,view,{},error),"Perforation project save failed");
            CAlfaDoc loaded;require(serializer.Load(dir.filePath("pattern.dom3d"),loaded,room,view,error),"Perforation project load failed");
            const auto* saved=loaded.FindMaterial(m.name);require(saved&&EncodePerforation(*saved)==encoded,"Project loses perforation");
            const auto sphere=RenderMaterialSphereGL(m,480);
            require(!sphere.isNull()&&sphere!=previous,"Perforation shader missing or presets identical");
            sphere.save(QString("output/perforation/sphere-%1.png").arg(type));previous=sphere;
            const auto plate=RenderMaterialSphereGL(m,480,true);
            Material plain=m;plain.perforation.enabled=false;
            const auto solid=RenderMaterialSphereGL(plain,480,true);
            require(!plate.isNull()&&plate!=solid,"Perforation is an opaque surface");
            plate.save(QString("output/perforation/plate-%1.png").arg(type));solid.save("output/perforation/solid.png");
            const auto background=sphere.pixel(0,0);int holes=0,metal=0;
            for(int y=80;y<400;++y)for(int x=80;x<400;++x){
                if(solid.pixel(x,y)!=background){if(plate.pixel(x,y)==background)++holes;else ++metal;}
            }
            std::cout<<"holes="<<holes<<" metal="<<metal<<" background="<<background<<std::endl;
            require(holes>1000&&metal>1000,"Perforation must contain real background openings and solid bridges");
            plate.save(QString("output/perforation/plate-%1.png").arg(type));
            RenderMesh mesh;mesh.vertices={{0,0,0},{100,0,0},{0,100,0}};
            RenderTriangle tri;tri.vertices={0,1,2};tri.uvs={UV{0,0},UV{1,0},UV{0,1}};tri.normals={Vec3{0,0,1},Vec3{0,0,1},Vec3{0,0,1}};mesh.triangles.push_back(tri);
            const auto maps=BakeGlbMaterial(mesh,m);int transparent=0,opaque=0;
            for(int y=0;y<maps.color.height();y+=8)for(int x=0;x<maps.color.width();x+=8){int a=qAlpha(maps.color.pixel(x,y));if(a==0)++transparent;if(a==255)++opaque;}
            require(transparent>100&&opaque>100,"GLB bake loses hole alpha mask");
        }
        Material invalid;require(!DecodePerforation("{\"version\":1,\"useUV\":true,\"pattern\":2}",invalid),"Invalid perforation pattern accepted");
        require(DecodePerforation({},invalid)&&!invalid.perforation.enabled,"Legacy material became perforated");
        Material material=Material::DefaultSurface();material.id=812;material.name="Editor perforation";Material edited;int commits=0;
        MaterialEditorDialog editor(dir.path(),{material},false,&material);
        QObject::connect(&editor,&MaterialEditorDialog::SaveMaterialToDocument,[&](const Material& m){edited=m;++commits;});
        auto* button=editor.findChild<QPushButton*>("ProceduralPerforationButton");require(button,"Perforation editor missing");
        QTimer::singleShot(0,[&](){auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());require(dialog,"Perforation dialog missing");dialog->reject();});button->click();
        require(commits==0,"Cancel changed material");
        QTimer::singleShot(0,[&](){auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());require(dialog,"Perforation dialog missing");
            dialog->findChild<QComboBox*>("perforation_pattern")->setCurrentIndex(1);
            dialog->findChild<QDoubleSpinBox*>("perforation_bridge")->setValue(.7);
            QTimer::singleShot(250,dialog,[dialog](){dialog->grab().save("output/perforation/editor.png");dialog->accept();});
        });button->click();
        require(commits>0&&edited.perforation.enabled&&edited.perforation.pattern==1&&std::abs(edited.perforation.bridge-.7f)<.001f,"Editor loses perforation settings");
        require(edited.metallic>.8f,"Perforated metal lost metallic shading");
        std::cout<<"Perforation: viewport openings, presets, serialization, GLB alpha and editor checks passed\n";return 0;
    }
    if(app.arguments().contains("--hidden-edges-preview")) {
        Material material = Material::DefaultSurface();
        material.diffuse = {.7f,.12f,.08f};
        material.name = "Preview without hidden edges";
        CSolid::SetHiddenEdgeDrawingEnabled(false);
        const auto baseline = RenderMaterialSphereGL(material,192);
        require(!baseline.isNull(),"Material preview context failed");
        // Names participate in the cache key but not shading: force a fresh
        // render with the scene option enabled instead of comparing cache hits.
        material.name = "Preview with hidden edges";
        CSolid::SetHiddenEdgeDrawingEnabled(true);
        const auto hidden = RenderMaterialSphereGL(material,192);
        require(hidden == baseline,"Scene hidden edges corrupt the material sphere");
        require(CSolid::IsHiddenEdgeDrawingEnabled(),"Preview changed the scene hidden-edge option");
        material.name = "Preview hidden edges disabled again";
        CSolid::SetHiddenEdgeDrawingEnabled(false);
        require(RenderMaterialSphereGL(material,192) == baseline,"Preview changes after toggling hidden edges");
        require(!CSolid::IsHiddenEdgeDrawingEnabled(),"Preview enabled hidden edges");
        QDir().mkpath("output/material-preview");
        require(hidden.save("output/material-preview/hidden-edges-fixed.png"),"Cannot save preview verification");
        std::cout << "Material preview is independent of scene hidden edges\n";
        return 0;
    }
    if(app.arguments().contains("--environment-prefilter")) {
        QImage constant(64,32,QImage::Format_RGB32); constant.fill(qRgb(80,120,160));
        for(bool diffuse : {false,true}) {
            const QImage filtered=PrefilterEnvironment(constant,diffuse);
            for(int y=0;y<filtered.height();++y) for(int x=0;x<filtered.width();++x)
                require(filtered.pixel(x,y)==qRgb(80,120,160),"Environment filtering changed constant illumination");
        }
        QImage stripes(64,32,QImage::Format_RGB32);
        for(int y=0;y<32;++y) for(int x=0;x<64;++x)
            stripes.setPixel(x,y,(x%4<2)?qRgb(255,255,255):qRgb(0,0,0));
        for(bool diffuse : {false,true}) {
            const QImage filtered=PrefilterEnvironment(stripes,diffuse);
            int low=255,high=0;
            for(int y=0;y<filtered.height();++y) for(int x=0;x<filtered.width();++x) {
                const int value=qRed(filtered.pixel(x,y)); low=std::min(low,value); high=std::max(high,value);
            }
            require(high-low<8,"Fine HDRI features survived rough filtering");
        }
        require(PrefilterEnvironment({},true).isNull(),"Empty environment should stay empty");
        std::cout<<"Environment prefilter checks passed\n"; return 0;
    }
    if(app.arguments().contains("--plaster-presets")){
        QDir().mkpath("output/plaster/presets");
        QTemporaryDir temporary;QString error;MaterialLibrary library;
        QImage previous;
        for(int i=0;i<=6;++i){
            Material preset;preset.diffuse={.78f,.73f,.64f};preset.plaster=PlasterPreset(i);
            Material decoded;require(DecodePlaster(EncodePlaster(preset),decoded)&&EncodePlaster(decoded)==EncodePlaster(preset),"Preset serialization failed");
            auto image=RenderMaterialSphereGL(preset,480,true);
            require(!image.isNull()&&image!=previous,"Preset is missing or identical to previous");
            require(image.save(QString("output/plaster/presets/type-%1.png").arg(i)),"Cannot save preset image");
            previous=image;
            require(library.SaveMaterial(temporary.filePath("preset.d3mat"),preset,&error)&&library.LoadMaterial(temporary.filePath("preset.d3mat"),decoded,&error)&&EncodePlaster(decoded)==EncodePlaster(preset),"Saved preset changed");
            PlasterBakeSettings settings;settings.resolution=256;settings.widthMm=60;settings.heightMm=60;
            QString package;require(BakePlasterMaps(preset,settings,temporary.path(),package,error),qPrintable(error));
            QImage normals(QDir(package).filePath("NormalGL.png"));require(!normals.isNull(),"Missing preset normals");
        }
        Material legacy;require(DecodePlaster("{\"version\":1}",legacy)&&legacy.plaster.pattern==0,"Old plaster changed type");
        require(!DecodePlaster("{\"version\":1,\"pattern\":1.5}",legacy),"Fractional type accepted");
        Material material;material.id=701;material.diffuse={.78f,.73f,.64f};material.plaster=PlasterPreset(0);Material edited;
        MaterialEditorDialog editor(temporary.path(),{material},false,&material);
        QObject::connect(&editor,&MaterialEditorDialog::SaveMaterialToDocument,[&](const Material& value){edited=value;});
        QTimer::singleShot(0,[&](){
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());require(dialog,"Plaster dialog missing");
            auto* presets=dialog->findChild<QListWidget*>("plaster_presets");require(presets&&presets->count()==7,"Preset gallery missing");
            presets->setCurrentRow(6);
            QTimer::singleShot(500,dialog,[dialog](){dialog->grab().save("output/plaster/presets/editor.png");dialog->accept();});
        });
        editor.findChild<QPushButton*>("ProceduralPlasterButton")->click();
        require(edited.plaster.pattern==6 && edited.plaster.patternDepth==PlasterPreset(6).patternDepth,"Gallery does not apply preset");
        std::cout<<"Plaster presets rendered and serialized"<<std::endl;return 0;
    }
    Material m;m.name="Fine Plaster";m.diffuse={.84f,.84f,.82f};m.plaster.enabled=true;
    m.plaster.seed=4294967295u;m.plaster.highQuality=true;m.plaster.grainSize=.7f;
    QString encoded=EncodePlaster(m);Material copy;
    require(DecodePlaster(encoded,copy)&&EncodePlaster(copy)==encoded,"Parameter round trip failed");
    require(!DecodePlaster("{\"version\":1,\"grainSize\":0}",copy),"Invalid grain size accepted");
    require(!DecodePlaster("{\"version\":2}",copy),"Unknown version accepted");
    require(DecodePlaster({},copy)&&!copy.plaster.enabled,"Legacy material became procedural");
    QMimeData mime;mime.setData(MaterialDrag::MimeType(),MaterialDrag::Encode(m));
    require(MaterialDrag::Decode(&mime,copy)&&EncodePlaster(copy)==encoded,"Drag/drop loses plaster");
    QTemporaryDir dir;QString error;MaterialLibrary library;
    require(library.SaveMaterial(dir.filePath("plaster.mat"),m,&error),"Library save failed");
    require(library.LoadMaterial(dir.filePath("plaster.mat"),copy,&error)&&EncodePlaster(copy)==encoded,"Library loses plaster");
    CAlfaDoc doc;doc.UpsertMaterial(m);Dom3DProjectSerializer serializer;ProjectViewState view;QString room;
    require(serializer.Save(dir.filePath("plaster.dom3d"),doc,room,view,{},error),"Project save failed");
    CAlfaDoc loaded;require(serializer.Load(dir.filePath("plaster.dom3d"),loaded,room,view,error),"Project load failed");
    auto* saved=loaded.FindMaterial("Fine Plaster");require(saved&&EncodePlaster(*saved)==encoded,"Project loses plaster");
    Material fabric;fabric.name="Linen";fabric.diffuse={.72f,.65f,.53f};fabric.fabric=FabricPreset(0);
    fabric.fabric.seed=4294967295u;
    fabric.fabric.useUV=true;fabric.fabric.uvSize=75;fabric.texture_wrap_object=true;
    const QString fabricEncoded=EncodeFabric(fabric);
    require(DecodeFabric(fabricEncoded,copy)&&EncodeFabric(copy)==fabricEncoded,"Fabric parameter round trip failed");
    require(!DecodeFabric("{\"version\":1,\"weave\":8,\"seed\":1}",copy),"Invalid fabric type accepted");
    require(!DecodeFabric("{\"version\":1,\"weave\":0,\"seed\":1,\"scale\":0}",copy),"Invalid fabric scale accepted");
    require(DecodeFabric({},copy)&&!copy.fabric.enabled,"Legacy material became fabric");
    mime.setData(MaterialDrag::MimeType(),MaterialDrag::Encode(fabric));
    require(MaterialDrag::Decode(&mime,copy)&&EncodeFabric(copy)==fabricEncoded,"Drag/drop loses fabric");
    require(copy.texture_wrap_object,"Drag/drop loses object wrap mapping");
    require(library.SaveMaterial(dir.filePath("linen.d3mat"),fabric,&error),"Fabric library save failed");
    require(library.LoadMaterial(dir.filePath("linen.d3mat"),copy,&error)&&EncodeFabric(copy)==fabricEncoded,"Library loses fabric");
    require(copy.texture_wrap_object,"Library loses object wrap mapping");
    doc.UpsertMaterial(fabric);
    require(serializer.Save(dir.filePath("fabric.dom3d"),doc,room,view,{},error),"Fabric project save failed");
    require(serializer.Load(dir.filePath("fabric.dom3d"),loaded,room,view,error),"Fabric project load failed");
    saved=loaded.FindMaterial("Linen");require(saved&&EncodeFabric(*saved)==fabricEncoded,"Project loses fabric");
    require(saved->texture_wrap_object,"Project loses object wrap mapping");
    fabric.texture_wrap_object=false;
    if(app.arguments().contains("--render")){
        CMesh3D::ReloadLightingSettings();
        CMesh3D::SetOpenEdgeDisplayEnabled(true);CMesh3D::SetSurfaceOpacity(.4f);
        QDir().mkpath("output/plaster");QElapsedTimer timer;timer.start();
        auto image=RenderMaterialSphereGL(m,640);require(!image.isNull(),"OpenGL preview failed");
        require(CMesh3D::IsOpenEdgeDisplayEnabled()&&CMesh3D::GetSurfaceOpacity()==.4f,"Preview changes scene display settings");
        int redPixels=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){auto c=image.pixel(x,y);if(qRed(c)>180&&qGreen(c)<70&&qBlue(c)<70)++redPixels;}
        require(redPixels==0,"Open-edge diagnostic stripe appears on the material sphere");
        CMesh3D::SetOpenEdgeDisplayEnabled(false);CMesh3D::SetSurfaceOpacity(1.f);
        std::cout<<"First render ms: "<<timer.elapsed()<<std::endl;timer.restart();
        require(RenderMaterialSphereGL(m,640)==image,"Cached image changed");
        std::cout<<"Cached render ms: "<<timer.elapsed()<<std::endl;
        require(image.save("output/plaster/fine-plaster.png"),"Cannot save preview");
        m.plaster.seed=42;auto seed=RenderMaterialSphereGL(m,640);require(seed!=image,"Seed does not affect shader");
        seed.save("output/plaster/seed-42.png");
        m.plaster.relief=0;auto flat=RenderMaterialSphereGL(m,640);require(flat!=seed,"Relief does not affect shader");flat.save("output/plaster/relief-zero.png");
        m.plaster.enabled=false;auto plain=RenderMaterialSphereGL(m,640);require(plain!=flat,"Procedural shader is inactive");plain.save("output/plaster/plain.png");
        QImage texture(32,32,QImage::Format_RGB32);for(int y=0;y<32;++y)for(int x=0;x<32;++x)texture.setPixel(x,y,((x/8+y/8)%2)?qRgb(180,70,35):qRgb(220,210,180));
        texture.save(dir.filePath("checker.png"));Material textured=m;textured.source_file_path=dir.filePath("sample.d3mat").toStdString();textured.color_texture_path="checker.png";
        auto texturedSphere=RenderMaterialSphereGL(textured,640);require(texturedSphere!=plain,"Relative material texture missing from OpenGL sphere");texturedSphere.save("output/plaster/textured-sphere.png");
        m.plaster.enabled=true;m.plaster.relief=1;m.plaster.highQuality=true;
        CAlfaDoc wall;auto mesh=std::make_unique<CMesh3D>("Plaster wall");auto* meshPtr=mesh.get();
        auto geometry=[&](int divisions){std::vector<Vec3> vertices;std::vector<CMesh3D::Face> faces;
            for(int y=0;y<=divisions;++y)for(int x=0;x<=divisions;++x)vertices.push_back({-300.f+600.f*x/divisions,-200.f+400.f*y/divisions,0});
            for(int y=0;y<divisions;++y)for(int x=0;x<divisions;++x){size_t a=y*(divisions+1)+x,b=a+divisions+1;CMesh3D::Face f;
                for(auto id:{a,a+1,b+1,b})f.corners.push_back({id,0,0});faces.push_back(f);}
            meshPtr->SetGeometry(std::move(vertices),std::move(faces),{{0,0}},{{0,0,1}});};
        m=wall.UpsertMaterial(m);geometry(1);mesh->SetMaterial(m);mesh->SetColor(m.diffuse);wall.AddObject(std::move(mesh),false);wall.ClearSelection();
        OpenGLViewport viewport;viewport.SetDocument(&wall);viewport.resize(800,600);viewport.move(-20000,-20000);viewport.SetOrthographicProjection(true);viewport.SetCoordinateAxesVisible(false);viewport.SetFloorGridVisible(false);
        CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceMaterial);CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        Camera camera;camera.orientation={1,0,0,0};camera.target={0,0,0};camera.distance=60;viewport.SetCamera(camera);viewport.show();app.processEvents();
        viewport.makeCurrent();std::cout<<"GPU: "<<reinterpret_cast<const char*>(viewport.context()->functions()->glGetString(GL_RENDERER))<<std::endl;viewport.doneCurrent();
        auto close=viewport.CaptureSceneImage({800,600});close.save("output/plaster/wall-close.png");
        geometry(20);auto dense=viewport.CaptureSceneImage({800,600});
        double difference=0;for(int y=20;y<580;++y)for(int x=20;x<780;++x)difference+=std::abs(qGray(close.pixel(x,y))-qGray(dense.pixel(x,y)));
        require(difference/(560*760)<1.0,"Plaster depends on mesh subdivision");
        camera.distance=900;viewport.SetCamera(camera);viewport.CaptureSceneImage({800,600}).save("output/plaster/wall-distant.png");
        camera.distance=60;viewport.SetCamera(camera);auto returned=viewport.CaptureSceneImage({800,600});require(returned==dense,"Camera movement changed material field");
        for(int quality=0;quality<3;++quality){m.plaster.enabled=quality>0;m.plaster.highQuality=quality==2;meshPtr->SetMaterial(m);
            viewport.CaptureSceneImage({800,600});std::vector<double> times;
            for(int frame=0;frame<20;++frame){timer.restart();viewport.CaptureSceneImage({800,600});times.push_back(timer.nsecsElapsed()/1000000.0);}
            std::sort(times.begin(),times.end());std::cout<<"Wall 800x600 quality "<<quality<<" render+readback median "<<times[10]<<" ms p95 "<<times[18]<<" ms"<<std::endl;
        }
        int pickedCount=0;
        QObject::connect(&viewport,&OpenGLViewport::MaterialPicked,[&](const Material& picked){
            require(EncodePlaster(picked)==EncodePlaster(m),"Picker lost plaster parameters");++pickedCount;
        });
        for(auto tool:{ToolMode::Orbit,ToolMode::ZoomRect,ToolMode::Select}) {
            viewport.SetTool(tool);viewport.BeginMaterialPick();
            const QPointF point(viewport.width()/2.0,viewport.height()/2.0);
            QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&press);
            QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&viewport,&release);
        }
        require(pickedCount==3,"Material picker was intercepted by the active tool");
        QDir().mkpath("output/fabric");
        QImage previousFabric;
        for(int weave=0;weave<4;++weave){
            fabric.fabric=FabricPreset(weave);
            auto rendered=RenderMaterialSphereGL(fabric,640);
            require(!rendered.isNull()&&rendered!=previousFabric,"Fabric presets have no rendered effect");
            require(RenderMaterialSphereGL(fabric,640)==rendered,"Fabric preview is not deterministic");
            rendered.save(QString("output/fabric/preset-%1.png").arg(weave));previousFabric=rendered;
            meshPtr->SetMaterial(fabric);camera.distance=18;viewport.SetCamera(camera);
            viewport.CaptureSceneImage({800,600}).save(QString("output/fabric/weave-%1.png").arg(weave));
        }
        fabric.fabric=FabricPreset(0);auto linen=RenderMaterialSphereGL(fabric,640);
        QImage print(64,64,QImage::Format_RGB32);
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)print.setPixel(x,y,((x/16+y/16)%2)?qRgb(190,25,25):qRgb(240,235,220));
        print.save(dir.filePath("fabric-print.png"));
        Material printed=fabric;printed.diffuse={1,1,1};printed.color_texture_path=dir.filePath("fabric-print.png").toStdString();
        printed.fabric.useUV=true;printed.fabric.uvSize=50;printed.texture_wrap_object=true;
        const auto printedImage=RenderMaterialSphereGL(printed,640);
        require(!printedImage.isNull()&&printedImage!=linen,"Fabric color texture is ignored");
        printedImage.save("output/fabric/printed-wrap.png");
        Material unprinted=printed;unprinted.color_texture_path.clear();
        require(RenderMaterialSphereGL(unprinted,640)!=printedImage,"Color map does not affect procedural fabric");
        Material flatPrint=printed;flatPrint.fabric.relief=0;
        require(RenderMaterialSphereGL(flatPrint,640)!=printedImage,"Color map disabled the procedural relief");
        printed.texture_wrap_object=false;
        require(RenderMaterialSphereGL(printed,640)!=printedImage,"Object wrap mapping has no effect");
        fabric.fabric.seed=57;require(RenderMaterialSphereGL(fabric,640)!=linen,"Fabric seed has no effect");
        fabric.fabric=FabricPreset(0);fabric.fabric.scale=2;require(RenderMaterialSphereGL(fabric,640)!=linen,"Fabric scale has no effect");
        fabric.fabric=FabricPreset(0);fabric.fabric.relief=0;require(RenderMaterialSphereGL(fabric,640)!=linen,"Fabric relief has no effect");
        fabric.fabric=FabricPreset(0);fabric.fabric.rotation=45;require(RenderMaterialSphereGL(fabric,640)!=linen,"Fabric angle has no effect");
        fabric.fabric=FabricPreset(0);meshPtr->SetMaterial(fabric);geometry(1);
        auto fabricCoarse=viewport.CaptureSceneImage({800,600});geometry(20);
        auto fabricDense=viewport.CaptureSceneImage({800,600});double fabricDifference=0;
        for(int y=20;y<580;++y)for(int x=20;x<780;++x)fabricDifference+=std::abs(qGray(fabricCoarse.pixel(x,y))-qGray(fabricDense.pixel(x,y)));
        require(fabricDifference/(560*760)<1.0,"Fabric depends on mesh subdivision");
        // A rounded cushion with many surface directions: color and weave
        // must use the same whole-object chart, including the curved sides.
        std::vector<Vec3> cushionVertices,cushionNormals;std::vector<UV> cushionUV;
        std::vector<CMesh3D::Face> cushionFaces;
        auto signedRoot=[](float v){return std::copysign(std::sqrt(std::abs(v)),v);};
        for(int y=0;y<=48;++y)for(int x=0;x<=96;++x){
            const float theta=3.14159265f*y/48,phi=6.2831853f*x/96;
            const float a=signedRoot(std::sin(theta)*std::cos(phi)),b=signedRoot(std::sin(theta)*std::sin(phi)),c=signedRoot(std::cos(theta));
            cushionVertices.push_back({60*a,45*b,12*c});cushionNormals.push_back(normalize(Vec3{a*a*a/60,b*b*b/45,c*c*c/12}));cushionUV.push_back({float(x)/96,float(y)/48});
        }
        for(size_t y=0;y<48;++y)for(size_t x=0;x<96;++x){size_t a=y*97+x,b=a+97;CMesh3D::Face face;
            for(size_t i:{a,b,b+1,a+1})face.corners.push_back({i,i,i});cushionFaces.push_back(face);}
        require(meshPtr->SetGeometry(cushionVertices,cushionFaces,cushionUV,cushionNormals),"Cushion geometry failed");
        printed.texture_wrap_object=true;printed.texture_scale_u=4;printed.texture_scale_v=4;printed.fabric.uvSize=30;
        meshPtr->SetMaterial(printed);camera.orientation={.753375f,.367205f,-.169699f,-.518449f};camera.distance=180;viewport.SetCamera(camera);
        const auto cushionImage=viewport.CaptureSceneImage({800,600});require(!cushionImage.isNull(),"Cushion render failed");cushionImage.save("output/fabric/printed-cushion.png");
        viewport.SetDocument(nullptr);
        m.id=701;m.plaster.enabled=true;
        MaterialEditorDialog editor(dir.path(),{m},false,&m);Material edited;
        QObject::connect(&editor,&MaterialEditorDialog::SaveMaterialToDocument,[&](const Material& value){edited=value;});
        QTimer::singleShot(0,[&](){auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());require(dialog,"Plaster dialog did not open");
            auto* scale=dialog->findChild<QDoubleSpinBox*>("plaster_scale");require(scale,"Scale control missing");
            scale->setValue(1.0);scale->stepUp();require(std::abs(scale->value()-1.01)<0.00001,"Scale step is too coarse");
            auto* grain=dialog->findChild<QDoubleSpinBox*>("plaster_grainSize");require(grain,"Grain control missing");grain->setValue(1.25);
            QTimer::singleShot(350,dialog,[dialog](){dialog->grab().save("output/plaster/editor.png");dialog->accept();});});
        editor.findChild<QPushButton*>("ProceduralPlasterButton")->click();
        require(edited.plaster.enabled&&edited.plaster.grainSize==1.25f,"Editor does not commit plaster parameters");
        require(editor.findChild<QPushButton*>("ProceduralPlasterButton"),"Plaster UI missing");
        QTimer::singleShot(0,[&](){auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());require(dialog,"Fabric dialog did not open");
            auto* preset=dialog->findChild<QComboBox*>("fabric_preset");require(preset,"Fabric presets missing");
            dialog->findChild<QCheckBox*>("fabric_useUV")->setChecked(true);
            preset->setCurrentIndex(2);preset->activated(2);
            QTimer::singleShot(350,dialog,[dialog](){dialog->grab().save("output/fabric/editor.png");dialog->accept();});});
        editor.findChild<QPushButton*>("ProceduralFabricButton")->click();
        require(edited.fabric.enabled&&edited.fabric.weave==2&&!edited.plaster.enabled,"Fabric editor failed to switch the surface source");
        require(edited.fabric.useUV,"Fabric UV mode is not committed");
        PlasterBakeSettings bakeSettings;bakeSettings.resolution=256;bakeSettings.widthMm=24;bakeSettings.heightMm=16;bakeSettings.originMm={2,3,4};
        QDir().mkpath("output/plaster/bakes");QString package,repeat;
        require(BakePlasterMaps(m,bakeSettings,"output/plaster/bakes",package,error),qPrintable(error));
        std::cout<<"PBR package: "<<package.toStdString()<<std::endl;
        const char* maps[]={"BaseColor","NormalGL","NormalDX","Roughness","AO","Height","Metallic"};
        for(auto* name:maps){QImage map(QDir(package).filePath(QString(name)+".png"));require(map.size()==QSize(256,256),"Baked map dimensions incorrect");}
        QImage heightMap(QDir(package).filePath("Height.png"));require(heightMap.format()==QImage::Format_Grayscale16,"Height PNG lost 16-bit precision");
        QImage normalGL(QDir(package).filePath("NormalGL.png")),normalDX(QDir(package).filePath("NormalDX.png"));
        QImage ao(QDir(package).filePath("AO.png")),metal(QDir(package).filePath("Metallic.png")),base(QDir(package).filePath("BaseColor.png"));
        std::set<quint16> heights;
        for(int y=0;y<256;++y)for(int x=0;x<256;++x){
            heights.insert(reinterpret_cast<const quint16*>(heightMap.constScanLine(y))[x]);
            QRgb a=normalGL.pixel(x,y),b=normalDX.pixel(x,y);
            require(qRed(a)==qRed(b)&&qBlue(a)==qBlue(b)&&std::abs(qGreen(a)+qGreen(b)-255)<=1,"OpenGL/DirectX normal convention differs incorrectly");
            require(qGray(ao.pixel(x,y))>=178&&qGray(metal.pixel(x,y))==0,"Invalid AO or metallic bake");
            require(std::abs(qRed(base.pixel(x,y))/255.f-m.diffuse.r)<=m.diffuse.r*m.plaster.colorVariation+.005f,"Base color contains illumination");
        }
        require(heights.size()>256,"Height has only 8-bit precision");
        require(library.LoadMaterial(QDir(package).filePath("FinePlaster.d3mat"),copy,&error)&&!copy.plaster.enabled&&copy.normal_texture_path=="NormalGL.png","Baked material is not importable");
        require(RenderMaterialSphereGL(copy,320).save("output/plaster/baked-sphere.png"),"Imported baked material preview failed");
        copy=wall.UpsertMaterial(copy);meshPtr->SetMaterial(copy);viewport.SetDocument(&wall);
        auto relativeScene=viewport.CaptureSceneImage({800,600});Material absolute=copy;
        auto makeAbsolute=[&](std::string& path){if(!path.empty())path=QDir(QFileInfo(QString::fromStdString(copy.source_file_path)).absolutePath()).absoluteFilePath(QString::fromStdString(path)).toStdString();};
        makeAbsolute(absolute.color_texture_path);makeAbsolute(absolute.normal_texture_path);makeAbsolute(absolute.roughness_texture_path);makeAbsolute(absolute.metallic_texture_path);
        absolute.source_file_path.clear();meshPtr->SetMaterial(absolute);
        require(relativeScene==viewport.CaptureSceneImage({800,600}),"Relative package maps fail in the scene renderer");
        meshPtr->SetMaterial(copy);viewport.SetDocument(nullptr);
        require(serializer.Save(dir.filePath("baked-scene.dom3d"),wall,room,view,{},error),"Baked scene save failed");
        CAlfaDoc restoredBake;require(serializer.Load(dir.filePath("baked-scene.dom3d"),restoredBake,room,view,error),"Baked scene load failed");
        const auto* savedBake=restoredBake.FindMaterial(copy.id);require(savedBake&&savedBake->source_file_path==copy.source_file_path,"Project loses the map directory");
        require(BakePlasterMaps(m,bakeSettings,dir.path(),repeat,error),qPrintable(error));
        for(auto* name:maps)require(QImage(QDir(package).filePath(QString(name)+".png"))==QImage(QDir(repeat).filePath(QString(name)+".png")),"Bake is not deterministic");
        m.plaster.seed+=1;QString other;require(BakePlasterMaps(m,bakeSettings,dir.path(),other,error),qPrintable(error));
        require(QImage(QDir(other).filePath("NormalGL.png"))!=normalGL,"Baked seed has no effect");
        const auto before=QDir(dir.path()).entryList(QDir::AllEntries|QDir::Hidden|QDir::NoDotAndDotDot);
        require(!BakePlasterMaps(m,bakeSettings,dir.path(),other,error,[](int value,const QString&){return value<14;})&&error=="Canceled."&&other.isEmpty(),"Bake cancel failed");
        require(QDir(dir.path()).entryList(QDir::AllEntries|QDir::Hidden|QDir::NoDotAndDotDot)==before,"Canceled export leaves incomplete files");
        // Check normal direction against independent finite differences of the exported height.
        Material smooth=m;smooth.plaster.grainSize=2;smooth.plaster.highQuality=false;smooth.plaster.poreDepth=0;
        require(BakePlasterMaps(smooth,bakeSettings,dir.path(),other,error),qPrintable(error));
        QImage smoothHeight(QDir(other).filePath("Height.png")),smoothNormal(QDir(other).filePath("NormalGL.png"));
        QFile metadata(QDir(other).filePath("manifest.json"));require(metadata.open(QIODevice::ReadOnly),"Bake metadata missing");
        auto meta=QJsonDocument::fromJson(metadata.readAll()).object();double range=meta["heightMaxMm"].toDouble()-meta["heightMinMm"].toDouble();
        auto sampleHeight=[&](int x,int y){return reinterpret_cast<const quint16*>(smoothHeight.constScanLine(y))[x]/65535.0*range;};
        double normalError=0;
        for(int y=1;y<255;++y)for(int x=1;x<255;++x){double dx=(sampleHeight(x+1,y)-sampleHeight(x-1,y))/(2*bakeSettings.widthMm/256.0);
            double dy=(sampleHeight(x,y-1)-sampleHeight(x,y+1))/(2*bakeSettings.heightMm/256.0);double length=std::sqrt(dx*dx+dy*dy+1);
            auto pixel=smoothNormal.pixel(x,y);normalError+=std::abs(qRed(pixel)/255.0*2-1+dx/length)+std::abs(qGreen(pixel)/255.0*2-1+dy/length);}
        require(normalError/(254*254)<.025,"Height and tangent normals disagree");
        if(app.arguments().contains("--bake-large")){
            PlasterBakeSettings large;large.resolution=2048;QString largePackage;
            require(BakePlasterMaps(m,large,"output/plaster/bakes",largePackage,error),qPrintable(error));
            require(QImage(QDir(largePackage).filePath("Height.png")).size()==QSize(2048,2048),"2K bake failed");
            std::cout<<"2K PBR package: "<<largePackage.toStdString()<<std::endl;
        }
        bakeSettings.resolution=8192;require(!BakePlasterMaps(m,bakeSettings,dir.path(),other,error),"Unsafe resolution accepted");
    }
    std::cout<<"Procedural material tests passed"<<std::endl;return 0;
}
