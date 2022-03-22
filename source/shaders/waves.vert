#version 450

#define NET_SIZE 10.0f
#define CELL_SIZE 0.1f

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec4 outViewPos;
layout (location = 4) out vec4 outLightDir;
layout (location = 5) out vec4 outWorldPos;
layout (location = 6) out vec2 outGridPos;

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

void main()
{

    vec2 localPos = inPos.xz * pushConstants.scale;

    vec2 translate = pushConstants.translate;
    vec2 gridPos = localPos + translate;
    outGridPos = gridPos;
    vec4 position = vec4(vec3(localPos.x, 0.0f,localPos.y) + vec3(translate.x, 0.0f, translate.y), 1.0f);
    float height = 0.0f;
    vec3 tangent = vec3(1.0f, 0.0, 0.0f);
    vec3 binormal = vec3(0.0f, 0.0f, 1.0f);



    for(int i = 0; i < 4; ++i){
        float lambda = length(waves.waves[i].xy);
        if(lambda == 0.0f)
            continue;

        vec2 k = normalize(waves.waves[i].xy) * 2.0f * 3.1415f / lambda;

        float dx = waves.waves[i].x / lambda;
        float dy = waves.waves[i].y / lambda;

        float c = sqrt(9.8f /* gravitation */ / length(k));
        float f = dot(gridPos, k) + waves.time * c;
        float steepness = waves.waves[i].w * pushConstants.waveEnable[i];
        float waveAmp = steepness / length(k);


        position += vec4(cos(f) * dx, sin(f), cos(f) * dy, 0.0f) * waveAmp;

        tangent += vec3(- dx * dx * steepness * sin(f), dx * steepness * cos(f), -dx * dy * steepness * sin(f));
        binormal += vec3(- dx * dy * steepness * sin(f), dy * steepness * cos(f), - dy * dy * steepness * sin(f));

    }

    vec3 normal = normalize(cross( binormal, tangent));

    vec2 uv = localPos / NET_SIZE;

    #if 0


    float height = texture(waveMap, uv).r;
    float cellUVSize = CELL_SIZE / NET_SIZE;

    float heightXP = texture(waveMap, uv + vec2(cellUVSize, 0)).r;
    float heightXM = texture(waveMap, uv + vec2(-cellUVSize, 0)).r;
    float heightYM = texture(waveMap, uv + vec2(0, -cellUVSize)).r;
    float heightYP = texture(waveMap, uv + vec2(0, cellUVSize)).r;

    float sinXP = (heightXP - height) / cellUVSize;
    float sinXM = (heightXM - height) / cellUVSize;
    float sinYP = (heightYP - height) / cellUVSize;
    float sinYM = (heightYM - height) / cellUVSize;

    float deltaX = sinXP - sinXM;
    float deltaY = sinYP - sinYM;

    deltaY = deltaX = 0.0f;
    height = 0.0f;
    #endif

    outColor = vec3(0.0f, 0.1f, 0.7f);

    outUV = uv;
    outLightDir = globals.lightDir;
    outNormal = normal;
    outWorldPos = position;
    outViewPos = inverse(globals.cameraSpace) * vec4(0.0f, 0.0f, 0.0f, 1.0f);
    gl_Position = globals.perspective * globals.cameraSpace * position;
}
