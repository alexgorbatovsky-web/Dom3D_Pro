#pragma once
// Appended after ProceduralPlasterShader (shared MaterialSample and varyings).
inline const char* FabricShaderSource(){return R"GLSL(
uniform bool proceduralFabric;
uniform bool fabricUseUV,wrapObject;
uniform float fabric_uvSize;
uniform vec3 wrapCenter,wrapExtent;
uniform vec4 uvTransform;
uniform float uvRotation;
vec2 MaterialMappedUV(vec2 original){
    if(!wrapObject)return original;
    vec3 direction=normalize((materialPosition-wrapCenter)/max(wrapExtent,vec3(.0001)));
    // Azimuthal chart: continuous over top and sides, singularity underneath.
    vec2 q=direction.xy/sqrt(max(2.0*(1.0+direction.z),.0001))*.5;
    q*=uvTransform.zw;
    float c=cos(uvRotation),s=sin(uvRotation);
    return vec2(q.x*c-q.y*s,q.x*s+q.y*c)+.5+uvTransform.xy;
}
uniform int fabricWeave;
uniform vec2 fabricSeed;
uniform float fabric_scale,fabric_threadSize,fabric_threadWidth,fabric_relief;
uniform float fabric_irregularity,fabric_colorVariation,fabric_roughness,fabric_rotation;
float fhash(vec2 p){p=fract(p*vec2(.1031,.1030)+fabricSeed*.000013);p+=dot(p,p.yx+33.33);return fract((p.x+p.y)*p.x);}
vec2 FabricField(vec2 q,float footprint){
    float a=fabric_rotation*.01745329252;
    q=mat2(cos(a),-sin(a),sin(a),cos(a))*q/fabric_threadSize;
    q+=fabric_irregularity*.1*vec2(sin(q.y*.63+sin(q.x*.27)),sin(q.x*.71+sin(q.y*.31)));
    vec2 cell=floor(q),f=fract(q);
    float warp=fhash(vec2(cell.x,17.0)),weft=fhash(vec2(31.0,cell.y));
    vec2 slubs=vec2(sin(q.y*.71+warp*12.0),sin(q.x*.63+weft*12.0));
    vec2 widths=fabric_threadWidth*(vec2(1.0)+fabric_irregularity*(.4*(vec2(warp,weft)-.5)+.13*slubs));
    vec2 thread=max(vec2(0),cos(clamp((f-.5)/widths,-.5,.5)*3.14159265));
    thread*=thread;
    float over=mod(cell.x+cell.y,2.0);
    if(fabricWeave==2)over=step(1.5,mod(cell.x-cell.y,4.0));
    // Each strand remains continuous through cell boundaries. The raised
    // crossing fades along its length instead of ending in a square step.
    vec2 arch=sin(f*3.14159265);arch*=arch;
    float hx=thread.x*(.42+.5*over*arch.y);
    float hy=thread.y*(.42+.5*(1.0-over)*arch.x);
    float height=max(hx,hy);
    // Long slubs and fine twisted fibers soften otherwise perfect woven lines.
    float slub=mix(warp,weft,1.0-over)-.5+dot(slubs,vec2(.12));
    height*=1.0+fabric_irregularity*slub*.7;
    float fiber=mix(sin(q.y*53.0+sin(q.x*2.0)),sin(q.x*57.0+sin(q.y*3.0)),over);
    float visible=1.0-smoothstep(.25,1.1,footprint/fabric_threadSize);
    float fine=1.0-smoothstep(.025,.12,footprint/fabric_threadSize);
    height+=fiber*.025*fine;
    float tone=(height-.5)*.8+slub*.5;
    if(fabricWeave==2)tone+=(over-.5)*.6;
    return vec2(mix(.5,height,visible)*fabric_threadSize*.15,tone*visible);
}
vec2 FabricVolume(vec3 p,vec3 w,float fp){return FabricField(p.yz,fp)*w.x+FabricField(p.zx,fp)*w.y+FabricField(p.xy,fp)*w.z;}
MaterialSample EvaluateFabric(vec3 normal,vec3 color){
    if(fabricUseUV){
        vec2 q=MaterialMappedUV(textureUV)*fabric_uvSize/fabric_scale;
        float fp=max(length(dFdx(q)),length(dFdy(q)));
        float e=max(fabric_threadSize*.025,fp*.35);
        vec2 value=FabricField(q,fp);
        vec2 dh=vec2(FabricField(q+vec2(e,0),fp).x-FabricField(q-vec2(e,0),fp).x,
            FabricField(q+vec2(0,e),fp).x-FabricField(q-vec2(0,e),fp).x)/(2.0*e);
        vec3 dx=dFdx(eyePosition),dy=dFdy(eyePosition);
        vec3 r1=cross(dy,normal),r2=cross(normal,dx);
        float det=dot(dx,r1);
        vec3 gradient=(r1*dot(dh,dFdx(q))+r2*dot(dh,dFdy(q)))/max(abs(det),.0000001)*sign(det);
        MaterialSample m;m.normal=normalize(normal-gradient*fabric_scale*fabric_relief);
        m.baseColor=color*max(.1,1.0+value.y*fabric_colorVariation*2.0);
        m.roughness=clamp(fabric_roughness+value.y*.08,.04,1.0);m.metallic=0.0;
        m.ao=1.0-clamp(-value.y*.18,0.0,.15);return m;
    }
    vec3 p=materialPosition/fabric_scale;
    vec3 worldNormal=vec3(dot(materialNormalToEye[0],normal),dot(materialNormalToEye[1],normal),dot(materialNormalToEye[2],normal));
    vec3 w=pow(abs(worldNormal),vec3(8.0));w/=max(w.x+w.y+w.z,.00001);
    float fp=max(length(dFdx(p)),length(dFdy(p)));
    float e=max(fabric_threadSize*.025,fp*.35);
    vec2 value=FabricVolume(p,w,fp);
    vec3 gradient=vec3(FabricVolume(p+vec3(e,0,0),w,fp).x-FabricVolume(p-vec3(e,0,0),w,fp).x,
        FabricVolume(p+vec3(0,e,0),w,fp).x-FabricVolume(p-vec3(0,e,0),w,fp).x,
        FabricVolume(p+vec3(0,0,e),w,fp).x-FabricVolume(p-vec3(0,0,e),w,fp).x)/(2.0*e);
    gradient=materialNormalToEye*gradient;gradient-=normal*dot(gradient,normal);
    MaterialSample m;m.normal=normalize(normal-gradient*fabric_relief);
    m.baseColor=color*max(.1,1.0+value.y*fabric_colorVariation*2.0);
    m.roughness=clamp(fabric_roughness+value.y*.08,.04,1.0);m.metallic=0.0;
    m.ao=1.0-clamp(-value.y*.18,0.0,.15);return m;
}
)GLSL";}
