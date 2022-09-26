#include <SceneProjector.h>
#include <SwapChainImpl.h>
#include <vkw/Queue.hpp>
#include <vkw/Fence.hpp>
#include "Utils.h"
#include <RenderPassesImpl.h>
#include <Semaphore.hpp>
#include <RenderEngine/Shaders/ShaderLoader.h>
#include "ShadowPass.h"
#include "GUI.h"
#include "AssetPath.inc"
#include "Model.h"
#include "GlobalLayout.h"
#include "SkyBox.h"
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

std::vector<std::filesystem::path> listAvailableModels(std::filesystem::path const &root) {
    if (!exists(root))
        return {};
    std::vector<std::filesystem::path> ret{};
    for (const auto &entry: std::filesystem::directory_iterator(root)) {
        auto &entry_path = entry.path();
        if (entry_path.has_extension()) {
            if (entry_path.extension() == ".gltf" || entry_path.extension() == ".glb") {
                ret.emplace_back(entry_path);
            }
        }
    }
    return ret;
}

int runModel() {
    TestApp::SceneProjector window{800, 600, "Model"};

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

    TestApp::requestQueues(deviceDesc);

    deviceDesc.enableExtension(vkw::ext::KHR_swapchain);

    auto device = vkw::Device{renderInstance, deviceDesc};

    auto surface = window.surface(renderInstance);
    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    auto mySwapChain = TestApp::SwapChainImpl{device, surface, true};

    TestApp::LightPass lightPass = TestApp::LightPass(device, mySwapChain.attachments().front().format(),
                                                      mySwapChain.depthAttachment().format(),
                                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    std::vector<vkw::FrameBuffer> framebuffers;

    std::vector<vkw::ImageViewVT<vkw::V2DA> const*> viewRefs = {nullptr, dynamic_cast<vkw::ImageViewVT<vkw::V2DA> const*>(&mySwapChain.depthAttachment())};
    for (auto &attachment: mySwapChain.attachments()) {
        viewRefs[0] = &attachment;
        framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,{viewRefs.begin(), viewRefs.end()}});
    }

    auto queue = device.anyGraphicsQueue();
    auto fence = vkw::Fence{device};

    auto shaderLoader = RenderEngine::ShaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto textureLoader = RenderEngine::TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/texture/")};

    GUI gui = GUI{window, device, lightPass, 0, textureLoader};


    window.setContext(gui);

    auto shadowPass = ShadowRenderPass{device};

    auto skybox = SkyBox{device, lightPass, 0, shaderLoader};
    auto globalState = GlobalLayout{device, lightPass, 0, window.camera(), shadowPass, skybox};
    auto skyboxSettings = SkyBoxSettings{gui, skybox, "Sky box"};

    auto pipelinePool = RenderEngine::GraphicsPipelinePool(device, shaderLoader);


    auto modelList = listAvailableModels("data/models");
    std::vector<std::string> modelListString{};
    std::vector<const char *> modelListCstr{};
    std::transform(modelList.begin(), modelList.end(), std::back_inserter(modelListString),
                   [](std::filesystem::path const &path) {
                       return path.filename().string();
                   });
    std::transform(modelListString.begin(), modelListString.end(), std::back_inserter(modelListCstr),
                   [](std::string const &name) {
                       return name.c_str();
                   });
    if (modelList.empty()) {
        std::cout << "No GLTF model files found in data/models" << std::endl;
        return 0;
    }

    TestApp::DefaultTexturePool defaultTextures{device};

    GLTFModel model{device, defaultTextures, modelList.front()};

    std::vector<GLTFModelInstance> instances;

    instances.reserve(100);

    for(int i = 0; i < 100; ++i){
        auto& inst = instances.emplace_back(model.createNewInstance());
        inst.translation = glm::vec3((float)(i % 10) * 5.0f, -10.0f, (float)(i / 10) * 5.0f);
        inst.update();
    }

    auto instance = model.createNewInstance();
    instance.update();

    gui.customGui = [&instance, &model, &modelList, &modelListCstr, &device, &defaultTextures, &instances, &globalState, &window]() {
        //ImGui::SetNextWindowSize({400.0f, 150.0f}, ImGuiCond_Once);
        ImGui::Begin("Model", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

        static int current_model = 0;
        bool upd = false;
        upd = ImGui::SliderFloat("rot x", &instance.rotation.x, -180.0f, 180.0f) | upd;
        upd = ImGui::SliderFloat("rot y", &instance.rotation.y, -180.0f, 180.0f) | upd;
        upd = ImGui::SliderFloat("rot z", &instance.rotation.z, -180.0f, 180.0f) | upd;
        upd = ImGui::SliderFloat("scale x", &instance.scale.x, 0.1f, 10.0f) | upd;
        upd = ImGui::SliderFloat("scale y", &instance.scale.y, 0.1f, 10.0f) | upd;
        upd = ImGui::SliderFloat("scale z", &instance.scale.z, 0.1f, 10.0f) | upd;

        if (upd)
            instance.update();

        if (ImGui::Combo("models", &current_model, modelListCstr.data(), modelListCstr.size())) {
            {
                // hack to get instance destroyed
                auto dummy = std::move(instance);
            }
            instances.clear();
            model = std::move(GLTFModel(device, defaultTextures, modelList.at(current_model)));
            instance = model.createNewInstance();
            instance.update();
            for(int i = 0; i < 100; ++i){
                auto& inst = instances.emplace_back(model.createNewInstance());
                inst.translation = glm::vec3((float)(i % 10) * 5.0f, -10.0f, (float)(i / 10) * 5.0f);
                inst.update();
            }
        }


        ImGui::End();

        ImGui::Begin("Globals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static float splitL = window.camera().splitLambda();
        if(ImGui::SliderFloat("Split lambda", &splitL, 0.0f, 1.0f)){
            window.camera().setSplitLambda(splitL);
        }
        static float farClip = window.camera().farClip();
        if(ImGui::SliderFloat("Far clip", &farClip, window.camera().nearPlane(), window.camera().farPlane())){
            window.camera().setFarClip(farClip);
        }
        ImGui::End();
    };

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    auto submitInfo = vkw::SubmitInfo{commandBuffer, presentComplete, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      renderComplete};

    shadowPass.onPass = [&instance, &instances](RenderEngine::GraphicsRecordingState& state, const Camera& camera){
        instance.drawGeometryOnly(state);

        for(auto& inst: instances)
            inst.drawGeometryOnly(state);
    };

    while (!window.shouldClose()) {
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
        globalState.update();
        recorder.reset();
        shadowPass.update(window.camera(), skybox.sunDirection());
        skybox.update(window.camera());
        globalState.update();
        if(skyboxSettings.needRecomputeOutScatter()){
            skybox.recomputeOutScatter();
        }

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

                viewRefs = {nullptr, static_cast<vkw::ImageViewVT<vkw::V2DA> const*>(&mySwapChain.depthAttachment())};
                for (auto &attachment: mySwapChain.attachments()) {
                    viewRefs[0] = &attachment;
                    framebuffers.push_back(vkw::FrameBuffer{device, lightPass, extents,{viewRefs.begin(), viewRefs.end()}});
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


        commandBuffer.begin(0);

        shadowPass.execute(commandBuffer, recorder);

        auto currentImage = mySwapChain.currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        skybox.draw(recorder);

        globalState.bind(recorder);

        instance.draw(recorder);

        for(auto& inst: instances)
            inst.draw(recorder);

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
    return ErrorCallbackWrapper<runModel>::run();
}

