#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inShadowPos;
layout (location = 4) in vec4 inLightDir;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D colorMap;
layout (binding = 2) uniform sampler2D shadowMap;

float shadowProj(vec4 shadowCoord){
    float shadow = 1.0;
    if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
        float dist = texture( shadowMap, shadowCoord.st).r;
        if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
        {
            shadow = 0.1;
        }
    }
    return shadow;
}

void main(){

    float shadow = shadowProj(inShadowPos);

    float diffuse = (dot(inLightDir, vec4(inNormal, 0.0f)) + 1.0f) * 0.5f;
    diffuse = diffuse * 0.4f + 0.4f;
    outFragColor = vec4(inColor, 1.0f) * texture(colorMap, inUV) * diffuse * shadow;

}