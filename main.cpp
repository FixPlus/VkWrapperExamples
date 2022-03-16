#include <iostream>

#define VKW_EXTENSION_INITIALIZERS_MAP_DEFINITION 1

#include "vkw/SymbolTable.hpp"
#include "Instance.hpp"
#include "SceneProjector.h"
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
#include <thread>

#define STB_IMAGE_IMPLEMENTATION

#include "external/stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SwapChainImpl.h"
#include "RenderPassesImpl.h"
#include "CubeGeometry.h"

struct GlobalUniform {
    glm::mat4 perspective;
    glm::mat4 cameraSpace;
    glm::vec4 lightVec = {-1.0f, -1.0f, -1.0f, 0.0f};
} myUniform;

struct ShadowMapSpace {
    glm::mat4 cascades[TestApp::SHADOW_CASCADES_COUNT];
    float splits[TestApp::SHADOW_CASCADES_COUNT * 4];
} shadowMapSpace;

template<uint32_t Cascades>
void setShadowMapUniform(TestApp::ShadowCascadesCamera<Cascades> const& camera, ShadowMapSpace& ubo, glm::vec3 lightDir){
    auto greaterRadius = camera.cascade(Cascades - 1).radius;
    for(int i = 0; i < Cascades; ++i){
        auto cascade = camera.cascade(i);
        float shadowDepthFactor = 5.0f;
        auto center = cascade.center;
        auto shadowDepth = 2000.0f;
        if(shadowDepth < cascade.radius * shadowDepthFactor)
            shadowDepth = cascade.radius * shadowDepthFactor;
        glm::mat4 proj = glm::ortho(-cascade.radius, cascade.radius, cascade.radius, -cascade.radius, 0.0f, shadowDepth/*cascade.radius * shadowDepthFactor*/);
        glm::mat4 lookAt = glm::lookAt(center - glm::normalize(lightDir) * (shadowDepth - cascade.radius), center, glm::vec3{0.0f, 1.0f, 0.0f});
        ubo.cascades[i] = proj * lookAt;
        ubo.splits[i * 4] = cascade.split;
    }
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


vkw::DepthStencilImage2D createDepthStencilImage(vkw::Device &device, uint32_t width, uint32_t height) {
    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    auto depthMap = vkw::DepthStencilImage2D{device.getAllocator(), createInfo, VK_FORMAT_D32_SFLOAT, width,
                                             height, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

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

int main() {


    // 1. Create Instance and Window


    TestApp::SceneProjector window{800, 600, "Hello, Boez!"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = TestApp::Window::vulkanInstance(vulkanLib, {}, false);

    std::for_each(renderInstance.extensions_begin(), renderInstance.extensions_end(),
                  [](vkw::Instance::extension_const_iterator::value_type const &ext) {
                      std::cout << ext.first << std::endl;
                  });

    // 2. Create Device

    auto devs = renderInstance.enumerateAvailableDevices();

    vkw::PhysicalDevice deviceDesc{renderInstance, 0u};

    if (devs.empty()) {
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
        std::cout << "Device " << counter << ": " << props.deviceName << std::endl;
        counter++;
    }

    // 3. Create surface

    auto surface = window.surface(renderInstance);

    // 4. Create swapchain

    auto mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});

    auto queue = device.getGraphicsQueue();

    // 5. create vertex buffer

    // 6. load data to vertex buffer


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


    // 8. create swapchain images views for framebuffer

    std::vector<vkw::Image2DArrayViewCRef> swapChainImageViews;
    auto swapChainImages = mySwapChain->retrieveImages();
    for (auto &image: swapChainImages) {
        swapChainImageViews.emplace_back(image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
    }

    // 9. create render pass

    auto lightRenderPass = TestApp::LightPass(device, swapChainImageViews.front().get().format(), VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto shadowRenderPass = TestApp::ShadowPass(device, VK_FORMAT_D32_SFLOAT);

    // 10. create framebuffer for each swapchain image view

    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    std::optional<vkw::DepthStencilImage2D> depthMap = createDepthStencilImage(device, extents.width, extents.height);
    vkw::DepthImage2DArrayView const *depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping,
                                                                                                      VK_FORMAT_D32_SFLOAT);
    std::vector<vkw::FrameBuffer> framebuffers;

    try {
        for (auto &view: swapChainImageViews) {
            framebuffers.emplace_back(device, lightRenderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                          view.get().image()->rawExtents().height},
                                      vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
        }
    } catch(vkw::Error &e){
        std::cout << e.what() << std::endl;
        return 1;
    }

    // 11. create Cubes


    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    vkw::UniformBuffer<GlobalUniform> uBuf{device, createInfo};

    ShadowMapSpace shadowProjector{};

    vkw::UniformBuffer<ShadowMapSpace> shadowProjBuf{device, createInfo};

    createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    createInfo.requiredFlags = 0;

    constexpr const uint32_t shadowMapSize = 2048;
    vkw::DepthStencilImage2DArray shadowMap{device.getAllocator(), createInfo, VK_FORMAT_D32_SFLOAT, shadowMapSize, shadowMapSize, TestApp::SHADOW_CASCADES_COUNT, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
    std::vector<vkw::DepthImage2DArrayView const*> shadowMapAttachmentViews{};
    for(int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i){
        shadowMapAttachmentViews.emplace_back(&shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), i, 1, mapping));
    }
    auto& shadowMapSampledView = shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), 0, TestApp::SHADOW_CASCADES_COUNT, mapping);

    std::vector<vkw::FrameBuffer> shadowBuffers{};
    for(int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        shadowBuffers.emplace_back(device, shadowRenderPass, VkExtent2D{shadowMapSize, shadowMapSize}, *shadowMapAttachmentViews.at(i));
    }

    auto cubeTexture = loadTexture("image.png", device);
    auto& cubeTextureView = cubeTexture.getView<vkw::ColorImageView>(device, cubeTexture.format(), mapping);

    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.pNext = nullptr;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCI.anisotropyEnable = false;

    auto textureSampler = vkw::Sampler{device, samplerCI};

    constexpr const int cubeCount = 50000;

    TestApp::CubePool cubePool{device, cubeCount};


    auto* uBufMapped = uBuf.map();
    auto* uShadowGlobal = shadowProjBuf.map();


    setShadowMapUniform(window.camera(),shadowProjector, glm::vec3{myUniform.lightVec});


    memcpy(uShadowGlobal, &shadowProjector, sizeof(shadowProjector));


    std::vector<TestApp::Cube> cubes{};
    cubes.emplace_back(cubePool, glm::vec3{500.0f, -500.0f, 500.0f}, glm::vec3{1000.0f}, glm::vec3{0.0f});
    for(int i = 0; i < cubeCount - 1; ++i) {
        float scale_mag = (float)(rand() % 5) + 1.0f;
        glm::vec3 pos = glm::vec3((float)(rand() % 1000), (float)(rand() % 1000), (float)(rand() % 1000));
        glm::vec3 rotate = glm::vec3((float)(rand() % 1000), (float)(rand() % 1000), (float)(rand() % 1000));
        auto scale = glm::vec3(scale_mag);
        cubes.emplace_back(cubePool, pos, scale, rotate);
    }


    //cubes.emplace_back(cubePool, glm::vec3{10.0f, 2.0f, 10.0f}, glm::vec3{1.0f}, glm::vec3{0.0f});

    vkw::DescriptorSetLayout cubeLightDescriptorLayout{device, {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                              vkw::DescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                              vkw::DescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                vkw::DescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}};
    vkw::DescriptorSetLayout cubeShadowDescriptorLayout{device, {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}};

    auto cubeVertShader = loadShader<vkw::VertexShader>(device, "triangle.vert.spv");
    auto cubeShadowShader = loadShader<vkw::VertexShader>(device, "shadow.vert.spv");
    auto cubeShadowShowShader = loadShader<vkw::FragmentShader>(device, "shadow.frag.spv");
    auto cubeFragShader = loadShader<vkw::FragmentShader>(device, "triangle.frag.spv");

    vkw::PipelineLayout lightLayout{device, cubeLightDescriptorLayout};
    vkw::PipelineLayout shadowLayout{device, cubeShadowDescriptorLayout, {{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(uint32_t)}}};

    vkw::GraphicsPipelineCreateInfo lightCreateInfo{lightRenderPass, 0, lightLayout};
    vkw::GraphicsPipelineCreateInfo shadowShowCreateInfo{lightRenderPass, 0, shadowLayout};
    auto& vertexInputState = TestApp::CubePool::geometryInputState();

    for(int i = 0; i < vertexInputState.totalBindings(); ++i){
        auto binding = vertexInputState.binding(i);
        std::cout << "Binding #" << binding.binding << ": rate=" << (binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX ? "per_vertex" : "per_instance")
        << ", stride=" << binding.stride << std::endl;
    }

    for(int i = 0; i < vertexInputState.totalAttributes(); ++i){
        auto attribute = vertexInputState.attribute(i);
        std::cout << "Attribute #" << i << ": location=" << attribute.location << ", binding=" << attribute.binding << ", offset=" <<
        attribute.offset << std::endl;
    }

    lightCreateInfo.addVertexInputState(TestApp::CubePool::geometryInputState());
    lightCreateInfo.addVertexShader(cubeVertShader);
    lightCreateInfo.addFragmentShader(cubeFragShader);
    lightCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});
    lightCreateInfo.addRasterizationState(vkw::RasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE});
    vkw::GraphicsPipeline lightCubePipeline{device, lightCreateInfo};

    shadowShowCreateInfo.addVertexInputState(TestApp::CubePool::geometryInputState());
    shadowShowCreateInfo.addVertexShader(cubeShadowShader);
    shadowShowCreateInfo.addFragmentShader(cubeShadowShowShader);
    shadowShowCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});
    shadowShowCreateInfo.addRasterizationState(vkw::RasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE});
    vkw::GraphicsPipeline shadowShowPipeline{device, shadowShowCreateInfo};

    vkw::GraphicsPipelineCreateInfo shadowCreateInfo{shadowRenderPass, 0, shadowLayout};

    shadowCreateInfo.addVertexInputState(TestApp::CubePool::geometryInputState());
    shadowCreateInfo.addVertexShader(cubeShadowShader);
    shadowCreateInfo.addRasterizationState(vkw::RasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, true, 2.0f, 0.0f, 6.0f});
    shadowCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});

    vkw::GraphicsPipeline shadowCubePipeline{device, shadowCreateInfo};

    vkw::DescriptorPool descriptorPool{device, 2, {VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,.descriptorCount=3},
                                                   VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.descriptorCount=2}}};

    vkw::DescriptorSet shadowSet{descriptorPool, cubeShadowDescriptorLayout};
    shadowSet.write(0, shadowProjBuf);
    vkw::DescriptorSet lightSet{descriptorPool, cubeLightDescriptorLayout};
    lightSet.write(0, uBuf);
    lightSet.write(1, cubeTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, textureSampler);
    lightSet.write(2, shadowMapSampledView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, textureSampler);
    lightSet.write(3, shadowProjBuf);


    // 12. create command buffer and sync primitives

    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};


    // 13. render on screen in a loop

    uint32_t framesTotal = 0;
    uint32_t framesCount = 0;
    double elapsedTime = 0.0;
    double totalTime = 0.0;
    double fps = 0.0;
    std::chrono::time_point<std::chrono::high_resolution_clock,
            std::chrono::duration<double>> tStart, tFinish;

    tStart = std::chrono::high_resolution_clock::now();
    myUniform.perspective = window.camera().projection();

    constexpr const int thread_count = 12;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    while (!window.shouldClose()) {
        framesTotal++;

        TestApp::Window::pollEvents();

        tFinish = std::chrono::high_resolution_clock::now();

        double deltaTime = std::chrono::duration<double, std::milli>(tFinish - tStart).count() / 1000.0;
        elapsedTime += deltaTime;
        totalTime += deltaTime;

        tStart = std::chrono::high_resolution_clock::now();
        framesCount++;
        if(framesCount > 100){
            fps = (float)framesCount / elapsedTime;
            std::cout << "FPS: " << fps << std::endl;
            auto camPos = window.camera().position();
            std::cout << "x:" << camPos.x << " y:" << camPos.y << " z:" << camPos.z << std::endl;
            framesCount = 0;
            elapsedTime = 0;
        }

        window.update(deltaTime);

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



            myUniform.perspective = window.camera().projection();
            myUniform.cameraSpace = window.camera().cameraSpace();
            //myUniform.lightVec.x = glm::cos(totalTime);
            //myUniform.lightVec.y = -1.0f;
            //myUniform.lightVec.z = glm::sin(totalTime);
            glm::normalize(myUniform.lightVec);
            setShadowMapUniform(window.camera(),shadowProjector, glm::vec3{myUniform.lightVec});
            memcpy(uBufMapped, &myUniform, sizeof(myUniform));
            memcpy(uShadowGlobal, &shadowProjector, sizeof(shadowProjector));
            uBuf.flush();
            shadowProjBuf.flush();


            commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            auto &currentFrameBuffer = framebuffers.at(mySwapChain->currentImage());
            std::array<VkClearValue, 2> values{};
            values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
            values.at(1).depthStencil.depth = 1.0f;
            values.at(1).depthStencil.stencil = 0.0f;


            VkViewport viewport;

            viewport.height = shadowMapSize;
            viewport.width = shadowMapSize;
            viewport.x = viewport.y = 0.0f;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor;
            scissor.extent.width = shadowMapSize;
            scissor.extent.height = shadowMapSize;
            scissor.offset.x = 0;
            scissor.offset.y = 0;

            auto per_thread = cubes.size() / thread_count;


            threads.clear();

            for(int i = 0; i < thread_count;++i){
                threads.emplace_back([&cubes, i, per_thread, deltaTime](){
                    for(int j = per_thread * i; j < std::min(per_thread * (i + 1), cubes.size()); ++j)
                        if(j != 0)
                            cubes.at(j).update(deltaTime);

                });
            }

            for(auto& thread: threads)
                thread.join();


            for(int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
                commandBuffer.beginRenderPass(shadowRenderPass, shadowBuffers.at(i), shadowBuffers.at(i).getFullRenderArea(), false,
                                              1, values.data() + 1);

                commandBuffer.setViewports({viewport}, 0);
                commandBuffer.setScissors({scissor}, 0);

                commandBuffer.bindGraphicsPipeline(shadowCubePipeline);
                commandBuffer.bindDescriptorSets(shadowLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowSet, 0);
                cubePool.bindGeometry(commandBuffer);
                uint32_t id = i;

                commandBuffer.pushConstants(shadowLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, id);

                cubePool.drawGeometry(commandBuffer);

                commandBuffer.endRenderPass();
            }


            commandBuffer.beginRenderPass(lightRenderPass, currentFrameBuffer, currentFrameBuffer.getFullRenderArea(), false,
                                          values.size(), values.data());
            viewport.height = currentFrameBuffer.getFullRenderArea().extent.height;
            viewport.width = currentFrameBuffer.getFullRenderArea().extent.width;

            scissor.extent.width = currentFrameBuffer.getFullRenderArea().extent.width;
            scissor.extent.height = currentFrameBuffer.getFullRenderArea().extent.height;

            commandBuffer.setViewports({viewport}, 0);
            commandBuffer.setScissors({scissor}, 0);

            commandBuffer.bindGraphicsPipeline(lightCubePipeline);
            commandBuffer.bindDescriptorSets(lightLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, lightSet, 0);
            cubePool.bindGeometry(commandBuffer);
            cubePool.drawGeometry(commandBuffer);
#if 0
            viewport.y = currentFrameBuffer.getFullRenderArea().extent.height / 2.0f;

            commandBuffer.setViewports({viewport}, 0);
            commandBuffer.setScissors({scissor}, 0);

            commandBuffer.bindGraphicsPipeline(shadowShowPipeline);
            commandBuffer.bindDescriptorSets(shadowLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowSet, 0);
            cubePool.bindGeometry(commandBuffer);
            cubePool.drawGeometry(commandBuffer);
#endif
            commandBuffer.endRenderPass();
            commandBuffer.end();

            queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                          renderComplete);

        } catch (vkw::VulkanError &e) {
            if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
                mySwapChain.reset();
                mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});
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
                depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping,
                                                                                VK_FORMAT_D32_SFLOAT);
                framebuffers.clear();

                for (auto &view: swapChainImageViews) {
                    framebuffers.emplace_back(device, lightRenderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                                  view.get().image()->rawExtents().height},
                                              vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
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

    device.waitIdle();

    fence.reset();
    mySwapChain.reset();

    cubes.clear();

    return 0;
}
