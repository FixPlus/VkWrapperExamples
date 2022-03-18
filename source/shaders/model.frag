#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;


layout (set = 2, binding = 0) uniform sampler2D colorMap;


layout (location = 0) out vec4 outFragColor;

void main() {
    vec4 outColor = texture(colorMap, inUV);
    vec3 lightDir = normalize(vec3(0.0, 0.5, -1.0));
    float diffuse = (dot(lightDir, inNormal) + 1.0f) / 2.0f;

    outFragColor = outColor * diffuse;
}
