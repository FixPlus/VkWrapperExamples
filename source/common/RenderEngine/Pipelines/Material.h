#ifndef TESTAPP_MATERIAL_H
#define TESTAPP_MATERIAL_H

#include <string>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <vkw/CommandBuffer.hpp>
#include <set>
#include "RenderEngine/Pipelines/PipelineStage.h"
#include <vkw/Pipeline.hpp>

namespace RenderEngine{

    class Material;

    class MaterialLayout: public PipelineStageLayout{
    public:
        struct CreateInfo{
            SubstageDescription substageDescription;
            vkw::RasterizationStateCreateInfo rasterizationState{};
            std::optional<vkw::DepthTestStateCreateInfo> depthTestState{};
            uint32_t maxMaterials = 0;
        };
        MaterialLayout(vkw::Device& device, CreateInfo const & createInfo);

        vkw::RasterizationStateCreateInfo const& rasterizationState() const{
            return m_rasterizationState;
        }
        std::optional<vkw::DepthTestStateCreateInfo> depthTestState() const{
            return m_depthTestState;
        }

        bool depthOnly() const{
            return m_rasterizationState.operator const VkPipelineRasterizationStateCreateInfo &().rasterizerDiscardEnable == VK_TRUE;
        }
    private:
        vkw::RasterizationStateCreateInfo m_rasterizationState;
        std::optional<vkw::DepthTestStateCreateInfo> m_depthTestState;
    };

    class GraphicsRecordingState;


    class Material: public PipelineStage<MaterialLayout>{
    public:
        explicit Material(MaterialLayout& parent): PipelineStage(parent) {};

        virtual void bind(GraphicsRecordingState& state) const;
    };


}
#endif //TESTAPP_MATERIAL_H
