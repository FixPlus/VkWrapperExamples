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

#include "tiny_gltf/stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SwapChainImpl.h"
#include "RenderPassesImpl.h"
#include "CubeGeometry.h"
#include "AssetImport.h"
#include "GUI.h"
#include "AssetPath.inc"
#include "Utils.h"

using namespace TestApp;

class GUI : public GUIFrontEnd, public GUIBackend {
public:
    GUI(TestApp::Window &window, vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
        ShaderLoader const &shaderLoader,
        TextureLoader const &textureLoader)
            : GUIBackend(device, pass, subpass, shaderLoader, textureLoader),
              m_window(window) {
        ImGui::SetCurrentContext(context());
        auto &io = ImGui::GetIO();

        ImGuiStyle &style = ImGui::GetStyle();
        style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
        style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);

        m_updateFontTexture();

        io.DisplaySize = {800, 600};
    }

    std::function<void(void)> customGui = []() {};

protected:
    void gui() const override {
        ImGui::SetNextWindowSize({300.0f, 100.0f}, ImGuiCond_Once);
        ImGui::Begin("Hello");
        static std::string buf;
        buf.resize(10);
        ImGui::InputText("<- type here", buf.data(), buf.size());
        ImGui::Text("FPS: %.2f", m_window.get().clock().fps());
        ImGui::End();

        customGui();
    }

private:
    std::reference_wrapper<TestApp::Window> m_window;

};


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
void
setShadowMapUniform(TestApp::ShadowCascadesCamera<Cascades> const &camera, ShadowMapSpace &ubo, glm::vec3 lightDir) {
    auto greaterRadius = camera.cascade(Cascades - 1).radius;
    for (int i = 0; i < Cascades; ++i) {
        auto cascade = camera.cascade(i);
        float shadowDepthFactor = 5.0f;
        auto center = cascade.center;
        auto shadowDepth = 2000.0f;
        if (shadowDepth < cascade.radius * shadowDepthFactor)
            shadowDepth = cascade.radius * shadowDepthFactor;
        glm::mat4 proj = glm::ortho(-cascade.radius, cascade.radius, cascade.radius, -cascade.radius, 0.0f,
                                    shadowDepth/*cascade.radius * shadowDepthFactor*/);
        glm::mat4 lookAt = glm::lookAt(center - glm::normalize(lightDir) * (shadowDepth - cascade.radius), center,
                                       glm::vec3{0.0f, 1.0f, 0.0f});
        ubo.cascades[i] = proj * lookAt;
        ubo.splits[i * 4] = cascade.split;
    }
}

int main() {


    // 1. Create Instance and Window


    TestApp::SceneProjector window{800, 600, "Hello, Boez!"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = TestApp::Window::vulkanInstance(vulkanLib, {}, true);

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

    auto lightRenderPass = TestApp::LightPass(device, swapChainImageViews.front().get().format(), VK_FORMAT_D32_SFLOAT,
                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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
    } catch (vkw::Error &e) {
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
    vkw::DepthStencilImage2DArray shadowMap{device.getAllocator(), createInfo, VK_FORMAT_D32_SFLOAT, shadowMapSize,
                                            shadowMapSize, TestApp::SHADOW_CASCADES_COUNT, 1,
                                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
    std::vector<vkw::DepthImage2DArrayView const *> shadowMapAttachmentViews{};
    for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        shadowMapAttachmentViews.emplace_back(
                &shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), i, 1, mapping));
    }
    auto &shadowMapSampledView = shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), 0,
                                                                        TestApp::SHADOW_CASCADES_COUNT, mapping);

    std::vector<vkw::FrameBuffer> shadowBuffers{};
    for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        shadowBuffers.emplace_back(device, shadowRenderPass, VkExtent2D{shadowMapSize, shadowMapSize},
                                   *shadowMapAttachmentViews.at(i));
    }

    TestApp::TextureLoader textureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};
    auto cubeTexture = textureLoader.loadTexture("image");
    auto &cubeTextureView = cubeTexture.getView<vkw::ColorImageView>(device, cubeTexture.format(), mapping);

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


    auto *uBufMapped = uBuf.map();
    auto *uShadowGlobal = shadowProjBuf.map();


    setShadowMapUniform(window.camera(), shadowProjector, glm::vec3{myUniform.lightVec});


    memcpy(uShadowGlobal, &shadowProjector, sizeof(shadowProjector));


    std::vector<TestApp::Cube> cubes{};
    cubes.emplace_back(cubePool, glm::vec3{500.0f, -500.0f, 500.0f}, glm::vec3{1000.0f}, glm::vec3{0.0f}).makeStatic();
    for (int i = 0; i < cubeCount - 1; ++i) {
        float scale_mag = (float) (rand() % 5) + 1.0f;
        glm::vec3 pos = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
        glm::vec3 rotate = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
        auto scale = glm::vec3(scale_mag);
        cubes.emplace_back(cubePool, pos, scale, rotate);
    }

    vkw::DescriptorSetLayout cubeLightDescriptorLayout{device, {vkw::DescriptorSetLayoutBinding{0,
                                                                                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                                vkw::DescriptorSetLayoutBinding{1,
                                                                                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                vkw::DescriptorSetLayoutBinding{2,
                                                                                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                vkw::DescriptorSetLayoutBinding{3,
                                                                                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}};
    vkw::DescriptorSetLayout cubeShadowDescriptorLayout{device, {vkw::DescriptorSetLayoutBinding{0,
                                                                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}};

    TestApp::ShaderLoader shaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto cubeVertShader = shaderLoader.loadVertexShader("triangle");
    auto cubeShadowShader = shaderLoader.loadVertexShader("shadow");
    auto cubeShadowShowShader = shaderLoader.loadFragmentShader("shadow");
    auto cubeFragShader = shaderLoader.loadFragmentShader("triangle");

    vkw::PipelineLayout lightLayout{device, cubeLightDescriptorLayout};
    vkw::PipelineLayout shadowLayout{device, cubeShadowDescriptorLayout,
                                     {{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(uint32_t)}}};

    vkw::GraphicsPipelineCreateInfo lightCreateInfo{lightRenderPass, 0, lightLayout};
    vkw::GraphicsPipelineCreateInfo shadowShowCreateInfo{lightRenderPass, 0, shadowLayout};
    auto &vertexInputState = TestApp::CubePool::geometryInputState();

    lightCreateInfo.addVertexInputState(TestApp::CubePool::geometryInputState());
    lightCreateInfo.addVertexShader(cubeVertShader);
    lightCreateInfo.addFragmentShader(cubeFragShader);
    lightCreateInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    lightCreateInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    lightCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});
    lightCreateInfo.addRasterizationState(
            vkw::RasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                              VK_FRONT_FACE_COUNTER_CLOCKWISE});
    vkw::GraphicsPipeline lightCubePipeline{device, lightCreateInfo};

    shadowShowCreateInfo.addVertexInputState(TestApp::CubePool::geometryInputState());
    shadowShowCreateInfo.addVertexShader(cubeShadowShader);
    shadowShowCreateInfo.addFragmentShader(cubeShadowShowShader);
    shadowShowCreateInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    shadowShowCreateInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    shadowShowCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});
    shadowShowCreateInfo.addRasterizationState(
            vkw::RasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                              VK_FRONT_FACE_COUNTER_CLOCKWISE});
    vkw::GraphicsPipeline shadowShowPipeline{device, shadowShowCreateInfo};

    vkw::GraphicsPipelineCreateInfo shadowCreateInfo{shadowRenderPass, 0, shadowLayout};

    shadowCreateInfo.addVertexInputState(TestApp::CubePool::geometryInputState());
    shadowCreateInfo.addVertexShader(cubeShadowShader);
    shadowCreateInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    shadowCreateInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    shadowCreateInfo.addRasterizationState(
            vkw::RasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                              VK_FRONT_FACE_COUNTER_CLOCKWISE, true, 2.0f, 0.0f, 6.0f});
    shadowCreateInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS_OR_EQUAL, true});

    vkw::GraphicsPipeline shadowCubePipeline{device, shadowCreateInfo};

    vkw::DescriptorPool descriptorPool{device, 2,
                                       {VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=3},
                                        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount=2}}};

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

    GUI gui{window, device, lightRenderPass, 0, shaderLoader, textureLoader};

    window.setContext(gui);

    gui.customGui = [&window]() {
        ImGui::SetNextWindowSize({300, 200}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);
        static float splitLambda = 0.5f;
        ImGui::SliderFloat("Split lambda", &splitLambda, 0.0f, 1.0f);
        window.camera().setSplitLambda(splitLambda);
        ImGui::End();
    };

    // 13. render on screen in a loop

    myUniform.perspective = window.camera().projection();

    constexpr const int thread_count = 12;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    while (!window.shouldClose()) {

        TestApp::Window::pollEvents();


        window.update();
        gui.frame();
        gui.push();

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
            setShadowMapUniform(window.camera(), shadowProjector, glm::vec3{myUniform.lightVec});
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

            auto deltaTime = window.clock().frameTime();

            for (int i = 0; i < thread_count; ++i) {
                threads.emplace_back([&cubes, i, per_thread, deltaTime]() {
                    for (int j = per_thread * i; j < std::min(per_thread * (i + 1), cubes.size()); ++j)
                        cubes.at(j).update(deltaTime);

                });
            }

            for (auto &thread: threads)
                thread.join();


            for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
                commandBuffer.beginRenderPass(shadowRenderPass, shadowBuffers.at(i),
                                              shadowBuffers.at(i).getFullRenderArea(), false,
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


            commandBuffer.beginRenderPass(lightRenderPass, currentFrameBuffer, currentFrameBuffer.getFullRenderArea(),
                                          false,
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

            gui.draw(commandBuffer);
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
                    framebuffers.emplace_back(device, lightRenderPass,
                                              VkExtent2D{view.get().image()->rawExtents().width,
                                                         view.get().image()->rawExtents().height},
                                              vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
                }
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
