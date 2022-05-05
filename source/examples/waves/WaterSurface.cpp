#include "WaterSurface.h"
#include <random>



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
        TestApp::PrecomputeImageLayout(device, shaderLoader, RenderEngine::SubstageDescription{.shaderSubstageName="fft", .setBindings={{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}}, 16, 16),
        m_spectrum_precompute_layout(device,
                                     shaderLoader,
                                     RenderEngine::SubstageDescription{
                                        .shaderSubstageName="spectrum",
                                        .setBindings={{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}, {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                                        16,
                                        16),
        m_spectrum_params(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
        m_params_mapped(m_spectrum_params.map()){
    for(int i = 0; i < cascades; ++i){
        m_cascades.emplace_back(*this, m_spectrum_precompute_layout, device, m_spectrum_params, baseCascadeSize / (1u << i)).computeSpectrum();
    }

    *m_params_mapped = spectrumParameters;
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::WaveSurfaceTextureCascade(WaveSurfaceTexture &parent, TestApp::PrecomputeImageLayout& spectrumPrecomputeLayout,
                                                                         vkw::Device &device, vkw::UniformBuffer<SpectrumParameters> const& spectrumParams, uint32_t cascadeSize):
        WaveSurfaceTextureCascadeImageHandler(device, cascadeSize), TestApp::PrecomputeImage(device, parent, texture()),
        m_spectrum_textures(spectrumPrecomputeLayout, spectrumParams, device, cascadeSize),
        m_device(device){
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto& spectraImageView = m_spectrum_textures.getView<vkw::ColorImageView>(device, m_spectrum_textures.format(), 0, 2, mapping);
    set().writeStorageImage(1, spectraImageView, VK_IMAGE_LAYOUT_GENERAL);
}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::computeSpectrum() {
    auto& device = m_device.get();
    auto queue = device.getComputeQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto buffer = vkw::PrimaryCommandBuffer{commandPool};

    buffer.begin(0);
    m_spectrum_textures.dispatch(buffer);
    buffer.end();

    queue->submit(buffer);
    queue->waitIdle();
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
    transferBuffer.end();

    queue->submit(transferBuffer);

    queue->waitIdle();

}

WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::GaussTexture::GaussTexture(vkw::Device &device, uint32_t size):
vkw::ColorImage2D(device.getAllocator(), VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                  VK_FORMAT_R32G32_SFLOAT,
                  size,
                  size,
                  1,
                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT){
    vkw::Buffer<glm::vec2> stageBuf{device, size * size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}};

    auto* mapped = stageBuf.map();

    // Create gaussian noise (normal distribution)

    std::random_device rd{};
    std::mt19937 gen{rd()};

    std::normal_distribution<> d{0.0f,1.0f};

    for(int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            mapped[i * size + j] = glm::vec2{d(gen), d(gen)};
        }
    }

    stageBuf.flush();

    auto transferQueue = device.getTransferQueue();

    auto commandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, transferQueue->familyIndex());
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = *this;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 1;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = 0;

    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, {transitLayout1});

    VkBufferImageCopy copyRegion{};
    copyRegion.imageExtent = VkExtent3D{size, size, 1};
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    transferCommand.copyBufferToImage(stageBuf, *this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {copyRegion});

    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, {transitLayout1});

    transferCommand.end();

    transferQueue->submit(transferCommand);

    transferQueue->waitIdle();
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::SpectrumTextures(TestApp::PrecomputeImageLayout& spectrumPrecomputeLayout, vkw::UniformBuffer<SpectrumParameters> const& spectrumParams, vkw::Device &device, uint32_t cascadeSize)
: vkw::ColorImage2DArray<2>{device.getAllocator(),
                            VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                            VK_FORMAT_R32G32B32A32_SFLOAT,
                            cascadeSize,
                            cascadeSize,
                            1,
                            VK_IMAGE_USAGE_STORAGE_BIT},
  TestApp::PrecomputeImage(device, spectrumPrecomputeLayout, *this),
  m_gauss_texture(device, cascadeSize),
  m_global_params(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
  m_globals_mapped(m_global_params.map()){
    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = *this;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 2;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = 0;

    auto transferQueue = device.getTransferQueue();

    auto commandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, transferQueue->familyIndex());
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

    transferCommand.begin(0);

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, {transitLayout1});

    transferCommand.end();

    transferQueue->submit(transferCommand);

    transferQueue->waitIdle();

    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto& gaussImageView = m_gauss_texture.getView<vkw::ColorImageView>(device, m_gauss_texture.format(), mapping);

    *m_globals_mapped = globalParameters;

    set().writeStorageImage(1, gaussImageView, VK_IMAGE_LAYOUT_GENERAL);
    set().write(2, spectrumParams);
    set().write(3, m_global_params);


}

