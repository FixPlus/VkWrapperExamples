#include "SwapChainImpl.h"
#include "vkw/Image.hpp"
#include "vkw/Device.hpp"
#include "vkw/Fence.hpp"
#include "vkw/Surface.hpp"
#include "vkw/Queue.hpp"
#include "vkw/CommandPool.hpp"
#include "vkw/CommandBuffer.hpp"
#include "Utils.h"

namespace TestApp{
    SwapChainImpl::SwapChainImpl(vkw::Device &device, vkw::Surface &surface, bool createDepthBuffer, bool vsync) :
    vkw::SwapChain(device, compileInfo(device, surface, vsync)),
    m_surface(surface), m_images(retrieveImages()) {

            std::vector<VkImageMemoryBarrier> transitLayouts;

            for (auto &image: m_images) {
                VkImageMemoryBarrier transitLayout{};
                transitLayout.image = image;
                transitLayout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                transitLayout.pNext = nullptr;
                transitLayout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                transitLayout.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                transitLayout.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transitLayout.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transitLayout.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                transitLayout.subresourceRange.baseArrayLayer = 0;
                transitLayout.subresourceRange.baseMipLevel = 0;
                transitLayout.subresourceRange.layerCount = 1;
                transitLayout.subresourceRange.levelCount = 1;
                transitLayout.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                transitLayout.srcAccessMask = 0;

                transitLayouts.push_back(transitLayout);
            }

            auto queue = device.anyGraphicsQueue();
            auto commandPool = vkw::CommandPool{device, 0, queue.family().index()};
            auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

            commandBuffer.begin(0);

            commandBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            transitLayouts);

            commandBuffer.end();

            auto fence = vkw::Fence{device};

            auto submitInfo = vkw::SubmitInfo(commandBuffer);

            queue.submit(submitInfo, fence);
            fence.wait();

            VkComponentMapping mapping;
            mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            for (auto &image: m_images) {
                m_image_views.emplace_back(device, image, image.format(), 0u, 1u, 0u, 1u, mapping);
            }

            auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

            m_depth.emplace(TestApp::createDepthStencilImage(device, extents.width, extents.height));
            m_depth_view = std::make_unique<vkw::ImageView<vkw::DEPTH, vkw::V2DA>>(device, m_depth.value(), m_depth->format(), 0u, 1u, mapping);

    }

    VkSwapchainCreateInfoKHR SwapChainImpl::compileInfo(vkw::Device &device, vkw::Surface &surface, bool vsync) {
        VkSwapchainCreateInfoKHR ret{};

        auto &physicalDevice = device.physicalDevice();
        auto availablePresentQueues = surface.getQueueFamilyIndicesThatSupportPresenting(physicalDevice);

        if (!device.physicalDevice().extensionSupported(vkw::ext::KHR_swapchain) || availablePresentQueues.empty())
            throw vkw::Error("Cannot create SwapChain on device that does not support presenting");

        auto surfCaps = surface.getSurfaceCapabilities(physicalDevice);

        ret.surface = surface;
        ret.clipped = VK_TRUE;
        ret.imageExtent = surfCaps.currentExtent;
        ret.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ret.imageArrayLayers = 1;
        ret.presentMode = surface.getAvailablePresentModes(physicalDevice).front();
        ret.queueFamilyIndexCount = 0;
        ret.imageColorSpace = surface.getAvailableFormats(physicalDevice).front().colorSpace;
        ret.imageFormat = surface.getAvailableFormats(physicalDevice).front().format;
        ret.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ret.minImageCount = surfCaps.minImageCount;
        ret.preTransform = surfCaps.currentTransform;
        ret.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto presentModes = surface.getAvailablePresentModes(physicalDevice);
        auto presentModeCount = presentModes.size();
        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        for (size_t i = 0; i < presentModeCount; i++) {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }

        if (swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR)
            for (size_t i = 0; i < presentModeCount; i++) {
                if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                    break;
                }
            }

        if(vsync)
            swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        // Find a supported composite alpha format (not all devices support alpha
        // opaque)
        VkCompositeAlphaFlagBitsKHR compositeAlpha =
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // Simply select the first composite alpha format available
        std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        for (auto &compositeAlphaFlag: compositeAlphaFlags) {
            if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
                compositeAlpha = compositeAlphaFlag;
                break;
            };
        }

        ret.compositeAlpha = compositeAlpha;
        ret.presentMode = swapchainPresentMode;

        return ret;
    }

}