#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 6) in vec3 inRayleigh;
layout (location = 7) in vec3 inMei;

layout (location = 0) out vec4 outFragColor;

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

void Lighting(SurfaceInfo surfaceInfo){
    vec3 direction = normalize(surfaceInfo.position - surfaceInfo.cameraOffset);
    vec3 sunDir = dir(vec2(sunlight.pos.x, sunlight.pos.y));
    float Thetha = acos(dot(direction, sunDir));

    float phaseMay = phaseFunction(Thetha, atmosphere.properties.w);
    float phaseRayleigh = phaseFunction(Thetha, 0.0f);

    outFragColor = vec4(inRayleigh * phaseRayleigh + inMei * phaseMay, 1.0f);
}