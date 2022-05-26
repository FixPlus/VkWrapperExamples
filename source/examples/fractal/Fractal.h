#ifndef TESTAPP_FRACTAL_H
#define TESTAPP_FRACTAL_H

#include <Sampler.hpp>
#include "RenderEngine/Pipelines/PipelinePool.h"
#include "RenderEngine/RecordingState.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include <glm/glm.hpp>
#include <GUI.h>
#include "Camera.h"

namespace TestApp{

    class Fractal{
    public:
        Fractal(vkw::Device& device, vkw::RenderPass& pass, uint32_t subpass, RenderEngine::TextureLoader& textureLoader);

        void update(TestApp::CameraPerspective const& camera);

        void draw(RenderEngine::GraphicsRecordingState& recorder);

        void switchSamplerMode(){
            m_material.switchSamplerMode();
        }

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

    private:
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
            vkw::ColorImage2D m_texture;
            vkw::Sampler m_sampler;
            vkw::Sampler m_sampler_with_mips;
            bool m_sampler_mode = false;
            std::reference_wrapper<vkw::Device> m_device;
        } m_material;

        RenderEngine::LightingLayout m_lighting_layout;
        RenderEngine::Lighting m_lighting;

    };

    class FractalSettings: public GUIWindow{
    public:
        FractalSettings(GUIFrontEnd &gui, Fractal& fractal);
    protected:
        void onGui() override;
    private:
        bool m_sampler_mode = false;
        std::reference_wrapper<Fractal> m_fractal;
    };
}
#endif //TESTAPP_FRACTAL_H
