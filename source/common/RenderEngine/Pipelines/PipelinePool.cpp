#include "PipelinePool.h"

namespace {

std::vector<VkPushConstantRange>
getPushConstants(const RenderEngine::GeometryLayout &geometryLayout,
                 const RenderEngine::ProjectionLayout &projectionLayout,
                 const RenderEngine::MaterialLayout &materialLayout,
                 const RenderEngine::LightingLayout &lightingLayout) {
  std::vector<VkPushConstantRange> ret;

  for (auto &&constant : geometryLayout.module().info().pushConstants()) {
    if (std::ranges::any_of(ret, [&constant](auto& c) {
          return c.offset == constant.offset();
        }))
      continue;
    ret.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT,
                                         constant.offset(), constant.size()});
  }

  for (auto &&constant : projectionLayout.module().info().pushConstants()) {
    if (std::ranges::any_of(ret,  [&constant](auto& c) {
          return c.offset == constant.offset();
        }))
      continue;
    ret.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT,
                                         constant.offset(), constant.size()});
  }

  for (auto &&constant : materialLayout.module().info().pushConstants()) {
    auto found = std::ranges::find_if(ret,  [&constant](auto& c) {
      return c.offset == constant.offset();
    });
    if (found != ret.end()) {
      found->stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
      continue;
    }
    ret.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT,
                                         constant.offset(), constant.size()});
  }

  for (auto &&constant : lightingLayout.module().info().pushConstants()) {
    auto found = std::ranges::find_if(ret,  [&constant](auto& c) {
      return c.offset == constant.offset();
    });
    if (found != ret.end()) {
      found->stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
      continue;
    }
    ret.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT,
                                         constant.offset(), constant.size()});
  }

  return ret;
}
} // namespace
vkw::PipelineLayout const &RenderEngine::GraphicsPipelinePool::layoutOf(
    const RenderEngine::GeometryLayout &geometryLayout,
    const RenderEngine::ProjectionLayout &projectionLayout,
    const RenderEngine::MaterialLayout &materialLayout,
    const RenderEngine::LightingLayout &lightingLayout) {

  auto key = M_PipelineKey{&geometryLayout, &projectionLayout, &materialLayout,
                           &lightingLayout};
  if (m_layouts.contains(key))
    return m_layouts.at(key);

  std::vector<VkPushConstantRange> pushConstants = getPushConstants(
      geometryLayout, projectionLayout, materialLayout, lightingLayout);

  std::vector<std::reference_wrapper<vkw::DescriptorSetLayout const>> bindings{
      geometryLayout.layout(), projectionLayout.layout(),
      materialLayout.layout(), lightingLayout.layout()};

  return m_layouts
      .emplace(key, vkw::PipelineLayout{m_device, bindings, pushConstants})
      .first->second;
}

vkw::GraphicsPipeline const &RenderEngine::GraphicsPipelinePool::pipelineOf(
    const RenderEngine::GeometryLayout &geometryLayout,
    const RenderEngine::ProjectionLayout &projectionLayout,
    const RenderEngine::MaterialLayout &materialLayout,
    const RenderEngine::LightingLayout &lightingLayout) {
  auto key = M_PipelineKey{&geometryLayout, &projectionLayout, &materialLayout,
                           &lightingLayout};
  if (m_pipelines.contains(key))
    return m_pipelines.at(key);

  vkw::GraphicsPipelineCreateInfo createInfo{
      lightingLayout.pass(), lightingLayout.subpass(),
      layoutOf(geometryLayout, projectionLayout, materialLayout,
               lightingLayout)};

  createInfo.addInputAssemblyState(geometryLayout.inputAssemblyState());
  createInfo.addVertexInputState(geometryLayout.vertexInputState());
  createInfo.addVertexShader(
      m_shaderLoader.get().loadVertexShader(geometryLayout, projectionLayout));

  if (!materialLayout.depthOnly()) {
    createInfo.addFragmentShader(m_shaderLoader.get().loadFragmentShader(
        materialLayout, lightingLayout));
  }

  if (materialLayout.depthTestState())
    createInfo.addDepthTestState(materialLayout.depthTestState().value());
  createInfo.addRasterizationState(materialLayout.rasterizationState());

  for (auto &blendState : lightingLayout.blendStates()) {
    createInfo.addBlendState(blendState.first, blendState.second);
  }

  createInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
  createInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);

  return m_pipelines.emplace(key, vkw::GraphicsPipeline{m_device, createInfo})
      .first->second;
}
