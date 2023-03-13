#include "RenderEngine/Shaders/ShaderLoader.h"

namespace RenderEngine {
ShaderLoader::ShaderLoader(vkw::Device &device, std::string shaderLoadPath)
    : ShaderImporter(device, shaderLoadPath) {}

vkw::VertexShader const &ShaderLoader::loadVertexShader(
    RenderEngine::GeometryLayout const &geometry,
    RenderEngine::ProjectionLayout const &projection) {
  auto key = std::make_pair(&geometry, &projection);
  if (m_vertexShaders.contains(key))
    return m_vertexShaders.at(key);

  boost::container::small_vector<std::string_view, 2> additionalStages;
  std::transform(geometry.description().additionalShaderFiles.begin(),
                 geometry.description().additionalShaderFiles.end(),
                 std::back_inserter(additionalStages),
                 [](auto &shaderName) { return shaderName; });

  std::transform(projection.description().additionalShaderFiles.begin(),
                 projection.description().additionalShaderFiles.end(),
                 std::back_inserter(additionalStages),
                 [](auto &shaderName) { return shaderName; });

  return m_vertexShaders
      .emplace(key, ShaderImporter::loadVertexShader(
                        geometry.description().shaderSubstageName,
                        projection.description().shaderSubstageName,
                        additionalStages))
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

  return m_fragmentShaders
      .emplace(key,
               ShaderImporter::loadFragmentShader(
                   material.description().shaderSubstageName,
                   lighting.description().shaderSubstageName, additionalStages))
      .first->second;
}

vkw::ComputeShader const &
ShaderLoader::loadComputeShader(const std::string &name) {
  if (m_computeShaders.contains(name))
    return m_computeShaders.at(name);

  return m_computeShaders.emplace(name, ShaderImporter::loadComputeShader(name))
      .first->second;
}
} // namespace RenderEngine