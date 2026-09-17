#pragma once
// Shared by the viewport, material previews and the GLB texture baker.
inline const char* PerforationShaderSource(){return R"GLSL(
uniform bool proceduralPerforation,perforationUseUV;
uniform int perforationPattern;
uniform float perforation_holeSize,perforation_bridge,perforation_bevel;
uniform float perforation_rotation,perforation_uvSize;
vec2 PerforationCoordinates(vec3 n){
    vec2 q;
    if(perforationUseUV)q=MaterialMappedUV(textureUV)*perforation_uvSize;
    else {
        vec3 axis=abs(vec3(dot(materialNormalToEye[0],n),dot(materialNormalToEye[1],n),dot(materialNormalToEye[2],n)));
        q=axis.z>=axis.x && axis.z>=axis.y ? materialPosition.xy : (axis.y>=axis.x ? materialPosition.xz : materialPosition.yz);
    }
    float a=radians(perforation_rotation);
    return mat2(cos(a),-sin(a),sin(a),cos(a))*q;
}
float PerforationDistance(vec2 q){
    float pitch=perforation_holeSize+perforation_bridge;
    vec2 period=vec2(pitch,1.732050808*pitch);
    vec2 a=mod(q+.5*period,period)-.5*period;
    vec2 b=mod(q,period)-.5*period;
    vec2 cell=dot(a,a)<dot(b,b)?a:b;
    float radius=perforationPattern==0 ? length(cell) : max(abs(cell.x),dot(abs(cell),vec2(.5,.866025404)));
    return radius-.5*perforation_holeSize;
}
float PerforationMask(vec3 n){return step(0.0,PerforationDistance(PerforationCoordinates(n)));}
vec3 PerforationNormal(vec3 n){
    vec2 q=PerforationCoordinates(n);
    float d=PerforationDistance(q);
    float bevel=min(perforation_bevel,perforation_bridge*.45);
    float t=clamp(d/max(bevel,.0001),0.0,1.0);
    float epsilon=max(.0001,perforation_holeSize*.0001);
    vec2 g=vec2(PerforationDistance(q+vec2(epsilon,0))-PerforationDistance(q-vec2(epsilon,0)),
                PerforationDistance(q+vec2(0,epsilon))-PerforationDistance(q-vec2(0,epsilon)))/(2.0*epsilon);
    g*=bevel>0.0 ? 6.0*t*(1.0-t) : 0.0;
    vec3 dx=dFdx(eyePosition),dy=dFdy(eyePosition);
    vec3 r1=cross(dy,n),r2=cross(n,dx);
    float determinant=dot(dx,r1);
    vec3 grad=(r1*dot(g,dFdx(q))+r2*dot(g,dFdy(q)))*sign(determinant)/max(abs(determinant),.00000001);
    return normalize(n-grad);
}
)GLSL";}
