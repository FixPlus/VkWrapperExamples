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
        vkw::ComputeShader const& loadComputeShader(std::string const& name) override;
    private:
        std::map<std::pair<std::string, std::string>, vkw::VertexShader> m_vertexShaders;
        std::map<std::pair<std::string, std::string>, vkw::FragmentShader> m_fragmentShaders;
        std::map<std::string, vkw::ComputeShader> m_computeShaders;
    };


}
#endif //TESTAPP_SHADERLOADER_H
