#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) out vec4 outFragColor;

void Lighting(SurfaceInfo surfaceInfo){
    outFragColor = surfaceInfo.albedo;
}