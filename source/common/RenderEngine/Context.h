#ifndef RENDER_ENGINE_CONTEXT_H
#define RENDER_ENGINE_CONTEXT_H

#include "RenderEngine/Window/Window.h"
#include "vkw/Device.hpp"

namespace RenderEngine {

class Context {
private:
  Context(vkw::Device &device, Window &window);

  vkw::Device &device() const { return m_device; }

  Window &window() const { return m_window; }

public:
  vkw::StrongReference<vkw::Device> m_device;
  // TODO: rewrite to StrongReference
  std::reference_wrapper<Window> m_window;
};
} // namespace RenderEngine
#endif // RENDER_ENGINE_CONTEXT_H
