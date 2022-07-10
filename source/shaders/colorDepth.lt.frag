#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) out vec4 outFragColor;
layout (location = 1) out vec4 outFragDepth;

void Lighting(SurfaceInfo surfaceInfo){
    outFragColor = surfaceInfo.albedo;
    outFragDepth = vec4(surfaceInfo.cameraOffset.x, 0.0f, 0.0f, 0.0f);
}