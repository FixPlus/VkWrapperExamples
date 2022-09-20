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
#include <RenderEngine/Shaders/ShaderLoader.h>

#include "SwapChainImpl.h"
#include "RenderPassesImpl.h"
#include "CubeGeometry.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include "GUI.h"
#include "AssetPath.inc"
#include "Utils.h"
#include "ShadowPass.h"
#include "RenderEngine/Window/Boxer.h"
#include "ErrorCallbackWrapper.h"

using namespace TestApp;

class GUI : public GUIFrontEnd, public GUIBackend {
public:
    GUI(TestApp::SceneProjector &window, vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
        RenderEngine::TextureLoader const &textureLoader)
            : GUIBackend(device, pass, subpass, textureLoader),
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
        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.2f", m_window.get().clock().fps());
        auto &camera = m_window.get().camera();
        auto pos = camera.position();
        ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f,", pos.x, pos.y, pos.z);
        ImGui::Text("(%.2f,%.2f)", camera.phi(), camera.psi());

        ImGui::SliderFloat("Cam rotate inertia", &camera.rotateInertia, 0.1f, 5.0f);
        ImGui::SliderFloat("Mouse sensitivity", &m_window.get().mouseSensitivity, 1.0f, 10.0f);

        ImGui::End();

        customGui();
    }

private:
    std::reference_wrapper<TestApp::SceneProjector> m_window;

};

class GlobalLayout {
public:

    struct LightUniform {
        glm::vec4 lightVec = glm::normalize(glm::vec4{-0.37, 0.37, -0.85, 0.0f});
        glm::vec4 skyColor = glm::vec4{158.0f, 146.0f, 144.0f, 255.0f} / 255.0f;
        glm::vec4 lightColor = glm::vec4{244.0f, 218.0f, 62.0f, 255.0f} / 255.0f;
    } light;

    GlobalLayout(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, TestApp::Camera const &camera,
                 ShadowRenderPass &shadowPass, vkw::Sampler &sampler) :
            m_camera_projection_layout(device,
                                       RenderEngine::SubstageDescription{.shaderSubstageName="perspective", .setBindings={
                                               vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                                       1),
            m_light_layout(device,
                           RenderEngine::LightingLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="sunlightShadowed", .setBindings={
                                   vkw::DescriptorSetLayoutBinding{0,
                                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                   vkw::DescriptorSetLayoutBinding{1,
                                                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                   vkw::DescriptorSetLayoutBinding{2,
                                                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, .pass=pass, .subpass=subpass},
                           1),
            m_camera(camera),
            m_light(device, m_light_layout, light, shadowPass, 4, sampler),
            m_camera_projection(device, m_camera_projection_layout) {
    }

    void bind(RenderEngine::GraphicsRecordingState &state) const {
        state.setProjection(m_camera_projection);
        state.setLighting(m_light);
    }

    void update() {
        m_light.update(light);
        m_camera_projection.update(m_camera);
    }

    TestApp::Camera const &camera() const {
        return m_camera.get();
    }

    uint32_t shadowTextureSize() const {
        return 2048;
    }

private:
    RenderEngine::ProjectionLayout m_camera_projection_layout;

    struct CameraProjection : public RenderEngine::Projection {
        struct ProjectionUniform {
            glm::mat4 perspective;
            glm::mat4 cameraSpace;
        } ubo;

        CameraProjection(vkw::Device &device, RenderEngine::ProjectionLayout &layout) : RenderEngine::Projection(
                layout), uniform(device,
                                 VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                                        mapped(uniform.map()) {
            set().write(0, uniform);
            *mapped = ubo;
            uniform.flush();
        }

        void update(TestApp::Camera const &camera) {
            ubo.perspective = camera.projection();
            ubo.cameraSpace = camera.cameraSpace();
            *mapped = ubo;
            uniform.flush();
        }

        vkw::UniformBuffer<ProjectionUniform> uniform;
        ProjectionUniform *mapped;
    } m_camera_projection;

    RenderEngine::LightingLayout m_light_layout;

    struct Light : public RenderEngine::Lighting {


        Light(vkw::Device &device, RenderEngine::LightingLayout &layout, LightUniform const &ubo, ShadowRenderPass &pass,
              uint32_t cascades, vkw::Sampler &sampler)
                : RenderEngine::Lighting(layout), uniform(device,
                                                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                  m_shadow_map_view(device, pass.shadowMap(), pass.shadowMap().format(), 0, cascades),
                  mapped(uniform.map()){

            auto& shadowCascades = pass.shadowMap();
            set().write(0, m_shadow_map_view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
            set().write(1, pass.ubo());
            set().write(2, uniform);
            *mapped = ubo;
            uniform.flush();
        }

        void update(LightUniform const &ubo) {
            *mapped = ubo;
            uniform.flush();
        }

        vkw::UniformBuffer<LightUniform> uniform;
        vkw::ImageView<vkw::DEPTH, vkw::V2DA> m_shadow_map_view;
        LightUniform *mapped;
    } m_light;

    std::reference_wrapper<TestApp::Camera const> m_camera;
};

class TexturedSurface {
public:
    TexturedSurface(vkw::Device &device, RenderEngine::TextureLoader &loader, vkw::Sampler &sampler,
                    std::string const &imageName) :
            m_layout(device,
                     RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="texture", .setBindings={
                             vkw::DescriptorSetLayoutBinding{0,
                                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .rasterizationState=vkw::RasterizationStateCreateInfo(0u, 0u, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT), .depthTestState=vkw::DepthTestStateCreateInfo{
                             VK_COMPARE_OP_LESS, VK_TRUE}, .maxMaterials=1}),
            m_material(device, m_layout, loader, sampler, imageName) {
    }


    RenderEngine::Material const &get() const {
        return m_material;
    }

private:
    RenderEngine::MaterialLayout m_layout;

    struct M_TexturedSurface : public RenderEngine::Material {
        M_TexturedSurface(vkw::Device &device, RenderEngine::MaterialLayout &layout,
                          RenderEngine::TextureLoader &loader, vkw::Sampler &sampler, std::string const &imageName) :
                RenderEngine::Material(layout), m_texture(loader.loadTexture(imageName)),
                m_texture_view(device, m_texture, m_texture.format()){
            set().write(0, m_texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
        }

    private:
        vkw::Image<vkw::COLOR, vkw::I2D> m_texture;
        vkw::ImageView<vkw::COLOR, vkw::V2D> m_texture_view;
    } m_material;
};

int runCubes() {


    // 1. Create Instance and Window


    TestApp::SceneProjector window{800, 600, "Cubes"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = RenderEngine::Window::vulkanInstance(vulkanLib, {}, vulkanLib.hasLayer("VK_LAYER_KHRONOS_validation"));

    // 2. Enumerate available devices

    auto devs = renderInstance.enumerateAvailableDevices();

    if (devs.empty()) {
        std::cout << "No available devices supporting vulkan on this machine." << std::endl <<
                  " Make sure your graphics drivers are installed and updated." << std::endl;
        return 1;
    }


    uint32_t counter = 0;
    for (auto &d: devs) {
        VkPhysicalDeviceProperties props{};
        renderInstance.core<1, 0>().vkGetPhysicalDeviceProperties(d, &props);
        std::cout << "Device " << counter << ": " << props.deviceName << std::endl;
        counter++;
    }

    // 3. Pick first in the list

    vkw::PhysicalDevice deviceDesc{renderInstance, 0u};

    // 4. enable needed device extensions and create logical device

    deviceDesc.enableExtension(vkw::ext::KHR_swapchain);

    std::cout << deviceDesc.isFeatureSupported(vkw::device::feature::multiViewport) << std::endl;

    auto device = vkw::Device{renderInstance, deviceDesc};

    // 5. Create surface

    auto surface = window.surface(renderInstance);

    // 6. Create swapchain, present queue and fence for sync

    auto mySwapChain = TestApp::SwapChainImpl{device, surface};


    auto queue = device.getGraphicsQueue();

    auto fence = vkw::Fence(device);

    // 7. create command pool

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        device.getGraphicsQueue()->familyIndex()};

    // 8. create swapchain images views for framebuffer

    std::vector<vkw::ImageView<vkw::COLOR, vkw::V2DA>> swapChainImageViews;
    auto swapChainImages = mySwapChain.retrieveImages();
    for (auto &image: swapChainImages) {
        swapChainImageViews.emplace_back(device, image, image.format(), 0, 1);
    }

    // 9. create render pass

    auto lightRenderPass = TestApp::LightPass(device, swapChainImageViews.front().format(), VK_FORMAT_D32_SFLOAT,
                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // 10. create framebuffer for each swapchain image view

    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    auto depthMap = createDepthStencilImage(device, extents.width, extents.height);
    auto depthImageView = vkw::ImageView<vkw::DEPTH, vkw::V2DA>(device, depthMap, depthMap.format());
    std::vector<vkw::FrameBuffer> framebuffers;

    try {
        for (auto &view: swapChainImageViews) {
            std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> views{&view, &depthImageView};
            framebuffers.emplace_back(device, lightRenderPass, VkExtent2D{view.image()->rawExtents().width,
                                                                          view.image()->rawExtents().height},
                                      std::span<vkw::ImageViewVT<vkw::V2DA> const*>{views.begin(), views.end()});
        }
    } catch (vkw::Error &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    // 11. create Shadow pass

    auto shadow = ShadowRenderPass(device);


    // 12. Create asset loaders

    RenderEngine::TextureLoader textureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};
    RenderEngine::ShaderImporter shaderImporter{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    RenderEngine::ShaderLoader shaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto cubeTexture = textureLoader.loadTexture("image");
    auto cubeTextureView = vkw::ImageView<vkw::COLOR, vkw::V2D>(device, cubeTexture, cubeTexture.format());

    // 13. create global layout

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

    auto globals = GlobalLayout(device, lightRenderPass, 0, window.camera(), shadow, textureSampler);

    // 14. create cubes

    constexpr const int cubeCount = 50000;

    TestApp::CubePool cubePool{device, cubeCount};

    shadow.update(window.camera(), glm::vec3{globals.light.lightVec});

    std::vector<TestApp::Cube> cubes{};
    cubes.emplace_back(cubePool, glm::vec3{500.0f, -500.0f, 500.0f}, glm::vec3{1000.0f}, glm::vec3{0.0f}).makeStatic();
    for (int i = 0; i < cubeCount - 1; ++i) {
        float scale_mag = (float) (rand() % 5) + 1.0f;
        glm::vec3 pos = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
        glm::vec3 rotate = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
        auto scale = glm::vec3(scale_mag);
        cubes.emplace_back(cubePool, pos, scale, rotate);
    }

    auto texturedSurface = TexturedSurface(device, textureLoader, textureSampler, "image");

    // 15. create command buffer and sync primitives

    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    // 16. Create pipeline pool and recorder state abstraction

    auto pipelinePool = RenderEngine::GraphicsPipelinePool{device, shaderLoader};
    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    // 17. Create GUI

    GUI gui{window, device, lightRenderPass, 0, textureLoader};

    window.setContext(gui);

    gui.customGui = [&window]() {
        ImGui::SetNextWindowSize({300, 200}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);
        static float splitLambda = 0.5f;
        ImGui::SliderFloat("Split lambda", &splitLambda, 0.0f, 1.0f);
        window.camera().setSplitLambda(splitLambda);
        ImGui::End();
    };

    shadow.onPass = [&cubePool](RenderEngine::GraphicsRecordingState& state, const Camera& camera){
        cubePool.bind(state);
        state.bindPipeline();
        cubePool.draw(state);
    };

    // 18. render on screen in a loop

    constexpr const int thread_count = 12;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    while (!window.shouldClose()) {

        RenderEngine::Window::pollEvents();

        static bool firstEncounter = true;
        if(!firstEncounter) {
            fence.wait();
            fence.reset();
        }
        else
            firstEncounter = false;

        window.update();
        gui.frame();
        gui.push();
        recorder.reset();
        globals.update();
        shadow.update(window.camera(), globals.light.lightVec);

        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

        if (extents.width == 0 || extents.height == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            firstEncounter = true;
            continue;
        }



        try {
            mySwapChain.acquireNextImage(presentComplete, 1000);
        } catch (vkw::VulkanError &e) {
            if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
                {
                    auto dummy = std::move(mySwapChain);
                }

                mySwapChain = TestApp::SwapChainImpl{device, surface};
                swapChainImages.clear();
                swapChainImages = mySwapChain.retrieveImages();

                swapChainImageViews.clear();

                for (auto &image: swapChainImages) {
                    swapChainImageViews.emplace_back(device, image, image.format(), 0, 1);
                }

                extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

                depthMap = createDepthStencilImage(device, extents.width, extents.height);
                depthImageView = vkw::ImageView<vkw::DEPTH, vkw::V2DA>(device, depthMap, depthMap.format());

                framebuffers.clear();

                for (auto &view: swapChainImageViews) {
                    std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> views{&view, &depthImageView};
                    framebuffers.emplace_back(device, lightRenderPass, VkExtent2D{view.image()->rawExtents().width,
                                                                                  view.image()->rawExtents().height},
                                              std::span<vkw::ImageViewVT<vkw::V2DA> const*>{views.begin(), views.end()});
                }
                firstEncounter = true;
                continue;
            } else {
                throw;
            }
        }



        commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        auto &currentFrameBuffer = framebuffers.at(mySwapChain.currentImage());
        std::array<VkClearValue, 2> values{};
        values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
        values.at(1).depthStencil.depth = 1.0f;
        values.at(1).depthStencil.stencil = 0.0f;


        VkViewport viewport;

        viewport.height = globals.shadowTextureSize();
        viewport.width = globals.shadowTextureSize();
        viewport.x = viewport.y = 0.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor;
        scissor.extent.width = globals.shadowTextureSize();
        scissor.extent.height = globals.shadowTextureSize();
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

        shadow.execute(commandBuffer, recorder);

        commandBuffer.beginRenderPass(lightRenderPass, currentFrameBuffer, currentFrameBuffer.getFullRenderArea(),
                                      false,
                                      values.size(), values.data());
        viewport.height = currentFrameBuffer.getFullRenderArea().extent.height;
        viewport.width = currentFrameBuffer.getFullRenderArea().extent.width;

        scissor.extent.width = currentFrameBuffer.getFullRenderArea().extent.width;
        scissor.extent.height = currentFrameBuffer.getFullRenderArea().extent.height;

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        globals.bind(recorder);
        recorder.setMaterial(texturedSurface.get());

        recorder.bindPipeline();
        cubePool.draw(recorder);


        gui.draw(recorder);
        commandBuffer.endRenderPass();
        commandBuffer.end();

        queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                      renderComplete, &fence);


        queue->present(mySwapChain, renderComplete);
    }


    // 19. clear resources and wait for device processes to finish

    fence.wait();
    fence.reset();

    device.waitIdle();

    return 0;
}



int main(){
    return ErrorCallbackWrapper<runCubes>::run();
}
