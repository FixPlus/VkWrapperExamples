#include "AssetImport.h"
#include <fstream>
#include "tiny_gltf/stb_image.h"
#include <set>
#include <vkw/Buffer.hpp>
#include <vkw/Queue.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/CommandBuffer.hpp>
#include <cstring>
#include <iostream>

bool RenderEngine::AssetImporterBase::try_open(const std::string &filename) const {
    std::ifstream is(m_root + filename, std::ios::binary | std::ios::in | std::ios::ate);
    return is.is_open();
}


vkw::ColorImage2D
RenderEngine::TextureLoader::loadTexture(const std::string &name, int mipLevels, VkImageLayout finalLayout, VkImageUsageFlags imageUsage,
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
        vkw::ColorImage2D ret = loadTexture(imageData, width, height, mipLevels, finalLayout, imageUsage, memUsage);
        stbi_image_free(imageData);
        return ret;
    } catch (std::runtime_error &e) {
        stbi_image_free(imageData);
        throw;
    }

}
static void generateMipMaps(vkw::CommandBuffer& buffer, vkw::ColorImage2D& image, int mipLevels){
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
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

        buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {barrier});

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        buffer.blitImage(image, blit);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {barrier});

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {barrier});

}

vkw::ColorImage2D
RenderEngine::TextureLoader::loadTexture(const unsigned char *texture, size_t textureWidth, size_t textureHeight, int mipLevels,
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

    int transferUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(mipLevels > 1)
        transferUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    auto ret = vkw::ColorImage2D{m_device.get().getAllocator(), allocInfo, VK_FORMAT_R8G8B8A8_UNORM,
                                 static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), static_cast<uint32_t>(mipLevels),
                                 transferUsage | imageUsage};

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
    transitLayout1.subresourceRange.levelCount = mipLevels;
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

    if(mipLevels > 1) {
        generateMipMaps(transferCommand, ret, mipLevels);
    } else
        transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout2});
    transferCommand.end();

    transferQueue->submit(transferCommand);
    transferQueue->waitIdle();

    return ret;
}

vkw::VertexShader RenderEngine::ShaderImporter::loadVertexShader(const std::string &name) const {
    std::string filename = name + ".vert.spv";
    auto code = read_binary<uint32_t>(filename);
    std::vector<std::vector<uint32_t> const*> binRefs;
    binRefs.push_back(&code);
    try {
        auto linked = m_link(binRefs);
        return {m_device, linked.size() * 4u, linked.data()};
    } catch (std::runtime_error& e){
        throw std::runtime_error(e.what() + std::string(". Linked modules were: ") + filename);
    }
}

vkw::FragmentShader RenderEngine::ShaderImporter::loadFragmentShader(const std::string &name) const {
    std::string filename = name + ".frag.spv";
    auto code = read_binary<uint32_t>(filename);
    std::vector<std::vector<uint32_t> const*> binRefs;
    binRefs.push_back(&code);
    try {
        auto linked = m_link(binRefs);
        return {m_device, linked.size() * 4u, linked.data()};
    } catch (std::runtime_error& e){
        throw std::runtime_error(e.what() + std::string(". Linked modules were: ") + filename);
    }
}

vkw::VertexShader
RenderEngine::ShaderImporter::loadVertexShader(const std::string &geometry, const std::string &projection) const {
    std::string geom_filename = geometry + ".gm.vert.spv";
    std::string proj_filename = projection + ".pr.vert.spv";
    std::vector<uint32_t> geometry_code = read_binary<uint32_t>(geom_filename);
    std::vector<uint32_t> projection_code = read_binary<uint32_t>(proj_filename);
    std::vector<std::vector<uint32_t> const*> binRefs;

    binRefs.push_back(&geometry_code);
    binRefs.push_back(&projection_code);
    binRefs.push_back(&m_general_vert);

    try{
        auto linked = m_link(binRefs);

        return {m_device, linked.size() * 4u, linked.data()};

    } catch (std::runtime_error& e){
        throw std::runtime_error(e.what() + std::string(". Linked modules were: ") + geom_filename + " and " + proj_filename);
    }
}


vkw::FragmentShader
RenderEngine::ShaderImporter::loadFragmentShader(const std::string &material, const std::string &lighting) const {
    std::string material_filename = material + ".mt.frag.spv";
    std::string lighting_filename = lighting + ".lt.frag.spv";
    std::vector<uint32_t> material_code = read_binary<uint32_t>(material_filename);
    std::vector<uint32_t> lighting_code = read_binary<uint32_t>(lighting_filename);
    std::vector<std::vector<uint32_t> const*> binRefs;

    binRefs.push_back(&material_code);
    binRefs.push_back(&lighting_code);
    binRefs.push_back(&m_general_frag);

    try{
        auto linked = m_link(binRefs);

        return {m_device, linked.size() * 4u, linked.data()};

    } catch (std::runtime_error& e){
        throw std::runtime_error(e.what() + std::string(". Linked modules were: ") + material_filename + " and " + lighting_filename);
    }
}


RenderEngine::ShaderImporter::ShaderImporter(vkw::Device &device, std::string const &rootDirectory) :
        AssetImporterBase(rootDirectory), m_device(device), m_context(SPV_ENV_VULKAN_1_0),
        m_message_consumer([](spv_message_level_t messageLevel, const char* message, const spv_position_t position, const char*){
            if(messageLevel == SPV_MSG_ERROR){
                std::cerr <<"SPIRV-Link failed. Error message: " + std::string(message) << std::endl;
            }
        }),
        m_general_vert(read_binary<uint32_t>("general.vert.spv")),
        m_general_frag(read_binary<uint32_t>("general.frag.spv")){
    m_context.SetMessageConsumer(m_message_consumer);
}

std::vector<uint32_t> RenderEngine::ShaderImporter::m_link(std::vector<std::vector<uint32_t> const*> const& binaries) const{
    std::vector<uint32_t const*> bin_refs{};
    std::vector<size_t> bin_sizes{};

    for(auto binary : binaries){
        bin_refs.push_back(binary->data());
        bin_sizes.push_back(binary->size());
    }
    std::vector<uint32_t> ret{};
    auto result = spvtools::Link(m_context, bin_refs.data(), bin_sizes.data(), binaries.size(), &ret);

    if(result != SPV_SUCCESS)
        throw std::runtime_error("SPIRV-Link failed");

    return ret;
}

vkw::ComputeShader RenderEngine::ShaderImporter::loadComputeShader(const std::string &name) const {
    std::string filename = name + ".comp.spv";
    auto code = read_binary<uint32_t>(filename);
    std::vector<std::vector<uint32_t> const*> binRefs;
    binRefs.push_back(&code);
    try {
        auto linked = m_link(binRefs);
        return {m_device, linked.size() * 4u, linked.data()};
    } catch (std::runtime_error& e){
        throw std::runtime_error(e.what() + std::string(". Linked modules were: ") + filename);
    }
}
