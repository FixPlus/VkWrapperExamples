#ifndef TESTAPP_LIGHTING_H
#define TESTAPP_LIGHTING_H

#include "RenderEngine/Pipelines/PipelineStage.h"
#include "vkw/RenderPass.hpp"

namespace RenderEngine{


    class LightingLayout: public PipelineStageLayout{
    public:
        struct CreateInfo{
            SubstageDescription substageDescription;
            vkw::StrongReference<vkw::RenderPass const> pass;
            uint32_t subpass;
            boost::container::small_vector<std::pair<VkPipelineColorBlendAttachmentState, uint32_t>, 2> blendStates{};
        };
        LightingLayout(vkw::Device& device, ShaderLoaderInterface& loader, CreateInfo const& desc, uint32_t maxSets = 0):
                PipelineStageLayout(device, loader, desc.substageDescription, 3, ".lt.frag", maxSets), m_pass(desc.pass.get()), m_subpass(desc.subpass), m_blend_states(desc.blendStates){}

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
        vkw::StrongReference<vkw::RenderPass const> m_pass;
        uint32_t m_subpass;
        boost::container::small_vector<std::pair<VkPipelineColorBlendAttachmentState, uint32_t>, 2> m_blend_states;
    };

    class GraphicsRecordingState;

    class Lighting: public PipelineStage<LightingLayout>{
    public:
        explicit Lighting(LightingLayout& layout): PipelineStage<LightingLayout>(layout){};

        virtual void bind(GraphicsRecordingState& state) const;

    };
}

#endif //TESTAPP_LIGHTING_H
