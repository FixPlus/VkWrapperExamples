#version 450

layout(location = 0) out vec2 uv;

void main() {
    vec4 outPos = vec4((gl_VertexIndex % 2) * 4.0f - 1.0f, ((gl_VertexIndex + 1) % 2) * (gl_VertexIndex / 2) * 4.0f - 1.0f, 0.0f, 1.0f);
    gl_Position = outPos;
    uv = vec2(outPos.xy);

}
