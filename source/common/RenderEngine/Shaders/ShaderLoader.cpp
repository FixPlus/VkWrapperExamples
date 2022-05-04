#include "RenderEngine/Shaders/ShaderLoader.h"


namespace RenderEngine {
    ShaderLoader::ShaderLoader(vkw::Device &device, std::string shaderLoadPath) : ShaderImporter(device,
                                                                                                 shaderLoadPath) {

    }

    vkw::VertexShader const &ShaderLoader::loadVertexShader(RenderEngine::GeometryLayout const &geometry,
                                                            RenderEngine::ProjectionLayout const &projection) {
        std::pair<std::string, std::string> key = {geometry.description().shaderSubstageName, projection.description().shaderSubstageName};
        if(m_vertexShaders.contains(key))
            return m_vertexShaders.at(key);

        return m_vertexShaders.emplace(key, ShaderImporter::loadVertexShader(key.first, key.second)).first->second;
    }

    vkw::FragmentShader const &ShaderLoader::loadFragmentShader(RenderEngine::MaterialLayout const &material,
                                                                RenderEngine::LightingLayout const &lighting) {
        std::pair<std::string, std::string> key = {material.description().shaderSubstageName, lighting.description().shaderSubstageName};
        if(m_fragmentShaders.contains(key))
            return m_fragmentShaders.at(key);

        return m_fragmentShaders.emplace(key, ShaderImporter::loadFragmentShader(key.first, key.second)).first->second;
    }

    vkw::ComputeShader const &ShaderLoader::loadComputeShader(const std::string &name) {
        if(m_computeShaders.contains(name))
            return m_computeShaders.at(name);

        return m_computeShaders.emplace(name, ShaderImporter::loadComputeShader(name)).first->second;
    }
}