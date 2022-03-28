#include "RenderEngine/Shaders/ShaderLoader.h"


namespace RenderEngine {
    ShaderLoader::ShaderLoader(vkw::Device &device, std::string shaderLoadPath) : ShaderImporter(device,
                                                                                                 shaderLoadPath) {

    }

    vkw::VertexShader const &ShaderLoader::loadVertexShader(RenderEngine::GeometryLayout const &geometry,
                                                            RenderEngine::ProjectionLayout const &projection) {


        return m_vertexShaders.emplace_back(ShaderImporter::loadVertexShader(geometry.description().shaderSubstageName, projection.description().shaderSubstageName));
    }

    vkw::FragmentShader const &ShaderLoader::loadFragmentShader(RenderEngine::MaterialLayout const &material,
                                                                RenderEngine::LightingLayout const &lighting) {
        return m_fragmentShaders.emplace_back(ShaderImporter::loadFragmentShader(material.description().shaderSubstageName, lighting.description().shaderSubstageName));
    }
}