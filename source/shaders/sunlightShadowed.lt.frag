#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

#define SHADOW_CASCADES 4

layout (set = 3, binding = 0) uniform sampler2DArray shadowMaps;

layout (set = 3, binding = 1) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;

layout (set = 3,binding = 2) uniform Globals{
    vec4 lightDir;
    vec4 skyColor;
    vec4 lightColor;
} globals;

const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0
);

layout (location = 0) out vec4 outFragColor;

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


void Lighting(SurfaceInfo surfaceInfo){
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

    float diffuse = dot(globals.lightDir, vec4(surfaceInfo.normal, 0.0f));

    // Self-Shadow artifacts elimination

    if(diffuse < 0.3f && diffuse > 0.0f){
        float factor = diffuse / 0.3f;
        shadow = 0.3f * (1.0f - factor) +  shadow * factor;
    }
    if(diffuse < 0.0f)
    shadow = 0.3f;

    diffuse = (diffuse + 1.0f) / 2.0f;
    diffuse = diffuse * 0.4f + 0.4f;
    outFragColor = surfaceInfo.albedo;
    outFragColor *= diffuse * shadow;

    #if 0
    switch(cascadeIndex) {
        case 0 :
        outFragColor.rgb *= vec3(1.0f, 0.5f, 0.5f);
        break;
        case 1 :
        outFragColor.rgb *= vec3(0.5f, 1.0f, 0.5f);
        break;
        case 2 :
        outFragColor.rgb *= vec3(0.5f, 0.5f, 1.0f);
        break;
        case 3 :
        outFragColor.rgb *= vec3(1.0f, 1.00f, 0.5f);
        break;
    }
     #endif

}