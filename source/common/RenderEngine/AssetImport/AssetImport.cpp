#include "AssetImport.h"
#include "tiny_gltf/stb_image.h"
#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <vkw/Buffer.hpp>
#include <vkw/CommandBuffer.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/Queue.hpp>
#include <vkw/SPIRVModule.hpp>
#include <vkw/StagingBuffer.hpp>
#include <iostream>
#include <ranges>
#include <algorithm>

bool RenderEngine::AssetImporterBase::try_open(
    const std::string &filename) const {
  std::ifstream is(m_root + filename,
                   std::ios::binary | std::ios::in | std::ios::ate);
  return is.is_open();
}

vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE>
RenderEngine::TextureLoader::loadTexture(const std::string &name, int mipLevels,
                                         VkImageLayout finalLayout,
                                         VkImageUsageFlags imageUsage,
                                         VmaMemoryUsage memUsage) const {
  std::set<std::string> fileExtensions = {"png", "jpg", "jpeg"};
  std::string filename;
  for (auto &ext : fileExtensions) {
    filename = name + "." + ext;
    if (try_open(filename))
      break;
    else
      filename = "";
  }

  if (filename.empty())
    throw std::runtime_error(
        "Failed to import '" + name +
        "' texture: could not open file with any known extension");

  auto data = read_binary(filename);

  int width, height, bits;

  auto *imageData =
      stbi_load_from_memory(reinterpret_cast<unsigned char *>(data.data()),
                            data.size(), &width, &height, &bits, 4);
  if (!imageData)
    throw std::runtime_error("Failed to read image data.");

  try {
    vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE> ret = loadTexture(
        imageData, width, height, mipLevels, finalLayout, imageUsage, memUsage);
    stbi_image_free(imageData);
    return ret;
  } catch (std::runtime_error &e) {
    stbi_image_free(imageData);
    throw;
  }
}
static void
generateMipMaps(vkw::CommandBuffer &buffer,
                vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE> &image,
                int mipLevels) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image.vkw::AllocatedImage::operator VkImage_T *();
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = image.width();
  int32_t mipHeight = image.height();

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, {&barrier, 1});

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    buffer.blitImage(image, blit);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              {&barrier, 1});

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            {&barrier, 1});
}

vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE>
RenderEngine::TextureLoader::loadTexture(const unsigned char *texture,
                                         size_t textureWidth,
                                         size_t textureHeight, int mipLevels,
                                         VkImageLayout finalLayout,
                                         VkImageUsageFlags imageUsage,
                                         VmaMemoryUsage memUsage) const {

  VmaAllocationCreateInfo allocInfo{};

  allocInfo.usage = memUsage;
  if (memUsage == VMA_MEMORY_USAGE_GPU_ONLY)
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  int transferUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (mipLevels > 1)
    transferUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  auto ret = vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE>{
      m_device.get().getAllocator(),
      allocInfo,
      VK_FORMAT_R8G8B8A8_UNORM,
      static_cast<uint32_t>(textureWidth),
      static_cast<uint32_t>(textureHeight),
      1,
      1,
      static_cast<uint32_t>(mipLevels),
      transferUsage | imageUsage};

  VkImageMemoryBarrier transitLayout1{};
  transitLayout1.image = ret.vkw::AllocatedImage::operator VkImage_T *();
  transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  transitLayout1.pNext = nullptr;
  transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  transitLayout1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  transitLayout1.subresourceRange.baseArrayLayer = 0;
  transitLayout1.subresourceRange.baseMipLevel = 0;
  transitLayout1.subresourceRange.layerCount = 1;
  transitLayout1.subresourceRange.levelCount = mipLevels;
  transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  transitLayout1.srcAccessMask = 0;

  VkImageMemoryBarrier transitLayout2{};
  transitLayout2.image = ret.vkw::AllocatedImage::operator VkImage_T *();
  transitLayout2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  transitLayout2.pNext = nullptr;
  transitLayout2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  transitLayout2.newLayout = finalLayout;
  transitLayout2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  transitLayout2.subresourceRange.baseArrayLayer = 0;
  transitLayout2.subresourceRange.baseMipLevel = 0;
  transitLayout2.subresourceRange.layerCount = 1;
  transitLayout2.subresourceRange.levelCount = 1;
  transitLayout2.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  transitLayout2.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

  auto const &transferQueue = m_device.get().anyTransferQueue();
  auto commandPool =
      vkw::CommandPool{m_device, 0, transferQueue.family().index()};
  auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
  transferCommand.begin(0);
  transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     {&transitLayout1, 1});
  // Limit the size of single staging buffer by 100MB
  auto linesPerCopy = 100000000u / (4 * textureWidth);

  auto dataSizePerCopy = textureWidth * linesPerCopy * 4u;
  auto textureEnd = texture + textureWidth * textureHeight * 4u;
  std::vector<vkw::StagingBuffer<unsigned char>> stagingBuffers;

  for (auto i = 0u; i <= textureHeight / linesPerCopy; ++i) {
    auto startOffset = texture + i * textureWidth * linesPerCopy * 4u;
#undef min
    auto linesToCopy = std::min(
        (size_t)(textureEnd - startOffset) / (textureWidth * 4u), linesPerCopy);
    auto &stageBuffer = stagingBuffers.emplace_back(
        m_device,
        std::span<const unsigned char>{
            startOffset, std::min(startOffset + dataSizePerCopy, textureEnd)});

    VkBufferImageCopy bufferCopy{};
    bufferCopy.imageOffset = {0, static_cast<int>(i * linesPerCopy), 0};
    bufferCopy.imageExtent = {static_cast<uint32_t>(textureWidth),
                              static_cast<uint32_t>(linesToCopy), 1};
    bufferCopy.imageSubresource.mipLevel = 0;
    bufferCopy.imageSubresource.layerCount = 1;
    bufferCopy.imageSubresource.baseArrayLayer = 0;
    bufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transferCommand.copyBufferToImage(stageBuffer, ret,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      {&bufferCopy, 1});
  }

  if (mipLevels > 1) {
    generateMipMaps(transferCommand, ret, mipLevels);
  } else
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {&transitLayout2, 1});
  transferCommand.end();

  transferQueue.submit(transferCommand);
  transferQueue.waitIdle();

  return ret;
}

vkw::SPIRVModule RenderEngine::ShaderImporter::loadModule(std::string_view name) const {
  std::string filename = std::string(name) + ".spv";
  auto code = read_binary<uint32_t>(filename);
  return vkw::SPIRVModule{code};
}

vkw::VertexShader
RenderEngine::ShaderImporter::loadVertexShader(const std::string &name) const {
  std::string filename = name + ".vert";
  return {m_device, loadModule(filename)};
}

vkw::FragmentShader RenderEngine::ShaderImporter::loadFragmentShader(
    const std::string &name) const {
  std::string filename = name + ".frag";
  return {m_device, loadModule(filename)};
}

vkw::ComputeShader
RenderEngine::ShaderImporter::loadComputeShader(const std::string &name) const {
  std::string filename = name + ".comp";
  return vkw::ComputeShader{m_device,
                            vkw::SPIRVModule{loadModule(filename)}};
}

vkw::VertexShader RenderEngine::ShaderImporter::loadVertexShader(vkw::SPIRVModule const& module) const{
  return vkw::VertexShader(m_device, module);
}
vkw::FragmentShader RenderEngine::ShaderImporter::loadFragmentShader(vkw::SPIRVModule const& module) const{
  return vkw::FragmentShader(m_device, module);
}
vkw::ComputeShader RenderEngine::ShaderImporter::loadComputeShader(vkw::SPIRVModule const& module) const{
  return vkw::ComputeShader(m_device, module);
}

RenderEngine::ShaderImporter::ShaderImporter(vkw::Device &device,
                                             std::string const &rootDirectory)
    : AssetImporterBase(rootDirectory), m_device(device){}

