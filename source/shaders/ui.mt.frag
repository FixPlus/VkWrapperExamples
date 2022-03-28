#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2, binding = 0) uniform sampler2D fontSampler;

SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = inColor * texture(fontSampler, inUVW.xy);
    return ret;
}
