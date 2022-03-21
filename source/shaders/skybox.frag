#version 450


layout(location = 0) in vec2 uv;

layout (set = 0,binding = 0) uniform Globals{
    mat4 perspective;
    mat4 cameraSpace;
    vec4 lightDir;
    vec4 skyColor;
    vec4 lightColor;
} globals;

layout (location = 0) out vec4 outFragColor;

void main() {
    outFragColor = globals.skyColor;
}
