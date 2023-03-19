#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inInScatterReileigh;
layout (location = 6) in vec3 inInScatterMei;
layout (location = 7) in vec3 inOutScatterSun;
layout (location = 8) in vec3 inOutScatterEmissive;
layout (location = 9) in vec3 inWorldTangent;

layout (set = 2, binding = 0) uniform sampler2D colorMap;
layout (set = 2, binding = 1) uniform sampler2D bumpMap;

layout (set = 1, binding = 0) uniform Atmosphere{
    vec4 ScatterConstants; // xyz - rayleigh, w - mei
    vec4 properties; // x - planet radius, y - atmosphere radius, z - H0, w - g
    vec4 center;
    int samples;
} atmosphere;

layout (set = 1, binding = 1) uniform SunLight{
    vec4 color;
    vec4 pos; // x - phi, y -phi, z - intensity, w -distance
} sunlight;



float phaseFunction(float Thetha, float g){
    float cosThetha = cos(Thetha);
    float gSquared = g * g;
    return 3.0f * (1.0f - gSquared) * (1.0f + cosThetha * cosThetha) / (2.0f * (2.0f + gSquared) * pow(1.0f + gSquared -  2.0f * g * cosThetha, 3.0f / 2.0f));
}

vec3 dir(vec2 spherical){
    float phi = spherical.x;
    float psi = spherical.y;
    return vec3(cos(psi) * sin(phi), sin(psi), cos(psi) * cos(phi));
}


vec3 calculateNormal(vec2 uv)
{
    vec3 tangentSample = texture(bumpMap, uv).xyz;
    if(length(tangentSample) == 0.0f)
    return inWorldNormal;
    vec3 tangentNormal = tangentSample * 2.0 - 1.0;

    vec3 N = normalize(inWorldNormal);
    vec3 T = normalize(inWorldTangent);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

vec2 calculateDisp(vec3 viewDir)
{
    #if 0
    float depthSample = 2.0f * texture(bumpMap, inUVW.xy).a - 1.0f;
    depthSample *= 0.01f;
    vec3 N = normalize(inWorldNormal);
    vec3 T = normalize(inWorldTangent);
    vec3 B = normalize(cross(N, T));
    float viewAngle = clamp(dot(viewDir, N), 0.5f, 1.0f);
    vec3 resultViewVec = sin(viewAngle) * (viewDir - N * dot(viewDir, N)) + cos(viewAngle) * N;
    vec3 disp = resultViewVec * (depthSample / dot(resultViewVec, N)) + N * depthSample;
    vec2 uvDiff = vec2(-dot(T, disp), dot(B, disp));
    return inUVW.xy + uvDiff;
    #endif
    return inUVW.xy;
}
SurfaceInfo Material(){


    vec3 direction = normalize(inWorldPos - inViewPos);
    vec2 UV =  calculateDisp(direction);
    vec3 sunDir = dir(vec2(sunlight.pos.x, sunlight.pos.y));
    float Thetha = acos(dot(direction, sunDir));

    float phaseMay = phaseFunction(Thetha, atmosphere.properties.w);
    float phaseRayleigh = phaseFunction(Thetha, 0.0f);

    float diffuse = clamp(dot(-sunDir, calculateNormal(UV)), 0.0f, 1.0f);
    SurfaceInfo ret;
    ret.albedo = vec4(vec3(texture(colorMap, UV)) * inOutScatterEmissive * clamp(inOutScatterSun, 0.0f, 1.0f) * diffuse + inInScatterReileigh * phaseRayleigh + inInScatterMei * phaseMay, 1.0f);
    //ret.albedo = texture(bumpMap, inUVW.xy);
    ret.position = inWorldPos;
    ret.normal = inWorldNormal;
    ret.cameraOffset = inViewPos;
    ret.metallic = 0.0f;
    ret.roughness = 1.0f;
    return ret;
}
