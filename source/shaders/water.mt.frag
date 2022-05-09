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

layout (set = 2, binding = 1) uniform sampler2D derivativesMap;
layout (set = 2, binding = 2) uniform sampler2D turbulenceMap;

SurfaceInfo Material(){

    float viewAngle = dot(-normalize(inWorldNormal), normalize(inWorldNormal - inViewPos));
    SurfaceInfo ret;

    ret.position = inWorldPos;
    vec4 derivatives = texture(derivativesMap, inUVW.xy);
    float turbulence = texture(turbulenceMap, inUVW.xy).r;
    vec3 binormal = normalize(vec3(1.0f, derivatives.x * 1.0f, 0.0f));
    vec3 tangent = normalize(vec3(0.0f, derivatives.y * 1.0f, 1.0f));
    ret.albedo = vec4(0.0f, 0.0f, 1.0f, 1.0f) * (1.0f - turbulence * 4.0f) + vec4(1.0f) * turbulence * 4.0f; //vec4(1.0f, 1.0f - turbulence * 4.0f, 1.0f - turbulence * 4.0f, 1.0f);
    ret.normal = normalize(cross( binormal, tangent));
    ret.cameraOffset = inViewPos;
    ret.metallic =  (1.0f - turbulence * 4.0f);
    ret.roughness = turbulence * 4.0f;

    return ret;
}
