#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2, binding = 0) uniform Waves{
    vec4 deepWaterColor;
    vec2 metallicRoughness;
}waves;


SurfaceInfo Material(){

    float viewAngle = dot(-normalize(inWorldNormal), normalize(inWorldNormal - inViewPos));
    SurfaceInfo ret;
    ret.albedo = vec4(1.0f); //waves.deepWaterColor;
    //ret.albedo.a = 1.0f;// - 0.8f * abs(viewAngle);
    ret.position = inWorldPos;
    ret.normal = inWorldNormal;
    ret.cameraOffset = inViewPos;
    ret.metallic = waves.metallicRoughness.x;
    ret.roughness = waves.metallicRoughness.y;

    return ret;
}
