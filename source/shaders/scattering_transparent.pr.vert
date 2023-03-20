#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"

#define PI 3.1415926

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 outUVW;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outWorldNormal;
layout (location = 4) out vec3 outViewPos;
layout (location = 5) out vec3 outTangent;
layout (location = 6) out vec3 outRayleigh;
layout (location = 7) out vec3 outMei;

layout (set = 1, binding = 0) uniform Atmosphere{
    vec4 ScatterConstants; // xyz - rayleigh, w - mei
    vec4 properties; // x - planet radius, y - atmosphere radius, z - H0, w - g
    vec4 center;
    int samples;
} atmosphere;

layout (set = 1, binding = 1) uniform SunLight{
    vec4 color;
    vec4 pos; // x - phi, y -phi, z - intensity, w -distance
} sunlight;


layout (set = 1, binding = 2) uniform Camera{
    mat4 cameraSpace;
    mat4 perspective;
} camera;

layout(set = 1, binding = 3) uniform sampler2D outScatterTexture;

vec3 sunIntensity(){
    return sunlight.pos.z * sunlight.color.rgb * exp(-sunlight.pos.w / 1000.0f);
}

float phaseFunction(float Thetha, float g){
    float cosThetha = cos(Thetha);
    float gSquared = g * g;
    return 3.0f * (1.0f - gSquared) * (1.0f + cosThetha * cosThetha) / (2.0f * (2.0f + gSquared) * pow(1.0f + gSquared -  2.0f * g * cosThetha, 3.0f / 2.0f));
}

vec2 sphericalCoords(vec3 rayDir){

    vec2 planeProj = rayDir.xz;
    float psi = PI / 2.0f - asin(rayDir.y);
    float sinVal = planeProj.x / abs(sin(psi));
    if(abs(sinVal) > 1.0f)
    sinVal = sign(sinVal);
    float phi = asin(sinVal);
    if(planeProj.y < 0.0f)
    phi = sign(planeProj.x) * PI - phi;


    return vec2(phi, psi);
}

vec3 dir(vec2 spherical){
    float phi = spherical.x;
    float psi = spherical.y;
    return vec3(cos(psi) * sin(phi), sin(psi), cos(psi) * cos(phi));
}

vec4 sphereHit(vec3 rayOrig, vec3 dir, float sphereRadius){
    vec4 ret = vec4(0.0f);
    float rayDot = dot(rayOrig, dir);
    vec3 closestWay = rayOrig - dir * dot(rayOrig, dir);
    float cwlen = length(closestWay);
    if(cwlen > sphereRadius)
      return ret;

    float a  = sqrt(sphereRadius * sphereRadius - cwlen * cwlen);
    vec3 P1 = closestWay + dir * a;
    vec3 P2 = closestWay - dir * a;
    float dist1 = dot(P1 - rayOrig, dir);
    float dist2 = dot(P2 - rayOrig, dir);
    if(dist1 >= 0.0f && dist2 >= 0.0f)
      return vec4(rayOrig + min(dist1, dist2) * dir, 1.0f);
    else if(dist1 >= 0.0f)
      return vec4(rayOrig + dist1 * dir, 1.0f);
    else if(dist2 >= 0.0f)
        return vec4(rayOrig + dist2 * dir, 1.0f);
    else
        return ret;
}
struct Scatter{
    vec3 Rayleigh;
    vec3 Mei;
};

float psiAngle(vec3 From, vec3 To){
    return clamp(acos(dot(normalize(From), normalize(To - From))) / PI, 0.0f, 1.0f);
}
Scatter inScatter(vec3 pov, vec3 vertex){
    Scatter ret;
    ret.Rayleigh = vec3(0.0f);
    ret.Mei = vec3(0.0f);

    vec3 direction = normalize(vertex - pov);
    vec3 sunDir = dir(vec2(sunlight.pos.x, sunlight.pos.y));
    float Thetha = acos(dot(direction, sunDir));

    vec3 planetCenter = vec3(atmosphere.center);
    float outerRadius = atmosphere.properties.x + atmosphere.properties.y;
    vec3 startPoint;

    if(length(pov - planetCenter) > outerRadius){
        // Camera is out of atmosphere
        vec4 hit = sphereHit(pov - planetCenter, direction, outerRadius);
        if(hit.w != 1.0f)
          return ret;
        startPoint = vec3(hit);
    } else{
        // Camera is inside atmosphere
        startPoint = pov - planetCenter;
    }

    vec3 endPoint;

    if(length(vertex - planetCenter) > outerRadius){
        // vertex is out of atmosphere
        vec4 hit = sphereHit(vertex - planetCenter, -direction, outerRadius);
        if(hit.w != 1.0f)
          return ret;
        endPoint = vec3(hit);
    } else{
        // vertex is inside atmosphere
        endPoint = vertex - planetCenter;
    }

    endPoint = vertex - planetCenter;

    // Maybe we hit ground first
    #if 0
    vec4 groundHit = sphereHit(startPoint, direction, atmosphere.properties.x * 0.95f);
    if(groundHit.w != 0.0f){
        if(length(groundHit.xyz - startPoint) < length(endPoint - startPoint))
          endPoint = groundHit.xyz;
    }
#endif
    float rayLength = length(endPoint - startPoint);
    if(rayLength < 0.01f)
      return ret;

    float psi = psiAngle(startPoint, endPoint);
    bool psiLargerThatPI2 = psi > 0.5f;
    if(psiLargerThatPI2)
      psi = 1.0f - psi;

    vec3 sunI = sunIntensity();
    float distancePerStep = rayLength / (outerRadius * float(atmosphere.samples));
    float startH = clamp((length(startPoint) - atmosphere.properties.x) / atmosphere.properties.y, 0.001f, 0.999f);


    vec3 ReleighScatter = vec3(0.0f);
    vec3 MeiScatter = vec3(0.0f);

    vec4 precomputedStartScatter = texture(outScatterTexture, vec2(startH, psi));
    vec3 ray = endPoint - startPoint;

    for(int i = 0; i < atmosphere.samples; ++i){
        vec3 currentPoint = startPoint + ray * float(i) / float(atmosphere.samples);
        float currentSunPsi = acos(dot(normalize(currentPoint), -sunDir));
        vec4 sunDirHitGround = sphereHit(currentPoint, -sunDir, atmosphere.properties.x * 0.95f);
        if(sunDirHitGround.w == 1.0f)
          continue;
        float currentPsi = psiAngle(currentPoint, endPoint);
        if(psiLargerThatPI2)
          currentPsi = 1.0f - currentPsi;
        float h = clamp((length(currentPoint) - atmosphere.properties.x) / atmosphere.properties.y, 0.001f, 0.999f);
        vec4 precomputedSunScatter = texture(outScatterTexture, vec2(h, currentSunPsi / PI ));
        vec4 precomputedScatter = texture(outScatterTexture, vec2(h, currentPsi));


        float ReleighScatterFactor = precomputedSunScatter.y + (psiLargerThatPI2 ?  precomputedScatter.y - precomputedStartScatter.y : precomputedStartScatter.y - precomputedScatter.y);
        float MeiScatterFactor = precomputedSunScatter.w + (psiLargerThatPI2 ?  precomputedScatter.w - precomputedStartScatter.w : precomputedStartScatter.w - precomputedScatter.w);

        vec3 inscatter = exp(-4.0f * PI * vec3(atmosphere.ScatterConstants.xyz) * ReleighScatterFactor) * exp(-4.0f * PI * vec3(atmosphere.ScatterConstants.w) * MeiScatterFactor );
        ReleighScatter += distancePerStep * precomputedScatter.x * inscatter;
        MeiScatter += distancePerStep * precomputedScatter.z * inscatter;

    }

    ReleighScatter *=  sunI * vec3(atmosphere.ScatterConstants.xyz);
    MeiScatter *= sunI * vec3(atmosphere.ScatterConstants.w);
    ret.Rayleigh = ReleighScatter;
    ret.Mei = MeiScatter;
    return ret;
}

void Projection(WorldVertexInfo worldVertexInfo){

    vec3 cameraPos = vec3(inverse(camera.cameraSpace) * vec4(0.0f, 0.0f, 0.0f, 1.0f));

    outWorldNormal = worldVertexInfo.normal;
    outUVW = worldVertexInfo.UVW;
    Scatter scatter = inScatter(cameraPos,  worldVertexInfo.position.xyz);
    outRayleigh = scatter.Rayleigh;
    outMei = scatter.Mei;
    outColor = vec4( 1.0f);
    outWorldPos = worldVertexInfo.position;
    outViewPos = cameraPos;
    gl_Position = camera.perspective * camera.cameraSpace * vec4(worldVertexInfo.position, 1.0f);

}