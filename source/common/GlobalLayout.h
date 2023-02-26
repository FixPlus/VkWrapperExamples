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
#include "common/GUI.h"

namespace TestApp {
    class GlobalLayout : public vkw::ReferenceGuard{
    public:

        GlobalLayout(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, TestApp::Camera const &camera,
                     TestApp::ShadowRenderPass &shadowPass, const SkyBox &skyBox);

        void bind(RenderEngine::GraphicsRecordingState &state, bool useSimpleLighting = false) const {
            state.setProjection(m_camera_projection);
            state.setLighting(useSimpleLighting ? static_cast<RenderEngine::Lighting const&>(m_simple_light) : m_light);
        }

        void update() {
            m_camera_projection.update(m_camera);
        }

        TestApp::Camera const &camera() const {
            return m_camera.get();
        }

    private:

        static VkPipelineColorBlendAttachmentState m_getBlendState();

        RenderEngine::ProjectionLayout m_camera_projection_layout;

        struct CameraProjection : public RenderEngine::Projection {
            struct ProjectionUniform {
                glm::mat4 perspective;
                glm::mat4 cameraSpace;
            } ubo;

            CameraProjection(vkw::Device &device, RenderEngine::ProjectionLayout &layout);

            void update(TestApp::Camera const &camera) {
                ubo.perspective = camera.projection();
                ubo.cameraSpace = camera.cameraSpace();
                *mapped = ubo;
                uniform.flush();
            }

            vkw::UniformBuffer<ProjectionUniform> uniform;
            ProjectionUniform *mapped;
        } m_camera_projection;

        RenderEngine::LightingLayout m_light_layout;
        RenderEngine::LightingLayout m_simple_light_layout;

        struct Light : public RenderEngine::Lighting {

            Light(vkw::Device &device, RenderEngine::LightingLayout &layout, TestApp::ShadowRenderPass &shadowPass,
                  const SkyBox &skyBox);

            vkw::ImageView<vkw::DEPTH, vkw::V2DA> m_shadow_map;
            vkw::Sampler m_sampler;
        private:
            static vkw::Sampler m_create_sampler(vkw::Device &device);
        } m_light;

        struct SimpleLight : public RenderEngine::Lighting {
            SimpleLight(vkw::Device &device, RenderEngine::LightingLayout &layout, const SkyBox &skyBox);
        } m_simple_light;

        // TODO: rewrite to StrongReference
        std::reference_wrapper<TestApp::Camera const> m_camera;
    };

    class GlobalLayoutSettings : public TestApp::GUIWindow{
    public:
        GlobalLayoutSettings(TestApp::GUIFrontEnd& gui, GlobalLayout& globalLayout);

        bool useSimpleLighting() const{
            return m_use_simple_lighting;
        }

        void onGui() override;
    private:
        bool m_use_simple_lighting = false;
        vkw::StrongReference<GlobalLayout> m_globalLayout;
    };

}
#endif //TESTAPP_GLOBALLAYOUT_H
