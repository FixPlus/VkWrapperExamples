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

    vec3 tanget = vec3(1.0f, 0.0, 0.0f);
    vec3 binormal = vec3(0.0f, 0.0f, 1.0f);


    // When geometry is too small to represent waves properly, do it in fragment shader then

    for(int i = 0; i < 4; ++i){
        if(pushConstants.waveEnable[i] != 0.0f)
            continue;
        float lambda = length(waves.waves[i].xy);
        if(lambda == 0.0f)
            continue;

        vec2 k = normalize(waves.waves[i].xy) * 2.0f * 3.1415f / lambda;

        float dx = waves.waves[i].x / lambda;
        float dy = waves.waves[i].y / lambda;

        float c = sqrt(9.8f /* gravitation */ / length(k));
        float f = dot(inGridPos, k) + waves.time * c;
        float steepness = waves.waves[i].w;
        float waveAmp = steepness / length(k);

        tanget += vec3(1.0f - dx * dx * steepness * sin(f), dx * steepness * cos(f), -dx * dy * steepness * sin(f));
        binormal += vec3(- dx * dy * steepness * sin(f), dy * steepness * cos(f), 1.0f - dy * dy * steepness * sin(f));

    }

    vec3 additionalNormal = cross(tanget, binormal);


    vec3 normal = normalize((length(additionalNormal) > 0.0f ? normalize(additionalNormal) : vec3(0.0)) + inNormal );

    float diffuse = dot(-inLightDir, vec4(normal, 0.0f));
    vec4 cameraDir = inWorldPos - inViewPos;
    vec4 reflectDir = vec4(reflect(normalize(cameraDir).xyz, normal), 0.0f);
    float angle = clamp(dot(normalize(cameraDir).xyz, normal), 0.0f, 1.0f);
    angle = pow(angle, 1.0f / 2.0f);
    vec4 baseColor = waves.deepWaterColor * angle + globals.skyColor * (1.0f - angle);

    float reflect = clamp(dot(normalize(reflectDir), -normalize(inLightDir)), 0.05f, 1.0f);
    reflect -= 0.05f;
    reflect = pow(reflect, 32.0f) * 2.2f;

    diffuse = (diffuse + 1.0f) / 2.0f;
    diffuse = diffuse * 0.4f + 0.4f;
    outFragColor = baseColor;
    outFragColor *= diffuse;
    outFragColor += reflect * globals.lightColor;

}