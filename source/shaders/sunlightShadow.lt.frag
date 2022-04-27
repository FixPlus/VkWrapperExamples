#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"


layout (set = 3,binding = 0) uniform Globals{
    vec4 lightDir;
    vec4 skyColor;
    vec4 lightColor;
    float fogginess;
} globals;

layout (set = 3, binding = 1) uniform sampler2DArray shadowMaps;

#define SHADOW_CASCADES 4

layout (set = 3, binding = 2) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;

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
    float scale = 1.5;
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
#if 0
    float diffuse = dot(globals.lightDir, vec4(surfaceInfo.normal, 0.0f));

    // Self-Shadow artifacts elimination

    if(diffuse < 0.3f && diffuse > 0.0f){
        float factor = diffuse / 0.3f;
        shadow = 0.3f * (1.0f - factor) +  shadow * factor;
    }
    if(diffuse < 0.0f)
    shadow = 0.3f;
#endif
    return shadow;
}

#define PI 3.141592

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

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, vec3 albedo)
{
    // Precalculate vectors and dot products
    vec3 H = normalize (V + L);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);

    vec3 lightColor = globals.lightColor.xyz;

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

    vec3 normal = normalize( surfaceInfo.normal );
    vec3 N = normal;
    vec4 cameraDir = vec4(surfaceInfo.position - surfaceInfo.cameraOffset, 1.0f);
    vec3 V = normalize((-cameraDir).xyz);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, surfaceInfo.albedo.xyz, surfaceInfo.metallic);

    vec3 Lo = vec3(0.0);
    vec3 L = normalize(vec3(globals.lightDir));
    Lo += specularContribution(L, V, N, F0, surfaceInfo.metallic, surfaceInfo.roughness, surfaceInfo.albedo.xyz);

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, surfaceInfo.roughness);

    float fog = 1.0f - exp(-length(cameraDir) / globals.fogginess);
    float diffuseFactor =  dot(L, normal);
    diffuseFactor = (diffuseFactor + 1.0f) / 2.0f;
    diffuseFactor = diffuseFactor * 0.4f + 0.2f;
    vec3 diffuse = surfaceInfo.albedo.xyz * globals.lightColor.xyz * diffuseFactor;
    vec4 reflectDir = vec4(reflect(normalize(cameraDir).xyz, normal), 0.0f);
    float reflect = clamp(dot(normalize(reflectDir), normalize(globals.lightDir)), 0.05f, 1.0f);
    reflect -= 0.05f;
    reflect = pow(reflect, 32.0f) * 2.2f;
    vec3 specular = globals.skyColor.xyz * F;//globals.skyColor.xyz * reflect * (1.0f - surfaceInfo.roughness);
    // Ambient part
    vec3 kD = 1.0 - F;
    kD *= 1.0 - surfaceInfo.metallic;
    vec3 ambient = (kD * diffuse * shadow  + specular);

    if(hasShadow)
        Lo *= 0.0f;
    vec3 color = ambient + Lo;



#if 0
    float angle = clamp(dot(normalize(cameraDir).xyz, -normal), 0.0f, 1.0f);
    angle = pow(angle, 1.0f / 2.0f);
    vec4 baseColor = surfaceInfo.albedo * angle * shadow + globals.skyColor * (1.0f - angle);
    outFragColor = baseColor;
    //outFragColor *= diffuse;
    outFragColor += reflect * globals.lightColor * shadow;
    //fog = 0.0f;
#endif
    outFragColor = vec4(color, surfaceInfo.albedo.a);
    outFragColor = outFragColor * (1.0f - fog) + globals.skyColor * fog;

}
