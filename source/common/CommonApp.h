#ifndef TESTAPP_COMMONAPP_H
#define TESTAPP_COMMONAPP_H

#include <vkw/Fence.hpp>
#include <vkw/Library.hpp>
#include <vkw/Semaphore.hpp>
#include <vkw/SwapChain.hpp>
#include <vkw/Validation.hpp>

#include "VulkanMemoryMonitor.h"
#include <RenderEngine/AssetImport/AssetImport.h>
#include <RenderEngine/Shaders/ShaderLoader.h>
#include <SceneProjector.h>

#include <memory>

namespace TestApp {

struct AppCreateInfo {
  bool enableValidation;
  std::string_view applicationName;
  std::function<void(vkw::InstanceCreateInfo &)> amendInstanceCreateInfo =
      [](auto &i) {};
  std::function<void(vkw::PhysicalDevice &)> amendDeviceCreateInfo =
      [](auto &d) {};
  WindowIO* customWindow = nullptr;
};

class CommonApp {
public:
  explicit CommonApp(AppCreateInfo const &createInfo);

  void run();

  virtual ~CommonApp();

protected:
  auto &library() { return *m_library; }
  template<class Upcast = SceneProjector>
  auto &window() { return static_cast<Upcast&>(*m_window); }

  auto &instance() { return *m_instance; }

  auto &physDevice() { return *m_physDevice; }

  auto &device() { return *m_device; }

  auto &surface() { return *m_surface; }

  auto &swapChain() { return *m_swapChain; }

  auto &shaderLoader() { return *m_shaderLoader; }

  auto &textureLoader() { return *m_textureLoader; }

  auto currentSurfaceExtents() const { return m_current_surface_extents; }

  GUIFrontEnd &gui();

  vkw::RenderPass &onScreenPass();

  void addMainPassDependency(std::shared_ptr<vkw::Semaphore> waitFor,
                             VkPipelineStageFlags stage);
  void signalOnMainPassComplete(std::shared_ptr<vkw::Semaphore> signal);
  void addFrameFence(std::shared_ptr<vkw::Fence> fence);

  unsigned mainPassQueueFamilyIndex() const;

  virtual void preMainPass(vkw::PrimaryCommandBuffer &buffer,
                           RenderEngine::GraphicsPipelinePool &pool) {}

  virtual void onMainPass(vkw::PrimaryCommandBuffer &buffer,
                          RenderEngine::GraphicsRecordingState &recorder) {}

  virtual void afterMainPass(vkw::PrimaryCommandBuffer &buffer,
                             RenderEngine::GraphicsPipelinePool &pool) {}

  virtual void onFramebufferResize();

  virtual void onPollEvents() {}

  virtual void postSubmit() {}

private:
  struct InternalState;

  auto &m_internal() { return *m_internalState; }

  std::unique_ptr<VulkanMemoryMonitor> m_allocator;
  std::unique_ptr<vkw::Library> m_library;
  std::unique_ptr<WindowIO> m_window;
  std::unique_ptr<vkw::Instance> m_instance;
  std::unique_ptr<vkw::debug::Validation> m_validation;
  std::unique_ptr<vkw::PhysicalDevice> m_physDevice;
  std::unique_ptr<vkw::Device> m_device;
  std::unique_ptr<vkw::Surface> m_surface;
  std::unique_ptr<vkw::SwapChain> m_swapChain;
  std::unique_ptr<RenderEngine::ShaderLoader> m_shaderLoader;
  std::unique_ptr<RenderEngine::TextureLoader> m_textureLoader;
  std::unique_ptr<InternalState> m_internalState;
  VkExtent2D m_current_surface_extents;
};
} // namespace TestApp
#endif // TESTAPP_COMMONAPP_H
