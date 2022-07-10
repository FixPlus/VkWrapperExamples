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
float maxLength = 100000.0f;
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
#define RANK 11.0f


#define SHAPE_BALL 1


vec4 ShapeOffset(vec4 pos){ //x y z for vector, a for length
    #ifdef SHAPE_BALL
    float new_len = length(pos.xyz) - 0.5f;
    vec3 dir = normalize(pos.xyz);
    return vec4(dir, max(0.0f, new_len));
    #elif SHAPE_CUBE
    vec3 dir;
    if(abs(pos.x) >= abs(pos.y) && abs(pos.x) >= abs(pos.z))
    dir =  vec3(pos.x > 0.0f ? 1.0f: -1.0f, 0.0f, 0.0f);
    else if(abs(pos.y) >= abs(pos.x) && abs(pos.y) >= abs(pos.z))
    dir =  vec3(0.0f, pos.y > 0.0f ? 1.0f: -1.0f, 0.0f);
    else
    dir =  vec3(0.0f, 0.0f, pos.z > 0.0f ? 1.0f: -1.0f);
    float divX = pos.x > 0.5f ? (pos.x - 0.5f) : (pos.x < -0.5f ? pos.x + 0.5f : 0.0f);
    float divY = pos.y > 0.5f ? (pos.y - 0.5f) : (pos.y < -0.5f ? pos.y + 0.5f : 0.0f);
    float divZ = pos.z > 0.5f ? (pos.z - 0.5f) : (pos.z < -0.5f ? pos.z + 0.5f : 0.0f);
    return vec4(dir, sqrt(divX * divX + divY * divY + divZ * divZ));
    #elif SHAPE_TORUS
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
        #elif
    return pos;
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

vec4 fractal(){

    vec4 outFragColor;
    // color and mix used for the background. Now it just uses single color, but could be altered by texture

    vec4 color = vec4(0.2f, 0.8f, 0.9f,1.0f); //texture(samplerColor1, vec2(inUVW.x, inUVW.y), 1.0f);
    vec4 mix = vec4(1.0f, 1.0f, 1.0f, 1.0f) - color; //texture(samplerColor1, vec2(inUVW.x, inUVW.y), 1.0f);
    vec4 glowCol = vec4(1.0f, 0.0f, 1.0f, 0.0f);


    vec4 dir = getMarchDirection(); //direction od ray shooting out of camera
    vec4 curPos = ubo.cameraPos; // contains the posirion of ray's front point


    int n = 0; //count of steps
    float totalRayLength = 0.0f; // counter for ray length
    float depth = 0.0f; // accum for depth


    float temp; // used to temproary contaion ED for each step
    float lastL; // used to contain the prev value of temp var
    vec4 prevPos = curPos; // used to contain the prev value of curPos

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
        if(log(length(curPos)) - log(temp) > 13.0f)
        temp = pow(2.715, log(length(curPos)) - 13.0f) ;
        curPos += dir * temp * 0.995f;
        n++;
        totalRayLength += temp * 0.995f;
        lastL = temp* 0.99f;
    }

    if(totalRayLength >= maxLength){ //should draw background case

        //sun function is to draw the light source
        float sun = pow(100,  10.0f * (-1.0f + max(0.0f, dot(dir, -normalize(ubo.lightPos)))));

        //float glowing = pow(2.715, -20.0f/float(n));
        //color = color + mix * inUVW.y;
        vec4 sunCol = vec4(sun, sun, sun * 0.8f, 0.0f);
        outFragColor = (color + sunCol); //+ glowCol * glowing;
        depth = maxLength;
    }
    else{
        depth = totalRayLength;
        vec4 fog = color;
        float foggy = pow(totalRayLength, 3.0) / pow(maxLength, 3.0);
        float mip = foggy * 8.0f;
        //ambient occlusion calculated from number of steps
        float occlusion = 1.0f - pow(2.715, -35.0f/float(n));
        //float occlusion = 1.0f - 0.8f * float(n) / float(maxSteps);

        //getting texture coordinates of the object point
        vec2 texCoords = closestTextureCoord(curPos);
        vec4 posColor = closestColor(curPos);//= vec4(0.5f + 0.5f * sin((curPos.x + curPos.y) / 10.0f), 0.0f, 0.0f, 1.0f)  + vec4(0.0f, 0.5f + 0.5f * sin((curPos.x + curPos.z) / 100.0f), 0.0f, 1.0f)  + vec4(1.0f, 1.0f, 0.5f + 0.5f * sin((curPos.z + curPos.y) / 1000.0f), 1.0f);
        //enlighted calculated by the angle between surface normal and lightSource beam
        vec4 normalVec = closestNormal(curPos);
        float normDot = dot(normalVec, -normalize(ubo.lightPos));
        float enlighted = normDot > 0.0f ? 0.5f + 0.3f * normDot : 0.5f + 0.1f * normDot;
        vec4 refVec = reflect(dir, normalVec);
        float specular = max(0.0f, pow(dot(normalize(refVec), -normalize(ubo.lightPos)), 32.0f));

        //	next step is to calculate dropped shadow
        //	for that we use same technic by shooting the ray from the object point
        //	in the direction of light source

        // here we use prevPos variable to start our marching from the point where we still
        // didnt hit epsilon treshhold
        if(/*ubo.shadowOption >= 0.0f &&*/ normDot > 0.0f && n < maxSteps / 2.0f && false){
            // we only calculate it if it is on, there are no self-shadowing and object is normally lit

            curPos = prevPos;

            // but to be sure this point is not far away from real object point we use lastL variable
            // to check how much did last step took
            // if it too much (more than 1.5 * epsilon) we do an additional step to our point
            // that is less by 1.2 * epsilon than in previous marhing was. That unsures ud that
            // we not hit epsilon treshhold again and be as close to real object point as we could

            if(lastL > epsilon * 1.5f)
            lastL -= epsilon * 1.2f;
            else
            lastL = 0.0f;

            curPos += dir * lastL;

            // then the technic is applied again

            totalRayLength = 0.0f;
            n = 0;
            vec4 normLightPos = normalize(ubo.lightPos);

            while(temp != 0.0f && (temp = closestDist(curPos)) > epsilon  && totalRayLength < maxLength && n < 100){
                curPos -= normLightPos * temp * 0.995f;
                n++;
                totalRayLength += temp * 0.995f;
            }
        }
        else{
            totalRayLength = maxLength * 1.1f;
        }

        // if we hit object on the way(rayLength < maxLength) - we apply a shadow
        // if not - normal enlighted var will be applied

        float shadow = pow(totalRayLength / maxLength, 0.125f) / 1.25f + 0.2f;
        if(totalRayLength < maxLength)
        specular = 0.0f;

        // and finnaly the totalLight is calculated based on ambient occlusion and shadowing

        float totalLight = occlusion;

        if(shadow >= 1.0f)
        totalLight *= enlighted;
        else
        totalLight *= shadow;

        //object also has a color that could be replaced by texture if needed
        //Now it is using previously calculated texture coordintes

        //color = vec4(0.5f, 0.3f, 0.6f, 0.0f);

        color = texture(samplerColor1, texCoords, mip);
        specular *= occlusion;
        /*
    color.x *= posColor.x;
    color.y *= posColor.y;
    color.z *= posColor.z;
    */
        outFragColor = (color * (1 - specular) + vec4(1.5f, 1.5f, 1.5f, 1.0f) * specular) * totalLight * (1 - foggy) + fog * foggy;
        //outFragColor = texture(samplerColor1, vec2(inUVW.x, inUVW.y), 0.0f);
    }

    outFragColor.w = depth;
    return outFragColor;
}

SurfaceInfo Material(){
    SurfaceInfo ret;
    ret.albedo = fractal();
    ret.cameraOffset.x = ret.albedo.a;

    return ret;
}
