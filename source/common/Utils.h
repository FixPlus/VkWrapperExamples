#ifndef TESTAPP_UTILS_H
#define TESTAPP_UTILS_H

#include <vkw/VertexBuffer.hpp>
#include <vkw/Device.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/CommandBuffer.hpp>
#include <vkw/Queue.hpp>
#include <vkw/Image.hpp>
namespace TestApp{


    template<typename Buffer, typename T, typename ForwardIter>
    Buffer createStaticBuffer(vkw::Device& device, ForwardIter begin, ForwardIter end){
        auto size = end - begin;
        VmaAllocationCreateInfo createInfo{};

        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        Buffer stagingBuffer{device, static_cast<uint64_t>(size), createInfo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};

        auto* mapped = stagingBuffer.map();

        for(auto it = begin; it < end; ++it, ++mapped){
            *mapped = *it;
        }

        stagingBuffer.flush();

        createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        Buffer ret{device, static_cast<uint64_t>(size), createInfo, VK_BUFFER_USAGE_TRANSFER_DST_BIT};


        auto queue = device.getTransferQueue();

        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};

        auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};
        VkBufferCopy region{};
        region.size = sizeof(T) * size;
        region.srcOffset = 0;
        region.dstOffset = 0;

        commandBuffer.begin(0);

        commandBuffer.copyBufferToBuffer(stagingBuffer, ret, {region});

        commandBuffer.end();

        queue->submit(commandBuffer);
        queue->waitIdle();

        return ret;
    }

    vkw::DepthStencilImage2D createDepthStencilImage(vkw::Device &device, uint32_t width, uint32_t height);

    vkw::Sampler createDefaultSampler(vkw::Device& device);

}
#endif //TESTAPP_UTILS_H
