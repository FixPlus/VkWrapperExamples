#include "common/ShadowPass.h"


namespace TestApp{

    ShadowRenderPass::ShadowRenderPass(vkw::Device &device) :
    m_pass{TestApp::ShadowPass(device, VK_FORMAT_D32_SFLOAT)},
m_shadow_proj_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="shadow", .setBindings={
        vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        vkw::DescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
        TestApp::SHADOW_CASCADES_COUNT),
m_shadow_material_layout(device,
        RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="null"}, .rasterizationState=vkw::RasterizationStateCreateInfo{
        VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE, true, 3.0f, 0.0f,
        1.2f}, .depthTestState=vkw::DepthTestStateCreateInfo(VK_COMPARE_OP_LESS,
                                                             VK_TRUE), .maxMaterials=1}),
m_shadow_material(m_shadow_material_layout),
m_shadow_pass_layout(device,
                     RenderEngine::LightingLayout::CreateInfo{.substageDescription={.shaderSubstageName="shadow"}, .pass=m_pass, .subpass=0},
                     1),
m_shadow_pass(m_shadow_pass_layout),
m_shadowCascades{device.getAllocator(),
                 VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                 VK_FORMAT_D32_SFLOAT, 2048,
                 2048, 1, TestApp::SHADOW_CASCADES_COUNT, 1,
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
m_ubo(device,
      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}){
        m_ubo.map();
        m_mapped = m_ubo.mapped().data();
    for(int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i){
        m_shadow_projs.emplace_back(m_shadow_proj_layout, device, m_ubo, i);
    }
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    for (unsigned i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        m_per_cascade_views.emplace_back(vkw::ImageView<vkw::DEPTH, vkw::V2D>{device, m_shadowCascades, m_shadowCascades.format(), i, 1});
    }
#if 0
    auto &shadowMapSampledView = m_shadowCascades.getView<vkw::DepthImageView>(device, m_shadowCascades.format(), 0,
                                                                               TestApp::SHADOW_CASCADES_COUNT, mapping);
#endif
    for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        auto* viewIt = dynamic_cast<vkw::ImageViewVT<vkw::V2D> const*>(m_per_cascade_views.data() + i);
        m_shadowBufs.emplace_back(device, m_pass,
                                  VkExtent2D{2048, 2048},
                                  std::span<vkw::ImageViewVT<vkw::V2D> const*>{&viewIt, 1});
    }
}

    void ShadowRenderPass::execute(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsRecordingState& state) const{

        assert(&buffer == &state.commands() && "Recording state must be created with passed command buffer");

        VkViewport viewport;

        viewport.height = 2048;
        viewport.width = 2048;
        viewport.x = viewport.y = 0.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor;
        scissor.extent.width = 2048;
        scissor.extent.height = 2048;
        scissor.offset.x = 0;
        scissor.offset.y = 0;

        auto value = VkClearValue{};
        value.depthStencil.depth = 1.0f;

        for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
            buffer.beginRenderPass(m_pass, m_shadowBufs.at(i),
                                   m_shadowBufs.at(i).getFullRenderArea(), false,
                                   1, &value);

            buffer.setViewports({&viewport, 1}, 0);
            buffer.setScissors({&scissor, 1}, 0);

            state.setMaterial(m_shadow_material);
            state.setLighting(m_shadow_pass);
            state.setProjection(m_shadow_projs.at(i));

            onPass(state, m_cameras.at(i));

            buffer.endRenderPass();
        }

    }

    void ShadowRenderPass::update(TestApp::ShadowCascadesCamera<TestApp::SHADOW_CASCADES_COUNT> const &camera, glm::vec3 lightDir) {
        lightDir *= -1.0f;
        auto greaterRadius = camera.cascade(TestApp::SHADOW_CASCADES_COUNT - 1).radius;
        for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
            auto cascade = camera.cascade(i);
            auto shadowDepthFactor = 5.0f;
            auto center = cascade.center;
            auto shadowDepth = 2000.0f;
            if (shadowDepth < cascade.radius * shadowDepthFactor)
                shadowDepth = cascade.radius * shadowDepthFactor;

            auto& cam = m_cameras.at(i);
            cam.setLeft(-cascade.radius);
            cam.setRight(cascade.radius);
            cam.setTop(cascade.radius);
            cam.setBottom(-cascade.radius);
            cam.setZNear(0.0f);
            cam.setZFar(shadowDepth);
            cam.update(0.0f);
            cam.lookAt(center - glm::normalize(lightDir) * (shadowDepth - cascade.radius),
                       center,
                       glm::vec3{0.0f, 1.0f, 0.0f});

            glm::mat4 proj = glm::ortho(-cascade.radius, cascade.radius, cascade.radius, -cascade.radius, 0.0f,
                                        shadowDepth/*cascade.radius * shadowDepthFactor*/);
            glm::mat4 lookAt = glm::lookAt(center - glm::normalize(lightDir) * (shadowDepth - cascade.radius),
                                           center,
                                           glm::vec3{0.0f, 1.0f, 0.0f});
            m_mapped->cascades[i] = cam.projection() * cam.cameraSpace();
            m_mapped->splits[i * 4] = cascade.split;
        }
        flush();
    }
}