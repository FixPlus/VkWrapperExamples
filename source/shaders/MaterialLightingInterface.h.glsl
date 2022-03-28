

struct SurfaceInfo{
    vec4 albedo;
    vec3 normal;
    vec3 cameraOffset;
    vec3 position;
};

SurfaceInfo Material();
void Lighting(SurfaceInfo surfaceInfo);