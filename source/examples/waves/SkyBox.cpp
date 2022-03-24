#include "SkyBox.h"
#include "vkw/CommandBuffer.hpp"

static vkw::NullVertexInputState skybox_state{};

SkyBox::SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass) :
        m_device(device),
        m_geometry_layout(device, RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&skybox_state,.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, .maxGeometries=1}),
        m_projection_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, 1),
        m_material_layout(device, RenderEngine::MaterialLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="skybox",.setBindings={vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},.maxMaterials=1}),
        m_lighting_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, pass, subpass, 1),
        m_geometry(m_geometry_layout),
        m_projection(m_projection_layout),
        m_lighting(m_lighting_layout),
        m_material(device, m_material_layout)
{
}

void SkyBox::draw(RenderEngine::GraphicsRecordingState &buffer) {

    buffer.setGeometry(m_geometry);
    buffer.setProjection(m_projection);
    buffer.setMaterial(m_material);
    buffer.setLighting(m_lighting);
    buffer.bindPipeline();
    buffer.commands().draw(3, 1);

}
