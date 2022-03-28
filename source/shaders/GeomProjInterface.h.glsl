
// Geometry => projection interface
// Must be persistent among all geometry and projection subshaders

struct WorldVertexInfo{
    vec3 position;
    vec3 normal;
    vec3 UVW;
    vec4 color;
};

WorldVertexInfo Geometry();
void Projection(WorldVertexInfo worldVertexInfo);
