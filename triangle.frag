#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inShadowPos;
layout (location = 4) in vec4 inLightDir;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D colorMap;
layout (binding = 2) uniform sampler2D shadowMap;

float textureProj(vec4 shadowCoord, vec2 off)
{
    float shadow = 1.0;
    if(shadowCoord.x > 1.0f || shadowCoord.x < 0.0f ||
    shadowCoord.y > 1.0f || shadowCoord.y < 0.0f)
        return shadow;

    if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
        float dist = texture( shadowMap, shadowCoord.st + off ).r;
        if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
        {
            shadow = 0.1;
        }
    }
    return shadow;
}

float filterPCF(vec4 sc)
{
    ivec2 texDim = textureSize(shadowMap, 0);
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
            shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
            count++;
        }

    }
    return shadowFactor / count;
}
void main(){

    float shadow = filterPCF(inShadowPos / inShadowPos.w);

    float diffuse = (dot(-inLightDir, vec4(inNormal, 0.0f)) + 1.0f) * 0.5f;
    diffuse = diffuse * 0.4f + 0.4f;
    outFragColor = vec4(inColor, 1.0f) * texture(colorMap, inUV) * diffuse * shadow;

}