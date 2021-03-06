#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 outUVW;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outWorldNormal;
layout (location = 4) out vec3 outViewPos;
layout (location = 5) out vec3 outWorldTangent;

layout (set = 1, binding = 0) uniform Camera{
    mat4 perspective;
    mat4 cameraSpace;
} camera;


void Projection(WorldVertexInfo worldVertexInfo){

    outWorldNormal = worldVertexInfo.normal;
    outWorldTangent = worldVertexInfo.tangent;
    outUVW = worldVertexInfo.UVW;
    outColor = worldVertexInfo.color;
    outWorldPos = worldVertexInfo.position;
    outViewPos = vec3(inverse(camera.cameraSpace) * vec4(0.0f, 0.0f, 0.0f, 1.0f));
    gl_Position = camera.perspective * camera.cameraSpace * vec4(worldVertexInfo.position, 1.0f);

}