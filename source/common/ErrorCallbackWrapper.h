#ifndef TESTAPP_ERRORCALLBACKWRAPPER_H
#define TESTAPP_ERRORCALLBACKWRAPPER_H

#include "RenderEngine/Window/Boxer.h"
#include <functional>
#include <vkw/Exception.hpp>

template <typename Callable> class ErrorCallbackWrapper {
public:
  template <typename... Args>
  static int run(Callable &&callable, Args &&...args) {
    try {
      std::invoke(std::forward<Callable>(callable),
                  std::forward<Args>(args)...);
      return 0;
    } catch (vkw::VulkanError &e) {
      RenderEngine::Boxer::show(e.what(), "Vulkan API error",
                                RenderEngine::Boxer::Style::Error);
    } catch (vkw::ExtensionUnsupported &e) {
      RenderEngine::Boxer::show(e.what(), "Unsupported extension",
                                RenderEngine::Boxer::Style::Error);
    } catch (vkw::FeatureUnsupported &e) {
      RenderEngine::Boxer::show(e.what(), "Unsupported feature",
                                RenderEngine::Boxer::Style::Error);
    } catch (vkw::ExtensionMissing &e) {
      RenderEngine::Boxer::show(e.what(), "Missing extension",
                                RenderEngine::Boxer::Style::Error);
    } catch (vkw::Error &e) {
      RenderEngine::Boxer::show(e.what(), "vkw::Error",
                                RenderEngine::Boxer::Style::Error);
    } catch (std::runtime_error &e) {
      RenderEngine::Boxer::show(e.what(), "Fatal error",
                                RenderEngine::Boxer::Style::Error);
    }
    return -1;
  }
};
#endif // TESTAPP_ERRORCALLBACKWRAPPER_H
