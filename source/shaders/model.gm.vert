#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec4 inJoint;
layout (location = 6) in vec4 inWeight;

layout (set = 0, binding = 0) uniform PrimitiveTransform{
    mat4 local;
} transform;


WorldVertexInfo Geometry(){
    WorldVertexInfo ret;
    ret.UVW = vec3(inUV, 0.0f);
    ret.position = vec3(transform.local * inPos);
    ret.normal = normalize(vec3(transform.local * vec4(inNormal, 0.0f)));
    ret.tangent = normalize(vec3(transform.local * vec4(inTangent, 0.0f)));
    return ret;
}
