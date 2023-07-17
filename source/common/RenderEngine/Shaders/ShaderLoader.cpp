#include "RenderEngine/Shaders/ShaderLoader.h"
#include <array>

namespace RenderEngine {
ShaderLoader::ShaderLoader(vkw::Device &device, std::string shaderLoadPath)
    : ShaderImporter(device, shaderLoadPath),
      m_general_vert(loadModule("general.vert")),
      m_general_frag(loadModule("general.frag")) {}

vkw::SPIRVModule ShaderLoader::loadModule(std::string_view name) {
  return ShaderImporter::loadModule(name);
}

vkw::VertexShader const &ShaderLoader::loadVertexShader(
    RenderEngine::GeometryLayout const &geometry,
    RenderEngine::ProjectionLayout const &projection) {
  auto key = std::make_pair(&geometry, &projection);
  if (m_vertexShaders.contains(key))
    return m_vertexShaders.at(key);

  std::array<vkw::SPIRVModule, 3> modules = {geometry.module(), projection.module(), m_general_vert};

  return m_vertexShaders
      .emplace(key, ShaderImporter::loadVertexShader(vkw::SPIRVModule(modules)))
      .first->second;
}

vkw::FragmentShader const &
ShaderLoader::loadFragmentShader(RenderEngine::MaterialLayout const &material,
                                 RenderEngine::LightingLayout const &lighting) {
  auto key = std::make_pair(&material, &lighting);
  if (m_fragmentShaders.contains(key))
    return m_fragmentShaders.at(key);

  boost::container::small_vector<std::string_view, 2> additionalStages;
  std::transform(material.description().additionalShaderFiles.begin(),
                 material.description().additionalShaderFiles.end(),
                 std::back_inserter(additionalStages),
                 [](auto &shaderName) { return shaderName; });

  std::transform(lighting.description().additionalShaderFiles.begin(),
                 lighting.description().additionalShaderFiles.end(),
                 std::back_inserter(additionalStages),
                 [](auto &shaderName) { return shaderName; });

  std::array<vkw::SPIRVModule, 3> modules = {material.module(), lighting.module(), m_general_frag};

  return m_fragmentShaders
      .emplace(key,
               ShaderImporter::loadFragmentShader(vkw::SPIRVModule(modules)))
      .first->second;
}

vkw::ComputeShader const &
ShaderLoader::loadComputeShader(vkw::SPIRVModule const& module) {
  if (m_computeShaders.contains(&module.info()))
    return m_computeShaders.at(&module.info());

  return m_computeShaders.emplace(&module.info(), ShaderImporter::loadComputeShader(vkw::SPIRVModule{std::span{&module, 1}}))
      .first->second;
}
} // namespace RenderEngine