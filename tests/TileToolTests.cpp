#include "architecture/TileLayout.h"
#include "ui/ToolRegistry.h"
#include "ui/TileTool.h"
#include "ui/OpenGLViewport.h"
#include "ui/MainWindow.h"
#include <QAction>
#include <QSettings>
#include "CAssembled.h"
#include "Dom3DProjectSerializer.h"
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QSurfaceFormat>
#include <iostream>
#include <map>
#include <cstdlib>

namespace {
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<std::endl;std::exit(1);}}
std::vector<TileLayout::Point> footprint(const CMesh3D& mesh){
    std::vector<TileLayout::Point> p;
    for(auto it=mesh.GetFaces()[1].corners.rbegin();it!=mesh.GetFaces()[1].corners.rend();++it){auto v=mesh.GetVertices()[it->v];p.push_back({v.x,v.y});}return p;
}
}
int TestTile(int argc,char** argv){
    QSurfaceFormat format;format.setVersion(2,1);format.setProfile(QSurfaceFormat::CompatibilityProfile);QSurfaceFormat::setDefaultFormat(format);
    CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceMaterial);
    QApplication app(argc,argv);const bool render=app.arguments().contains("--render");
    for(int pattern=0;pattern<8;++pattern) for(double gap:{0.0,2.0}){
        TileLayout::Settings s;s.length=300;s.width=100;s.surface_length=730;s.surface_width=510;s.pattern=pattern;s.gap=gap;
        auto result=TileLayout::Build(s);check(result.error.empty()&&!result.tiles.empty(),"Tile build failed");
        double total=0;std::vector<std::vector<TileLayout::Point>> footprints;
        for(auto& tile:result.tiles){
            auto& mesh=*tile.mesh;auto p=footprint(mesh);total+=TileLayout::area(p);footprints.push_back(p);
            std::map<std::pair<size_t,size_t>,int> edges;
            for(auto& f:mesh.GetFaces())for(size_t i=0;i<f.corners.size();++i){size_t a=f.corners[i].v,b=f.corners[(i+1)%f.corners.size()].v;++edges[std::minmax(a,b)];}
            for(auto edge:edges)check(edge.second==2,"Tile mesh is not closed");
            for(auto v:mesh.GetVertices())check(v.x>=-.001&&v.x<=730.001&&v.y>=-.001&&v.y<=510.001&&v.z>=0&&v.z<=6.001,"Tile escapes surface bounds");
            for(auto uv:mesh.GetUVs())check(std::isfinite(uv.u)&&std::isfinite(uv.v),"Invalid UV");
        }
        for(size_t i=0;i<footprints.size();++i)for(size_t j=i+1;j<footprints.size();++j){auto overlap=footprints[i];auto& b=footprints[j];
            for(size_t k=0;k<b.size();++k){auto a=b[k],d=TileLayout::sub(b[(k+1)%b.size()],a);TileLayout::Point n{-d.y,d.x};overlap=TileLayout::clip(overlap,n,n.x*a.x+n.y*a.y);}
            check(std::abs(TileLayout::area(overlap))<.1,"Tiles overlap");
        }
        std::cout<<"pattern "<<pattern<<" gap "<<gap<<" tiles "<<result.tiles.size()<<" area "<<total<<std::endl;
        if(gap==0)check(std::abs(total-730*510)<.5,"Layout leaves uncovered area with zero gap");
        else check(total<730*510,"Gap absent");
        auto repeat=TileLayout::Build(s);check(repeat.tiles.size()==result.tiles.size(),"Unstable tile count");
        for(size_t i=0;i<result.tiles.size();++i){auto& a=result.tiles[i].mesh->GetUVs();auto& b=repeat.tiles[i].mesh->GetUVs();check(a.size()==b.size(),"UV size changed");for(size_t j=0;j<a.size();++j)check(a[j].u==b[j].u&&a[j].v==b[j].v,"UV changed during rebuild");}
    }
    for(int quantity:{0,1}){TileLayout::Settings s;s.quantity=quantity;auto r=TileLayout::Build(s);check(r.tiles.size()==(quantity==0?1:4),"Single/four tile mode incorrect");}
    {TileLayout::Settings s;s.length=0;check(!TileLayout::Build(s).error.empty(),"Invalid size accepted");s.length=1;s.width=1;s.gap=0;s.surface_length=100000;check(!TileLayout::Build(s).error.empty(),"Tile budget not enforced");}
    {
        CAlfaDoc materials;EnsureTileMaterials(materials);
        check(materials.FindMaterial("Tile Cream",true)->color_texture_path=="texture/tile/327.jpg","Cream tile texture missing");
        auto* old=materials.FindMaterial("Tile Terracotta",true);old->color_texture_path.clear();old->diffuse={.56f,.32f,.18f};
        auto id=old->id;EnsureTileMaterials(materials);
        check(materials.FindMaterial(id)->color_texture_path=="texture/tile/5230_31,6x31,6.jpg","Legacy tile material not upgraded");
        materials.FindMaterial(id)->color_texture_path="custom.jpg";EnsureTileMaterials(materials);
        check(materials.FindMaterial(id)->color_texture_path=="custom.jpg","Custom texture overwritten");
        TileLayout::Settings s;s.quantity=1;s.uv_mode=2;auto a=TileLayout::Build(s);s.seed=123;auto b=TileLayout::Build(s);
        bool changed=false;for(size_t i=0;i<a.tiles.size();++i)changed|=a.tiles[i].mesh->GetUVs()[0].u!=b.tiles[i].mesh->GetUVs()[0].u;
        check(changed,"Texture Seed does not affect UV rotation");
    }
    QTemporaryDir temp;ToolRegistry registry;Dom3DProjectSerializer serializer;
    QDir().mkpath("output/tile");
    for(int pattern=0;pattern<8;++pattern){
        CAlfaDoc document;auto params=registry.Find("tile")->defaults;
        for(auto& p:params){if(p.id=="pattern")p.value=pattern;if(p.id=="tile_length")p.value=400;if(p.id=="tile_width")p.value=200;}
        auto active=registry.CreateParametricObject("tile",document,params);
        auto* assembly=dynamic_cast<CAssembled*>(document.GetObjects()[active.object_index].get());check(assembly&&!assembly->GetElementIds().empty(),"Tile assembly missing");
        auto count=assembly->GetElementIds().size();assembly->Translate({10,20,30});registry.Rebuild(active,document);
        check(assembly->GetElementIds().size()==count,"Rebuild changed tile count");
        QString error,room;ProjectViewState state;const QString path=render?QString("output/tile/layout-%1.dom3d").arg(pattern):temp.filePath(QString("%1.dom3d").arg(pattern));
        check(serializer.Save(path,document,"Architecture",state,{},error),"Tile project save failed");
        CAlfaDoc loaded;check(serializer.Load(path,loaded,room,state,error),"Tile project load failed");
        auto* restored=dynamic_cast<CAssembled*>(loaded.GetObjects()[active.object_index].get());check(restored&&restored->GetParametricToolId()=="tile"&&restored->GetElementIds().size()==count,"Tile parameters lost on load");
        const auto& original=document.GetObjects();const auto& copy=loaded.GetObjects();
        for(size_t i=0;i<original.size();++i)if(auto* a=dynamic_cast<CMesh3D*>(original[i].get())){auto* b=dynamic_cast<CMesh3D*>(copy[i].get());check(b&&a->GetUVs().size()==b->GetUVs().size(),"Tile UV lost on load");for(size_t k=0;k<a->GetUVs().size();++k)check(a->GetUVs()[k].u==b->GetUVs()[k].u&&a->GetUVs()[k].v==b->GetUVs()[k].v,"Tile UV differs after load");}
        registry.Rebuild(registry.ActiveObjectFromDocument(active.object_index,*restored,0,&loaded),loaded);
        if(render){loaded.ClearSelection();OpenGLViewport viewport;CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceMaterial);viewport.SetDocument(&loaded);viewport.resize(1000,750);viewport.move(-20000,-20000);viewport.SetOrthographicProjection(true);viewport.show();viewport.FitToDocument();app.processEvents();check(viewport.grabFramebuffer().save(QString("output/tile/layout-%1.png").arg(pattern)),"Cannot save tile preview");viewport.SetDocument(nullptr);}
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    {
        MainWindow window;QAction* action=nullptr;
        for(auto* item:window.findChildren<QAction*>())if(item->property("toolKey").toString()=="tile")action=item;
        check(action,"Tile architecture action missing");action->trigger();
        PropertyPanel* panel=nullptr;
        for(auto* item:window.findChildren<PropertyPanel*>())if(item->ActiveObject().tool_id=="tile")panel=item;
        check(panel,"Tile panel did not open");
        for(auto& p:panel->ActiveObject().parameters)if(p.id=="pattern")check(p.options.size()==8,"Eight layouts missing from UI");
        panel->UpdateParameterValue("pattern",5);panel->ParametersChanged();
        panel->Canceled();
    }
    std::cout<<"Tile tests passed"<<std::endl;return 0;
}
