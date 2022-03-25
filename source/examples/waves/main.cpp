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
#include "SkyBox.h"

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

int main() {
    TestApp::SceneProjector window{800, 600, "Waves"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = RenderEngine::Window::vulkanInstance(vulkanLib, {}, true);

    auto devs = renderInstance.enumerateAvailableDevices();

    vkw::PhysicalDevice deviceDesc{renderInstance, 0u};

    if (devs.empty()) {
        std::cout << "No available devices supporting vulkan on this machine." << std::endl <<
                  " Make sure your graphics drivers are installed and updated." << std::endl;
        return 1;
    }

    deviceDesc.enableExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // to support wireframe display
    deviceDesc.enableFeature(vkw::feature::fillModeNonSolid{});

    auto device = vkw::Device{renderInstance, deviceDesc};

    auto surface = window.surface(renderInstance);
    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    auto mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface, true});

    TestApp::LightPass lightPass = TestApp::LightPass(device, mySwapChain->attachments().front().get().format(),
                                                      mySwapChain->depthAttachment().get().format(),
                                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    std::vector<vkw::FrameBuffer> framebuffers;

    for (auto &attachment: mySwapChain->attachments()) {
        framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                vkw::Image2DArrayViewConstRefArray{attachment,
                                                                                   mySwapChain->depthAttachment()}});
    }

    auto queue = device.getGraphicsQueue();
    auto fence = vkw::Fence{device};

    auto shaderLoader = RenderEngine::ShaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto textureLoader = RenderEngine::TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};
    auto pipelinePool = RenderEngine::GraphicsPipelinePool(device, shaderLoader);

    GUI gui = GUI{window, device, lightPass, 0, textureLoader};


    window.setContext(gui);

    auto globalState = GlobalLayout{device, lightPass, 0, window.camera()};
    auto waves = WaterSurface(device);
    auto waveMaterial = WaterMaterial{device};
    auto waveMaterialWireframe = WaterMaterial{device, true};

    waves.ubo.waves[0].w = 0.180f;
    waves.ubo.waves[1].w = 0.108f;
    waves.ubo.waves[2].w = 0.412f;
    waves.ubo.waves[3].w = 0.16f;

    waves.ubo.waves[2].z = 0.01f;
    waves.ubo.waves[3].z = 0.01f;

    window.camera().set(glm::vec3{0.0f, 25.0f, 0.0f});
    window.camera().setOrientation(172.0f, 15.0f, 0.0f);


    auto skybox = SkyBox{device, lightPass, 0};

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};
    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    skybox.lightColor = globalState.light.skyColor;

    gui.customGui = [&waves, &globalState, &skybox, &waveMaterial, &waveMaterialWireframe]() {
        ImGui::Begin("Waves", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static float dirs[4] = {33.402f, 103.918f, 68.66f, 50.103f};
        static float wavenums[4] = {28.709f, 22.041f, 10.245f, 2.039f};
        static bool firstSet = true;
        for (int i = 0; i < 4; ++i) {
            std::string header = "Wave #" + std::to_string(i);
            if (firstSet) {
                waves.ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                waves.ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];

            }
            if (ImGui::TreeNode(header.c_str())) {
                if (ImGui::SliderFloat("Direction", dirs + i, 0.0f, 360.0f)) {
                    waves.ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                    waves.ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];

                }

                if (ImGui::SliderFloat("Wavelength", wavenums + i, 0.5f, 100.0f)) {
                    waves.ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                    waves.ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];
                }
                ImGui::SliderFloat("Steepness", &waves.ubo.waves[i].w, 0.0f, 1.0f);
                ImGui::SliderFloat("Steepness decay factor", &waves.ubo.waves[i].z, 0.0f, 1.0f);
                ImGui::TreePop();
            }

        }

        firstSet = false;
        ImGui::Checkbox("wireframe", &waves.wireframe);
        ImGui::SliderInt("Tile cascades", &waves.cascades, 1, 7);
        ImGui::SliderFloat("Tile scale", &waves.tileScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Elevation scale", &waves.elevationScale, 0.0f, 1.0f);

        ImGui::Text("Total tiles: %d", waves.totalTiles());

        if (ImGui::ColorEdit4("Deep water color", &waveMaterial.deepWaterColor.x))
            waveMaterialWireframe.deepWaterColor = waveMaterial.deepWaterColor;


        ImGui::End();

        ImGui::Begin("Globals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::ColorEdit4("Sky color", &globalState.light.skyColor.x))
            skybox.lightColor = globalState.light.skyColor;
        ImGui::ColorEdit4("Light color", &globalState.light.lightColor.x);
        if (ImGui::SliderFloat3("Light direction", &globalState.light.lightVec.x, -1.0f, 1.0f))
            globalState.light.lightVec = glm::normalize(globalState.light.lightVec);

        ImGui::End();
    };
    while (!window.shouldClose()) {
        window.pollEvents();
        if (fence.signaled()) {
            fence.wait();
            fence.reset();
        }

        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;


        window.update();
        gui.frame();
        gui.push();
        globalState.update();
        skybox.update();
        waves.update(window.clock().frameTime());
        waveMaterial.update();
        waveMaterialWireframe.update();

        if (window.minimized())
            continue;

        try {
            mySwapChain.get()->acquireNextImage(presentComplete, fence, 1000);
        } catch (vkw::VulkanError &e) {
            if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
                mySwapChain.reset();
                mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});

                framebuffers.clear();

                for (auto &attachment: mySwapChain->attachments()) {
                    framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                            vkw::Image2DArrayViewConstRefArray{attachment,
                                                                                               mySwapChain->depthAttachment()}});
                }
                continue;
            } else {
                throw;
            }
        }

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


        recorder.reset();
        commandBuffer.begin(0);
        auto currentImage = mySwapChain->currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        skybox.draw(recorder);

        globalState.bind(recorder);

        if (waves.wireframe)
            recorder.setMaterial(waveMaterialWireframe.get());
        else
            recorder.setMaterial(waveMaterial.get());

        waves.draw(recorder, globalState);

        gui.draw(recorder);

        commandBuffer.endRenderPass();
        commandBuffer.end();
        queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                      renderComplete);
        queue->present(*mySwapChain, renderComplete);
        queue->waitIdle();
    }

    if (fence.signaled()) {
        fence.wait();
        fence.reset();
    }
    device.waitIdle();

    return 0;
}