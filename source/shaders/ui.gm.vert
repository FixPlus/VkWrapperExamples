#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;

layout (push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
} pushConstants;

WorldVertexInfo Geometry(){
    WorldVertexInfo ret;
    ret.UVW = vec3(inUV, 0.0f);
    ret.color = inColor;
    ret.position = vec3(inPos * pushConstants.scale + pushConstants.translate, 0.0f);
    return ret;
}