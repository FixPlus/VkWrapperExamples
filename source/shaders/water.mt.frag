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

layout (set = 2, binding = 1) uniform Ubo{
    vec4 scales;
} ubo;

layout (set = 2, binding = 2) uniform sampler2D derivativesMap1;
layout (set = 2, binding = 3) uniform sampler2D derivativesMap2;
layout (set = 2, binding = 4) uniform sampler2D derivativesMap3;
layout (set = 2, binding = 5) uniform sampler2D turbulenceMap1;
layout (set = 2, binding = 6) uniform sampler2D turbulenceMap2;
layout (set = 2, binding = 7) uniform sampler2D turbulenceMap3;

SurfaceInfo Material(){

    float viewAngle = dot(-normalize(inWorldNormal), normalize(inWorldNormal - inViewPos));
    SurfaceInfo ret;

    ret.position = inWorldPos;
    vec4 derivatives = texture(derivativesMap1, inUVW.xy / ubo.scales.x) + texture(derivativesMap2, inUVW.xy / ubo.scales.y)  + texture(derivativesMap3, inUVW.xy / ubo.scales.z);
    float turbulence = texture(turbulenceMap1, inUVW.xy / ubo.scales.x).r + texture(turbulenceMap2, inUVW.xy / ubo.scales.y).r + texture(turbulenceMap3, inUVW.xy / ubo.scales.z).r;
    turbulence *= 2.0f;
    turbulence = pow(turbulence, 8.0f);
    turbulence = clamp(turbulence, 0.0f, 1.0f);
    turbulence *= 0.8f;
    vec3 binormal = normalize(vec3(1.0f, derivatives.x * 1.0f, 0.0f));
    vec3 tangent = normalize(vec3(0.0f, derivatives.y * 1.0f, 1.0f));
    ret.albedo = vec4(1.0f);
    ret.normal = -normalize(cross( binormal, tangent));
    ret.cameraOffset = inViewPos;
    ret.metallic =  0.8f - turbulence;
    ret.roughness = 0.25f + clamp(turbulence, 0.0f, 1.0f);

    return ret;
}
