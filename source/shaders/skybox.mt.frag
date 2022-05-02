#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2,binding = 0) uniform Globals{
    vec4 viewportYAxis;
    vec4 viewportXAxis;
    vec4 viewportDirectionAxis;
    vec4 cameraPos;
    vec4 params; // x - fov, y - ratio
} globals;

layout (set = 2, binding = 1) uniform Sun{
    vec4 color;
    vec4 params; // x - phi, y - psi, z - irradiance
} sun;

layout (set = 2, binding = 2) uniform Atmosphere{
    vec4 K; // K.xyz - scattering constants in Rayleigh scatter model for rgb chanells accrodingly, k.w - scattering constant for Mie scattering
    vec4 params; // x - planet radius, y - atmosphere radius, z - H0: atmosphere density factor, w - g: coef for Phase Function modeling Mie scattering
    int samples;
} atmosphere;

vec3 rayDirection(){
    float fov = globals.params.x;
    float ratio = globals.params.y;
    vec3 ret = vec3(globals.viewportDirectionAxis + globals.viewportXAxis * tan(fov * ratio / 2.0f * (inUVW.x * 2.0f - 1.0f)) + globals.viewportYAxis * tan(fov / 2.0f * (inUVW.y * 2.0f - 1.0f)));
    ret = normalize(ret);
    ret.y *= -1.0f;
    return ret;
}

#define PI 3.141592

vec2 sphericalCoords(vec3 rayDir){

    vec2 planeProj = rayDir.xz;
    float psi = PI / 2.0f - asin(rayDir.y);
    float sinVal = planeProj.x / abs(sin(psi));
    if(abs(sinVal) > 1.0f)
        sinVal = sign(sinVal);
    float phi = asin(sinVal);
    if(planeProj.y < 0.0f)
        phi = sign(planeProj.x) * PI - phi;


    return vec2(phi, psi);
}

float phaseFunction(float psi, float g){
    //float g = atmosphere.params.w;
    float cosPsi = cos(psi);
    return (3.0f * (1 - g * g) / (2.0f * (2.0f + g * g))) * (1 + cosPsi * cosPsi) / pow(1 + g * g - 2.0f * g * cosPsi, 3.0f / 2.0f);
}

vec3 outScattering(float height, float psi){
    float samples = atmosphere.samples;
    float atmosphereDepth = (atmosphere.params.y - height) / atmosphere.params.y * (1.0f + psi);
    float stepDistance = atmosphereDepth / samples;
    vec3 ret = vec3(0);
    vec3 K = atmosphere.K.xyz;
    float H0 = atmosphere.params.z;
    for(int i = 0; i < samples; ++i){
        float currentHeight = height / atmosphere.params.y + i * stepDistance * cos(psi);
        ret += 4.0f * PI * K * exp(-currentHeight/ H0) * stepDistance;
    }

    return ret;
}
vec4 skyColor(vec2 sphCoords){
    //if(sphCoords.y > PI / 2.0f || sun.params.y > PI / 2.0f)
        //return vec4(0.0f);
    float samples = atmosphere.samples;
    float height = globals.cameraPos.y;
    float atmosphereDepth = (atmosphere.params.y - height) / atmosphere.params.y * (1.0f + sphCoords.y);
    float stepPerSample = atmosphereDepth / samples;
    vec3 ray = rayDirection();
    vec3 sunDir = vec3(sin(sun.params.x) * sin(sun.params.y), cos(sun.params.y), cos(sun.params.x) * sin(sun.params.y));
    ray.y *= -1.0f;
    float sunPsi = asin(length(cross(sunDir, ray)));

    if(dot(sunDir, ray) < 0.0f){
        sunPsi = sign(sunPsi) * PI - sunPsi;
    }
    //return vec4(pow(sunPsi / PI * 1.1f, 3.0f), 0.0f, 0.0f, 1.0f);
    float phaseMie = phaseFunction(sunPsi, atmosphere.params.w);
    float phaseReleigh = phaseFunction(sunPsi, 0.0f);
    float H0 = atmosphere.params.z;
    vec3 ret = vec3(0.0f);
    vec3 sunIrradiance = sun.color.xyz * sun.params.z;

    for(int i = 0; i < samples; ++i){
        float currentHeight = height/ atmosphere.params.y + i * cos(sphCoords.y) * stepPerSample;
        ret += exp(-currentHeight/H0)  * stepPerSample * exp(-outScattering(currentHeight, sun.params.y) - (outScattering(height, sphCoords.y) - outScattering(currentHeight, sphCoords.y)));
    }

    ret = sunIrradiance * ret * (atmosphere.K.xyz * phaseReleigh + atmosphere.K.w * phaseMie);

    return vec4(ret, 1.0f);

}


SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = skyColor(sphericalCoords(rayDirection()));
    //ret.albedo = vec4((PI + sphericalCoords(rayDirection()).x) / (2.0f * PI), 0.0f, 0.0f, 1.0f);
    return ret;
}
