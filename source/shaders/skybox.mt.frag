#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

layout (set = 2,binding = 0) uniform Globals{
    mat4 invProjView;
    vec4 cameraPos;
    float near;
    float far;
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

layout(set = 2, binding = 3) uniform sampler2D outScatterTexture;

vec3 rayDirection(){
    vec3 ray = (globals.invProjView * vec4((2.0f * inUVW.xy - vec2(1.0f)) * (globals.far - globals.near), globals.far + globals.near, globals.far - globals.near)).xyz;
    ray = normalize(ray);
    return ray;
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
    phi = phi + PI;

    return vec2(phi, psi);
}

float phaseFunction(float psi, float g){
    //float g = atmosphere.params.w;
    float cosPsi = cos(psi);
    return (3.0f * (1 - g * g) / (2.0f * (2.0f + g * g))) * (1 + cosPsi * cosPsi) / pow(1 + g * g - 2.0f * g * cosPsi, 3.0f / 2.0f);
}

float blockedByPlanet(float height, float psi){
    return height < 0.0f || psi < asin(atmosphere.params.x / (atmosphere.params.x + height)) - 0.01f ? 0.0f: 1.0f;
}

vec2 scatterTexCoords(float height, float psi){
    return vec2((height + 0.1f * atmosphere.params.y) / (1.1f * atmosphere.params.y), psi / PI);
}
vec3 inScatter(float height, vec2 sphCoords, float distance){
    float samples = atmosphere.samples;
    float stepPerSample = distance / samples;
    vec3 ray = vec3(sin(sphCoords.x) * sin(sphCoords.y), cos(sphCoords.y), cos(sphCoords.x) * sin(sphCoords.y));
    vec3 sunDir = vec3(sin(sun.params.x) * sin(sun.params.y), cos(sun.params.y), cos(sun.params.x) * sin(sun.params.y));
    ray.y *= -1.0f;
    float sunPsi = asin(length(cross(sunDir, ray)));

    if(dot(sunDir, ray) < 0.0f){
        sunPsi = sign(sunPsi) * PI - sunPsi;
    }
    float phaseMie = phaseFunction(sunPsi, atmosphere.params.w);
    float phaseReleigh = phaseFunction(sunPsi, 0.0f);
    float H0 = atmosphere.params.z;
    vec3 ret = vec3(0.0f);
    vec3 sunIrradiance = sun.color.xyz * sun.params.z;
    float angle = ray.y > 0.0f ? sphCoords.y : PI - sphCoords.y;
    float toCenterOfPlanet = height + atmosphere.params.x;
    float toCenterOfPlanet2 = toCenterOfPlanet * toCenterOfPlanet;

    for(int i = 0; i < samples; ++i){
        float accDist = stepPerSample * i;
        float curHeightNonNorm  = sqrt(toCenterOfPlanet2 + accDist * accDist - 2.0f * cos(PI - sphCoords.y) * toCenterOfPlanet * accDist) - atmosphere.params.x;
        float currentHeight = curHeightNonNorm / atmosphere.params.y;

        float curPsi = acos((-toCenterOfPlanet2 + accDist * accDist + (atmosphere.params.x + curHeightNonNorm) * (atmosphere.params.x + curHeightNonNorm))/(2.0f * accDist * (atmosphere.params.x + curHeightNonNorm)));

        ret += blockedByPlanet(curHeightNonNorm, PI - sun.params.y) * exp(-currentHeight/H0)  * stepPerSample / atmosphere.params.y *
        exp(-vec3(texture(outScatterTexture,scatterTexCoords(curHeightNonNorm, sun.params.y)).rgb) -
        abs(vec3(texture(outScatterTexture, scatterTexCoords(height , sphCoords.y > PI / 2.0f ? PI -  sphCoords.y :  sphCoords.y))).rgb -
            vec3(texture(outScatterTexture, scatterTexCoords(curHeightNonNorm , sphCoords.y > PI / 2.0f ? PI -  curPsi : curPsi))).rgb));
    }

    ret = sunIrradiance * ret * (atmosphere.K.xyz * phaseReleigh + vec3(atmosphere.K.w * phaseMie));

    return ret;

}

float atmoDepth(float height, float psi){
    psi = PI -psi;
    float toCenterOfPlanet = height + atmosphere.params.x;
    float toAtmospherePeak = atmosphere.params.y - height;
    float outerRadius = atmosphere.params.y + atmosphere.params.x;
    float atmosphereDepth;
    float cos2Psi = cos(psi) * cos(psi);
    float sin2Psi = 1.0f - cos2Psi;

    if(psi < asin(atmosphere.params.x / (toCenterOfPlanet)) - 0.01f){
        // Ray intersect with planet
        atmosphereDepth = cos(psi) * (toCenterOfPlanet) - sqrt(cos2Psi * atmosphere.params.x * atmosphere.params.x - sin2Psi * (2.0f * atmosphere.params.x * height + height * height));
    } else{
        // Ray goes only through atmosphere
        atmosphereDepth = cos(psi) * toCenterOfPlanet + sqrt(cos2Psi * outerRadius * outerRadius + sin2Psi * (2.0f * outerRadius * toAtmospherePeak - toAtmospherePeak * toAtmospherePeak));
    }

    return atmosphereDepth;
}
vec4 skyColor(vec2 sphCoords){
    float height = globals.cameraPos.y;
    float atmosphereDepth = atmoDepth(globals.cameraPos.x, sphCoords.y);
    return vec4(inScatter(globals.cameraPos.y, sphCoords, atmosphereDepth), 1.0f);
}


SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = skyColor(sphericalCoords(rayDirection()));
    return ret;
}
