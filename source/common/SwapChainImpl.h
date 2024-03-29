#ifndef TESTAPP_SWAPCHAINIMPL_H
#define TESTAPP_SWAPCHAINIMPL_H
#include "vkw/FrameBuffer.hpp"
#include "vkw/Image.hpp"
#include "vkw/Surface.hpp"
#include "vkw/SwapChain.hpp"

namespace TestApp {

class SwapChainImpl : public vkw::SwapChain {
public:
  SwapChainImpl(vkw::Device &device, vkw::Surface &surface,
                bool createDepthBuffer = false, bool vsync = false);

  boost::container::small_vector<vkw::ImageView<vkw::COLOR, vkw::V2DA>, 3> const
      &
      attachments() const {
    return m_image_views;
  };

  vkw::ImageView<vkw::DEPTH, vkw::V2DA> const &depthAttachment() const {
    return *m_depth_view;
  }

  SwapChainImpl(SwapChainImpl &&another) noexcept
      : vkw::SwapChain(std::move(another)),
        m_image_views(std::move(another.m_image_views)),
        m_surface(another.m_surface),
        m_depth_view(another.m_depth_view.release()) {
    if (another.m_depth.has_value())
      m_depth.emplace(std::move(another.m_depth.value()));
  }

  SwapChainImpl &operator=(SwapChainImpl &&another) noexcept {
    vkw::SwapChain::operator=(std::move(another));
    m_image_views = std::move(another.m_image_views);
    if (another.m_depth.has_value())
      m_depth.emplace(std::move(another.m_depth.value()));
    std::swap(m_depth_view, another.m_depth_view);

    m_surface = another.m_surface;
    return *this;
  }

private:
  boost::container::small_vector<vkw::ImageView<vkw::COLOR, vkw::V2DA>, 3>
      m_image_views;

  std::optional<vkw::Image<vkw::DEPTH, vkw::I2D, vkw::SINGLE>> m_depth{};
  std::unique_ptr<vkw::ImageView<vkw::DEPTH, vkw::V2DA>> m_depth_view{};
  static VkSwapchainCreateInfoKHR
  compileInfo(vkw::Device &device, vkw::Surface &surface, bool vsync);

  vkw::StrongReference<vkw::Surface> m_surface;
};

} // namespace TestApp
#endif // TESTAPP_SWAPCHAINIMPL_H
