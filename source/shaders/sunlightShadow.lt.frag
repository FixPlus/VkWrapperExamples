#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"


layout (set = 3,binding = 0) uniform Sun{
    vec4 color;
    vec4 params; // x - phi, y - psi, z - distance
} sun;

layout (set = 3, binding = 1) uniform sampler2DArray shadowMaps;

#define SHADOW_CASCADES 4

layout (set = 3, binding = 2) uniform ShadowSpace{
    mat4 cascades[SHADOW_CASCADES];
    float splits[SHADOW_CASCADES];
} shadowSpace;

layout (set = 3, binding = 3) uniform Atmosphere{
    vec4 K; // K.xyz - scattering constants in Rayleigh scatter model for rgb chanells accrodingly, k.w - scattering constant for Mie scattering
    vec4 params; // x - planet radius, y - atmosphere radius, z - H0: atmosphere density factor, w - g: coef for Phase Function modeling Mie scattering
    int samples;
} atmosphere;

layout(set = 3, binding = 4) uniform sampler2D outScatterTexture;

layout (location = 0) out vec4 outFragColor;

// SHADOW PROCESSING

const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0
);

float textureProj(vec4 shadowCoord, vec2 off, uint cascadeIndex)
{
    float shadow = 1.0;
    if(shadowCoord.x > 1.0f || shadowCoord.x < 0.0f ||
    shadowCoord.y > 1.0f || shadowCoord.y < 0.0f)
    return shadow;

    if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
        float dist = texture( shadowMaps, vec3(shadowCoord.st + off, cascadeIndex)).r;
        if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
        {
            //float shadowFactor =  1.0f - (shadowCoord.z - dist);
            shadow = 0.3f;//shadowFactor * 0.3f + (1.0f - shadowFactor) * 1.0f;
        }
    }
    return shadow;
}

float filterPCF(vec4 sc, uint cascadeIndex)
{
    ivec2 texDim = textureSize(shadowMaps, 0).xy;
    float scale = 1.0;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 3;

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
            count++;
        }

    }
    return shadowFactor / count;
}

float getShadowInPos(vec3 position, vec3 cameraPosition){
    uint cascadeIndex = 0;
    vec3 inViewPos = position - cameraPosition;

    for(uint i = 0; i < SHADOW_CASCADES - 1; ++i) {
        if(length(inViewPos) > shadowSpace.splits[i]) {
            cascadeIndex = i + 1;
        }
    }
    // Depth compare for shadowing
    vec4 shadowCoord = (biasMat * shadowSpace.cascades[cascadeIndex]) * vec4(position, 1.0f);
    return textureProj(shadowCoord / shadowCoord.w, vec2(0.0f), cascadeIndex);
}
float getShadow(SurfaceInfo surfaceInfo){
    // Get cascade index for the current fragment's view position
    uint cascadeIndex = 0;
    vec3 inViewPos = surfaceInfo.position - surfaceInfo.cameraOffset;

    for(uint i = 0; i < SHADOW_CASCADES - 1; ++i) {
        if(length(inViewPos) > shadowSpace.splits[i]) {
            cascadeIndex = i + 1;
        }
    }


    // Depth compare for shadowing
    vec4 shadowCoord = (biasMat * shadowSpace.cascades[cascadeIndex]) * vec4(surfaceInfo.position, 1.0f);


    float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
    return shadow;
}

#define PI 3.141592

vec2 scatterTexCoords(float height, float psi){
    return vec2((height + 0.1f * atmosphere.params.y) / (1.1f * atmosphere.params.y), psi / PI);
}

vec4 outScatterDirection(float initialHeight, float psi, float distance){
    float toCenterOfPlanet = initialHeight + atmosphere.params.x;
    float toCenterOfPlanet2 = toCenterOfPlanet * toCenterOfPlanet;
    float destinationHeight  = sqrt(toCenterOfPlanet2 + distance * distance - 2.0f * cos(PI - psi) * toCenterOfPlanet * distance) - atmosphere.params.x;

    float destPsi = acos((-toCenterOfPlanet2 + distance * distance + (atmosphere.params.x + destinationHeight) * (atmosphere.params.x + destinationHeight))/(2.0f * distance * (atmosphere.params.x + destinationHeight)));

    return exp(-abs(vec4(texture(outScatterTexture, scatterTexCoords(initialHeight, (psi > PI / 2.0f ? PI -  psi :  psi) ))) -
    vec4(texture(outScatterTexture, scatterTexCoords(destinationHeight, (psi > PI / 2.0f ? PI -  destPsi : destPsi) )))));
}
    // Atmosphere processing

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

float phaseFunction(float psi, float g){
    //float g = atmosphere.params.w;
    float cosPsi = cos(psi);
    return (3.0f * (1 - g * g) / (2.0f * (2.0f + g * g))) * (1 + cosPsi * cosPsi) / pow(1 + g * g - 2.0f * g * cosPsi, 3.0f / 2.0f);
}




float atmoDepth(float height, float psi){
    psi = PI -psi;
    float toCenterOfPlanet = height + atmosphere.params.x;
    float toAtmospherePeak = atmosphere.params.y - height;
    float outerRadius = atmosphere.params.y + atmosphere.params.x;
    float atmosphereDepth;
    float cos2Psi = cos(psi) * cos(psi);
    float sin2Psi = 1.0f - cos2Psi;

    if(psi < asin(atmosphere.params.x / (toCenterOfPlanet)) - 0.01f){
        // Ray intersect with planet
        atmosphereDepth = cos(psi) * (toCenterOfPlanet) - sqrt(cos2Psi * atmosphere.params.x * atmosphere.params.x - sin2Psi * (2.0f * atmosphere.params.x * height + height * height));
    } else{
        // Ray goes only through atmosphere
        atmosphereDepth = cos(psi) * toCenterOfPlanet + sqrt(cos2Psi * outerRadius * outerRadius + sin2Psi * (2.0f * outerRadius * toAtmospherePeak - toAtmospherePeak * toAtmospherePeak));
    }

    return atmosphereDepth;
}


vec3 inScatter(float height, vec2 sphCoords, float distance, vec3 initPos, vec3 cameraPos){

    vec4 ret = vec4(0.0f);

    float samples = atmosphere.samples;
    float stepPerSample = distance / samples;
    vec3 ray = vec3(sin(sphCoords.x) * sin(sphCoords.y), cos(sphCoords.y), cos(sphCoords.x) * sin(sphCoords.y));
    vec3 sunDir = vec3(sin(sun.params.x) * sin(sun.params.y), cos(sun.params.y), cos(sun.params.x) * sin(sun.params.y));
    ray.y *= -1.0f;
    float sunPsi = asin(length(cross(sunDir, ray)));

    if(dot(sunDir, ray) < 0.0f){
        sunPsi = sign(sunPsi) * PI - sunPsi;
    }
    float phaseMie = phaseFunction(sunPsi, atmosphere.params.w);
    float phaseReleigh = phaseFunction(sunPsi, 0.0f);
    float H0 = atmosphere.params.z;

    vec3 sunIrradiance = sun.color.xyz * sun.params.z;
    float angle = ray.y > 0.0f ? sphCoords.y : PI - sphCoords.y;
    float toCenterOfPlanet = height + atmosphere.params.x;
    float toCenterOfPlanet2 = toCenterOfPlanet * toCenterOfPlanet;

    for(int i = 0; i < samples; ++i){
        float accDist = stepPerSample * i;
        float curHeightNonNorm  = sqrt(toCenterOfPlanet2 + accDist * accDist - 2.0f * cos(PI - sphCoords.y) * toCenterOfPlanet * accDist) - atmosphere.params.x;
        float currentHeight = curHeightNonNorm / atmosphere.params.y;

        float curPsi = acos((-toCenterOfPlanet2 + accDist * accDist + (atmosphere.params.x + curHeightNonNorm) * (atmosphere.params.x + curHeightNonNorm))/(2.0f * accDist * (atmosphere.params.x + curHeightNonNorm)));

        vec3 currentPos = initPos + ray * i * stepPerSample;

        if(getShadowInPos(currentPos, cameraPos) > 0.5f)
            ret += (curHeightNonNorm < -0.1f * atmosphere.params.y || PI - sun.params.y < asin(atmosphere.params.x / (atmosphere.params.x + curHeightNonNorm)) - 0.01f ? 0.0f: 1.0f) *
            exp(-currentHeight/H0)  * stepPerSample / atmosphere.params.y *
            exp(-vec4(texture(outScatterTexture, scatterTexCoords(curHeightNonNorm, sun.params.y))) -
            abs(vec4(texture(outScatterTexture, scatterTexCoords(height, (sphCoords.y > PI / 2.0f ? PI -  sphCoords.y :  sphCoords.y) ))) -
            vec4(texture(outScatterTexture, scatterTexCoords(curHeightNonNorm, (sphCoords.y > PI / 2.0f ? PI -  curPsi : curPsi) )))));

    }


    return sunIrradiance * (atmosphere.K.xyz * ret.xyz * phaseReleigh + vec3(atmosphere.K.w * ret.w * phaseMie));

}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void Lighting(SurfaceInfo surfaceInfo){

    if(surfaceInfo.albedo.a == 0.0f)
        discard;

    float shadow = getShadow(surfaceInfo);

    bool hasShadow = shadow < 0.5f;

    vec3 sunIrradiance = hasShadow ? vec3(0.0f, 0.0f, 0.0f) :
    (surfaceInfo.position.y < -0.1f * atmosphere.params.y || PI - sun.params.y < asin(atmosphere.params.x / (atmosphere.params.x + surfaceInfo.position.y)) - 0.01f ? 0.0f: 1.0f)
     * exp(-texture(outScatterTexture, scatterTexCoords(surfaceInfo.position.y, sun.params.y)).rgb) * sun.color.xyz * sun.params.z;
    vec3 normal = normalize( surfaceInfo.normal );
    vec3 N = normal;
    vec4 cameraDir = vec4(surfaceInfo.position - surfaceInfo.cameraOffset, 1.0f);
    vec3 V = normalize((-cameraDir).xyz);

    vec3 F0 = vec3(0.0);
    F0 = mix(F0, surfaceInfo.albedo.xyz, surfaceInfo.metallic);

    vec3 L = normalize(vec3(sin(sun.params.x) * sin(sun.params.y), cos(sun.params.y), cos(sun.params.x) * sin(sun.params.y)));

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, surfaceInfo.roughness);

    vec4 reflectDir = normalize(vec4(reflect(normalize(-cameraDir).xyz, normal), 0.0f));
    float fragmentHeight = surfaceInfo.position.y;
    vec2 reflectSpherical = sphericalCoords(vec3(reflectDir.x, -reflectDir.y, reflectDir.z));
    vec3 groundColor = vec3(0.3f, 0.3f, 0.3f);
    vec3 ambientColor = inScatter(fragmentHeight, reflectSpherical, atmoDepth(fragmentHeight, reflectSpherical.y), surfaceInfo.position, surfaceInfo.cameraOffset);
    float transit = clamp((reflectSpherical.y - PI / 4.0f) / (PI / 4.0f), 0.0f, 1.0f);
    ambientColor = (1.0f - transit) * ambientColor + groundColor * transit;
    vec3 diffuse = surfaceInfo.albedo.xyz * (clamp(ambientColor, 0.0f, 0.3f) + clamp(clamp(sunIrradiance, 0.0f, 1.0f) * clamp(dot(L, N), 0.0f, 1.0f), 0.0f, 0.4f));

    vec3 specular = (clamp(sunIrradiance, 0.0f, 1.0f) * pow(clamp(dot(L, -reflectDir.xyz), 0.0f, 1.0f), 128.0f) + 0.3f * clamp(ambientColor, 0.0f, 1.0f)) * F;

    // Ambient part
    vec3 kD = 1.0 - F;
    kD *= 1.0 - surfaceInfo.metallic;
    vec3 ambient = kD * diffuse + specular;
    vec3 color = ambient;

    outFragColor = vec4(color, surfaceInfo.albedo.a);

    float cameraHeight = surfaceInfo.cameraOffset.y;


    vec2 sphereCoords = sphericalCoords(vec3(V.x, -V.y, V.z));


    vec4 outScatter = outScatterDirection(surfaceInfo.position.y, PI - sphereCoords.y, length(cameraDir));
    outFragColor = vec4(outFragColor.xyz * outScatter.xyz * outScatter.w, surfaceInfo.albedo.a);
    outFragColor += vec4(inScatter(cameraHeight, sphereCoords, length(cameraDir), surfaceInfo.cameraOffset, surfaceInfo.cameraOffset), 0.0f);

}
