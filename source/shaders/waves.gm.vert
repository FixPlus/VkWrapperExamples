#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

#define NET_SIZE 10.0f
#define CELL_SIZE 0.1f

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform sampler2D displacementMap;
layout (set = 0, binding = 1) uniform Ubo{
    float scale;
} ubo;
layout (set = 1, binding = 0) uniform Camera{
    mat4 perspective;
    mat4 cameraSpace;
} camera;

layout (push_constant) uniform PushConstants {
    vec2 translate;
    float scale;
    float cellSize;
} pushConstants;

WorldVertexInfo Geometry(){
    vec2 localPos = inPos.xz * pushConstants.scale;

    vec2 translate = pushConstants.translate;
    vec2 gridPos = localPos + translate;
    vec2 inUV = gridPos / ubo.scale;
    vec4 position = vec4(vec3(localPos.x, 0.0f,localPos.y) + vec3(translate.x, 0.0f, translate.y), 1.0f);

    position += vec4(texture(displacementMap, inUV).xyz, 0.0f) * ubo.scale / 200.0f;

    WorldVertexInfo ret;

    ret.position = vec3(position);
    ret.color = vec4(0.0f, 0.1f, 0.7f, 1.0f);
    ret.UVW = vec3(inUV, 1.0f);
    return ret;
}
