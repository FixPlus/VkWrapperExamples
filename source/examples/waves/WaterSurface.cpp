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

    ImGui::SliderFloat("Gravitation", &m_water.get().ubo.params.x, 0.1f, 100.0f);

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

WaveSurfaceTexture::WaveSurfaceTexture(vkw::Device &device, RenderEngine::ShaderLoaderInterface& shaderLoader, uint32_t baseCascadeSize, uint32_t cascades):
        TestApp::PrecomputeImageLayout(device, shaderLoader, RenderEngine::SubstageDescription{.shaderSubstageName="fft"}, 16, 16){
    for(int i = 0; i < cascades; ++i){
        m_cascades.emplace_back(*this, device, baseCascadeSize / (1u << i));
    }
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::WaveSurfaceTextureCascade(WaveSurfaceTexture &parent,
                                                                         vkw::Device &device, uint32_t cascadeSize):
        WaveSurfaceTextureCascadeImageHandler(device, cascadeSize), TestApp::PrecomputeImage(device, parent, texture()){

}

WaveSurfaceTexture::WaveSurfaceTextureCascadeImageHandler::WaveSurfaceTextureCascadeImageHandler(vkw::Device& device, uint32_t cascadeSize):
        m_surfaceTexture{device.getAllocator(),
                        VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY,
                                                .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                        VK_FORMAT_R8G8B8A8_UNORM,
                        cascadeSize,
                        cascadeSize,
                        1,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT}{

    auto queue = device.getComputeQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

    transferBuffer.begin(0);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_surfaceTexture;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//queue->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_surfaceTexture.arrayLayers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = 0;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, {transitLayout1});

    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = queue->familyIndex();
    transitLayout1.dstQueueFamilyIndex = device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, {transitLayout1});
/*
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = queue->familyIndex();
    transitLayout1.dstQueueFamilyIndex = device.getGraphicsQueue()->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.dstAccessMask = 0;
    transitLayout1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;


    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, {transitLayout1});
*/
    transferBuffer.end();

    queue->submit(transferBuffer);

    queue->waitIdle();

}
