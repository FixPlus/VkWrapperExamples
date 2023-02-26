#include <vkw/Fence.hpp>
#include "GlobalLayout.h"
#include "WaterSurface.h"
#include "LandSurface.h"
#include "SkyBox.h"
#include "ShadowPass.h"
#include "ErrorCallbackWrapper.h"
#include "CommonApp.h"
#include <iostream>

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
    // TODO: rewrite to StrongReference
    std::reference_wrapper<TestApp::SceneProjector> m_window;

};

class WavesApp final: public CommonApp{
public:
    WavesApp(): CommonApp(AppCreateInfo{false, "Waves", [](auto& i){}, [](vkw::PhysicalDevice& device){
        // to support wireframe display
        device.enableFeature(vkw::PhysicalDevice::feature::fillModeNonSolid);

        // to support anisotropy filtering (if possible) in ocean
        if(device.isFeatureSupported(vkw::PhysicalDevice::feature::samplerAnisotropy)) {
            std::cout << "Sampler anisotropy enabled" << std::endl;
            device.enableFeature(vkw::PhysicalDevice::feature::samplerAnisotropy);
        } else{
            std::cout << "Sampler anisotropy disabled" << std::endl;
        }
        auto dedicatedComputeFamily = std::find_if(device.queueFamilies().begin(), device.queueFamilies().end(), [](auto& family){ return family.strictly(TestApp::dedicatedCompute());});

        if(dedicatedComputeFamily == device.queueFamilies().end())
            throw std::runtime_error("Device does not have dedicated compute queue family");

        dedicatedComputeFamily->requestQueue();
    }}), gui(window(), device(), onScreenPass(), 0, textureLoader()),
                shadowPass(device()),
                skybox(device(), onScreenPass(), 0, shaderLoader()),
                globalState(device(), onScreenPass(), 0, window().camera(), shadowPass, skybox),
                globalStateSettings(gui, globalState),
                waveSurfaceTexture(device(), shaderLoader(), 256, 3),
                waves(device(), waveSurfaceTexture),
                waveMaterial(device(), waveSurfaceTexture),
                waveMaterialWireframe(device(), waveSurfaceTexture, true),
                land(device()),
                landMaterial(device()),
                landMaterialWireframe(device(), true),
                computeQueue(device().getSpecificQueue(TestApp::dedicatedCompute())),
                computeCommandPool(device(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                   computeQueue.family().index()),
                computeCommandBuffer(computeCommandPool),
                computeImageReady(std::make_shared<vkw::Semaphore>(device())),
                computeImageRelease(std::make_shared<vkw::Semaphore>(device())),
                computeSubmitInfo(computeCommandBuffer, *computeImageRelease, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, *computeImageReady),
                fence(std::make_shared<vkw::Fence>(device())),
                waveSettings(gui, waves, waveSurfaceTexture,
                             {{"solid", waveMaterial}, {"wireframe", waveMaterialWireframe}}),
                landSettings(gui, land, {{"solid", landMaterial}, {"wireframe", landMaterialWireframe}}),
                skyboxSettings(gui, skybox, "Skybox")

    {
        attachGUI(&gui);
        skybox.update(window().camera());
        skybox.recomputeOutScatter();

        addMainPassDependency(computeImageReady, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
        signalOnMainPassComplete(computeImageRelease);

        auto syncCSubmitInfo = vkw::SubmitInfo{std::span<vkw::Semaphore const>{computeImageReady.get(), 0}, {}, std::span<vkw::Semaphore const>{computeImageReady.get(), 1}};
        computeQueue.submit(syncCSubmitInfo);

        shadowPass.onPass = [this](RenderEngine::GraphicsRecordingState &state,
                                                                 const Camera &camera) {
            if (landSettings.enabled())
                land.draw(state, globalState.camera().position(), camera);
        };

        gui.customGui = [this]() {
            ImGui::Begin("Globals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            static float splitL = window().camera().splitLambda();
            if (ImGui::SliderFloat("Split lambda", &splitL, 0.0f, 1.0f)) {
                window().camera().setSplitLambda(splitL);
            }

            static float farClip = window().camera().farClip();
            if (ImGui::SliderFloat("Far clip", &farClip, window().camera().nearPlane(), window().camera().farPlane())) {
                window().camera().setFarClip(farClip);
            }
            ImGui::End();
        };

        // Prerecord compute command buffer
        computeCommandBuffer.begin(0);
        waveSurfaceTexture.acquireOwnership(computeCommandBuffer, mainPassQueueFamilyIndex(),
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        waveSurfaceTexture.dispatch(computeCommandBuffer);
        waveSurfaceTexture.releaseOwnership(computeCommandBuffer, mainPassQueueFamilyIndex(),
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
        computeCommandBuffer.end();

        addFrameFence(fence);

        window().camera().set(glm::vec3{0.0f, 25.0f, 0.0f});
        window().camera().setOrientation(172.0f, 15.0f, 0.0f);
        window().camera().setFarPlane(10000.0f);
    }

protected:
    void preMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool) override {
        RenderEngine::GraphicsRecordingState recorder{buffer, pool};
        shadowPass.execute(buffer, recorder);

        waveSurfaceTexture.acquireOwnershipFrom(buffer, computeCommandBuffer.queueFamily(),
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    }

    void onMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsRecordingState& recorder) override {
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

    }
    void afterMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool) override{
        waveSurfaceTexture.releaseOwnershipTo(buffer, computeCommandBuffer.queueFamily(),
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void onFramebufferResize() override {};
    void onPollEvents() override {
        gui.frame();
        gui.push();
        globalState.update();
        skybox.update(window().camera());
        waveSurfaceTexture.update(window().clock().frameTime());
        waves.update(window().clock().frameTime());
        waveMaterial.update();
        waveMaterialWireframe.update();
        land.update();
        landMaterial.update();
        landMaterialWireframe.update();
        shadowPass.update(window().camera(), skybox.sunDirection());

        if (waveSettings.needUpdateStaticSpectrum()) {
            waveSurfaceTexture.computeSpectrum();
            waveSettings.resetUpdateSpectrum();
        }

        if(skyboxSettings.needRecomputeOutScatter()){
            skybox.recomputeOutScatter();
        }
    }

    void postSubmit() override{
        computeQueue.submit(computeSubmitInfo, *fence);
    }

private:
    GUI gui;
    ShadowRenderPass shadowPass;
    SkyBox skybox;
    GlobalLayout globalState;
    GlobalLayoutSettings globalStateSettings;
    WaveSurfaceTexture waveSurfaceTexture;
    WaterSurface waves;
    WaterMaterial waveMaterial;
    WaterMaterial waveMaterialWireframe;

    LandSurface land;
    LandMaterial landMaterial;
    LandMaterial landMaterialWireframe;

    // Compute
    vkw::Queue computeQueue;
    vkw::CommandPool computeCommandPool;
    vkw::PrimaryCommandBuffer computeCommandBuffer;
    std::shared_ptr<vkw::Semaphore> computeImageReady;
    std::shared_ptr<vkw::Semaphore> computeImageRelease;
    vkw::SubmitInfo computeSubmitInfo;
    std::shared_ptr<vkw::Fence> fence;

    WaveSettings waveSettings;
    LandSettings landSettings;
    SkyBoxSettings skyboxSettings;
};

int runWaves() {
    WavesApp wavesApp{};
    wavesApp.run();
    return 0;
}

int main(){
    return ErrorCallbackWrapper<decltype(&runWaves)>::run(&runWaves);
}