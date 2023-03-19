#include "SunLight.hpp"
#include <vkw/CommandBuffer.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/Queue.hpp>
#include <vkw/StagingBuffer.hpp>

namespace TestApp {

SunLight::SunLight(vkw::Device &device)
    : m_ubo(device,
            VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                    .requiredFlags =
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
      m_device(device) {

  update();
}

void SunLight::update() {
  auto &device = m_device.get();
  auto &queue = device.anyTransferQueue();

  vkw::StagingBuffer<Properties> stagingBuffer{device, {&properties, 1}};

  auto commandPool = vkw::CommandPool(
      device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue.family().index());

  auto transferCommand = vkw::PrimaryCommandBuffer(commandPool);

  VkBufferCopy region{};

  region.size = stagingBuffer.bufferSize();
  transferCommand.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transferCommand.copyBufferToBuffer(stagingBuffer, m_ubo, {&region, 1});

  transferCommand.end();

  queue.submit(transferCommand);

  queue.waitIdle();
}
SunLightProperties::SunLightProperties(GUIFrontEnd &gui, SunLight &sunlight,
                                       std::string_view title)
    : GUIWindow(gui,
                WindowSettings{.title = std::string(title), .autoSize = true}),
      m_sunlight(sunlight),
      m_angleInDeg(glm::degrees(m_sunlight.get().properties.eulerAngle)) {}
void SunLightProperties::onGui() {
  bool needUpdate = false;
  if (ImGui::SliderFloat("phi", &m_angleInDeg.x, 0.0f, 360.0f)) {
    m_sunlight.get().properties.eulerAngle.x = glm::radians(m_angleInDeg.x);
    needUpdate = true;
  }

  if (ImGui::SliderFloat("psi", &m_angleInDeg.y, -90.0f, 90.0f)) {
    m_sunlight.get().properties.eulerAngle.y = glm::radians(m_angleInDeg.y);
    needUpdate = true;
  }

  if (ImGui::SliderFloat("intensity", &m_sunlight.get().properties.intensity,
                         0.0f, 100000.0f)) {
    needUpdate = true;
  }

  if (ImGui::SliderFloat("distance", &m_sunlight.get().properties.distance,
                         0.0f, 10000000.0f)) {
    needUpdate = true;
  }

  if (ImGui::ColorEdit3("color", &m_sunlight.get().properties.color.x)) {
    needUpdate = true;
  }

  if (needUpdate) {
    m_sunlight.get().update();
  }
}

} // namespace TestApp