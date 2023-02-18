#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"


SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = vec4(0.0f);
    ret.position = vec3(0.0f);
    ret.normal = vec3(0.0f);
    ret.cameraOffset = vec3(0.0f);
    return ret;
}
