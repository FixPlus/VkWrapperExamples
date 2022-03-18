#ifndef TESTAPP_SWAPCHAINIMPL_H
#define TESTAPP_SWAPCHAINIMPL_H
#include "vkw/SwapChain.hpp"
#include "vkw/FrameBuffer.hpp"
#include "vkw/Image.hpp"

namespace TestApp{

    class SwapChainImpl : public vkw::SwapChain {
    public:
        SwapChainImpl(vkw::Device &device, vkw::Surface &surface, bool createDepthBuffer = false);

        std::vector<vkw::Image2DArrayViewCRef> const&attachments() const{
            return m_image_views;
        };

        vkw::Image2DArrayViewCRef depthAttachment() const{
            return m_depth_view.value();
        }
    private:

        std::vector<vkw::Image2DArrayViewCRef> m_image_views;
        std::vector<vkw::SwapChainImage> m_images;
        std::optional<vkw::DepthStencilImage2D> m_depth{};
        std::optional<vkw::Image2DArrayViewCRef> m_depth_view{};
        static VkSwapchainCreateInfoKHR compileInfo(vkw::Device &device, vkw::Surface &surface);

        vkw::Surface &m_surface;
    };

}
#endif //TESTAPP_SWAPCHAINIMPL_H
