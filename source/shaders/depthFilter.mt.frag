#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2, binding = 0) uniform Settings{
    float distance;
} settings;

layout (set = 2, binding = 1) uniform sampler2D color;
layout (set = 2, binding = 2) uniform sampler2D depth;
#define DIFF_DELTA 0.001f
vec4 fxaa(){
    float diffX = ((length(texture(color, inUVW.xy + vec2(DIFF_DELTA, 0.0f)).rgb) - length(texture(color, inUVW.xy).rgb)) +
                 (-length(texture(color, inUVW.xy).rgb) + length(texture(color, inUVW.xy - vec2(DIFF_DELTA, 0.0f)).rgb))) / 2.0f;
    float diffY = ((length(texture(color, inUVW.xy + vec2(0.0f, DIFF_DELTA)).rgb) - length(texture(color, inUVW.xy).rgb)) +
    (-length(texture(color, inUVW.xy).rgb) + length(texture(color, inUVW.xy - vec2(0.0f, DIFF_DELTA)).rgb))) / 2.0f;

    float diffDepthX = ((length(texture(depth, inUVW.xy + vec2(DIFF_DELTA, 0.0f)).r) - length(texture(depth, inUVW.xy).r)) +
    (length(texture(depth, inUVW.xy).r) - length(texture(depth, inUVW.xy - vec2(DIFF_DELTA, 0.0f)).r))) / 2.0f;

    float diffDepthY = ((length(texture(depth, inUVW.xy + vec2(0.0f, DIFF_DELTA)).r) - length(texture(depth, inUVW.xy).r)) +
    (length(texture(depth, inUVW.xy).r) - length(texture(depth, inUVW.xy - vec2(0.0f, DIFF_DELTA)).r))) / 2.0f;
    //diffX /= 0.001f;
    //diffY /= 0.001f;


    vec2 gradient = clamp(vec2(diffX, diffY) * exp(-abs(vec2(diffDepthX, diffDepthY))) * 3.0f, 0.0f, 1.0f);

    //gradient -= 0.2f;
    //gradient = clamp(gradient, 0.0f, 1.0f);
    //if(length(gradient) < 0.1f)
        //return texture(color, inUVW.xy);
    //return vec4(abs(gradient), 0.0f, 0.0f);//(texture(color, inUVW.xy + normalize(gradient) * 0.001f) + texture(color, inUVW.xy - normalize(gradient) * 0.001f) + texture(color, inUVW.xy)) / 3.0f;
    vec4 nonFiltered = texture(color, inUVW.xy);
    vec4 filtered = (texture(color, inUVW.xy + normalize(gradient) * DIFF_DELTA) + texture(color, inUVW.xy - normalize(gradient) * DIFF_DELTA) + nonFiltered) / 3.0f;
    if(length(gradient) < 0.05f)
        return nonFiltered;

    return filtered * length(gradient) + nonFiltered * (1.0f - length(gradient));
}

vec4 filtered(){
    vec4 nonFilteredColor = texture(color, inUVW.xy);
    float depth = clamp(texture(depth, inUVW.xy).r / 10000.0f, 0.0f, 1.0f);
    depth = pow(depth, 0.25f);
    vec4 filteredColor = vec4(0.0f);
    int counter = 0;
    for(int i = -3; i < 3; ++i)
        for(int j = -3; j < 3; ++j){
            counter++;
            vec2 alteredUV = vec2(inUVW.x + i * 0.001f, inUVW.y + j * 0.001f);
            filteredColor += texture(color, alteredUV) ;
        }
    filteredColor /= counter;
    return filteredColor * depth + nonFilteredColor * (1.0f - depth);//nonFilteredColor;
}
SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = settings.distance == 0.0f ? fxaa()  : texture(color, inUVW.xy);
    ret.position = inWorldPos;
    ret.normal = inWorldNormal;
    ret.cameraOffset = inViewPos;
    return ret;
}
