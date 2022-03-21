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

    TestApp::Camera const& camera() const{
        return m_camera.get();
    }

private:
    GlobalUniform m_buffer{};
    vkw::DescriptorSetLayout m_layout;
    vkw::DescriptorPool m_pool;
    vkw::DescriptorSet m_set;
    std::reference_wrapper<TestApp::Camera const> m_camera;
    vkw::UniformBuffer<GlobalUniform> m_uniform;


};

class WaveMap: public vkw::ColorImage2D{
public:
    struct Pixel{
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;

    };
    WaveMap(vkw::Device& device, uint32_t dim = 2048):
            vkw::ColorImage2D(device.getAllocator(), VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY}, VK_FORMAT_R8G8B8A8_UNORM, dim ,dim, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT){
        vkw::Buffer<Pixel> staging{device, dim * dim, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}};

        auto* mapped = staging.map();

        for(int i = 0; i < dim; ++i){
            for(int j = 0; j < dim; ++j){
                mapped[i * dim + j].r = static_cast<unsigned char>((glm::sin(i * 0.003f + j * 0.003f) + 1.0f) * 127.0f);
                glm::vec2 gradient = glm::vec2{glm::cos(i * 0.003f + j * 0.003f) * 0.003f * 127.0f, glm::cos(i * 0.003f + j * 0.003f) * 0.003f * 127.0f};
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

class WaterSurface {
public:
    static constexpr uint32_t NET_SIZE = 100;
    static constexpr float NET_CELL_SIZE = 0.1f;
    struct PrimitiveAttrs
            : public vkw::AttributeBase<vkw::VertexAttributeType::VEC3F> {
        glm::vec3 pos;
    };

    struct UBO{
        glm::vec4 waves[4];
        glm::vec4 deepWaterColor = glm::vec4(0.0f, 0.01f, 0.3f, 1.0f);
        glm::vec4 skyColor = glm::vec4(0.3f, 0.3f, 0.9f, 1.0f);
        float time;
    } ubo;

    float cascadeFactor = 50.0f;

    WaterSurface(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                 vkw::DescriptorSetLayout const &globalLayout, ShaderLoader &loader)
            : m_descriptor_layout(device,
                                  {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}),
              m_pipeline_layout(device, { globalLayout, m_descriptor_layout}, {VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0,.size=sizeof(glm::vec2) + sizeof(float) + sizeof(int)}}),
              m_vertex_shader(loader.loadVertexShader("waves")),
              m_fragment_shader(loader.loadFragmentShader("waves")),
              m_pipeline(m_compile_pipeline(device, pass, subpass, loader)),
              m_pool(device, 1,
                     {VkDescriptorPoolSize{.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=1}}),
              m_set(m_pool, m_descriptor_layout),
              m_buffer(device, 2 * NET_SIZE * NET_SIZE, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
              m_sampler(m_create_sampler(device)),
              m_ubo(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
              m_ubo_mapped(m_ubo.map()){
        m_set.write(0, m_ubo);
        std::vector<PrimitiveAttrs> attrs{};

        attrs.reserve(6 * NET_SIZE * NET_SIZE);

        for(int i = 0; i < NET_SIZE; ++i)
            for(int j = 0; j < NET_SIZE; ++j){
                PrimitiveAttrs attr{};
                attr.pos = glm::vec3(i * NET_CELL_SIZE, 0.0f, j * NET_CELL_SIZE);
                attrs.push_back(attr);
                attr.pos = glm::vec3((i + 1) * NET_CELL_SIZE, 0.0f, j * NET_CELL_SIZE);
                attrs.push_back(attr);
                attr.pos = glm::vec3(i  * NET_CELL_SIZE, 0.0f, (j + 1) * NET_CELL_SIZE);
                attrs.push_back(attr);
                attr.pos = glm::vec3((i + 1) * NET_CELL_SIZE, 0.0f, (j + 1) * NET_CELL_SIZE);
                attrs.push_back(attr);
                attr.pos = glm::vec3(i  * NET_CELL_SIZE, 0.0f, (j + 1) * NET_CELL_SIZE);
                attrs.push_back(attr);
                attr.pos = glm::vec3((i + 1)  * NET_CELL_SIZE, 0.0f, j * NET_CELL_SIZE);
                attrs.push_back(attr);
            }
        m_buffer = TestApp::createStaticBuffer<vkw::VertexBuffer<PrimitiveAttrs>, PrimitiveAttrs>(device, attrs.begin(), attrs.end());
    }

    void update(float deltaTime){
        ubo.time += deltaTime;
        *m_ubo_mapped = ubo;
        m_ubo.flush();
    }

    void draw(vkw::CommandBuffer& buffer, GlobalLayout const& globalLayout){
        struct PushConstantBlock{
            glm::vec2 translate;
            float cell_size = 0.1f;
            int grid_size = 100;

        } constants;
        buffer.bindGraphicsPipeline(m_pipeline);
        buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, globalLayout.set(), 0);
        buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, m_set, 1);
        //buffer.bindVertexBuffer(m_buffer, 0, 0);
        int cascadeIndex = 0;

        int size_cascades[4] = {50, 20, 10, 5};
        float cell_size_cascades[4] = {0.2f, 0.5f, 1.0f, 2.0f};

        int centerX = globalLayout.camera().position().x / 10.0f;
        int centerZ = globalLayout.camera().position().z / 10.0f;
        float centerY = glm::clamp(globalLayout.camera().position().y, 20.0f, 200.0f);

        int range = glm::clamp((int)(centerY / 5), 10, 50);

        for(int i = centerX - range; i < centerX + range; ++i) {
            for(int j = centerZ - range; j < centerZ + range; ++j) {
                float distance = (float)(glm::abs(i - centerX) + glm::abs(j - centerZ));
                cascadeIndex = glm::clamp( distance / cascadeFactor * centerY / 10.0f, 0.0f, 3.0f);

                constants.translate = glm::vec2{i, j} * float(size_cascades[cascadeIndex] * cell_size_cascades[cascadeIndex]);
                constants.cell_size = cell_size_cascades[cascadeIndex];
                constants.grid_size = size_cascades[cascadeIndex];

                buffer.pushConstants(m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, constants);
                buffer.pushConstants(m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, constants);
                buffer.draw(size_cascades[cascadeIndex] * size_cascades[cascadeIndex] * 6, 1);
            }
        }
    }


private:
    vkw::GraphicsPipeline
    m_compile_pipeline(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, ShaderLoader &loader) {
        vkw::GraphicsPipelineCreateInfo createInfo{pass, subpass, m_pipeline_layout};
#if 0
        static vkw::VertexInputStateCreateInfo<vkw::per_vertex<PrimitiveAttrs, 0>> vertexInputStateCreateInfo{};
        createInfo.addVertexInputState(vertexInputStateCreateInfo);
#endif
        createInfo.addVertexShader(m_vertex_shader);
        createInfo.addFragmentShader(m_fragment_shader);
        createInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
        createInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        vkw::DepthTestStateCreateInfo depthTestStateCreateInfo{VK_COMPARE_OP_LESS, true};
        createInfo.addDepthTestState(depthTestStateCreateInfo);
        vkw::RasterizationStateCreateInfo rasterizationStateCreateInfo{false, false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE};
        createInfo.addRasterizationState(rasterizationStateCreateInfo);


        return {device, createInfo};
    }

    static vkw::Sampler m_create_sampler(vkw::Device &device) {
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.maxLod = 1.0f;
        createInfo.minLod = 0.0f;
        createInfo.magFilter = VK_FILTER_LINEAR;
        createInfo.minFilter = VK_FILTER_LINEAR;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        return vkw::Sampler{device, createInfo};

    }

    vkw::VertexBuffer<PrimitiveAttrs> m_buffer;
    vkw::UniformBuffer<UBO> m_ubo;
    UBO* m_ubo_mapped;
    vkw::DescriptorSetLayout m_descriptor_layout;
    vkw::PipelineLayout m_pipeline_layout;
    vkw::VertexShader m_vertex_shader;
    vkw::FragmentShader m_fragment_shader;
    vkw::GraphicsPipeline m_pipeline;
    vkw::DescriptorPool m_pool;
    vkw::DescriptorSet m_set;
    vkw::Sampler m_sampler;
};

int main() {
    TestApp::SceneProjector window{800, 600, "Waves"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = TestApp::Window::vulkanInstance(vulkanLib, {}, true);

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

    gui.customGui = [&waves](){
        ImGui::Begin("Waves", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static float dirs[4] = {30.0f, 50.0f, 270.0f, 60.0f};
        static float wavenums[4] = {1.0f, 10.0f, 0.1f, 3.0f};

        for(int i = 0; i < 4; ++i){
            std::string header = "Wave #" + std::to_string(i);
            if(ImGui::TreeNode(header.c_str())){
                if(ImGui::SliderFloat("Direction", dirs + i, 0.0f, 360.0f)){
                    waves.ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                    waves.ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];
                }

                if(ImGui::SliderFloat("Wavelength", wavenums + i, 0.0f, 10.0f)){
                    waves.ubo.waves[i].x = glm::sin(glm::radians(dirs[i])) * wavenums[i];
                    waves.ubo.waves[i].y = glm::cos(glm::radians(dirs[i])) * wavenums[i];
                }
                ImGui::SliderFloat("Steepness", &waves.ubo.waves[i].w, 0.0f, 1.0f);
                ImGui::TreePop();
            }

        }
        ImGui::SliderFloat("Cascade factor", &waves.cascadeFactor, 1.0f, 100.0f);

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

        if(window.minimized())
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