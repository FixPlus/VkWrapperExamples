#include "GlobalLayout.h"

namespace TestApp {
    GlobalLayout::GlobalLayout(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                               TestApp::Camera const &camera, TestApp::ShadowRenderPass &shadowPass,
                               const SkyBox &skyBox) :
            m_camera_projection_layout(device,
                                       RenderEngine::SubstageDescription{
                                               .shaderSubstageName="perspective",
                                               .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                                       1),
            m_light_layout(device,
                           RenderEngine::LightingLayout::CreateInfo{
                                   .substageDescription=RenderEngine::SubstageDescription{
                                           .shaderSubstageName="sunlightShadow",
                                           .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                         {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                         {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                         {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                         {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}},
                                   .pass=pass,
                                   .subpass=subpass
#if 1
                                   ,
                                   .blendStates={{m_getBlendState(), 0}}
#endif
                           },
                           1),
            m_camera(camera),
            m_light(device, m_light_layout, shadowPass, skyBox),
            m_camera_projection(device, m_camera_projection_layout),
            m_simple_light_layout(device, RenderEngine::LightingLayout::CreateInfo{
                .substageDescription=RenderEngine::SubstageDescription{
                    .shaderSubstageName="sunlightSimple",
                    .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}
                },
                .pass=pass,
                .subpass=subpass
            }, 1),
            m_simple_light(device, m_simple_light_layout, skyBox){}

    vkw::Sampler GlobalLayout::Light::m_create_sampler(vkw::Device &device) {
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.minFilter = VK_FILTER_LINEAR;
        createInfo.magFilter = VK_FILTER_LINEAR;
        createInfo.minLod = 0.0f;
        createInfo.maxLod = 1.0f;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        createInfo.anisotropyEnable = false;

        return vkw::Sampler(device, createInfo);
    }


    GlobalLayout::Light::Light(vkw::Device &device, RenderEngine::LightingLayout &layout,
                               TestApp::ShadowRenderPass &shadowPass, const SkyBox &skyBox) : RenderEngine::Lighting(
            layout),
                                                                                              m_sampler(
                                                                                                      m_create_sampler(
                                                                                                              device)) {
        set().write(0, skyBox.sunBuffer());
        VkComponentMapping mapping;
        mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        auto &shadowMap = shadowPass.shadowMap();
        set().write(1, shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), 0, shadowMap.arrayLayers(),
                                                              mapping), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    m_sampler);
        set().write(2, shadowPass.ubo());
        set().write(3, skyBox.atmoBuffer());
        set().write(4, skyBox.outScatterTexture(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
    }

    VkPipelineColorBlendAttachmentState GlobalLayout::m_getBlendState() {
        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable = VK_TRUE;
        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;
        state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.colorBlendOp = VK_BLEND_OP_ADD;
        state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        state.alphaBlendOp = VK_BLEND_OP_ADD;
        return state;
    }

    GlobalLayout::CameraProjection::CameraProjection(vkw::Device &device, RenderEngine::ProjectionLayout &layout) : RenderEngine::Projection(
    layout), uniform(device,
                     VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
    mapped(uniform.map()) {
        set().write(0, uniform);
        *mapped = ubo;
        uniform.flush();
    }
    GlobalLayout::SimpleLight::SimpleLight(vkw::Device &device, RenderEngine::LightingLayout &layout,
                                           const SkyBox &skyBox) :
            RenderEngine::Lighting(layout) {
        set().write(0, skyBox.sunBuffer());
    }

    GlobalLayoutSettings::GlobalLayoutSettings(TestApp::GUIFrontEnd& gui, GlobalLayout& globalLayout):
            TestApp::GUIWindow(gui, WindowSettings{.title="Global layout"}),
            m_globalLayout(globalLayout){

    }

    void GlobalLayoutSettings::onGui() {
        ImGui::Checkbox("Simple shading", &m_use_simple_lighting);
    }
}