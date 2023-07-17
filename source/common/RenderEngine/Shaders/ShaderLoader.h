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
        vkw::SPIRVModule loadModule(std::string_view name) override;
        vkw::VertexShader const& loadVertexShader(RenderEngine::GeometryLayout const& geometry, RenderEngine::ProjectionLayout const& projection) override;
        vkw::FragmentShader const& loadFragmentShader(RenderEngine::MaterialLayout const& material, RenderEngine::LightingLayout const& lighting) override;
        vkw::ComputeShader const& loadComputeShader(vkw::SPIRVModule const& module) override;
    private:
        vkw::SPIRVModule m_general_vert;
        vkw::SPIRVModule m_general_frag;
        std::map<std::pair<RenderEngine::GeometryLayout const*, RenderEngine::ProjectionLayout const*>, vkw::VertexShader> m_vertexShaders;
        std::map<std::pair<RenderEngine::MaterialLayout const*, RenderEngine::LightingLayout const*>, vkw::FragmentShader> m_fragmentShaders;
        std::map<vkw::SPIRVModuleInfo const*, vkw::ComputeShader> m_computeShaders;
    };


}
#endif //TESTAPP_SHADERLOADER_H
