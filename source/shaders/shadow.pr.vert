#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

#define SHADOW_CASCADES 4

layout (set = 1, binding = 0) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;


layout (set = 1, binding = 1) uniform CascadeID{
    int cascade;
} cascadeID;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 outUVW;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outWorldNormal;
layout (location = 4) out vec3 outViewPos;

void Projection(WorldVertexInfo worldVertexInfo){
    gl_Position = shadowSpace.cascades[cascadeID.cascade] * vec4(worldVertexInfo.position, 1.0);
}
