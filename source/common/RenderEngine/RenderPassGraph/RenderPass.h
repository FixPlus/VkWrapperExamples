#ifndef TESTAPP_RENDERPASS_H
#define TESTAPP_RENDERPASS_H

#include "vkw/RenderPass.hpp"
#include "vkw/Buffer.hpp"
#include "vkw/Image.hpp"
#include "vkw/Queue.hpp"


namespace RenderEngine{

    enum class ResourceType{
        IMAGE,
        BUFFER
    };

    enum class ResourceUseType{
        COLOR_ATTACHMENT,
        DEPTH_ATTACHMENT,
        INPUT_ATTACHMENT,
        SAMPLED_IMAGE,
        STORAGE_IMAGE,
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        VERTEX_BUFFER,
        IMAGE_COPY_DST,
        IMAGE_COPY_SRC,
        BUFFER_COPY_SRC,
        BUFFER_COPY_DST
    };

    /** Any object allocated and/or used by gpu. */
    class ResourceUse;

    class Resource{
    public:
        ResourceType type() const;
    };

    class ResourceUse{
        ResourceUseType type() const;
    };

    class PassInfo{
    public:

    private:
        std::vector<std::unique_ptr<ResourceUse>> m_inputs;
        std::vector<std::unique_ptr<ResourceUse>> m_outputs;

    };



    struct BufferAccessInfo{
        VkAccessFlags accessMask;
        VkPipelineStageFlags stageMask;
        uint32_t accessOffset;
        uint32_t accessRange;
    };

    struct ImageAccessInfo{
        VkAccessFlags accessMask;
        VkPipelineStageFlags stageMask;
        VkImageLayout accessLayout;  // if image used as input, this is layout it must enter the pass
        VkImageLayout finalLayout;   // if image used as output, this is layout it must leave the pass
        VkImageSubresourceRange accessRange;
    };

    /** Pass is arbitrary amount of work put on gpu with it's input and output. */
    class Pass{
    public:

        virtual void operator()(vkw::CommandBuffer& commandBuffer) = 0;

    };

    class TransitPass: public Pass{
    public:

    };

    class ColorPass: public Pass, public vkw::RenderPass{
    public:

    };

    class ComputePass: public Pass{
    public:

    };

    class CopyPass: public Pass{

    };
}
#endif //TESTAPP_RENDERPASS_H
