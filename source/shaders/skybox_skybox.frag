#version 450


layout(location = 0) in vec2 uv;

layout (set = 2,binding = 0) uniform Globals{
    vec4 skyColor;
} globals;

layout (location = 0) out vec4 outFragColor;

void main() {
    outFragColor = globals.skyColor;
}
