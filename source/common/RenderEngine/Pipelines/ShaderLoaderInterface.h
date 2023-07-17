#ifndef TESTAPP_SHADERLOADERINTERFACE_H
#define TESTAPP_SHADERLOADERINTERFACE_H

#include "vkw/Shader.hpp"

namespace RenderEngine{

    class GeometryLayout;
    class ProjectionLayout;
    class MaterialLayout;
    class LightingLayout;

    class ShaderLoaderInterface: public vkw::ReferenceGuard{
    public:

        virtual vkw::SPIRVModule loadModule(std::string_view name) = 0;

        virtual vkw::VertexShader const& loadVertexShader(GeometryLayout const& geometry, ProjectionLayout const& projection) = 0;

        virtual vkw::FragmentShader const& loadFragmentShader(MaterialLayout const& material, LightingLayout const& lighting) = 0;

        virtual vkw::ComputeShader const& loadComputeShader(vkw::SPIRVModule const& module) = 0;

    };
}
#endif //TESTAPP_SHADERLOADERINTERFACE_H
