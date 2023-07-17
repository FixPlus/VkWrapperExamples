#include "BumpMap.hpp"
#include "RenderEngine/Pipelines/Compute.h"
#include "Utils.h"

namespace TestApp {

namespace {

class BumpCompute : public RenderEngine::Compute {
public:
  BumpCompute(RenderEngine::ComputeLayout &layout, vkw::Device &device,
              vkw::Image<vkw::COLOR, vkw::I2D> const &heightMap,
              vkw::Image<vkw::COLOR, vkw::I2D> const &bumpMap)
      : RenderEngine::Compute(layout),
        m_heightView(device, heightMap, heightMap.format()),
        m_bumpView(device, bumpMap, bumpMap.format()),
        m_heightSampler(createDefaultSampler(device)) {
    set().write(0, m_heightView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_heightSampler);
    set().writeStorageImage(1, m_bumpView);
  }

private:
  vkw::Sampler m_heightSampler;
  vkw::ImageView<vkw::COLOR, vkw::V2D> m_heightView;
  vkw::ImageView<vkw::COLOR, vkw::V2D> m_bumpView;
};
} // namespace
BumpMap::BumpMap(vkw::Device &device,
                 RenderEngine::ShaderLoaderInterface &shaderLoader,
                 vkw::Image<vkw::COLOR, vkw::I2D> const &heightMap)
    : Image<vkw::COLOR, vkw::I2D>{
          device.getAllocator(),
          VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                  .requiredFlags =
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
          VK_FORMAT_R8G8B8A8_UNORM,
          heightMap.width(),
          heightMap.height(),
          1,
          1,
          1,
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT} {
  doTransitLayout(*this, device, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_GENERAL);
  auto computeLayout = RenderEngine::ComputeLayout(
      device, shaderLoader,
      RenderEngine::SubstageDescription{.shaderSubstageName =
                                            "generate_bump_map"},
      1);
  auto compute = BumpCompute(computeLayout, device, heightMap, *this);

  auto computeQueue = device.anyComputeQueue();
  auto commandPool =
      vkw::CommandPool(device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                       computeQueue.family().index());
  auto commandBuffer = vkw::PrimaryCommandBuffer(commandPool);

  commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  compute.dispatch(commandBuffer, heightMap.width(), heightMap.height());

  commandBuffer.end();

  computeQueue.submit(vkw::SubmitInfo(commandBuffer));

  computeQueue.waitIdle();

  doTransitLayout(*this, device, VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

} // namespace TestApp