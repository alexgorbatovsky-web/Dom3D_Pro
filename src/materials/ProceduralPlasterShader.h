#pragma once
inline const char* PlasterShaderSource() { return R"GLSL(
        varying vec3 materialPosition;
        varying mat3 materialNormalToEye;
        uniform bool proceduralPlaster;
        uniform vec2 plasterSeed;
        uniform bool plasterHighQuality;
        uniform float plaster_scale,plaster_grainSize,plaster_grainStrength;
        uniform float plaster_pattern,plaster_patternSize,plaster_patternDepth,plaster_patternDensity,plaster_patternStretch,plaster_patternAngle;
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
        vec3 plasterProjectionWeights=vec3(0,0,1);
        float plasterPatternPlane(vec3 p) {
            float angle=radians(plaster_patternAngle);
            vec3 q=p/plaster_patternSize;
            q.xy=mat2(cos(angle),-sin(angle),sin(angle),cos(angle))*q.xy;
            float n=pnoise(q), fine=pnoise(q*3.7+vec3(9.2));
            float coverage=plaster_patternDensity;
            if(plaster_pattern<1.5) {
                // Broken, wandering grooves pulled by aggregate in the trowel.
                vec3 s=q;s.y/=plaster_patternStretch;s.z/=plaster_patternStretch;
                float field=pnoise(s+vec3(.17*fine,0,0));
                return -smoothstep(1.0-coverage*1.3,1.12-coverage*1.3,field);
            }
            if(plaster_pattern<2.5) {
                // Flattened islands over a coarse porous substrate.
                float island=smoothstep(-.12,.12,n+.4*fine+(coverage-.5));
                return mix(-.55+.3*fine,.35,island);
            }
            if(plaster_pattern<3.5) {
                // Independent curled trowel strokes; cell jitter avoids a repeated stamp.
                vec2 cell=floor(q.xy),f=fract(q.xy);float result=0.0;
                for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
                    vec3 id=vec3(cell+vec2(x,y),floor(q.z));
                    vec2 center=vec2(x,y)+vec2(phash(id),phash(id+17.0));
                    vec2 d=f-center;float r=length(d);
                    float turn=phash(id+31.0)*6.28;
                    d=mat2(cos(turn),-sin(turn),sin(turn),cos(turn))*d;
                    float a=atan(d.y,d.x);
                    float ring=abs(r-(.18+.075*(a+3.14)+.06*fine));
                    float stroke=(1.0-smoothstep(.025,.065,ring))*smoothstep(-2.6,-1.9,a)*(1.0-smoothstep(1.8,2.7,a));
                    result=max(result,stroke*step(phash(id+5.0),coverage));
                }
                return -result;
            }
            if(plaster_pattern<4.5) {
                vec2 cell=floor(q.xy),f=fract(q.xy);float result=0.0;
                for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
                    vec3 id=vec3(cell+vec2(x,y),0);
                    vec2 d=f-vec2(x,y)-vec2(phash(id),phash(id+17.0));
                    if(phash(id+33.0)>.5)d=d.yx;
                    float strokeLength=(.2+.65*(plaster_patternStretch-1.0)/19.0)*(.5+.5*phash(id+41.0));
                    float stroke=(1.0-smoothstep(.025,.08,abs(d.x+.06*fine)))*(1.0-smoothstep(strokeLength*.7,strokeLength,abs(d.y)));
                    result=max(result,stroke*step(phash(id+5.0),coverage));
                }
                return -result;
            }
            if(plaster_pattern<5.5) {
                float t=n+.24*fine;
                return .5*t+.35*smoothstep(-.08,.04,t+coverage-.5);
            }
            // Overlapping tapered folds, rather than closed stone-like cells.
            vec2 base=floor(q.xy),f=fract(q.xy);float folds=0.0;
            for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)for(int k=0;k<3;++k){
                vec3 id=vec3(base+vec2(x,y),float(k)*19.0);
                vec2 d=f-vec2(x,y)-vec2(phash(id),phash(id+19.0));
                float angle=phash(id+31.0)*6.283;
                d=mat2(cos(angle),-sin(angle),sin(angle),cos(angle))*d;
                float extent=.4+.4*phash(id+7.0);
                float taper=max(0.0,1.0-abs(d.x)/extent);
                float width=.18+.27*phash(id+11.0);
                float ridge=max(0.0,1.0-abs(d.y+.06*fine)/(width*max(taper,.01)));
                folds+=ridge*taper*(.5+.5*phash(id+23.0))*step(phash(id+47.0),coverage+.3);
            }
            return 1.8*(1.0-exp(-folds/1.8))+.15*n;
        }
        float plasterPattern(vec3 p) {
            float result=0.0;
            if(plasterProjectionWeights.z>.001)result+=plasterProjectionWeights.z*plasterPatternPlane(vec3(p.xy,0));
            if(plasterProjectionWeights.y>.001)result+=plasterProjectionWeights.y*plasterPatternPlane(vec3(p.xz,0));
            if(plasterProjectionWeights.x>.001)result+=plasterProjectionWeights.x*plasterPatternPlane(vec3(p.yz,0));
            return result;
        }
        float PlasterHeight(vec3 p,float footprint) {
            float h=pnoise(p/plaster_macroSize)*plaster_macroStrength*bandWeight(plaster_macroSize,footprint);
            h+=pnoise(p/plaster_grainSize)*plaster_grainStrength*bandWeight(plaster_grainSize,footprint);
            if(plasterHighQuality) h+=pnoise(p/plaster_microSize+vec3(17.3))*plaster_microStrength*bandWeight(plaster_microSize,footprint);
            // Continuous pockets also intersect axis-aligned walls on lattice planes.
            float poreField=.5+.5*pnoise(p/plaster_poreSize+vec3(7.31,13.57,19.73));
            float threshold=1.0-.65*plaster_poreDensity;
            float pit=smoothstep(threshold,threshold+.16,poreField);
            h-=pit*plaster_poreDepth*bandWeight(plaster_poreSize,footprint);
            if(plaster_pattern>.5)h+=plasterPattern(p)*plaster_patternDepth*bandWeight(plaster_patternSize*.15,footprint);
            return h;
        }
        struct MaterialSample { vec3 baseColor;vec3 normal;float roughness;float metallic;float ao; };
        MaterialSample EvaluatePlaster(vec3 normal,vec3 color) {
            vec3 objectNormal=abs(vec3(dot(materialNormalToEye[0],normal),dot(materialNormalToEye[1],normal),dot(materialNormalToEye[2],normal)));
            plasterProjectionWeights=pow(objectNormal,vec3(8));
            plasterProjectionWeights/=max(dot(plasterProjectionWeights,vec3(1)),.00001);
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
