#include <SceneProjector.h>
#include <RenderEngine/AssetImport/AssetImport.h>
#include <vkw/Library.hpp>
#include <SwapChainImpl.h>
#include <vkw/Queue.hpp>
#include <vkw/Fence.hpp>
#include "Utils.h"
#include <RenderPassesImpl.h>
#include <Semaphore.hpp>
#include <RenderEngine/Shaders/ShaderLoader.h>
#include "GUI.h"
#include "AssetPath.inc"
#include "Model.h"
#include "GlobalLayout.h"
#include "WaterSurface.h"
#include "LandSurface.h"
#include "SkyBox.h"
#include "ShadowPass.h"
#include "RenderEngine/Window/Boxer.h"
#include "ErrorCallbackWrapper.h"
#include "Validation.hpp"

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

int runWaves() {
    TestApp::SceneProjector window{800, 600, "Waves"};

    vkw::Library vulkanLib{};

    auto validationPossible = vulkanLib.hasLayer(vkw::layer::KHRONOS_validation);

    if(!validationPossible)
        std::cout << "Validation unavailable" << std::endl;
    else
        std::cout << "Validation enabled" << std::endl;

    vkw::InstanceCreateInfo createInfo{};

    if(validationPossible) {
        createInfo.requestLayer(vkw::layer::KHRONOS_validation);
        createInfo.requestExtension(vkw::ext::EXT_debug_utils);
    }

    vkw::Instance renderInstance = RenderEngine::Window::vulkanInstance(vulkanLib, createInfo);

    std::optional<vkw::debug::Validation> validation;

    if(validationPossible)
        validation.emplace(renderInstance);
    auto devs = renderInstance.enumerateAvailableDevices();

    if (devs.empty()) {
        std::cout << "No available devices supporting vulkan on this machine." << std::endl <<
                  " Make sure your graphics drivers are installed and updated." << std::endl;
        return 1;
    }

    vkw::PhysicalDevice deviceDesc{renderInstance, 0u};

    TestApp::requestQueues(deviceDesc, true, true);

    deviceDesc.enableExtension(vkw::ext::KHR_swapchain);

    // to support wireframe display
    deviceDesc.enableFeature(vkw::PhysicalDevice::feature::fillModeNonSolid);

    // to support anisotropy filtering (if possible) in ocean
    if(deviceDesc.isFeatureSupported(vkw::PhysicalDevice::feature::samplerAnisotropy)) {
        std::cout << "Sampler anisotropy enabled" << std::endl;
        deviceDesc.enableFeature(vkw::PhysicalDevice::feature::samplerAnisotropy);
    } else{
        std::cout << "Sampler anisotropy disabled" << std::endl;
    }

    auto device = vkw::Device{renderInstance, deviceDesc};

    auto surface = window.surface(renderInstance);
    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    auto mySwapChain = TestApp::SwapChainImpl{device, surface, true};

    TestApp::LightPass lightPass = TestApp::LightPass(device, mySwapChain.attachments().front().format(),
                                                      mySwapChain.depthAttachment().format(),
                                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    auto shadowPass = TestApp::ShadowRenderPass{device};
    std::vector<vkw::FrameBuffer> framebuffers;

    for (auto &attachment: mySwapChain.attachments()) {
        std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2 > views = {&attachment, &mySwapChain.depthAttachment()};
        framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                {views.begin(), views.end()}});
    }

    auto queue = device.anyGraphicsQueue();
    auto fence = vkw::Fence{device};

    auto shaderLoader = RenderEngine::ShaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto textureLoader = RenderEngine::TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};
    auto pipelinePool = RenderEngine::GraphicsPipelinePool(device, shaderLoader);

    GUI gui = GUI{window, device, lightPass, 0, textureLoader};


    window.setContext(gui);

    auto skybox = SkyBox{device, lightPass, 0, shaderLoader};
    skybox.update(window.camera());
    skybox.recomputeOutScatter();

    auto globalState = GlobalLayout{device, lightPass, 0, window.camera(), shadowPass, skybox};
    auto globalStateSettings = GlobalLayoutSettings{gui, globalState};
    auto waveSurfaceTexture = WaveSurfaceTexture(device, shaderLoader, 256, 3);
    auto waves = WaterSurface(device, waveSurfaceTexture);
    auto waveMaterial = WaterMaterial{device, waveSurfaceTexture};
    auto waveMaterialWireframe = WaterMaterial{device, waveSurfaceTexture, true};


    auto land = LandSurface(device);
    auto landMaterial = LandMaterial{device};
    auto landMaterialWireframe = LandMaterial{device, true};


    window.camera().set(glm::vec3{0.0f, 25.0f, 0.0f});
    window.camera().setOrientation(172.0f, 15.0f, 0.0f);


    // Compute queue and buffer

    auto computeQueue = device.getSpecificQueue(TestApp::dedicatedCompute());
    auto computeCommandPool = vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                               computeQueue.family().index());
    auto computeCommandBuffer = vkw::PrimaryCommandBuffer(computeCommandPool);

    auto computeImageReady = vkw::Semaphore(device);
    auto syncCSubmitInfo = vkw::SubmitInfo{std::span<vkw::Semaphore const>{&computeImageReady, 0}, {}, std::span<vkw::Semaphore const>{&computeImageReady, 1}};
    computeQueue.submit(syncCSubmitInfo);
    auto computeImageRelease = vkw::Semaphore(device);


    // 3D queue and buffer

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};
    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    auto computeSubmitInfo = vkw::SubmitInfo{computeCommandBuffer, computeImageRelease, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, computeImageReady};

    auto submitInfo = vkw::SubmitInfo{commandBuffer, std::array<vkw::SemaphoreCRef, 2>{computeImageReady, presentComplete},
                                             {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                                             std::array<vkw::SemaphoreCRef, 2>{computeImageRelease, renderComplete}};

    auto waveSettings = WaveSettings{gui, waves, waveSurfaceTexture,
                                     {{"solid", waveMaterial}, {"wireframe", waveMaterialWireframe}}};
    auto landSettings = LandSettings{gui, land, {{"solid", landMaterial}, {"wireframe", landMaterialWireframe}}};
    auto skyboxSettings = SkyBoxSettings{gui, skybox, "Skybox"};

    shadowPass.onPass = [&land, &globalState, &landSettings](RenderEngine::GraphicsRecordingState &state,
                                                             const Camera &camera) {
        if (landSettings.enabled())
            land.draw(state, globalState.camera().position(), camera);
    };
    gui.customGui = [&globalState, &skybox, &window]() {

        ImGui::Begin("Globals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        static float splitL = window.camera().splitLambda();
        if (ImGui::SliderFloat("Split lambda", &splitL, 0.0f, 1.0f)) {
            window.camera().setSplitLambda(splitL);
        }

        static float farClip = window.camera().farClip();
        if (ImGui::SliderFloat("Far clip", &farClip, window.camera().nearPlane(), window.camera().farPlane())) {
            window.camera().setFarClip(farClip);
        }
        ImGui::End();
    };

    // Prerecord compute command buffer

    computeCommandBuffer.begin(0);

    waveSurfaceTexture.acquireOwnership(computeCommandBuffer, commandBuffer.queueFamily(),
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    waveSurfaceTexture.dispatch(computeCommandBuffer);
    waveSurfaceTexture.releaseOwnership(computeCommandBuffer, commandBuffer.queueFamily(),
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

    computeCommandBuffer.end();


    window.camera().setFarPlane(10000.0f);
    while (!window.shouldClose()) {
        window.pollEvents();


        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

        static bool firstEncounter = true;
        if (!firstEncounter) {
            fence.wait();
            fence.reset();
        } else
            firstEncounter = false;

        window.update();
        gui.frame();
        gui.push();
        globalState.update();
        skybox.update(window.camera());
        waveSurfaceTexture.update(window.clock().frameTime());
        waves.update(window.clock().frameTime());
        waveMaterial.update();
        waveMaterialWireframe.update();
        land.update();
        landMaterial.update();
        landMaterialWireframe.update();
        shadowPass.update(window.camera(), skybox.sunDirection());

        if (waveSettings.needUpdateStaticSpectrum()) {
            waveSurfaceTexture.computeSpectrum();
            waveSettings.resetUpdateSpectrum();
        }

        if(skyboxSettings.needRecomputeOutScatter()){
            skybox.recomputeOutScatter();
        }

        if (window.minimized()) {
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

                framebuffers.clear();

                for (auto &attachment: mySwapChain.attachments()) {
                    std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2 > views = {&attachment, &mySwapChain.depthAttachment()};
                    framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                            {views.begin(), views.end()}});
                }
                firstEncounter = true;
                continue;
            } else {
                throw;
            }
        }

        recorder.reset();


        commandBuffer.begin(0);

        shadowPass.execute(commandBuffer, recorder);


        std::array<VkClearValue, 2> values{};
        values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
        values.at(1).depthStencil.depth = 1.0f;
        values.at(1).depthStencil.stencil = 0.0f;


        VkViewport viewport;

        viewport.height = extents.height;
        viewport.width = extents.width;
        viewport.x = viewport.y = 0.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor;
        scissor.extent.width = extents.width;
        scissor.extent.height = extents.height;
        scissor.offset.x = 0;
        scissor.offset.y = 0;


        auto currentImage = mySwapChain.currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        waveSurfaceTexture.acquireOwnershipFrom(commandBuffer, computeCommandBuffer.queueFamily(),
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        if(!globalStateSettings.useSimpleLighting())
            skybox.draw(recorder);

        globalState.bind(recorder, globalStateSettings.useSimpleLighting());

        if (landSettings.enabled()) {
            recorder.setMaterial(landSettings.pickedMaterial().get());
            land.draw(recorder, globalState.camera().position(), globalState.camera());
        }

        if (waveSettings.enabled()) {
            recorder.setMaterial(waveSettings.pickedMaterial().get());
            waves.draw(recorder, globalState.camera().position(), globalState.camera());
        }


        gui.draw(recorder);


        commandBuffer.endRenderPass();

        waveSurfaceTexture.releaseOwnershipTo(commandBuffer, computeCommandBuffer.queueFamily(),
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        commandBuffer.end();


        queue.submit(submitInfo);

        computeQueue.submit(computeSubmitInfo, fence);

        auto presentInfo = vkw::PresentInfo{mySwapChain, renderComplete};
        queue.present(presentInfo);
    }


    fence.wait();
    fence.reset();

    device.waitIdle();

    return 0;
}


int main(){
    return ErrorCallbackWrapper<runWaves>::run();
}