#include <CommonApp.h>
#include <vkw/Layers.hpp>
#include <vkw/Validation.hpp>
#include "Utils.h"
#include "SwapChainImpl.h"
#include <vkw/FrameBuffer.hpp>
#include "RenderPassesImpl.h"
#include <vkw/Fence.hpp>
#include "AssetPath.inc"
#include "RenderEngine/Window/Boxer.h"
#include <iostream>

namespace TestApp{

class GUI : public GUIFrontEnd, public GUIBackend {
public:
    GUI(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
        RenderEngine::TextureLoader const &textureLoader)
            : GUIBackend(device, pass, subpass, textureLoader) {
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

};

class ApplicationStatistics: public GUIWindow{
public:
    ApplicationStatistics(GUIFrontEnd& gui, TestApp::SceneProjector& window, VulkanMemoryMonitor const& monitor):
    GUIWindow(gui, WindowSettings{.title="Application stat", .autoSize=true}), m_window(window), m_monitor(monitor){

    }

protected:
    void onGui() override{
        ImGui::Text("FPS: %.2f", m_window.get().clock().fps());
        ImGui::Text("vk mem: %lluK; a:%llu; r:%llu; f:%llu", m_monitor.get().totalHostMemory() / 1000,
                    m_monitor.get().totalAllocations(),
                    m_monitor.get().totalReallocations(),
                    m_monitor.get().totalFrees());
        auto &camera = m_window.get().camera();
        auto pos = camera.position();
        ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f,", pos.x, pos.y, pos.z);
        ImGui::Text("(%.2f,%.2f)", camera.phi(), camera.psi());

        ImGui::SliderFloat("Cam rotate inertia", &camera.rotateInertia, 0.1f, 5.0f);
        ImGui::SliderFloat("Mouse sensitivity", &m_window.get().mouseSensitivity, 1.0f, 10.0f);
        ImGui::SliderFloat("Camera speed", &m_window.get().camera().force, 20.0f, 1000.0f);
    }
private:
    // TODO: rewite to StrongReference
    std::reference_wrapper<TestApp::SceneProjector> m_window;
    vkw::StrongReference<VulkanMemoryMonitor const> m_monitor;
};

class SwapChainWithFramebuffers: public SwapChainImpl{
public:
    SwapChainWithFramebuffers(vkw::Device &device, vkw::Surface& surface): SwapChainImpl(device, surface, true), m_device(device), m_surface(surface){

    }
    void createFrameBuffers(vkw::RenderPass &pass, std::vector<vkw::FrameBuffer>& framebuffers) {
        auto& surface = m_surface.get();
        auto extents = surface.getSurfaceCapabilities(m_device.get().physicalDevice()).currentExtent;
        std::transform(attachments().begin(), attachments().end(), std::back_inserter(framebuffers), [this, &extents, &pass](auto &attachment){
            std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> views = {&attachment, &depthAttachment()};
            return vkw::FrameBuffer{m_device.get(), pass, extents,
                                    {views.begin(), views.end()}};
        });
    }

private:
    vkw::StrongReference<vkw::Device> m_device;
    vkw::StrongReference<vkw::Surface> m_surface;
};

class CommonValidation: public vkw::debug::Validation{
public:

    CommonValidation(vkw::Instance const&instance):vkw::debug::Validation(instance){}

    void onValidationMessage(vkw::debug::MsgSeverity severity,
                             vkw::debug::Validation::Message const &message) override {
        std::string prefix;
        switch(severity){
            case vkw::debug::MsgSeverity::Error:
                prefix = "ERROR: ";
                break;
            case vkw::debug::MsgSeverity::Warning:
                prefix = "WARNING: ";
                break;
            case vkw::debug::MsgSeverity::Info:
                prefix = "INFO: ";
                break;
            case vkw::debug::MsgSeverity::Verbose:
                prefix = "VERBOSE: ";
                break;
        }

        std::stringstream debugMessage;
        debugMessage << prefix << "[" << message.id << "]["
                     << message.name
                     << "] : " << message.what;
        std::cout << debugMessage.str() << std::endl;
        fflush(stdout);
    }

    void onPerformanceMessage(vkw::debug::MsgSeverity severity,
                                      vkw::debug::Validation::Message const &message) override{
        onValidationMessage(severity, message);
    }

    void onGeneralMessage(vkw::debug::MsgSeverity severity, vkw::debug::Validation::Message const &message) override{
        onValidationMessage(severity, message);
    }
};

struct CommonApp::InternalState{

    InternalState(vkw::Device& device, RenderEngine::ShaderLoader &shaderLoader, VkFormat colorFormat, VkFormat depthFormat):
            pass(device, colorFormat, depthFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
            renderQueue(device.anyGraphicsQueue()),
            mainPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, renderQueue.family().index()),
            mainBuffer(mainPool),
            renderComplete(device),
            presentComplete(device),
            fence(device),
            submitInfo(mainBuffer, presentComplete, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       renderComplete){

    }

    void updateSubmitInfo(){
        std::vector<std::reference_wrapper<vkw::Semaphore const>> deps;
        deps.emplace_back(presentComplete);
        std::transform(externalDeps.begin(), externalDeps.end(), std::back_inserter(deps), [](auto& ext) ->std::reference_wrapper<vkw::Semaphore const>{ return *ext.first;});
        std::vector<VkPipelineStageFlags> depStages;
        depStages.emplace_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        std::transform(externalDeps.begin(), externalDeps.end(), std::back_inserter(depStages), [](auto& ext){ return ext.second;});
        std::vector<std::reference_wrapper<vkw::Semaphore const>> signals;
        signals.emplace_back(renderComplete);
        std::transform(externalSignals.begin(), externalSignals.end(), std::back_inserter(signals), [](auto& ext) ->std::reference_wrapper<vkw::Semaphore const>{ return *ext;});

        submitInfo = vkw::SubmitInfo{mainBuffer, deps, depStages, signals};
    }
    void waitForFences(){
        fence.wait();
        fence.reset();
        for(auto& ext: externalFences){
            ext->wait();
            ext->reset();
        }
    }
    void submit(){
        renderQueue.submit(submitInfo, fence);
    }
    LightPass pass;
    std::vector<vkw::FrameBuffer> framebuffers;
    vkw::Queue renderQueue;
    vkw::CommandPool mainPool;
    vkw::PrimaryCommandBuffer mainBuffer;
    vkw::Semaphore renderComplete;
    vkw::Semaphore presentComplete;
    vkw::Fence fence;

    vkw::SubmitInfo submitInfo;
    SwapChainWithFramebuffers *swapChain = nullptr;
    std::vector<std::pair<std::shared_ptr<vkw::Semaphore>, VkPipelineStageFlags>> externalDeps;
    std::vector<std::shared_ptr<vkw::Semaphore>> externalSignals;
    std::vector<std::shared_ptr<vkw::Fence>> externalFences;
    std::unique_ptr<GUI> gui;
    std::unique_ptr<ApplicationStatistics> appStat;
};

CommonApp::CommonApp(AppCreateInfo const& createInfo) {
    vkw::addIrrecoverableErrorCallback([](vkw::Error& e) {
        RenderEngine::Boxer::show(e.codeString().append(": ").append(e.what()), "Irrecoverable vkw::Error", RenderEngine::Boxer::Style::Error);
    });
    m_window = std::make_unique<TestApp::SceneProjector>(800, 600, createInfo.applicationName.data());

    m_allocator = std::make_unique<VulkanMemoryMonitor>();

    m_library = std::make_unique<vkw::Library>(nullptr, m_allocator.get());

    auto validationPossible = library().hasLayer(vkw::layer::KHRONOS_validation);

    if(createInfo.enableValidation) {
        if (!validationPossible)
            std::cout << "Validation unavailable" << std::endl;
        else
            std::cout << "Validation enabled" << std::endl;
    } else {
        std::cout << "Validation disabled" << std::endl;
        validationPossible = false;
    }

    vkw::InstanceCreateInfo instanceCreateInfo{};

    if(validationPossible) {
        instanceCreateInfo.requestLayer(vkw::layer::KHRONOS_validation);
        instanceCreateInfo.requestExtension(vkw::ext::EXT_debug_utils);
    }

    createInfo.amendInstanceCreateInfo(instanceCreateInfo);

    m_instance = std::make_unique<vkw::Instance>(RenderEngine::Window::vulkanInstance(library(), instanceCreateInfo));

    std::optional<vkw::debug::Validation> validation;

    if(validationPossible)
        m_validation = std::make_unique<CommonValidation>(instance());
    auto devs = instance().enumerateAvailableDevices();


    if (devs.empty()) {
        std::stringstream ss;
        ss << "No available devices supporting vulkan on this machine." << std::endl <<
                  " Make sure your graphics drivers are installed and updated." << std::endl;
        throw std::runtime_error(ss.str());
    }

    m_physDevice = std::make_unique<vkw::PhysicalDevice>(instance(), 0u);

    physDevice().enableExtension(vkw::ext::KHR_swapchain);

    TestApp::requestQueues(physDevice());
    createInfo.amendDeviceCreateInfo(physDevice());

    m_device = std::make_unique<vkw::Device>(instance(), physDevice());

    m_surface = std::make_unique<vkw::Surface>(window().surface(instance()));

    m_swapChain = std::make_unique<SwapChainWithFramebuffers>(device(), surface());
    auto* swapChain = dynamic_cast<SwapChainWithFramebuffers*>(m_swapChain.get());

    m_shaderLoader = std::make_unique<RenderEngine::ShaderLoader>(device(), EXAMPLE_ASSET_PATH + std::string("/shaders/"));
    m_textureLoader = std::make_unique<RenderEngine::TextureLoader>(device(), EXAMPLE_ASSET_PATH + std::string("/textures/"));

    m_internalState = std::make_unique<InternalState>(device(), shaderLoader(), swapChain->attachments().front().format(), swapChain->depthAttachment().format());
    m_internalState->swapChain = swapChain;
    m_internalState->swapChain->createFrameBuffers(m_internalState->pass, m_internal().framebuffers);

    assert(m_internalState->swapChain && "You messed up with types");

    m_current_surface_extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;

    m_internal().gui = std::make_unique<GUI>(device(), m_internal().pass, 0, textureLoader());
    window().setContext(*m_internal().gui);
    m_internal().appStat = std::make_unique<ApplicationStatistics>(gui(), window(), *m_allocator);
}

    CommonApp::~CommonApp() = default;

    vkw::RenderPass &CommonApp::onScreenPass() {
        return m_internalState->pass;
    }

    void CommonApp::run() {
        RenderEngine::GraphicsPipelinePool pipelinePool{device(), shaderLoader()};
        RenderEngine::GraphicsRecordingState recordingState{m_internal().mainBuffer, pipelinePool};
        auto customDel = [](vkw::Device* device){ device->waitIdle();};
        auto deviceKeeper = std::unique_ptr<vkw::Device, decltype(customDel)>(&device(), customDel);

        m_current_surface_extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;
        auto& extents = m_current_surface_extents;

        while(!window().shouldClose()){
            window().pollEvents();
            static bool firstEncounter = true;
            if(!firstEncounter) {
                m_internal().waitForFences();
            }
            else
                firstEncounter = false;

            window().update();
            recordingState.reset();
            onPollEvents();
            m_internal().gui->frame();
            m_internal().gui->push();

            if (extents.width == 0 || extents.height == 0){
                extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;
                firstEncounter = true;
                continue;
            }

            auto acquireResult = swapChain().acquireNextImage(m_internal().presentComplete, 1000);
            if(acquireResult == vkw::SwapChain::AcquireStatus::TIMEOUT)
                continue;
            if(acquireResult == vkw::SwapChain::AcquireStatus::OUT_OF_DATE ||
               acquireResult == vkw::SwapChain::AcquireStatus::SUBOPTIMAL
            ) {

                extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;
                firstEncounter = true;

                // can't create a new framebuffer if surface is practically empty
                if (extents.width == 0 || extents.height == 0)
                    continue;

                m_internal().framebuffers.clear();
                m_swapChain.reset();
                m_internal().swapChain = new TestApp::SwapChainWithFramebuffers{device(), surface()};
                m_internal().swapChain->createFrameBuffers(m_internalState->pass, m_internal().framebuffers);
                m_swapChain.reset(m_internal().swapChain);


                onFramebufferResize();

                firstEncounter = true;

                continue;

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


            auto& commandBuffer = m_internal().mainBuffer;
            commandBuffer.begin(0);
            auto &recorder = recordingState;
            recorder.reset();

            preMainPass(commandBuffer, pipelinePool);
            auto currentImage = swapChain().currentImage();

            auto &fb = m_internal().framebuffers.at(currentImage);

            auto renderArea = fb.getFullRenderArea();

            commandBuffer.beginRenderPass(m_internal().pass, fb, renderArea, false, values.size(), values.data());

            commandBuffer.setViewports({&viewport, 1}, 0);
            commandBuffer.setScissors({&scissor, 1}, 0);

            onMainPass(commandBuffer, recorder);

            m_internal().gui->draw(recorder);

            commandBuffer.endRenderPass();

            afterMainPass(commandBuffer, pipelinePool);

            commandBuffer.end();

            m_internal().submit();

            postSubmit();

            auto presentInfo = vkw::PresentInfo{swapChain(), m_internal().renderComplete};
            m_internal().renderQueue.present(presentInfo);

        }
        m_internal().waitForFences();

        device().waitIdle();
    }

    void CommonApp::addMainPassDependency(std::shared_ptr<vkw::Semaphore> waitFor, VkPipelineStageFlags stage) {
        m_internal().externalDeps.emplace_back(waitFor, stage);
        m_internal().updateSubmitInfo();
    }

    void CommonApp::signalOnMainPassComplete(std::shared_ptr<vkw::Semaphore> signal){
        m_internal().externalSignals.emplace_back(signal);
        m_internal().updateSubmitInfo();
    }

    unsigned CommonApp::mainPassQueueFamilyIndex() const {
        return m_internalState->mainBuffer.queueFamily();
    }

    void CommonApp::addFrameFence(std::shared_ptr<vkw::Fence> fence) {
        m_internal().externalFences.emplace_back(fence);
    }

    GUIFrontEnd& CommonApp::gui(){
        return *m_internal().gui;
    }
}