#ifndef TESTAPP_SHADERLOADERINTERFACE_H
#define TESTAPP_SHADERLOADERINTERFACE_H

#include "vkw/Shader.hpp"

namespace RenderEngine{

    class GeometryLayout;
    class ProjectionLayout;
    class MaterialLayout;
    class LightingLayout;

    class ShaderLoaderInterface{
    public:
        virtual vkw::VertexShader const& loadVertexShader(GeometryLayout const& geometry, ProjectionLayout const& projection) = 0;

        virtual vkw::FragmentShader const& loadFragmentShader(MaterialLayout const& material, LightingLayout const& lighting) = 0;

        virtual vkw::ComputeShader const& loadComputeShader(std::string const& name) = 0;

    };
}
#endif //TESTAPP_SHADERLOADERINTERFACE_H
