#include "WaterSurface.h"
#include <random>


WaterSurface::WaterSurface(vkw::Device &device) : TestApp::Grid(device), RenderEngine::GeometryLayout(device,
                                                                                                      RenderEngine::GeometryLayout::CreateInfo{.vertexInputState = &m_vertexInputStateCreateInfo, .substageDescription={.shaderSubstageName="waves",
                                                                                                              .setBindings = {
                                                                                                                      vkw::DescriptorSetLayoutBinding{
                                                                                                                              0,
                                                                                                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}, .pushConstants={
                                                                                                                      VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=
                                                                                                                      8 *
                                                                                                                      sizeof(float)}}}, .maxGeometries=1}),
                                                  m_geometry(device, *this) {

}

void WaterSurface::preDraw(RenderEngine::GraphicsRecordingState &buffer) {
    buffer.setGeometry(m_geometry);
    buffer.bindPipeline();
}

WaterSurface::Geometry::Geometry(vkw::Device &device, WaterSurface &surface) : RenderEngine::Geometry(surface),
                                                                               m_ubo(device,
                                                                                     VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                               m_ubo_mapped(m_ubo.map()) {
    set().write(0, m_ubo);
}

WaterMaterial::Material::Material(vkw::Device &device, WaterMaterial &waterMaterial) : RenderEngine::Material(
        waterMaterial), m_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU}),
                                                                                       m_mapped(m_buffer.map()) {
    set().write(0, m_buffer);
}

WaveSettings::WaveSettings(TestApp::GUIFrontEnd &gui, WaterSurface &water,
                           std::map<std::string, std::reference_wrapper<WaterMaterial>> materials)
        : TestApp::GridSettings(gui, water, "Waves"),
          m_water(water), m_materials(std::move(materials)) {
    if (m_materials.empty())
        throw std::runtime_error("Cannot create WaveSettings window with empty material map");

    for (auto &material: m_materials) {
        m_materialNames.emplace_back(material.first.c_str());
    }
}

void WaveSettings::onGui() {
    TestApp::GridSettings::onGui();

    if (!ImGui::CollapsingHeader("Surface waves"))
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

    auto &material = pickedMaterial();

    ImGui::ColorEdit4("Deep water color", &material.description.deepWaterColor.x);
    ImGui::SliderFloat("Metallic", &material.description.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &material.description.roughness, 0.0f, 1.0f);
}

WaveSettings::WaveSettings(WaveSettings &&another) noexcept: TestApp::GridSettings(std::move(another)),
                                                             m_water(another.m_water),
                                                             m_pickedMaterial(another.m_pickedMaterial),
                                                             m_materials(std::move(another.m_materials)) {
    for (auto &material: m_materials) {
        m_materialNames.emplace_back(material.first.c_str());
    }
}

WaveSettings &WaveSettings::operator=(WaveSettings &&another) noexcept {
    TestApp::GridSettings::operator=(std::move(another));
    m_water = another.m_water;
    m_materials = std::move(another.m_materials);
    m_pickedMaterial = another.m_pickedMaterial;
    m_materialNames.clear();
    for (auto &material: m_materials) {
        m_materialNames.emplace_back(material.first.c_str());
    }
    return *this;
}

void WaveSurfaceTexture::dispatch(vkw::CommandBuffer &buffer) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;


    for (auto &cascade: m_cascades) {
        cascade.computeSpectrum(buffer);

        m_fft.ftt(buffer, cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          0,
                          mapping),
                  cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          4,
                          mapping));


        m_fft.ftt(buffer, cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          1,
                          mapping),
                  cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          5,
                          mapping));

        m_fft.ftt(buffer, cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          2,
                          mapping),
                  cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          6,
                          mapping));


        m_fft.ftt(buffer, cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          3,
                          mapping),
                  cascade.spectrum().getView<vkw::ColorImageView>(
                          m_device.get(),
                          cascade.spectrum().format(),
                          7,
                          mapping));

        cascade.combineFinalTexture(buffer);
    }
}

WaveSurfaceTexture::WaveSurfaceTexture(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader,
                                       uint32_t baseCascadeSize, uint32_t cascades) :
        m_spectrum_precompute_layout(device,
                                     shaderLoader,
                                     RenderEngine::SubstageDescription{
                                             .shaderSubstageName="spectrum",
                                             .setBindings={{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                                                           {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                           {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                                     16,
                                     16),
        m_spectrum_params(device,
                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
        m_params_mapped(m_spectrum_params.map()),
        m_device(device),
        m_fft(device, shaderLoader),
        m_dyn_spectrum_gen_layout(device, shaderLoader, RenderEngine::SubstageDescription{
                .shaderSubstageName="dynamic_spectrum",
                .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}}, 1),
        m_combiner_layout(device, shaderLoader, RenderEngine::SubstageDescription{
                .shaderSubstageName="combine_water_texture",
                .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}}, 1) {


    for (int i = 0; i < cascades; ++i) {
        m_cascades.emplace_back(*this, m_spectrum_precompute_layout, m_dyn_spectrum_gen_layout, m_combiner_layout,
                                device, m_spectrum_params, baseCascadeSize / (1u << i));
    }

    computeSpectrum();

    *m_params_mapped = spectrumParameters;
}

void WaveSurfaceTexture::computeSpectrum() {
    auto &device = m_device.get();
    auto queue = device.getComputeQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto buffer = vkw::PrimaryCommandBuffer{commandPool};
    buffer.begin(0);

    m_spectrum_precompute_layout.bind(buffer);

    for (auto &cascade: m_cascades)
        cascade.precomputeStaticSpectrum(buffer);

    buffer.end();

    queue->submit(buffer);
    queue->waitIdle();
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::WaveSurfaceTextureCascade(WaveSurfaceTexture &parent,
                                                                         TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
                                                                         RenderEngine::ComputeLayout &dynSpectrumCompute,
                                                                         RenderEngine::ComputeLayout &combinerLayout,
                                                                         vkw::Device &device,
                                                                         vkw::UniformBuffer<SpectrumParameters> const &spectrumParams,
                                                                         uint32_t cascadeSize) :
        WaveSurfaceTextureCascadeImageHandler(device, cascadeSize),
        m_spectrum_textures(spectrumPrecomputeLayout, spectrumParams, device, cascadeSize),
        m_device(device),
        m_layout(spectrumPrecomputeLayout),
        m_dynamic_spectrum(dynSpectrumCompute, m_spectrum_textures, device, cascadeSize),
        cascade_size(cascadeSize),
        m_texture_combiner(combinerLayout, m_dynamic_spectrum, *this, device) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &spectraImageView = m_spectrum_textures.getView<vkw::ColorImageView>(device, m_spectrum_textures.format(), 0,
                                                                              2, mapping);
}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::precomputeStaticSpectrum(vkw::CommandBuffer &buffer) {

    m_spectrum_textures.dispatch(buffer);

}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::computeSpectrum(vkw::CommandBuffer &buffer) {
    m_dynamic_spectrum.dispatch(buffer, cascade_size / 16, cascade_size / 16, 1);

    VkImageMemoryBarrier transitLayout1{};

    transitLayout1.image = m_dynamic_spectrum;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//queue->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_dynamic_spectrum.arrayLayers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              {transitLayout1});

}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::combineFinalTexture(vkw::CommandBuffer &buffer) {
    m_texture_combiner.dispatch(buffer, cascade_size / 16, cascade_size / 16, 1);
}


WaveSurfaceTexture::WaveSurfaceTextureCascadeImageHandler::WaveSurfaceTextureCascadeImageHandler(vkw::Device &device,
                                                                                                 uint32_t cascadeSize) :
        m_surfaceTexture{device.getAllocator(),
                         VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY,
                                 .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                         VK_FORMAT_R32G32B32A32_SFLOAT,
                         cascadeSize,
                         cascadeSize,
                         1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT} {

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

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      {transitLayout1});

    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = queue->familyIndex();
    transitLayout1.dstQueueFamilyIndex = device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                      {transitLayout1});
    transferBuffer.end();

    queue->submit(transferBuffer);

    queue->waitIdle();

}

WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::GaussTexture::GaussTexture(vkw::Device &device,
                                                                                            uint32_t size) :
        vkw::ColorImage2D(device.getAllocator(),
                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                          VK_FORMAT_R32G32_SFLOAT,
                          size,
                          size,
                          1,
                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    vkw::Buffer<glm::vec2> stageBuf{device, size * size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}};

    auto *mapped = stageBuf.map();

    // Create gaussian noise (normal distribution)

    std::random_device rd{};
    std::mt19937 gen{rd()};

    std::normal_distribution<> d{0.0f, 1.0f};

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            mapped[i * size + j] = glm::vec2{d(gen), d(gen)};
        }
    }

    stageBuf.flush();

    auto transferQueue = device.getTransferQueue();

    auto commandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        transferQueue->familyIndex());
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
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       {transitLayout1});

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

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       {transitLayout1});

    transferCommand.end();

    transferQueue->submit(transferCommand);

    transferQueue->waitIdle();
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::SpectrumTextures(
        TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
        vkw::UniformBuffer<SpectrumParameters> const &spectrumParams, vkw::Device &device, uint32_t cascadeSize)
        : vkw::ColorImage2DArray<2>{device.getAllocator(),
                                    VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                                    VK_FORMAT_R32G32B32A32_SFLOAT,
                                    cascadeSize,
                                    cascadeSize,
                                    1,
                                    VK_IMAGE_USAGE_STORAGE_BIT},
          TestApp::PrecomputeImage(device, spectrumPrecomputeLayout, *this),
          m_gauss_texture(device, cascadeSize),
          m_global_params(device,
                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
          m_globals_mapped(m_global_params.map()) {
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

    auto commandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        transferQueue->familyIndex());
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

    transferCommand.begin(0);

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       {transitLayout1});

    transferCommand.end();

    transferQueue->submit(transferCommand);

    transferQueue->waitIdle();

    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &gaussImageView = m_gauss_texture.getView<vkw::ColorImageView>(device, m_gauss_texture.format(), mapping);

    *m_globals_mapped = globalParameters;

    set().writeStorageImage(1, gaussImageView, VK_IMAGE_LAYOUT_GENERAL);
    set().write(2, spectrumParams);
    set().write(3, m_global_params);


}

FFT::FFT(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader) :
        m_permute_row_layout(device,
                             shaderLoader,
                             RenderEngine::SubstageDescription{
                                     .shaderSubstageName="permute_row",
                                     .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                                                   {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}},
                             100),
        m_permute_column_layout(device,
                                shaderLoader,
                                RenderEngine::SubstageDescription{
                                        .shaderSubstageName="permute_column",
                                        .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                                                      {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}},
                                100),
        m_fft_row_layout(device,
                         shaderLoader,
                         RenderEngine::SubstageDescription{
                                 .shaderSubstageName="fft_row",
                                 .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                                               {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, .pushConstants={
                                         VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT, .offset=0, .size=sizeof(int)}}},
                         100),
        m_fft_column_layout(device,
                            shaderLoader,
                            RenderEngine::SubstageDescription{
                                    .shaderSubstageName="fft_column",
                                    .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                                                  {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}, .pushConstants={
                                            VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT, .offset=0, .size=sizeof(int)}}},
                            100) {

}

void FFT::ftt(vkw::CommandBuffer &buffer, const Complex2DTexture &input, const Complex2DTexture &output) {
    size_t x, y;
    x = input.image()->rawExtents().width;
    y = input.image()->rawExtents().height;

    auto key = std::pair<const Complex2DTexture *, const Complex2DTexture *>{&input, &output};
    auto found = m_computes.contains(key);
    auto &comp = found ? m_computes.at(key) : m_computes.emplace(key,
                                                                 std::tuple<PermuteRow, FFTRow, PermuteColumn, FFTColumn>
                                                                         {PermuteRow{m_permute_row_layout, input,
                                                                                     output},
                                                                          FFTRow{m_fft_row_layout, output},
                                                                          PermuteColumn{m_permute_column_layout,
                                                                                        output},
                                                                          FFTColumn{m_fft_column_layout,
                                                                                    output}}).first->second;

    get<0>(comp).dispatch(buffer, x / 16, y / 16, 1);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = *output.image();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = output.baseLayer();
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 1;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              {transitLayout1});
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    uint32_t size = 1;

    auto &fft_row = get<1>(comp);

    while (size <= x) {
        buffer.pushConstants(fft_row.layout().pipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, size);
        fft_row.dispatch(buffer, x / 32, y / 16, 1);
        buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  {transitLayout1});
        size *= 2u;
    }

    get<2>(comp).dispatch(buffer, x / 16, y / 16, 1);

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              {transitLayout1});

    auto &fft_column = get<3>(comp);

    size = 1;

    while (size <= x) {
        buffer.pushConstants(fft_column.layout().pipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, size);
        fft_column.dispatch(buffer, x / 32, y / 16, 1);
        buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  {transitLayout1});
        size *= 2u;
    }

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              {transitLayout1});
}

FFT::PermuteRow::PermuteRow(RenderEngine::ComputeLayout &layout, const Complex2DTexture &input,
                            const Complex2DTexture &output) : RenderEngine::Compute(layout) {
    set().writeStorageImage(0, output);
    set().writeStorageImage(1, input);

}

FFT::FFTRow::FFTRow(RenderEngine::ComputeLayout &layout, const Complex2DTexture &io) : RenderEngine::Compute(layout) {
    set().writeStorageImage(0, io);
}

FFT::PermuteColumn::PermuteColumn(RenderEngine::ComputeLayout &layout, const Complex2DTexture &io)
        : RenderEngine::Compute(layout) {
    set().writeStorageImage(0, io);
}

FFT::FFTColumn::FFTColumn(RenderEngine::ComputeLayout &layout, const Complex2DTexture &io) : RenderEngine::Compute(
        layout) {
    set().writeStorageImage(0, io);
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::DynamicSpectrumTextures::DynamicSpectrumTextures(
        RenderEngine::ComputeLayout &layout, SpectrumTextures &spectrum, vkw::Device &device, uint32_t cascadeSize) :
        vkw::ColorImage2DArray<8>(device.getAllocator(),
                                  VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                                  VK_FORMAT_R32G32_SFLOAT, cascadeSize, cascadeSize, 1, VK_IMAGE_USAGE_STORAGE_BIT),
        RenderEngine::Compute(layout) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &staticSpectrumView = spectrum.getView<vkw::ColorImageView>(device, spectrum.format(), 0, 2, mapping);
    auto &displacementXY = getView<vkw::ColorImageView>(device, format(), 0, mapping);
    auto &displacementZXdx = getView<vkw::ColorImageView>(device, format(), 1, mapping);
    auto &displacementYdxZdx = getView<vkw::ColorImageView>(device, format(), 2, mapping);
    auto &displacementYdzZdz = getView<vkw::ColorImageView>(device, format(), 3, mapping);
    set().writeStorageImage(0, staticSpectrumView);
    set().writeStorageImage(1, displacementXY);
    set().writeStorageImage(2, displacementZXdx);
    set().writeStorageImage(3, displacementYdxZdx);
    set().writeStorageImage(4, displacementYdzZdz);

    auto queue = device.getComputeQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

    transferBuffer.begin(0);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = *this;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//queue->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = arrayLayers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = 0;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      {transitLayout1});

    transferBuffer.end();

    queue->submit(transferBuffer);

    queue->waitIdle();
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::FinalTextureCombiner::FinalTextureCombiner(
        RenderEngine::ComputeLayout &layout, DynamicSpectrumTextures &specTex, WaveSurfaceTextureCascade &final,
        vkw::Device &device) :
        RenderEngine::Compute(layout) {
    VkComponentMapping mappping{};
    mappping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mappping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mappping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mappping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &dis1 = specTex.getView<vkw::ColorImageView>(device, specTex.format(), 4, mappping);
    auto &dis2 = specTex.getView<vkw::ColorImageView>(device, specTex.format(), 5, mappping);
    auto &dis3 = specTex.getView<vkw::ColorImageView>(device, specTex.format(), 6, mappping);
    auto &dis4 = specTex.getView<vkw::ColorImageView>(device, specTex.format(), 7, mappping);
    auto &disp = final.texture().getView<vkw::ColorImageView>(device, final.texture().format(), 0, mappping);
    auto &deriv = final.texture().getView<vkw::ColorImageView>(device, final.texture().format(), 1, mappping);

    set().writeStorageImage(0, dis1);
    set().writeStorageImage(1, dis2);
    set().writeStorageImage(2, dis3);
    set().writeStorageImage(3, dis4);
    set().writeStorageImage(4, disp);
    set().writeStorageImage(5, deriv);

}
