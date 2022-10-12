#include <SceneProjector.h>
#include <thread>
#include <SwapChainImpl.h>
#include <RenderPassesImpl.h>
#include <Fence.hpp>
#include <RenderEngine/Shaders/ShaderLoader.h>
#include <Semaphore.hpp>
#include <CommandPool.hpp>
#include "AssetPath.inc"
#include <Queue.hpp>
#include "Fractal.h"
#include "RenderEngine/Window/Boxer.h"
#include "ErrorCallbackWrapper.h"
#include "Validation.hpp"
#include "Utils.h"

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
        ImGui::SliderFloat("Camera speed", &m_window.get().camera().force, 20.0f, 1000.0f);

        ImGui::End();

        customGui();
    }

private:
    std::reference_wrapper<TestApp::SceneProjector> m_window;

};

int runFractal(){

    TestApp::SceneProjector window{800, 600, "Fractal"};

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

    deviceDesc.enableExtension(vkw::ext::KHR_swapchain);

    TestApp::requestQueues(deviceDesc);

    auto device = vkw::Device{renderInstance, deviceDesc};

    auto surface = window.surface(renderInstance);
    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    auto mySwapChain = TestApp::SwapChainImpl{device, surface, true};

    TestApp::LightPass lightPass = TestApp::LightPass(device, mySwapChain.attachments().front().format(),
                                                      mySwapChain.depthAttachment().format(),
                                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    std::vector<vkw::FrameBuffer> framebuffers;

    for (auto &attachment: mySwapChain.attachments()) {
        std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> views = {&attachment, &mySwapChain.depthAttachment()};
        framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                {views.begin(), views.end()}});
    }

    auto queue = device.anyGraphicsQueue();
    auto fence = vkw::Fence{device};

    auto shaderLoader = RenderEngine::ShaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto textureLoader = RenderEngine::TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};

    GUI gui = GUI{window, device, lightPass, 0, textureLoader};

    window.setContext(gui);

    auto pipelinePool = RenderEngine::GraphicsPipelinePool(device, shaderLoader);
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    auto submitInfo = vkw::SubmitInfo{commandBuffer, presentComplete, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      renderComplete};

    auto fractal = Fractal{device, lightPass, 0, textureLoader, framebuffers.front().extents().width, framebuffers.front().extents().height};
    auto fractalSettings = FractalSettings{gui, fractal};

    while(!window.shouldClose()){
        window.pollEvents();
        static bool firstEncounter = true;
        if(!firstEncounter) {
            fence.wait();
            fence.reset();
        }
        else
            firstEncounter = false;

        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;


        window.update();
        gui.frame();
        gui.push();
        recorder.reset();
        fractal.update(window.camera());

        if (window.minimized()){
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
                    std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> views = {&attachment, &mySwapChain.depthAttachment()};
                    framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,
                                                            {views.begin(), views.end()}});
                }
                fractal.resizeOffscreenBuffer(extents.width, extents.height);
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


        commandBuffer.begin(0);

        fractal.drawOffscreen(commandBuffer, pipelinePool);

        auto currentImage = mySwapChain.currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({&viewport, 1}, 0);
        commandBuffer.setScissors({&scissor, 1}, 0);

        fractal.draw(recorder);
        gui.draw(recorder);

        commandBuffer.endRenderPass();
        commandBuffer.end();
        queue.submit(submitInfo, fence);
        auto presentInfo = vkw::PresentInfo{mySwapChain, renderComplete};
        queue.present(presentInfo);

    }

    fence.wait();
    fence.reset();

    device.waitIdle();

    return 0;

}

int main(){
    return ErrorCallbackWrapper<runFractal>::run();
}