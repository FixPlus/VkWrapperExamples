#ifndef TESTAPP_LIGHTING_H
#define TESTAPP_LIGHTING_H

#include "RenderEngine/Pipelines/PipelineStage.h"

namespace RenderEngine{


    class LightingLayout: public PipelineStageLayout{
    public:
        LightingLayout(vkw::Device& device, SubstageDescription const& desc, vkw::RenderPass const& pass, uint32_t subpass, uint32_t maxSets = 0):
                PipelineStageLayout(device, desc, maxSets), m_pass(pass), m_subpass(subpass){}

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
