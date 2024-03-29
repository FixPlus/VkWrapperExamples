#ifndef TESTAPP_PRECOMPUTE_H
#define TESTAPP_PRECOMPUTE_H

#include "RenderEngine/Pipelines/Compute.h"
#include <vkw/CommandBuffer.hpp>
#include <vkw/Image.hpp>

namespace TestApp {

class PrecomputeImageLayout : public RenderEngine::ComputeLayout {
public:
  PrecomputeImageLayout(vkw::Device &device,
                        RenderEngine::ShaderLoaderInterface &shaderLoader,
                        RenderEngine::SubstageDescription stageDescription,
                        uint32_t xGroupSize, uint32_t yGroupSize);

  uint32_t xGroupSize() const { return m_xGroupSize; }

  uint32_t yGroupSize() const { return m_yGroupSize; }

private:
  uint32_t m_xGroupSize, m_yGroupSize;
};

class PrecomputeImage : public RenderEngine::Compute {
public:
  PrecomputeImage(vkw::Device &device, PrecomputeImageLayout &parent,
                  vkw::BasicImage<vkw::COLOR, vkw::I2D, vkw::ARRAY> &image);

  void releaseOwnershipTo(vkw::CommandBuffer &buffer,
                          uint32_t computeFamilyIndex,
                          VkImageLayout incomingLayout,
                          VkAccessFlags incomingAccessMask,
                          VkPipelineStageFlags incomingStageMask) const;

  void acquireOwnership(vkw::CommandBuffer &buffer,
                        uint32_t incomingFamilyIndex,
                        VkImageLayout incomingLayout,
                        VkAccessFlags incomingAccessMask,
                        VkPipelineStageFlags incomingStageMask) const;

  void releaseOwnership(vkw::CommandBuffer &buffer, uint32_t acquireFamilyIndex,
                        VkImageLayout acquireLayout,
                        VkAccessFlags acquireAccessMask,
                        VkPipelineStageFlags acquireStageMask) const;

  void acquireOwnershipFrom(vkw::CommandBuffer &buffer,
                            uint32_t computeFamilyIndex,
                            VkImageLayout acquireLayout,
                            VkAccessFlags acquireAccessMask,
                            VkPipelineStageFlags acquireStageMask) const;

  void dispatch(vkw::CommandBuffer &buffer);

private:
  vkw::StrongReference<vkw::BasicImage<vkw::COLOR, vkw::I2D, vkw::ARRAY>>
      m_image;
  vkw::ImageView<vkw::COLOR, vkw::V2DA> m_imageView;
  std::pair<uint32_t, uint32_t> m_cached_group_size;
};

} // namespace TestApp
#endif // TESTAPP_PRECOMPUTE_H
