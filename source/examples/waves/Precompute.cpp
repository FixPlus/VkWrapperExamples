#include "Precompute.h"

TestApp::PrecomputeImageLayout::PrecomputeImageLayout(vkw::Device& device,
                                                      RenderEngine::ShaderLoaderInterface& shaderLoader, RenderEngine::SubstageDescription stageDescription, uint32_t xGroupSize, uint32_t yGroupSize):
                                                      RenderEngine::ComputeLayout(device, shaderLoader, stageDescription, 10),
                                                      m_xGroupSize(xGroupSize),
                                                      m_yGroupSize(yGroupSize)
{

}

TestApp::PrecomputeImage::PrecomputeImage(vkw::Device &device, TestApp::PrecomputeImageLayout &parent,
                                          vkw::BasicImage<vkw::COLOR, vkw::I2D, vkw::ARRAY> &image): RenderEngine::Compute(parent),
                                                                     m_image(image),
                                                                                   m_imageView(device, image, image.format(), 0u, image.layers()),
                                                                     m_cached_group_size(image.width() / parent.xGroupSize(), image.height() / parent.yGroupSize()){

    set().writeStorageImage(0, m_imageView, VK_IMAGE_LAYOUT_GENERAL);
}

void TestApp::PrecomputeImage::acquireOwnership(vkw::CommandBuffer &buffer, uint32_t incomingFamilyIndex,
                                                VkImageLayout incomingLayout, VkAccessFlags incomingAccessMask,
                                                VkPipelineStageFlags incomingStageMask) const{
    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_image.get();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = incomingLayout;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = incomingFamilyIndex;
    transitLayout1.dstQueueFamilyIndex = buffer.queueFamily();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_image.get().layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = incomingAccessMask;

    buffer.imageMemoryBarrier(incomingStageMask, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {&transitLayout1, 1});

}

void TestApp::PrecomputeImage::releaseOwnershipTo(vkw::CommandBuffer &buffer, uint32_t computeFamilyIndex,
                                                  VkImageLayout incomingLayout, VkAccessFlags incomingAccessMask,
                                                  VkPipelineStageFlags incomingStageMask) const{
    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_image.get();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = incomingLayout;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = buffer.queueFamily();
    transitLayout1.dstQueueFamilyIndex = computeFamilyIndex;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_image.get().layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = incomingAccessMask;

    buffer.imageMemoryBarrier(incomingStageMask, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {&transitLayout1, 1});
}

void TestApp::PrecomputeImage::releaseOwnership(vkw::CommandBuffer &buffer, uint32_t acquireFamilyIndex,
                                                VkImageLayout acquireLayout, VkAccessFlags acquireAccessMask,
                                                VkPipelineStageFlags acquireStageMask) const{
    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_image.get();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = acquireLayout;
    transitLayout1.srcQueueFamilyIndex = buffer.queueFamily();
    transitLayout1.dstQueueFamilyIndex = acquireFamilyIndex;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_image.get().layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = acquireAccessMask;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, acquireStageMask, {&transitLayout1, 1});

}

void TestApp::PrecomputeImage::acquireOwnershipFrom(vkw::CommandBuffer &buffer, uint32_t computeFamilyIndex,
                                                   VkImageLayout acquireLayout, VkAccessFlags acquireAccessMask,
                                                   VkPipelineStageFlags acquireStageMask) const{
    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_image.get();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = acquireLayout;
    transitLayout1.srcQueueFamilyIndex = computeFamilyIndex;
    transitLayout1.dstQueueFamilyIndex = buffer.queueFamily();
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = m_image.get().layers();
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = acquireAccessMask;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, acquireStageMask, {&transitLayout1, 1});

}

void TestApp::PrecomputeImage::dispatch(vkw::CommandBuffer &buffer) {
   RenderEngine::Compute::dispatch(buffer, m_cached_group_size.first, m_cached_group_size.second);
}
