#ifndef TESTAPP_UTILS_H
#define TESTAPP_UTILS_H

#include <vkw/VertexBuffer.hpp>
#include <vkw/Device.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/CommandBuffer.hpp>
#include <vkw/Queue.hpp>
#include <vkw/Image.hpp>
#include <vkw/StagingBuffer.hpp>

namespace TestApp{


    template<typename Buffer, typename T, typename ForwardIter>
    Buffer createStaticBuffer(vkw::Device& device, ForwardIter begin, ForwardIter end){
        auto size = end - begin;
        VmaAllocationCreateInfo createInfo{};

        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        Buffer stagingBuffer{device, static_cast<uint64_t>(size), createInfo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};

        stagingBuffer.map();
        auto mapped = stagingBuffer.mapped();
        auto mapIt = mapped.begin();

        for(auto it = begin; it < end; ++it, ++mapIt){
            *mapIt = *it;
        }

        stagingBuffer.flush();

        createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        Buffer ret{device, static_cast<uint64_t>(size), createInfo, VK_BUFFER_USAGE_TRANSFER_DST_BIT};


        auto const& queue = device.anyTransferQueue();

        auto commandPool = vkw::CommandPool{device, 0, queue.family().index()};

        auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};
        VkBufferCopy region{};
        region.size = sizeof(T) * size;
        region.srcOffset = 0;
        region.dstOffset = 0;

        commandBuffer.begin(0);

        commandBuffer.copyBufferToBuffer(stagingBuffer, ret, {&region, 1});

        commandBuffer.end();

        queue.submit(commandBuffer);
        queue.waitIdle();

        return ret;
    }

    template<typename T>
    void loadUsingStaging(vkw::Device& device, vkw::UniformBuffer<T> const& buffer, T const& data){
      vkw::StagingBuffer<T> stagingBuffer{device, {&data, 1}};

      auto& queue = device.anyTransferQueue();

      auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue.family().index()};

      auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

      commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

      VkBufferCopy region{};

      region.size = stagingBuffer.bufferSize();

      commandBuffer.copyBufferToBuffer(stagingBuffer, buffer, {&region, 1});

      commandBuffer.end();

      queue.submit(vkw::SubmitInfo{commandBuffer});

      queue.waitIdle();
    }

    vkw::Image<vkw::DEPTH, vkw::I2D, vkw::SINGLE> createDepthStencilImage(vkw::Device &device, uint32_t width, uint32_t height);

    vkw::Sampler createDefaultSampler(vkw::Device& device, int mipLevels = 1);

    void doTransitLayout(vkw::ImageInterface& image, vkw::Device& device, VkImageLayout from, VkImageLayout to);

    inline vkw::QueueFamily::Type dedicatedTransfer() { return vkw::QueueFamily::TRANSFER;}

    inline vkw::QueueFamily::Type dedicatedCompute() { return vkw::QueueFamily::TRANSFER | vkw::QueueFamily::COMPUTE;}

    void requestQueues(vkw::PhysicalDevice& physicalDevice, bool wantTransfer = true, bool wantCompute = false);

}
#endif //TESTAPP_UTILS_H
