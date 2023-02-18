#include "Compute.h"
#include "vkw/CommandBuffer.hpp"
#include <array>


RenderEngine::ComputeLayout::ComputeLayout(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader,
                                           const RenderEngine::SubstageDescription &desc, uint32_t maxSets):
        PipelineStageLayout(device, desc, maxSets),
        m_layout(device, std::array<vkw::DescriptorSetLayoutCRef, 1>{layout()}, description().pushConstants),
        m_pipeline(device, {m_layout, shaderLoader.loadComputeShader(desc.shaderSubstageName)}){

}

void RenderEngine::ComputeLayout::bind(vkw::CommandBuffer &buffer) const{
    buffer.bindComputePipeline(m_pipeline);
}

RenderEngine::Compute::Compute(RenderEngine::ComputeLayout &parent):
            PipelineStage<ComputeLayout>(parent) {

}

void RenderEngine::Compute::dispatch(vkw::CommandBuffer &buffer, uint32_t groupsX, uint32_t groupsY,
                                     uint32_t groupsZ) const {
    layout().bind(buffer);
    if(hasSet()){
        buffer.bindDescriptorSets(layout().pipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, set(), 0);
    }

    buffer.dispatch(groupsX, groupsY, groupsZ);
}
