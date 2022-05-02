#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2,binding = 0) uniform Globals{
    vec4 viewportYAxis;
    vec4 viewportXAxis;
    vec4 viewportDirectionAxis;
    vec4 params; // x - fov, y - ratio
} globals;

layout (set = 2, binding = 1) uniform SkyMaterial{
    vec4 skyColor1;
    vec4 skyColor2;
} skyMaterial;

vec3 rayDirection(){
    float fov = globals.params.x;
    float ratio = globals.params.y;
    vec3 ret = vec3(normalize(globals.viewportDirectionAxis + globals.viewportXAxis * tan(fov) * (inUVW.x * 2.0f - 1.0f) + globals.viewportYAxis * tan(fov / ratio) * (inUVW.y * 2.0f - 1.0f)));
    ret.y *= -1.0f;
    return ret;
}

#define PI 3.141592

vec2 sphericalCoords(vec3 rayDir){

    vec2 planeProj = rayDir.xz;
    float phi = asin(planeProj.x) / PI;
    float psi = asin(rayDir.y) / (PI / 2.0f);

    return vec2(phi, psi);
}

vec4 skyColor(vec2 sphCoords){

    float f = 1.0f - 0.7f * abs(sphCoords.y);
    float phi = clamp((sphCoords.x + 1.0f) / 2.0f, 0.0f, 1.0f);
    vec4 ret = f * (phi * skyMaterial.skyColor1 + (1.0f - phi) * skyMaterial.skyColor2) ;//f * globals.skyColor + (1.0f - f) * vec4(0.8f, 0.8f, 0.8f, 1.0f);
    return ret;

}


SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = skyColor(sphericalCoords(rayDirection()));
    return ret;
}
