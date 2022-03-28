#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"


layout (set = 3,binding = 0) uniform Globals{
    vec4 lightDir;
    vec4 skyColor;
    vec4 lightColor;
} globals;

layout (location = 0) out vec4 outFragColor;

void Lighting(SurfaceInfo surfaceInfo){

    vec3 normal = normalize( surfaceInfo.normal );
    vec4 cameraDir = vec4(surfaceInfo.position - surfaceInfo.cameraOffset, 1.0f);

    float fog = clamp(length(cameraDir) / 1000.0f, 0.0f , 1.0f);
    float diffuse = dot(globals.lightDir, vec4(normal, 0.0f));

    vec4 reflectDir = vec4(reflect(normalize(cameraDir).xyz, normal), 0.0f);
    float angle = clamp(dot(normalize(cameraDir).xyz, -normal), 0.0f, 1.0f);
    angle = pow(angle, 1.0f / 2.0f);
    vec4 baseColor = surfaceInfo.albedo * angle + globals.skyColor * (1.0f - angle);

    float reflect = clamp(dot(normalize(reflectDir), normalize(globals.lightDir)), 0.05f, 1.0f);
    reflect -= 0.05f;
    reflect = pow(reflect, 32.0f) * 2.2f;

    diffuse = (diffuse + 1.0f) / 2.0f;
    diffuse = diffuse * 0.4f + 0.2f;
    outFragColor = baseColor;
    outFragColor *= diffuse;
    outFragColor += reflect * globals.lightColor;
    //fog = 0.0f;
    outFragColor = outFragColor * (1.0f - fog) + globals.skyColor * fog;

}
