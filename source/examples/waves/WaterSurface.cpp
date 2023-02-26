#include "WaterSurface.h"
#include <random>


WaterSurface::WaterSurface(vkw::Device &device, WaveSurfaceTexture &texture) : TestApp::Grid(device),
                                                                               RenderEngine::GeometryLayout(device,
                                                                                                            RenderEngine::GeometryLayout::CreateInfo{.vertexInputState = &m_vertexInputStateCreateInfo, .substageDescription={.shaderSubstageName="waves",
                                                                                                                    .setBindings = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                                                                                                    {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                                                                                    {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                                                                                    {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}, .pushConstants={
                                                                                                                            VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=
                                                                                                                            8 *
                                                                                                                            sizeof(float)}}}, .maxGeometries=1}),
                                                                               m_geometry(device, *this, texture) {

}

void WaterSurface::preDraw(RenderEngine::GraphicsRecordingState &buffer) {
    buffer.setGeometry(m_geometry);
    buffer.bindPipeline();
}

WaterSurface::Geometry::Geometry(vkw::Device &device, WaterSurface &surface, WaveSurfaceTexture &texture)
        : RenderEngine::Geometry(surface),
          m_sampler(m_sampler_create(device)){
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    set().write(0, texture.scalesBuffer());

    auto cascadeCount = texture.cascadesCount();
    for(int i = 0; i < cascadeCount; ++i){
        auto &cascade = texture.cascade(i);
        auto &displacement = m_displacements_view.emplace_back(device, cascade, cascade.format());
        //auto &derivatives = cascade.getView<vkw::ColorImageView>(device, cascade.format(), 1, mapping);
        set().write(i + 1, displacement, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
    }



}

vkw::Sampler WaterSurface::Geometry::m_sampler_create(vkw::Device &device) {
    VkSamplerCreateInfo createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    if(device.physicalDevice().enabledFeatures().samplerAnisotropy) {
        createInfo.anisotropyEnable = true;
        createInfo.maxAnisotropy = device.physicalDevice().properties().limits.maxSamplerAnisotropy;
    }
    createInfo.minLod = 0.0f;
    createInfo.maxLod = 1.0f;


    return {device, createInfo};
}
WaterMaterial::WaterMaterial(vkw::Device &device, WaveSurfaceTexture &texture, bool wireframe)
: RenderEngine::MaterialLayout(device,
        RenderEngine::MaterialLayout::CreateInfo{RenderEngine::SubstageDescription{"water", {
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        {1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        {4,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        {5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        {6,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        {7,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}},
                                                 {
        VK_FALSE,
        VK_FALSE,
        wireframe
        ? VK_POLYGON_MODE_LINE
        : VK_POLYGON_MODE_FILL,VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE}, vkw::DepthTestStateCreateInfo{
        VK_COMPARE_OP_LESS,
        true}, 1}),
m_material(device, *this, texture) {

};

WaterMaterial::Material::Material(vkw::Device &device, WaterMaterial &waterMaterial, WaveSurfaceTexture &texture)
        : RenderEngine::Material(
        waterMaterial), m_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU}),
          m_sampler(m_sampler_create(device)) {
    m_buffer.map();
    m_mapped = m_buffer.mapped().data();
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;


    set().write(0, m_buffer);
    set().write(1, texture.scalesBuffer());
    auto cascadeCount = texture.cascadesCount();
    for(int i = 0; i < cascadeCount; ++i) {
        auto &tex = texture.cascade(i);
        auto& [derivatives, turbulence] = m_derivTurbViews.emplace_back(vkw::ImageView<vkw::COLOR, vkw::V2D>{device, tex, tex.format(), 1},vkw::ImageView<vkw::COLOR, vkw::V2D>{device, tex, tex.format(), 2});
        set().write(2 + i, derivatives, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
        set().write(2 + cascadeCount + i, turbulence, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
    }
}

WaveSettings::WaveSettings(TestApp::GUIFrontEnd &gui, WaterSurface &water, WaveSurfaceTexture &texture,
                           std::map<std::string, vkw::StrongReference<WaterMaterial>> materials)
        : TestApp::GridSettings(gui, water, "Waves"),
          m_water(water), m_materials(std::move(materials)), m_texture(texture) {
    if (m_materials.empty())
        throw std::runtime_error("Cannot create WaveSettings window with empty material map");

    for (auto &material: m_materials) {
        m_materialNames.emplace_back(material.first.c_str());
    }

    m_calculate_alpha();
    m_calculate_peak_frequency();
}

void WaveSettings::onGui() {
    TestApp::GridSettings::onGui();

    if (!ImGui::CollapsingHeader("Surface waves"))
        return;
    ImGui::SliderFloat("Stretch", &m_texture.get().dynamicSpectrumParams.stretch, 0.0f, 10.0f);

    m_need_update_static_spectrum =
            ImGui::SliderFloat("Wind Direction", &m_texture.get().spectrumParameters.angle, 0.0f,
                               2.0f * glm::pi<float>()) || m_need_update_static_spectrum;
    m_need_update_static_spectrum =
            ImGui::SliderFloat("Gravity", &m_texture.get().spectrumParameters.GravityAcceleration, 0.0f, 100.0f) ||
            m_need_update_static_spectrum;

    m_need_update_static_spectrum =
            ImGui::SliderFloat("Fetch", &m_fetch, 0.0f, 100.0f) || m_need_update_static_spectrum;
    m_need_update_static_spectrum =
            ImGui::SliderFloat("WindSpeed", &m_wind_speed, 0.0f, 100.0f) || m_need_update_static_spectrum;
    m_need_update_static_spectrum =
            ImGui::SliderFloat("Height scale", &m_texture.get().spectrumParameters.scale, 0.0f, 100.0f) ||
            m_need_update_static_spectrum;
    m_need_update_static_spectrum =
            ImGui::SliderFloat("Gamma", &m_texture.get().spectrumParameters.gamma, 0.0f, 100.0f) ||
            m_need_update_static_spectrum;
    m_need_update_static_spectrum =
            ImGui::SliderFloat("Spread blend", &m_texture.get().spectrumParameters.spreadBlend, 0.0f, 1.0f) ||
            m_need_update_static_spectrum;
    m_need_update_static_spectrum =
            ImGui::SliderFloat("Swell", &m_texture.get().spectrumParameters.swell, 0.0f, 100.0f) ||
            m_need_update_static_spectrum;
#if 0
    m_need_update_static_spectrum = ImGui::SliderFloat("Alpha", &m_texture.get().spectrumParameters.alpha, 0.0f,
                                                       100.0f) ||
                                    m_need_update_static_spectrum;

    m_need_update_static_spectrum = ImGui::SliderFloat("Peak omega", &m_texture.get().spectrumParameters.peakOmega,
                                                       0.0f, 100.0f) ||
                                    m_need_update_static_spectrum;
#endif
    m_need_update_static_spectrum = ImGui::SliderFloat("Short waves fade",
                                                       &m_texture.get().spectrumParameters.shortWavesFade, 0.0f,
                                                       1.0f) ||
                                    m_need_update_static_spectrum;
    m_need_update_static_spectrum = ImGui::SliderFloat("Depth",
                                                       &m_texture.get().spectrumParameters.Depth, 1.0f,
                                                       1000.0f) ||
                                    m_need_update_static_spectrum;
    ImGui::Text("alpha: %.3f, peak omega %.3f", m_texture.get().spectrumParameters.alpha, m_texture.get().spectrumParameters.peakOmega);

    for (int i = 0; i < m_texture.get().cascadesCount(); ++i) {
        if (!ImGui::TreeNode(("Cascade" + std::to_string(i)).c_str()))
            continue;
        auto &params = m_texture.get().cascadeParams(i);

        m_need_update_static_spectrum =
                ImGui::SliderFloat("Cutoff low", &params.CutoffLow, 0.0f, 1000.0f) || m_need_update_static_spectrum;
        m_need_update_static_spectrum = ImGui::SliderFloat("Cutoff high", &params.CutoffHigh, 200.0f, 10000.0f) ||
                                        m_need_update_static_spectrum;
        if (ImGui::SliderFloat("Scale", &params.LengthScale, 10.0f, 1000.0f)) {
            *(&m_texture.get().ubo.scale.x + i) = params.LengthScale;
            m_need_update_static_spectrum = true;
        }

        ImGui::TreePop();
    }

    if (m_need_update_static_spectrum) {
        m_calculate_alpha();
        m_calculate_peak_frequency();
    }

    ImGui::Combo("Materials", &m_pickedMaterial, m_materialNames.data(), m_materialNames.size());

    auto &material = pickedMaterial();

    ImGui::ColorEdit4("Deep water color", &material.description.deepWaterColor.x);
    ImGui::SliderFloat("Metallic", &material.description.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &material.description.roughness, 0.0f, 1.0f);
}

WaveSettings::WaveSettings(WaveSettings &&another) noexcept: TestApp::GridSettings(std::move(another)),
                                                             m_water(another.m_water),
                                                             m_pickedMaterial(another.m_pickedMaterial),
                                                             m_materials(std::move(another.m_materials)),
                                                             m_texture(another.m_texture) {
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

void WaveSettings::m_calculate_alpha() {
    m_texture.get().spectrumParameters.alpha = 0.076f * glm::pow(
            m_texture.get().spectrumParameters.GravityAcceleration * m_fetch / m_wind_speed / m_wind_speed, -0.22f);
}

void WaveSettings::m_calculate_peak_frequency() {
    auto g = m_texture.get().spectrumParameters.GravityAcceleration;
    m_texture.get().spectrumParameters.peakOmega = 22.0f * glm::pow(m_wind_speed * m_fetch / g / g, -0.33f);
}

void WaveSurfaceTexture::dispatch(vkw::CommandBuffer &buffer) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;


    for (auto &cascade: m_cascades) {
        cascade.computeSpectrum(buffer);

        m_fft.ftt(buffer, cascade.spectrum().displacementView(0),
                  cascade.spectrum().displacementView(4));


        m_fft.ftt(buffer, cascade.spectrum().displacementView(1),
                  cascade.spectrum().displacementView(5));

        m_fft.ftt(buffer, cascade.spectrum().displacementView(2),
                  cascade.spectrum().displacementView(6));


        m_fft.ftt(buffer, cascade.spectrum().displacementView(3),
                  cascade.spectrum().displacementView(7));

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
        m_device(device),
        m_fft(device, shaderLoader),
        m_dyn_spectrum_gen_layout(device, shaderLoader, RenderEngine::SubstageDescription{
                .shaderSubstageName="dynamic_spectrum",
                .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, cascades),
        m_combiner_layout(device, shaderLoader, RenderEngine::SubstageDescription{
                .shaderSubstageName="combine_water_texture",
                .setBindings={{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                              {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}}, cascades),
        m_dyn_params(device,
                     VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
        m_scales_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}){

    m_dyn_params.map();
    m_dyn_params_mapped = m_dyn_params.mapped().data();
    m_scales_buffer.map();
    m_scales_buffer_mapped = m_scales_buffer.mapped().data();
    m_spectrum_params.map();
    m_params_mapped = m_spectrum_params.mapped().data();
    m_cascades.reserve(cascades);

    float cascadeSizes[3] = {491.0f, 41.0f, 11.0f};
    float cutoffsLow[3] = {0.01f, 3.0f, 7.0f};
    float cutoffsHigh[3] = {3.0f, 7.0f, 300.0f};
    for (int i = 0; i < cascades; ++i) {

        auto& cascade = m_cascades.emplace_back(*this, m_spectrum_precompute_layout, m_dyn_spectrum_gen_layout, m_combiner_layout,
                                device, m_spectrum_params, m_dyn_params, baseCascadeSize, cascadeSizes[i]);
        *(&ubo.scale.x + i) = cascade.params().LengthScale;
        cascade.params().CutoffHigh = cutoffsHigh[i];
        cascade.params().CutoffLow = cutoffsLow[i];


    }

    computeSpectrum();


}

void WaveSurfaceTexture::computeSpectrum() {
    *m_params_mapped = spectrumParameters;
    m_spectrum_params.flush();

    auto &device = m_device.get();
    auto queue = device.getSpecificQueue(TestApp::dedicatedCompute());
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto buffer = vkw::PrimaryCommandBuffer{commandPool};
    buffer.begin(0);

    m_spectrum_precompute_layout.bind(buffer);

    for (auto &cascade: m_cascades)
        cascade.precomputeStaticSpectrum(buffer);

    buffer.end();

    queue.submit(buffer);
    queue.waitIdle();
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::WaveSurfaceTextureCascade(WaveSurfaceTexture &parent,
                                                                         TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
                                                                         RenderEngine::ComputeLayout &dynSpectrumCompute,
                                                                         RenderEngine::ComputeLayout &combinerLayout,
                                                                         vkw::Device &device,
                                                                         vkw::UniformBuffer<SpectrumParameters> const &spectrumParams,
                                                                         vkw::UniformBuffer<DynamicSpectrumParams> const &dynParams,
                                                                         uint32_t cascadeSize, float cascadeScale) :
        WaveSurfaceTextureCascadeImageHandler(device, cascadeSize),
        m_spectrum_textures(spectrumPrecomputeLayout, spectrumParams, device, cascadeSize, cascadeScale),
        m_device(device),
        m_layout(spectrumPrecomputeLayout),
        m_dynamic_spectrum(dynSpectrumCompute, m_spectrum_textures, dynParams, device, cascadeSize),
        cascade_size(cascadeSize),
        m_texture_combiner(combinerLayout, m_dynamic_spectrum, *this, device) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &spectraImageView = m_spectrum_textures.view();
}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::precomputeStaticSpectrum(vkw::CommandBuffer &buffer) {

    m_spectrum_textures.update();
    m_spectrum_textures.dispatch(buffer);

}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::computeSpectrum(vkw::CommandBuffer &buffer) {
    m_dynamic_spectrum.dispatch(buffer, cascade_size / 16, cascade_size / 16, 1);

    VkImageMemoryBarrier transitLayout1{};

    transitLayout1.image = m_dynamic_spectrum.vkw::AllocatedImage::operator VkImage_T *();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//queue->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_dynamic_spectrum.layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              {&transitLayout1, 1});

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
                         3,
                         1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT},
        m_views({vkw::ImageView<vkw::COLOR, vkw::V2D>{device, m_surfaceTexture, m_surfaceTexture.format(), 0u, 1u},
                 {device, m_surfaceTexture, m_surfaceTexture.format(), 1u, 1u},
                 {device, m_surfaceTexture, m_surfaceTexture.format(), 2u, 1u}}){

    auto queue = device.getSpecificQueue(TestApp::dedicatedCompute());
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

    transferBuffer.begin(0);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_surfaceTexture.vkw::AllocatedImage::operator VkImage_T *();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//queue->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_surfaceTexture.layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = 0;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      {&transitLayout1, 1});

    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = queue.family().index();
    transitLayout1.dstQueueFamilyIndex = device.anyGraphicsQueue().family().index();
    transitLayout1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                      {&transitLayout1, 1});
    transferBuffer.end();

    queue.submit(transferBuffer);

    queue.waitIdle();

}

WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::GaussTexture::GaussTexture(vkw::Device &device,
                                                                                            uint32_t size) :
        vkw::Image<vkw::COLOR, vkw::I2D>(device.getAllocator(),
                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                          VK_FORMAT_R32G32_SFLOAT,
                          size,
                          size,
                          1,
                          1,
                          1,
                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        m_view(device, *this, format()){
    vkw::Buffer<glm::vec2> stageBuf{device, size * size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}};

    stageBuf.map();
    auto *mapped = stageBuf.mapped().data();

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

    auto transferQueue = device.getSpecificQueue(TestApp::dedicatedTransfer());

    auto commandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        transferQueue.family().index());
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
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
                                       {&transitLayout1, 1});

    VkBufferImageCopy copyRegion{};
    copyRegion.imageExtent = VkExtent3D{size, size, 1};
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    transferCommand.copyBufferToImage(stageBuf, *this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {&copyRegion, 1});

    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       {&transitLayout1, 1});

    transferCommand.end();

    transferQueue.submit(transferCommand);

    transferQueue.waitIdle();
}

WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::SpectrumTextures(
        TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
        vkw::UniformBuffer<SpectrumParameters> const &spectrumParams, vkw::Device &device, uint32_t cascadeSize, float cascadeScale)
        : vkw::Image<vkw::COLOR, vkw::I2D, vkw::ARRAY>{device.getAllocator(),
                                    VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                                    VK_FORMAT_R32G32B32A32_SFLOAT,
                                    cascadeSize,
                                    cascadeSize,
                                    1,
                                    2,
                                    1,
                                    VK_IMAGE_USAGE_STORAGE_BIT},
          TestApp::PrecomputeImage(device, spectrumPrecomputeLayout, *this),
          m_gauss_texture(device, cascadeSize),
          m_global_params(device,
                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
          m_view(device, *this, format(), 0u, 2u){
    m_global_params.map();
    m_globals_mapped = m_global_params.mapped().data();
    globalParameters.Size = cascadeSize;
    globalParameters.LengthScale = (float)cascadeScale;
    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
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

    auto transferQueue = device.getSpecificQueue(TestApp::dedicatedTransfer());

    auto commandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        transferQueue.family().index());
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

    transferCommand.begin(0);

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       {&transitLayout1, 1});

    transferCommand.end();

    transferQueue.submit(transferCommand);

    transferQueue.waitIdle();

    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    *m_globals_mapped = globalParameters;

    set().writeStorageImage(1, m_gauss_texture.view(), VK_IMAGE_LAYOUT_GENERAL);
    set().write(2, spectrumParams);
    set().write(3, m_global_params);


}

void WaveSurfaceTexture::WaveSurfaceTextureCascade::SpectrumTextures::update() {
    *m_globals_mapped = globalParameters;
    m_global_params.flush();
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
                              {&transitLayout1, 1});
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    uint32_t size = 1;

    auto &fft_row = get<1>(comp);

    while (size <= x) {
        buffer.pushConstants(fft_row.layout().pipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, size);
        fft_row.dispatch(buffer, x / 32, y / 16, 1);
        buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  {&transitLayout1, 1});
        size *= 2u;
    }

    get<2>(comp).dispatch(buffer, x / 16, y / 16, 1);

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              {&transitLayout1, 1});

    auto &fft_column = get<3>(comp);

    size = 1;

    while (size <= x) {
        buffer.pushConstants(fft_column.layout().pipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, size);
        fft_column.dispatch(buffer, x / 32, y / 16, 1);
        buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  {&transitLayout1, 1});
        size *= 2u;
    }

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              {&transitLayout1, 1});
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
        RenderEngine::ComputeLayout &layout, SpectrumTextures &spectrum,
        vkw::UniformBuffer<DynamicSpectrumParams> const &params, vkw::Device &device, uint32_t cascadeSize) :
        vkw::Image<vkw::COLOR, vkw::I2D, vkw::ARRAY>(device.getAllocator(),
                                  VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                                  VK_FORMAT_R32G32_SFLOAT, cascadeSize, cascadeSize, 1, 8, 1, VK_IMAGE_USAGE_STORAGE_BIT),
        RenderEngine::Compute(layout),
        staticSpectrumView(device, spectrum, spectrum.format(), 0, 2),
        displacementXY(device, *this, format(), 0),
        displacementZXdx(device, *this, format(), 1),
        displacementYdxZdx(device, *this, format(), 2),
        displacementYdzZdz(device, *this, format(), 3),
        m_displacementViews{
                vkw::ImageView<vkw::COLOR, vkw::V2D>{device, *this, format(), 0},
                {device, *this, format(), 1},
                {device, *this, format(), 2},
                {device, *this, format(), 3},
                {device, *this, format(), 4},
                {device, *this, format(), 5},
                {device, *this, format(), 6},
                {device, *this, format(), 7},
        }
{
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    set().writeStorageImage(0, staticSpectrumView);
    set().writeStorageImage(1, displacementXY);
    set().writeStorageImage(2, displacementZXdx);
    set().writeStorageImage(3, displacementYdxZdx);
    set().writeStorageImage(4, displacementYdzZdz);
    set().write(5, params);

    auto queue = device.getSpecificQueue(TestApp::dedicatedCompute());
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

    transferBuffer.begin(0);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//device.getGraphicsQueue()->familyIndex();
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//queue->familyIndex();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transitLayout1.srcAccessMask = 0;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      {&transitLayout1, 1});

    transferBuffer.end();

    queue.submit(transferBuffer);

    queue.waitIdle();
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

    auto &dis1 = specTex.displacementView(4);
    auto &dis2 = specTex.displacementView(5);
    auto &dis3 = specTex.displacementView(6);
    auto &dis4 = specTex.displacementView(7);

    auto &disp = final.view(0);
    auto &deriv = final.view(1);
    auto &turbulence = final.view(2);

    set().writeStorageImage(0, dis1);
    set().writeStorageImage(1, dis2);
    set().writeStorageImage(2, dis3);
    set().writeStorageImage(3, dis4);
    set().writeStorageImage(4, disp);
    set().writeStorageImage(5, deriv);
    set().writeStorageImage(6, turbulence);

}

vkw::Sampler WaterMaterial::Material::m_sampler_create(vkw::Device &device) {
    VkSamplerCreateInfo createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    if(device.physicalDevice().enabledFeatures().samplerAnisotropy) {
        createInfo.anisotropyEnable = true;
        createInfo.maxAnisotropy = device.physicalDevice().properties().limits.maxSamplerAnisotropy;
    }
    createInfo.minLod = 0.0f;
    createInfo.maxLod = 1.0f;


    return {device, createInfo};
}
