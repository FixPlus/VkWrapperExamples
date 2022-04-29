#include "WaterSurface.h"




WaterSurface::WaterSurface(vkw::Device &device) : TestApp::Grid(device), RenderEngine::GeometryLayout(device,
                                                                                      RenderEngine::GeometryLayout::CreateInfo{.vertexInputState = &m_vertexInputStateCreateInfo, .substageDescription={.shaderSubstageName="waves",
                                                                                              .setBindings = {vkw::DescriptorSetLayoutBinding{0,
                                                                                                                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}, .pushConstants={
                                                                                                      VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=
                                                                                                      8 * sizeof(float)}}}, .maxGeometries=1}),
                                                         m_geometry(device, *this) {

}

void WaterSurface::preDraw(RenderEngine::GraphicsRecordingState &buffer) {
    buffer.setGeometry(m_geometry);
    buffer.bindPipeline();
}

WaterSurface::Geometry::Geometry(vkw::Device &device, WaterSurface &surface): RenderEngine::Geometry(surface), m_ubo(device,
                                                                                    VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_ubo_mapped(m_ubo.map()){
    set().write(0, m_ubo);
}

WaterMaterial::Material::Material(vkw::Device& device, WaterMaterial& waterMaterial): RenderEngine::Material(waterMaterial),  m_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU}),
                                                                                      m_mapped(m_buffer.map()){
    set().write(0, m_buffer);
}

WaveSettings::WaveSettings(TestApp::GUIFrontEnd &gui, WaterSurface &water,
                           std::map<std::string, std::reference_wrapper<WaterMaterial>> materials): TestApp::GridSettings(gui, water, "Waves"),
                           m_water(water), m_materials(std::move(materials)){
    if(m_materials.empty())
        throw std::runtime_error("Cannot create WaveSettings window with empty material map");

    for(auto& material: m_materials){
        m_materialNames.emplace_back(material.first.c_str());
    }
}

void WaveSettings::onGui() {
    TestApp::GridSettings::onGui();

    if(!ImGui::CollapsingHeader("Surface waves"))
        return;

    static float dirs[4] = {33.402f, 103.918f, 68.66f, 50.103f};
    static float wavenums[4] = {28.709f, 22.041f, 10.245f, 2.039f};
    static bool firstSet = true;
    for (int i = 0; i < 4; ++i) {
        std::string header = "Wave #" + std::to_string(i);
        if (firstSet) {
            m_water.get().ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
            m_water.get().ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];

        }
        if (ImGui::TreeNode(header.c_str())) {
            if (ImGui::SliderFloat("Direction", dirs + i, 0.0f, 360.0f)) {
                m_water.get().ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                m_water.get().ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];

            }

            if (ImGui::SliderFloat("Wavelength", wavenums + i, 0.5f, 100.0f)) {
                m_water.get().ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                m_water.get().ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];
            }
            ImGui::SliderFloat("Steepness", &m_water.get().ubo.waves[i].w, 0.0f, 1.0f);
            ImGui::SliderFloat("Steepness decay factor", &m_water.get().ubo.waves[i].z, 0.0f, 1.0f);
            ImGui::TreePop();
        }

    }

    firstSet = false;

    ImGui::Combo("Materials", &m_pickedMaterial, m_materialNames.data(), m_materialNames.size());

    auto& material = pickedMaterial();

    ImGui::ColorEdit4("Deep water color", &material.description.deepWaterColor.x);
    ImGui::SliderFloat("Metallic", &material.description.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &material.description.roughness, 0.0f, 1.0f);
}

WaveSettings::WaveSettings(WaveSettings &&another) noexcept: TestApp::GridSettings(std::move(another)), m_water(another.m_water), m_pickedMaterial(another.m_pickedMaterial), m_materials(std::move(another.m_materials)) {
    for(auto& material: m_materials){
        m_materialNames.emplace_back(material.first.c_str());
    }
}

WaveSettings &WaveSettings::operator=(WaveSettings &&another) noexcept {
    TestApp::GridSettings::operator=(std::move(another));
    m_water = another.m_water;
    m_materials = std::move(another.m_materials);
    m_pickedMaterial = another.m_pickedMaterial;
    m_materialNames.clear();
    for(auto& material: m_materials){
        m_materialNames.emplace_back(material.first.c_str());
    }
    return *this;
}
