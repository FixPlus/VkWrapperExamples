#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

#define NET_SIZE 10.0f
#define CELL_SIZE 0.1f

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform Waves{
    vec4 waves[4]; /* xy - wave vector, z - steepnees decay factor, w - steepness */
    vec4 params;
    float time;
}waves;

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
    vec4 position = vec4(vec3(localPos.x, 0.0f,localPos.y) + vec3(translate.x, 0.0f, translate.y), 1.0f);
    float distance = length(camera.cameraSpace * position);
    float height = 0.0f;
    vec3 tangent = vec3(1.0f, 0.0, 0.0f);
    vec3 binormal = vec3(0.0f, 0.0f, 1.0f);

    float gravitation = waves.params.x;

    for(int i = 0; i < 4; ++i){
        float lambda = length(waves.waves[i].xy);

        float steepnessFactor = clamp(1.0f - distance * waves.waves[i].z / ( lambda), 0.0f, 1.0f);

        if(lambda == 0.0f || steepnessFactor == 0.0f)
        continue;

        vec2 k = normalize(waves.waves[i].xy) * 2.0f * 3.1415f / lambda;

        float dx = waves.waves[i].x / lambda;
        float dy = waves.waves[i].y / lambda;

        float c = sqrt(gravitation / length(k));
        float f = dot(gridPos, k) + waves.time * c;
        float steepness = waves.waves[i].w * steepnessFactor;
        float waveAmp = steepness / length(k);


        position += vec4(cos(f) * dx, sin(f), cos(f) * dy, 0.0f) * waveAmp;

        tangent += vec3(- dx * dx * steepness * sin(f), dx * steepness * cos(f), -dx * dy * steepness * sin(f));
        binormal += vec3(- dx * dy * steepness * sin(f), dy * steepness * cos(f), - dy * dy * steepness * sin(f));

    }

    vec3 normal = normalize(cross( binormal, tangent));

    vec2 uv = localPos / NET_SIZE;

    WorldVertexInfo ret;

    ret.position = vec3(position);
    ret.color = vec4(0.0f, 0.1f, 0.7f, 1.0f);
    ret.UVW = vec3(uv, 1.0f);
    ret.normal = normal;
    ret.tangent = normalize(tangent);
    return ret;
}
