#pragma once
#include "ToolRegistry.h"
#include "../architecture/TileLayout.h"
#include <QMessageBox>
#include <stdexcept>
struct TileBuildError : std::runtime_error { using std::runtime_error::runtime_error; };

inline void EnsureTileMaterials(CAlfaDoc& document) {
    for(int i=0;i<2;++i) {
        const std::string name=i?"Tile Cream":"Tile Terracotta";
        Material* existing=document.FindMaterial(name,true);
        const Color legacy=i?Color{.85f,.79f,.65f}:Color{.56f,.32f,.18f};
        if(existing && (!existing->color_texture_path.empty()
            || std::abs(existing->diffuse.r-legacy.r)>1e-5f
            || std::abs(existing->diffuse.g-legacy.g)>1e-5f
            || std::abs(existing->diffuse.b-legacy.b)>1e-5f))continue;
        Material material=existing?*existing:Material{};
        material.name=name;if(!existing)material.id=0;
        material.diffuse={1,1,1};
        material.color_texture_path=i?"texture/tile/327.jpg":"texture/tile/5230_31,6x31,6.jpg";
        if(!existing){material.ambient={.2f,.2f,.2f};material.roughness=.55f;material.shininess=24;material.specular=.2f;}
        const Material upgraded=document.UpsertMaterial(std::move(material));
        for(const auto& object:document.GetObjects())if(object&&object->GetMaterialId()==upgraded.id){
            object->SetMaterial(upgraded);object->SetColor(upgraded.diffuse);
        }

    }
}
inline std::vector<std::unique_ptr<CAlfaObject>> BuildTileParts(CAlfaDoc& document,const std::vector<ToolParameter>& parameters) {
    auto value=[&](const char* id,double fallback){for(const auto& p:parameters)if(p.id==id)return p.value;return fallback;};
    TileLayout::Settings s;
    s.length=value("tile_length",400);s.width=value("tile_width",400);s.thickness=value("thickness",6);
    s.gap=value("gap",2);s.bevel=value("bevel",3);s.surface_length=value("surface_length",3000);s.surface_width=value("surface_width",2000);
    s.pattern=int(value("pattern",0));s.quantity=int(value("quantity",2));s.uv_mode=int(value("uv_rotation",1));s.seed=int(value("seed",1));
    s.insert_size=value("insert_size",100);
    auto result=TileLayout::Build(s);
    if(!result.error.empty()){throw TileBuildError(result.error);}
    EnsureTileMaterials(document);
    std::vector<std::unique_ptr<CAlfaObject>> parts;
    for(auto& tile:result.tiles){
        const char* name=tile.material?"Tile Cream":"Tile Terracotta";
        const auto* material=document.FindMaterial(static_cast<unsigned long>(value(tile.material?"tile_material_b":"tile_material_a",0)));
        if(!material)material=document.FindMaterial(name,true);
        if(material){tile.mesh->SetMaterial(*material);tile.mesh->SetMaterialId(material->id);tile.mesh->SetColor(material->diffuse);}
        parts.push_back(std::move(tile.mesh));
    }
    return parts;
}
inline std::vector<ToolParameter> TileParameters() {
    return {
        {"pattern","Layout",0,0,7,1,ToolParameterType::Combo,{"Straight","Checkerboard","Running bond 1/2","Running bond 1/3","Diagonal 45 degrees","Herringbone","Herringbone 45 degrees","Two square sizes"}},
        {"quantity","Quantity",2,0,2,1,ToolParameterType::Combo,{"One tile","Four tiles","Fill surface"}},
        {"tile_length","Tile Length",400,1,10000,10,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"tile_width","Tile Width",400,1,10000,10,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"insert_size","Insert Size (two square sizes)",100,1,10000,10,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"thickness","Thickness",6,.1,100,1,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"gap","Gap",2,0,100,1,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"bevel","Bevel (max. half thickness)",3,0,50,.5,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"surface_length","Surface Length",3000,1,100000,100,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"surface_width","Surface Width",2000,1,100000,100,ToolParameterType::Number,{},ToolParameterUnit::Length},
        {"uv_rotation","Texture Rotation",1,0,2,1,ToolParameterType::Combo,{"Fixed","Random 0 / 90 / 180 / 270","Random angle"}},
        {"seed","Texture Seed (random rotations)",1,0,100000,1},
        {"tile_material_a","Tile Material A",0,0,1000000,1,ToolParameterType::Material},
        {"tile_material_b","Tile Material B (checkerboard / inserts)",0,0,1000000,1,ToolParameterType::Material}
    };
}
