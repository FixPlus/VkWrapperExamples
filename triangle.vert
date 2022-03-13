#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 inUV;

layout (location = 4) in mat4 inModel;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec4 outShadowPos;
layout (location = 4) out vec4 outLightDir;

layout (binding = 0) uniform MaBoi{
    mat4 perspective;
    vec4 lightDir;
} maBoi;

layout (binding = 3) uniform ShadowSpace{
    mat4 perspective;
} shadowSpace;

void main()
{

    outColor = col;
    outUV = inUV;
    outLightDir = maBoi.lightDir;
    outNormal = (inModel * vec4(normal, 0.0f)).xyz;
    vec4 position = vec4(inModel * vec4( pos , 1.0));
    outShadowPos = shadowSpace.perspective * position;
    gl_Position = maBoi.perspective * position;
}
