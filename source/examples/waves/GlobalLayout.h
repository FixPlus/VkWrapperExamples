#ifndef TESTAPP_GLOBALLAYOUT_H
#define TESTAPP_GLOBALLAYOUT_H

#include <glm/glm.hpp>
#include <vkw/Device.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <common/Camera.h>
#include "RenderEngine/RecordingState.h"
#include <vkw/Image.hpp>
#include <vkw/Sampler.hpp>
#include "ShadowPass.h"
#include "SkyBox.h"

class GlobalLayout {
public:

    GlobalLayout(vkw::Device &device, vkw::RenderPass& pass, uint32_t subpass, TestApp::Camera const &camera, TestApp::ShadowRenderPass& shadowPass, const SkyBox& skyBox) :
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
                                  {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                     .pass=pass,
                     .subpass=subpass,
                     .blendStates={{m_getBlendState(), 0}}},
                     1),
            m_camera(camera),
            m_light(device, m_light_layout, shadowPass, skyBox),
            m_camera_projection(device, m_camera_projection_layout){
    }

    void bind(RenderEngine::GraphicsRecordingState& state) const{
        state.setProjection(m_camera_projection);
        state.setLighting(m_light);
    }

    void update() {
        m_camera_projection.update(m_camera);
    }

    TestApp::Camera const &camera() const {
        return m_camera.get();
    }

private:

    static VkPipelineColorBlendAttachmentState m_getBlendState(){
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
    RenderEngine::ProjectionLayout m_camera_projection_layout;
    struct CameraProjection: public RenderEngine::Projection{
        struct ProjectionUniform {
            glm::mat4 perspective;
            glm::mat4 cameraSpace;
        } ubo;

        CameraProjection(vkw::Device& device, RenderEngine::ProjectionLayout& layout): RenderEngine::Projection(layout),uniform(device,
                                                                                                           VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                                                           mapped(uniform.map()){
            set().write(0, uniform);
            *mapped = ubo;
            uniform.flush();
        }

        void update(TestApp::Camera const& camera){
            ubo.perspective = camera.projection();
            ubo.cameraSpace = camera.cameraSpace();
            *mapped = ubo;
            uniform.flush();
        }
        vkw::UniformBuffer<ProjectionUniform> uniform;
        ProjectionUniform* mapped;
    } m_camera_projection;

    RenderEngine::LightingLayout m_light_layout;
    struct Light: public RenderEngine::Lighting{


        Light(vkw::Device& device, RenderEngine::LightingLayout& layout, TestApp::ShadowRenderPass& shadowPass, const SkyBox& skyBox): RenderEngine::Lighting(layout),
                                                                                                                                             m_sampler(m_create_sampler(device)){
            set().write(0, skyBox.sunBuffer());
            VkComponentMapping mapping;
            mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            auto& shadowMap = shadowPass.shadowMap();
            set().write(1, shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), 0, shadowMap.arrayLayers(), mapping), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
            set().write(2, shadowPass.ubo());
        }

        vkw::Sampler m_sampler;
    private:
        static vkw::Sampler m_create_sampler(vkw::Device& device);
    }m_light;
    std::reference_wrapper<TestApp::Camera const> m_camera;
};


#endif //TESTAPP_GLOBALLAYOUT_H
