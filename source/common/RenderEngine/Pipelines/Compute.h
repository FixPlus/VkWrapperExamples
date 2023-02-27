#ifndef TESTAPP_COMPUTE_H
#define TESTAPP_COMPUTE_H

#include <vkw/Pipeline.hpp>
#include <vkw/CommandBuffer.hpp>
#include "RenderEngine/Pipelines/PipelineStage.h"
#include "RenderEngine/Pipelines/ShaderLoaderInterface.h"

namespace RenderEngine{


    class ComputeLayout: public PipelineStageLayout{
    public:
        ComputeLayout(vkw::Device& device, RenderEngine::ShaderLoaderInterface& shaderLoader, SubstageDescription const& desc, uint32_t maxSets);

        void bind(vkw::CommandBuffer& buffer) const;

        vkw::PipelineLayout const& pipelineLayout() const{
            return m_layout;
        }
    private:
        vkw::PipelineLayout m_layout;
        vkw::ComputePipeline m_pipeline;

    };

    class Compute: public PipelineStage<ComputeLayout>{
    public:
        explicit Compute(ComputeLayout& parent);

        void dispatch(vkw::CommandBuffer& buffer, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) const;
    };
}
#endif //TESTAPP_COMPUTE_H
