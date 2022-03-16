#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 outFragColor;

void main(){
    vec3 lightDir = normalize(vec3(1.0, 0.0, 0.5));
    float diffuse = (dot(lightDir, inNormal) + 1.0f) * 0.5f;
    diffuse = diffuse * 0.6f + 0.4f;
    outFragColor = vec4(inColor, 1.0f) * diffuse;
}