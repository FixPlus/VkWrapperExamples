#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 inUV;

layout (location = 4) in mat4 inModel;


layout (binding = 0) uniform ShadowSpace{
    mat4 perspective;
} shadowSpace;

void main()
{
    vec4 position = vec4(inModel * vec4( pos , 1.0));
    gl_Position = shadowSpace.perspective * position;
}
