#include "Atmosphere.hpp"
#include <vkw/Queue.hpp>
#include <vkw/StagingBuffer.hpp>

namespace TestApp {

Atmosphere::OutScatterTexture::OutScatterTexture(
    vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader,
    const vkw::UniformBuffer<Properties> &atmo, uint32_t psiRate,
    uint32_t heightRate)
    : vkw::Image<vkw::COLOR,
                 vkw::I2D>{device.getAllocator(),
                           VmaAllocationCreateInfo{
                               .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                               .requiredFlags =
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                           VK_FORMAT_R32G32B32A32_SFLOAT,
                           heightRate,
                           psiRate,
                           1,
                           1,
                           1,
                           VK_IMAGE_USAGE_STORAGE_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT},
      RenderEngine::ComputeLayout(
          device, shaderLoader,
          RenderEngine::SubstageDescription{
              .shaderSubstageName = "atmosphere_outscatter",
              .setBindings = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                              {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}},
          1),
      RenderEngine::Compute(static_cast<RenderEngine::ComputeLayout &>(*this)),
      m_view(device, *this, format()) {

  set().write(0, atmo);
  set().writeStorageImage(1, m_view);

  auto &queue = device.anyComputeQueue();
  auto commandPool =
      vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                       queue.family().index()};
  auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

  transferBuffer.begin(0);

  VkImageMemoryBarrier transitLayout1{};
  transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
  transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  transitLayout1.pNext = nullptr;
  transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  transitLayout1.subresourceRange.baseArrayLayer = 0;
  transitLayout1.subresourceRange.baseMipLevel = 0;
  transitLayout1.subresourceRange.layerCount = 1;
  transitLayout1.subresourceRange.levelCount = 1;
  transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  transitLayout1.srcAccessMask = 0;

  transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    {&transitLayout1, 1});

  transferBuffer.end();

  queue.submit(transferBuffer);

  queue.waitIdle();

  recompute(device);
}

void Atmosphere::OutScatterTexture::recompute(vkw::Device &device) {

  auto &queue = device.anyComputeQueue();
  auto commandPool =
      vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                       queue.family().index()};
  auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

  transferBuffer.begin(0);

  VkImageMemoryBarrier transitLayout1{};
  transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
  transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  transitLayout1.pNext = nullptr;
  transitLayout1.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  transitLayout1.subresourceRange.baseArrayLayer = 0;
  transitLayout1.subresourceRange.baseMipLevel = 0;
  transitLayout1.subresourceRange.layerCount = 1;
  transitLayout1.subresourceRange.levelCount = 1;
  transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

  transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    {&transitLayout1, 1});

  dispatch(transferBuffer, width() / 16, height() / 16, 1);

  transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  transitLayout1.dstAccessMask = 0;
  transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

  transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    {&transitLayout1, 1});
  transferBuffer.end();

  queue.submit(transferBuffer);

  queue.waitIdle();
}
Atmosphere::Atmosphere(vkw::Device &device,
                       RenderEngine::ShaderLoaderInterface &shaderLoader,
                       unsigned int psiRate, unsigned int heightRate)
    : m_ubo(device, properties),
      m_out_scatter_texture(device, shaderLoader, m_ubo, psiRate, heightRate),
      m_device(device) {}
Atmosphere::PropertiesBuffer::PropertiesBuffer(vkw::Device &device,
                                               Properties const &props)
    : vkw::UniformBuffer<Properties>(
          device,
          VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                  .requiredFlags =
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
          VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
  update(device, props);
}

void Atmosphere::PropertiesBuffer::update(vkw::Device &device,
                                          const Atmosphere::Properties &props) {
  auto &queue = device.anyTransferQueue();

  vkw::StagingBuffer<Properties> stagingBuffer{device, {&props, 1}};

  auto commandPool = vkw::CommandPool(
      device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue.family().index());

  auto transferCommand = vkw::PrimaryCommandBuffer(commandPool);

  VkBufferCopy region{};

  region.size = stagingBuffer.bufferSize();
  transferCommand.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transferCommand.copyBufferToBuffer(stagingBuffer, *this, {&region, 1});

  transferCommand.end();

  queue.submit(transferCommand);

  queue.waitIdle();
}
AtmosphereProperties::AtmosphereProperties(GUIFrontEnd &gui,
                                           Atmosphere &atmosphere,
                                           std::string_view title)
    : GUIWindow(gui,
                WindowSettings{.title = std::string(title), .autoSize = true}),
      m_atmosphere(atmosphere) {}

void AtmosphereProperties::onGui() {
  bool needUpdate = false;
  if(m_trackRadius){
    if(ImGui::SliderFloat("Planet radius", &m_atmosphere.get().properties.planetRadius, 10.0f, 1000.0f))
      needUpdate = true;
    if(ImGui::SliderFloat("Atmosphere radius", &m_atmosphere.get().properties.atmosphereRadius, 1.0f, 100.0f))
      needUpdate = true;
  }
  if(ImGui::SliderFloat3("Rayleigh", &m_atmosphere.get().properties.RayleighConstants.x, 0.0f, 1.0f)){
    needUpdate = true;
  }

  if(ImGui::SliderFloat("Mei", &m_atmosphere.get().properties.MeiConstants, 0.0f, 1.0f)){
    needUpdate = true;
  }

  if(ImGui::SliderFloat("HO", &m_atmosphere.get().properties.H0, 0.0f, 1.0f)){
    needUpdate = true;
  }

  if(ImGui::SliderFloat("g", &m_atmosphere.get().properties.g, -0.9999f, -0.5f)){
    needUpdate = true;
  }

  if(ImGui::SliderInt("samples", &m_atmosphere.get().properties.samples, 5, 50)){
    needUpdate = true;
  }

  if(needUpdate)
    m_atmosphere.get().update();
}

} // namespace TestApp