#include <cmath>
#include <thread>
#include "CubeGeometry.h"
#include "ShadowPass.h"
#include "SkyBox.h"
#include "GlobalLayout.h"
#include "ErrorCallbackWrapper.h"
#include "CommonApp.h"

#undef max
#undef min

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
    // TODO: rewite to StrongReference
    std::reference_wrapper<TestApp::SceneProjector> m_window;

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

class CubesApp final: public CommonApp{
public:
    CubesApp(unsigned cubeCount = 50000): CommonApp(AppCreateInfo{true, "Cubes", [](auto& i){}, [](vkw::PhysicalDevice& device){
        if(device.isFeatureSupported(vkw::PhysicalDevice::feature::samplerAnisotropy))
            device.enableFeature(vkw::PhysicalDevice::feature::samplerAnisotropy);
    }}),
                shadow(device()),
                textureSampler(device(), m_fillSamplerCI(device())),
                skybox(device(), onScreenPass(), 0, shaderLoader()),
                skyboxSettings(gui, skybox, "SkyBox"),
                globals(device(), onScreenPass(), 0, window().camera(), shadow, skybox),
                globalLayoutSettings(gui, globals),
                cubePool(device(), cubeCount),
                texturedSurface(device(), textureLoader(), textureSampler, "image"),
                gui{window(), device(), onScreenPass(), 0, textureLoader()}{


        attachGUI(&gui);

        shadow.update(window().camera(), skybox.sunDirection());

        cubes.emplace_back(cubePool, glm::vec3{500.0f, -500.0f, 500.0f}, glm::vec3{1000.0f}, glm::vec3{0.0f}).makeStatic();
        for (int i = 0; i < cubeCount - 1; ++i) {
            float scale_mag = (float) (rand() % 5) + 1.0f;
            glm::vec3 pos = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
            glm::vec3 rotate = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
            auto scale = glm::vec3(scale_mag);
            cubes.emplace_back(cubePool, pos, scale, rotate);
        }

        gui.customGui = [this]() {
            ImGui::SetNextWindowSize({300, 200}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);
            static float splitLambda = 0.5f;
            ImGui::SliderFloat("Split lambda", &splitLambda, 0.0f, 1.0f);
            window().camera().setSplitLambda(splitLambda);
            ImGui::End();
        };

        shadow.onPass = [this](RenderEngine::GraphicsRecordingState& state, const Camera& camera){
            cubePool.bind(state);
            state.bindPipeline();
            cubePool.draw(state);
        };

        threads.reserve(thread_count);
    }

protected:
    void preMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool) override{
        auto per_thread = cubes.size() / thread_count;


        threads.clear();

        auto deltaTime = window().clock().frameTime();

        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([this, i, per_thread, deltaTime]() {
                for (int j = per_thread * i; j < std::min(per_thread * (i + 1), cubes.size()); ++j)
                    cubes.at(j).update(deltaTime);

            });
        }

        for (auto &thread: threads)
            thread.join();
        RenderEngine::GraphicsRecordingState recorder{buffer, pool};
        shadow.execute(buffer, recorder);
    }

    void onMainPass(RenderEngine::GraphicsRecordingState& recorder) override{
        skybox.draw(recorder);
        globals.bind(recorder);
        recorder.setMaterial(texturedSurface.get());

        cubePool.draw(recorder);

    }

    void onPollEvents() override{
        gui.frame();
        gui.push();
        skybox.update(window().camera());
        globals.update();
        shadow.update(window().camera(), skybox.sunDirection());
        if(skyboxSettings.needRecomputeOutScatter()){
            skybox.recomputeOutScatter();
        }
    }

private:
    static VkSamplerCreateInfo m_fillSamplerCI(vkw::Device & device){
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
        if(device.physicalDevice().enabledFeatures().samplerAnisotropy) {
            samplerCI.anisotropyEnable = true;
            samplerCI.maxAnisotropy = device.physicalDevice().properties().limits.maxSamplerAnisotropy;
        }
        return samplerCI;
    }
    GUI gui;
    ShadowRenderPass shadow;
    vkw::Sampler textureSampler;
    SkyBox skybox;
    SkyBoxSettings skyboxSettings;
    GlobalLayout globals;
    GlobalLayoutSettings globalLayoutSettings;
    unsigned thread_count = 12;
    TestApp::CubePool cubePool;
    std::vector<TestApp::Cube> cubes;
    TexturedSurface texturedSurface;

    std::vector<std::thread> threads;
};

int runCubes() {
    CubesApp app{};
    app.run();
    return 0;
}

int main(){
    return ErrorCallbackWrapper<decltype(&runCubes)>::run(&runCubes);
}
