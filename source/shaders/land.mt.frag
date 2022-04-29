#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2, binding = 0) uniform Land{
    vec4 color;
    vec4 params;
}land;

vec4 mixColor(){
    vec4 base = land.color;
    float height = inWorldPos.y;
    float snowiness = 0.0f;

    snowiness = clamp(height - land.params.x, 0.0f, 1.0f);
    float steepness = abs(dot(inWorldNormal, normalize(vec3(1.0f, 0.0f, 1.0f))));

    base = snowiness * vec4(1.0f, 1.0f, 1.0f, 1.0f) + (1 - snowiness) * base;

    base = steepness * vec4(0.2f, 0.2f, 0.2f, 1.0f) + (1 - steepness) * base;

    return base;
}

SurfaceInfo Material(){

    SurfaceInfo ret;
    ret.albedo = mixColor(); //* inColor;
    ret.position = inWorldPos;
    ret.normal = inWorldNormal;
    ret.cameraOffset = inViewPos;
    ret.metallic = 0.0f;
    ret.roughness = 0.0f;

    return ret;
}
