#ifndef TESTAPP_PRECOMPUTE_H
#define TESTAPP_PRECOMPUTE_H

#include <vkw/CommandBuffer.hpp>
#include <Image.hpp>
#include "RenderEngine/Pipelines/Compute.h"

namespace TestApp {


    class PrecomputeImageLayout : public RenderEngine::ComputeLayout {
    public:
        PrecomputeImageLayout(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader,
                              RenderEngine::SubstageDescription stageDescription, uint32_t xGroupSize,
                              uint32_t yGroupSize);

        uint32_t xGroupSize() const {
            return m_xGroupSize;
        }

        uint32_t yGroupSize() const {
            return m_yGroupSize;
        }

    private:
        static RenderEngine::SubstageDescription &m_emplace_image_binding(RenderEngine::SubstageDescription &desc);

        uint32_t m_xGroupSize, m_yGroupSize;
    };


    class PrecomputeImage : public RenderEngine::Compute {
    public:
        PrecomputeImage(vkw::Device &device, PrecomputeImageLayout &parent, vkw::ColorImage2DArrayInterface &image);


        void releaseOwnershipTo(vkw::CommandBuffer &buffer,
                                uint32_t computeFamilyIndex,
                                VkImageLayout incomingLayout,
                                VkAccessFlags incomingAccessMask,
                                VkPipelineStageFlags incomingStageMask) const;

        void acquireOwnership(vkw::CommandBuffer &buffer,
                              uint32_t incomingFamilyIndex,
                              VkImageLayout incomingLayout,
                              VkAccessFlags incomingAccessMask,
                              VkPipelineStageFlags incomingStageMask) const;


        void releaseOwnership(vkw::CommandBuffer &buffer,
                              uint32_t acquireFamilyIndex,
                              VkImageLayout acquireLayout,
                              VkAccessFlags acquireAccessMask,
                              VkPipelineStageFlags acquireStageMask) const;

        void acquireOwnershipFrom(vkw::CommandBuffer &buffer,
                                  uint32_t computeFamilyIndex,
                                  VkImageLayout acquireLayout,
                                  VkAccessFlags acquireAccessMask,
                                  VkPipelineStageFlags acquireStageMask) const;

        void dispatch(vkw::CommandBuffer &buffer);

    private:
        std::reference_wrapper<vkw::ColorImage2DArrayInterface> m_image;
        std::pair<uint32_t, uint32_t> m_cached_group_size;
    };

}
#endif //TESTAPP_PRECOMPUTE_H
