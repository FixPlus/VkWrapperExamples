#ifndef TESTAPP_FRACTAL_H
#define TESTAPP_FRACTAL_H

#include <Sampler.hpp>
#include "RenderEngine/Pipelines/PipelinePool.h"
#include "RenderEngine/RecordingState.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include <glm/glm.hpp>
#include <GUI.h>
#include "Camera.h"
#include "vkw/FrameBuffer.hpp"


namespace TestApp{

    class Fractal{
    public:

        using ColorImage2D = vkw::Image<vkw::COLOR, vkw::I2D>;
        Fractal(vkw::Device& device, vkw::RenderPass& pass, uint32_t subpass, RenderEngine::TextureLoader& textureLoader, uint32_t texWidth, uint32_t texHeight);

        void update(TestApp::CameraPerspective const& camera);

        void drawOffscreen(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool);

        void draw(RenderEngine::GraphicsRecordingState& recorder);

        void switchSamplerMode(){
            m_material.switchSamplerMode();
        }

        void resizeOffscreenBuffer(uint32_t width, uint32_t height);

        struct UBO
        {
            glm::mat4 invProjView;
            glm::vec4 cameraPos;
            float near_plane;
            float far_plane;
            glm::vec2 pad;
            glm::vec4 lightPos = glm::vec4{1.0f, 0.0f, 1.0f, 0.0f};
            glm::vec4 params = glm::vec4{1.0f};
            float shadowOption = -1.0f; // < 0 == shadow off ; >= 0 == shadow on
        } ubo;

        struct FilterUBO{
            float distance = 0.0f;
        } filter;

    private:
        class RenderPassAttachments{
        public:
            explicit RenderPassAttachments(vkw::Device& device);

            vkw::AttachmentDescription const& colorAttachment() const{
                return m_attachments.at(0);
            }
            vkw::AttachmentDescription const& depthAttachment() const{
                return m_attachments.at(1);
            }

        private:
            std::array<vkw::AttachmentDescription, 2> m_attachments;
        };

        class RenderPass: public RenderPassAttachments, public vkw::RenderPass{
        public:
            explicit RenderPass(vkw::Device& device);
        private:
            vkw::RenderPassCreateInfo m_compileRenderPassInfo();
        } m_offscreenPass;

        class OffscreenBufferImages{
        public:
            OffscreenBufferImages(vkw::Device& device, uint32_t width, uint32_t height);

            ColorImage2D& colorTarget() {
                return m_colorTarget;
            };

            ColorImage2D& depthTarget() {
                return m_depthTarget;
            }

            std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2>& targetViews(){
                return m_targetRefViews;
            };
        private:
            ColorImage2D m_colorTarget;
            ColorImage2D m_depthTarget;
            std::array<vkw::ImageView<vkw::COLOR, vkw::V2DA>, 2> m_targetViews;
            std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> m_targetRefViews;


        };

        class OffscreenBuffer: public OffscreenBufferImages, public vkw::FrameBuffer{
        public:
            OffscreenBuffer(vkw::Device& device, RenderPass& pass, uint32_t width, uint32_t height);
        };

        std::unique_ptr<OffscreenBuffer> m_offscreenBuffer;

        RenderEngine::GeometryLayout m_geom_layout;
        RenderEngine::Geometry m_geometry;
        RenderEngine::ProjectionLayout m_projection_layout;
        RenderEngine::Projection m_projection;
        RenderEngine::MaterialLayout m_material_layout;

        class FractalMaterial: public RenderEngine::Material{
        public:
            FractalMaterial(RenderEngine::MaterialLayout& parent, vkw::Device& device, RenderEngine::TextureLoader& textureLoader);

            void update(UBO const& ubo);

            void switchSamplerMode();
        private:
            vkw::UniformBuffer<UBO> m_buffer;
            UBO* m_mapped;
            ColorImage2D m_texture;
            vkw::ImageView<vkw::COLOR, vkw::V2D> m_texture_view;
            vkw::Sampler m_sampler;
            vkw::Sampler m_sampler_with_mips;
            bool m_sampler_mode = false;
            std::reference_wrapper<vkw::Device> m_device;
        } m_material;

        RenderEngine::LightingLayout m_offscreen_lighting_layout;
        RenderEngine::Lighting m_offscreen_lighting;

        RenderEngine::LightingLayout m_lighting_layout;
        RenderEngine::Lighting m_lighting;

        RenderEngine::MaterialLayout m_filter_layout;

        class FilterMaterial: public RenderEngine::Material{
        public:
            FilterMaterial(RenderEngine::MaterialLayout& parent, vkw::Device& device, OffscreenBuffer& offscreenBuffer);

            void update(FilterUBO const& ubo);

            void updateTargetViews(OffscreenBuffer& offscreenBuffer);

            void rewriteOffscreenTextures(OffscreenBuffer& offscreenBuffer, vkw::Device& device);

        private:
            vkw::UniformBuffer<FilterUBO> m_buffer;
            FilterUBO* m_mapped;
            vkw::Sampler m_sampler;
            vkw::ImageView<vkw::COLOR, vkw::V2D> m_colorTargetView;
            vkw::ImageView<vkw::COLOR, vkw::V2D> m_depthTargetView;
            std::reference_wrapper<vkw::Device> m_device;
        } m_filter_material;

        std::reference_wrapper<vkw::Device> m_device;
    };

    class FractalSettings: public GUIWindow{
    public:
        FractalSettings(GUIFrontEnd &gui, Fractal& fractal);
    protected:
        void onGui() override;
    private:
        bool m_sampler_mode = false;
        bool m_fxaa_on = false;
        std::reference_wrapper<Fractal> m_fractal;
    };
}
#endif //TESTAPP_FRACTAL_H
