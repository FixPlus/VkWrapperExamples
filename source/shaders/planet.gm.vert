#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 inUV;

layout (set = 0, binding = 0) uniform PrimitiveTransform{
    mat4 local;
} transform;

layout (push_constant) uniform PushConstants {
    float preScale;
} pushConstants;

WorldVertexInfo Geometry(){
    WorldVertexInfo ret;
    ret.UVW = vec3(inUV, 0.0f);
    ret.position = vec3(transform.local * vec4(inPos * pushConstants.preScale, 1.0f));
    ret.normal = normalize(vec3(transform.local * vec4(inNormal, 0.0f)));
    ret.tangent = normalize(vec3(transform.local * vec4(inTangent, 0.0f)));
    ret.color = vec4(1.0f);
    return ret;
}
