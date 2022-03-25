#include "RenderEngine/Shaders/ShaderLoader.h"


namespace RenderEngine {
    ShaderLoader::ShaderLoader(vkw::Device &device, std::string shaderLoadPath) : ShaderImporter(device,
                                                                                                 shaderLoadPath) {

    }

    vkw::VertexShader const &ShaderLoader::loadVertexShader(RenderEngine::GeometryLayout const &geometry,
                                                            RenderEngine::ProjectionLayout const &projection) {
        auto fullShaderName =
                geometry.description().shaderSubstageName + "_" + projection.description().shaderSubstageName;
        return m_vertexShaders.emplace_back(ShaderImporter::loadVertexShader(fullShaderName));
    }

    vkw::FragmentShader const &ShaderLoader::loadFragmentShader(RenderEngine::MaterialLayout const &material,
                                                                RenderEngine::LightingLayout const &lighting) {
        auto fullShaderName =
                material.description().shaderSubstageName + "_" + lighting.description().shaderSubstageName;
        return m_fragmentShaders.emplace_back(ShaderImporter::loadFragmentShader(fullShaderName));
    }
}