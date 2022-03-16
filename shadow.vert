#version 450

#define SHADOW_CASCADES 4

layout (location = 0) in vec3 pos;

layout (location = 4) in mat4 inModel;


layout (binding = 0) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;

layout(push_constant) uniform PushConsts {
    uint cascadeIndex;
} pushConsts;

void main()
{
    vec4 position = vec4(inModel * vec4( pos , 1.0));
    gl_Position = shadowSpace.cascades[pushConsts.cascadeIndex] * position;
}
