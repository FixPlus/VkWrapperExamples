#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inRayleigh;
layout (location = 6) in vec3 inMei;


float phaseFunction(float Thetha, float g){
    float cosThetha = cos(Thetha);
    float gSquared = g * g;
    return 3.0f * (1.0f - gSquared) * (1.0f + cosThetha * cosThetha) / (2.0f * (2.0f + gSquared) * pow(1.0f + gSquared -  2.0f * g * cosThetha, 3.0f / 2.0f));
}

vec3 dir(vec2 spherical){
    float phi = spherical.x;
    float psi = spherical.y;
    return vec3(cos(psi) * sin(phi), sin(psi), cos(psi) * cos(phi));
}

layout (set = 1, binding = 0) uniform Atmosphere{
    vec4 ScatterConstants; // xyz - rayleigh, w - mei
    vec4 properties; // x - planet radius, y - atmosphere radius, z - H0, w - g
    vec4 center;
    int samples;
} atmosphere;

layout (set = 1, binding = 1) uniform SunLight{
    vec4 color;
    vec3 pos; // x - phi, y -phi, z - distance
} sunlight;


SurfaceInfo Material(){

    vec3 direction = normalize(inWorldPos - inViewPos);
    vec3 sunDir = dir(vec2(sunlight.pos.x, sunlight.pos.y));
    float Thetha = acos(dot(direction, sunDir));

    float phaseMay = phaseFunction(Thetha, atmosphere.properties.w);
    float phaseRayleigh = phaseFunction(Thetha, 0.0f);

    SurfaceInfo ret;
    ret.albedo = vec4(inRayleigh * phaseRayleigh + inMei * phaseMay, 1.0f);
    //ret.albedo = vec4(inRayleigh + inMei, 1.0f);
    ret.position = inWorldPos;
    ret.normal = inWorldNormal;
    ret.cameraOffset = inViewPos;
    ret.metallic = 0.0f;
    ret.roughness = 1.0f;
    return ret;
}