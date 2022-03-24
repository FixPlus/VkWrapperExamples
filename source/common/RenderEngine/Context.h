#ifndef RENDER_ENGINE_CONTEXT_H
#define RENDER_ENGINE_CONTEXT_H


#include "vkw/Device.hpp"
#include "RenderEngine/Window/Window.h"

namespace RenderEngine{

    class Context{
    private:

        Context(vkw::Device& device, Window& window);

        vkw::Device& device() const{
            return m_device;
        }

        Window& window() const{
            return m_window;
        }

    public:

        std::reference_wrapper<vkw::Device> m_device;
        std::reference_wrapper<Window> m_window;
    };
}
#endif //RENDER_ENGINE_CONTEXT_H
