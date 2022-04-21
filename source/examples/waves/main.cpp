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

    auto mySwapChain = TestApp::SwapChainImpl{device, surface, true};

    TestApp::LightPass lightPass = TestApp::LightPass(device, mySwapChain.attachments().front().get().format(),
                                                      mySwapChain.depthAttachment().get().format(),
                                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    std::vector<vkw::FrameBuffer> framebuffers;

    for (auto &attachment: mySwapChain.attachments()) {
        framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                vkw::Image2DArrayViewConstRefArray{attachment,
                                                                                   mySwapChain.depthAttachment()}});
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


    auto land = LandSurface(device);
    auto landMaterial = LandMaterial{device};
    auto landMaterialWireframe = LandMaterial{device, true};

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

    auto waveSettings = WaveSettings{gui, waves, {{"solid", waveMaterial}, {"wireframe", waveMaterialWireframe}}};
    auto landSettings = LandSettings{gui, land, {{"solid", landMaterial}, {"wireframe", landMaterialWireframe}}};

    gui.customGui = [ &globalState, &skybox]() {

        ImGui::Begin("Globals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::ColorEdit4("Sky color", &globalState.light.skyColor.x))
            skybox.lightColor = globalState.light.skyColor;
        ImGui::ColorEdit4("Light color", &globalState.light.lightColor.x);
        if (ImGui::SliderFloat3("Light direction", &globalState.light.lightVec.x, -1.0f, 1.0f))
            globalState.light.lightVec = glm::normalize(globalState.light.lightVec);
        ImGui::SliderFloat("fog", &globalState.light.fogginess, 10.0f, 1000.0f);
        ImGui::End();
    };
    while (!window.shouldClose()) {
        window.pollEvents();


        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

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
        globalState.update();
        skybox.update();
        waves.update(window.clock().frameTime());
        waveMaterial.update();
        waveMaterialWireframe.update();
        land.update();
        landMaterial.update();
        landMaterialWireframe.update();

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
                    framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                            vkw::Image2DArrayViewConstRefArray{attachment,
                                                                                               mySwapChain.depthAttachment()}});
                }
                firstEncounter = true;
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
        auto currentImage = mySwapChain.currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        skybox.draw(recorder);

        globalState.bind(recorder);

        if(landSettings.enabled()) {
            recorder.setMaterial(landSettings.pickedMaterial().get());
            land.draw(recorder, globalState);
        }

        if(waveSettings.enabled()){
            recorder.setMaterial(waveSettings.pickedMaterial().get());
            waves.draw(recorder, globalState);
        }

        gui.draw(recorder);

        commandBuffer.endRenderPass();
        commandBuffer.end();
        queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                      renderComplete, &fence);
        queue->present(mySwapChain, renderComplete);
    }


    fence.wait();
    fence.reset();

    device.waitIdle();

    return 0;
}