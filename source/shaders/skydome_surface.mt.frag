#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inTangent;



SurfaceInfo Material(){

    SurfaceInfo ret;
    ret.albedo = vec4(0.0f);
    ret.position = inWorldPos;
    ret.normal = inWorldNormal;
    ret.cameraOffset = inViewPos;
    ret.metallic = 0.0f;
    ret.roughness = 1.0f;
    return ret;
}
