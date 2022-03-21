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
#include "GlobalLayout.h"
#include "WaterSurface.h"

using namespace TestApp;

class GUI : public GUIFrontEnd, public GUIBackend {
public:
    GUI(TestApp::SceneProjector &window, vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
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

class WaveMap : public vkw::ColorImage2D {
public:
    struct Pixel {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;

    };

    WaveMap(vkw::Device &device, uint32_t dim = 2048) :
            vkw::ColorImage2D(device.getAllocator(), VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                              VK_FORMAT_R8G8B8A8_UNORM, dim, dim, 1,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        vkw::Buffer<Pixel> staging{device, dim * dim, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}};

        auto *mapped = staging.map();

        for (int i = 0; i < dim; ++i) {
            for (int j = 0; j < dim; ++j) {
                mapped[i * dim + j].r = static_cast<unsigned char>((glm::sin(i * 0.003f + j * 0.003f) + 1.0f) * 127.0f);
                glm::vec2 gradient = glm::vec2{glm::cos(i * 0.003f + j * 0.003f) * 0.003f * 127.0f,
                                               glm::cos(i * 0.003f + j * 0.003f) * 0.003f * 127.0f};
                mapped[i * dim + j].g = gradient.x;
                mapped[i * dim + j].b = gradient.y;

            }
        }
        staging.flush();

        VkImageMemoryBarrier transitLayout{};
        transitLayout.image = *this;
        transitLayout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transitLayout.pNext = nullptr;
        transitLayout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        transitLayout.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transitLayout.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        transitLayout.subresourceRange.baseArrayLayer = 0;
        transitLayout.subresourceRange.baseMipLevel = 0;
        transitLayout.subresourceRange.layerCount = 1;
        transitLayout.subresourceRange.levelCount = 1;
        transitLayout.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        transitLayout.srcAccessMask = 0;

        VkImageMemoryBarrier transitLayout2{};
        transitLayout2.image = *this;
        transitLayout2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transitLayout2.pNext = nullptr;
        transitLayout2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transitLayout2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transitLayout2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transitLayout2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        transitLayout2.subresourceRange.baseArrayLayer = 0;
        transitLayout2.subresourceRange.baseMipLevel = 0;
        transitLayout2.subresourceRange.layerCount = 1;
        transitLayout2.subresourceRange.levelCount = 1;
        transitLayout2.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        transitLayout2.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

        auto queue = device.getTransferQueue();
        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
        auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

        transferCommand.begin(0);
        transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           {transitLayout});
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageSubresource.mipLevel = 0;
        region.imageExtent = VkExtent3D{dim, dim, 1};

        transferCommand.copyBufferToImage(staging, *this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {region});

        transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           {transitLayout2});
        transferCommand.end();

        queue->submit(transferCommand);
        queue->waitIdle();

    }
};

int main() {
    TestApp::SceneProjector window{800, 600, "Waves"};

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
    auto textureLoader = TextureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};

    auto randomTexture = WaveMap{device, 2048};


    GUI gui = GUI{window, device, lightPass, 0, shaderLoader, textureLoader};


    window.setContext(gui);

    auto globalState = GlobalLayout{device, window.camera()};
    auto waves = WaterSurface(device, lightPass, 0, globalState.layout(), shaderLoader);

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->familyIndex()};
    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    gui.customGui = [&waves]() {
        ImGui::Begin("Waves", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static float dirs[4] = {30.0f, 50.0f, 270.0f, 60.0f};
        static float wavenums[4] = {1.0f, 10.0f, 0.1f, 3.0f};

        for (int i = 0; i < 4; ++i) {
            std::string header = "Wave #" + std::to_string(i);
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
                ImGui::TreePop();
            }

        }
        ImGui::SliderInt("Tile cascades", &waves.cascades, 1, 7);
        ImGui::Text("Total tiles: %d", waves.totalTiles());

        ImGui::ColorEdit4("Deep water color", &waves.ubo.deepWaterColor.x);
        ImGui::ColorEdit4("Sky color", &waves.ubo.skyColor.x);

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
        waves.update(window.clock().frameTime());

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


        commandBuffer.begin(0);
        auto currentImage = mySwapChain->currentImage();

        auto &fb = framebuffers.at(currentImage);
        auto renderArea = framebuffers.at(currentImage).getFullRenderArea();

        commandBuffer.beginRenderPass(lightPass, fb, renderArea, false, values.size(), values.data());

        commandBuffer.setViewports({viewport}, 0);
        commandBuffer.setScissors({scissor}, 0);

        waves.draw(commandBuffer, globalState);

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