#include "Compute.h"
#include "vkw/CommandBuffer.hpp"
#include <array>

RenderEngine::ComputeLayout::ComputeLayout(
    vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader,
    const RenderEngine::SubstageDescription &desc, uint32_t maxSets)
    : PipelineStageLayout(device, shaderLoader, desc, 0, ".comp", maxSets),
      m_layout([&]() {
        boost::container::small_vector<VkPushConstantRange, 2> pushConstants;
        for (auto &&constant : module().info().pushConstants()) {
          pushConstants.emplace_back(VkPushConstantRange{
              VK_SHADER_STAGE_COMPUTE_BIT, constant.offset(), constant.size()});
        }
        return vkw::PipelineLayout(
            device, layout(),
            std::span<const VkPushConstantRange>(pushConstants.data(),
                                                 pushConstants.size()));
      }()),
      m_pipeline(device, {m_layout, shaderLoader.loadComputeShader(module())}) {
}

void RenderEngine::ComputeLayout::bind(vkw::CommandBuffer &buffer) const {
  buffer.bindComputePipeline(m_pipeline);
}

RenderEngine::Compute::Compute(RenderEngine::ComputeLayout &parent)
    : PipelineStage<ComputeLayout>(parent) {}

void RenderEngine::Compute::dispatch(vkw::CommandBuffer &buffer,
                                     uint32_t groupsX, uint32_t groupsY,
                                     uint32_t groupsZ) const {
  layout().bind(buffer);
  if (hasSet()) {
    buffer.bindDescriptorSets(layout().pipelineLayout(),
                              VK_PIPELINE_BIND_POINT_COMPUTE, set(), 0);
  }

  buffer.dispatch(groupsX, groupsY, groupsZ);
}
