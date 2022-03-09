#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 inUV;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;
layout (binding = 0) uniform MaBoi{
    mat4 displacement;
    mat4 perspective;
} maBoi;
void main()
{

    outColor = col;
    outUV = inUV;
    outNormal = (maBoi.displacement * vec4(normal, 0.0f)).xyz;
    gl_Position = maBoi.perspective * maBoi.displacement * vec4( pos , 1.0);
}
