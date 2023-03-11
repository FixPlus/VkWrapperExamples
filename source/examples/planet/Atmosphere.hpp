#ifndef TESTAPP_ATMOSPHERE_HPP
#define TESTAPP_ATMOSPHERE_HPP

#include "GUI.h"
#include <RenderEngine/Pipelines/Compute.h>
#include <RenderEngine/RecordingState.h>
#include <glm/glm.hpp>
#include <vkw/Image.hpp>
#include <vkw/UniformBuffer.hpp>

namespace TestApp {

class Atmosphere {
public:
  struct Properties {
    glm::vec3 RayleighConstants = glm::vec3{0.02f, 0.085f, 0.128f};
    float MeiConstants = 0.04f;
    float planetRadius = 10000.0f;
    float atmosphereRadius = 100.0f;
    float H0 = 0.25f;
    float g = -0.9999f;
    int samples = 10;
  } properties;

  auto &outScatterTexture() const { return m_out_scatter_texture.m_view; }

  const vkw::UniformBuffer<Properties> &propertiesBuffer() const {
    return m_ubo;
  }

  void update() {
    m_out_scatter_texture.recompute(m_device.get());
    m_ubo.update(m_device.get(), properties);
  }

  Atmosphere(vkw::Device &device,
             RenderEngine::ShaderLoaderInterface &shaderLoader,
             unsigned psiRate = 2048, unsigned heightRate = 2048);

private:
  class PropertiesBuffer : public vkw::UniformBuffer<Properties> {
  public:
    explicit PropertiesBuffer(vkw::Device &device, Properties const &props);

    void update(vkw::Device &device, Properties const &props);
  } m_ubo;

  class OutScatterTexture : public vkw::Image<vkw::COLOR, vkw::I2D>,
                            public RenderEngine::ComputeLayout,
                            public RenderEngine::Compute {
  public:
    vkw::ImageView<vkw::COLOR, vkw::V2D> m_view;

    OutScatterTexture(vkw::Device &device,
                      RenderEngine::ShaderLoaderInterface &shaderLoader,
                      vkw::UniformBuffer<Properties> const &atmo,
                      uint32_t psiRate, uint32_t heightRate);

    void recompute(vkw::Device &device);

  } m_out_scatter_texture;
  vkw::StrongReference<vkw::Device> m_device;
};

class AtmosphereProperties : public GUIWindow {
public:
  AtmosphereProperties(GUIFrontEnd &gui, Atmosphere &atmosphere,
                       std::string_view title = "Atmosphere properties");

  void disableRadiusTrack(){
    m_trackRadius = false;
  }

  void enableRadiusTrack() {
    m_trackRadius = true;
  }
protected:
  void onGui() override;

private:
  bool m_trackRadius = true;
  std::reference_wrapper<Atmosphere> m_atmosphere;
};

} // namespace TestApp
#endif // TESTAPP_ATMOSPHERE_HPP
