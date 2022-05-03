#include "SkyBox.h"
#include "vkw/CommandBuffer.hpp"

static vkw::NullVertexInputState skybox_state{};

SkyBox::SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass) :
        m_device(device),
        m_geometry_layout(device, RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&skybox_state,.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, .maxGeometries=1}),
        m_projection_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, 1),
        m_material_layout(device, RenderEngine::MaterialLayout::CreateInfo{
            .substageDescription=RenderEngine::SubstageDescription{
                .shaderSubstageName="skybox",
                .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                .maxMaterials=1}),
        m_lighting_layout(device, RenderEngine::LightingLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="flat"}, .pass=pass, .subpass=subpass}, 1),
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

SkyBoxSettings::SkyBoxSettings(TestApp::GUIFrontEnd &gui, SkyBox &skybox, const std::string &title): GUIWindow(gui, TestApp::WindowSettings{.title=title}), m_skybox(skybox) {

}

void SkyBoxSettings::onGui() {
    if(ImGui::CollapsingHeader("Sun")){
        ImGui::ColorEdit4("color", &m_skybox.get().sun.color.x);
        ImGui::SliderFloat("irradiance", &m_skybox.get().sun.params.z, 0.1f, 1000.0f);
        ImGui::SliderFloat("lon", &m_skybox.get().sun.params.x, 0.0f, glm::pi<float>() * 2.0f);
        ImGui::SliderFloat("lat", &m_skybox.get().sun.params.y, 0.0f, glm::pi<float>());
    }
    if(ImGui::CollapsingHeader("Atmosphere")){
        ImGui::SliderFloat4("K", &m_skybox.get().atmosphere.K.x, 0.0f, 1.0f);
        ImGui::SliderFloat("H0", &m_skybox.get().atmosphere.params.z, 0.0f, 1.0f);
        ImGui::SliderFloat("Atmosphere height", &m_skybox.get().atmosphere.params.y, 0.0f, 10000.0f);
        ImGui::SliderFloat("g", &m_skybox.get().atmosphere.params.w, -1.0f, 0.0f);
        ImGui::SliderInt("samples", &m_skybox.get().atmosphere.samples, 5, 50);
    }
}
