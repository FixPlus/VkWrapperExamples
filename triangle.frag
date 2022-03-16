#version 450
#define SHADOW_CASCADES 4
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inViewPos;
layout (location = 4) in vec4 inLightDir;
layout (location = 5) in vec4 inWorldPos;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D colorMap;
layout (binding = 2) uniform sampler2DArray shadowMaps;

layout (binding = 3) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;

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
            shadow = 0.3;
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
    int range = 1;

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
void main(){

    // Get cascade index for the current fragment's view position
    uint cascadeIndex = 0;
    for(uint i = 0; i < SHADOW_CASCADES - 1; ++i) {
        if(inViewPos.z < shadowSpace.splits[i]) {
            cascadeIndex = i + 1;
        }
    }

    // Depth compare for shadowing
    vec4 shadowCoord = (biasMat * shadowSpace.cascades[cascadeIndex]) * inWorldPos;


    float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);

    float diffuse = (dot(-inLightDir, vec4(inNormal, 0.0f)) + 1.0f) * 0.5f;
    diffuse = diffuse * 0.4f + 0.4f;
    outFragColor = vec4(inColor, 1.0f) * texture(colorMap, inUV) * diffuse * shadow;

}