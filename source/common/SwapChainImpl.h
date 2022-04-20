#ifndef TESTAPP_SWAPCHAINIMPL_H
#define TESTAPP_SWAPCHAINIMPL_H
#include "vkw/SwapChain.hpp"
#include "vkw/FrameBuffer.hpp"
#include "vkw/Image.hpp"

namespace TestApp{

    class SwapChainImpl : public vkw::SwapChain {
    public:
        SwapChainImpl(vkw::Device &device, vkw::Surface &surface, bool createDepthBuffer = false, bool vsync = false);

        std::vector<vkw::Image2DArrayViewCRef> const&attachments() const{
            return m_image_views;
        };

        vkw::Image2DArrayViewCRef depthAttachment() const{
            return m_depth_view.value();
        }

        SwapChainImpl(SwapChainImpl&& another) noexcept:
            vkw::SwapChain(std::move(another)),
            m_images(std::move(another.m_images)),
            m_image_views(std::move(another.m_image_views)),
            m_surface(another.m_surface)
        {
            if(another.m_depth.has_value())
                m_depth.emplace(std::move(another.m_depth.value()));
            if(another.m_depth_view.has_value())
                m_depth_view.emplace(another.m_depth_view.value());
        }

        SwapChainImpl& operator=(SwapChainImpl&& another) noexcept{
            vkw::SwapChain::operator=(std::move(another));
            m_image_views = std::move(another.m_image_views);
            m_images = std::move(another.m_images);
            if(another.m_depth.has_value())
                m_depth.emplace(std::move(another.m_depth.value()));
            if(another.m_depth_view.has_value())
                m_depth_view.emplace(another.m_depth_view.value());
            m_surface = another.m_surface;
            return *this;
        }
    private:

        std::vector<vkw::Image2DArrayViewCRef> m_image_views;
        std::vector<vkw::SwapChainImage> m_images;
        std::optional<vkw::DepthStencilImage2D> m_depth{};
        std::optional<vkw::Image2DArrayViewCRef> m_depth_view{};
        static VkSwapchainCreateInfoKHR compileInfo(vkw::Device &device, vkw::Surface &surface, bool vsync);

        std::reference_wrapper<vkw::Surface> m_surface;
    };

}
#endif //TESTAPP_SWAPCHAINIMPL_H
