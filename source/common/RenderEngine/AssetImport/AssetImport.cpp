#include "AssetImport.h"
#include <fstream>
#include "tiny_gltf/stb_image.h"
#include <set>
#include <vkw/Buffer.hpp>
#include <vkw/Queue.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/CommandBuffer.hpp>
#include <cstring>

bool RenderEngine::AssetImporterBase::try_open(const std::string &filename) const {
    std::ifstream is(m_root + filename, std::ios::binary | std::ios::in | std::ios::ate);
    return is.is_open();
}

std::vector<char> RenderEngine::AssetImporterBase::read_binary(const std::string &filename) const {
    std::ifstream is(m_root + filename, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open()) {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);

        std::vector<char> ret{};

        ret.resize(size);
        is.read(ret.data(), size);
        is.close();

        if (size == 0)
            throw std::runtime_error(
                    "Asset import failed: could not read " + filename);

        return ret;
    }
    throw std::runtime_error("Asset import failed: cannot open file '" + filename + "'");
}

vkw::ColorImage2D
RenderEngine::TextureLoader::loadTexture(const std::string &name, VkImageLayout finalLayout, VkImageUsageFlags imageUsage,
                                    VmaMemoryUsage memUsage) const {
    std::set<std::string> fileExtensions = {"png", "jpg", "jpeg"};
    std::string filename;
    for (auto &ext: fileExtensions) {
        filename = name + "." + ext;
        if (try_open(filename))
            break;
        else
            filename = "";
    }

    if (filename.empty())
        throw std::runtime_error(
                "Failed to import '" + name + "' texture: could not open file with any known extension");

    auto data = read_binary(filename);

    int width, height, bits;

    auto *imageData = stbi_load_from_memory(reinterpret_cast<unsigned char *>(data.data()), data.size(), &width,
                                            &height, &bits, 4);
    if (!imageData)
        throw std::runtime_error("Failed to read image data.");


    try {
        vkw::ColorImage2D ret = loadTexture(imageData, width, height, finalLayout, imageUsage, memUsage);
        stbi_image_free(imageData);
        return ret;
    } catch (std::runtime_error &e) {
        stbi_image_free(imageData);
        throw;
    }

}

vkw::ColorImage2D
RenderEngine::TextureLoader::loadTexture(const unsigned char *texture, size_t textureWidth, size_t textureHeight,
                                    VkImageLayout finalLayout, VkImageUsageFlags imageUsage,
                                    VmaMemoryUsage memUsage) const {

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    vkw::Buffer<unsigned char> stageBuffer{m_device, textureWidth * textureHeight * 4u,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT, allocInfo};

    auto mappedBuffer = stageBuffer.map();
    memcpy(mappedBuffer, texture, textureWidth * textureHeight * 4u);
    stageBuffer.flush();

    allocInfo.usage = memUsage;
    if (memUsage == VMA_MEMORY_USAGE_GPU_ONLY)
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    auto ret = vkw::ColorImage2D{m_device.get().getAllocator(), allocInfo, VK_FORMAT_R8G8B8A8_UNORM,
                                 static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), 1,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | imageUsage};

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = ret;
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
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = 0;

    VkImageMemoryBarrier transitLayout2{};
    transitLayout2.image = ret;
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

    auto transferQueue = m_device.get().getTransferQueue();
    auto commandPool = vkw::CommandPool{m_device, 0, transferQueue->familyIndex()};
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout1});
    VkBufferImageCopy bufferCopy{};
    bufferCopy.imageExtent = {static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), 1};
    bufferCopy.imageSubresource.mipLevel = 0;
    bufferCopy.imageSubresource.layerCount = 1;
    bufferCopy.imageSubresource.baseArrayLayer = 0;
    bufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    transferCommand.copyBufferToImage(stageBuffer, ret, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {bufferCopy});

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout2});
    transferCommand.end();

    transferQueue->submit(transferCommand);
    transferQueue->waitIdle();

    return ret;
}

vkw::VertexShader RenderEngine::ShaderImporter::loadVertexShader(const std::string &name) const {
    std::string filename = name + ".vert.spv";
    auto code = read_binary(filename);
    return {m_device, code.size(), reinterpret_cast<uint32_t *>(code.data())};
}

vkw::FragmentShader RenderEngine::ShaderImporter::loadFragmentShader(const std::string &name) const {
    std::string filename = name + ".frag.spv";
    auto code = read_binary(filename);
    return {m_device, code.size(), reinterpret_cast<uint32_t *>(code.data())};
}
