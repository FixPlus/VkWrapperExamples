#version 450
#extension GL_GOOGLE_include_directive : require
#include "GeomProjInterface.h.glsl"


void main() {
    Projection(Geometry());
}
