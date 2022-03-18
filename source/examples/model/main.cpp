#include <SceneProjector.h>
#include <AssetImport.h>
#include <vkw/Library.hpp>
#include <SwapChainImpl.h>
#include <vkw/Queue.hpp>
#include <vkw/Fence.hpp>
#include "Utils.h"
#include <RenderPassesImpl.h>
#include <Semaphore.hpp>
#include "GUI.h"
#include "AssetPath.inc"
#include "Model.h"

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

class GlobalLayout {
public:
    GlobalLayout(vkw::Device &device, TestApp::Camera const &camera) :
            m_layout(device, {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}),
            m_pool(device, 1, {VkDescriptorPoolSize{.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=1}}),
            m_set(m_pool, m_layout),
            m_uniform(device,
                      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
            m_camera(camera) {
        m_set.write(0, m_uniform);
    }

    void update() {
        m_buffer.perspective = m_camera.get().projection();
        m_buffer.cameraSpace = m_camera.get().cameraSpace();
        auto *mapped = m_uniform.map();

        *mapped = m_buffer;

        m_uniform.flush();

        m_uniform.unmap();
    }

    vkw::DescriptorSet const &set() const {
        return m_set;
    }

    vkw::DescriptorSetLayout const &layout() const {
        return m_layout;
    }

private:
    GlobalUniform m_buffer{};
    vkw::DescriptorSetLayout m_layout;
    vkw::DescriptorPool m_pool;
    vkw::DescriptorSet m_set;
    std::reference_wrapper<TestApp::Camera const> m_camera;
    vkw::UniformBuffer<GlobalUniform> m_uniform;


};

int main() {
    TestApp::SceneProjector window{800, 600, "Model"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = TestApp::Window::vulkanInstance(vulkanLib, {}, false);

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

    auto shaderLoader = ShaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    auto textureLoader = TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/texture/")};

    GUI gui = GUI{window, device, lightPass, 0, shaderLoader, textureLoader};


    window.setContext(gui);

    auto globalState = GlobalLayout{device, window.camera()};

    GLTFModel model{device, shaderLoader, "model.gltf"};
    model.setPipelinePool(lightPass, 0, globalState.layout());

    auto instance = model.createNewInstance();

    gui.customGui = [&instance]() {
        ImGui::SetNextWindowSize({300.0f, 100.0f}, ImGuiCond_Once);
        ImGui::Begin("Model", nullptr, ImGuiWindowFlags_NoResize);
        static float x, y, z;
        ImGui::SliderFloat("x", &x, -180.0f, 180.0f);
        ImGui::SliderFloat("y", &y, -180.0f, 180.0f);
        ImGui::SliderFloat("z", &z, -180.0f, 180.0f);
        instance.setRotation(x, y, z);
        ImGui::End();
    };

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

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


        commandBuffer.begin(0);
        auto currentImage = mySwapChain->currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        instance.draw(commandBuffer, globalState.set());

        gui.draw(commandBuffer);

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