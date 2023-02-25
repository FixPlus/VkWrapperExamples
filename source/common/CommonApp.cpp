#include <CommonApp.h>
#include <vkw/Layers.hpp>
#include <vkw/Validation.hpp>
#include "Utils.h"
#include "SwapChainImpl.h"
#include <vkw/FrameBuffer.hpp>
#include "RenderPassesImpl.h"
#include <vkw/Fence.hpp>
#include "AssetPath.inc"

namespace TestApp{

class SwapChainWithFramebuffers: public SwapChainImpl, public LightPass, public std::vector<vkw::FrameBuffer>{
public:
    SwapChainWithFramebuffers(vkw::Device &device, vkw::Surface& surface): SwapChainImpl(device, surface, true),
                                                                           LightPass(device, attachments().front().format(),
                                                                                     depthAttachment().format(),
                                                                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR){
        auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;
        std::transform(attachments().begin(), attachments().end(), std::back_inserter(*this), [this, &extents, &device](auto &attachment){
            std::array<vkw::ImageViewVT<vkw::V2DA> const*, 2> views = {&attachment, &depthAttachment()};
            return vkw::FrameBuffer{device, *this, extents,
                                                    {views.begin(), views.end()}};
        });
    }
};

struct CommonApp::InternalState{

    InternalState(vkw::Device& device, RenderEngine::ShaderLoader &shaderLoader):
            renderQueue(device.anyGraphicsQueue()),
            mainPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, renderQueue.family().index()),
            mainBuffer(mainPool),
            renderComplete(device),
            presentComplete(device),
            fence(device),
            pipelinePool(device, shaderLoader),
            recordingState(mainBuffer, pipelinePool),
            submitInfo(mainBuffer, presentComplete, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       renderComplete){

    }

    void updateSubmitInfo(){
        std::vector<vkw::SemaphoreCRef> deps;
        deps.emplace_back(presentComplete);
        std::transform(externalDeps.begin(), externalDeps.end(), std::back_inserter(deps), [](auto& ext){ return vkw::SemaphoreCRef(*ext.first);});
        std::vector<VkPipelineStageFlags> depStages;
        depStages.emplace_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        std::transform(externalDeps.begin(), externalDeps.end(), std::back_inserter(depStages), [](auto& ext){ return ext.second;});
        std::vector<vkw::SemaphoreCRef> signals;
        signals.emplace_back(renderComplete);
        std::transform(externalSignals.begin(), externalSignals.end(), std::back_inserter(signals), [](auto& ext){ return vkw::SemaphoreCRef(*ext);});

        submitInfo = vkw::SubmitInfo{mainBuffer, deps, depStages, signals};
    }
    void waitForFences(){
        fence.wait();
        fence.reset();
        for(auto& ext: externalFences){
            ext.get().wait();
            ext.get().reset();
        }
    }
    void submit(){
        renderQueue.submit(submitInfo, fence);
    }
    vkw::Queue renderQueue;
    vkw::CommandPool mainPool;
    vkw::PrimaryCommandBuffer mainBuffer;
    vkw::Semaphore renderComplete;
    vkw::Semaphore presentComplete;
    vkw::Fence fence;
    RenderEngine::GraphicsPipelinePool pipelinePool;
    RenderEngine::GraphicsRecordingState recordingState;
    vkw::SubmitInfo submitInfo;
    SwapChainWithFramebuffers *swapChain = nullptr;
    std::vector<std::pair<vkw::Semaphore*, VkPipelineStageFlags>> externalDeps;
    std::vector<vkw::Semaphore*> externalSignals;
    std::vector<vkw::FenceRef> externalFences;
};

CommonApp::CommonApp(AppCreateInfo const& createInfo) {
    m_window = std::make_unique<TestApp::SceneProjector>(800, 600, createInfo.applicationName.data());

    m_library = std::make_unique<vkw::Library>();

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
        m_validation = std::make_unique<vkw::debug::Validation>(instance());
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

    m_shaderLoader = std::make_unique<RenderEngine::ShaderLoader>(device(), EXAMPLE_ASSET_PATH + std::string("/shaders/"));
    m_textureLoader = std::make_unique<RenderEngine::TextureLoader>(device(), EXAMPLE_ASSET_PATH + std::string("/textures/"));

    m_internalState = std::make_unique<InternalState>(device(), shaderLoader());
    m_internalState->swapChain = dynamic_cast<SwapChainWithFramebuffers*>(m_swapChain.get());

    assert(m_internalState->swapChain && "You messed up with types");

    m_current_surface_extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;
}

    CommonApp::~CommonApp() = default;

    vkw::RenderPass &CommonApp::onScreenPass() {
        return *m_internalState->swapChain;
    }

    void CommonApp::attachGUI(GUIBackend *gui) {
        m_gui = gui;
        m_window->setContext(*gui);
    }

    void CommonApp::run(){
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
            onPollEvents();

            if (extents.width == 0 || extents.height == 0){
                extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;
                firstEncounter = true;
                continue;
            }

            try {
                swapChain().acquireNextImage(m_internal().presentComplete, 1000);
            } catch (vkw::VulkanError &e) {
                if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {

                    extents = surface().getSurfaceCapabilities(physDevice()).currentExtent;
                    firstEncounter = true;

                    // can't create a new framebuffer if surface is practically empty
                    if (extents.width == 0 || extents.height == 0)
                        continue;


                    m_swapChain.reset();
                    m_internal().swapChain = new TestApp::SwapChainWithFramebuffers{device(), surface()};
                    m_swapChain.reset(m_internal().swapChain);


                    onFramebufferResize();

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


            auto& commandBuffer = m_internal().mainBuffer;
            commandBuffer.begin(0);
            auto &recorder = m_internal().recordingState;
            recorder.reset();

            preMainPass(commandBuffer, m_internal().pipelinePool);
            auto currentImage = swapChain().currentImage();

            auto &fb = m_internal().swapChain->at(currentImage);

            auto renderArea = fb.getFullRenderArea();

            commandBuffer.beginRenderPass(*m_internal().swapChain, fb, renderArea, false, values.size(), values.data());

            commandBuffer.setViewports({&viewport, 1}, 0);
            commandBuffer.setScissors({&scissor, 1}, 0);

            onMainPass(recorder);
            if(m_gui)
                m_gui->draw(recorder);

            commandBuffer.endRenderPass();

            afterMainPass(commandBuffer, m_internal().pipelinePool);

            commandBuffer.end();

            m_internal().submit();

            postSubmit();

            auto presentInfo = vkw::PresentInfo{swapChain(), m_internal().renderComplete};
            m_internal().renderQueue.present(presentInfo);

        }
        m_internal().waitForFences();

        device().waitIdle();

    }

    void CommonApp::addMainPassDependency(vkw::Semaphore& waitFor, VkPipelineStageFlags stage) {
        m_internal().externalDeps.emplace_back(&waitFor, stage);
        m_internal().updateSubmitInfo();
    }

    void CommonApp::signalOnMainPassComplete(vkw::Semaphore& signal){
        m_internal().externalSignals.emplace_back(&signal);
        m_internal().updateSubmitInfo();
    }

    unsigned CommonApp::mainPassQueueFamilyIndex() const {
        return m_internalState->mainBuffer.queueFamily();
    }

    void CommonApp::addFrameFence(vkw::Fence &fence) {
        m_internal().externalFences.emplace_back(fence);
    }
}