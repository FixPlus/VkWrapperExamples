#ifndef TESTAPP_SHADOWPASS_H
#define TESTAPP_SHADOWPASS_H

#include <glm/glm.hpp>
#include <RenderEngine/RecordingState.h>
#include <SceneProjector.h>
#include <RenderPassesImpl.h>
#include <vkw/FrameBuffer.hpp>

namespace TestApp{

    class ShadowRenderPass {
    public:
        struct ShadowMapSpace {
            glm::mat4 cascades[TestApp::SHADOW_CASCADES_COUNT];
            float splits[TestApp::SHADOW_CASCADES_COUNT * 4];
        } shadowMapSpace;

        ShadowRenderPass(vkw::Device &device);


        void execute(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsRecordingState& state) const;

        auto& shadowMap() {
            return m_shadowCascades;
        }

        std::function<void(RenderEngine::GraphicsRecordingState& state, const Camera& camera)> onPass = [](RenderEngine::GraphicsRecordingState& state, const Camera& camera){};

        template<uint32_t Cascades>
        void update(TestApp::ShadowCascadesCamera<Cascades> const &camera, glm::vec3 lightDir) {
            lightDir *= -1.0f;
            auto greaterRadius = camera.cascade(Cascades - 1).radius;
            for (int i = 0; i < Cascades; ++i) {
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

        auto& ubo() const{
            return m_ubo;
        }


    private:

        void flush() {
            m_ubo.flush();
        }
        RenderEngine::ProjectionLayout m_shadow_proj_layout;

        std::array<CameraOrtho, 4> m_cameras;

        struct ShadowProjection : public RenderEngine::Projection {
            ShadowProjection(RenderEngine::ProjectionLayout &layout, vkw::Device &device, vkw::UniformBuffer<ShadowMapSpace> const &ubo, int id) :
                    RenderEngine::Projection(layout),
                    m_ubo(device,
                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                    m_mapped(m_ubo.map()) {
                *m_mapped = id;
                set().write(0, ubo);
                set().write(1, m_ubo);
            }


        private:
            vkw::UniformBuffer<int> m_ubo;
            int *m_mapped;
        };

        std::vector<ShadowProjection> m_shadow_projs;

        vkw::UniformBuffer<ShadowMapSpace> m_ubo;
        ShadowMapSpace* m_mapped;
        TestApp::ShadowPass m_pass;
        RenderEngine::MaterialLayout m_shadow_material_layout;
        RenderEngine::Material m_shadow_material;
        RenderEngine::LightingLayout m_shadow_pass_layout;
        RenderEngine::Lighting m_shadow_pass;

        vkw::DepthStencilImage2DArray m_shadowCascades;
        std::vector<vkw::FrameBuffer> m_shadowBufs;
    };

}
#endif //TESTAPP_SHADOWPASS_H
