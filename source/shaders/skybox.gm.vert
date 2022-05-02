#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"


WorldVertexInfo Geometry(){
    WorldVertexInfo ret;
    ret.UVW.x = (gl_VertexIndex & 1u) * 2.0f;
    ret.UVW.y = (gl_VertexIndex & 2u);
    return ret;
}
