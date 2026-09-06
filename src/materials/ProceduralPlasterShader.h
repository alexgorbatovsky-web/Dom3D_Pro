#pragma once
inline const char* PlasterShaderSource() { return R"GLSL(
        varying vec3 materialPosition;
        varying mat3 materialNormalToEye;
        uniform bool proceduralPlaster;
        uniform vec2 plasterSeed;
        uniform bool plasterHighQuality;
        uniform float plaster_scale,plaster_grainSize,plaster_grainStrength;
        uniform float plaster_macroSize,plaster_macroStrength,plaster_microSize,plaster_microStrength;
        uniform float plaster_poreSize,plaster_poreDensity,plaster_poreDepth;
        uniform float plaster_roughness,plaster_roughnessVariation,plaster_colorVariation,plaster_relief,plaster_aoStrength;
        float phash(vec3 p) {
            p=fract(p*vec3(.1031,.1030,.0973)+vec3(plasterSeed*.000013,plasterSeed.x*.000017));
            p+=dot(p,p.yxz+33.33);return fract((p.x+p.y)*p.z);
        }
        float pnoise(vec3 p) {
            // Oblique lattice avoids grain bands aligned with architectural axes.
            p=mat3(0.0,.8,.6,-.8,-.36,.48,.6,-.48,.64)*p;
            vec3 i=floor(p),f=fract(p);f=f*f*f*(f*(f*6.0-15.0)+10.0);
            return mix(mix(mix(phash(i),phash(i+vec3(1,0,0)),f.x),mix(phash(i+vec3(0,1,0)),phash(i+vec3(1,1,0)),f.x),f.y),
                       mix(mix(phash(i+vec3(0,0,1)),phash(i+vec3(1,0,1)),f.x),mix(phash(i+vec3(0,1,1)),phash(i+vec3(1,1,1)),f.x),f.y),f.z)*2.0-1.0;
        }
        float bandWeight(float size,float footprint) {return 1.0-smoothstep(.3,1.2,footprint/size);}
        float PlasterHeight(vec3 p,float footprint) {
            float h=pnoise(p/plaster_macroSize)*plaster_macroStrength*bandWeight(plaster_macroSize,footprint);
            h+=pnoise(p/plaster_grainSize)*plaster_grainStrength*bandWeight(plaster_grainSize,footprint);
            if(plasterHighQuality) h+=pnoise(p/plaster_microSize+vec3(17.3))*plaster_microStrength*bandWeight(plaster_microSize,footprint);
            // Continuous pockets also intersect axis-aligned walls on lattice planes.
            float poreField=.5+.5*pnoise(p/plaster_poreSize+vec3(7.31,13.57,19.73));
            float threshold=1.0-.65*plaster_poreDensity;
            float pit=smoothstep(threshold,threshold+.16,poreField);
            h-=pit*plaster_poreDepth*bandWeight(plaster_poreSize,footprint);
            return h;
        }
        struct MaterialSample { vec3 baseColor;vec3 normal;float roughness;float metallic;float ao; };
        MaterialSample EvaluatePlaster(vec3 normal,vec3 color) {
            vec3 p=materialPosition/plaster_scale;
            float footprint=max(length(dFdx(p)),length(dFdy(p)));
            float e=max(.02,min(plaster_grainSize*.15,max(footprint*.5,.02)));
            float h=PlasterHeight(p,footprint);
            float xp=PlasterHeight(p+vec3(e,0,0),footprint),xm=PlasterHeight(p-vec3(e,0,0),footprint);
            float yp=PlasterHeight(p+vec3(0,e,0),footprint),ym=PlasterHeight(p-vec3(0,e,0),footprint);
            float zp=PlasterHeight(p+vec3(0,0,e),footprint),zm=PlasterHeight(p-vec3(0,0,e),footprint);
            vec3 grad=materialNormalToEye*vec3(xp-xm,yp-ym,zp-zm)/(2.0*e);
            grad-=normal*dot(grad,normal);
            MaterialSample m;m.normal=normalize(normal-grad*plaster_relief);
            float structure=clamp(h/max(plaster_grainStrength+plaster_macroStrength+plaster_microStrength,.001),-1.0,1.0);
            m.baseColor=color*(1.0+structure*plaster_colorVariation);
            m.roughness=clamp(plaster_roughness+structure*plaster_roughnessVariation,.04,1.0);
            m.metallic=0.0;
            float cavity=max(0.0,(xp+xm+yp+ym+zp+zm)/6.0-h)/max(e,.02);
            m.ao=1.0-min(.3,cavity*plaster_aoStrength);
            return m;
        }
)GLSL"; }
