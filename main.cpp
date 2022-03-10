#include <iostream>

#define VKW_EXTENSION_INITIALIZERS_MAP_DEFINITION 1
#include "vkw/SymbolTable.hpp"
#include "Instance.hpp"
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>
#include <chrono>
#include <thread>
#include <SwapChain.hpp>
#include <Buffer.hpp>
#include <Queue.hpp>
#include <Semaphore.hpp>
#include <Fence.hpp>
#include <Image.hpp>
#include <CommandPool.hpp>
#include <CommandBuffer.hpp>
#include <VertexBuffer.hpp>
#include <Pipeline.hpp>
#include <cassert>
#include <Shader.hpp>
#include <fstream>
#include <RenderPass.hpp>
#include <FrameBuffer.hpp>
#include <DescriptorPool.hpp>
#include <DescriptorSet.hpp>
#include <UniformBuffer.hpp>
#include <cmath>
#include <chrono>
#include <Sampler.hpp>
#include <Surface.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include "external/stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
class MySwapChain : public vkw::SwapChain {
public:
    MySwapChain(vkw::Device &device, vkw::Surface &surface) :
            vkw::SwapChain(device, compileInfo(device, surface)),
            m_surface(surface) {

        auto images = retrieveImages();
        std::vector<VkImageMemoryBarrier> transitLayouts;

        for (auto &image: images) {
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

        auto queue = device.getGraphicsQueue();
        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
        auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

        commandBuffer.begin(0);

        commandBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                         transitLayouts);

        commandBuffer.end();

        auto fence = vkw::Fence{device};

        queue->submit(commandBuffer, {}, {}, {}, &fence);
        fence.wait();

        //queue->waitIdle();

    }


private:

    static VkSwapchainCreateInfoKHR compileInfo(vkw::Device &device, vkw::Surface &surface) {
        VkSwapchainCreateInfoKHR ret{};

        auto& physicalDevice = device.physicalDevice();
        auto availablePresentQueues = surface.getQueueFamilyIndicesThatSupportPresenting(physicalDevice);

        if (!device.supportsPresenting() || availablePresentQueues.empty())
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

    vkw::Surface &m_surface;
};

struct MyAttrs : vkw::AttributeBase<vkw::VertexAttributeType::VEC3F,
        vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC2F> {
    glm::vec3 pos;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    glm::vec3 normal;
    glm::vec2 uv;
};

struct MyUniform {
    glm::mat4 displacement;
    glm::mat4 perspective = glm::perspective(60.0f, 16.0f/9.0f, 0.01f, 1000.0f);
} myUniform;


std::vector<MyAttrs> makeCube(){
    std::vector<MyAttrs> ret{};
    // top
    ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}});

    ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}});
    ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f}});

    // bottom

    ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}});

    ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}});
    ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 1.0f}});

    // east

    ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}});

    ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}});
    ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 1.0f}});

    // west

    ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}});

    ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 1.0f}});

    // north

    ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});

    ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});
    ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}});

    // south

    ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});

    ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
    ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});
    ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}});

    return ret;
}

template<typename T>
requires std::derived_from<T, vkw::ShaderBase>
T loadShader(vkw::Device &device, std::string const &filename) {
    std::ifstream input{filename, std::ios::binary};
    std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open()) {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<char> shaderCode;

        shaderCode.resize(size);

        is.read(shaderCode.data(), size);
        is.close();

        if (size == 0)
            throw std::runtime_error(
                    "Failed to read " + filename);

        return T{device, shaderCode.size(), reinterpret_cast<uint32_t *>(shaderCode.data())};
    } else {
        throw std::runtime_error("Failed to open file " + filename);
    }

    return T{device, 0, nullptr};
}

vkw::ColorImage2D loadTexture(const char *filename, vkw::Device &device) {
    int width, height, bits;
    auto *imageData = stbi_load(filename, &width, &height, &bits, 4);
    if (!imageData)
        throw std::runtime_error("Failed to load image: " + std::string(filename));

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    vkw::Buffer<unsigned char> stageBuffer{device, width * height * 4u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, allocInfo};

    auto mappedBuffer = stageBuffer.map();
    memcpy(mappedBuffer, imageData, width * height * 4u);
    stageBuffer.flush();

    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    auto ret = vkw::ColorImage2D{device.getAllocator(), allocInfo, VK_FORMAT_R8G8B8A8_UNORM,
                                 static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};

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
    transitLayout2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout2.subresourceRange.baseArrayLayer = 0;
    transitLayout2.subresourceRange.baseMipLevel = 0;
    transitLayout2.subresourceRange.layerCount = 1;
    transitLayout2.subresourceRange.levelCount = 1;
    transitLayout2.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout2.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    auto transferQueue = device.getTransferQueue();
    auto commandPool = vkw::CommandPool{device, 0, transferQueue->familyIndex()};
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout1});
    VkBufferImageCopy bufferCopy{};
    bufferCopy.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
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


    stbi_image_free(imageData);

    return ret;
}

vkw::DepthStencilImage2D createDepthStencilImage(vkw::Device& device, uint32_t width, uint32_t height){
    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    auto depthMap = vkw::DepthStencilImage2D{device.getAllocator(), createInfo, VK_FORMAT_D24_UNORM_S8_UINT, width, height, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

    VkImageMemoryBarrier transitLayout{};
    transitLayout.image = depthMap;
    transitLayout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout.pNext = nullptr;
    transitLayout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
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

int main() {


    // 1. Create Instance and Window

    if (glfwInit() != GLFW_TRUE) {
        std::cout << "Failed to init GLFW" << std::endl;
        return 0;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto *window = glfwCreateWindow(800, 600, "Hello, Boez!", nullptr, nullptr);

    uint32_t count = 0;
    auto ext = glfwGetRequiredInstanceExtensions(&count);

    std::vector<std::string> reqExtensions;
    for (int i = 0; i < count; ++i)
        reqExtensions.emplace_back(ext[i]);

#ifdef __linux__
    reqExtensions.emplace_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance{vulkanLib, reqExtensions};

    renderInstance.printExtensions();

    // 2. Create Device

    auto devs = renderInstance.enumerateAvailableDevices();

    vkw::PhysicalDevice deviceDesc{renderInstance, 0u};

    if(devs.empty()) {
        std::cout << "No available devices supporting vulkan on this machine." << std::endl <<
                  " Make sure your graphics drivers are installed and updated." << std::endl;
        return 1;
    }

    deviceDesc.enableExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    deviceDesc.isFeatureSupported(vkw::feature::multiViewport());

    auto device = vkw::Device{renderInstance, deviceDesc};


    uint32_t counter = 0;
    for (auto &d: devs) {
        VkPhysicalDeviceProperties props{};
        renderInstance.core<1, 0>().vkGetPhysicalDeviceProperties(d, &props);
        std::cout << "Device " << counter << ": " <<  props.deviceName << std::endl;
        counter++;
    }

    // 3. Create surface
#ifdef _WIN32
    auto surface = vkw::Surface(renderInstance, GetModuleHandle(NULL), glfwGetWin32Window(window));
#elif defined(__linux__)
    auto surface = vkw::Surface(renderInstance, glfwGetX11Display(), glfwGetX11Window (window));
#else
    #error "Platform is not supported"
#endif
    // 4. Create swapchain

    std::unique_ptr<MySwapChain> mySwapChain = std::make_unique<MySwapChain>(MySwapChain{device, surface});

    auto queue = device.getGraphicsQueue();

    // 5. create vertex buffer

    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    auto attrs = makeCube();

    auto vBuf = vkw::VertexBuffer<MyAttrs>(device, attrs.size(), createInfo);
    auto uBuf = vkw::UniformBuffer<MyUniform>(device, createInfo);

    auto texture = loadTexture("image.png", device);
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 0.0f;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;

    auto sampler = vkw::Sampler(device, samplerCI);

    // 6. load data to vertex buffer



    auto mapped = vBuf.map();
    memcpy(mapped, attrs.data(), attrs.size() * sizeof(MyAttrs));
    vBuf.flush();
    auto uBufMapped = uBuf.map();

    auto fence = std::make_unique<vkw::Fence>(device);

    // 7. create command pool

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        device.getGraphicsQueue()->familyIndex()};

    VkComponentMapping mapping;
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &textureView = texture.getView<vkw::ColorImageView>(device, VK_FORMAT_R8G8B8A8_UNORM, mapping);

    // 8. create swapchain images views for framebuffer

    std::vector<vkw::Image2DArrayViewCRef> swapChainImageViews;
    auto swapChainImages = mySwapChain->retrieveImages();
    for (auto &image: swapChainImages) {
        swapChainImageViews.emplace_back(image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
    }

    // 9. create render pass

    auto attachmentDescription = vkw::AttachmentDescription{swapChainImageViews.front().get().format(),
                                                            VK_SAMPLE_COUNT_1_BIT,
                                                            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                            VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    auto depthAttachment = vkw::AttachmentDescription{VK_FORMAT_D24_UNORM_S8_UINT,
                                                            VK_SAMPLE_COUNT_1_BIT,
                                                            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                            VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    auto subpassDescription = vkw::SubpassDescription{};
    subpassDescription.addColorAttachment(attachmentDescription, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    subpassDescription.addDepthAttachment(depthAttachment, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    auto inputDependency = vkw::SubpassDependency{};
    inputDependency.setDstSubpass(subpassDescription);
    inputDependency.srcAccessMask = 0;
    inputDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    inputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto outputDependency = vkw::SubpassDependency{};
    outputDependency.setSrcSubpass(subpassDescription);
    outputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    outputDependency.dstAccessMask = 0;
    outputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputDependency.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    outputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


    auto renderPass = vkw::RenderPass{device, vkw::RenderPassCreateInfo{{attachmentDescription, depthAttachment}, {subpassDescription},
                                                                         {inputDependency, outputDependency}}};

    // 10. create framebuffer for each swapchain image view

    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;
    createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    std::optional<vkw::DepthStencilImage2D> depthMap = createDepthStencilImage(device, extents.width, extents.height);
    vkw::DepthImage2DArrayView const* depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping, VK_FORMAT_D24_UNORM_S8_UINT);
    std::vector<vkw::FrameBuffer> framebuffers;

    for (auto &view: swapChainImageViews) {
        framebuffers.emplace_back(device, renderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                  view.get().image()->rawExtents().height}, vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
    }

    // 11. create graphics pipeline

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1});
    poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1});

    auto descriptorPool = vkw::DescriptorPool{device, 1, poolSizes};
    auto myBinding = vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    auto myTextureBinding = vkw::DescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    auto descriptorSetLayout = vkw::DescriptorSetLayout{device, {myBinding, myTextureBinding}};
    auto myDescriptor = vkw::DescriptorSet{descriptorPool, descriptorSetLayout};
    myDescriptor.write(0, uBuf);
    myDescriptor.write(1, textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);

    auto vertexInputCreateInfo = vkw::VertexInputStateCreateInfo<vkw::per_vertex<MyAttrs, 0>>{};

    auto vertexShader = loadShader<vkw::VertexShader>(device, "triangle.vert.spv");
    auto fragmentShader = loadShader<vkw::FragmentShader>(device, "triangle.frag.spv");

    auto pipelineLayout = vkw::PipelineLayout{device, descriptorSetLayout};

    vkw::GraphicsPipelineCreateInfo pipelineCreateInfo{renderPass, 0, pipelineLayout};
    pipelineCreateInfo.addVertexShader(vertexShader);
    pipelineCreateInfo.addFragmentShader(fragmentShader);
    pipelineCreateInfo.addVertexInputState(vertexInputCreateInfo);
    pipelineCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});

    auto pipeline = vkw::GraphicsPipeline{device, pipelineCreateInfo};

    std::cout << "VertexInputStateInfo:" << std::endl;
    for (uint32_t i = 0; i < vertexInputCreateInfo.totalBindings(); ++i) {
        auto binding = vertexInputCreateInfo.binding(i);
        std::cout << "Binding #" << binding.binding << ": stride=" << binding.stride << ", input_rate="
                  << (binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX ?
                      "per_vertex" : "per_instance") << std::endl;
    }
    for (uint32_t i = 0; i < vertexInputCreateInfo.totalAttributes(); ++i) {
        auto attribute = vertexInputCreateInfo.attribute(i);
        std::cout << "Attribute #" << i << ": binding=" << attribute.binding << ", location=" << attribute.location
                  << ", offset=" << attribute.offset << std::endl;
    }


    // 12. create command buffer and sync primitives

    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};



    // 13. render on screen in a loop

    uint32_t framesTotal = 0;

    myUniform.displacement = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, -1.0f));

    while (!glfwWindowShouldClose(window)) {
        framesTotal++;
        glfwPollEvents();


        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

        if (extents.width == 0 || extents.height == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (fence->signaled()) {
            fence->wait();
            fence->reset();
        }

        try {
            mySwapChain->acquireNextImage(presentComplete, 1000);
            myUniform.displacement = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, -1.0f)), framesTotal / 2000.0f, glm::vec3(1.0f, 1.0f, 0.0f));
            memcpy(uBufMapped, &myUniform, sizeof(myUniform));
            uBuf.flush();

            commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            auto &currentFrameBuffer = framebuffers.at(mySwapChain->currentImage());
            std::array<VkClearValue, 2> values{};
            values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
            values.at(1).depthStencil.depth = 1.0f;
            values.at(1).depthStencil.stencil = 0.0f;

            commandBuffer.beginRenderPass(renderPass, currentFrameBuffer, currentFrameBuffer.getFullRenderArea(), false,
                                          values.size(), values.data());
            VkViewport viewport;

            viewport.height = currentFrameBuffer.getFullRenderArea().extent.height;
            viewport.width = currentFrameBuffer.getFullRenderArea().extent.width;
            viewport.x = viewport.y = 0.0f;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor;
            scissor.extent.width = currentFrameBuffer.getFullRenderArea().extent.width;
            scissor.extent.height = currentFrameBuffer.getFullRenderArea().extent.height;
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            commandBuffer.setViewports({viewport}, 0);
            commandBuffer.setScissors({scissor}, 0);
            commandBuffer.bindGraphicsPipeline(pipeline);
            commandBuffer.bindDescriptorSets(pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, myDescriptor, 0);
            commandBuffer.bindVertexBuffer(vBuf, 0, 0);
            commandBuffer.draw(attrs.size(), 1);
            commandBuffer.endRenderPass();
            commandBuffer.end();

            queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                          renderComplete);

        } catch (vkw::VulkanError &e) {
            if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
                mySwapChain.reset();
                mySwapChain = std::make_unique<MySwapChain>(MySwapChain{device, surface});
                swapChainImages.clear();
                swapChainImages = mySwapChain->retrieveImages();

                swapChainImageViews.clear();

                for (auto &image: swapChainImages) {
                    swapChainImageViews.emplace_back(
                            image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
                }

                extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

                depthMap.reset();
                depthMap.emplace(createDepthStencilImage(device, extents.width, extents.height));
                depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping, VK_FORMAT_D24_UNORM_S8_UINT);
                framebuffers.clear();

                for (auto &view: swapChainImageViews) {
                    framebuffers.emplace_back(device, renderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                              view.get().image()->rawExtents().height},
                                              vkw::Image2DArrayViewConstRefArray {view, *depthImageView});
                }

                //std::cout << "Window resized" << std::endl;
                continue;
            } else {
                throw;
            }
        }
        queue->present(*mySwapChain, renderComplete);
        queue->waitIdle();
    }


    // 14. clear resources and wait for device processes to finish

    if (fence->signaled()) {
        fence->wait();
        fence->reset();
    }

    device.core<1, 0>().vkDeviceWaitIdle(device);
    device.waitIdle();

    fence.reset();
    mySwapChain.reset();

    glfwDestroyWindow(window);

    return 0;
}
