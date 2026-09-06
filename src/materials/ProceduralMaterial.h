#pragma once
#include <cstdint>
#define DOM_FABRIC_PARAMETERS(X) \
 X(uvSize,100.f,.1f,10000.f,"UV tile size (mm)") \
 X(scale,1.f,.01f,100.f,"Scale") \
 X(threadSize,.45f,.05f,5.f,"Thread spacing (mm)") \
 X(threadWidth,.78f,.25f,.98f,"Thread width") \
 X(relief,.65f,0.f,3.f,"Relief") \
 X(irregularity,.3f,0.f,1.f,"Thread irregularity") \
 X(colorVariation,.12f,0.f,.5f,"Color variation") \
 X(roughness,.88f,.04f,1.f,"Roughness") \
 X(rotation,0.f,-180.f,180.f,"Weave angle (degrees)")
struct ProceduralFabricParameters {
    bool enabled=false;
    bool useUV=false;
    int weave=0; // Linen, cotton, twill, burlap.
    std::uint32_t seed=1;
#define FABRIC_FIELD(name,value,lo,hi,label) float name=value;
    DOM_FABRIC_PARAMETERS(FABRIC_FIELD)
#undef FABRIC_FIELD
};
inline ProceduralFabricParameters FabricPreset(int weave) {
    ProceduralFabricParameters p;p.enabled=true;p.weave=weave;
    if(weave==1){p.threadSize=.22f;p.threadWidth=.9f;p.irregularity=.08f;p.relief=.35f;p.colorVariation=.05f;}
    if(weave==2){p.threadSize=.32f;p.threadWidth=.92f;p.irregularity=.12f;p.relief=.5f;p.colorVariation=.18f;p.roughness=.78f;}
    if(weave==3){p.threadSize=1.2f;p.threadWidth=.65f;p.irregularity=.5f;p.relief=.9f;p.colorVariation=.22f;}
    return p;
}
#define DOM_PLASTER_PARAMETERS(X) \
 X(scale,1.0f,.01f,100.f,"Scale") \
 X(grainSize,.8f,.05f,20.f,"Grain Size (mm)") \
 X(grainStrength,.12f,0.f,2.f,"Grain Strength (mm)") \
 X(macroSize,30.f,1.f,500.f,"Macro Size (mm)") \
 X(macroStrength,.04f,0.f,2.f,"Macro Strength (mm)") \
 X(microSize,.18f,.02f,5.f,"Micro Size (mm)") \
 X(microStrength,.025f,0.f,.5f,"Micro Strength (mm)") \
 X(poreSize,.6f,.05f,10.f,"Pore Size (mm)") \
 X(poreDensity,.22f,0.f,1.f,"Pore Density") \
 X(poreDepth,.08f,0.f,2.f,"Pore Depth (mm)") \
 X(roughness,.85f,.04f,1.f,"Roughness") \
 X(roughnessVariation,.06f,0.f,.3f,"Roughness Variation") \
 X(colorVariation,.025f,0.f,.15f,"Color Variation") \
 X(relief,1.f,0.f,4.f,"Relief") \
 X(aoStrength,.12f,0.f,1.f,"AO Strength")
struct ProceduralPlasterParameters {
    bool enabled=false;
    std::uint32_t seed=1;
    bool highQuality=false;
#define FIELD(name,value,lo,hi,label) float name=value;
    DOM_PLASTER_PARAMETERS(FIELD)
#undef FIELD
};
