#ifndef TESTAPP_SHADERLOADER_H
#define TESTAPP_SHADERLOADER_H
#include "RenderEngine/AssetImport/AssetImport.h"
#include "RenderEngine/Pipelines/ShaderLoaderInterface.h"
#include "RenderEngine/Pipelines/PipelinePool.h"
#include "spirv-tools/linker.hpp"

namespace RenderEngine{

    class ShaderLoader: public ShaderLoaderInterface,  public ShaderImporter{
    public:
        ShaderLoader(vkw::Device& device, std::string shaderLoadPath);
        vkw::VertexShader const& loadVertexShader(RenderEngine::GeometryLayout const& geometry, RenderEngine::ProjectionLayout const& projection) override;
        vkw::FragmentShader const& loadFragmentShader(RenderEngine::MaterialLayout const& material, RenderEngine::LightingLayout const& lighting) override;

    private:
        std::vector<vkw::VertexShader> m_vertexShaders;
        std::vector<vkw::FragmentShader> m_fragmentShaders;

    };


}
#endif //TESTAPP_SHADERLOADER_H