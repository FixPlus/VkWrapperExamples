#include <iostream>

#define VKW_EXTENSION_INITIALIZERS_MAP_DEFINITION 1

#include "vkw/SymbolTable.hpp"
#include "Instance.hpp"
#include "SceneProjector.h"
#include <chrono>
#include <thread>
#include <SwapChain.hpp>
#include <Buffer.hpp>
#include <Queue.hpp>
#include <Semaphore.hpp>
#include <Fence.hpp>
#include <Image.hpp>
#include <CommandPool.hpp>
#include <CommandBuffer.hpp>
#include <VertexBuffer.hpp>
#include <Pipeline.hpp>
#include <cassert>
#include <Shader.hpp>
#include <fstream>
#include <RenderPass.hpp>
#include <FrameBuffer.hpp>
#include <DescriptorPool.hpp>
#include <DescriptorSet.hpp>
#include <UniformBuffer.hpp>
#include <cmath>
#include <chrono>
#include <Sampler.hpp>
#include <Surface.hpp>
#include <thread>

#include "tiny_gltf/stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <RenderEngine/Shaders/ShaderLoader.h>

#include "SwapChainImpl.h"
#include "RenderPassesImpl.h"
#include "CubeGeometry.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include "GUI.h"
#include "AssetPath.inc"
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

        ImGui::End();

        customGui();
    }

private:
    std::reference_wrapper<TestApp::SceneProjector> m_window;

};

class ShadowPass {
public:
    struct ShadowMapSpace {
        glm::mat4 cascades[TestApp::SHADOW_CASCADES_COUNT];
        float splits[TestApp::SHADOW_CASCADES_COUNT * 4];
    } shadowMapSpace;

    ShadowPass(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass) :
            m_shadow_proj_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="shadow", .setBindings={
                                         vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}, .pushConstants={
                                         VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=sizeof(uint32_t)}}},
                                 1),
            m_shadow_proj(m_shadow_proj_layout, device),
            m_shadow_material_layout(device,
                                     RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="flat"}, .rasterizationState=vkw::RasterizationStateCreateInfo{
                                             VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                             VK_FRONT_FACE_COUNTER_CLOCKWISE, true, 2.0f, 0.0f,
                                             6.0f}, .depthTestState=vkw::DepthTestStateCreateInfo(VK_COMPARE_OP_LESS,
                                                                                                  VK_TRUE), .maxMaterials=1}),
            m_shadow_material(m_shadow_material_layout),
            m_shadow_pass_layout(device,
                                 RenderEngine::LightingLayout::CreateInfo{.substageDescription={.shaderSubstageName="shadow"}, .pass=pass, .subpass=subpass},
                                 1),
            m_shadow_pass(m_shadow_pass_layout) {

    }


    void bind(RenderEngine::GraphicsRecordingState &state) const {
        state.setProjection(m_shadow_proj);
        state.setMaterial(m_shadow_material);
        state.setLighting(m_shadow_pass);
    }

private:
    RenderEngine::ProjectionLayout m_shadow_proj_layout;

    struct ShadowProjection : public RenderEngine::Projection {
        ShadowProjection(RenderEngine::ProjectionLayout &layout, vkw::Device &device) :
                RenderEngine::Projection(layout),
                m_ubo(device,
                      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                m_mapped(m_ubo.map()) {
            set().write(0, m_ubo);
        }

        template<uint32_t Cascades>
        void update(TestApp::ShadowCascadesCamera<Cascades> const &camera, glm::vec3 lightDir) {
            lightDir *= -1.0f;
            auto greaterRadius = camera.cascade(Cascades - 1).radius;
            for (int i = 0; i < Cascades; ++i) {
                auto cascade = camera.cascade(i);
                float shadowDepthFactor = 5.0f;
                auto center = cascade.center;
                auto shadowDepth = 2000.0f;
                if (shadowDepth < cascade.radius * shadowDepthFactor)
                    shadowDepth = cascade.radius * shadowDepthFactor;
                glm::mat4 proj = glm::ortho(-cascade.radius, cascade.radius, cascade.radius, -cascade.radius, 0.0f,
                                            shadowDepth/*cascade.radius * shadowDepthFactor*/);
                glm::mat4 lookAt = glm::lookAt(center - glm::normalize(lightDir) * (shadowDepth - cascade.radius),
                                               center,
                                               glm::vec3{0.0f, 1.0f, 0.0f});
                m_mapped->cascades[i] = proj * lookAt;
                m_mapped->splits[i * 4] = cascade.split;
            }
            flush();
        }

        void flush() {
            m_ubo.flush();
        }

        vkw::UniformBuffer<ShadowMapSpace> const &ubo() const {
            return m_ubo;
        }

    private:
        vkw::UniformBuffer<ShadowMapSpace> m_ubo;
        ShadowMapSpace *m_mapped;
    } m_shadow_proj;

    RenderEngine::MaterialLayout m_shadow_material_layout;
    RenderEngine::Material m_shadow_material;
    RenderEngine::LightingLayout m_shadow_pass_layout;
    RenderEngine::Lighting m_shadow_pass;
public:
    ShadowPass::ShadowProjection &projection() {
        return m_shadow_proj;
    }
};

class GlobalLayout {
public:

    struct LightUniform {
        glm::vec4 lightVec = glm::normalize(glm::vec4{-0.37, 0.37, -0.85, 0.0f});
        glm::vec4 skyColor = glm::vec4{158.0f, 146.0f, 144.0f, 255.0f} / 255.0f;
        glm::vec4 lightColor = glm::vec4{244.0f, 218.0f, 62.0f, 255.0f} / 255.0f;
    } light;

    GlobalLayout(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, TestApp::Camera const &camera,
                 ::ShadowPass &shadowPass, vkw::Sampler &sampler) :
            m_camera_projection_layout(device,
                                       RenderEngine::SubstageDescription{.shaderSubstageName="perspective", .setBindings={
                                               vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
                                       1),
            m_light_layout(device,
                           RenderEngine::LightingLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="sunlightShadowed", .setBindings={
                                   vkw::DescriptorSetLayoutBinding{0,
                                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                   vkw::DescriptorSetLayoutBinding{1,
                                                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                   vkw::DescriptorSetLayoutBinding{2,
                                                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, .pass=pass, .subpass=subpass},
                           1),
            m_camera(camera),
            m_light(device, m_light_layout, light, shadowPass, 4, sampler),
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

    vkw::DepthStencilImage2DArray &shaodowCascades() {
        return m_light.m_shadowCascades;
    }

    uint32_t shadowTextureSize() const {
        return 2048;
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


        Light(vkw::Device &device, RenderEngine::LightingLayout &layout, LightUniform const &ubo, ::ShadowPass &pass,
              uint32_t cascades, vkw::Sampler &sampler)
                : RenderEngine::Lighting(layout), uniform(device,
                                                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                  mapped(uniform.map()),
                  m_shadowCascades{device.getAllocator(),
                                   VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                                   VK_FORMAT_D32_SFLOAT, 2048,
                                   2048, TestApp::SHADOW_CASCADES_COUNT, 1,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT} {
            VkComponentMapping mapping{};
            mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            set().write(0, m_shadowCascades.getView<vkw::DepthImageView>(device, m_shadowCascades.format(), 0, cascades,
                                                                         mapping),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
            set().write(1, pass.projection().ubo());
            set().write(2, uniform);
            *mapped = ubo;
            uniform.flush();
        }

        void update(LightUniform const &ubo) {
            *mapped = ubo;
            uniform.flush();
        }

        vkw::UniformBuffer<LightUniform> uniform;
        vkw::DepthStencilImage2DArray m_shadowCascades;
        LightUniform *mapped;
    } m_light;

    std::reference_wrapper<TestApp::Camera const> m_camera;
};

class TexturedSurface {
public:
    TexturedSurface(vkw::Device &device, RenderEngine::TextureLoader &loader, vkw::Sampler &sampler,
                    std::string const &imageName) :
            m_layout(device,
                     RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="pbr", .setBindings={
                             vkw::DescriptorSetLayoutBinding{0,
                                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .depthTestState=vkw::DepthTestStateCreateInfo{
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
                RenderEngine::Material(layout), m_texture(loader.loadTexture(imageName)) {
            VkComponentMapping mapping{};
            mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            set().write(0, m_texture.getView<vkw::ColorImageView>(device, m_texture.format(), mapping),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
        }

    private:
        vkw::ColorImage2D m_texture;
    } m_material;
};

int main() {


    // 1. Create Instance and Window


    TestApp::SceneProjector window{800, 600, "Cubes"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = RenderEngine::Window::vulkanInstance(vulkanLib, {}, true);

    std::for_each(renderInstance.extensions_begin(), renderInstance.extensions_end(),
                  [](vkw::Instance::extension_const_iterator::value_type const &ext) {
                      std::cout << ext.first << std::endl;
                  });

    // 2. Create Device

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


    uint32_t counter = 0;
    for (auto &d: devs) {
        VkPhysicalDeviceProperties props{};
        renderInstance.core<1, 0>().vkGetPhysicalDeviceProperties(d, &props);
        std::cout << "Device " << counter << ": " << props.deviceName << std::endl;
        counter++;
    }

    // 3. Create surface

    auto surface = window.surface(renderInstance);

    // 4. Create swapchain

    auto mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});

    auto queue = device.getGraphicsQueue();

    // 5. create vertex buffer

    // 6. load data to vertex buffer


    auto fence = std::make_unique<vkw::Fence>(device);

    // 7. create command pool

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        device.getGraphicsQueue()->familyIndex()};

    VkComponentMapping mapping;
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;


    // 8. create swapchain images views for framebuffer

    std::vector<vkw::Image2DArrayViewCRef> swapChainImageViews;
    auto swapChainImages = mySwapChain->retrieveImages();
    for (auto &image: swapChainImages) {
        swapChainImageViews.emplace_back(image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
    }

    // 9. create render pass

    auto lightRenderPass = TestApp::LightPass(device, swapChainImageViews.front().get().format(), VK_FORMAT_D32_SFLOAT,
                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto shadowRenderPass = TestApp::ShadowPass(device, VK_FORMAT_D32_SFLOAT);

    // 10. create framebuffer for each swapchain image view

    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    std::optional<vkw::DepthStencilImage2D> depthMap = createDepthStencilImage(device, extents.width, extents.height);
    vkw::DepthImage2DArrayView const *depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping,
                                                                                                      VK_FORMAT_D32_SFLOAT);
    std::vector<vkw::FrameBuffer> framebuffers;

    try {
        for (auto &view: swapChainImageViews) {
            framebuffers.emplace_back(device, lightRenderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                          view.get().image()->rawExtents().height},
                                      vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
        }
    } catch (vkw::Error &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    // 11. create Cubes


    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;


    auto shadow = ::ShadowPass(device, shadowRenderPass, 0);


    createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    createInfo.requiredFlags = 0;


    RenderEngine::TextureLoader textureLoader{device, EXAMPLE_ASSET_PATH + std::string("/textures/")};
    auto cubeTexture = textureLoader.loadTexture("image");
    auto &cubeTextureView = cubeTexture.getView<vkw::ColorImageView>(device, cubeTexture.format(), mapping);

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
    samplerCI.anisotropyEnable = false;

    auto textureSampler = vkw::Sampler{device, samplerCI};

    auto globals = GlobalLayout(device, lightRenderPass, 0, window.camera(), shadow, textureSampler);

    auto &shadowMap = globals.shaodowCascades();

    std::vector<vkw::DepthImage2DArrayView const *> shadowMapAttachmentViews{};
    for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        shadowMapAttachmentViews.emplace_back(
                &shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), i, 1, mapping));
    }
    auto &shadowMapSampledView = shadowMap.getView<vkw::DepthImageView>(device, shadowMap.format(), 0,
                                                                        TestApp::SHADOW_CASCADES_COUNT, mapping);

    std::vector<vkw::FrameBuffer> shadowBuffers{};
    for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
        shadowBuffers.emplace_back(device, shadowRenderPass,
                                   VkExtent2D{globals.shadowTextureSize(), globals.shadowTextureSize()},
                                   *shadowMapAttachmentViews.at(i));
    }

    constexpr const int cubeCount = 50000;

    TestApp::CubePool cubePool{device, cubeCount};

    shadow.projection().update(window.camera(), glm::vec3{globals.light.lightVec});

    std::vector<TestApp::Cube> cubes{};
    cubes.emplace_back(cubePool, glm::vec3{500.0f, -500.0f, 500.0f}, glm::vec3{1000.0f}, glm::vec3{0.0f}).makeStatic();
    for (int i = 0; i < cubeCount - 1; ++i) {
        float scale_mag = (float) (rand() % 5) + 1.0f;
        glm::vec3 pos = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
        glm::vec3 rotate = glm::vec3((float) (rand() % 1000), (float) (rand() % 1000), (float) (rand() % 1000));
        auto scale = glm::vec3(scale_mag);
        cubes.emplace_back(cubePool, pos, scale, rotate);
    }

    vkw::DescriptorSetLayout cubeLightDescriptorLayout{device, {vkw::DescriptorSetLayoutBinding{0,
                                                                                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                                vkw::DescriptorSetLayoutBinding{1,
                                                                                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                vkw::DescriptorSetLayoutBinding{2,
                                                                                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                vkw::DescriptorSetLayoutBinding{3,
                                                                                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}};
    vkw::DescriptorSetLayout cubeShadowDescriptorLayout{device, {vkw::DescriptorSetLayoutBinding{0,
                                                                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}};

    RenderEngine::ShaderImporter shaderImporter{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};
    RenderEngine::ShaderLoader shaderLoader{device, EXAMPLE_ASSET_PATH + std::string("/shaders/")};

    auto texturedSurface = TexturedSurface(device, textureLoader, textureSampler, "image");

    // 12. create command buffer and sync primitives

    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};

    auto pipelinePool = RenderEngine::GraphicsPipelinePool{device, shaderLoader};
    auto recorder = RenderEngine::GraphicsRecordingState{commandBuffer, pipelinePool};

    GUI gui{window, device, lightRenderPass, 0, textureLoader};

    window.setContext(gui);

    gui.customGui = [&window]() {
        ImGui::SetNextWindowSize({300, 200}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);
        static float splitLambda = 0.5f;
        ImGui::SliderFloat("Split lambda", &splitLambda, 0.0f, 1.0f);
        window.camera().setSplitLambda(splitLambda);
        ImGui::End();
    };

    // 13. render on screen in a loop

    constexpr const int thread_count = 12;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    while (!window.shouldClose()) {

        RenderEngine::Window::pollEvents();


        window.update();
        gui.frame();
        gui.push();
        recorder.reset();
        globals.update();
        shadow.projection().update(window.camera(), globals.light.lightVec);

        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

        if (extents.width == 0 || extents.height == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (fence->signaled()) {
            fence->wait();
            fence->reset();
        }

        try {
            mySwapChain->acquireNextImage(presentComplete, 1000);


            commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            auto &currentFrameBuffer = framebuffers.at(mySwapChain->currentImage());
            std::array<VkClearValue, 2> values{};
            values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
            values.at(1).depthStencil.depth = 1.0f;
            values.at(1).depthStencil.stencil = 0.0f;


            VkViewport viewport;

            viewport.height = globals.shadowTextureSize();
            viewport.width = globals.shadowTextureSize();
            viewport.x = viewport.y = 0.0f;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor;
            scissor.extent.width = globals.shadowTextureSize();
            scissor.extent.height = globals.shadowTextureSize();
            scissor.offset.x = 0;
            scissor.offset.y = 0;

            auto per_thread = cubes.size() / thread_count;


            threads.clear();

            auto deltaTime = window.clock().frameTime();

            for (int i = 0; i < thread_count; ++i) {
                threads.emplace_back([&cubes, i, per_thread, deltaTime]() {
                    for (int j = per_thread * i; j < std::min(per_thread * (i + 1), cubes.size()); ++j)
                        cubes.at(j).update(deltaTime);

                });
            }

            for (auto &thread: threads)
                thread.join();


            for (int i = 0; i < TestApp::SHADOW_CASCADES_COUNT; ++i) {
                commandBuffer.beginRenderPass(shadowRenderPass, shadowBuffers.at(i),
                                              shadowBuffers.at(i).getFullRenderArea(), false,
                                              1, values.data() + 1);

                commandBuffer.setViewports({viewport}, 0);
                commandBuffer.setScissors({scissor}, 0);

                shadow.bind(recorder);

                uint32_t id = i;

                cubePool.bind(recorder);

                recorder.bindPipeline();

                recorder.pushConstants(id, VK_SHADER_STAGE_VERTEX_BIT, 0);

                cubePool.draw(recorder);

                commandBuffer.endRenderPass();
            }


            commandBuffer.beginRenderPass(lightRenderPass, currentFrameBuffer, currentFrameBuffer.getFullRenderArea(),
                                          false,
                                          values.size(), values.data());
            viewport.height = currentFrameBuffer.getFullRenderArea().extent.height;
            viewport.width = currentFrameBuffer.getFullRenderArea().extent.width;

            scissor.extent.width = currentFrameBuffer.getFullRenderArea().extent.width;
            scissor.extent.height = currentFrameBuffer.getFullRenderArea().extent.height;

            commandBuffer.setViewports({viewport}, 0);
            commandBuffer.setScissors({scissor}, 0);

            globals.bind(recorder);
            recorder.setMaterial(texturedSurface.get());

            recorder.bindPipeline();
            cubePool.draw(recorder);


            gui.draw(recorder);
            commandBuffer.endRenderPass();
            commandBuffer.end();

            queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                          renderComplete);

        } catch (vkw::VulkanError &e) {
            if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
                mySwapChain.reset();
                mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});
                swapChainImages.clear();
                swapChainImages = mySwapChain->retrieveImages();

                swapChainImageViews.clear();

                for (auto &image: swapChainImages) {
                    swapChainImageViews.emplace_back(
                            image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
                }

                extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

                depthMap.reset();
                depthMap.emplace(createDepthStencilImage(device, extents.width, extents.height));
                depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping,
                                                                                VK_FORMAT_D32_SFLOAT);
                framebuffers.clear();

                for (auto &view: swapChainImageViews) {
                    framebuffers.emplace_back(device, lightRenderPass,
                                              VkExtent2D{view.get().image()->rawExtents().width,
                                                         view.get().image()->rawExtents().height},
                                              vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
                }
                continue;
            } else {
                throw;
            }
        }
        queue->present(*mySwapChain, renderComplete);
        queue->waitIdle();
    }


    // 14. clear resources and wait for device processes to finish

    if (fence->signaled()) {
        fence->wait();
        fence->reset();
    }

    device.waitIdle();

    fence.reset();
    mySwapChain.reset();

    cubes.clear();

    return 0;
}
