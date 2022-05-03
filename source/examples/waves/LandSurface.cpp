
#include "LandSurface.h"

TestApp::LandSurface::LandSurface(vkw::Device &device) : Grid(device, false),
         RenderEngine::GeometryLayout(device,
                                       RenderEngine::GeometryLayout::CreateInfo{.vertexInputState = &m_vertexInputStateCreateInfo, .substageDescription={.shaderSubstageName="land",
                                               .setBindings = {vkw::DescriptorSetLayoutBinding{0,
                                                                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}, .pushConstants={
                                                       VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=
                                                       8 * sizeof(float)}}}, .maxGeometries=1}),
                                                         m_geometry(device, *this){
    cascadePower = 2;
    cascades = 8;
    tileScale = 0.75f;
}

void TestApp::LandSurface::preDraw(RenderEngine::GraphicsRecordingState &buffer) {
    buffer.setGeometry(m_geometry);
    buffer.bindPipeline();
}

TestApp::LandSurface::Geometry::Geometry(vkw::Device &device, TestApp::LandSurface &surface): RenderEngine::Geometry(surface),
                                                                                              m_ubo(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                                              m_ubo_mapped(m_ubo.map()){
    set().write(0, m_ubo);
}

TestApp::LandMaterial::Material::Material(vkw::Device &device, TestApp::LandMaterial &landMaterial): RenderEngine::Material(landMaterial),
                                                                                                     m_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                                                     m_mapped(m_buffer.map()){
    set().write(0, m_buffer);
}

TestApp::LandSettings::LandSettings(TestApp::GUIFrontEnd &gui, TestApp::LandSurface &land,
                                    std::map<std::string, std::reference_wrapper<LandMaterial>> materials):
        GridSettings(gui, land, "Land"), m_land(land), m_materials(std::move(materials))
                                    {
    for(auto& material: m_materials){
        m_materialNames.emplace_back(material.first.c_str());
    }

}

void TestApp::LandSettings::onGui() {
    GridSettings::onGui();

    auto& land = m_land.get();

    ImGui::Combo("Materials", &m_pickedMaterial, m_materialNames.data(), m_materialNames.size());
    auto& material = pickedMaterial();
    ImGui::ColorEdit4("Land color", &material.description.color.x);
    ImGui::SliderFloat("Split level", &material.description.params.x, -100.0f, 100.0f);
    if(!ImGui::CollapsingHeader("Perlin noise"))
        return;



    ImGui::SliderFloat("Height scale", &land.ubo.params.x, 0.1f, 100.0f);
    ImGui::SliderFloat("Distance scale", &land.ubo.params.y, 0.1f, 1000.0f);
    ImGui::SliderInt("Harmonics", &land.ubo.harmonics, 1, 20);
    ImGui::SliderFloat("Height mutation factor", &land.ubo.params.z, 0.0f, 1.0f);


}
