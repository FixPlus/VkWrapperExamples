#include "SkyBox.h"
#include "vkw/CommandBuffer.hpp"


SkyBox::SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass, const GlobalLayout &globalLayout,
               const TestApp::ShaderLoader &loader) :
        m_device(device),
        m_global_layout(globalLayout),
        m_vertexShader(loader.loadVertexShader("skybox")),
        m_fragmentShader(loader.loadFragmentShader("skybox")),
        m_pipelineLayout(device, globalLayout.layout()),
        m_pipeline(m_compile_pipeline(pass, subpass)) {

}

vkw::GraphicsPipeline SkyBox::m_compile_pipeline(vkw::RenderPass const &pass, uint32_t subpass) {

    vkw::GraphicsPipelineCreateInfo createInfo{pass, subpass, m_pipelineLayout};

    createInfo.addVertexShader(m_vertexShader);
    createInfo.addFragmentShader(m_fragmentShader);

    createInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    createInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);


    return {m_device, createInfo};
}

void SkyBox::draw(vkw::CommandBuffer &buffer) {
    buffer.bindGraphicsPipeline(m_pipeline);
    buffer.bindDescriptorSets(m_pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, m_global_layout.get().set(), 0);
    buffer.draw(3, 1);

}
