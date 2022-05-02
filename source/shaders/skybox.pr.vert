#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 outUVW;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outWorldNormal;
layout (location = 4) out vec3 outViewPos;

void Projection(WorldVertexInfo worldVertexInfo){
    vec4 outPos = vec4((gl_VertexIndex % 2) * 4.0f - 1.0f, ((gl_VertexIndex + 1) % 2) * (gl_VertexIndex / 2) * 4.0f - 1.0f, 0.0f, 1.0f);
    gl_Position = outPos;
    outColor = vec4(1.0f);
    outUVW.x = (gl_VertexIndex & 1u) * 2.0f;
    outUVW.y = (gl_VertexIndex & 2u);
    outWorldPos = vec3(outPos);
    outWorldNormal = vec3(0.0f, 1.0f, 0.0f);
    outViewPos = outWorldPos;
}