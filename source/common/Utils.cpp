#include "Utils.h"
#include <vkw/Sampler.hpp>

namespace TestApp{
    vkw::Image<vkw::DEPTH, vkw::I2D, vkw::SINGLE> createDepthStencilImage(vkw::Device &device, uint32_t width, uint32_t height) {
        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        auto depthMap = vkw::Image<vkw::DEPTH, vkw::I2D, vkw::SINGLE>{device.getAllocator(), createInfo, VK_FORMAT_D32_SFLOAT, width,
                                                 height, 1, 1, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

        VkImageMemoryBarrier transitLayout{};
        transitLayout.image = depthMap;
        transitLayout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transitLayout.pNext = nullptr;
        transitLayout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        transitLayout.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        transitLayout.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        transitLayout.subresourceRange.baseArrayLayer = 0;
        transitLayout.subresourceRange.baseMipLevel = 0;
        transitLayout.subresourceRange.layerCount = 1;
        transitLayout.subresourceRange.levelCount = 1;
        transitLayout.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        transitLayout.srcAccessMask = 0;

        auto queue = device.getTransferQueue();
        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
        auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

        transferCommand.begin(0);
        transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           {transitLayout});
        transferCommand.end();

        queue->submit(transferCommand);
        queue->waitIdle();

        return depthMap;
    }

    vkw::Sampler createDefaultSampler(vkw::Device& device, int mipLevels){
        VkSamplerCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.magFilter = VK_FILTER_LINEAR;
        createInfo.minFilter = VK_FILTER_LINEAR;
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        createInfo.anisotropyEnable = false;
        createInfo.minLod = 0.0f;
        createInfo.maxLod = mipLevels;

        return vkw::Sampler{device, createInfo};
    }

    void doTransitLayout(vkw::ImageInterface& image, vkw::Device& device, VkImageLayout from, VkImageLayout to){
        VkImageMemoryBarrier transitLayout{};
        transitLayout.image = image;
        transitLayout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transitLayout.pNext = nullptr;
        transitLayout.oldLayout = from;
        transitLayout.newLayout = to;
        transitLayout.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout.subresourceRange = image.completeSubresourceRange();
        transitLayout.dstAccessMask = 0;
        transitLayout.srcAccessMask = 0;

        auto queue = device.getTransferQueue();
        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
        auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

        transferCommand.begin(0);
        transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           {transitLayout});
        transferCommand.end();

        queue->submit(transferCommand);
        queue->waitIdle();
    }
}