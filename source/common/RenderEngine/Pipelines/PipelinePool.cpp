#include "PipelinePool.h"


vkw::PipelineLayout const &
RenderEngine::GraphicsPipelinePool::layoutOf(const RenderEngine::GeometryLayout &geometryLayout,
                                             const RenderEngine::ProjectionLayout &projectionLayout,
                                             const RenderEngine::MaterialLayout &materialLayout,
                                             const RenderEngine::LightingLayout &lightingLayout) {

    auto key = M_PipelineKey{&geometryLayout, &projectionLayout, &materialLayout, &lightingLayout};
    if(m_layouts.contains(key))
        return m_layouts.at(key);

    std::vector<VkPushConstantRange> pushConstants{};
    std::copy(geometryLayout.description().pushConstants.begin(), geometryLayout.description().pushConstants.end(), std::back_inserter(pushConstants));
    std::copy(projectionLayout.description().pushConstants.begin(), projectionLayout.description().pushConstants.end(), std::back_inserter(pushConstants));
    std::copy(materialLayout.description().pushConstants.begin(), materialLayout.description().pushConstants.end(), std::back_inserter(pushConstants));
    std::copy(lightingLayout.description().pushConstants.begin(), lightingLayout.description().pushConstants.end(), std::back_inserter(pushConstants));

    std::vector<vkw::DescriptorSetLayoutCRef> bindings{geometryLayout.layout(), projectionLayout.layout(), materialLayout.layout(), lightingLayout.layout()};

    return m_layouts.emplace(key, vkw::PipelineLayout{m_device, bindings, pushConstants}).first->second;
}

vkw::GraphicsPipeline const &
RenderEngine::GraphicsPipelinePool::pipelineOf(const RenderEngine::GeometryLayout &geometryLayout,
                                               const RenderEngine::ProjectionLayout &projectionLayout,
                                               const RenderEngine::MaterialLayout &materialLayout,
                                               const RenderEngine::LightingLayout &lightingLayout) {
    auto key = M_PipelineKey{&geometryLayout, &projectionLayout, &materialLayout, &lightingLayout};
    if(m_pipelines.contains(key))
        return m_pipelines.at(key);

    vkw::GraphicsPipelineCreateInfo createInfo{lightingLayout.pass(), lightingLayout.subpass(), layoutOf(geometryLayout, projectionLayout, materialLayout, lightingLayout)};

    createInfo.addInputAssemblyState(geometryLayout.inputAssemblyState());
    createInfo.addVertexInputState(*geometryLayout.vertexInputState());
    createInfo.addVertexShader(m_shaderLoader.get().loadVertexShader(geometryLayout, projectionLayout));
    createInfo.addFragmentShader(m_shaderLoader.get().loadFragmentShader(materialLayout, lightingLayout));
    if(materialLayout.depthTestState())
        createInfo.addDepthTestState(materialLayout.depthTestState().value());
    createInfo.addRasterizationState(materialLayout.rasterizationState());

    for(auto& blendState: lightingLayout.blendStates()){
        createInfo.addBlendState(blendState.first, blendState.second);
    }

    createInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    createInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);


    return m_pipelines.emplace(key, vkw::GraphicsPipeline{m_device, createInfo}).first->second;
}
