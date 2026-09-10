#include "KitchenLayout.h"
#include "CAlfaDoc.h"
#include "CAssembled.h"
#include "CKitchenCabinet.h"
#include "CFurnitureAssemblies.h"
#include "FurnitureMaterialFactory.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"
#include "CPolyline.h"
#include "DrawingText.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <functional>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>

namespace { void alignCornerHood(KitchenLayout& kitchen); }
KitchenLayout KitchenLayout::Preset(int index) {
    KitchenLayout k;
    auto add=[&](int row,int type,double width,double height=0) {
        k.modules.push_back({int(k.modules.size()+1),row,type,width,height});
    };
    if(index==0) {
        add(0,0,600); add(0,5,600); add(0,3,600); add(0,0,600); add(0,6,600,1850);
        add(1,0,600); add(1,7,600,360); add(1,0,600); add(1,0,600); add(1,0,600,250);
    } else if(index==1) {
        add(0,0,400); add(0,4,400); add(0,2,600); add(0,1,800); add(0,2,600);
        add(1,1,800); add(1,7,600,360); add(1,1,800); add(1,1,600);
    } else if(index==2) {
        add(0,2,600); add(0,0,400); add(0,5,600); add(0,4,400); add(0,1,800);
        add(1,1,1000); add(1,7,600,0); add(1,1,1200);
    }
    if(index>=3) {
        k.rows={{0,false},{1,true},{2,false,0,0,-90,0},{3,true,0,0,-90,1}};
        add(0,0,600);add(0,3,600);add(0,9,900);
        add(1,1,1200);add(1,9,900);
        add(2,0,600);add(2,5,600);add(2,2,600);
        add(3,0,600);add(3,7,600);add(3,0,600);
    }
    alignCornerHood(k);
    return k;
}
namespace {
constexpr double pi=3.14159265358979323846;
double radians(double v) { return v*pi/180.0; }
std::array<double,2> position(double x,double y,double angle,double dx,double dy) {
    const double c=std::cos(radians(angle)), s=std::sin(radians(angle));
    return {x+c*dx-s*dy,y+s*dx+c*dy};
}
bool contains(CAlfaDoc& doc,unsigned long root,unsigned long member,std::set<unsigned long>& seen) {
    if(root==member)return true;
    if(!seen.insert(root).second)return false;
    auto* group=dynamic_cast<CGroup*>(doc.FindObjectById(root));
    if(group)for(auto id:group->GetElementIds())if(contains(doc,id,member,seen))return true;
    return false;
}
}
bool KitchenLayout::Legs(const KitchenModule& m) const { return m.addLegs<0?(makeLegs && Row(m.row) && !Row(m.row)->upper):m.addLegs!=0; }
const KitchenRow* KitchenLayout::Row(int uid) const {
    for(const auto& r:rows)if(r.uid==uid)return &r;
    return nullptr;
}
KitchenRowFrame KitchenLayout::Frame(int uid) const {
    std::set<int> visiting;
    std::function<KitchenRowFrame(int)> resolve=[&](int id) {
        const auto* r=Row(id);
        if(!r||!visiting.insert(id).second)throw std::runtime_error("Invalid row connection");
        KitchenRowFrame f;f.x=r->x;f.y=r->y;f.angle=r->angle;
        f.depth=r->upper?upperDepth:baseDepth;
        for(const auto& m:modules)if(m.row==id){f.length+=m.width;f.depth=m.depth?m.depth:(r->upper?upperDepth:baseDepth);}
        if(r->parent>=0){auto parent=resolve(r->parent);double thickness=panelThickness;for(const auto& m:modules)if(m.row==r->parent)thickness=Thickness(m);auto xy=position(parent.x,parent.y,parent.angle,parent.length,-parent.depth-thickness);
            f.x+=xy[0];f.y+=xy[1];f.angle+=parent.angle;}
        else if(id==1)f.x+=upperOffset;
        visiting.erase(id);return f;
    };
    return resolve(uid);
}
namespace {
// A shallow upper corner starts earlier along the return wall than the base
// corner. Absorb that distance in the cupboard preceding the hood, not in a gap.
void alignCornerHood(KitchenLayout& k) {
    try {
        for(const auto& row:k.rows)if(row.upper&&row.parent>=0) {
            double offset=0;KitchenModule* preceding=nullptr;
            const auto upper=k.Frame(row.uid);
            for(auto& hood:k.modules)if(hood.row==row.uid) {
                if(hood.type==7&&preceding&&preceding->type<=1) {
                    const auto center=position(upper.x,upper.y,upper.angle,offset+hood.width/2,0);
                    double best=1e30,correction=0;
                    for(const auto& lowerRow:k.rows)if(!lowerRow.upper) {
                        const auto lower=k.Frame(lowerRow.uid);
                        if(std::abs(std::remainder(lower.angle-upper.angle,360.0))>1e-6)continue;
                        double start=0;
                        for(const auto& stove:k.modules)if(stove.row==lowerRow.uid){
                            if(stove.type==5){const auto target=position(lower.x,lower.y,lower.angle,start+stove.width/2,0);
                                const double dx=target[0]-center[0],dy=target[1]-center[1];
                                const double c=std::cos(radians(upper.angle)),sn=std::sin(radians(upper.angle));
                                const double along=dx*c+dy*sn,across=-dx*sn+dy*c;
                                if(std::abs(across)<1e-5&&std::abs(along)<best){best=std::abs(along);correction=along;}}
                            start+=stove.width;
                        }
                    }
                    if(best<1e30&&preceding->width+correction>=200&&preceding->width+correction<=1800){preceding->width+=correction;offset+=correction;}
                }
                offset+=hood.width;preceding=&hood;
            }
        }
    }catch(...){/* Invalid saved row connections are handled by Validate. */}
}
}
std::vector<KitchenModulePlacement> KitchenLayout::Placements() const {
    std::map<int,double> offsets;std::vector<KitchenModulePlacement> result;
    for(const auto& m:modules) {
        const auto* r=Row(m.row);if(!r)throw std::runtime_error("Missing row");
        auto f=Frame(m.row);auto xy=position(f.x,f.y,f.angle,offsets[m.row],0);offsets[m.row]+=m.width;
        KitchenModulePlacement p;p.uid=m.uid;p.row=m.row;p.x=xy[0];p.y=xy[1];p.angle=f.angle;p.width=m.width;
        p.depth=m.depth?m.depth:(r->upper?upperDepth:baseDepth);
        p.height=m.height?m.height:(r->upper?upperHeight:(m.type==6?1850:baseHeight));
        p.bottom=r->upper?legs+baseHeight+top+gap+upperHeight-p.height:(m.type==6?0:legs);
        p.corners={position(p.x,p.y,p.angle,0,0),position(p.x,p.y,p.angle,p.width,0),
            position(p.x,p.y,p.angle,p.width,-p.depth),position(p.x,p.y,p.angle,0,-p.depth)};
        result.push_back(p);
    }
    return result;
}
std::vector<ParametricParameterValue> KitchenLayout::Encode() const {
    std::vector<ParametricParameterValue> p{{"version",6},{"showPlan",double(showPlan)},{"plinthRecess",plinthRecess},{"baseHeight",baseHeight},{"baseDepth",baseDepth},
        {"upperHeight",upperHeight},{"upperDepth",upperDepth},{"legs",legs},{"gap",gap},{"top",top},
        {"upperOffset",upperOffset},{"count",double(modules.size())},{"row_count",double(rows.size())},{"facadeStyle",double(facadeStyle)},{"handleType",double(handleType)},{"makeLegs",double(makeLegs)},{"panelThickness",panelThickness}};
    for(int c=0;c<4;++c)p.push_back({"material_"+std::to_string(c),materials[c]});
    for(size_t i=0;i<rows.size();++i){const auto& r=rows[i];auto s="row_"+std::to_string(i)+"_";
        p.insert(p.end(),{{s+"uid",double(r.uid)},{s+"upper",double(r.upper)},{s+"x",r.x},{s+"y",r.y},{s+"angle",r.angle},{s+"parent",double(r.parent)}});}
    for(size_t i=0;i<modules.size();++i) {
        const auto& m=modules[i];auto s="module_"+std::to_string(i)+"_";
        p.insert(p.end(),{{s+"uid",double(m.uid)},{s+"row",double(m.row)},{s+"type",double(m.type)},
            {s+"width",m.width},{s+"height",m.height},{s+"depth",m.depth},{s+"facadeStyle",double(m.facadeStyle)},
            {s+"shelves",double(m.shelves)},{s+"openDrawer",double(m.openDrawer)},{s+"doorAngle",m.doorAngle},{s+"doorAxis",double(m.doorAxis)},{s+"pullout",m.pullout},{s+"handleType",double(m.handleType)},{s+"addLegs",double(m.addLegs)},{s+"showcaseFill",double(m.showcaseFill)},{s+"drawerCount",double(m.drawerCount)},{s+"panelThickness",m.panelThickness}});
        for(int c=0;c<4;++c)p.push_back({s+"material_"+std::to_string(c),m.materials[c]});
        for(int j=0;j<6;++j)p.push_back({s+"drawer_"+std::to_string(j),m.drawerRatios[j]});
    }
    return p;
}
KitchenLayout KitchenLayout::Decode(const std::vector<ParametricParameterValue>& p) {
    KitchenLayout k;std::map<std::string,double> v;for(const auto& x:p)v[x.id]=x.value;
    auto get=[&](std::string s,double fallback){auto it=v.find(s);return it==v.end()?fallback:it->second;};
    auto integer=[&](std::string s,int fallback){double n=get(s,fallback);return std::isfinite(n)&&std::floor(n)==n&&std::abs(n)<1000000?int(n):-999;};
    k.baseHeight=get("baseHeight",720);k.baseDepth=get("baseDepth",560);
    k.upperHeight=get("upperHeight",720);k.upperDepth=get("upperDepth",320);
    k.showPlan=get("showPlan",0)!=0;k.plinthRecess=get("plinthRecess",50);
    k.legs=get("legs",100);k.gap=get("gap",550);k.top=get("top",38);k.upperOffset=get("upperOffset",0);
    k.facadeStyle=integer("facadeStyle",1);k.handleType=integer("handleType",0);k.makeLegs=integer("makeLegs",1)!=0;k.panelThickness=get("panelThickness",18);
    for(int c=0;c<4;++c)k.materials[c]=get("material_"+std::to_string(c),0);
    int count=integer("count",0);if(count<0||count>100){k.rows.clear();return k;}
    int rowCount=integer("row_count",-1);
    if(rowCount>=0&&rowCount<=20){k.rows.clear();for(int i=0;i<rowCount;++i){auto s="row_"+std::to_string(i)+"_";
        k.rows.push_back({integer(s+"uid",i),integer(s+"upper",0)!=0,get(s+"x",0),get(s+"y",0),get(s+"angle",0),integer(s+"parent",-1)});}}
    else if(rowCount!=-1)k.rows.clear();
    for(int i=0;i<count;++i){auto s="module_"+std::to_string(i)+"_";
        KitchenModule m;m.uid=integer(s+"uid",i+1);m.row=integer(s+"row",0);m.type=integer(s+"type",0);
        m.width=get(s+"width",600);m.height=get(s+"height",0);m.depth=get(s+"depth",0);
        m.facadeStyle=integer(s+"facadeStyle",0);m.shelves=integer(s+"shelves",2);m.openDrawer=integer(s+"openDrawer",0);
        m.handleType=integer(s+"handleType",-1);m.addLegs=integer(s+"addLegs",-1);m.showcaseFill=integer(s+"showcaseFill",4);m.drawerCount=integer(s+"drawerCount",0);m.panelThickness=get(s+"panelThickness",0);
        for(int c=0;c<4;++c)m.materials[c]=get(s+"material_"+std::to_string(c),-1);
        m.doorAxis=integer(s+"doorAxis",0);m.doorAngle=get(s+"doorAngle",0);m.pullout=get(s+"pullout",300);
        for(int j=0;j<6;++j)m.drawerRatios[j]=get(s+"drawer_"+std::to_string(j),1);
        k.modules.push_back(m);
    }
    if(get("version",1)==2 && !k.modules.empty()) {
        k.facadeStyle=std::max(0,k.modules.front().facadeStyle);
        for(auto& m:k.modules)if(m.facadeStyle==k.facadeStyle)m.facadeStyle=-1;
    }
    if(get("version",1)<5)alignCornerHood(k);
    return k;
}
bool KitchenLayout::Validate(std::string& error) const {
    auto range=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if(!range(plinthRecess,0,150)||facadeStyle<0||facadeStyle>4||handleType<0||handleType>3||!range(panelThickness,5,40)||!range(baseHeight,400,1200)||!range(baseDepth,300,900)||!range(upperHeight,200,1200)
       ||!range(upperDepth,200,600)||!range(legs,50,250)||!range(gap,200,1200)||!range(top,10,100)
       ||!range(upperOffset,-10000,10000)||modules.empty()||modules.size()>100||rows.empty()||rows.size()>20) {
        error="Invalid kitchen dimensions or empty layout.";return false;
    }
    auto validMaterial=[&](double id){return range(id,-1,4294967295.0)&&std::floor(id)==id;};
    for(double id:materials)if(!validMaterial(id)||id<0){error="Invalid kitchen material.";return false;}
    std::set<int> rowIds,ids;
    for(const auto& r:rows)if(r.uid<0||!rowIds.insert(r.uid).second||!range(r.x,-20000,20000)||!range(r.y,-20000,20000)
        ||!range(r.angle,-360,360)||(r.parent>=0&&(!Row(r.parent)||Row(r.parent)->upper!=r.upper))) {
        error="Invalid row position or connection. Connect rows on the same level.";return false;
    }
    for(const auto& m:modules) {
        const auto* r=Row(m.row);
        if(m.uid<=0||!ids.insert(m.uid).second||!r||m.type<0||m.type>9||!range(m.width,200,1800)
            ||!(m.height==0||range(m.height,200,2400))||!(m.depth==0||range(m.depth,200,900))
            ||m.facadeStyle<-1||m.facadeStyle>4||m.handleType<-1||m.handleType>3||m.addLegs<-1||m.addLegs>1||m.showcaseFill<0||m.showcaseFill>4
            ||m.doorAxis<0||m.doorAxis>1||m.drawerCount<0||m.drawerCount>6||!(m.panelThickness==0||range(m.panelThickness,5,40))||m.shelves<0||m.shelves>8||!range(m.doorAngle,0,120)
            ||m.openDrawer<0||m.openDrawer>(m.type>=2&&m.type<=4?Drawers(m):0)||!range(m.pullout,0,800)
            ||(r->upper&&m.type>=2&&m.type<=6)||(!r->upper&&m.type==7)) {
            error="Check module dimensions, facade and opening parameters; drawers/appliances belong below, hoods above.";return false;
        }
        for(double id:m.materials)if(!validMaterial(id)){error="Invalid module material.";return false;}
        if(m.type>=2&&m.type<=4)for(int i=0;i<Drawers(m);++i)if(!range(m.drawerRatios[i],0.1,10000)){error="Drawer front heights must be positive.";return false;}
        if(m.type==9&&m.width<(m.depth?m.depth:(r->upper?upperDepth:baseDepth))+220){error="Blind corner needs room for its accessible door: width must exceed depth by at least 220 mm.";return false;}
    }
    try {
        for(const auto& row:rows)(void)Frame(row.uid);
        const auto places=Placements();
        for(size_t i=0;i<places.size();++i)for(size_t j=i+1;j<places.size();++j) {
            if(modules[i].type==8||modules[j].type==8)continue;
            const auto& a=places[i];const auto& b=places[j];
            if(std::min(a.bottom+a.height,b.bottom+b.height)-std::max(a.bottom,b.bottom)<0.01)continue;
            bool separated=false;
            for(double angle:{a.angle,a.angle+90,b.angle,b.angle+90}) {
                double ax=std::cos(radians(angle)),ay=std::sin(radians(angle));
                double amin=1e30,amax=-1e30,bmin=1e30,bmax=-1e30;
                for(auto c:a.corners){double t=c[0]*ax+c[1]*ay;amin=std::min(amin,t);amax=std::max(amax,t);}
                for(auto c:b.corners){double t=c[0]*ax+c[1]*ay;bmin=std::min(bmin,t);bmax=std::max(bmax,t);}
                if(std::min(amax,bmax)-std::max(amin,bmin)<0.01){separated=true;break;}
            }
            if(!separated){error="Modules "+std::to_string(a.uid)+" and "+std::to_string(b.uid)+" overlap. Adjust their rows or dimensions.";return false;}
        }
    }catch(...){error="Row connections contain a cycle or a missing row.";return false;}
    return true;
}
CAssembled* FindKitchenLayout(CAlfaDoc& doc,unsigned long member) {
    for(auto& o:doc.GetObjects())if(o&&o->GetParametricToolId()=="kitchen_layout") {
        std::set<unsigned long> seen;if(contains(doc,o->m_id,member,seen))return dynamic_cast<CAssembled*>(o.get());
    }
    return nullptr;
}
int FindKitchenModule(CAlfaDoc& doc,unsigned long member) {
    if(auto* item=doc.FindObjectById(member))if(item->GetParametricToolId()=="kitchen_plan_item")for(const auto& p:item->GetParametricParameters())if(p.id=="uid")return int(p.value);

    for(auto& o:doc.GetObjects())if(o&&(o->GetParametricToolId()=="kitchen_layout_module"||o->GetGroupName().rfind("Kitchen group module ",0)==0)) {
        std::set<unsigned long> seen;if(contains(doc,o->m_id,member,seen))
            for(const auto& p:o->GetParametricParameters())if(p.id=="uid")return int(p.value);
    }
    return 0;
}
namespace {
KitchenCabinetDefinition cabinetDefinition(const KitchenLayout& k,const KitchenModule& m,const KitchenModulePlacement& p) {
    KitchenCabinetDefinition d;d.width=p.width;d.height=p.height;d.depth=p.depth;
    d.facade_type=m.type==1?KitchenCabinetFacadeType::DoubleDoor:KitchenCabinetFacadeType::SingleDoor;
    d.facade_style=static_cast<KitchenCabinetFacadeStyle>(k.Facade(m));d.panel_thickness=k.Thickness(m);
    d.door_axis=m.doorAxis;d.shelf_count=m.shelves;d.door_open_angle=d.left_door_open_angle=d.right_door_open_angle=m.doorAngle;
    d.handle_type=k.HandleType(m);d.make_legs=k.Legs(m);d.leg_height=k.legs;d.leg_inset=std::max(20.0,k.plinthRecess-k.Thickness(m)+20);
    d.showcase_fill=static_cast<KitchenCabinetShowcaseFill>(m.showcaseFill);return d;
}
const char* materialKeys[]={"body_material_id","facade_material_id","hardware_material_id","top_material_id"};
const char* defaultMaterials[]={"Inside","Facade wood","Gold","Marble"};
double materialChoice(const KitchenLayout& k,const KitchenModule& m,int role) {return m.materials[role]<0?k.materials[role]:m.materials[role];}
unsigned long materialId(CAlfaDoc& doc,double choice,int role) {
    if(choice>0)return static_cast<unsigned long>(choice);
    const auto* material=doc.FindMaterial(defaultMaterials[role]);return material?material->id:0;
}
CAssembled::TransformMatrix moduleTransform(const KitchenLayout& k,const KitchenModule& m,const KitchenModulePlacement& p,const CAssembled* root) {
    double c=std::cos(radians(p.angle)),s=std::sin(radians(p.angle));auto xy=position(p.x,p.y,p.angle,p.width/2,-p.depth/2);
    CAssembled::TransformMatrix local{c,-s,0,xy[0],s,c,0,xy[1],0,0,1,p.bottom-(k.Legs(m)?k.legs:0),0,0,0,1};
    if(!root)return local;auto result=local;result.fill(0);
    const auto& world=root->GetAssemblyTransform();
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int t=0;t<4;++t)result[i*4+j]+=world[i*4+t]*local[t*4+j];return result;
}
std::vector<ParametricParameterValue> nativeParameters(CAlfaDoc& doc,const KitchenLayout& k,const KitchenModule& m,const KitchenModulePlacement& p) {
    std::vector<ParametricParameterValue> v{{"uid",double(m.uid)},{"row",double(m.row)},{"width",p.width},{"height",p.height},{"depth",p.depth},
        {"panel_thickness",k.Thickness(m)},{"handle_type",double(k.HandleType(m))},{"make_legs",double(k.Legs(m))},{"leg_height",k.legs},{"leg_inset",std::max(20.0,k.plinthRecess-k.Thickness(m)+20)},
        {"overhead",0},{"mounting_height",0},{"body_type",0},{"facade_style",double(k.Facade(m))},{"showcase_fill",double(m.showcaseFill)},
        {"shelf_count",double(m.shelves)},{"door_open_angle",m.doorAngle},{"left_door_open_angle",m.doorAngle},{"right_door_open_angle",m.doorAngle},
        {"door_hinge_side",0},{"handle_orientation",0},{"door_axis",double(m.doorAxis)}};
    if(m.type>=2&&m.type<=4) {
        v.push_back({"facade_type",double(k.Facade(m))});v.push_back({"drawer_count",double(k.Drawers(m))});
        v.push_back({"open_drawer",double(m.openDrawer)});v.push_back({"pullout_distance",m.pullout});
        v.insert(v.end(),{{"drawer_side_thickness",12},{"drawer_bottom_thickness",6},{"slide_clearance",13}});
        double sum=0;for(int i=0;i<k.Drawers(m);++i)sum+=m.drawerRatios[i];
        for(int i=0;i<6;++i)v.push_back({"drawer_height_"+std::to_string(i+1),i<k.Drawers(m)?p.height*m.drawerRatios[i]/sum:200});
    } else v.push_back({"facade_type",m.type==1?2.0:1.0});
    for(int i=0;i<4;++i)v.push_back({materialKeys[i],double(materialId(doc,materialChoice(k,m,i),i))});
    return v;
}
}
namespace {
std::vector<std::unique_ptr<CAlfaObject>> kitchenPlan(const KitchenLayout& k) {
    std::vector<std::unique_ptr<CAlfaObject>> result;if(!k.showPlan)return result;
    const auto placements=k.Placements();double xmin=0,xmax=0,ymax=0;
    for(const auto& p:placements)for(auto c:p.corners){xmin=std::min(xmin,c[0]);xmax=std::max(xmax,c[0]);ymax=std::max(ymax,c[1]);}
    const double spacing=xmax-xmin+1200;
    auto add=[&](std::unique_ptr<CAlfaObject> o,int uid,bool upper,const std::string& role,Color color){
        o->SetGroupName("Kitchen plan "+std::to_string(uid)+" / "+role);
        o->SetParametricDefinition("kitchen_plan_item",{{"uid",double(uid)},{"upper",double(upper)}});
        o->SetColor(color);o->SetLineWidth(0.25);result.push_back(std::move(o));
    };
    const Color dimensionColor{0.75f,0.8f,0.85f};
    auto line=[&](const KitchenModulePlacement& p,bool upper,std::vector<std::array<double,2>> points,const std::string& role,Color color){
        auto object=std::make_unique<CPolyline>(role);const double offset=-(upper?2:1)*spacing;
        for(auto q:points){auto xy=position(p.x+offset,p.y,p.angle,q[0],q[1]);object->AddPoint(CPoint3d(xy[0],xy[1],0));}
        add(std::move(object),p.uid,upper,role,color);
    };
    auto label=[&](const KitchenModulePlacement& p,bool upper,double x,double y,const std::string& text,const std::string& role){
        auto xy=position(p.x-(upper?2:1)*spacing,p.y,p.angle,x,y);
        auto o=std::make_unique<CDrawingText>(text,CPoint3d(xy[0],xy[1],0),32,p.angle,"Arial");
        add(std::move(o),p.uid,upper,role,dimensionColor);
    };
    auto dimension=[&](const KitchenModulePlacement& p,bool upper,double a,double b,double y,const std::string& role){
        line(p,upper,{{a,y},{b,y}},role,dimensionColor);
        for(double x:{a,b})line(p,upper,{{x-14,y-14},{x+14,y+14}},role+std::to_string(x),dimensionColor);
        auto text=std::to_string(int(std::lround(b-a)));label(p,upper,(a+b)/2-10*text.size(),y+20,text,role+" value");
    };
    for(size_t i=0;i<placements.size();++i){const auto& p=placements[i];const auto& m=k.modules[i];bool upper=k.Row(m.row)->upper;
        const Color color=upper?Color{0.35f,0.7f,1.0f}:Color{1.0f,0.2f,0.65f};
        line(p,upper,{{0,0},{p.width,0},{p.width,-p.depth},{0,-p.depth},{0,0}},"module outline",color);
        std::string type=m.type>=2&&m.type<=4?std::to_string(k.Drawers(m))+" drawers":m.type==5?"Stove":m.type==6?"Fridge":m.type==7?"Hood":m.type==8?"Space":m.type==9?"Corner":m.type==1?"2 doors":"1 door";
        label(p,upper,25,-p.depth/2,"#"+std::to_string(m.uid)+"  "+type,"module label");
        dimension(p,upper,0,p.width,90,"module width");
    }
    for(const auto& row:k.rows){auto f=k.Frame(row.uid);if(f.length<=0)continue;KitchenModulePlacement p;p.uid=0;p.x=f.x;p.y=f.y;p.angle=f.angle;
        auto role="row "+std::to_string(row.uid);dimension(p,row.upper,0,f.length,260,role+" length");
        line(p,row.upper,{{f.length+100,0},{f.length+100,-f.depth}},role+" depth",dimensionColor);
        label(p,row.upper,f.length+125,-f.depth/2,std::to_string(int(std::lround(f.depth))),role+" depth value");
    }
    for(bool upper:{false,true}){KitchenModulePlacement p;p.x=xmin;p.y=ymax+430;
        label(p,upper,0,0,upper?"UPPER CABINETS - mm":"LOWER CABINETS - mm",upper?"upper title":"lower title");}
    return result;
}
}
CAssembled* FindKitchenModuleObject(CAlfaDoc& doc,unsigned long member) {
    for(auto& o:doc.GetObjects())if(o&&o->GetGroupName().rfind("Kitchen group module ",0)==0&&o->GetParametricToolId()!="kitchen_layout_facade") {
        std::set<unsigned long> seen;if(contains(doc,o->m_id,member,seen))return dynamic_cast<CAssembled*>(o.get());
    }
    return nullptr;
}
bool BuildKitchenLayout(CAlfaDoc& doc,const KitchenLayout& k,CAssembled* existing,std::string& error) {
    if(!k.Validate(error))return false;
    struct DocumentScope { CAlfaDoc* previous; DocumentScope(CAlfaDoc& d):previous(GetAlfaDoc()){SetAlfaDoc(&d);} ~DocumentScope(){SetAlfaDoc(previous);} } scope(doc);
    std::vector<std::unique_ptr<CAlfaObject>> parts,planParts;
    try {
        planParts=kitchenPlan(k);
        const auto placements=k.Placements();
        auto place=[](std::unique_ptr<CAlfaObject>& o,double x,double y,double z,double angle) {
            auto* solid=dynamic_cast<CSolid*>(o.get());if(!solid)throw std::runtime_error("Expected furniture solid");
            gp_Trsf t;t.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),radians(angle));t.SetTranslationPart(gp_Vec(x,y,z));
            TopoDS_Shape shape=BRepBuilderAPI_Transform(solid->m_Shape,t,true).Shape();
            auto replacement=std::make_unique<CSolid>(shape);replacement->SetName(o->GetName());replacement->SetGroupName(o->GetGroupName());replacement->SetColor(o->GetColor());
            if(!replacement->InitSurfaces())throw std::runtime_error("Could not place furniture");o=std::move(replacement);
        };
        auto box=[&](std::string role,double px,double py,double pz,double w,double d,double h,Color c) {
            TopoDS_Shape shape=BRepPrimAPI_MakeBox(gp_Pnt(px,py,pz),w,d,h).Shape();
            auto o=std::make_unique<CSolid>(shape); if(!o->InitSurfaces())throw std::runtime_error("Invalid kitchen solid");o->SetName(role);o->SetGroupName(role);o->SetColor(c);parts.push_back(std::move(o));
        };
        const Color light{0.88f,0.87f,0.82f}, metal{0.45f,0.47f,0.49f}, dark{0.09f,0.11f,0.12f};
        for(size_t index=0;index<k.modules.size();++index) {
            const auto& m=k.modules[index];const auto& p=placements[index];const auto* row=k.Row(m.row);
            if(m.type==8)continue;
            const size_t begin=parts.size();const double px=0,d=p.depth,h=p.height,z=p.bottom;
            const auto key="Kitchen module "+std::to_string(m.uid)+" / ";
            const double sourceZ=z-(k.Legs(m)?k.legs:0);
            if(m.type<=4||m.type==9) {
                std::vector<std::unique_ptr<CAlfaObject>> module;
                if(m.type<=1||m.type==9) {KitchenCabinetDefinition def;def.width=m.width;def.height=h;def.depth=d;
                    def.facade_type=m.type==9?KitchenCabinetFacadeType::Open:(m.type?KitchenCabinetFacadeType::DoubleDoor:KitchenCabinetFacadeType::SingleDoor);
                    def.facade_style=static_cast<KitchenCabinetFacadeStyle>(k.Facade(m));def.shelf_count=m.shelves;def.panel_thickness=k.Thickness(m);def.handle_type=k.HandleType(m);def.make_legs=k.Legs(m);def.leg_height=k.legs;def.leg_inset=std::max(20.0,k.plinthRecess-k.Thickness(m)+20);def.showcase_fill=static_cast<KitchenCabinetShowcaseFill>(m.showcaseFill);
                    def.door_axis=m.doorAxis;def.door_open_angle=m.doorAngle;def.left_door_open_angle=m.doorAngle;def.right_door_open_angle=m.doorAngle;
                    module=CKitchenCabinet::BuildParts(def);
                    if(m.type==9) {
                        const double doorWidth=m.width-d-30;def.width=doorWidth;def.facade_type=KitchenCabinetFacadeType::SingleDoor;
                        auto front=CKitchenCabinet::BuildParts(def);
                        for(auto& part:front)if(part&&(part->GetName().find("Facade")!=std::string::npos||part->GetName().find("Handle")!=std::string::npos)) {
                            place(part,(doorWidth-m.width)/2,0,0,0);module.push_back(std::move(part));
                        }
                        box(key+"blind corner front",doorWidth+3,-d-k.Thickness(m),z,m.width-doorWidth-3,k.Thickness(m),h,light);
                    }
                } else {DrawerBoxDefinition def;def.width=m.width;def.height=h;def.depth=d;def.make_legs=k.Legs(m);def.leg_height=k.legs;def.leg_inset=std::max(20.0,k.plinthRecess-k.Thickness(m)+20);def.panel_thickness=k.Thickness(m);def.handle_type=k.HandleType(m);
                    def.drawer_count=k.Drawers(m);def.drawer_heights.assign(m.drawerRatios.begin(),m.drawerRatios.begin()+def.drawer_count);
                    def.facade_type=k.Facade(m);def.open_drawer=m.openDrawer;def.pullout_distance=m.pullout;module=CDrawerBoxFurniture::BuildParts(def);}
                if(module.empty()){error="Could not build cabinet.";return false;}
                std::map<std::string,int> occurrences;
                for(auto& o:module){if(!o)throw std::runtime_error("Invalid cabinet part");auto name=o->GetName();o->SetGroupName(key+name+" "+std::to_string(++occurrences[name]));
                    o->SetColor(name.find("Handle")!=std::string::npos||name.find("Rail")!=std::string::npos?metal:light);place(o,m.width/2,-d/2,sourceZ,0);parts.push_back(std::move(o));}
            } else if(m.type==5||m.type==6) {
                double bodyH=m.type==5?h+k.top:h;
                box(key+"appliance",px,-d,z,m.width,d,bodyH,light);
                if(m.type==5) {
                    box(key+"oven glass",px+35,-d-5,z+80,m.width-70,5,bodyH-180,dark);
                    box(key+"handle",px+50,-d-25,z+bodyH-80,m.width-100,20,12,metal);
                    for(int b=0;b<4;++b)box(key+"burner "+std::to_string(b),px+60+(b%2)*(m.width-180),-d+60+(b/2)*(d-180),z+bodyH,60,60,5,dark);
                } else {
                    box(key+"door seam",px,-d-2,z+h*0.35,m.width,2,4,dark);
                    box(key+"handle",px+30,-d-25,z+h*0.55,12,20,220,metal);
                }
            } else if(m.type==7) {
                double hoodZ=m.height>0?z-60:k.legs+k.baseHeight+k.top+k.gap;
                box(key+"hood",px,-d,hoodZ,m.width,d,60,metal);
                if(m.height>0) {
                    auto def=cabinetDefinition(k,m,p);def.make_legs=false;
                    def.facade_style=static_cast<KitchenCabinetFacadeStyle>(k.Facade(m));auto upper=CKitchenCabinet::BuildParts(def);
                    for(auto& o:upper){if(!o)throw std::runtime_error("Invalid hood cabinet");o->SetGroupName(key+o->GetName());o->SetColor(light);place(o,m.width/2,-d/2,z,0);parts.push_back(std::move(o));}
                }
                else box(key+"chimney",px+m.width*0.35,-d*0.65,hoodZ+60,m.width*0.3,d*0.5,k.upperHeight-60,metal);
            }
            if(!row->upper&&(m.type<=5||m.type==9)) {
                box(key+"plinth",px,-d-k.Thickness(m)+k.plinthRecess,0,m.width,18,k.legs,light);
            }
            for(size_t i=begin;i<parts.size();++i)place(parts[i],p.x,p.y,0,p.angle);
        }
        struct TopSection { size_t index; int row; int parent; bool joined; };
        std::vector<TopSection> tops;
        for(const auto& row:k.rows) {
            if(row.upper)continue;
            const auto frame=k.Frame(row.uid);const size_t begin=parts.size();
            const KitchenModule* parentEnd=nullptr;
            if(row.parent>=0)for(const auto& parentModule:k.modules)if(parentModule.row==row.parent)parentEnd=&parentModule;
            double start=0,width=0,height=0,depth=0,cursor=0;int uid=0;bool joined=false;
            auto flush=[&]() {
                if(width<=0)return;
                const double front=-depth-25, bottom=k.legs+height;
                const auto blank=BRepPrimAPI_MakeBox(gp_Pnt(start,front,bottom),width,depth+25,k.top).Shape();
                BRepFilletAPI_MakeFillet fillet(blank);
                // Round the upper front edge only: R20 fits the standard 38 mm slab.
                // A thinner custom slab must retain some straight front thickness.
                const double radius=std::min({20.0,k.top*0.95,(depth+25)*0.95});
                for(TopExp_Explorer edges(blank,TopAbs_EDGE);edges.More();edges.Next()) {
                    const auto edge=TopoDS::Edge(edges.Current());BRepAdaptor_Curve curve(edge);
                    const auto a=curve.Value(curve.FirstParameter()), b=curve.Value(curve.LastParameter());
                    if(std::abs(a.Y()-front)<1e-6&&std::abs(b.Y()-front)<1e-6
                        &&std::abs(a.Z()-bottom-k.top)<1e-6&&std::abs(b.Z()-bottom-k.top)<1e-6)
                        fillet.Add(radius,edge);
                }
                fillet.Build();
                if(!fillet.IsDone())throw std::runtime_error("Could not round kitchen worktop");
                const auto role="Kitchen module "+std::to_string(uid)+" / worktop";
                auto shape=fillet.Shape();auto top=std::make_unique<CSolid>(shape);
                if(!top->InitSurfaces())throw std::runtime_error("Invalid rounded worktop");
                tops.push_back({parts.size(),row.uid,row.parent,joined});
                top->SetName(role);top->SetGroupName(role);top->SetColor(light);parts.push_back(std::move(top));
                width=0;
            };
            for(const auto& m:k.modules) {
                if(m.row!=row.uid)continue;
                double h=m.height?m.height:k.baseHeight,d=m.depth?m.depth:k.baseDepth;
                if(m.type<=4||m.type==9) {
                    if(width>0&&(h!=height||d!=depth))flush();
                    if(width==0){start=cursor;height=h;depth=d;uid=m.uid;joined=false;}
                    width+=m.width;
                    // The child starts at the parent's 18 mm front; the worktop projects 25 mm.
                    if(cursor==0&&parentEnd&&(parentEnd->type<=4||parentEnd->type==9)
                        &&std::abs(row.angle+90)<0.01
                        &&std::abs((parentEnd->height?parentEnd->height:k.baseHeight)-h)<0.01){// Extend underneath the parent's rounded edge, then trim to its actual shape.
                            const double inset=25-k.Thickness(*parentEnd)-std::min(20.0,k.top*0.95);
                            start+=inset;width-=inset;joined=true;}
                }else flush();
                cursor+=m.width;
            }
            flush();
            for(size_t i=begin;i<parts.size();++i)place(parts[i],frame.x,frame.y,0,frame.angle);
        }
        // Both shapes are now in kitchen coordinates. Cutting the extended child
        // gives a mating end even where the parent has a cylindrical front edge.
        for(const auto& section:tops)if(section.joined) {
            auto* child=dynamic_cast<CSolid*>(parts[section.index].get());
            auto shape=child->m_Shape;
            for(const auto& parent:tops)if(parent.row==section.parent) {
                auto* solid=dynamic_cast<CSolid*>(parts[parent.index].get());
                BRepAlgoAPI_Cut cut(shape,solid->m_Shape);cut.Build();
                if(!cut.IsDone())throw std::runtime_error("Could not fit corner worktop");
                shape=cut.Shape();
            }
            auto fitted=std::make_unique<CSolid>(shape);
            if(!fitted->InitSurfaces())throw std::runtime_error("Invalid corner worktop");
            fitted->SetName(child->GetName());fitted->SetGroupName(child->GetGroupName());fitted->SetColor(child->GetColor());
            parts[section.index]=std::move(fitted);
        }
    }catch(...){error="Kitchen geometry could not be built.";return false;}
    FurnitureMaterialFactory::EnsureStandardMaterials(doc);
    const auto oldLayout=existing?KitchenLayout::Decode(existing->GetParametricParameters()):k;
    bool oldMaterials=false;
    if(existing)for(const auto& v:existing->GetParametricParameters())if(v.id=="version"&&v.value>=3)oldMaterials=true;
    for(const auto& m:k.modules)for(int r=0;r<4;++r)if(materialChoice(k,m,r)>0&&!doc.FindMaterial(static_cast<unsigned long>(materialChoice(k,m,r)))) {error="Selected material is missing from this document.";return false;}
    // Index old generated parts recursively, including the flat version-1 layout.
    std::map<std::string,size_t> old;
    std::map<std::string,std::vector<unsigned long>> extras;
    std::set<unsigned long> visited;
    std::function<void(const CGroup&,const std::string&)> collect=[&](const CGroup& group,const std::string& owner) {
        for(auto id:group.GetElementIds()) {
            if(!visited.insert(id).second)continue;
            const auto index=doc.FindObjectIndexById(id);
            if(index>=doc.GetObjects().size()||!doc.GetObjects()[index])continue;
            const auto& o=doc.GetObjects()[index];const auto& key=o->GetGroupName();
            if(key.rfind("Kitchen module ",0)==0||key.rfind("Kitchen group ",0)==0||key.rfind("Kitchen plan ",0)==0) {
                old[key]=index;
                if(auto* child=dynamic_cast<CGroup*>(o.get()))collect(*child,key);
            }else extras[owner].push_back(id);
        }
    };
    if(existing)collect(*existing,"root");
    std::vector<unsigned long> generatedLeaves;
    std::map<int,std::vector<unsigned long>> moduleParts,facadeParts,rowParts;
    auto install=[&](std::unique_ptr<CAlfaObject> o,bool leaf) {
        const auto key=o->GetGroupName();auto found=old.find(key);unsigned long id=0;
        if(found!=old.end()) {
            auto& previous=doc.GetObjects()[found->second];id=previous->m_id;o->m_id=id;
            // Group setters propagate to their children; appearance belongs to leaves.
            if(leaf){o->SetColor(previous->GetColor());o->SetMaterial(previous->GetMaterial());o->SetMaterialId(previous->GetMaterialId());
                o->SetVisible(previous->IsVisible());o->SetLineWidth(previous->GetLineWidth());o->SetLineStyle(previous->GetLineStyle());}
            o->CAlfaObject::SetVisible(previous->IsVisible());o->m_LayerID=previous->m_LayerID;previous=std::move(o);old.erase(found);
        }else{doc.AddObject(std::move(o),false);id=doc.GetObjects().back()->m_id;}
        if(leaf)generatedLeaves.push_back(id);return id;
    };
    for(auto& o:parts) {
        int uid=0;const auto key=o->GetGroupName();
        try{uid=std::stoi(key.substr(15));}catch(...){error="Invalid generated module identity.";return false;}
        auto m=std::find_if(k.modules.begin(),k.modules.end(),[&](const KitchenModule& candidate){return candidate.uid==uid;});
        const bool topPart=key.find(" / worktop")!=std::string::npos;
        const bool plinthPart=key.find(" / plinth")!=std::string::npos;
        const bool facade=o->GetName().find("Facade")!=std::string::npos||key.find("blind corner front")!=std::string::npos||key.find(" / plinth")!=std::string::npos;
        const auto name=o->GetName();
        int role=topPart?3:(name.find("Handle")!=std::string::npos||name.find("Guide")!=std::string::npos||name.find("Rail")!=std::string::npos||name.find("Leg")!=std::string::npos?2:(facade?1:0));
        auto previous=old.find(key);bool hadPrevious=previous!=old.end();
        auto oldModule=std::find_if(oldLayout.modules.begin(),oldLayout.modules.end(),[&](const KitchenModule& a){return a.uid==uid;});
        const double choice=materialChoice(k,*m,role);
        const double previousChoice=oldModule!=oldLayout.modules.end()?materialChoice(oldLayout,*oldModule,role):-2;
        auto id=install(std::move(o),true);
        auto* installed=doc.FindObjectById(id);
        if((m->type<=4||m->type==9||topPart||plinthPart||name.find("Cabinet")!=std::string::npos) && (!hadPrevious||!oldMaterials||choice!=previousChoice||(plinthPart&&installed->GetMaterialId()==0))) {
            const Material* selected=doc.FindMaterial(materialId(doc,choice,role));
            if(name.find("Showcase Glass")!=std::string::npos)selected=doc.FindMaterial("Glass");
            else if(name.find("Stained Glass Wire")!=std::string::npos)selected=doc.FindMaterial("Gold");
            if(selected){auto appearance=*selected;if(!appearance.color_texture_path.empty())appearance.texture_fit_to_surface=true;installed->SetMaterial(appearance);installed->SetMaterialId(selected->id);installed->SetColor(appearance.diffuse);}
        }
        if(!installed->GetMaterial().color_texture_path.empty())if(auto* solid=dynamic_cast<CSolid*>(installed))
            for(int face=0;face<solid->GetNumSurfaces();++face){SurfaceTextureTransform mapping;mapping.fit_to_surface=true;solid->SetSurfaceTextureTransform(face,mapping);}
        if(topPart||key.find(" / plinth")!=std::string::npos)rowParts[m->row].push_back(id);
        else if(facade && !(m->type<=4))facadeParts[uid].push_back(id);
        else moduleParts[uid].push_back(id);
    }
    if(existing){CAssembled transformed("Generated kitchen",generatedLeaves);transformed.SetAssemblyTransform(existing->GetAssemblyTransform());transformed.ApplyStoredTransformToElements();}
    auto group=[&](std::string key,std::string name,std::string tool,std::vector<unsigned long> children,std::vector<ParametricParameterValue> parameters) {
        auto& additional=extras[key];children.insert(children.end(),additional.begin(),additional.end());extras.erase(key);
        auto o=std::make_unique<CAssembled>(std::move(name),std::move(children));o->SetGroupName(key);o->SetParametricDefinition(std::move(tool),std::move(parameters));
        if(existing)o->SetAssemblyTransform(existing->GetAssemblyTransform());return install(std::move(o),false);
    };
    const char* names[]={"Single-door cabinet","Double-door cabinet","2-drawer cabinet","3-drawer cabinet","4-drawer cabinet","Stove","Refrigerator","Hood","Space","Blind corner cabinet"};
    const auto locations=k.Placements();
    for(size_t i=0;i<k.modules.size();++i) {
        const auto& m=k.modules[i];const auto& p=locations[i];if(m.type==8)continue;
        const auto moduleKey="Kitchen group module "+std::to_string(m.uid);
        if(m.type<=4) {
            auto children=moduleParts[m.uid];
            for(const auto& extraKey:{moduleKey,moduleKey+" facade"}){auto& extra=extras[extraKey];children.insert(children.end(),extra.begin(),extra.end());extras.erase(extraKey);}
            std::unique_ptr<CAssembled> module;
            std::string tool,name=std::to_string(m.uid)+" - "+(m.type>=2&&m.type<=4?std::to_string(k.Drawers(m))+"-drawer cabinet":std::string(names[m.type]));
            if(m.type<=1){module=std::make_unique<CKitchenCabinet>(name,children,cabinetDefinition(k,m,p));tool="cabinet_advanced";}
            else{module=std::make_unique<CDrawerBoxFurniture>(name,children);tool="drawer_box";}
            module->SetGroupName(moduleKey);module->SetParametricDefinition(tool,nativeParameters(doc,k,m,p));
            module->SetAssemblyTransform(moduleTransform(k,m,p,existing));rowParts[m.row].push_back(install(std::move(module),false));
        } else {
            auto children=moduleParts[m.uid];children.insert(children.end(),facadeParts[m.uid].begin(),facadeParts[m.uid].end());
            rowParts[m.row].push_back(group(moduleKey,std::to_string(m.uid)+" - "+names[m.type],"kitchen_layout_module",children,{{"uid",double(m.uid)},{"row",double(m.row)}}));
        }
    }
    std::vector<unsigned long> rootIds;
    for(const auto& row:k.rows)rootIds.push_back(group("Kitchen group row "+std::to_string(row.uid),
        std::string(row.upper?"Upper row ":"Lower row ")+std::to_string(row.uid+1),"kitchen_layout_row",rowParts[row.uid],
        {{"uid",double(row.uid)},{"upper",double(row.upper)},{"x",row.x},{"y",row.y},{"angle",row.angle},{"parent",double(row.parent)}}));
    if(k.showPlan) {
        std::vector<unsigned long> lower,upper,all;
        for(auto& object:planParts){
            if(existing){const auto& t=existing->GetAssemblyTransform();
                auto transform=[&](const CPoint3d& p){return CPoint3d(t[0]*p.x+t[1]*p.y+t[2]*p.z+t[3],t[4]*p.x+t[5]*p.y+t[6]*p.z+t[7],t[8]*p.x+t[9]*p.y+t[10]*p.z+t[11]);};
                if(auto* line=dynamic_cast<CPolyline*>(object.get()))for(size_t n=0;n<line->GetPointCount();++n)line->SetPoint(n,transform(line->GetPoints()[n]));
                if(auto* text=dynamic_cast<CDrawingText*>(object.get())){double angle=radians(text->GetRotationDegrees()),c=std::cos(angle),s=std::sin(angle);
                    text->SetInsertion(transform(text->GetInsertion()));text->SetRotationDegrees(std::atan2(t[4]*c+t[5]*s,t[0]*c+t[1]*s)*180/pi);
                    double hx=-t[0]*s+t[1]*c,hy=-t[4]*s+t[5]*c,hz=-t[8]*s+t[9]*c;text->SetHeight(text->GetHeight()*std::sqrt(hx*hx+hy*hy+hz*hz));}
            }
            bool topPlan=false;for(const auto& p:object->GetParametricParameters())if(p.id=="upper")topPlan=p.value!=0;
            auto id=install(std::move(object),true);(topPlan?upper:lower).push_back(id);all.push_back(id);}
        rootIds.push_back(group("Kitchen group plan lower","2D plan - lower cabinets","kitchen_layout_plan",lower,{}));
        rootIds.push_back(group("Kitchen group plan upper","2D plan - upper cabinets","kitchen_layout_plan",upper,{}));
    }
    // Attachments of removed modules remain in the kitchen, rather than being discarded.
    for(const auto& entry:extras)rootIds.insert(rootIds.end(),entry.second.begin(),entry.second.end());
    for(const auto& entry:old)doc.GetObjects()[entry.second].reset();
    if(existing){existing->SetElementIds(rootIds);existing->SetParametricDefinition("kitchen_layout",k.Encode());doc.SelectObjectById(existing->m_id);}
    else{auto root=std::make_unique<CAssembled>("Kitchen Layout",rootIds);root->SetParametricDefinition("kitchen_layout",k.Encode());doc.AddObject(std::move(root));}
    // Newly generated parts must respect hidden rows/modules as well as the root.
    for(const auto& o:doc.GetObjects())if(o&&dynamic_cast<CGroup*>(o.get())&&!o->IsVisible()
        && (o.get()==existing||o->GetGroupName().rfind("Kitchen group ",0)==0))doc.SetObjectVisibility(o->m_id,false);
    return true;
}

bool UpdateKitchenModuleParameters(CAlfaDoc& doc,unsigned long moduleId,const std::vector<ParametricParameterValue>& values,std::string& error) {
    auto* root=FindKitchenLayout(doc,moduleId);auto* object=FindKitchenModuleObject(doc,moduleId);
    if(!root||!object){error="This module does not belong to a kitchen.";return false;}
    auto k=KitchenLayout::Decode(root->GetParametricParameters());int uid=FindKitchenModule(doc,moduleId);
    auto it=std::find_if(k.modules.begin(),k.modules.end(),[&](const KitchenModule& m){return m.uid==uid;});
    if(it==k.modules.end())return false;auto& m=*it;bool drawers=m.type>=2&&m.type<=4;
    std::map<std::string,double> baseline;for(const auto& p:object->GetParametricParameters())baseline[p.id]=p.value;
    bool heightsChanged=false;bool changed=false;
    for(const auto& p:values) {
        auto old=baseline.find(p.id);if(old==baseline.end()||std::abs(old->second-p.value)<1e-7)continue;
        changed=true;
        if(p.id=="width")m.width=p.value;
        else if(p.id=="height")m.height=p.value;
        else if(p.id=="depth")m.depth=p.value;
        else if(p.id=="panel_thickness")m.panelThickness=p.value;
        else if(p.id=="handle_type")m.handleType=int(p.value);
        else if(p.id=="make_legs")m.addLegs=int(p.value);
        else if(p.id=="facade_style"&&!drawers)m.facadeStyle=int(p.value);
        else if(p.id=="facade_type"&&drawers)m.facadeStyle=int(p.value);
        else if(p.id=="facade_type"&&!drawers&&(p.value==1||p.value==2))m.type=int(p.value)-1;
        else if(p.id=="showcase_fill")m.showcaseFill=int(p.value);
        else if(p.id=="shelf_count")m.shelves=int(p.value);
        else if(p.id=="door_axis")m.doorAxis=int(p.value);
        else if(p.id=="door_open_angle"||p.id=="left_door_open_angle"||p.id=="right_door_open_angle")m.doorAngle=p.value;
        else if(p.id=="drawer_count"){m.drawerCount=int(p.value);heightsChanged=true;}
        else if(p.id.rfind("drawer_height_",0)==0)heightsChanged=true;
        else if(p.id=="open_drawer")m.openDrawer=int(p.value);
        else if(p.id=="pullout_distance")m.pullout=p.value;
        else {
            bool material=false;for(int i=0;i<4;++i)if(p.id==materialKeys[i]){m.materials[i]=p.value;material=true;}
            if(!material){error="Edit this setting through Kitchen Layout, or detach the cabinet before changing its construction: "+p.id;return false;}
        }
    }
    if(!changed)return true;
    if(heightsChanged)for(int i=0;i<6;++i)for(const auto& p:values)if(p.id=="drawer_height_"+std::to_string(i+1))m.drawerRatios[i]=p.value;
    if(!BuildKitchenLayout(doc,k,root,error))return false;
    doc.SelectObjectById(moduleId);return true;
}

unsigned long PickKitchenPlanModule(CAlfaDoc& doc,DomPoint point,const std::function<bool(Vec3,DomPoint&)>& project) {
    for(auto it=doc.GetObjects().rbegin();it!=doc.GetObjects().rend();++it){
        auto* outline=dynamic_cast<CPolyline*>(it->get());
        if(!outline||outline->GetParametricToolId()!="kitchen_plan_item"||outline->GetName()!="module outline"||!doc.IsObjectSelectable(*outline)||outline->GetPoints().size()<4)continue;
        std::array<DomPoint,4> projected;bool valid=true;
        for(int i=0;i<4;++i){const auto& p=outline->GetPoints()[i];if(!project({float(p.x),float(p.y),float(p.z)},projected[i])){valid=false;break;}}
        if(!valid)continue;bool positive=false,negative=false;double area=0;
        for(int i=0;i<4;++i){const auto a=projected[i],b=projected[(i+1)%4];double cross=double(b.x-a.x)*(point.y-a.y)-double(b.y-a.y)*(point.x-a.x);positive|=cross>0;negative|=cross<0;area+=double(a.x)*b.y-double(a.y)*b.x;}
        if(std::abs(area)>1&&!(positive&&negative))return outline->m_id;
    }
    for(auto it=doc.GetObjects().rbegin();it!=doc.GetObjects().rend();++it){auto* o=it->get();if(!o||o->GetParametricToolId()!="kitchen_plan_item"||!doc.IsObjectSelectable(*o))continue;
        if(auto* text=dynamic_cast<CDrawingText*>(o))if(text->HitTestScreen(point,project,7))return o->m_id;
        if(auto* line=dynamic_cast<CPolyline*>(o)){const auto& pts=line->GetPoints();for(size_t i=1;i<pts.size();++i){DomPoint a,b;
            if(!project({float(pts[i-1].x),float(pts[i-1].y),float(pts[i-1].z)},a)||!project({float(pts[i].x),float(pts[i].y),float(pts[i].z)},b))continue;
            double dx=b.x-a.x,dy=b.y-a.y,l=dx*dx+dy*dy;if(l<1)continue;double t=std::clamp(((point.x-a.x)*dx+(point.y-a.y)*dy)/l,0.0,1.0);
            if(std::hypot(point.x-a.x-t*dx,point.y-a.y-t*dy)<=7)return o->m_id;
        }}
    }
    return 0;
}

