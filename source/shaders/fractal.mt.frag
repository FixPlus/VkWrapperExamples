#version 450
#extension GL_GOOGLE_include_directive : require
#include "MaterialLightingInterface.h.glsl"

layout (binding = 1, set = 2) uniform sampler2D samplerColor1;

layout (binding = 0, set = 2) uniform UBO
{
    mat4 invProjView;
    vec4 cameraPos;
    float near;
    float far;
    vec2 pad;
    vec4 lightPos;
    vec4 params;
    float shadowOption; // < 0 == shadow off ; >= 0 == shadow on
} ubo;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUVW;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inWorldNormal;
layout (location = 4) in vec3 inViewPos;

float FOV = 0.5f; //in radian
float epsilon = 0.0001f; // distance epsilon vincinity
float maxLength = 1000.0f;
int maxSteps = 250;

#define NUM_OF_MIRRORS 7
#define PI 3.141592f
vec4 mirrorRoots[NUM_OF_MIRRORS]   =   {vec4(0.0f, 0.0f, 3.0f, 0.0f), vec4(0.0f), vec4(1.5f, 0.0f, 0.0f, 0.0f),
vec4(0.0f, 1.5f, 0.0f, 0.0f), vec4(2.0f, 1.0f, 0.0f, 0.0f), vec4(0.0f),
vec4(1.0f, 0.0f, 0.0f, 0.0f)};
vec4 mirrorNormals[NUM_OF_MIRRORS] =   {vec4(-1.0f, 0.0f, -1.0f, 0.0f), vec4(1.0f, 0.0f, -1.0f, 0.0f), vec4(-1.0f, 0.0f, 0.0f, 0.0f),
vec4(0.0f, -1.0f, 0.0f, 0.0f), vec4(-1.0f, -1.0f, 0.0f, 0.0f), vec4(1.0f, -1.0f, 0.0f, 0.0f),
vec4(-1.0f, 0.0f, 0.0f, 0.0f)};

#define SPOUNGE_UNIT 1.0f
#define RANK 6.0f


#define SHAPE_CUBE_BALL_TORUS 1

vec4 BallOffset(vec4 pos) {
    float new_len = length(pos.xyz) - 0.6f;
    vec3 dir = normalize(pos.xyz);
    return vec4(dir, new_len);
}

vec4 CubeOffset(vec4 pos) {
    vec3 dir;
    if(abs(pos.x) >= abs(pos.y) && abs(pos.x) >= abs(pos.z))
    dir =  vec3(pos.x > 0.0f ? 1.0f: -1.0f, 0.0f, 0.0f);
    else if(abs(pos.y) >= abs(pos.x) && abs(pos.y) >= abs(pos.z))
    dir =  vec3(0.0f, pos.y > 0.0f ? 1.0f: -1.0f, 0.0f);
    else
    dir =  vec3(0.0f, 0.0f, pos.z > 0.0f ? 1.0f: -1.0f);
    float divX = abs(pos.x) - 0.5f;
    float divY = abs(pos.y) - 0.5f;
    float divZ = abs(pos.z) - 0.5f;
    float dist = 0.0f;
    if(divX >= 0.0f || divY >= 0.0f || divZ >= 0.0f)
        dist = sqrt(max(divX, 0.0f) * max(divX, 0.0f) + max(divY, 0.0f) * max(divY, 0.0f) + max(divZ, 0.0f) * max(divZ, 0.0f));
    else
        dist = max(divX, max(divY, divZ));
    return vec4(dir, dist);
}

vec4 TorusOffset(vec4 pos) {
    pos.w = 0.0f;
    vec4 ret;
    vec4 torusFaceNormal = vec4(1.0f, 0.0f, 0.0f, 0.0f);
    vec4 torusCenterDisposal = pos;
    vec4 torusRefVec = torusCenterDisposal - torusFaceNormal * dot(torusCenterDisposal, torusFaceNormal);
    if(length(torusRefVec) < 0.001f)
    return vec4(1.0f, 0.0f, 0.0f, (sqrt(0.375f * 0.375f + dot(torusCenterDisposal, torusFaceNormal) * dot(torusCenterDisposal, torusFaceNormal)) - 0.125f));
    else{
        torusRefVec = normalize(torusRefVec);
        ret = torusCenterDisposal - 0.375f * torusRefVec;
        return vec4(normalize(ret).xyz, max(0.0f, length(ret) - 0.125f));
    }
}

vec4 diff(vec4 Shape1, vec4 Shape2) {
  if(Shape1.w > -Shape2.w)
    return Shape1;
  else
    return -Shape2;
}

vec4 concat(vec4 Shape1, vec4 Shape2) {
    if(Shape1.w < Shape2.w){
        return Shape1;
    }
    else {
        return Shape2;
    }
}

vec4 ShapeOffset(vec4 pos){ //x y z for vector, a for length
    #ifdef SHAPE_BALL
    return BallOffset(pos);
    #elif SHAPE_CUBE
    return CubeOffset(pos);
    #elif SHAPE_CUBE_BALL
    return diff(CubeOffset(pos), BallOffset(pos));
    #elif SHAPE_CUBE_BALL_TORUS
    return concat(TorusOffset(pos), diff(CubeOffset(pos), BallOffset(pos)));
    #endif
}

float roundUp(float f){
    float ret = round(f);
    if(ret < f){
        ret += 1.0f;
    }
    return ret;
}
float roundDown(float f){
    float ret = round(f);
    if(ret > f){
        ret -= 1.0f;
    }
    return ret;
}

vec4 planeMirrorForced(vec4 point, vec4 planeRoot, vec4 planeNormal){
    planeNormal = normalize(planeNormal);

    vec4 div = point - planeRoot;
    float normProj = dot(planeNormal, div);
    point -= 2.0f * planeNormal *  normProj;

    return point;
}

int flagMirror = 0;

vec4 planeMirror(vec4 point, vec4 planeRoot, vec4 planeNormal){
    planeNormal = normalize(planeNormal);

    vec4 div = point - planeRoot;
    float normProj = dot(planeNormal, div);
    if(normProj < 0.0f){
        point -= 2.0f * planeNormal *  normProj;
        flagMirror = 1;
    }
    else
    flagMirror = 0;

    return point;
}

float closestDist(vec4 pos){

    for(int i = 0; i < RANK; i++){
        float curRank = RANK - i;
        float multiplier = pow(3, curRank - 1) * SPOUNGE_UNIT;
        for(int j = 0; j < NUM_OF_MIRRORS; j++){
            vec4 normal = mirrorNormals[j];
            if(j == 5)
            normal.x *= ubo.params.x;

            pos = planeMirror(pos, mirrorRoots[j] * multiplier, normal);
        }

    }

    float x = pos.x / SPOUNGE_UNIT;
    float y = pos.y / SPOUNGE_UNIT;
    float z = pos.z / SPOUNGE_UNIT;
    float divX = x > 1.0f ? (x - 1.0f) * SPOUNGE_UNIT : (x < 0.0f ? x * SPOUNGE_UNIT : 0.0f);
    float divY = y > 1.0f ? (y - 1.0f) * SPOUNGE_UNIT : (y < 0.0f ? y * SPOUNGE_UNIT : 0.0f);
    float divZ = z > 1.0f ? (z - 1.0f) * SPOUNGE_UNIT : (z < 0.0f ? z * SPOUNGE_UNIT : 0.0f);


    return ShapeOffset(pos / SPOUNGE_UNIT - vec4(0.5f, 0.5f, 0.5f, 0.0f)).a * SPOUNGE_UNIT;
}

vec4 closestColor(vec4 pos){
    vec4 ret = vec4(1.0f);

    for(int i = 0; i < RANK; i++){
        float curRank = RANK - i;
        float multiplier = pow(3, curRank - 1) * SPOUNGE_UNIT;
        for(int j = 0; j < NUM_OF_MIRRORS; j++){
            vec4 normal = mirrorNormals[j];
            if(j == 5)
            normal.x *= ubo.params.x;

            pos = planeMirror(pos, mirrorRoots[j] * multiplier, normal);
        }

        if(curRank == 4){
            float param = 0.85f + 0.2f * sin((pos.x / multiplier + pos.y / multiplier) * PI * 2.0f + 1.0f);
            ret.x *= param;
            //ret.z *= param;
        }

        if(curRank == 6){
            float param = 0.7f + 0.4f * sin(pos.x / multiplier * PI * 2.0f + 0.32f);
            ret.y *= param;
            ret.z *= param;

        }

        if(curRank == 2){
            float param = 0.9f + 0.1f * sin(pos.x / multiplier * 2.0f * PI + 1.75f);
            ret.y *= param;
            ret.x *= param;
        }


    }

    return ret;

}
vec2 closestTextureCoord(vec4 pos){
    for(int i = 0; i < RANK; i++){
        float curRank = RANK - i;
        float multiplier = pow(3, curRank - 1) * SPOUNGE_UNIT;
        for(int j = 0; j < NUM_OF_MIRRORS; j++){
            vec4 normal = mirrorNormals[j];
            if(j == 5)
            normal.x *= ubo.params.x;

            pos = planeMirror(pos, mirrorRoots[j] * multiplier, normal);
        }

    }
    float x = pos.x / SPOUNGE_UNIT - 0.5f;
    float y = pos.y / SPOUNGE_UNIT - 0.5f;
    float z = pos.z / SPOUNGE_UNIT - 0.5f;
    vec2 ret;
    if(abs(x) >= abs(y) && abs(x) >= abs(z) ){

        ret.x = y > 0.0f ? min(0.5f, y) : max(-0.5f, y);
        ret.x += 0.5f;

        ret.y = z > 0.0f ? min(0.5f, z) : max(-0.5f, z);
        ret.y += 0.5f;

    }
    else
    if(abs(y) >= abs(z)){
        ret.x = x > 0.0f ? min(0.5f, x) : max(-0.5f, x);
        ret.x += 0.5f;

        ret.y = z > 0.0f ? min(0.5f, z) : max(-0.5f, z);
        ret.y += 0.5f;

    }
    else{
        ret.x = x > 0.0f ? min(0.5f, x) : max(-0.5f, x);
        ret.x += 0.5f;

        ret.y = y > 0.0f ? min(0.5f, y) : max(-0.5f, y);
        ret.y += 0.5f;

    }

    return ret;

}

vec4 closestNormal(vec4 pos){

    vec4 xAxis = vec4(1.0f, 0.0f, 0.0f, 0.0f);
    vec4 yAxis = vec4(0.0f, 1.0f, 0.0f, 0.0f);
    vec4 zAxis = vec4(0.0f, 0.0f, 1.0f, 0.0f);
    vec4 nullVec = vec4(0.0f);
    for(int i = 0; i < RANK; i++){
        float curRank = RANK - i;
        float multiplier = pow(3, curRank - 1) * SPOUNGE_UNIT;
        for(int j = 0; j < NUM_OF_MIRRORS; j++){
            vec4 normal = mirrorNormals[j];
            if(j == 5)
            normal.x *= ubo.params.x;
            pos = planeMirror(pos, mirrorRoots[j] * multiplier, normal);
            if(flagMirror == 1){
                xAxis = planeMirrorForced(xAxis, nullVec, normal);
                yAxis = planeMirrorForced(yAxis, nullVec, normal);
                zAxis = planeMirrorForced(zAxis, nullVec, normal);
            }
        }
    }
    vec4 ret = vec4(ShapeOffset(pos / SPOUNGE_UNIT - vec4(0.5f, 0.5f, 0.5f, 0.0f)).xyz, 0.0f);

    vec4 retReal = vec4(dot(ret, xAxis), dot(ret, yAxis), dot(ret, zAxis), 0.0f);

    return retReal;

}

vec4 getMarchDirection(){
    vec4 dir = vec4((ubo.invProjView * vec4((2.0f * inUVW.xy - vec2(1.0f)) * (ubo.far - ubo.near), ubo.far + ubo.near, ubo.far - ubo.near)).xyz, 0.0f);
    dir = normalize(dir);
    return dir;
}

struct Hit{
    vec4 albedo;
    vec4 normal;
    vec4 hitPoint;
    float distance;
    int steps;
    bool hasHit;
};
float checkCloseAmbient(vec4 origin, vec4 direction, float distanceLimit) {
    int n = 0; //count of steps
    float totalRayLength = 0.0f; // counter for ray length
    float depth = 0.0f; // accum for depth


    float temp; // used to temproary contaion ED for each step
    vec4 prevPos = origin; // used to contain the prev value of curPos
    vec4 curPos = prevPos;
    vec4 dir = direction;

    float ambient = 0.0f;
    while((temp = closestDist(curPos)) > (totalRayLength < SPOUNGE_UNIT ? SPOUNGE_UNIT * epsilon : epsilon * totalRayLength) && totalRayLength < distanceLimit && n < maxSteps){

        prevPos = curPos;
        //if(log(length(curPos)) - log(temp) > 13.0f)
        //temp = pow(2.715, log(length(curPos)) - 13.0f) ;
        curPos += dir * temp * 0.995f;
        n++;
        totalRayLength += temp * 0.995f;
        ambient += temp / totalRayLength;
    }
    return ambient;
}

Hit march(vec4 origin, vec4 direction){
    int n = 0; //count of steps
    float totalRayLength = 0.0f; // counter for ray length
    float depth = 0.0f; // accum for depth


    float temp; // used to temproary contaion ED for each step
    float lastL; // used to contain the prev value of temp var
    vec4 prevPos = origin; // used to contain the prev value of curPos
    vec4 curPos = prevPos;
    vec4 dir = direction;

    /*
        Next cycle is representation of ray marching technic

        Procces goes till ED hits the epsilon treshhold (aka ray collides an object)

        or rayLength hits maxLength treshhold, so there are no objects in this direction

        or number of steps hits maxSteps treshhold, so it will be considered we hit complex structure
        of object that is poorly lit

        closestDist is DE function
    */

    while((temp = closestDist(curPos)) > (totalRayLength < SPOUNGE_UNIT ? SPOUNGE_UNIT * epsilon : epsilon * totalRayLength)  && totalRayLength < maxLength && n < maxSteps){
        prevPos = curPos;
        //if(log(length(curPos)) - log(temp) > 13.0f)
        //temp = pow(2.715, log(length(curPos)) - 13.0f) ;
        curPos += dir * temp * 0.995f;
        n++;
        totalRayLength += temp * 0.995f;
        lastL = temp* 0.99f;
    }

    Hit ret;
    ret.hasHit = totalRayLength < maxLength;
    ret.distance = totalRayLength;
    if(ret.hasHit){
        vec2 texCoords = closestTextureCoord(curPos);
        vec4 posColor = closestColor(curPos);
        float foggy = pow(totalRayLength, 3.0) / pow(maxLength, 3.0);
        float mip = foggy * 8.0f;
        ret.albedo = texture(samplerColor1, texCoords, mip);
        ret.distance = totalRayLength;
        ret.steps = n;
        ret.normal = closestNormal(curPos);
        ret.hitPoint = curPos;
    } else {
        ret.albedo = vec4(0.2f, 0.8f, 0.9f,1.0f);
    }

    return ret;
}

float occlusionFromSteps(int steps) {
    return 1.0f - pow(2.715, -35.0f/float(steps));
}
vec4 disnatceDiffuse(vec4 Light, float distance){
    vec4 fog = vec4(0.2f, 0.8f, 0.9f,1.0f);
    float foggy = pow(distance, 3.0) / pow(maxLength, 3.0);
    return foggy * fog + Light * (1.0f - foggy);
}

vec4 scatterColor(vec4 dir, vec4 DirectLight, float distance, float occlusionFactor){
    float sunDot = (1.0f + dot(vec3(dir), vec3(-normalize(ubo.lightPos)))) / 2.0f;
    float distanceFactor = min(1.0f, distance / maxLength);
    float sunInscatterFactor = pow(sunDot, 32.0f) * occlusionFactor * distanceFactor;
    float ambientInscatterFactor = distanceFactor;


    float outScatterFactor = distanceFactor;
    vec4 inscatteredLight = (vec4(2.0f) * sunInscatterFactor + vec4(0.2f, 0.8f, 0.9f,1.0f) * ambientInscatterFactor) * (1.0f - outScatterFactor / 3.0f);
    return inscatteredLight + DirectLight * outScatterFactor;
}

vec4 skyColor(vec4 dir){
    vec4 skyColor = vec4(0.2f, 0.8f, 0.9f,1.0f);
    float sun = pow(100,  10.0f * (-1.0f + max(0.0f, dot(dir, -normalize(ubo.lightPos)))));
    vec4 sunCol = vec4(sun, sun, sun * 0.8f, 0.0f);
    return disnatceDiffuse(vec4(0.0f), maxLength) + sunCol;
}

vec4 inducedDiffuse(float distance){
  float foggy = pow(distance, 3.0) / pow(maxLength, 3.0);
  return foggy * skyColor(normalize(ubo.lightPos));
}
#define RAY_COLOR_FINAL                                                                                      \
vec4 rayColorR(vec4 origin, vec4 direction, int recursiveDepth){                                                                    \
    Hit hit = march(origin, direction);                                                                    \
                                                                    \
                                                                    \
    float occlusion = occlusionFromSteps(hit.steps);                                                                    \
    vec4 ret;                                                                    \
    if(!hit.hasHit){                                                                    \
                                                                   \
        ret = scatterColor(direction, vec4(0.0f), hit.distance, occlusionFromSteps(hit.steps));                                                                    \
    } else {                                                                    \
                                                                       \
vec4 normalVec = hit.normal;                                                                    \
        float normDot = dot(normalVec, -normalize(ubo.lightPos));                                                                    \
        float diffuse = normDot > 0.0f ? 0.5f + 0.3f * normDot : 0.5f + 0.1f * normDot;                                                                    \
                                                                    \
        vec3 reflectDir = normalize(reflect(vec3(direction), vec3(hit.normal)));                                                                    \
                                                                    \
        vec4 newOriginSun = hit.hitPoint - direction * hit.distance * 0.001f - normalize(ubo.lightPos) * hit.distance * 0.001f;                                                                    \
        vec4 newOriginRef = hit.hitPoint - direction * hit.distance * 0.001f + vec4(reflectDir, 0.0f) * hit.distance * 0.001f;                                                                    \
        vec4 newOriginAmbient = hit.hitPoint - direction * hit.distance * 0.001f + normalVec * hit.distance * 0.001f;                                                                    \
        vec4 diffuseColor;                                                                    \
        vec4 reflectColor;                                                                    \
            Hit sunBeam = march(newOriginSun, -normalize(ubo.lightPos));                                                                    \
            float ambient = checkCloseAmbient(newOriginAmbient, normalVec, 500.0f);                                                                    \
            if(sunBeam.hasHit){                                                                    \
                diffuseColor = hit.albedo * (scatterColor(-normalize(ubo.lightPos), vec4(0.0f), sunBeam.distance, occlusionFromSteps(sunBeam.steps)) + vec4(0.8f) * ambient / 10.0f);                                                                    \
            } else {                                                                    \
                diffuseColor = hit.albedo * (scatterColor(-normalize(ubo.lightPos), vec4(0.0f), sunBeam.distance, occlusionFromSteps(sunBeam.steps)) + vec4(0.8f) * ambient / 10.0f);                                                                    \
            }                                                                    \
            Hit refBeam = march(newOriginRef, vec4(reflectDir, 0.0f));                                                                    \
            if(refBeam.hasHit) {                                                                    \
                reflectColor = refBeam.albedo * occlusionFromSteps(refBeam.steps) + disnatceDiffuse(vec4(0.0f), refBeam.distance);                                                                    \
            } else {                                                                    \
                reflectColor =skyColor(vec4(reflectDir, 0.0f));                                                                    \
            }                                                                    \
                                                                    \
        float alpha = hit.albedo.r;                                                                    \
        ret = occlusion * (diffuseColor * diffuse * (1.0f - alpha) + alpha * reflectColor) + disnatceDiffuse(vec4(0.0f), hit.distance);                                                                    \
    }                                                                    \
    ret.w = hit.distance;                                                                    \
    return ret;                                                                    \
}

#define RAY_COLOR_RECURSE(RECURSIVE)                                                                                      \
vec4 rayColor ## RECURSIVE ## RECURSIVE(vec4 origin, vec4 direction, int recursiveDepth){                                                                    \
    Hit hit = march(origin, direction);                                                                    \
                                                                    \
                                                                    \
    float occlusion = occlusionFromSteps(hit.steps);                                                                    \
    vec4 ret;                                                                    \
    if(!hit.hasHit){                                                                    \
        ret = scatterColor(direction, vec4(0.0f), hit.distance, occlusionFromSteps(hit.steps));                                                                    \
    } else {                                                                    \
        vec4 normalVec = hit.normal;                                                                    \
        float normDot = dot(normalVec, -normalize(ubo.lightPos));                                                                    \
        float diffuse = normDot > 0.0f ? 0.5f + 0.3f * normDot : 0.5f + 0.1f * normDot;                                                                    \
                                                                    \
        vec3 reflectDir = normalize(reflect(vec3(direction), vec3(hit.normal)));                                                                    \
                                                                    \
        vec4 newOriginSun = hit.hitPoint - direction * hit.distance * 0.001f - normalize(ubo.lightPos) * hit.distance * 0.001f;                                                                    \
        vec4 newOriginRef = hit.hitPoint - direction * hit.distance * 0.001f + vec4(reflectDir, 0.0f) * hit.distance * 0.001f;                                                                    \
        vec4 newOriginAmbient = hit.hitPoint - direction * hit.distance * 0.001f + normalVec * hit.distance * 0.001f;                                                                    \
        vec4 diffuseColor;                                                                    \
        vec4 reflectColor;                                                                    \
        Hit sunBeam = march(newOriginSun, -normalize(ubo.lightPos));                                                                    \
        float ambient = checkCloseAmbient(newOriginAmbient, normalVec, 500.0f);                                                                    \
        if(sunBeam.hasHit){                                                                    \
            diffuseColor = hit.albedo * (scatterColor(-normalize(ubo.lightPos), vec4(0.0f), sunBeam.distance, occlusionFromSteps(sunBeam.steps)) + vec4(0.8f) * ambient / 10.0f);                                                                    \
        } else {                                                                    \
            diffuseColor = hit.albedo * (scatterColor(-normalize(ubo.lightPos), vec4(0.0f), sunBeam.distance, occlusionFromSteps(sunBeam.steps)) + vec4(0.8f) * ambient / 10.0f);                                                                    \
        }                                                                    \
        reflectColor = rayColor ## RECURSIVE (newOriginRef, vec4(reflectDir, 0.0f), recursiveDepth - 1);                                                                    \
        float alpha = hit.albedo.r;                                                                    \
        ret = occlusion * (diffuseColor * diffuse * (1.0f - alpha) + alpha * reflectColor) + disnatceDiffuse(vec4(0.0f), hit.distance);                                                                    \
    }                                                                    \
    ret.w = hit.distance;                                                                    \
    return ret;                                                                    \
}

RAY_COLOR_FINAL

RAY_COLOR_RECURSE(R)
RAY_COLOR_RECURSE(RR)
RAY_COLOR_RECURSE(RRRR)
RAY_COLOR_RECURSE(RRRRRRRR)
RAY_COLOR_RECURSE(RRRRRRRRRRRRRRRR)
vec4 fractal(){
     return rayColorRR(ubo.cameraPos, getMarchDirection(), 0);
}

SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = fractal();
    ret.cameraOffset.x = ret.albedo.a;

    return ret;
}
