#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"


layout (set = 3,binding = 0) uniform Sun{
    vec4 color;
    vec4 params; // x - phi, y - psi, z - distance
} sun;

layout (set = 3, binding = 1) uniform sampler2DArray shadowMaps;

#define SHADOW_CASCADES 4

layout (set = 3, binding = 2) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;

layout (set = 3, binding = 3) uniform Atmosphere{
    vec4 K; // K.xyz - scattering constants in Rayleigh scatter model for rgb chanells accrodingly, k.w - scattering constant for Mie scattering
    vec4 params; // x - planet radius, y - atmosphere radius, z - H0: atmosphere density factor, w - g: coef for Phase Function modeling Mie scattering
    int samples;
} atmosphere;

layout (location = 0) out vec4 outFragColor;

// SHADOW PROCESSING

const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0
);

float textureProj(vec4 shadowCoord, vec2 off, uint cascadeIndex)
{
    float shadow = 1.0;
    if(shadowCoord.x > 1.0f || shadowCoord.x < 0.0f ||
    shadowCoord.y > 1.0f || shadowCoord.y < 0.0f)
    return shadow;

    if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
        float dist = texture( shadowMaps, vec3(shadowCoord.st + off, cascadeIndex)).r;
        if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
        {
            //float shadowFactor =  1.0f - (shadowCoord.z - dist);
            shadow = 0.3f;//shadowFactor * 0.3f + (1.0f - shadowFactor) * 1.0f;
        }
    }
    return shadow;
}

float filterPCF(vec4 sc, uint cascadeIndex)
{
    ivec2 texDim = textureSize(shadowMaps, 0).xy;
    float scale = 1.0;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 3;

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
            count++;
        }

    }
    return shadowFactor / count;
}

float getShadowInPos(vec3 position, vec3 cameraPosition){
    uint cascadeIndex = 0;
    vec3 inViewPos = position - cameraPosition;

    for(uint i = 0; i < SHADOW_CASCADES - 1; ++i) {
        if(length(inViewPos) > shadowSpace.splits[i]) {
            cascadeIndex = i + 1;
        }
    }
    // Depth compare for shadowing
    vec4 shadowCoord = (biasMat * shadowSpace.cascades[cascadeIndex]) * vec4(position, 1.0f);
    return textureProj(shadowCoord / shadowCoord.w, vec2(0.0f), cascadeIndex);
}
float getShadow(SurfaceInfo surfaceInfo){
    // Get cascade index for the current fragment's view position
    uint cascadeIndex = 0;
    vec3 inViewPos = surfaceInfo.position - surfaceInfo.cameraOffset;

    for(uint i = 0; i < SHADOW_CASCADES - 1; ++i) {
        if(length(inViewPos) > shadowSpace.splits[i]) {
            cascadeIndex = i + 1;
        }
    }


    // Depth compare for shadowing
    vec4 shadowCoord = (biasMat * shadowSpace.cascades[cascadeIndex]) * vec4(surfaceInfo.position, 1.0f);


    float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
    return shadow;
}

#define PI 3.141592


    // Atmosphere processing

    #if 1
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

vec3 outScatteringTwoPoints(float height1, float height2, float distance){
    float samples = atmosphere.samples;
    float stepDistance = distance / atmosphere.params.y / samples;
    float stepHeight = (height2 - height1) / samples / atmosphere.params.y;
    vec3 ret = vec3(0);
    vec3 K = atmosphere.K.xyz;
    float H0 = atmosphere.params.z;
    for(int i = 0; i < samples; ++i){
        float currentHeight = height1 / atmosphere.params.y + i * stepHeight;
        ret += 4.0f * PI * K * exp(-currentHeight/ H0) * stepDistance;
    }

    return ret;
}
vec3 inScatter(float height, vec2 sphCoords, float distance, vec3 initPos, vec3 cameraPos){
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

    for(int i = 0; i < samples; ++i){
        float currentHeight = (height + i * cos(sphCoords.y) * stepPerSample) / atmosphere.params.y;
        float curHeightNonNorm = height + i * cos(sphCoords.y) * stepPerSample;
        vec3 currentPos = initPos + ray * i * stepPerSample;
        if(getShadowInPos(currentPos, cameraPos) > 0.5f)
            ret += exp(-currentHeight/H0)  * stepPerSample / atmosphere.params.y * exp(-outScattering(curHeightNonNorm, sun.params.y) - abs(outScatteringTwoPoints(height, curHeightNonNorm, i * stepPerSample)));
    }

    ret = sunIrradiance * ret * (atmosphere.K.xyz * phaseReleigh + atmosphere.K.w * phaseMie);

    return ret;

}
#endif

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, vec3 albedo, vec3 lightColor)
{
    // Precalculate vectors and dot products
    vec3 H = normalize (V + L);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);

    vec3 color = vec3(0.0);

    if (dotNL > 0.0) {
        // D = Normal distribution (Distribution of the microfacets)
        float D = D_GGX(dotNH, roughness);
        // G = Geometric shadowing term (Microfacets shadowing)
        float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
        // F = Fresnel factor (Reflectance depending on angle of incidence)
        vec3 F = F_Schlick(dotNV, F0);
        vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001) * lightColor;
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        color += (kD * albedo / PI + spec) * dotNL;
    }

    return color;
}

void Lighting(SurfaceInfo surfaceInfo){

    if(surfaceInfo.albedo.a == 0.0f)
        discard;

    float shadow = getShadow(surfaceInfo);

    bool hasShadow = shadow < 0.5f;

    vec3 sunIrradiance = hasShadow ? vec3(0.0f, 0.0f, 0.0f) : exp(-outScattering(surfaceInfo.position.y, sun.params.y)) * sun.color.xyz * sun.params.z;
    vec3 normal = normalize( surfaceInfo.normal );
    vec3 N = normal;
    vec4 cameraDir = vec4(surfaceInfo.position - surfaceInfo.cameraOffset, 1.0f);
    vec3 V = normalize((-cameraDir).xyz);

    vec3 F0 = vec3(0.0);
    F0 = mix(F0, surfaceInfo.albedo.xyz, surfaceInfo.metallic);

    vec3 Lo = vec3(0.0);
    vec3 L = normalize(vec3(sin(sun.params.x) * sin(sun.params.y), cos(sun.params.y), cos(sun.params.x) * sin(sun.params.y)));
    //Lo += specularContribution(L, V, N, F0, surfaceInfo.metallic, surfaceInfo.roughness, surfaceInfo.albedo.xyz, sunIrradiance);

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, surfaceInfo.roughness);

    vec4 reflectDir = normalize(vec4(reflect(normalize(-cameraDir).xyz, normal), 0.0f));
    float fragmentHeight = surfaceInfo.position.y;
    vec2 reflectSpherical = sphericalCoords(vec3(reflectDir.x, -reflectDir.y, reflectDir.z));
    vec3 groundColor = vec3(0.3f, 0.3f, 0.3f);
    vec3 ambientColor = inScatter(fragmentHeight, reflectSpherical, (atmosphere.params.y - fragmentHeight) * (1.0f + reflectSpherical.y), surfaceInfo.position, surfaceInfo.cameraOffset);// + 0.2f * clamp(sunIrradiance, 0.0f, 1.0f);
    float transit = clamp((reflectSpherical.y - PI / 4.0f) / (PI / 4.0f), 0.0f, 1.0f);
    ambientColor = (1.0f - transit) * ambientColor + groundColor * transit;
    vec3 diffuse = surfaceInfo.albedo.xyz * (clamp(ambientColor, 0.0f, 0.3f) + clamp(sunIrradiance * clamp(dot(L, N), 0.0f, 1.0f), 0.0f, 0.4f));

    #if 0
    float sunRef = clamp(dot(normalize(reflectDir), L), 0.05f, 1.0f);
    sunRef -= 0.05f;
    sunRef = pow(sun, 32.0f) * 2.2f;

    reflect += vec4(sunIrradiance, 1.0f) * sunRef;
    #endif
    vec3 specular = (sunIrradiance * pow(clamp(dot(L, -reflectDir.xyz), 0.0f, 1.0f), 64.0f) + clamp(ambientColor, 0.0f, 0.3f)) * F;
    //specular *= 0.0f;
    // Ambient part
    vec3 kD = 1.0 - F;
    kD *= 1.0 - surfaceInfo.metallic;
    vec3 ambient = kD * diffuse  + specular;

    //if(hasShadow)
        Lo *= 0.0f;
    vec3 color = ambient + Lo;

    outFragColor = vec4(color, surfaceInfo.albedo.a);

    float cameraHeight = surfaceInfo.cameraOffset.y;


    vec2 sphereCoords = sphericalCoords(vec3(V.x, -V.y, V.z));
    outFragColor = vec4(outFragColor.xyz * exp(-abs(outScatteringTwoPoints(cameraHeight, fragmentHeight, sphereCoords.y))) , surfaceInfo.albedo.a);
    outFragColor += vec4(inScatter(cameraHeight, sphereCoords, length(cameraDir), surfaceInfo.cameraOffset, surfaceInfo.cameraOffset), 0.0f);


}
