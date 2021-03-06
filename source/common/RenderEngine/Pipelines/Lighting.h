#ifndef TESTAPP_LIGHTING_H
#define TESTAPP_LIGHTING_H

#include "RenderEngine/Pipelines/PipelineStage.h"

namespace RenderEngine{


    class LightingLayout: public PipelineStageLayout{
    public:
        struct CreateInfo{
            SubstageDescription substageDescription;
            vkw::RenderPassCRef pass;
            uint32_t subpass;
            std::vector<std::pair<VkPipelineColorBlendAttachmentState, uint32_t>> blendStates{};
        };
        LightingLayout(vkw::Device& device, CreateInfo const& desc, uint32_t maxSets = 0):
                PipelineStageLayout(device, desc.substageDescription, maxSets), m_pass(desc.pass), m_subpass(desc.subpass), m_blend_states(desc.blendStates){}

        auto const& blendStates() const{
            return m_blend_states;
        }
        vkw::RenderPass const& pass() const{
            return m_pass;
        }

        uint32_t subpass() const{
            return m_subpass;
        }
    protected:
        vkw::RenderPassCRef m_pass;
        uint32_t m_subpass;
        std::vector<std::pair<VkPipelineColorBlendAttachmentState, uint32_t>> m_blend_states;
    };

    class GraphicsRecordingState;

    class Lighting: public PipelineStage<LightingLayout>{
    public:
        explicit Lighting(LightingLayout& layout): PipelineStage<LightingLayout>(layout){};

        virtual void bind(GraphicsRecordingState& state) const;

    };
}

#endif //TESTAPP_LIGHTING_H
