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
//#include <examples/waves/GlobalLayout.h>

#include "GUI.h"
#include "AssetPath.inc"
#include "Model.h"
#include "RenderEngine/RecordingState.h"

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

    GlobalLayout(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, TestApp::Camera const &camera) :
            m_camera_projection_layout(device,
                                       RenderEngine::SubstageDescription{.shaderSubstageName="perspective", .setBindings={
                                               vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                                       1),
            m_light_layout(device,
                           RenderEngine::LightingLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="sunlight", .setBindings={
                                   vkw::DescriptorSetLayoutBinding{0,
                                                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, .pass=pass, .subpass=subpass},
                           1),
            m_camera(camera),
            m_light(device, m_light_layout, light),
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


        Light(vkw::Device &device, RenderEngine::LightingLayout &layout, LightUniform const &ubo)
                : RenderEngine::Lighting(layout), uniform(device,
                                                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                  mapped(uniform.map()) {
            set().write(0, uniform);
            *mapped = ubo;
            uniform.flush();
        }

        void update(LightUniform const &ubo) {
            *mapped = ubo;
            uniform.flush();
        }

        vkw::UniformBuffer<LightUniform> uniform;
        LightUniform *mapped;
    } m_light;

    std::reference_wrapper<TestApp::Camera const> m_camera;
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

int main() {
    TestApp::SceneProjector window{800, 600, "Model"};

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

    deviceDesc.isFeatureSupported(vkw::feature::multiViewport());

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
    auto textureLoader = RenderEngine::TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/texture/")};

    GUI gui = GUI{window, device, lightPass, 0, textureLoader};


    window.setContext(gui);

    auto globalState = GlobalLayout{device, lightPass, 0, window.camera()};

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

    gui.customGui = [&instance, &model, &modelList, &modelListCstr, &device, &defaultTextures, &instances]() {
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
    };

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

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

        if (window.minimized())
            continue;

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


        commandBuffer.begin(0);
        auto currentImage = mySwapChain.currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        globalState.bind(recorder);

        instance.draw(recorder);

        for(auto& inst: instances)
            inst.draw(recorder);

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

