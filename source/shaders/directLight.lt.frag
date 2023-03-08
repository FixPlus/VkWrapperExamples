#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) out vec4 fragColor;

layout (set = 3, binding = 0) uniform Light{
   vec3 direction;
   vec3 color;
} light;

void Lighting(SurfaceInfo surfaceInfo){
    float diffuse = (dot(light.direction, surfaceInfo.normal) + 1.0f) * 0.5f;
    vec3 ambient = vec3(0.1f, 0.1f, 0.2f);
    fragColor = vec4(ambient + vec3(surfaceInfo.albedo) * light.color * diffuse, 1.0f);
}