#ifndef TESTAPP_SWAPCHAINIMPL_H
#define TESTAPP_SWAPCHAINIMPL_H
#include "vkw/SwapChain.hpp"

namespace TestApp{

    class SwapChainImpl : public vkw::SwapChain {
    public:
        SwapChainImpl(vkw::Device &device, vkw::Surface &surface);

    private:

        static VkSwapchainCreateInfoKHR compileInfo(vkw::Device &device, vkw::Surface &surface);

        vkw::Surface &m_surface;
    };

}
#endif //TESTAPP_SWAPCHAINIMPL_H
