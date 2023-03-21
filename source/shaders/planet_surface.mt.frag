#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"
#define PI 3.1415926

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inWorldTangent;

layout (set = 2, binding = 0) uniform sampler2D colorMap;
layout (set = 2, binding = 1) uniform sampler2D bumpMap;
layout (set = 2, binding = 2) uniform sampler2D metallicMap;

layout (set = 2, binding = 3) uniform Landscape{
    vec4 props; // x - height, y - planet radius
    int samples;
} landscape;

vec3 calculateNormal(vec2 uv)
{
    vec3 tangentSample = texture(bumpMap, uv).xyz;
    if(length(tangentSample) == 0.0f)
    return inWorldNormal;
    vec3 tangentNormal = tangentSample * 2.0 - 1.0;

    vec3 N = normalize(inWorldNormal);
    vec3 T = normalize(inWorldTangent);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

vec2 calculateDisp(vec3 viewDir)
{
    float planetRadius = landscape.props.y;
    vec3 N = normalize(inWorldNormal);
    vec3 T = normalize(inWorldTangent);
    vec3 B = normalize(cross(N, T));

    int samples = landscape.samples;
    float heightScale = landscape.props.x;
    float distanceScale = 1.0f / (planetRadius * 2.0f * PI);
    float viewAngleCos = clamp(dot(-viewDir, N), 0.0f, 1.0f);
    float gamma = acos(planetRadius / (planetRadius + heightScale) * sin(acos(viewAngleCos))) - (PI / 2.0f - acos(viewAngleCos));
    float distance = planetRadius * gamma * distanceScale;
    vec2 dispVec2 = vec2(dot(T, -viewDir), -dot(B, -viewDir)) * distance;
    vec2 orig = vec2(planetRadius, 0.0f);
    vec2 dest = vec2(cos(gamma), sin(gamma)) * (planetRadius + heightScale);
    vec2 ray = dest - orig;

    vec2 curUV = inUVW.xy + dispVec2;

    vec2 lastUV = curUV;
    float lastHeight = (2.0f * texture(bumpMap, curUV).a - 1.0f);
    float lastRayHeight = 1.0f;

    if(lastHeight >= lastRayHeight)
      return curUV;
    for(int i = 1; i <= samples; ++i){
        float factor = 1.0f - float(i) / float(samples);
        curUV = inUVW.xy + factor * dispVec2;
        float curHeight = (2.0f * texture(bumpMap, curUV).a - 1.0f);
        float currentRayHeight = (length(ray * factor + orig) - planetRadius) / heightScale;
        if(curHeight > currentRayHeight){
            float currentDepth = curHeight - currentRayHeight;
            float prevDepth = abs(lastHeight - lastRayHeight);
            float weight = prevDepth / (currentDepth + prevDepth);
            return curUV * weight + (1.0f - weight) * lastUV;
        }
        lastHeight = curHeight;
        lastRayHeight = currentRayHeight;
        lastUV = curUV;
    }

    return inUVW.xy;
#if 0
    float depthSample = 2.0f * texture(bumpMap, inUVW.xy).a - 1.0f;
    depthSample *= 0.004f;

    float viewAngle = clamp(dot(viewDir, N), 0.5, 1.5f);
    vec3 resultViewVec = sin(viewAngle) * (viewDir - N * dot(viewDir, N)) + cos(viewAngle) * N;
    vec3 disp = resultViewVec * (depthSample / dot(resultViewVec, N)) + N * depthSample;
    vec2 uvDiff = vec2(-dot(T, disp), dot(B, disp));

    depthSample = 2.0f * texture(bumpMap, inUVW.xy + uvDiff).a - 1.0f;
    depthSample *= 0.004f;
    disp = resultViewVec * (depthSample / dot(resultViewVec, N)) + N * depthSample;
    uvDiff = vec2(-dot(T, disp), dot(B, disp));

    depthSample = 2.0f * texture(bumpMap, inUVW.xy + uvDiff).a - 1.0f;
    depthSample *= 0.004f;
    disp = resultViewVec * (depthSample / dot(resultViewVec, N)) + N * depthSample;
    uvDiff = vec2(-dot(T, disp), dot(B, disp));

    depthSample = 2.0f * texture(bumpMap, inUVW.xy + uvDiff).a - 1.0f;
    depthSample *= 0.004f;
    disp = resultViewVec * (depthSample / dot(resultViewVec, N)) + N * depthSample;
    uvDiff = vec2(-dot(T, disp), dot(B, disp));

    depthSample = 2.0f * texture(bumpMap, inUVW.xy + uvDiff).a - 1.0f;
    depthSample *= 0.004f;
    disp = resultViewVec * (depthSample / dot(resultViewVec, N)) + N * depthSample;
    uvDiff = vec2(-dot(T, disp), dot(B, disp));

    return inUVW.xy + uvDiff;
    //return inUVW.xy;
    #endif
}
SurfaceInfo Material(){
    vec3 direction = normalize(inWorldPos - inViewPos);
    vec2 UV =  calculateDisp(direction);

    SurfaceInfo ret;
    ret.albedo = texture(colorMap, UV);
    ret.metallic = texture(metallicMap, UV).x;
    ret.position = inWorldPos;
    //if(ret.metallic > 0.5f)
        //ret.normal = inWorldNormal;
    //else
        ret.normal = calculateNormal(UV);
    ret.cameraOffset = inViewPos;

    ret.roughness = 1.0f;
    return ret;
}
