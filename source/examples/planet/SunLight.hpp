#ifndef TESTAPP_SUNLIGHT_HPP
#define TESTAPP_SUNLIGHT_HPP

#include <vkw/UniformBuffer.hpp>
#include <glm/glm.hpp>
#include "GUI.h"


namespace TestApp {

class SunLight {
public:
  struct Properties {
    glm::vec4 color = glm::vec4{1.0f, 1.0f, 1.0f, 0.0f};
    glm::vec2 eulerAngle = glm::vec2{0.0f, 0.0f};
    float distance = 1000.0f;
  } properties;

  explicit SunLight(vkw::Device &device);

  auto& propertiesBuffer() const{
    return m_ubo;
  }

  void update();

private:
  vkw::UniformBuffer<Properties> m_ubo;
  vkw::StrongReference<vkw::Device> m_device;
};

class SunLightProperties: public GUIWindow{
public:

  SunLightProperties(GUIFrontEnd& gui, SunLight& sunlight, std::string_view title = "Sunlight properties");

protected:

  void onGui() override;

private:
  glm::vec2 m_angleInDeg;
  std::reference_wrapper<SunLight> m_sunlight;
};
} // namespace TestApp
#endif // TESTAPP_SUNLIGHT_HPP
