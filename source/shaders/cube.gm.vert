#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 inUV;

layout (location = 4) in mat4 inModel;


WorldVertexInfo Geometry(){
    WorldVertexInfo ret;
    ret.UVW = vec3(inUV, 0.0f);
    ret.position = vec3(inModel * vec4(pos, 1.0));
    ret.normal = normalize(vec3(inModel * vec4(normal, 0.0f)));
    ret.color = vec4(col, 1.0);
    return ret;
}

