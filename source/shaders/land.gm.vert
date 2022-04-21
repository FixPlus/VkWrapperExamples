#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"



layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform Land{
    vec4 params;
    int harmonics;
}land;

layout (set = 1, binding = 0) uniform Camera{
    mat4 perspective;
    mat4 cameraSpace;
} camera;

layout (push_constant) uniform PushConstants {
    vec2 translate;
    float scale;
    float cellSize;
} pushConstants;

/* Function to linearly interpolate between a0 and a1
 * Weight w should be in the range [0.0, 1.0]
 */
float interpolate(float a0, float a1, float w) {
    /* // You may want clamping by inserting:
     * if (0.0 > w) return a0;
     * if (1.0 < w) return a1;
     */
    return (a1 - a0) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + a0;//(a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
    /* // Use this cubic interpolation [[Smoothstep]] instead, for a smooth appearance:
     * return (a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
     *
     * // Use [[Smootherstep]] for an even smoother result with a second derivative equal to zero on boundaries:
     * return (a1 - a0) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + a0;
     */
}

/* Create pseudorandom direction vector
 */
vec2 randomGradient(int ix, int iy) {
    // No precomputed gradients mean this works for any number of grid coordinates
    const uint w = 8 * 4;
    const uint s = w / 2; // rotation width
    uint a = ix, b = iy;
    a *= 3284157443; b ^= a << s | a >> w-s;
    b *= 1911520717; a ^= b << s | b >> w-s;
    a *= 2048419325;
    float random = a * (3.14159265 / ~(~0u >> 1)); // in [0, 2*Pi]
    vec2 v;
    v.x = cos(random); v.y = sin(random);
    return v;
}

// Computes the dot product of the distance and gradient vectors.
float dotGridGradient(int ix, int iy, float x, float y) {
    // Get gradient from integer coordinates
    vec2 gradient = randomGradient(ix, iy);

    // Compute the distance vector
    float dx = x - float(ix);
    float dy = y - float(iy);

    // Compute the dot-product
    return (dx*gradient.x + dy*gradient.y);
}

// Compute Perlin noise at coordinates x, y
float perlin(float x, float y) {
    // Determine grid cell coordinates
    int x0 = int(floor(x));
    int x1 = x0 + 1;
    int y0 = int(floor(y));
    int y1 = y0 + 1;

    // Determine interpolation weights
    // Could also use higher order polynomial/s-curve here
    float sx = x - float(x0);
    float sy = y - float(y0);

    // Interpolate between grid point gradients
    float n0, n1, ix0, ix1, value;

    n0 = dotGridGradient(x0, y0, x, y);
    n1 = dotGridGradient(x1, y0, x, y);
    ix0 = interpolate(n0, n1, sx);

    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    ix1 = interpolate(n0, n1, sx);

    value = interpolate(ix0, ix1, sy);

    return value;
}

float perlinHarmonics(float x, float y, int harmonics){
    float ret = 0.0f;
    float harm = 1.0f;
    for(int i = 0; i < harmonics; ++i){
        ret += perlin(x * harm + i * 13, y * harm + i * 19) / harm;
        harm *= 2.0f;
    }

    return ret / 2.0f;
}

float mutate(float x){
    if(x < -0.1f){
        return x - (x + 1.0f) / 0.9f * 0.4f;
    } else if (x < 0.1f){
        return x + x / 0.1f * 0.4f;
    } else{
        return x + (1.0f - (x - 0.1f) / 0.9f) * 0.4f;
    }
}

WorldVertexInfo Geometry(){
    vec2 localPos = inPos.xz * pushConstants.scale;

    vec2 translate = pushConstants.translate;
    vec2 gridPos = localPos + translate;

    float heightScale = land.params.x;
    float distanceScale = land.params.y;

    vec4 position = vec4(vec3(localPos.x, 0.0f,localPos.y) + vec3(translate.x, 0.0f, translate.y), 1.0f);

    float distance = length(camera.cameraSpace * position);

    int harmonics = land.harmonics;

    float currentElevation = mutate(perlinHarmonics(gridPos.x / distanceScale, gridPos.y / distanceScale, harmonics)) * heightScale;

    position += vec4(0.0f, currentElevation, 0.0f, 0.0f);

    float height = 0.0f;
    float deltaP = pushConstants.cellSize;
    vec3 tangent1 = normalize(vec3(deltaP, mutate(perlinHarmonics((gridPos.x + deltaP) / distanceScale, gridPos.y / distanceScale, harmonics)) * heightScale - currentElevation, 0.0f));
    vec3 binormal1 = normalize(vec3(0.0f, mutate(perlinHarmonics(gridPos.x / distanceScale, (gridPos.y + deltaP) / distanceScale, harmonics)) * heightScale - currentElevation, deltaP));
    vec3 tangent2 = normalize(vec3(-deltaP, mutate(perlinHarmonics((gridPos.x - deltaP) / distanceScale, gridPos.y / distanceScale, harmonics)) * heightScale - currentElevation, 0.0f));
    vec3 binormal2 = normalize(vec3(0.0f, mutate(perlinHarmonics(gridPos.x / distanceScale, (gridPos.y - deltaP) / distanceScale, harmonics)) * heightScale - currentElevation, -deltaP));

    vec3 normal = (normalize(cross( binormal1, tangent1)) + normalize(cross( binormal2, tangent2))) / 2.0f;

    vec2 uv = localPos / 1.0f;

    WorldVertexInfo ret;

    ret.position = vec3(position);
    ret.UVW = vec3(uv, 1.0f);
    ret.normal = normal;
    ret.tangent = normalize(tangent1);
    return ret;
}
