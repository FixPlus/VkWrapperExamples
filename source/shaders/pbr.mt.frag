#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inWorldTangent;

layout (set = 2, binding = 0) uniform sampler2D colorMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D metallicRoughnessMap;

vec3 calculateNormal()
{
    vec3 tangentSample = texture(normalMap, inUVW.xy).xyz;
    if(length(tangentSample) == 0.0f)
        return inWorldNormal;
    vec3 tangentNormal = tangentSample * 2.0 - 1.0;

    vec3 N = normalize(inWorldNormal);
    vec3 T = normalize(inWorldTangent.xyz);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = texture(colorMap, inUVW.xy);
    ret.metallic = texture(metallicRoughnessMap, inUVW.xy).b;
    ret.roughness = texture(metallicRoughnessMap, inUVW.xy).g;
    ret.position = inWorldPos;
    ret.normal = calculateNormal();
    ret.cameraOffset = inViewPos;
    return ret;
}
