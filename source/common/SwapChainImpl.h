#ifndef TESTAPP_SWAPCHAINIMPL_H
#define TESTAPP_SWAPCHAINIMPL_H
#include "vkw/SwapChain.hpp"
#include "vkw/FrameBuffer.hpp"
#include "vkw/Image.hpp"

namespace TestApp{

    class SwapChainImpl : public vkw::SwapChain {
    public:
        SwapChainImpl(vkw::Device &device, vkw::Surface &surface, bool createDepthBuffer = false, bool vsync = false);

        std::vector<vkw::ImageView<vkw::COLOR, vkw::V2DA>> const&attachments() const{
            return m_image_views;
        };

        vkw::ImageView<vkw::DEPTH, vkw::V2DA> const& depthAttachment() const{
            return *m_depth_view;
        }

        SwapChainImpl(SwapChainImpl&& another) noexcept:
            vkw::SwapChain(std::move(another)),
            m_images(std::move(another.m_images)),
            m_image_views(std::move(another.m_image_views)),
            m_surface(another.m_surface),
            m_depth_view(another.m_depth_view.release())
        {
            if(another.m_depth.has_value())
                m_depth.emplace(std::move(another.m_depth.value()));
        }

        SwapChainImpl& operator=(SwapChainImpl&& another) noexcept{
            vkw::SwapChain::operator=(std::move(another));
            m_image_views = std::move(another.m_image_views);
            m_images = std::move(another.m_images);
            if(another.m_depth.has_value())
                m_depth.emplace(std::move(another.m_depth.value()));
            std::swap(m_depth_view, another.m_depth_view);

            m_surface = another.m_surface;
            return *this;
        }
    private:

        std::vector<vkw::ImageView<vkw::COLOR, vkw::V2DA>> m_image_views;
        std::vector<vkw::SwapChainImage> m_images;
        std::optional<vkw::Image<vkw::DEPTH, vkw::I2D, vkw::SINGLE>> m_depth{};
        std::unique_ptr<vkw::ImageView<vkw::DEPTH, vkw::V2DA>> m_depth_view{};
        static VkSwapchainCreateInfoKHR compileInfo(vkw::Device &device, vkw::Surface &surface, bool vsync);

        std::reference_wrapper<vkw::Surface> m_surface;
    };

}
#endif //TESTAPP_SWAPCHAINIMPL_H
