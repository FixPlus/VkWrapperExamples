#ifndef TESTAPP_RENDERPASSESIMPL_H
#define TESTAPP_RENDERPASSESIMPL_H
#include "vkw/RenderPass.hpp"

namespace TestApp {

class PassBase {
public:
protected:
  std::vector<vkw::AttachmentDescription> m_attachments;
};
class LightPass : public PassBase, public vkw::RenderPass {
public:
  using PassBase::m_attachments;
  LightPass(vkw::Device &device, VkFormat colorFormat, VkFormat depthFormat,
            VkImageLayout colorLayout)
      : vkw::RenderPass(device,
                        m_compile_info(colorFormat, depthFormat, colorLayout)) {

  }

private:
  vkw::RenderPassCreateInfo m_compile_info(VkFormat colorFormat,
                                           VkFormat depthFormat,
                                           VkImageLayout colorLayout);
};

class ShadowPass : public PassBase, public vkw::RenderPass {
public:
  using PassBase::m_attachments;
  ShadowPass(vkw::Device &device, VkFormat depthFormat)
      : vkw::RenderPass(device, m_compile_info(depthFormat)) {}

private:
  vkw::RenderPassCreateInfo m_compile_info(VkFormat attachmentFormat);
};

} // namespace TestApp
#endif // TESTAPP_RENDERPASSESIMPL_H
