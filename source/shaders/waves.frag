#version 450
#define SHADOW_CASCADES 4
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inViewPos;
layout (location = 4) in vec4 inLightDir;
layout (location = 5) in vec4 inWorldPos;
layout (location = 6) in vec2 inGridPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 0,binding = 0) uniform Globals{
    mat4 perspective;
    mat4 cameraSpace;
    vec4 lightDir;
    vec4 skyColor;
    vec4 lightColor;
} globals;

layout (set = 1, binding = 0) uniform Waves{
    vec4 waves[4];
    vec4 deepWaterColor;
    float time;
}waves;


layout (push_constant) uniform PushConstants {
    vec2 translate;
    float scale;
    float waveEnable[4];
} pushConstants;


void main(){

    vec3 normal = normalize( inNormal );
    vec4 cameraDir = inWorldPos - inViewPos;

    float fog = clamp(length(cameraDir) / 1000.0f, 0.0f , 1.0f);
    float diffuse = dot(inLightDir, vec4(normal, 0.0f));

    vec4 reflectDir = vec4(reflect(normalize(cameraDir).xyz, normal), 0.0f);
    float angle = clamp(dot(normalize(cameraDir).xyz, -normal), 0.0f, 1.0f);
    angle = pow(angle, 1.0f / 2.0f);
    vec4 baseColor = waves.deepWaterColor * angle + globals.skyColor * (1.0f - angle);

    float reflect = clamp(dot(normalize(reflectDir), normalize(inLightDir)), 0.05f, 1.0f);
    reflect -= 0.05f;
    reflect = pow(reflect, 32.0f) * 2.2f;

    diffuse = (diffuse + 1.0f) / 2.0f;
    diffuse = diffuse * 0.4f + 0.4f;
    outFragColor = baseColor;
    outFragColor *= diffuse;
    outFragColor += reflect * globals.lightColor;
    //fog = 0.0f;
    outFragColor = outFragColor * (1.0f - fog) + globals.skyColor * fog;

}