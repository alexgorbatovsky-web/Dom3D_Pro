#pragma once
#include "CAlfaObject.h"
#include <array>
#include <functional>
class CAlfaDoc;
class CAssembled;
struct KitchenModule {
    int uid=1, row=0, type=0;
    double width=600, height=0;
    double depth=0;
    int facadeStyle=-1, shelves=2, openDrawer=0;
    int doorAxis=0;
    int handleType=-1, addLegs=-1, showcaseFill=4, drawerCount=0;
    double panelThickness=0;
    std::array<double,4> materials{-1,-1,-1,-1};
    double doorAngle=0, pullout=300;
    std::array<double,6> drawerRatios{1,1,1,1,1,1};
};
struct KitchenRow {
    int uid=0;
    bool upper=false;
    double x=0, y=0, angle=0;
    // A connected row follows the end and depth of its parent; angle is relative.
    int parent=-1;
};
struct KitchenRowFrame { double x=0,y=0,angle=0,length=0,depth=0; };
struct KitchenModulePlacement {
    int uid=0,row=0;
    double x=0,y=0,angle=0,width=0,depth=0,bottom=0,height=0;
    std::array<std::array<double,2>,4> corners;
};
struct KitchenLayout {
    double baseHeight=720, baseDepth=560, upperHeight=720, upperDepth=320;
    double legs=100, gap=550, top=38, upperOffset=0;
    int facadeStyle=1, handleType=0;
    bool makeLegs=true;
    bool showPlan=false;
    double plinthRecess=50;
    double panelThickness=18;
    std::array<double,4> materials{0,0,0,0};
    int Facade(const KitchenModule& m) const { return m.facadeStyle<0?facadeStyle:m.facadeStyle; }
    int HandleType(const KitchenModule& m) const { return m.handleType<0?handleType:m.handleType; }
    double Thickness(const KitchenModule& m) const { return m.panelThickness==0?panelThickness:m.panelThickness; }
    bool Legs(const KitchenModule& m) const;
    int Drawers(const KitchenModule& m) const { return m.drawerCount?m.drawerCount:m.type; }
    std::vector<KitchenModule> modules;
    std::vector<KitchenRow> rows{{0,false},{1,true}};
    static KitchenLayout Preset(int index);
    std::vector<ParametricParameterValue> Encode() const;
    static KitchenLayout Decode(const std::vector<ParametricParameterValue>& values);
    const KitchenRow* Row(int uid) const;
    KitchenRowFrame Frame(int uid) const;
    std::vector<KitchenModulePlacement> Placements() const;
    bool Validate(std::string& error) const;
};
// Types: single/double door, 2/3/4 drawers, stove, fridge, hood, gap, blind corner.
CAssembled* FindKitchenLayout(CAlfaDoc& doc, unsigned long member);
int FindKitchenModule(CAlfaDoc& doc, unsigned long member);
bool BuildKitchenLayout(CAlfaDoc& doc, const KitchenLayout& layout,
                        CAssembled* existing, std::string& error);

CAssembled* FindKitchenModuleObject(CAlfaDoc& doc, unsigned long member);
bool UpdateKitchenModuleParameters(CAlfaDoc& doc, unsigned long moduleId,
    const std::vector<ParametricParameterValue>& parameters, std::string& error);

unsigned long PickKitchenPlanModule(CAlfaDoc& doc, DomPoint point,
    const std::function<bool(Vec3, DomPoint&)>& project);
