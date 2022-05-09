#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"


layout (set = 3,binding = 0) uniform Sun{
    vec4 color;
    vec4 params; // x - phi, y - psi, z - distance
} sun;

layout (location = 0) out vec4 outFragColor;

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

    vec3 lightColor = sun.color.xyz;

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

    vec4 lightDir = vec4(sin(sun.params.x) * cos(sun.params.y), cos(sun.params.x) * cos(sun.params.y), sin(sun.params.y), 0.0f);
    vec3 normal = normalize( surfaceInfo.normal );
    vec3 N = normal;
    vec4 cameraDir = vec4(surfaceInfo.position - surfaceInfo.cameraOffset, 1.0f);
    vec3 V = normalize((-cameraDir).xyz);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, surfaceInfo.albedo.xyz, surfaceInfo.metallic);

    vec3 Lo = vec3(0.0);
    vec3 L = normalize(vec3(lightDir));
    Lo += specularContribution(L, V, N, F0, surfaceInfo.metallic, surfaceInfo.roughness, surfaceInfo.albedo.xyz);

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, surfaceInfo.roughness);

    float fog = 1.0f - exp(-length(cameraDir) / 1000.0f);
    float diffuseFactor =  dot(L, normal);
    diffuseFactor = (diffuseFactor + 1.0f) / 2.0f;
    diffuseFactor = diffuseFactor * 0.4f + 0.2f;
    vec3 diffuse = surfaceInfo.albedo.xyz * sun.color.xyz * diffuseFactor;
    vec4 reflectDir = vec4(reflect(normalize(cameraDir).xyz, normal), 0.0f);
    float reflect = clamp(dot(normalize(reflectDir), normalize(lightDir)), 0.05f, 1.0f);
    reflect -= 0.05f;
    reflect = pow(reflect, 32.0f) * 2.2f;
    vec3 specular = sun.color.xyz * F;
    // Ambient part
    vec3 kD = 1.0 - F;
    kD *= 1.0 - surfaceInfo.metallic;
    vec3 ambient = (kD * diffuse  + specular);

    vec3 color = ambient + Lo;

    outFragColor = vec4(color, surfaceInfo.albedo.a);

    outFragColor = outFragColor * (1.0f - fog) + vec4(1.0f) * fog;

}
